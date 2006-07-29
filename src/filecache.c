/*
	$Id$

	Caching of file handles, and read/write buffers


	Copyright (C) 2006 Alex Waugh
	
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	
	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <swis.h>
#include <kernel.h>
#include <ctype.h>

#include "moonfish.h"
#include "utils.h"

#include "filecache.h"


#define MAXCACHEDFILES 3

#define DATABUFFER 128*1024

static int ownerseq = 0;
static struct stateid *owners = NULL;

/* Have to be per lock_owner per file */
struct open_owner {
	unsigned access;
	unsigned deny;
	struct stateid *stateid;
	/*struct lock *locks;*/
	struct open_owner *next;
};

static const char zeros[12] = {0,0,0,0,0,0,0,0,0,0,0,0};
static const char ones[12] = {0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF};


struct openfile {
	char name[MAX_PATHNAME]; /* Could make this dynamic? */
	int handle;
	unsigned int filesize;
	unsigned int load;
	unsigned int exec;
	unsigned int attr;
	struct open_owner *owners;
	struct openfile *next;
};

static struct openfile *openfiles = NULL;

static struct cachedfile {
	char name[MAX_PATHNAME];
	int handle;
	clock_t time;
	char buffer[DATABUFFER];
	unsigned int bufferoffset;
	unsigned int buffercount;
	int write;
	enum nstat writeerror;
	unsigned int filesize;
	unsigned int load;
	unsigned int exec;
	unsigned int attr;
	struct openfile *file;
} cachedfiles[MAXCACHEDFILES];


static unsigned int verifier[2];

static enum nstat filecache_createstateid(unsigned clientid, char *owner, int ownerlen, unsigned seqid, struct stateid **stateid, int *confirmrequired)
{
	struct stateid *id = owners;

	while (id) {
		if ((id->clientid == clientid) &&
		    (id->ownerlen == ownerlen) &&
		    (memcmp(id->owner, owner, ownerlen) == 0)) {
			*confirmrequired = 0;
			id->refcount++;
			*stateid = id;
			id->seqid = seqid;
			return NFS_OK;
		}
		id = id->next;
	}
	UR(id = malloc(sizeof(struct stateid)));
	id->owner = malloc(ownerlen);
	if (id->owner == NULL) {
		free(id);
		UR(NULL);
	}
	memcpy(id->owner, owner, ownerlen);
	id->ownerlen = ownerlen;
	id->clientid = clientid;
	id->refcount = 1;
	id->ownerseq = ownerseq++;
	id->seqid = seqid;
	id->unconfirmed = 1;
	id->timeout = clock();
	id->next = owners;
	owners = id;

	*stateid = id;
	*confirmrequired = 1;
	return NFS_OK;
}

static enum nstat filecache_removestateid(struct stateid *stateid)
{
	stateid->refcount--;

	if (stateid->refcount == 0) {
		struct stateid *id = owners;

		if (owners == stateid) {
			owners = stateid->next;
		} else {
			while (id) {
				if (id->next == stateid) {
					id->next = stateid->next;
					break;
				}
				id = id->next;
			}
		}
		free(stateid->owner);
		free(stateid);
	}
	/*FIXME set timeout and leave? */
	return NFS_OK;
}


enum nstat filecache_checkseqid(struct stateid *stateid, unsigned clientid, char *owner, int ownerlen, unsigned seqid, int confirm, union duplicate **duplicate)
{
	if (stateid == NULL) {
		stateid = owners;
		while (stateid) {
			if ((stateid->clientid == clientid) &&
			    (stateid->ownerlen == ownerlen) &&
			    (memcmp(stateid->owner, owner, ownerlen) == 0)) break;
			stateid = stateid->next;
		}
	}
	if (stateid == NULL) {
		*duplicate = NULL;
		return NFS_OK;
	}

	if (seqid < stateid->seqid) return NFSERR_BAD_SEQID;
	if (seqid > stateid->seqid + 1) return NFSERR_BAD_SEQID;
	if (seqid == stateid->seqid) {
		*duplicate = &(stateid->duplicate);
	} else {
		*duplicate = NULL;
	}
	if (stateid->unconfirmed) {
		if (confirm) {
			stateid->unconfirmed = 0;
		} else {
			return NFSERR_BAD_SEQID;
		}
	}

	return NFS_OK;
}

enum nstat filecache_getstateid(unsigned seqid, char *other, struct stateid **stateid)
{
	struct stateid *id = owners;
	unsigned *other2 = (unsigned *)other;

	if ((seqid == 0) && (memcmp(other, zeros, 12) == 0)) {
		*stateid = STATEID_NONE;
		return NFS_OK;
	}

	if ((seqid == 0xFFFFFFFF) && (memcmp(other, ones, 12) == 0)) {
		*stateid = STATEID_ANY;
		return NFS_OK;
	}

	if ((other2[1] != verifier[0]) || (other2[2] != verifier[1])) {
		return NFSERR_STALE_STATEID;
	}

	while (id) {
		if (id == (struct stateid *)(other2[0])) {
			if (id->ownerseq != seqid) return NFSERR_STALE_STATEID;

			*stateid = id;
			return NFS_OK;
		}
		id = id->next;
	}
	return NFSERR_STALE_STATEID;
}

void filecache_init(void)
{
	int i;

	/* Initialise all entries */
	for (i = 0; i < MAXCACHEDFILES; i++) cachedfiles[i].handle = 0;

	/* Get the time we are initialised to use as a verifier */
	verifier[0] = (unsigned int)time(NULL);
	verifier[1] = 0;
}

static enum nstat filecache_flush(int index)
{
	enum nstat ret = NFS_OK;

	/* Write out any cached data */
	if (cachedfiles[index].write && cachedfiles[index].buffercount > 0) {
		os_error *err;

		if (cachedfiles[index].buffercount > DATABUFFER) return NFSERR_SERVERFAULT;

		err = _swix(OS_GBPB, _INR(0,4), 1, cachedfiles[index].handle,
		                                   cachedfiles[index].buffer,
		                                   cachedfiles[index].buffercount,
		                                   cachedfiles[index].bufferoffset);
		if (err) {
			syslogf(LOGNAME, LOG_SERIOUS, "Error writing cached data for file %s (%s)",
			        cachedfiles[index].name, err->errmess);
			ret = cachedfiles[index].writeerror = oserr_to_nfserr(err->errnum);
		}
	}
	cachedfiles[index].buffercount = 0;

	return ret;
}

static void filecache_evict(int index, struct openfile *file)
{
	os_error *err;

	/* Write out any cached data */
	filecache_flush(index);

	if (file) {
		/* do something with writeerror? */
	} else {
		/* Close the file */
		err = _swix(OS_Find, _INR(0,1), 0, cachedfiles[index].handle);
		if (err) syslogf(LOGNAME, LOG_SERIOUS, "Error closing cached file %s (%s)",
		                 cachedfiles[index].name, err->errmess);
	
		if (err || (cachedfiles[index].writeerror != NFS_OK)) {
			/* An error occured that hasn't been returned to the client,
			   change the verifier as this is the only way it will now
			   be able to detect it. */
			verifier[1]++;
		}
	}

	/* Invalidate entry */
	cachedfiles[index].handle = 0;
}

static enum nstat filecache_checkpermissions(struct openfile *file, struct stateid *stateid, int read)
{
	struct open_owner *owner;
	enum nstat permission = NFS_OK;

	if ((file == NULL) || (stateid == STATEID_ANY)) return permission;
	owner = file->owners;
	while (owner) {
		if (owner->stateid == stateid) {
			return (owner->access & (read ? 1 : 2)) ? NFS_OK : NFSERR_LOCKED;
		} else {
			if (owner->deny & (read ? 1 : 2)) {
				permission = NFSERR_LOCKED; /*FIXME NFS2/3 error? */
			}
		}
		owner = owner->next;
	}
	if (stateid != STATEID_NONE) return NFSERR_BAD_STATEID;
	return permission;
}

static enum nstat filecache_opencached(char *path, struct stateid *stateid, int read, int *index, struct openfile **openfile, int dontopen)
{
	int i;
	clock_t now = clock();
	clock_t lasttime = 0;
	int earliest = 0;
	int available = -1;
	struct openfile *file;

	/* Search all entries for a matching filename */
	for (i = 0; i < MAXCACHEDFILES; i++) {
		if (cachedfiles[i].handle == 0) {
			available = i;
		} else if (strcmp(path, cachedfiles[i].name) == 0) {
			NR(filecache_checkpermissions(cachedfiles[i].file, stateid, read));
			if (index) *index = i;
			if (openfile) *openfile = cachedfiles[i].file;
			cachedfiles[i].time = now;

			return NFS_OK;
		} else if (now - cachedfiles[i].time > lasttime) {
			lasttime = now - cachedfiles[i].time;
			earliest = i;
		}
	}
	if (available != -1) earliest = available;

	/* Search all NFS4 open files for a match */
	file = openfiles;
	while (file) {
		if (strcmp(path, file->name) == 0) break;
		file = file->next;
	}

	if (openfile) *openfile = file;

	NR(filecache_checkpermissions(file, stateid, read));

	if (dontopen) {
		if (index) *index = -1;
		return NFS_OK;
	}

	/* Evict the oldest existing entry */
	if (cachedfiles[earliest].handle) filecache_evict(earliest, file);

	/* Open the file and cache the attributes */
	cachedfiles[earliest].time = now;
	cachedfiles[earliest].buffercount = 0;
	cachedfiles[earliest].bufferoffset = 0;
	cachedfiles[earliest].write = 0;
	cachedfiles[earliest].writeerror = NFS_OK;
	snprintf(cachedfiles[earliest].name, MAX_PATHNAME, "%s", path);
	if (file) {
		/* Take attributes from already cached info */
		cachedfiles[earliest].load = file->load;
		cachedfiles[earliest].exec = file->exec;
		cachedfiles[earliest].filesize = file->filesize;
		cachedfiles[earliest].attr = file->attr;
		cachedfiles[earliest].handle = file->handle;
		cachedfiles[earliest].file = file;
	} else {
		int handle;

		OR(_swix(OS_File, _INR(0,1) | _OUTR(2,5), 17, path, &(cachedfiles[earliest].load),
	                                                            &(cachedfiles[earliest].exec),
	                                                            &(cachedfiles[earliest].filesize),
	                                                            &(cachedfiles[earliest].attr)));
		OR(_swix(OS_Find, _INR(0,1) | _OUT(0), 0xC3, path, &handle));

		/* Everything went OK, so mark the entry as valid */
		cachedfiles[earliest].handle = handle;
	}


	if (index) *index = earliest;
	return NFS_OK;
}

enum nstat filecache_open(char *path, unsigned clientid, char *owner, int ownerlen, unsigned access, unsigned deny, unsigned seqid, int *ownerseqid, char *other, int *confirmrequired)
{
	int index;
	struct openfile *file;
	struct stateid *stateid;
	unsigned *other2 = (unsigned *)(other);

	NR(filecache_opencached(path, STATEID_ANY, 0, &index, &file, 1));
	NR(filecache_createstateid(clientid, owner, ownerlen, seqid, &stateid, confirmrequired));

	if (file) {
		struct open_owner *current = file->owners;
		struct open_owner *matching = NULL;
		unsigned currentaccess = 0;
		unsigned currentdeny = 0;

		while (current) {
			if (current->stateid == stateid) {
				matching = current;
			}
			currentaccess |= current->access;
			currentdeny |= current->deny;
			current = current->next;
		}
		/* Check share status etc. */
		if (access == 0) return NFSERR_INVAL;
		if ((access & currentdeny) || (deny & currentaccess)) return NFSERR_DENIED;
		if (matching) {
			matching->access |= access;
			matching->deny |= deny;
		} else {
			struct open_owner *newowner;
			newowner = malloc(sizeof(struct open_owner));
			if (newowner == NULL) {
				filecache_removestateid(stateid);
				UR(NULL);
			}
			newowner->stateid = stateid;
			newowner->access = access;
			newowner->deny = deny;
			newowner->next = file->owners;
			file->owners = newowner;
		}
	} else {
		os_error *err2;

		file = malloc(sizeof(struct openfile));
		if (file == NULL) {
			filecache_removestateid(stateid);
			UR(NULL);
		}

		file->owners = malloc(sizeof(struct open_owner));
		if (file->owners == NULL) {
			free(file);
			filecache_removestateid(stateid);
			UR(NULL);
		}

		file->owners->stateid = stateid;
		file->owners->access = access;
		file->owners->deny = deny;
		file->owners->next = NULL;

		snprintf(file->name, MAX_PATHNAME, "%s", path);

		err2 = _swix(OS_File, _INR(0,1) | _OUTR(2,5), 17, path, &(file->load),
	                                                                &(file->exec),
	                                                                &(file->filesize),
	                                                                &(file->attr));
		if (err2 == NULL) err2 = _swix(OS_Find, _INR(0,1) | _OUT(0), 0xCF, path, &(file->handle));

		if (err2) {
			free(file->owners);
			free(file);
			filecache_removestateid(stateid);
			/* Check for file already open error */
			if (err2->errnum == 0x117c2) return NFSERR_SHARE_DENIED;
			OR(err2);
		}

		/* Add to linked list */
		file->next = openfiles;
		openfiles = file;
	}
	*ownerseqid = stateid->ownerseq;
	other2[0] = (unsigned)stateid;
	other2[1] = verifier[0];
	other2[2] = verifier[1];

	return NFS_OK;
}

enum nstat filecache_opendowngrade(char *path, struct stateid *stateid, unsigned access, unsigned deny)
{
	int index;
	struct openfile *file;
	struct open_owner *current;

	NR(filecache_opencached(path, STATEID_ANY, 0, &index, &file, 1));
	if (file == NULL) {
		return NFSERR_BAD_STATEID;
	}

	current = file->owners;

	while (current) {
		if (current->stateid == stateid) {
			/* Upgrading access is not permitted */
			if ((access & ~current->access) ||
			    (deny & ~current->deny)) {
				return NFSERR_INVAL;
			}
			current->access = access;
			current->deny = deny;
			return NFS_OK;
		}
		current = current->next;
	}
	return NFSERR_BAD_STATEID;
}

enum nstat filecache_close(char *path, struct stateid *stateid)
{
	int index;
	struct openfile *file;
	enum nstat ret = NFS_OK;
	int handle;
	struct open_owner *current;
	struct open_owner **prev;

	NR(filecache_opencached(path, stateid, 0, &index, &file, 1));

	if (file == NULL) return NFSERR_BAD_STATEID;

	prev = &(file->owners);
	current = file->owners;

	while (current) {
		if (current->stateid == stateid) break;
		prev = &(current->next);
		current = current->next;
	}
	if (current == NULL) return NFSERR_BAD_STATEID;

	/* Remove from list */
	*prev = current->next;

	free(current);

	/* If last client with this file open, then close the file */
	if (file->owners == NULL) {
		if (index != -1) {
			filecache_evict(index, file);
			ret = cachedfiles[index].writeerror;
		}
	
		/* Remove from linked list */
		if (openfiles == file) {
			openfiles = file->next;
		} else {
			struct openfile *thisfile = openfiles;
			while (thisfile->next != file) thisfile = thisfile->next;
			thisfile->next = file->next;
		}
	
		/* Close the file */
		handle = file->handle;
		free(file);
		OR(_swix(OS_Find, _INR(0,1), 0, handle));
	}

	return ret;
}

void filecache_reap(int all)
{
	int i;
	clock_t now = clock();

	for (i = 0; i < MAXCACHEDFILES; i++) {
		/* Close a file if there has been no activity for a while */
		if (cachedfiles[i].handle != 0 && (all || (now - cachedfiles[i].time > 5 * CLOCKS_PER_SEC))) {
			filecache_evict(i, cachedfiles[i].file);
		}
	}
	if (all) {
		/* Close all open files as well */
		struct openfile *thisfile = openfiles;
		openfiles = NULL;
		while (thisfile) {
			struct openfile *file = thisfile;
			os_error *err;

			thisfile = thisfile->next;
			err = _swix(OS_Find, _INR(0,1), 0, file->handle);
			/* Can't do much about errors at this stage */
			if (err) syslogf(LOGNAME, LOG_SERIOUS, "Error closing cached file %s (%s)",
			                 file->name, err->errmess);
			free(file);
		}
	}
}

enum nstat filecache_read(char *path, struct stateid *stateid, unsigned int count, unsigned int offset, char **data, unsigned int *read, int *eof)
{
	int index;
	unsigned int bufferread;
	unsigned int bufferoffset;

	NR(filecache_opencached(path, stateid, 1, &index, NULL, 0));

	/* If the previous access was a write we need to flush any buffered
	   data before overwriting it */
	if (cachedfiles[index].write) filecache_flush(index);
	cachedfiles[index].write = 0;

	if (count > DATABUFFER) count = DATABUFFER;

	/* Ensure offset and count lie within the file */
	if (offset > cachedfiles[index].filesize) {
		count = 0;
		offset = 0;
	}
	if (offset + count > cachedfiles[index].filesize) count = cachedfiles[index].filesize - offset;

	if (offset < cachedfiles[index].bufferoffset) {
		/* Buffer is too high */
		bufferoffset = 0;
		bufferread = DATABUFFER;
		cachedfiles[index].bufferoffset = offset;
	} else if (offset > cachedfiles[index].bufferoffset + cachedfiles[index].buffercount) {
		/* Buffer is too low */
		bufferoffset = 0;
		bufferread = DATABUFFER;
		cachedfiles[index].bufferoffset = offset;
	} else if (offset + count > cachedfiles[index].bufferoffset + cachedfiles[index].buffercount) {
		/* Buffer contains partial results */
		bufferoffset = offset - cachedfiles[index].bufferoffset;
		bufferread = cachedfiles[index].buffercount - bufferoffset;

		if ((bufferread > DATABUFFER) || (bufferoffset > DATABUFFER)) return NFSERR_SERVERFAULT;
		memmove(cachedfiles[index].buffer, cachedfiles[index].buffer + bufferoffset, bufferread);

		bufferoffset = bufferread;
		bufferread = DATABUFFER - bufferoffset;
		cachedfiles[index].bufferoffset = offset;
		cachedfiles[index].buffercount = bufferoffset;
	} else {
		/* Buffer contains full results */
		bufferoffset = 0;
		bufferread = 0;
	}

	/* Fill the buffer if we need */
	if (bufferread > 0) {
		if (bufferoffset + bufferread > DATABUFFER) return NFSERR_SERVERFAULT;
		OR(_swix(OS_GBPB, _INR(0,4) | _OUT(3), 3, cachedfiles[index].handle, cachedfiles[index].buffer + bufferoffset, bufferread, offset + bufferoffset, &bufferread));
		cachedfiles[index].buffercount = DATABUFFER - bufferread;
	}

	/* Return data from the buffer */
	bufferoffset = (offset - cachedfiles[index].bufferoffset);
	if (bufferoffset > DATABUFFER) return NFSERR_SERVERFAULT;

	*data = cachedfiles[index].buffer + bufferoffset;
	*read = cachedfiles[index].buffercount - bufferoffset;
	if (*read > count) *read = count;
	if (eof) *eof = offset + *read >= cachedfiles[index].filesize;

	return NFS_OK;
}

enum nstat filecache_write(char *path, struct stateid *stateid, unsigned int count, unsigned int offset, char *data, int sync, char *verf)
{
	int index;
	struct openfile *file;

	if (verf) memcpy(verf, verifier, 8);

	if (stateid == STATEID_ANY) stateid = STATEID_NONE;

	NR(filecache_opencached(path, stateid, 0, &index, &file, 0));

	/* If the data in the buffer is from a read then discard it */
	if (cachedfiles[index].write == 0) {
		cachedfiles[index].write = 1;
		cachedfiles[index].bufferoffset = 0;
		cachedfiles[index].buffercount = 0;
	}

	/* Check for data that would be non-contiguous, or overflow the buffer */
	if ((cachedfiles[index].buffercount > 0) &&
	    (((cachedfiles[index].bufferoffset + cachedfiles[index].buffercount) != offset) ||
	     ((cachedfiles[index].buffercount + count) > DATABUFFER))) {
		NR(filecache_flush(index));
	}

	if (sync || count > DATABUFFER) {
		/* Write directly to disc */
		OR(_swix(OS_GBPB, _INR(0,4), 1, cachedfiles[index].handle, data, count, offset));
		if (sync) OR(_swix(OS_Args, _INR(0,1), 255, cachedfiles[index].handle));
	} else {
		/* Write to the buffer */
		if (cachedfiles[index].buffercount + count > DATABUFFER) return NFSERR_SERVERFAULT;
		memcpy(cachedfiles[index].buffer + cachedfiles[index].buffercount, data, count);
		if (cachedfiles[index].buffercount == 0) cachedfiles[index].bufferoffset = offset;
		cachedfiles[index].buffercount += count;
	}

	if (cachedfiles[index].filesize < offset + count) {
		cachedfiles[index].filesize = offset + count;
		if (file) file->filesize = offset + count;
	}

	return NFS_OK;
}

enum nstat filecache_commit(char *path, char *verf)
{
	int index;
	enum nstat ret = NFS_OK;

	if (verf) memcpy(verf, verifier, 8);

	NR(filecache_opencached(path, STATEID_ANY, 0, &index, NULL, 1));

	if (index != -1) {
		filecache_flush(index);
		ret = cachedfiles[index].writeerror;
		cachedfiles[index].writeerror = NFS_OK;
	}

	return ret;
}

enum nstat filecache_getattr(char *path, unsigned int *load, unsigned int *exec, unsigned int *size, unsigned int *attr, int *cached)
{
	int index;
	struct openfile *file;

	NR(filecache_opencached(path, STATEID_ANY, 1, &index, &file, 1));

	if (index != -1) {
		if (cached) *cached = 1;

		*load = cachedfiles[index].load;
		*exec = cachedfiles[index].exec;
		*size = cachedfiles[index].filesize;
		*attr = cachedfiles[index].attr;
	} else if (file) {
		if (cached) *cached = 1;

		*load = file->load;
		*exec = file->exec;
		*size = file->filesize;
		*attr = file->attr;
	} else {
		if (cached) *cached = 0;
	}

	return NFS_OK;
}

enum nstat filecache_setattr(char *path, struct stateid *stateid, unsigned int load, unsigned int exec, unsigned int attr, unsigned int size, int setsize)
{
	int index;
	struct openfile *file;

	if (!setsize) stateid = STATEID_ANY;

	NR(filecache_opencached(path, stateid, 0, &index, &file, !setsize));

	if (index != -1) {
		cachedfiles[index].load = load;
		cachedfiles[index].exec = exec;
		cachedfiles[index].filesize = size;
		cachedfiles[index].attr = attr;
		if (setsize) OR(_swix(OS_Args, _INR(0,2), 3, cachedfiles[index].handle, size));
	}
	if (file) {
		file->load = load;
		file->exec = exec;
		file->filesize = size;
		file->attr = attr;
	}

	return NFS_OK;
}


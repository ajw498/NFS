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
#include <unixlib.h>

#include "moonfish.h"
#include "utils.h"

#include "filecache.h"
#include "clientid.h"


#define MAXCACHEDFILES 3

#define DATABUFFER 128*1024

static struct openfile *openfiles = NULL;

static struct cachedfile {
	char name[MAX_PATHNAME];
	int handle;
	clock_t time;
	char buffer[DATABUFFER];
	unsigned int bufferoffset;
	unsigned int buffercount;
	int write;
	nstat writeerror;
	unsigned int filesize;
	unsigned int load;
	unsigned int exec;
	unsigned int attr;
	struct openfile *file;
} cachedfiles[MAXCACHEDFILES];

unsigned int verifier[2];

int graceperiod = 1;

#define GRACEFILE "<Choices$Write>.Moonfish.SkipGrace"

void filecache_init(void)
{
	int i;
	os_error *err;
	int type;

	/* Initialise all entries */
	for (i = 0; i < MAXCACHEDFILES; i++) cachedfiles[i].handle = 0;

	/* Get the time we are initialised to use as a verifier */
	verifier[0] = (unsigned)time(NULL);
	verifier[1] = 0;

	state_init();

	/* Check for the existence of the file indicating that we can skip
	   the grace period. Remove the file so that it will not be present
	   if the server crashes */
	err = _swix(OS_File, _INR(0,1) | _OUT(0), 6, GRACEFILE, &type);
	if (err) {
		syslogf(LOGNAME, LOG_SERIOUS, "Error deleting grace file %s (%s)", GRACEFILE, err->errmess);
	} else {
		if (type == OBJ_FILE) graceperiod = 0;
	}
}

static nstat filecache_flush(int index)
{
	nstat ret = NFS_OK;

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
		if (cachedfiles[index].writeerror != NFS_OK) {
			file->writeerror = cachedfiles[index].writeerror;
		}
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

void filecache_removestate(uint64_t clientid)
{
	struct openfile *file = openfiles;

	while (file) {
		struct openfile *nextfile = file->next;

		state_removeclientstate(file, clientid);
		file = nextfile;
	}
}

static nstat filecache_opencached(char *path, struct stateid *stateid, enum accesstype access, int *index, struct openfile **openfile, int dontopen)
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
			NR(state_checkpermissions(cachedfiles[i].file, stateid, access));
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

	NR(state_checkpermissions(file, stateid, access));

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
		int type;

		OR(_swix(OS_File, _INR(0,1) | _OUT(0) | _OUTR(2,5), 17, path, &type,
		         &(cachedfiles[earliest].load),
		         &(cachedfiles[earliest].exec),
		         &(cachedfiles[earliest].filesize),
		         &(cachedfiles[earliest].attr)));
		if (type == 0) NR(NFSERR_NOENT);
		OR(_swix(OS_Find, _INR(0,1) | _OUT(0), 0xCF, path, &handle));

		/* Everything went OK, so mark the entry as valid */
		cachedfiles[earliest].handle = handle;
	}


	if (index) *index = earliest;
	return NFS_OK;
}

nstat filecache_getfile(char *path, struct openfile **file)
{
	int index;

	NR(filecache_opencached(path, STATEID_ANY, ACC_EITHER, &index, file, 1));

	return NFS_OK;
}

nstat filecache_open(char *path, struct open_owner *open_owner, unsigned access, unsigned deny, unsigned *ownerseqid, char *other)
{
	int index;
	struct openfile *file;
	struct stateid_other *other2 = (struct stateid_other *)other;
	struct open_stateid *open_stateid;

	NR(filecache_opencached(path, STATEID_ANY, ACC_EITHER, &index, &file, 1));

	if (file) {
		NR(state_createopenstateid(file, open_owner, access, deny, &open_stateid));
	} else {
		os_error *err2;
		nstat nerr;
		int type;

		UR(file = malloc(sizeof(struct openfile)));

		file->open_stateids = NULL;
		file->lock_stateids = NULL;
		file->writeerror = NFS_OK;

		file->name = strdup(path);
		if (file->name == NULL) {
			free(file);
			UR(NULL);
		}

		nerr = state_createopenstateid(file, open_owner, access, deny, &open_stateid);
		if (nerr != NFS_OK) {
			free(file->name);
			free(file);
			return nerr;
		}

		err2 = _swix(OS_File, _INR(0,1) | _OUT(0) | _OUTR(2,5), 17, path, &type,
		             &(file->load),
		             &(file->exec),
		             &(file->filesize),
		             &(file->attr));
		if ((type != 0) && (err2 == NULL)) {
			err2 = _swix(OS_Find, _INR(0,1) | _OUT(0), 0xCF, path, &(file->handle));
		}

		if ((type == 0) || err2) {
			state_removeopenstateid(file, open_stateid);
			free(file->name);
			free(file);
			if (type == 0) return NFSERR_NOENT;
			OR(err2);
		}

		LL_ADD(openfiles, file);
	}
	*ownerseqid = open_stateid->seqid;
	other2->id = open_stateid;
	other2->verifier = verifier[0];
	other2->lock = 0;

	return NFS_OK;
}

nstat filecache_opendowngrade(char *path, struct stateid *stateid, unsigned access, unsigned deny, unsigned *ownerseqid)
{
	int index;
	struct openfile *file;

	NR(filecache_opencached(path, stateid, ACC_EITHER, &index, &file, 1));
	if (file == NULL) return NFSERR_BAD_STATEID;

	NR(state_opendowngrade(stateid, access, deny, ownerseqid));

	return NFS_OK;
}

/* Flush the file from the cache, so it can be removed or renamed */
nstat filecache_closecache(char *path)
{
	int index;
	struct openfile *file;

	NR(filecache_opencached(path, STATEID_ANY, ACC_EITHER, &index, &file, 1));

	/* Don't flush if there is NFS4 state open on it */
	if ((file == NULL) && (index != -1)) filecache_evict(index, file);

	return NFS_OK;
}

nstat filecache_close(char *path, struct stateid *stateid)
{
	int index;
	struct openfile *file;
	nstat ret = NFS_OK;
	int handle;

	NR(filecache_opencached(path, stateid, ACC_EITHER, &index, &file, 1));

	if (file == NULL) return NFSERR_BAD_STATEID;

	state_removeopenstateid(file, stateid->open);

	/* If last client with this file open, then close the file */
	if (file->open_stateids == NULL) {
		if (index != -1) {
			filecache_evict(index, file);
			ret = cachedfiles[index].writeerror;
		}
		if (file->writeerror != NFS_OK) ret = file->writeerror; 

		LL_REMOVE(openfiles, struct openfile, file);

		/* Close the file */
		handle = file->handle;
		free(file->name);
		free(file);
		OR(_swix(OS_Find, _INR(0,1), 0, handle));
	}

	return ret;
}

void filecache_savegrace(void)
{
	if ((graceperiod == 0) && (openfiles == NULL)) {
		/* We are shutting down with no open state, so record this
		   fact to avoid the grace period on startup.
		   Ignore errors creating the file */
		_swix(OS_File, _INR(0,2) | _INR(4,5), 11, GRACEFILE, 0xFFF, 0, 0);
	}
}

void filecache_reap(int all)
{
	int i;
	clock_t now = clock();

	/* Check when the grace period is over */
	if (graceperiod) {
		if ((unsigned)time(NULL) > verifier[0] + LEASE_TIMEOUT) {
			graceperiod = 0;
		}
	}

	for (i = 0; i < MAXCACHEDFILES; i++) {
		/* Close a file if there has been no activity for a while */
		if (cachedfiles[i].handle != 0 && (all || (now - cachedfiles[i].time > 5 * CLOCKS_PER_SEC))) {
			filecache_evict(i, cachedfiles[i].file);
		}
	}

	if (all) {
		/* Close all open files as well */
		while (openfiles) filecache_close(openfiles->name, NULL);
	}

	state_reap(all, now);

	if (all && openfiles) syslogf(LOGNAME, LOG_SERIOUS, "filecache_reap files still open");
}

nstat filecache_read(char *path, struct stateid *stateid, unsigned int count, unsigned int offset, char **data, unsigned int *read, int *eof)
{
	int index;
	struct openfile *file;
	unsigned int bufferread;
	unsigned int bufferoffset;

	if (graceperiod) return NFSERR_GRACE;

	NR(filecache_opencached(path, stateid, ACC_READ, &index, &file, 0));
	NR(state_checklocks(file, stateid, 0, offset, count));

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

nstat filecache_write(char *path, struct stateid *stateid, unsigned int count, unsigned int offset, char *data, int sync, char *verf)
{
	int index;
	struct openfile *file;

	if (graceperiod) return NFSERR_GRACE;

	if (verf) memcpy(verf, verifier, 8);

	if (stateid == STATEID_ANY) stateid = STATEID_NONE;

	NR(filecache_opencached(path, stateid, ACC_WRITE, &index, &file, 0));
	NR(state_checklocks(file, stateid, 1, offset, count));

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

nstat filecache_commit(char *path, char *verf)
{
	int index;
	nstat ret = NFS_OK;

	if (verf) memcpy(verf, verifier, 8);

	NR(filecache_opencached(path, STATEID_ANY, ACC_WRITE, &index, NULL, 1));

	if (index != -1) {
		filecache_flush(index);
		ret = cachedfiles[index].writeerror;
		cachedfiles[index].writeerror = NFS_OK;
	}

	return ret;
}

nstat filecache_getattr(char *path, unsigned int *load, unsigned int *exec, unsigned int *size, unsigned int *attr, int *cached)
{
	int index;
	struct openfile *file;

	NR(filecache_opencached(path, STATEID_ANY, ACC_READ, &index, &file, 1));

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

nstat filecache_setattr(char *path, struct stateid *stateid, unsigned int load, unsigned int exec, unsigned int attr, unsigned int size, int setsize)
{
	int index;
	struct openfile *file;

	if (graceperiod && setsize) return NFSERR_GRACE;

	if (!setsize) stateid = STATEID_ANY;

	NR(filecache_opencached(path, stateid, ACC_WRITE, &index, &file, !setsize));
	if (setsize && file) {
		unsigned offset;
		unsigned count;

		if (size > file->filesize) {
			offset = file->filesize;
			count = size - file->filesize;
		} else {
			offset = size;
			count = file->filesize - size;
		}
		NR(state_checklocks(file, stateid, 1, offset, count));
	}

	if (index != -1) {
		cachedfiles[index].load = load;
		cachedfiles[index].exec = exec;
		cachedfiles[index].filesize = size;
		cachedfiles[index].attr = attr;
		if (setsize) {
			OR(_swix(OS_Args, _INR(0,2), 3, cachedfiles[index].handle, size));
			/* We must reduce the size of the cached data if it is
			   bigger than the filesize, otherwise when it is
			   flushed the filesize will increase again. */
			if (cachedfiles[index].bufferoffset > size) {
				cachedfiles[index].buffercount = 0;
			} else if (cachedfiles[index].bufferoffset + cachedfiles[index].buffercount > size) {
				cachedfiles[index].buffercount = size - cachedfiles[index].bufferoffset;
			}
		}
	}
	if (file) {
		file->load = load;
		file->exec = exec;
		file->filesize = size;
		file->attr = attr;
	}

	return NFS_OK;
}


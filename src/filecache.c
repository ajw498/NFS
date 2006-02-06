/*
	$Id$
	$URL$

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
#include <sys/types.h>
#include <swis.h>
#include <kernel.h>
#include <ctype.h>

#include "nfs2-procs.h" /*FIXME*/

#include "utils.h"

#include "filecache.h"

#define NR(x) do { \
	enum nstat status = x; \
	if (status != NFS_OK) return status; \
} while (0)

#define OR(x) do { \
	os_error *err = x; \
	if (err) { \
		syslogf(LOGNAME, LOG_ERROR, "Error: %x %s", err->errnum, err->errmess); \
		return oserr_to_nfserr(err->errnum); \
	} \
} while (0)


#define MAXOPENFILES 3

#define DATABUFFER 512*1024

static struct openfile {
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
} openfiles[MAXOPENFILES];

static unsigned int verifier[2];

void filecache_init(void)
{
	int i;

	/* Initialise all entries */
	for (i = 0; i < MAXOPENFILES; i++) openfiles[i].handle = 0;

	/* Get the time we are initialised to use as a verifier */
	verifier[0] = (unsigned int)time(NULL);
	verifier[1] = 0;
}

static enum nstat filecache_flush(int index)
{
	enum nstat ret = NFS_OK;

	/* Write out any cached data */
	if (openfiles[index].write && openfiles[index].buffercount > 0) {
		os_error *err;

		err = _swix(OS_GBPB, _INR(0,4), 1, openfiles[index].handle,
		                                   openfiles[index].buffer,
		                                   openfiles[index].buffercount,
		                                   openfiles[index].bufferoffset);
		if (err) {
			syslogf(LOGNAME, LOG_SERIOUS, "Error writing cached data for file %s (%s)",
			        openfiles[index].name, err->errmess);
			ret = openfiles[index].writeerror = oserr_to_nfserr(err->errnum);
		}
		openfiles[index].buffercount = 0;
	}

	return ret;
}

static void filecache_evict(int index)
{
	os_error *err;

	/* Write out any cached data */
	filecache_flush(index);

	/* Close the file */
	err = _swix(OS_Find, _INR(0,1), 0, openfiles[index].handle);
	if (err) syslogf(LOGNAME, LOG_SERIOUS, "Error closing cached file %s (%s)",
	                 openfiles[index].name, err->errmess);

	if (err || (openfiles[index].writeerror != NFS_OK)) {
		/* An error occured that hasn't been returned to the client,
		   change the verifier as this is the only way it will now
		   be able to detect it. */
		verifier[1]++;
	}

	/* Invalidate entry */
	openfiles[index].handle = 0;
}

static enum nstat filecache_open(char *path, int *index, int dontopen)
{
	int i;
	clock_t now = clock();
	clock_t lasttime = 0;
	int earliest = 0;
	int available = -1;
	int handle;

	/* Search all entries for a matching filename */
	for (i = 0; i < MAXOPENFILES; i++) {
		if (openfiles[i].handle == 0) {
			available = i;
		} else if (strcmp(path, openfiles[i].name) == 0) {
			*index = i;
			openfiles[i].time = now;

			return NFS_OK;
		} else if (now - openfiles[i].time > lasttime) {
			lasttime = now - openfiles[i].time;
			earliest = i;
		}
	}
	if (available != -1) earliest = available;

	if (dontopen) {
		*index = -1;
		return NFS_OK;
	}

	/* Evict the oldest existing entry */
	if (openfiles[earliest].handle) filecache_evict(earliest);

	/* Open the file and cache the attributes */
	openfiles[earliest].time = now;
	openfiles[earliest].buffercount = 0;
	openfiles[earliest].bufferoffset = 0;
	openfiles[earliest].write = 0;
	openfiles[earliest].writeerror = NFS_OK;
	snprintf(openfiles[earliest].name, MAX_PATHNAME, "%s", path);
	OR(_swix(OS_File, _INR(0,1) | _OUTR(2,5), 17, path, &(openfiles[earliest].load),
	                                                    &(openfiles[earliest].exec),
	                                                    &(openfiles[earliest].filesize),
	                                                    &(openfiles[earliest].attr)));
	OR(_swix(OS_Find, _INR(0,1) | _OUT(0), 0xC3, path, &handle));

	/* Everything went OK, so mark the entry as valid */
	openfiles[earliest].handle = handle;

	*index = earliest;
	return NFS_OK;
}

void filecache_reap(int all)
{
	int i;
	clock_t now = clock();

	for (i = 0; i < MAXOPENFILES; i++) {
		/* Close a file if there has been no activity for a while */
		if (openfiles[i].handle != 0 && (all || (now - openfiles[i].time > 5 * CLOCKS_PER_SEC))) {
			filecache_evict(i);
		}
	}
}

enum nstat filecache_read(char *path, unsigned int count, unsigned int offset, char **data, unsigned int *read, int *eof)
{
	int index;
	unsigned int bufferread;
	unsigned int bufferoffset;

	NR(filecache_open(path, &index, 0));

	/* If the previous access was a write we need to flush any buffered
	   data before overwriting it */
	if (openfiles[index].write) filecache_flush(index);
	openfiles[index].write = 0;

	if (count > DATABUFFER) count = DATABUFFER;

	/* Ensure offset and count lie withing the file */
	if (offset > openfiles[index].filesize) {
		count = 0;
		offset = 0;
	}
	if (offset + count > openfiles[index].filesize) count = openfiles[index].filesize - offset;

	if (offset < openfiles[index].bufferoffset) {
		/* Buffer is too high */
		bufferoffset = 0;
		bufferread = DATABUFFER;
		openfiles[index].bufferoffset = offset;
	} else if (offset > openfiles[index].bufferoffset + openfiles[index].buffercount) {
		/* Buffer is too low */
		bufferoffset = 0;
		bufferread = DATABUFFER;
		openfiles[index].bufferoffset = offset;
	} else if (offset + count > openfiles[index].bufferoffset + openfiles[index].buffercount) {
		/* Buffer contains partial results */
		bufferoffset = openfiles[index].bufferoffset - offset;
		bufferread = openfiles[index].buffercount - bufferoffset;
		memmove(openfiles[index].buffer, openfiles[index].buffer + bufferoffset, bufferread);
		bufferoffset = bufferread;
		bufferread = DATABUFFER - bufferoffset;
		openfiles[index].bufferoffset = offset;
		openfiles[index].buffercount = bufferoffset;
	} else {
		/* Buffer contains full results */
		bufferoffset = 0;
		bufferread = 0;
	}

	/* Fill the buffer if we need */
	if (bufferread > 0) {
		OR(_swix(OS_GBPB, _INR(0,4) | _OUT(3), 3, openfiles[index].handle, openfiles[index].buffer + bufferoffset, bufferread, offset + bufferoffset, &bufferread));
		openfiles[index].buffercount = DATABUFFER - bufferread;
	}

	/* Return data from the buffer */
	bufferoffset = (offset - openfiles[index].bufferoffset);
	*data = openfiles[index].buffer + bufferoffset;
	*read = openfiles[index].buffercount - bufferoffset;
	if (*read > count) *read = count;
	if (eof) *eof = offset + *read >= openfiles[index].filesize;

	return NFS_OK;
}

enum nstat filecache_write(char *path, unsigned int count, unsigned int offset, char *data, int sync, char *verf)
{
	int index;

	if (verf) memcpy(verf, verifier, 8);

	NR(filecache_open(path, &index, 0));

	/* If the data in the buffer is from a read then discard it */
	if (openfiles[index].write == 0) {
		openfiles[index].write = 1;
		openfiles[index].bufferoffset = 0;
		openfiles[index].buffercount = 0;
	}

	/* Check for data that would be non-contiguous, or overflow the buffer */
	if (openfiles[index].buffercount > 0 &&
	    ((openfiles[index].bufferoffset + openfiles[index].buffercount != offset) ||
	     (openfiles[index].buffercount + count > DATABUFFER))) {
		NR(filecache_flush(index));
	}

	if (sync || count > DATABUFFER) {
		/* Write directly to disc */
		OR(_swix(OS_GBPB, _INR(0,4), 1, openfiles[index].handle, data, count, offset));
		if (sync) OR(_swix(OS_Args, _INR(0,1), 255, openfiles[index].handle));
	} else {
		/* Write to the buffer */
		memcpy(openfiles[index].buffer + openfiles[index].buffercount, data, count);
		if (openfiles[index].buffercount == 0) openfiles[index].bufferoffset = offset;
		openfiles[index].buffercount += count;
	}

	if (openfiles[index].filesize < offset + count) openfiles[index].filesize = offset + count;

	return NFS_OK;
}

enum nstat filecache_commit(char *path, char *verf)
{
	int index;
	enum nstat ret = NFS_OK;

	if (verf) memcpy(verf, verifier, 8);

	NR(filecache_open(path, &index, 1));

	if (index != -1) {
		filecache_flush(index);
		ret = openfiles[index].writeerror;
		openfiles[index].writeerror = NFS_OK;
	}

	return ret;
}

enum nstat filecache_getattr(char *path, unsigned int *load, unsigned int *exec, unsigned int *size, unsigned int *attr, int *cached)
{
	int index;

	NR(filecache_open(path, &index, 1));

	if (index == -1) {
		if (cached) *cached = 0;
	} else {
		if (cached) *cached = 1;

		*load = openfiles[index].load;
		*exec = openfiles[index].exec;
		*size = openfiles[index].filesize;
		*attr = openfiles[index].attr;
	}

	return NFS_OK;
}

enum nstat filecache_setattr(char *path, unsigned int load, unsigned int exec, unsigned int attr, unsigned int size, int setsize)
{
	int index;

	NR(filecache_open(path, &index, !setsize));

	if (index != -1) {
		openfiles[index].load = load;
		openfiles[index].exec = exec;
		openfiles[index].filesize = size;
		openfiles[index].attr = attr;
		if (setsize) OR(_swix(OS_Args, _INR(0,2), 3, openfiles[index].handle, size));
	}

	return NFS_OK;
}


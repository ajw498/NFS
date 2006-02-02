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


#define OR(x) do { \
	os_error *err = x; \
	if (err) { \
		syslogf(LOGNAME, LOG_ERROR, "Error: %x %s", err->errnum, err->errmess); \
		return oserr_to_nfserr(err->errnum); \
	} \
} while (0)

static enum nstat oserr_to_nfserr(int errnum)
{
	switch (errnum) {
	case 0x117b4: return NFSERR_NOTEMPTY;
	case 0x117c3: return NFSERR_ACCES;
	case 0x117c6: return NFSERR_NOSPC;
	case 0x80344a: return NFSERR_ROFS;
	case 0xb0: return (enum nstat)18; /*NFS3ERR_XDEV */
	}
	return NFSERR_IO;
}


#define MAXOPENFILES 3

#define DATABUFFER 512*1024

static struct openfile {
	char name[MAX_PATHNAME];
	int handle;
	clock_t time;
	char buffer[DATABUFFER];
	int bufferoffset;
	int buffercount;
} openfiles[MAXOPENFILES];

static int openfileinit = 0;

os_error *open_file(char *path, int *handle, int *index)
{
	int i;
	clock_t now = clock();
	clock_t lasttime = 0;
	int earliest = 0;
	int available = -1;
	os_error *err;

	if (!openfileinit) {
		for (i = 0; i < MAXOPENFILES; i++) openfiles[i].handle = 0;
		openfileinit = 1;
	}

	for (i = 0; i < MAXOPENFILES; i++) {
		if (openfiles[i].handle == 0) {
			available = i;
		} else if (strcmp(path, openfiles[i].name) == 0) {
			*handle = openfiles[i].handle;
			if (index) *index = i;
			openfiles[i].time = now;
			return NULL;
		} else if (now - openfiles[i].time > lasttime) {
			lasttime = now - openfiles[i].time;
			earliest = i;
		}
	}
	if (available != -1) earliest = available;

	if (openfiles[earliest].handle) {
		_swix(OS_Find, _INR(0,1), 0, handle);
		/*FIXME syslog errors*/
	}
	openfiles[earliest].time = now;
	snprintf(openfiles[earliest].name, MAX_PATHNAME, "%s", path);
	err = _swix(OS_Find, _INR(0,1) | _OUT(0), 0xC3, path, handle);
	openfiles[earliest].handle = err ? 0 : *handle;
	openfiles[earliest].buffercount = 0;
	openfiles[earliest].bufferoffset = 0;
	if (index) *index = earliest;
	return err;

}

void reap_files(int all)
{
	int i;
	clock_t now = clock();

	if (!openfileinit) return;

	for (i = 0; i < MAXOPENFILES; i++) {
		/* Close a file if there has been no activity for a second */
		if (openfiles[i].handle != 0 && (all || (now - openfiles[i].time > 5 * CLOCKS_PER_SEC))) {
			_swix(OS_Find, _INR(0,1), 0, openfiles[i].handle);
			/*FIXME syslog errors */
			openfiles[i].handle = 0;
		}
	}
}

enum nstat read_file(char *path, unsigned int count, unsigned int offset, char **data, unsigned int *read)
{
	int handle;
	int index;
	unsigned int read2;
	int bufferoffset;

	OR(open_file(path, &handle, &index));

	if (count > DATABUFFER) count = DATABUFFER;

	if ((offset < openfiles[index].bufferoffset) ||
	    ((offset + count) > (openfiles[index].bufferoffset + openfiles[index].buffercount))) {
		openfiles[index].bufferoffset = offset;
		openfiles[index].buffercount = 0;
/*		syslogf(LOGNAME, 1, "Reading %d %d", offset, count);*/
		OR(_swix(OS_GBPB, _INR(0,4) | _OUT(3), 3, handle, openfiles[index].buffer, DATABUFFER, offset, &read2));
		openfiles[index].buffercount = DATABUFFER - read2;
	}
	bufferoffset = (offset - openfiles[index].bufferoffset);
	*data = openfiles[index].buffer + bufferoffset;
	*read = openfiles[index].buffercount - bufferoffset;
	if (*read > count) *read = count;
	return NFS_OK;
}


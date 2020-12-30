/*
	Caching of file handles, and read/write buffers


	Copyright (C) 2006 Alex Waugh

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef FILECACHE_H
#define FILECACHE_H

#include "state.h"

extern unsigned int verifier[2];
extern int graceperiod;


void filecache_init(void);

void filecache_savegrace(void);

void filecache_reap(int all);

void filecache_removestate(uint64_t clientid);

nstat filecache_getfile(char *path, struct openfile **file);

nstat filecache_open(char *path, struct open_owner *open_owner, unsigned access, unsigned deny, unsigned *ownerseqid, char *other);

nstat filecache_opendowngrade(char *path, struct stateid *stateid, unsigned access, unsigned deny, unsigned *ownerseqid);

nstat filecache_closecache(char *path);

nstat filecache_close(char *path, struct stateid *stateid);

nstat filecache_read(char *path, struct stateid *stateid, unsigned int count, unsigned int offset, char **data, unsigned int *read, int *eof);

nstat filecache_write(char *path, struct stateid *stateid, unsigned int count, unsigned int offset, char *data, int sync, char *verf);

nstat filecache_commit(char *path, char *verf);

nstat filecache_getattr(char *path, unsigned int *load, unsigned int *exec, unsigned int *size, unsigned int *attr, int *cached);

nstat filecache_setattr(char *path, struct stateid *stateid, unsigned int load, unsigned int exec, unsigned int attr, unsigned int size, int setsize);

#endif


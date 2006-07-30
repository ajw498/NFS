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

union duplicate {
	enum nstat status;
	struct {
		enum nstat status;
		int stateidseq;
		char stateidother[12];
		unsigned rflags;
		unsigned attrset[2];
	} open;
};

struct stateid {
	unsigned clientid;
	char *owner;
	int ownerlen;
	int refcount;
	int ownerseq;
	unsigned seqid;
	int unconfirmed;
	union duplicate duplicate;
	time_t time;
	struct stateid *next;
};

void filecache_init(void);

void filecache_reap(int all);

enum nstat filecache_setclientid(char *id, int idlen, char *clientverf, uint64_t *clientid);

enum nstat filecache_setclientidconfirm(uint64_t clientid);

enum nstat filecache_renew(uint64_t clientid);

enum nstat filecache_createstateid(unsigned clientid, char *owner, int ownerlen, unsigned seqid, struct stateid **stateid, int *confirmrequired);

enum nstat filecache_checkseqid(struct stateid *stateid, unsigned seqid, int confirm, int *duplicate);

enum nstat filecache_getstateid(unsigned seqid, char *other, struct stateid **stateid);

enum nstat filecache_open(char *path, struct stateid *stateid, unsigned access, unsigned deny, int *ownerseqid, char *other);

enum nstat filecache_opendowngrade(char *path, struct stateid *stateid, unsigned access, unsigned deny);

enum nstat filecache_close(char *path, struct stateid *stateid);

enum nstat filecache_read(char *path, struct stateid *stateid, unsigned int count, unsigned int offset, char **data, unsigned int *read, int *eof);

enum nstat filecache_write(char *path, struct stateid *stateid, unsigned int count, unsigned int offset, char *data, int sync, char *verf);

enum nstat filecache_commit(char *path, char *verf);

enum nstat filecache_getattr(char *path, unsigned int *load, unsigned int *exec, unsigned int *size, unsigned int *attr, int *cached);

enum nstat filecache_setattr(char *path, struct stateid *stateid, unsigned int load, unsigned int exec, unsigned int attr, unsigned int size, int setsize);


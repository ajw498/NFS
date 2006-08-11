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

#ifndef STATE_H
#define STATE_H

#include "serverconn.h"

union duplicate {
	struct {
		enum nstat status;
		int stateidseq;
		char stateidother[12];
		unsigned rflags;
		unsigned attrset[2];
	} open;
	struct {
		enum nstat status;
	} open_downgrade;
	struct {
		enum nstat status;
	} close;
	struct {
		enum nstat status;
	} lock;
	struct {
		enum nstat status;
		unsigned seqid;
	} locku;
};

/* Identifies an entity on the client that has opened one or more files */
struct open_owner {
	uint64_t clientid;
	char *owner;
	int ownerlen;
	int refcount;
	unsigned seqid;
	int unconfirmed;
	union duplicate duplicate;
	time_t time;
	struct open_owner *next;
};

struct open_stateid {
	struct openfile *file;
	unsigned access;
	unsigned deny;
	unsigned seqid;
	struct open_owner *open_owner;
	struct open_stateid *next;
	struct open_stateid *hashnext;
};

struct lock_owner {
	uint64_t clientid;
	char *owner;
	int ownerlen;
	int refcount;
	unsigned seqid;
	union duplicate duplicate;
	struct lock_owner *next;
};

/* Bottom and top are inclusive */
struct lock {
	int write;
	unsigned bottom;
	unsigned top;
	struct lock *next;
};

struct lock_stateid {
	struct open_stateid *open_stateid;
	struct lock_owner *lock_owner;
	unsigned seqid;
	struct lock *locks;
	struct lock_stateid *next;
	struct lock_stateid *hashnext;
};

struct stateid {
	struct lock_stateid *lock;
	struct open_stateid *open;
};

struct stateid_other {
	void *id;
	unsigned verifier;
	int lock;
};

enum accesstype {
	ACC_READ,
	ACC_WRITE,
	ACC_EITHER
};

struct openfile {
	char name[MAX_PATHNAME]; /* Could make this dynamic? */
	int handle;
	unsigned int filesize;
	unsigned int load;
	unsigned int exec;
	unsigned int attr;
	struct open_stateid *open_stateids;
	struct lock_stateid *lock_stateids;
	struct openfile *next;
};

#define LL_ADD(start, item) do { \
	item->next = start; \
	start = item; \
} while (0)

#define LL_REMOVE(start, type, item) do { \
	if (start == item) { \
		start = item->next; \
	} else { \
		type *ptr = start; \
		while (ptr) { \
			if (ptr->next == item) { \
				ptr->next = item->next; \
				break; \
			} else { \
				ptr = ptr->next; \
			} \
		} \
		/* FIXME - if ptr == NULL syslog an error */ \
	} \
} while (0)

#define HASH_SIZE 64
#define HASH_BITS(ptr) ((((int)ptr) & 0x1F8) >> 3)

#define HASH_DECLARE(table, type) static type *table[HASH_SIZE]
#define HASH_INIT(table) do { \
	int i; \
	for (i = 0; i < HASH_SIZE; i++) { \
		table[i] = NULL; \
	} \
} while (0)

#define HASH_ADD(table, item) do { \
	int i = HASH_BITS(item); \
	item->hashnext = table[i]; \
	table[i] = item; \
} while (0)

#define HASH_REMOVE(table, type, item) do { \
	int i = HASH_BITS(item); \
	if (table[i] == item) { \
		table[i] = item->hashnext; \
	} else { \
		type *ptr = table[i]; \
		while (ptr) { \
			if (ptr->hashnext == item) { \
				ptr->hashnext = item->hashnext; \
				break; \
			} else { \
				ptr = ptr->hashnext; \
			} \
		} \
		/* FIXME - if ptr == NULL syslog an error */ \
	} \
} while (0)

#define HASH_GET(res, table, item)  do { \
	res = table[HASH_BITS(item)]; \
	while (res && res != item) res = res->hashnext; \
} while (0)

void state_init(void);

void state_removeclientstate(struct openfile *file, uint64_t clientid);

enum nstat state_newopenowner(uint64_t clientid, char *owner, int ownerlen, unsigned seqid, struct open_owner **open_owner, int *confirmrequired, int *duplicate);

enum nstat state_newlockowner(uint64_t clientid, char *owner, int ownerlen, unsigned seqid, struct lock_owner **lock_owner, int *duplicate);

enum nstat state_releaselockowner(uint64_t clientid, char *owner, int ownerlen);

enum nstat state_checkopenseqid(struct stateid *stateid, unsigned seqid, int openconfirm, int *duplicate);

enum nstat state_checklockseqid(struct stateid *stateid, unsigned seqid, int *duplicate);

enum nstat state_getstateid(unsigned seqid, char *other, struct stateid **stateid, struct server_conn *conn);

enum nstat state_checkpermissions(struct openfile *file, struct stateid *stateid, enum accesstype access);

enum nstat state_createopenstateid(struct openfile *file, struct open_owner *open_owner, unsigned access, unsigned deny, struct open_stateid **open_stateid);

void state_removeopenstateid(struct openfile *file, struct open_stateid *open_stateid);

enum nstat state_opendowngrade(struct stateid *stateid, unsigned access, unsigned deny, unsigned *ownerseqid);

enum nstat state_lock(struct open_stateid *open_stateid, struct lock_owner *lock_owner, int write, uint64_t offset, uint64_t length, struct lock_stateid **lock_stateid);

enum nstat state_unlock(struct stateid *stateid, uint64_t offset, uint64_t length);

void state_reap(int all, clock_t now);


#endif


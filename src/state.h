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

/* An open file is associated with an open owner. There can be many open
   files per open owner. A lock is associated with an open file and a
   lock owner. There can be many locks per file and many locks per lock
   owner. There can be many lock owners per open owner, and many open
   owners per lock owner.

   A stateid returned by the client can be a stateid resulting from an
   open or resulting from a lock, and we need to be able to distinguish
   between the two.

   There are three types of sequence ids.
   The first id is the sequence id in open, open confirm, open downgrade,
   lock, and close requests. This is one sequence per open owner, starts
   when the open owner is first created and continues until the open owner
   is deallocated.
   The second is in lock and locku requests which is one sequence per lock
   owner. It starts when the lock owner is first created by a lock request.
   The third is the sequence id in the stateid structure, and is unrelated
   to the first two (why couldn't it have been named differently?). It is
   incremented everytime the stateid represents a new state, i.e. an open
   downgrade, lock, locku is successful.
*/

/* The fields in the opaque 'other' part of a stateid */
struct stateid_other {
	void *id;
	unsigned verifier;
	int lock;
};

/* Record the results of the last request in the open or lock sequence.
   Allows duplicate answers to be generated on duplicate requests. */
union duplicate {
	struct {
		nstat status;
		int stateidseq;
		char stateidother[sizeof(struct stateid_other)];
		unsigned rflags;
		unsigned attrset[2];
	} open;
	struct {
		nstat status;
	} open_downgrade;
	struct {
		nstat status;
		union {
			struct {
				int write;
				uint64_t offset;
				uint64_t length;
				uint64_t clientid;
				char owner[1024];
				int ownerlen;
			} denied;
			struct {
				unsigned seqid;
				char other[sizeof(struct stateid_other)];
			} ok;
		} res;
	} lock;
	struct {
		nstat status;
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

/* A stateid created by an open request */
struct open_stateid {
	struct openfile *file;
	unsigned access;
	unsigned deny;
	unsigned seqid;
	struct open_owner *open_owner;
	struct open_stateid *next;
	struct open_stateid *hashnext;
};

/* Identifies an entity on the client that may have locked files.
   This is independant of the open owners, although they may be
   the same entity on the client. */
struct lock_owner {
	uint64_t clientid;
	char *owner;
	int ownerlen;
	int refcount;
	unsigned seqid;
	union duplicate duplicate;
	struct lock_owner *next;
};

/* An individual locked region. Bottom and top are inclusive */
struct lock {
	int write;
	unsigned bottom;
	unsigned top;
	struct lock *next;
};

/* A stateid created by an lock request */
struct lock_stateid {
	struct open_stateid *open_stateid;
	struct lock_owner *lock_owner;
	unsigned seqid;
	struct lock *locks;
	struct lock_stateid *next;
	struct lock_stateid *hashnext;
};

/* Internal stateid structure for either open or lock stateids */
struct stateid {
	struct lock_stateid *lock;
	struct open_stateid *open;
};

enum accesstype {
	ACC_READ,
	ACC_WRITE,
	ACC_EITHER
};

struct openfile {
	char *name;
	int handle;
	unsigned int filesize;
	unsigned int load;
	unsigned int exec;
	unsigned int attr;
	nstat writeerror;
	struct open_stateid *open_stateids;
	struct lock_stateid *lock_stateids;
	struct openfile *next;
};


/* Macros for manipulating state structures */

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
		if (ptr == NULL) syslogf(LOGNAME, LOG_SERIOUS, "Item to remove not found in list %s %d", __FILE__, __LINE__); \
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
		if (ptr == NULL) syslogf(LOGNAME, LOG_SERIOUS, "Item to remove not found in list %s %d", __FILE__, __LINE__); \
	} \
} while (0)

#define HASH_GET(res, table, item)  do { \
	res = table[HASH_BITS(item)]; \
	while (res && res != item) res = res->hashnext; \
} while (0)

void state_init(void);

void state_removeclientstate(struct openfile *file, uint64_t clientid);

nstat state_newopenowner(uint64_t clientid, char *owner, int ownerlen, unsigned seqid, struct open_owner **open_owner, int *confirmrequired, int *duplicate);

nstat state_newlockowner(uint64_t clientid, char *owner, int ownerlen, unsigned seqid, struct lock_owner **lock_owner);

nstat state_getlockowner(uint64_t clientid, char *owner, int ownerlen, struct lock_owner **lock_owner);

nstat state_releaselockowner(uint64_t clientid, char *owner, int ownerlen);

nstat state_checkopenseqid(struct stateid *stateid, unsigned seqid, int openconfirm, int *duplicate);

nstat state_checklockseqid(struct stateid *stateid, unsigned seqid, int *duplicate);

nstat state_getstateid(unsigned seqid, char *other, struct stateid **stateid, struct server_conn *conn);

nstat state_checkpermissions(struct openfile *file, struct stateid *stateid, enum accesstype access);

nstat state_createopenstateid(struct openfile *file, struct open_owner *open_owner, unsigned access, unsigned deny, struct open_stateid **open_stateid);

void state_removeopenstateid(struct openfile *file, struct open_stateid *open_stateid);

nstat state_opendowngrade(struct stateid *stateid, unsigned access, unsigned deny, unsigned *ownerseqid);

nstat state_lock(struct openfile *file, struct open_stateid *open_stateid,
                 struct lock_owner *lock_owner, int write, uint64_t offset,
                 uint64_t length, struct lock_stateid **lock_stateid,
                 int *deniedwrite, uint64_t *deniedoffset, uint64_t *deniedlength,
                 uint64_t *deniedclientid, char **deniedowner, int *deniedownerlen);

nstat state_unlock(struct stateid *stateid, uint64_t offset, uint64_t length);

nstat state_checklocks(struct openfile *file, struct stateid *stateid, int write, unsigned offset, unsigned length);

void state_reap(int all, clock_t now);


#endif


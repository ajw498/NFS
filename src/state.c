/*
	Handling of NFS4 open and locking state


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
#include "clientid.h"
#include "serverconn.h"


#define MAXCACHEDFILES 3

#define DATABUFFER 128*1024

static struct open_owner *open_owners = NULL;
static struct lock_owner *lock_owners = NULL;

HASH_DECLARE(open_hash, struct open_stateid);
HASH_DECLARE(lock_hash, struct lock_stateid);

static const char zeros[12] = {0,0,0,0,0,0,0,0,0,0,0,0};
static const char ones[12] = {0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF,0xF};

void state_init(void)
{
	HASH_INIT(open_hash);
	HASH_INIT(lock_hash);
}

void state_removeclientstate(struct openfile *file, uint64_t clientid)
{
	struct open_stateid *open_stateid = file->open_stateids;
	struct lock_owner *lock_owner = lock_owners;

	/* Free all this client's files */
	while (open_stateid) {
		struct open_stateid *next = open_stateid->next;

		if (open_stateid->open_owner->clientid == clientid) {
			struct stateid stateid;

			stateid.open = open_stateid;
			stateid.lock = NULL;
			/* Ignore any errors. OS errors will have been logged, and we
			   have nowhere to return the error to. */
			filecache_close(file->name, &stateid);
		}
		open_stateid = next;
	}

	/* Free all this client's lock_owners */
	while (lock_owner) {
		struct lock_owner *next = lock_owner->next;
		if (lock_owner->clientid == clientid) {
			if (lock_owner->refcount) {
				syslogf(LOGNAME, LOG_SERIOUS, "state_removeclientstate with lock_owner in use");
			} else {
				LL_REMOVE(lock_owners, struct lock_owner, lock_owner);
				free(lock_owner->owner);
				free(lock_owner);
			}
		}
		lock_owner = next;
	}
}

nstat state_newopenowner(uint64_t clientid, char *owner, int ownerlen, unsigned seqid, struct open_owner **open_owner, int *confirmrequired, int *duplicate)
{
	struct open_owner *id = open_owners;

	NR(clientid_renew(clientid));

	*duplicate = 0;

	/* Search for an existing owner with the same details */
	while (id) {
		if ((id->clientid == clientid) &&
		    (id->ownerlen == ownerlen) &&
		    (memcmp(id->owner, owner, ownerlen) == 0)) {
			*confirmrequired = id->unconfirmed;
			*open_owner = id;

			/* Check the sequence id is correct */
			if (seqid < id->seqid) {
				return NFSERR_BAD_SEQID;
			} else if (seqid == id->seqid) {
				*duplicate = 1;
			} else if (seqid == id->seqid + 1) {
				id->seqid = seqid;
			} else {
				return NFSERR_BAD_SEQID;
			}

			return NFS_OK;
		}
		id = id->next;
	}

	/* Not found so create a new owner */
	UR(id = malloc(sizeof(struct open_owner)));
	id->owner = malloc(ownerlen);
	if (id->owner == NULL) {
		free(id);
		UR(NULL);
	}
	memcpy(id->owner, owner, ownerlen);
	id->ownerlen = ownerlen;
	id->clientid = clientid;
	id->refcount = 0;
	id->seqid = seqid;
	id->unconfirmed = 1;
	id->time = clock();
	LL_ADD(open_owners, id);

	*open_owner = id;
	*confirmrequired = 1;
	return NFS_OK;
}

nstat state_newlockowner(uint64_t clientid, char *owner, int ownerlen, unsigned seqid, struct lock_owner **lock_owner)
{
	struct lock_owner *id = lock_owners;

	NR(clientid_renew(clientid));

	/* Search for an existing owner with the same details */
	while (id) {
		if ((id->clientid == clientid) &&
		    (id->ownerlen == ownerlen) &&
		    (memcmp(id->owner, owner, ownerlen) == 0)) {
			*lock_owner = id;

			/* Check the sequence id is correct */
			if (seqid != id->seqid + 1) return NFSERR_BAD_SEQID;
			id->seqid = seqid;

			return NFS_OK;
		}
		id = id->next;
	}

	/* Not found so create a new owner */
	UR(id = malloc(sizeof(struct lock_owner)));
	id->owner = malloc(ownerlen);
	if (id->owner == NULL) {
		free(id);
		UR(NULL);
	}
	memcpy(id->owner, owner, ownerlen);
	id->ownerlen = ownerlen;
	id->clientid = clientid;
	id->refcount = 0;
	id->seqid = seqid;
	LL_ADD(lock_owners, id);

	*lock_owner = id;
	return NFS_OK;
}

nstat state_getlockowner(uint64_t clientid, char *owner, int ownerlen, struct lock_owner **lock_owner)
{
	struct lock_owner *id = lock_owners;

	while (id) {
		if ((id->clientid == clientid) &&
		    (id->ownerlen == ownerlen) &&
		    (memcmp(id->owner, owner, ownerlen) == 0)) {
			*lock_owner = id;
			return NFS_OK;
		}
		id = id->next;
	}

	*lock_owner = NULL;
	return NFS_OK;
}

nstat state_releaselockowner(uint64_t clientid, char *owner, int ownerlen)
{
	struct lock_owner *id = lock_owners;

	/* Search for an existing owner with the same details */
	while (id) {
		if ((id->clientid == clientid) &&
		    (id->ownerlen == ownerlen) &&
		    (memcmp(id->owner, owner, ownerlen) == 0)) {
			if (id->refcount) return NFSERR_LOCKS_HELD;

			LL_REMOVE(lock_owners, struct lock_owner, id);
			free(id->owner);
			free(id);

			return NFS_OK;
		}
		id = id->next;
	}

	return NFS_OK;
}

nstat state_checkopenseqid(struct stateid *stateid, unsigned seqid, int openconfirm, int *duplicate)
{
	*duplicate = 0;

	if ((stateid == STATEID_NONE) || (stateid == STATEID_ANY)) return NFSERR_BAD_STATEID;

	if (seqid < stateid->open->open_owner->seqid) {
		return NFSERR_BAD_SEQID;
	} else if (seqid == stateid->open->open_owner->seqid) {
		*duplicate = 1;
	} else if (seqid == stateid->open->open_owner->seqid + 1) {
		if (stateid->open->open_owner->unconfirmed) {
			if (openconfirm) {
				stateid->open->open_owner->unconfirmed = 0;
				stateid->open->seqid++;
			} else {
				return NFSERR_BAD_SEQID;
			}
		}
		stateid->open->open_owner->seqid = seqid;
	} else {
		return NFSERR_BAD_SEQID;
	}

	return NFS_OK;
}

nstat state_checklockseqid(struct stateid *stateid, unsigned seqid, int *duplicate)
{
	*duplicate = 0;

	if ((stateid == STATEID_NONE) || (stateid == STATEID_ANY)) return NFSERR_BAD_STATEID;

	if (seqid < stateid->lock->lock_owner->seqid) {
		return NFSERR_BAD_SEQID;
	} else if (seqid == stateid->lock->lock_owner->seqid) {
		*duplicate = 1;
	} else if (seqid == stateid->lock->lock_owner->seqid + 1) {
		stateid->lock->lock_owner->seqid = seqid;
	} else {
		return NFSERR_BAD_SEQID;
	}

	return NFS_OK;
}

static nstat state_getlockrange(uint64_t offset, uint64_t length, unsigned *bottom, unsigned *top)
{
	/* A valid lock must stay with the 32bit range, unless it includes
	   everything upto the top of the 64bit range. */
	if (offset > 0xFFFFFFFFULL) return NFSERR_INVAL;
	*bottom = (unsigned)offset;

	if (length == 0) {
		return NFSERR_INVAL;
	} else if (length == 0xFFFFFFFFFFFFFFFFULL) {
		*top = 0xFFFFFFFFU;
	} else {
		if (length > 0x100000000ULL) {
			if (0xFFFFFFFFFFFFFFFFULL - length != offset) return NFSERR_BAD_RANGE;
			*top = 0xFFFFFFFFU;
		} else {
			if (offset + length > 0x100000000ULL) return NFSERR_BAD_RANGE;
			*top = (unsigned)(offset + length - 1);
		}
	}
	return NFS_OK;
}

nstat state_checklocks(struct openfile *file, struct stateid *stateid, int write, unsigned offset, unsigned length)
{
	struct lock_stateid *current = file->lock_stateids;
	struct lock_owner *lock_owner = NULL;

	if ((file == NULL) || (stateid == STATEID_ANY)) return NFS_OK;

	if ((stateid != STATEID_NONE) && (stateid->lock != NULL)) {
		lock_owner = stateid->lock->lock_owner;
	}

	/* Search for matching locks */
	while (current) {
		if (current->lock_owner != lock_owner) {
			struct lock *lock = current->locks;
			while (lock) {
				if ((offset <= lock->top) && (offset + length >= lock->bottom)) {
					/* Overlapping lock found, check if it is conflicting */
					if (lock->write || write) {
						return NFSERR_LOCKED;
					}
				}
				lock = lock->next;
			}
		}
		current = current->next;
	}
	return NFS_OK;
}

nstat state_lock(struct openfile *file, struct open_stateid *open_stateid,
                 struct lock_owner *lock_owner, int write, uint64_t offset,
                 uint64_t length, struct lock_stateid **lock_stateid,
                 int *deniedwrite, uint64_t *deniedoffset, uint64_t *deniedlength,
                 uint64_t *deniedclientid, char **deniedowner, int *deniedownerlen)
{
	struct lock_stateid *current = file->lock_stateids;
	struct lock_stateid *matching = NULL;
	struct lock *newlock = NULL;
	unsigned bottom;
	unsigned top;

	NR(state_getlockrange(offset, length, &bottom, &top));

	/* Search for existing locks */
	while (current) {
		if (current->lock_owner == lock_owner) {
			matching = current;
		} else {
			struct lock *lock = current->locks;
			while (lock) {
				if ((lock->bottom <= top) && (lock->top >= bottom)) {
					/* Overlapping lock found, check if it is conflicting */
					if (lock->write || write) {
						*deniedwrite = lock->write;
						*deniedoffset = lock->bottom;
						*deniedlength = ((uint64_t)lock->top - (uint64_t)lock->bottom) + 1ULL;
						*deniedclientid = current->lock_owner->clientid;
						*deniedowner = current->lock_owner->owner;
						*deniedownerlen = current->lock_owner->ownerlen;
						return NFSERR_DENIED;
					}
				}
				lock = lock->next;
			}
		}
		current = current->next;
	}

	/* No conflicting locks were found. If this was just a test for a lock
	   then return, else add the lock */
	if (open_stateid == NULL) return NFS_OK;

	/* Allocate the new lock here so that if the allocation fails we won't
	   have modified any existing locks */
	UR(newlock = malloc(sizeof(struct lock)));
	newlock->write = write;
	newlock->bottom = bottom;
	newlock->top = top;

	if (matching) {
		/* To implement POSIX type locking, we must search through existing
		   locks owned by this owner, and merge any that overlap with the
		   new one. The records are not actually merged, they are just
		   adjusted so that they are adjacent and there are no overlaps. */
		struct lock *lock = matching->locks;

		*lock_stateid = matching;
		while (lock) {
			struct lock *next = lock->next;

			if (lock->bottom < bottom) {
				if (top > top) {
					/* New lock completely within an existing lock */
					if (lock->write != write) {
						/* Different types, so have to split the old lock */
						struct lock *split = malloc(sizeof(struct lock));
						if (split == NULL) {
							/* If we get here, we cannot have modified any
							   other existing locks. To do so would have
							   required two existing locks overlapping each
							   other */
							free(newlock);
							UR(NULL);
						}
						split->write = lock->write;
						split->bottom = top + 1;
						split->top = lock->top;
						LL_ADD(matching->locks, split);
						lock->top = bottom - 1;
						LL_ADD(matching->locks, newlock);
					} else {
						/* Same type, so new lock can be ignored */
						free(newlock);
					}
					return NFS_OK;
				} else if (lock->top >= bottom) {
					/* Partial overlap at bottom of new lock */
					lock->top = bottom - 1;
				}
			} else if (lock->bottom <= top) {
				if (lock->top <= top) {
					/* Old lock completely within new lock, so remove it */
					LL_REMOVE(matching->locks, struct lock, lock);
					free(lock);
				} else {
					/* Partial overlap at top of new lock */
					lock->bottom = top + 1;
				}
			}
			lock = next;
		}
	} else {
		/* No existing stateid for this owner/file pair, so create one */
		*lock_stateid = matching = malloc(sizeof(struct lock_stateid));
		if (matching == NULL) {
			free(newlock);
			UR(NULL);
		}
		matching->open_stateid = open_stateid;
		matching->lock_owner = lock_owner;
		matching->locks = NULL;
		matching->seqid = 0;
		LL_ADD(file->lock_stateids, matching);
		HASH_ADD(lock_hash, matching);
		lock_owner->refcount++;
	}

	LL_ADD(matching->locks, newlock);
	return NFS_OK;
}

nstat state_unlock(struct stateid *stateid, uint64_t offset, uint64_t length)
{
	struct lock *lock = stateid->lock->locks;
	unsigned bottom;
	unsigned top;

	NR(state_getlockrange(offset, length, &bottom, &top));

	/* Search for existing locks */
	while (lock) {
		struct lock *next = lock->next;

		if (lock->bottom < bottom) {
			if (lock->top > top) {
				/* Removed lock completely within an existing lock, so have
				   to split the lock */
				struct lock *split;

				UR(split = malloc(sizeof(struct lock)));
				split->write = lock->write;
				split->bottom = top + 1;
				split->top = lock->top;
				LL_ADD(stateid->lock->locks, split);
				lock->top = bottom - 1;
				return NFS_OK;
			} else if (lock->top >= bottom) {
				/* Partial overlap at bottom of removed lock */
				lock->top = bottom - 1;
			}
		} else if (lock->bottom <= top) {
			if (lock->top <= top) {
				/* Old lock completely within new lock, so remove it */
				LL_REMOVE(stateid->lock->locks, struct lock, lock);
				free(lock);
			} else {
				/* Partial overlap at top of new lock */
				lock->bottom = top + 1;
			}
		}
		lock = next;
	}

	return NFS_OK;
}

nstat state_getstateid(unsigned seqid, char *other, struct stateid **stateid, struct server_conn *conn)
{
	struct stateid_other *other2 = (struct stateid_other *)other;


	if ((seqid == 0) && (memcmp(other, zeros, 12) == 0)) {
		*stateid = STATEID_NONE;
		return NFS_OK;
	}

	if ((seqid == 0xFFFFFFFF) && (memcmp(other, ones, 12) == 0)) {
		*stateid = STATEID_ANY;
		return NFS_OK;
	}

	if (other2->verifier != verifier[0]) {
		return NFSERR_STALE_STATEID;
	}

	UR(*stateid = palloc(sizeof(struct stateid), conn->pool));

	if (other2->lock) {
		/* This is a lock stateid */
		struct lock_stateid *id;

		HASH_GET(id, lock_hash, other2->id);
		if (id == NULL) return NFSERR_EXPIRED;
		if (id->seqid != seqid) return NFSERR_OLD_STATEID;
		(*stateid)->lock = id;
		(*stateid)->open = id->open_stateid;
	} else {
		/* This is an open stateid */
		struct open_stateid *id;

		HASH_GET(id, open_hash, other2->id);
		if (id == NULL) return NFSERR_EXPIRED;
		if (id->seqid != seqid) return NFSERR_OLD_STATEID;
		(*stateid)->lock = NULL;
		(*stateid)->open = id;
	}

	NR(clientid_renew((*stateid)->open->open_owner->clientid));

	return NFS_OK;
}

nstat state_checkpermissions(struct openfile *file, struct stateid *stateid, enum accesstype access)
{
	if ((file == NULL) || (stateid == STATEID_ANY)) return NFS_OK;

	if (stateid == STATEID_NONE) {
		struct open_stateid *id = file->open_stateids;

		while (id) {
			if (id->deny & (access == ACC_READ ? 1 : 2)) {
				return NFSERR_OPENMODE;
			}
			id = id->next;
		}
	}

	if (stateid->open->file != file) return NFSERR_BAD_STATEID;
	if (access == ACC_EITHER) return NFS_OK;

	return (stateid->open->access & (access == ACC_READ ? 1 : 2)) ? NFS_OK : NFSERR_OPENMODE;
}

nstat state_createopenstateid(struct openfile *file, struct open_owner *open_owner, unsigned access, unsigned deny, struct open_stateid **open_stateid)
{
	struct open_stateid *current = file->open_stateids;
	struct open_stateid *matching = NULL;
	unsigned currentaccess = 0;
	unsigned currentdeny = 0;

	/* Combine all existing shares */
	while (current) {
		if (current->open_owner == open_owner) {
			matching = current;
		}
		currentaccess |= current->access;
		currentdeny |= current->deny;
		current = current->next;
	}

	/* Check share request is satisfiable */
	if (access == 0) return NFSERR_INVAL;
	if ((access & currentdeny) || (deny & currentaccess)) return NFSERR_DENIED;

	if (matching) {
		/* There is already a stateid for this open owner, so just
		   upgrade the share details */
		matching->access |= access;
		matching->deny |= deny;
		*open_stateid = matching;
	} else {
		/* No existing stateid for this owner, so create a new one */
		struct open_stateid *newstateid;
		UR(newstateid = malloc(sizeof(struct open_stateid)));
		newstateid->file = file;
		newstateid->open_owner = open_owner;
		newstateid->access = access;
		newstateid->deny = deny;
		newstateid->seqid = 0;
		LL_ADD(file->open_stateids, newstateid);
		HASH_ADD(open_hash, newstateid);
		open_owner->refcount++;
		*open_stateid = newstateid;
	}
	return NFS_OK;
}

static void state_removesingleopenstateid(struct openfile *file, struct open_stateid *open_stateid)
{
	struct lock_stateid *lock_stateid = file->lock_stateids;
	while (lock_stateid) {
		struct lock_stateid *next = lock_stateid->next;

		if (lock_stateid->open_stateid == open_stateid) {
			while (lock_stateid->locks) {
				struct lock *lock = lock_stateid->locks;
				LL_REMOVE(lock_stateid->locks, struct lock, lock);
				free(lock);
			}
			LL_REMOVE(file->lock_stateids, struct lock_stateid, lock_stateid);
			HASH_REMOVE(lock_hash, struct lock_stateid, lock_stateid);
			lock_stateid->lock_owner->refcount--;
			free(lock_stateid);
		}
		lock_stateid = next;
	}

	LL_REMOVE(file->open_stateids, struct open_stateid, open_stateid);
	HASH_REMOVE(open_hash, struct open_stateid, open_stateid);
	open_stateid->open_owner->refcount--;
	free(open_stateid);
}

void state_removeopenstateid(struct openfile *file, struct open_stateid *open_stateid)
{
	if (open_stateid == NULL) {
		/* Remove all stateids */
		while (file->open_stateids) {
			state_removesingleopenstateid(file, file->open_stateids);
		}
	} else {
		state_removesingleopenstateid(file, open_stateid);
	}
}

nstat state_opendowngrade(struct stateid *stateid, unsigned access, unsigned deny, unsigned *ownerseqid)
{
	/* Upgrading access is not permitted */
	if ((access & ~stateid->open->access) ||
	    (deny & ~stateid->open->deny)) {
		return NFSERR_INVAL;
	}
	stateid->open->access = access;
	stateid->open->deny = deny;
	*ownerseqid = stateid->open->seqid++;

	return NFS_OK;
}

void state_reap(int all, clock_t now)
{
	struct open_owner *open_owner = open_owners;
	struct open_owner **previd = &open_owners;

	/* Remove all timed out open_owners. Only remove unused ones, all the
	   rest will be removed when the clientids are removed */
	while (open_owner) {
		if ((open_owner->refcount == 0) && (all || (now - open_owner->time > 2 * (LEASE_TIMEOUT * CLOCKS_PER_SEC)))) {
			struct open_owner *next = open_owner->next;

			*previd = next;
			free(open_owner->owner);
			free(open_owner);
			open_owner = next;
		} else {
			previd = &(open_owner->next);
			open_owner = open_owner->next;
		}
	}

	if (all) {
		/* Remove all unused lock_owners */
		while (lock_owners) {
			struct lock_owner *lock_owner = lock_owners;
			if (lock_owner->refcount) syslogf(LOGNAME, LOG_SERIOUS, "state_reap lock_owner with non-zero refcount");
			LL_REMOVE(lock_owners, struct lock_owner, lock_owner);
			free(lock_owner->owner);
			free(lock_owner);
		}
	}

	if (all) {
		/* Sanity checking */
		int i;

		for (i = 0; i < HASH_SIZE; i++) {
			if (open_hash[i]) syslogf(LOGNAME, LOG_SERIOUS, "state_reap open stateids still hashed");
			if (lock_hash[i]) syslogf(LOGNAME, LOG_SERIOUS, "state_reap lock stateids still hashed");
		}
		if (open_owners) syslogf(LOGNAME, LOG_SERIOUS, "state_reap open_owners still allocated");
		if (lock_owners) syslogf(LOGNAME, LOG_SERIOUS, "state_reap lock_owners still allocated");
	}
}


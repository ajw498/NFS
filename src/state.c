/*
	$Id$

	Handling of NFS4 open and locking state


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

static unsigned verifier;

void state_init(void)
{
	HASH_INIT(open_hash);
	HASH_INIT(lock_hash);
	verifier = (unsigned)time(NULL); /* FIXME - use a global var */
}

void state_removeclientstate(struct openfile *file, uint64_t clientid)
{
	struct open_stateid *open_stateid = file->open_stateids;

	while (open_stateid) {
		struct open_stateid *next = open_stateid->next;

		if (open_stateid->open_owner->clientid == clientid) {
			struct stateid stateid;

			stateid.open = open_stateid;
			stateid.lock = NULL;
			filecache_close(file->name, &stateid);
			/* FIXME log failure? */
		}
		open_stateid = next;
	}
}

enum nstat state_newopenowner(uint64_t clientid, char *owner, int ownerlen, unsigned seqid, struct open_owner **open_owner, int *confirmrequired, int *duplicate)
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

enum nstat state_checkopenseqid(struct stateid *stateid, unsigned seqid, int openconfirm, int *duplicate)
{
	*duplicate = 0;

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

enum nstat state_getstateid(unsigned seqid, char *other, struct stateid **stateid, struct server_conn *conn)
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

	if (other2->verifier != verifier) {
		return NFSERR_STALE_STATEID;
	}

	UR(*stateid = palloc(sizeof(struct stateid), conn->pool));

	if (other2->lock) {
		/* This is a lock stateid */
		struct lock_stateid *id;

		HASH_GET(id, lock_hash, other2->id);
		if (id == NULL) return NFSERR_OLD_STATEID;
		if (id->seqid != seqid) return NFSERR_OLD_STATEID;
		(*stateid)->lock = id;
		(*stateid)->open = id->open_stateid;
	} else {
		/* This is an open stateid */
		struct open_stateid *id;

		HASH_GET(id, open_hash, other2->id);
		if (id == NULL) return NFSERR_OLD_STATEID;
		if (id->seqid != seqid) return NFSERR_OLD_STATEID;
		(*stateid)->lock = NULL;
		(*stateid)->open = id;
	}

	NR(clientid_renew((*stateid)->open->open_owner->clientid));

	return NFS_OK;
}

enum nstat state_checkpermissions(struct openfile *file, struct stateid *stateid, enum accesstype access)
{
	if ((file == NULL) || (stateid == STATEID_ANY)) return NFS_OK;

	if (stateid == STATEID_NONE) {
		struct open_stateid *id = file->open_stateids;

		while (id) {
			if (id->deny & (access == ACC_READ ? 1 : 2)) {
				return NFSERR_LOCKED; /*FIXME NFS2/3 error? */
			}
			id = id->next;
		}
	}

	if (stateid->open->file != file) return NFSERR_BAD_STATEID;
	if (access == ACC_EITHER) return NFS_OK;

	return (stateid->open->access & (access == ACC_READ ? 1 : 2)) ? NFS_OK : NFSERR_LOCKED;
}

enum nstat state_createopenstateid(struct openfile *file, struct open_owner *open_owner, unsigned access, unsigned deny, struct open_stateid **open_stateid)
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
		matching->access |= access;
		matching->deny |= deny;
		*open_stateid = matching;
	} else {
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
	LL_REMOVE(file->open_stateids, struct open_stateid, open_stateid);
	HASH_REMOVE(open_hash, struct open_stateid, open_stateid);
	open_stateid->open_owner->refcount--;
	free(open_stateid);
}

void state_removeopenstateid(struct openfile *file, struct open_stateid *open_stateid)
{
	if (open_stateid == NULL) {
		while (file->open_stateids) {
			state_removesingleopenstateid(file, file->open_stateids);
		}
	} else {
		state_removesingleopenstateid(file, open_stateid);
	}
}

enum nstat state_opendowngrade(struct stateid *stateid, unsigned access, unsigned deny, unsigned *ownerseqid)
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

/*FIXME	if (all && open_hash) syslogf(LOGNAME, LOG_SERIOUS, "filecache_reap files still open");*/
	if (all && open_owners) syslogf(LOGNAME, LOG_SERIOUS, "state_reap open_owners still allocated");
}

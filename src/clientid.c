/*
	Clientid handling for NFS4


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


struct clientid {
	uint64_t clientid;
	int unconfirmed;
	time_t lastactivity;
	char clientverf[8];
	char *id;
	int idlen;
	struct clientid *next;
};

static unsigned nextclientid = 0;
static struct clientid *clients = NULL;

nstat clientid_setclientid(char *cid, int cidlen, char *clientverf, uint64_t *clientid)
{
	struct clientid *confirmed = NULL;
	struct clientid *unconfirmed = NULL;
	struct clientid *id = clients;

	/* Search the list for existing ids. There should be at most one
	   confirmed and one unconfirmed. */
	while (id) {
		if ((id->idlen == cidlen) && (memcmp(id->id, cid, cidlen) == 0)) {
			if (id->unconfirmed) {
				unconfirmed = id;
				if (confirmed) break;
			} else {
				confirmed = id;
				if (unconfirmed) break;
			}
		}
		id = id->next;
	}

	if (unconfirmed) {
		/* Use existing unconfirmed entry */
		unconfirmed->clientid = nextclientid++ | (((uint64_t)verifier[0]) << 32);
		unconfirmed->lastactivity = clock();
		memcpy(unconfirmed->clientverf, clientverf, 8);
		*clientid = unconfirmed->clientid;
	} else {
		/* Allocate a new unconfirmed entry */
		UR(unconfirmed = malloc(sizeof(struct clientid)));
		unconfirmed->idlen = cidlen;
		unconfirmed->id = malloc(cidlen);
		if (unconfirmed->id == NULL) {
			free(unconfirmed);
			UR(NULL);
		}
		memcpy(unconfirmed->clientverf, clientverf, 8);
		memcpy(unconfirmed->id, cid, cidlen);
		unconfirmed->lastactivity = clock();
		unconfirmed->unconfirmed = 1;
		unconfirmed->next = clients;
		clients = unconfirmed;

		if (confirmed) {
			if (memcmp(confirmed->clientverf, clientverf, 8) == 0) {
				/* This is an update, so use the same id as before */
				*clientid = unconfirmed->clientid = confirmed->clientid;
			} else {
				/* This is a client reboot, so use a new id */
				*clientid = unconfirmed->clientid = nextclientid++ | (((uint64_t)verifier[0]) << 32);
			}
		} else {
			/* This is a new client, so use a new id */
			*clientid = unconfirmed->clientid = nextclientid++ | (((uint64_t)verifier[0]) << 32);
		}
	}

	return NFS_OK;
}

nstat clientid_setclientidconfirm(uint64_t clientid)
{
	struct clientid *confirmed = NULL;
	struct clientid *unconfirmed = NULL;
	struct clientid *id = clients;

	/* Search the list for existing ids. There should be at most one
	   confirmed and one unconfirmed. */
	while (id) {
		if (id->clientid == clientid) {
			if (id->unconfirmed) {
				unconfirmed = id;
				if (confirmed) break;
			} else {
				confirmed = id;
				if (unconfirmed) break;
			}
		}
		id = id->next;
	}

	if ((confirmed == NULL) && (unconfirmed == NULL)) {
		return NFSERR_STALE_CLIENTID;
	} else {
		if (confirmed && unconfirmed) {
			/* Remove the old confirmed entry */
			if (clients == confirmed) {
				clients = confirmed->next;
			} else {
				id = clients;
				while (id->next != confirmed) id = id->next;
				id->next = confirmed->next;
			}
			free(confirmed->id);
			free(confirmed);
			/* The unconfirmed entry is now confirmed */
			unconfirmed->unconfirmed = 0;
			unconfirmed->lastactivity = clock();
		} else if (confirmed) {
			/* Must be a duplicate, so return ok */
			return NFS_OK;
		} else {
			struct clientid **prev = &clients;
			/* The unconfirmed entry is now confirmed */
			unconfirmed->unconfirmed = 0;
			unconfirmed->lastactivity = clock();
			/* The wasn't a previously confirmed entry, so
			   remove any state from previous incarnations
			   of the same client */
			id = clients;
			while (id) {
				struct clientid *next = id->next;
				if ((id->idlen == unconfirmed->idlen) &&
				    (memcmp(id->id, unconfirmed->id, id->idlen) == 0) &&
				    (memcmp(id->clientverf, unconfirmed->clientverf, 8) != 0)) {
					*prev = id->next;
					filecache_removestate(id->clientid);
					free(id->id);
					free(id);
				} else {
					prev = &(id->next);
				}
				id = next;
			}
		}
	}
	return NFS_OK;
}

nstat clientid_renew(uint64_t clientid)
{
	struct clientid *id = clients;

	while (id) {
		if ((id->clientid == clientid) && !id->unconfirmed) {
			id->lastactivity = clock();
			return NFS_OK;
		}
		id = id->next;
	}

	if (clientid >> 32 == verifier[0]) return NFSERR_EXPIRED;

	return NFSERR_STALE_CLIENTID;
}

void clientid_reap(int all)
{
	clock_t now = clock();
	struct clientid *cid = clients;
	struct clientid **prevcid = &clients;

	/* Remove all timed out clientids */
	while (cid) {
		struct clientid *next = cid->next;

		if (all || (now - cid->lastactivity > 2 * (LEASE_TIMEOUT * CLOCKS_PER_SEC))) {
			*prevcid = cid->next;
			filecache_removestate(cid->clientid);
			free(cid->id);
			free(cid);
		} else {
			prevcid = &(cid->next);
		}
		cid = next;
	}

	if (all && clients) syslogf(LOGNAME, LOG_SERIOUS, "clientid_reap clients still allocated");
}



/*
	$Id$

	Browse for available mounts


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


#include "rpc-calls.h"
#include "portmapper-calls.h"
#include "pcnfsd-calls.h"
#include "mount1-calls.h"
#include "mount3-calls.h"

#include "sunfish.h"
#include "browse.h"

#include <sys/param.h>
#include <unistd.h>
#include <stdio.h>

int enablelog = 0;

static os_error *browse_initconn(struct conn_info *conn, const char *host, int tcp, struct pool *pool)
{
	conn->config = NULL;
	conn->portmapper_port = PMAP_PORT;
	conn->mount_port = 0;
	conn->nfs_port = 0;
	conn->pcnfsd_port = 0;
	conn->tcp = tcp;
	conn->timeout = 200;
	conn->retries = 1;
	conn->username = NULL;
	conn->password = NULL;
	conn->uid = 0;
	conn->gid = 0;
	conn->numgids = 0;
	conn->logging = 0;
	conn->auth = NULL;
	conn->machinename = NULL;
	conn->localportmin = LOCALPORTMIN_DEFAULT;
	conn->localportmax = LOCALPORTMAX_DEFAULT;
	conn->maxdatabuffer = 0;
	conn->pipelining = 0;
	conn->nfs3 = 0;
	conn->toenc = (iconv_t)-1;
	conn->fromenc = (iconv_t)-1;
	if (pool) {
		conn->pool = pool;
	} else {
		if ((conn->pool = pinit(NULL)) == NULL) {
			return gen_error(NOMEM,NOMEMMESS);
		}
	}
	conn->txmutex = 0;
	conn->rxmutex = NULL;
	conn->reference = 0;
	conn->server = (char*)host;
	conn->export = "";

	return rpc_init_connection(conn);
}

char *browse_gethost(struct hostinfo *info, enum broadcast_type type, const char *hostname)
{
	pmaplist2 res;
	pmaplist list;
	os_error *err;
	static struct conn_info broadcastconn;
	enum callctl blocking = RXNONBLOCKING;

	if (type == BROADCAST) {
		err = browse_initconn(&broadcastconn, NULL, 0, NULL);
		if (err) return err->errmess;
		blocking = TXNONBLOCKING;
	}

	if (type == CLOSE) {
		err = rpc_close_connection(&broadcastconn);
		pfree(broadcastconn.pool);
		return err ? err->errmess : NULL;
	}

	if (type == HOST) {
		err = browse_initconn(&broadcastconn, hostname, 0, NULL);
		if (err) return err->errmess;
		blocking = TXBLOCKING;
	}

	err = PMAPPROC_DUMP(&res, &broadcastconn, blocking);

	if (type == HOST) {
		rpc_close_connection(&broadcastconn);
		pfree(broadcastconn.pool);
	}

	if (err) {
		if (err == ERR_WOULDBLOCK) {
			info->valid = 0;
			return NULL;
		} else {
			return err->errmess;
		}
	}

	info->valid = 1;
	strcpy(info->host, rpc_get_last_host());
	info->mount1tcpport = 0;
	info->mount1udpport = 0;
	info->mount3tcpport = 0;
	info->mount3udpport = 0;
	info->pcnfsdtcpport = 0;
	info->pcnfsdudpport = 0;

	list = res.list;
	while (list) {
		switch (list->map.prog) {
		case MOUNT_RPC_PROGRAM:
			if (list->map.vers == 1) {
				if (list->map.prot == IPPROTO_UDP) {
					info->mount1udpport = list->map.port;
				} else if (list->map.prot == IPPROTO_TCP) {
					info->mount1tcpport = list->map.port;
				}
			} else if (list->map.vers == 3) {
				if (list->map.prot == IPPROTO_UDP) {
					info->mount3udpport = list->map.port;
				} else if (list->map.prot == IPPROTO_TCP) {
					info->mount3tcpport = list->map.port;
				}
			}
			break;
		case PCNFSD_RPC_PROGRAM:
			if (list->map.vers == PCNFSD_RPC_VERSION) {
				if (list->map.prot == IPPROTO_UDP) {
					info->pcnfsdudpport = list->map.port;
				} else if (list->map.prot == IPPROTO_TCP) {
					info->pcnfsdtcpport = list->map.port;
				}
			}
			break;
		}
		list = list->next;
	}
	return NULL;
}


char *browse_getexports(const char *host, unsigned port, unsigned mount3, unsigned tcp, struct exportinfo **ret)
{
	os_error *err;
	struct conn_info conn;
	static struct pool *pool = NULL;

	if (pool == NULL) pool = pinit(NULL);
	if (pool == NULL) return NOMEMMESS;
	pclear(pool);

	err = browse_initconn(&conn, host, tcp, pool);
	if (err) return err->errmess;
	conn.mount_port = port;
	conn.timeout = 2;
	conn.retries = 3;

	if (mount3) {
		exportlist32 res;
		exportlist3 list;

		err = MOUNTPROC3_EXPORT(&res, &conn);
		if (err) {
			rpc_close_connection(&conn);
			return err->errmess;
		}

		list = res.list;
		while (list) {
			*ret = palloc(sizeof(struct exportinfo), pool);
			(*ret)->exportname = palloc(list->filesys.size + 1, pool);
			memcpy((*ret)->exportname, list->filesys.data, list->filesys.size);
			(*ret)->exportname[list->filesys.size] = '\0';
			ret = &((*ret)->next);
			list = list->next;
		}
	} else {
		exportlist2 res;
		exportlist list;

		err = MNTPROC_EXPORT(&res, &conn);
		if (err) {
			rpc_close_connection(&conn);
			return err->errmess;
		}

		list = res.list;
		while (list) {
			*ret = palloc(sizeof(struct exportinfo), pool);
			(*ret)->exportname = palloc(list->filesys.size + 1, pool);
			memcpy((*ret)->exportname, list->filesys.data, list->filesys.size);
			(*ret)->exportname[list->filesys.size] = '\0';
			ret = &((*ret)->next);
			list = list->next;
		}
	}
	*ret = NULL;

	err = rpc_close_connection(&conn);
	return err ? err->errmess : NULL;
}

/* Encode a username or password for pcnfsd */
static void encode(char *str)
{
	while (*str) {
		*str = *str ^ 0x5b;
		str++;
	}
}

char *browse_lookuppassword(const char *host, unsigned port, unsigned tcp, const char *username, const char *password, unsigned *uid, unsigned *umask, char *gids, size_t gidsize)
{
	struct auth_args args;
	struct auth_res res;
	char machinename[MAXHOSTNAMELEN];
	struct conn_info conn;
	os_error *err;

	err = browse_initconn(&conn, host, tcp, NULL);
	if (err) return err->errmess;
	conn.pcnfsd_port = port;
	conn.timeout = 2;
	conn.retries = 3;

	gethostname(machinename, MAXHOSTNAMELEN);
	args.system.data = machinename;
	args.system.size = strlen(machinename);
	char *encusername = pstrdup(username, conn.pool);
	encode(encusername);
	args.id.data = encusername;
	args.id.size = strlen(encusername);
	char *encpassword = pstrdup(password, conn.pool);
	encode(encpassword);
	args.pw.data = encpassword;
	args.pw.size = strlen(encpassword);
	args.comment.size = 0;

	err = PCNFSD_AUTH(&args, &res, &conn);
	if (err) {
		rpc_close_connection(&conn);
		pfree(conn.pool);
		return err->errmess;
	}
	if (res.stat != AUTH_RES_OK) {
		rpc_close_connection(&conn);
		pfree(conn.pool);
		return "PCNFSD authentication failed (Incorrect username/password)";
	}
	*uid = res.uid;
	*umask = res.def_umask;
	int written = snprintf(gids, gidsize, "%d", res.gid);

	for (int i = 0; i < res.gids.size; i++) {
		if (written > gidsize) break;
		gids += written;
		gidsize -= written;
		written = snprintf(gids, gidsize, "%d", res.gids.data[i]);
	}

	return NULL;
}


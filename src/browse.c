/*
	Browse for available mounts


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


#include "rpc-calls.h"
#include "portmapper-calls.h"
#include "pcnfsd-calls.h"
#include "mount1-calls.h"
#include "mount3-calls.h"
#include "nfs3-calls.h"

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
	conn->timeout = 1000;
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
	conn->exportname = "";

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
	snprintf(info->ip, sizeof(info->ip), "%s", rpc_get_last_host());
	snprintf(info->host, sizeof(info->host), "%s", info->ip);
	info->mount1tcpport = 0;
	info->mount1udpport = 0;
	info->mount3tcpport = 0;
	info->mount3udpport = 0;
	info->nfs2tcpport = 0;
	info->nfs2udpport = 0;
	info->nfs3tcpport = 0;
	info->nfs3udpport = 0;
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
		case NFS_RPC_PROGRAM:
			if (list->map.vers == 2) {
				if (list->map.prot == IPPROTO_UDP) {
					info->nfs2udpport = list->map.port;
				} else if (list->map.prot == IPPROTO_TCP) {
					info->nfs2tcpport = list->map.port;
				}
			} else if (list->map.vers == 3) {
				if (list->map.prot == IPPROTO_UDP) {
					info->nfs3udpport = list->map.port;
				} else if (list->map.prot == IPPROTO_TCP) {
					info->nfs3tcpport = list->map.port;
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


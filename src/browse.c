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
/*#include "mount1-calls.h"*/
#include "mount3-calls.h"
#include "nfs2-calls.h"
/*#include "nfs3-calls.h"*/

#include "sunfish.h"
#include "browse.h"

int enablelog = 0;

static os_error *browse_initconn(struct conn_info *conn, char *host, int tcp)
{
	conn->config = NULL;
	conn->portmapper_port = PMAP_PORT;
	conn->mount_port = 0;
	conn->nfs_port = 0;
	conn->pcnfsd_port = 0;
	conn->tcp = tcp;
	conn->timeout = 30;
	conn->retries = 0;
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
	if ((conn->pool = pinit(NULL)) == NULL) {
		return gen_error(NOMEM,NOMEMMESS);
	}
	conn->txmutex = 0;
	conn->rxmutex = NULL;
	conn->reference = 0;
	conn->server = host;
	conn->export = "";

	return rpc_init_connection(conn);
}

os_error *browse_gethost(struct hostinfo *info, int type)
{
	pmaplist2 res;
	pmaplist list;
	os_error *err;
	static struct conn_info broadcastconn;

	if (type == 0) {
		err = browse_initconn(&broadcastconn, NULL, 0);
		if (err) return err;
	}

	if (type == 2) {
		err = rpc_close_connection(&broadcastconn);
		pfree(broadcastconn.pool);
		return err;
	}

	err = PMAPPROC_DUMP(&res, &broadcastconn, type == 0 ? TXNONBLOCKING : RXNONBLOCKING);
	if (err) return err;

	strcpy(info->host, rpc_get_last_host());
	info->mount1tcpport = 0;
	info->mount1udpport = 0;
	info->mount3tcpport = 0;
	info->mount3udpport = 0;

	list = res.list;
	while (list) {
		if (list->map.prog == MOUNT_RPC_PROGRAM) {
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
		}
		list = list->next;
	}
	return NULL;
}


os_error *browse_getexports(char *host, unsigned port, unsigned mount3, unsigned tcp)
{
	exportlist32 res;
	exportlist3 list;
	os_error *err;
	struct conn_info conn;

	err = browse_initconn(&conn, host, tcp);
	if (err) return err;
	conn.mount_port = port;
	conn.timeout = 2;
	conn.retries = 3;

	err = MOUNTPROC3_EXPORT(&res, &conn);
 if (err)  syslogf("Wibble",66,"browse_getexports error %s",err->errmess);
	if (err) {
		rpc_close_connection(&conn);
		pfree(conn.pool);
		return err;
	}


	list = res.list;
	while (list) {
		syslogf("Wibble",66,"export: %s",list->filesys.data);
		list = list->next;
	}

	err = rpc_close_connection(&conn);
	pfree(conn.pool);
	return err;
}

/*
	$Id: $

	Routines for ImageEntry_Func
*/


#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "imageentry_func.h"

#include "nfs-calls.h"
#include "mount-calls.h"
#include "portmapper-calls.h"


static os_error no_mem = {1, "Out of memory"};

static void free_conn_info(struct conn_info *conn)
{
	if (conn == NULL) return;
	if (conn->server) free(conn->server);
	if (conn->export) free(conn->export);
	free(conn);
}

os_error *func_newimage(unsigned int fileswitchhandle, 	struct conn_info **myhandle)
{
	struct conn_info *conn;

	conn = malloc(sizeof(struct conn_info));
	if (conn == NULL) return &no_mem;

	/* Set defaults */
	conn->portmapper_port = PMAP_PORT;
	conn->mount_port = 0;
	conn->nfs_port = 0;
	/*conn->auth = ;*/
	conn->server = NULL;
	conn->export = NULL;

	/* Read details from file */
	conn->server = "mint";
	conn->export = "/nfssandbox";

	/* Get port numbers if not specified */
	if (conn->mount_port == 0) {
		struct mapping map = {MOUNT_RPC_PROGRAM,MOUNT_RPC_VERSION,IPPROTO_UDP,0};
		int port;
		os_error *err;

		err = PMAPPROC_GETPORT(&map,&port,conn);
		if (err) {
			free_conn_info(conn);
			return err;
		}
		conn->mount_port = port;
	}

	if (conn->nfs_port == 0) {
		struct mapping map = {NFS_RPC_PROGRAM,NFS_RPC_VERSION,IPPROTO_UDP,0};
		int port;
		os_error *err;

		err = PMAPPROC_GETPORT(&map,&port,conn);
		if (err) {
			free_conn_info(conn);
			return err;
		}
		conn->nfs_port = port;
	}

	*myhandle = conn;

	return NULL;
}


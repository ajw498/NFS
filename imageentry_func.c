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

#include "readdir.h"

static os_error no_mem = {1, "Out of memory"};

static void free_conn_info(struct conn_info *conn)
{
	if (conn == NULL) return;
	if (conn->server) free(conn->server);
	if (conn->export) free(conn->export);
	free(conn);
}

os_error *func_closeimage(struct conn_info *conn)
{
	string dir;
	os_error *err;

	dir.size = strlen(conn->export);
	dir.data = conn->export;
	err = MNTPROC_UMNT(&dir, conn);
	free_conn_info(conn);
	return err; /* Should we supress errors from UMNT? */
}

os_error *func_newimage(unsigned int fileswitchhandle, struct conn_info **myhandle)
{
	struct conn_info *conn;
	string dir;
	struct fhstatus rootfh;
	os_error *err;

	conn = malloc(sizeof(struct conn_info));
	if (conn == NULL) return &no_mem;
	*myhandle = conn;

	/* Set defaults */
	conn->portmapper_port = PMAP_PORT;
	conn->mount_port = 0;
	conn->nfs_port = 0;
	/*conn->auth = ;*/
	conn->server = NULL;
	conn->export = NULL;

	/* Read details from file */
	fileswitchhandle = 0;
	conn->server = "mint";
	conn->export = "/nfssandbox";

	/* Get port numbers if not specified */
	if (conn->mount_port == 0) {
		struct mapping map = {MOUNT_RPC_PROGRAM,MOUNT_RPC_VERSION,IPPROTO_UDP,0};
		int port;

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

		err = PMAPPROC_GETPORT(&map,&port,conn);
		if (err) {
			free_conn_info(conn);
			return err;
		}
		conn->nfs_port = port;
	}

	/* Get a filehandle for the root directory */
	dir.size = strlen(conn->export);
	dir.data = conn->export;
	err = MNTPROC_MNT(&dir, &rootfh, conn);
	if (err) {
		free_conn_info(conn);
		return err;
	}
	if (rootfh.status != 0) {
		/* status is an errno value */
		static _kernel_oserror mnterr = {1,"mount failed"};
		free_conn_info(conn);
		return &mnterr;
	}
	memcpy(conn->rootfh, rootfh.u.directory, FHSIZE);

	return NULL;
}


struct dir_entry {
	unsigned int load;
	int exec;
	int len;
	int attr;
	int type;
	char name[1];
};

os_error *func_readdirinfo(int info, const char *dirname, void *buffer, int numobjs, int start, int buflen, struct conn_info *conn, int *objsread, int *continuepos)
{
	struct dir_entry *entry;
	static struct readdirargs rddir;
	static struct readdirres rdres;
	static struct readdirok_entry direntry;
	static struct readdirok eof;
	os_error *err;

	entry = buffer;
	*objsread = 0;

	memcpy(rddir.dir, conn->rootfh, FHSIZE);
	rddir.count = 1024;
	rddir.cookie = 0;
	err = NFSPROC_READDIR(&rddir, &rdres, conn);
	if (err) return err;
	do {
		err = NFSPROC_READDIR_entry(&direntry,conn);
		if (err) return err;
		if (direntry.opted) {
			if (direntry.u.u.name.size == 1 && direntry.u.u.name.data[0] == '.') {
				/* current dir */
			} else if (direntry.u.u.name.size == 2 && direntry.u.u.name.data[0] == '.' && direntry.u.u.name.data[1] == '.') {
				/* parent dir */
			} else {
				entry->load = 0xFFF10200;
				entry->exec = 0;
				entry->len = 1023;
				entry->attr = 3;
				entry->type = 1;
				memcpy(entry->name,direntry.u.u.name.data,direntry.u.u.name.size);
				entry->name[direntry.u.u.name.size] = '\0';
				entry = (struct dir_entry *)((int)((((char *)entry) + 5*4 + direntry.u.u.name.size + 4)) & ~3);
				/* Check buffer size */
				(*objsread)++;
			}
		}
	} while (direntry.opted);
	err = NFSPROC_READDIR_eof(&eof, conn);
	if (err) return err;
	*continuepos = -1;
	return NULL;
}

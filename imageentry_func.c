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
				entry->load = 0xFFFFFF00;
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

#define return_error(msg) do { strcpy(err_buf.errmess,msg); return &err_buf; } while (0)

static os_error *filename_to_finfo(char *filename, struct diropok **dinfo, struct diropok **finfo, char **leafname, struct conn_info *conn)
{
	char *start = filename;
	char *end;
	char *dirhandle = conn->rootfh;
	struct diropargs current;
	static struct diropres next;
	os_error *err;

/*return_error("foo");*/
	if (dinfo) *dinfo = NULL;
	if (finfo) *finfo = NULL;

	do {
		end = start;
		/* Find end of dirname */
		while (*end && *end != '.') end++;

		current.name.data = start;
		current.name.size = end - start;
		memcpy(current.dir, dirhandle, FHSIZE);
		err = NFSPROC_LOOKUP(&current, &next, conn);
		if (err) return err;

		if (leafname) *leafname = start;
		if (next.status != NFS_OK && next.status != NFSERR_NOENT) return_error("Lookup failed");
		if (*end != '\0') {
			if (next.u.diropok.attributes.type != NFDIR) return_error("not a directory");
			if (dinfo) *dinfo = &(next.u.diropok);
		}
		dirhandle = next.u.diropok.file;
		start = end + 1;
	} while (*end != '\0');
	if (finfo) *finfo = &(next.u.diropok);
	return NULL;
}

os_error *open_file(char *filename, int access, struct conn_info *conn, int *file_info_word, int *internal_handle, int *extent)
{
	struct file_handle *handle;
    struct diropok *dinfo;
    struct diropok *finfo;
    struct createargs createargs;
    struct diropres createres;
    os_error *err;
    char *leafname;

	handle = malloc(sizeof(struct file_handle));
	if (handle == NULL) return_error("out of mem");
	err = filename_to_finfo(filename, &dinfo, &finfo, &leafname, conn);
	if (err) {
		free(handle);
		return err;
	}
	if (finfo == NULL) {
/*		char tmp[20];
		sprintf(tmp,"access = %x",access);
		return_error(tmp);*/
		if (access ==1) {
			memcpy(createargs.where.dir, dinfo ? dinfo->file : conn->rootfh, FHSIZE);
			createargs.where.name.data = leafname;
			createargs.where.name.size = strlen(leafname);
			createargs.attributes.mode = 0x00008180; /**/
			createargs.attributes.uid = -1;
			createargs.attributes.gid = -1;
			createargs.attributes.size = access;
			createargs.attributes.atime.seconds = -1;
			createargs.attributes.atime.useconds = -1;
			createargs.attributes.mtime.seconds = -1;
			createargs.attributes.mtime.useconds = -1;
			err = NFSPROC_CREATE(&createargs, &createres, conn);
			if (err) {
				free(handle);
				return err;
			}
			if (createres.status != NFS_OK) return_error("create file failed");
			finfo = &(createres.u.diropok);
		} else {
			*internal_handle = 0;
			return NULL;
		}
	}
	handle->conn = conn;
	memcpy(handle->fhandle, finfo->file, FHSIZE);

	*internal_handle = (int)handle;
	*file_info_word = (int)0xC0000000;/*((finfo->attributes.mode & 0x400) << 20) | ((finfo->attributes.mode & 0x200) << 22);*/
	*extent = finfo->attributes.size;
	return NULL;
}

os_error *close_file(struct file_handle *handle, int load, int exec)
{
	/* FIXME update load/exec */
	free(handle);
	return NULL;
}

os_error *get_bytes(struct file_handle *handle, char *buffer, unsigned len, unsigned offset)
{
	os_error *err;
	struct readargs args;
	struct readres res;

/* FIXME */
	if (len > 7000/*rx_buffersize*/) return_error("file too big");
	memcpy(args.file, handle->fhandle, FHSIZE);
	args.offset = offset;
	args.count = len;
	err = NFSPROC_READ(&args, &res, handle->conn);
	if (err) return err;
	if (res.status != NFS_OK) return_error("read failed");
	/*if (res.u.data.size != len )return_error("unexpected amount of data read");*/
	memcpy(buffer, res.u.data.data, res.u.data.size);
	return NULL;
}

os_error *put_bytes(struct file_handle *handle, char *buffer, unsigned len, unsigned offset)
{
	os_error *err;
	struct writeargs args;
	struct attrstat res;

/* FIXME */
	if (len > 7000/*tx_buffersize*/) return_error("file too big");
	memcpy(args.file, handle->fhandle, FHSIZE);
	args.offset = offset;
	args.data.size = len;
	args.data.data = buffer;
	err = NFSPROC_WRITE(&args, &res, handle->conn);
	if (err) return err;
	if (res.status != NFS_OK) return_error("write failed");
	return NULL;
}

os_error *file_readcatinfo(char *filename, struct conn_info *conn, int *objtype, int *load, int *exec, int *len, int *attr)
{
    struct diropok *finfo;
    os_error *err;

/*	return_error("ofla avoidance");*/
	err = filename_to_finfo(filename, NULL, &finfo, NULL, conn);
	if (err) return err;
	if (finfo == NULL) {
		*objtype = 0;
		return NULL;
	}

	*objtype = finfo->attributes.type == NFDIR ? 2 : 1;
	*load = (int)0xFFFFFF00;
	*exec = 0;
	*len = finfo->attributes.size;
	*attr = 3;/*((finfo->attributes.mode & 0x400) >> 10) | ((finfo->attributes.mode & 0x200) >> 8);*/
	return NULL;
}

os_error *file_savefile(char *filename, int load, int exec, char *buffer, char *buffer_end, struct conn_info *conn, char **leafname)
{
    struct diropok *dinfo;
    os_error *err;
    struct createargs createargs;
    struct diropres createres;
	struct writeargs writeargs;
	struct attrstat writeres;

syslogf("NFS",25,"foo0");
	if (buffer_end - buffer > 7000/*tx_buffersize*/) return_error("file too big");
syslogf("NFS",25,"foo1");
	err = filename_to_finfo(filename, &dinfo, NULL, leafname, conn);
syslogf("NFS",25,"foo2");
	if (err) return err;
syslogf("NFS",25,"foo3");

	memcpy(createargs.where.dir, dinfo ? dinfo->file : conn->rootfh, FHSIZE);
	createargs.where.name.data = *leafname;
	createargs.where.name.size = strlen(*leafname);
	createargs.attributes.mode = 0x00008180; /**/
	createargs.attributes.uid = -1;
	createargs.attributes.gid = -1;
	createargs.attributes.size = buffer_end - buffer;
	createargs.attributes.atime.seconds = -1;
	createargs.attributes.atime.useconds = -1;
	createargs.attributes.mtime.seconds = -1;
	createargs.attributes.mtime.useconds = -1;
	err = NFSPROC_CREATE(&createargs, &createres, conn);
	if (err) return err;
	if (createres.status != NFS_OK && createres.status != NFSERR_EXIST) return_error("create file failed");
	memcpy(writeargs.file, createres.u.diropok.file, FHSIZE);
	writeargs.offset = 0;
	writeargs.data.size = buffer_end - buffer;
	writeargs.data.data = buffer;
	err = NFSPROC_WRITE(&writeargs, &writeres, conn);
	
	return NULL;
}

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
	int load;
	int exec;
	int len;
	int attr;
	int type;
/*	char name[1];*/
};

#define return_error(msg) do { strcpy(err_buf.errmess,msg); return &err_buf; } while (0)

#define OBJ_NONE 0
#define OBJ_FILE 1
#define OBJ_DIR  2

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
	if (finfo && next.status != NFSERR_NOENT) *finfo = &(next.u.diropok);
	return NULL;
}

static void timeval_to_loadexec(struct ntimeval *unixtime, int filetype, int *load, int *exec)
{
	int64_t csecs;
	csecs = unixtime->seconds;
	csecs *= 100;
	csecs += (unixtime->useconds / 10);
	csecs += 0x336e996a00; /* Difference between 1900 and 1970 */
	*load = (int)((csecs >> 32) & 0xFF);
	*load |= (0xFFF00000 | ((filetype & 0xFFF) << 8));
	*exec = (int)(csecs & 0xFFFFFFFF);
}

static int mode_to_attr(unsigned int mode)
{
	int attr;
	attr  = ( mode & 0x100) >> 8; /* Owner read */
	attr |= ( mode & 0x080) >> 6; /* Owner write */
	attr |= (~mode & 0x080) >> 4; /* Locked */
	attr |= ( mode & 0x004) << 2; /* Others read */
	attr |= ( mode & 0x002) << 4; /* Others write */
	return attr;
}
os_error *func_readdirinfo(int info, char *dirname, void *buffer, int numobjs, int start, int buflen, struct conn_info *conn, int *objsread, int *continuepos)
{
	char *bufferpos;
	static struct readdirargs rddir;
	static struct readdirres rdres;
	struct entry *direntry;
	os_error *err;

	bufferpos = buffer;
	*objsread = 0;

	if (dirname[0] == '\0') {
		memcpy(rddir.dir, conn->rootfh, FHSIZE);
	} else {
		struct diropok *finfo;
		err = filename_to_finfo(dirname, NULL, &finfo, NULL, conn);
		if (err) return err;
		if (finfo == NULL) return_error("dir doesn't exist");
		memcpy(rddir.dir, finfo->file, FHSIZE);
	}
	rddir.count = 1024;
	rddir.cookie = start;
	err = NFSPROC_READDIR(&rddir, &rdres, conn);
	if (err) return err;
	if (rdres.status != NFS_OK) return_error("readdir failed");
	swap_rxbuffers();
	direntry = rdres.u.readdirok.entries;
	while (direntry) {
		if (direntry->name.size == 1 && direntry->name.data[0] == '.') {
			/* current dir */
		} else if (direntry->name.size == 2 && direntry->name.data[0] == '.' && direntry->name.data[1] == '.') {
			/* parent dir */
		} else {
			if (info) {
				struct diropargs lookupargs;
				struct diropres lookupres;
				struct dir_entry *entry;

				memcpy(lookupargs.dir, rddir.dir, FHSIZE);
				lookupargs.name.data = direntry->name.data;
				lookupargs.name.size = direntry->name.size;
				err = NFSPROC_LOOKUP(&lookupargs, &lookupres, conn);
				if (err) return err;
				if (lookupres.status != NFS_OK) return_error("lookup failed");

				entry = (struct dir_entry *)bufferpos;
				bufferpos += sizeof(struct dir_entry);
				timeval_to_loadexec(&(lookupres.u.diropok.attributes.mtime), 0xFFF, &(entry->load), &(entry->exec));
				entry->len = lookupres.u.diropok.attributes.size;
				entry->attr = mode_to_attr(lookupres.u.diropok.attributes.mode);
				entry->type = lookupres.u.diropok.attributes.type == NFDIR ? OBJ_DIR : OBJ_FILE;
			}
			memcpy(bufferpos,direntry->name.data,direntry->name.size);
			bufferpos[direntry->name.size] = '\0';
			bufferpos += (direntry->name.size + 4) & ~3;
			/* Check buffer size */
			(*objsread)++;
		}
		start = direntry->cookie;
		direntry = direntry->next;
	}
	if (rdres.u.readdirok.eof) {
		*continuepos = -1;
	} else {
		*continuepos = start;
	}
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

os_error *file_writecatinfo(char *filename, int load, int exec, int attr, struct conn_info *conn)
{
	/**/
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

	if (buffer_end - buffer > 7000/*tx_buffersize*/) return_error("file too big");
	err = filename_to_finfo(filename, &dinfo, NULL, leafname, conn);
	if (err) return err;

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

os_error *file_delete(char *filename, struct conn_info *conn, int *objtype, int *load, int *exec, int *len, int *attr)
{
    struct diropok *dinfo;
    struct diropok *finfo;
    os_error *err;
    char *leafname;

	err = filename_to_finfo(filename, &dinfo, &finfo, &leafname, conn);
	if (err) return err;

	if (finfo == NULL) {
		*objtype = 0;
	} else {
	    struct diropargs removeargs;
	    enum nstat removeres;

		memcpy(removeargs.dir, dinfo ? dinfo->file : conn->rootfh, FHSIZE);
		removeargs.name.data = leafname;
		removeargs.name.size = strlen(leafname);
		if (finfo->attributes.type == NFDIR) {
			err = NFSPROC_RMDIR(&removeargs, &removeres, conn);
		} else {
			err = NFSPROC_REMOVE(&removeargs, &removeres, conn);
		}
		if (err) return err;
		if (removeres != NFS_OK) return_error("remove failed");
		*objtype = finfo->attributes.type == NFDIR ? 2 : 1;
		*load = (int)0xFFFFFF00;
		*exec = 0;
		*len = finfo->attributes.size;
		*attr = 3;/*((finfo->attributes.mode & 0x400) >> 10) | ((finfo->attributes.mode & 0x200) >> 8);*/
	}
	return NULL;
}

os_error *func_rename(char *oldfilename, char *newfilename, struct conn_info *conn, int *renamefailed)
{
    struct diropok *dinfo;
    struct diropok *finfo;
    struct renameargs renameargs;
    enum nstat renameres;
    os_error *err;
    char *leafname;

	err = filename_to_finfo(oldfilename, &dinfo, &finfo, &leafname, conn);
	if (err) return err;
	if (finfo == NULL) return_error("file not found");

	memcpy(renameargs.from.dir, dinfo ? dinfo->file : conn->rootfh, FHSIZE);
	renameargs.from.name.data = leafname;
	renameargs.from.name.size = strlen(leafname);

	if (strncmp(oldfilename, newfilename, leafname - oldfilename) == 0) {
		/* Both files are in the same directory */
		memcpy(renameargs.to.dir, renameargs.from.dir, FHSIZE);
		renameargs.to.name.data = newfilename + (leafname - oldfilename);
		renameargs.to.name.size = strlen(renameargs.to.name.data);
	} else {
		err = filename_to_finfo(newfilename, &dinfo, NULL, &leafname, conn);
		if (err) return err;
		memcpy(renameargs.to.dir, dinfo ? dinfo->file : conn->rootfh, FHSIZE);
		renameargs.to.name.data = leafname;
		renameargs.to.name.size = strlen(leafname);
	}

	err = NFSPROC_RENAME(&renameargs, &renameres, conn);
	if (err) return err;
	if (renameres != NFS_OK) return_error("rename failed");
	*renamefailed = 0;
	return NULL;
}

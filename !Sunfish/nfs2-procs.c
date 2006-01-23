/*
	$Id$
	$URL$

	NFSv2 procedures


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
#include <sys/types.h>
#include <swis.h>
#include <kernel.h>
#include <ctype.h>



#include "nfs2-procs.h"

typedef _kernel_oserror os_error;

#define NE(x) do { \
	res->status = x; \
	if (res->status != NFS_OK) return SUCCESS; \
} while (0)

#define UE(x) do { \
	if ((x) == NULL) { \
		res->status = NFSERR_IO; \
		return SUCCESS; \
	} else { \
		res->status = NFS_OK; \
	} \
} while (0)


#define OE(x) do { \
	os_error *err = x; \
	if (err) { \
		res->status = oserr_to_nfserr(err->errnum); \
		printf("ERROR: %x %s\n", err->errnum, err->errmess); \
		return SUCCESS; \
	} else { \
		res->status = NFS_OK; \
	} \
} while (0)

#define NR(x) do { \
	enum nstat status = x; \
	if (status != NFS_OK) return status; \
} while (0)

#define UR(x) do { \
	if ((x) == NULL) return NFSERR_IO; \
} while (0)

#define OR(x) do { \
	os_error *err = x; \
	if (err) { \
		printf("ERROR: %x %s\n", err->errnum, err->errmess); \
		return oserr_to_nfserr(err->errnum); \
	} \
} while (0)

static enum nstat oserr_to_nfserr(int errnum)
{
	switch (errnum) {
	case 0x117b4: return NFSERR_NOTEMPTY;
	case 0x117c3: return NFSERR_ACCES;
	case 0x117c6: return NFSERR_NOSPC;
	case 0x80344a: return NFSERR_ROFS;
	case 0xb0: return (enum nstat)18; /*NFS3ERR_XDEV */
	}
	return NFSERR_IO;
}

static enum nstat nfs2fh_to_path(struct nfs_fh *fhandle, char **path, struct server_conn *conn)
{
	int extendedfh;
	int exportnum;
	int hash;

	extendedfh = ((((unsigned char)(fhandle->data[0])) & 0x10) != 0);
	exportnum = fhandle->data[0] & 0x7F;
	hash = fhandle->data[1];

	if (conn->export == NULL) {
		conn->export = conn->exports;
		while ((conn->export->exportnum != exportnum) && conn->export) {
			conn->export = conn->export->next;
		}
		if (conn->export == NULL) return NFSERR_STALE;
	}
	if (conn->export->basedirhash != hash) return NFSERR_STALE;

	if (extendedfh) {
		/*FIXME*/
	} else {
		char *dest;
		int i;

		UR(*path = palloc(conn->export->basedirlen + FHSIZE, conn->pool));
		dest = *path;
		memcpy(dest, conn->export->basedir, conn->export->basedirlen);
		dest += conn->export->basedirlen;
		if (fhandle->data[2]) *dest++ = '.';
		for (i = 2; i < FHSIZE; i++) {
			char ch = fhandle->data[i];

			if (ch == '\0') break;
			if (!(isalnum(ch) || ch == '.')) return NFSERR_STALE; /*FIXME - allow more */
			*dest++ = ch;
		}
		*dest = '\0';
	}

	return NFS_OK;
}

static enum nstat path_to_nfs2fh(char *path, struct nfs_fh *fhandle, struct server_conn *conn)
{
	size_t len;
	memset(fhandle->data, 0, FHSIZE);
	len = strlen(path);
	if ((len < conn->export->basedirlen) ||
	    (memcmp(path, conn->export->basedir, conn->export->basedirlen) != 0) ||
	    ((path[conn->export->basedirlen] != '.') &&
	     (path[conn->export->basedirlen] != '\0'))) {
		abort();/*FIXME*/
	}
	len -= conn->export->basedirlen + 1;
	if (len > FHSIZE - 2) {
		/*FIXME extended FH*/
		abort();
	} else {
		fhandle->data[0] = conn->export->exportnum;
		fhandle->data[1] = conn->export->basedirhash;
		memcpy(fhandle->data + 2, path + conn->export->basedirlen + 1, len);
		/* Terminated by the earlier memset */
	}

	return NFS_OK;
}

int calc_fileid(char *path, char *leaf)
{
	int fileid = 0;
	char *dot = leaf ? "." : NULL;
	do {
		while (path && *path) {
			int hash;
			fileid = (fileid << 4) + *path++;
			hash = fileid & 0xf0000000;
			if (hash) {
				fileid = fileid ^ (hash >> 24);
				fileid = fileid ^ hash;
			}
		}
		path = dot;
		dot = leaf;
		leaf = NULL;
	} while (path);
	return fileid;
}

/* Convert a RISC OS load and execution address into a unix timestamp */
static void loadexec_to_timeval(unsigned int load, unsigned int exec, struct ntimeval *unixtime)
{
	if ((load & 0xFFF00000) != 0xFFF00000) {
		/* A real load/exec address */
		unixtime->seconds = -1;
		unixtime->useconds = -1;
	} else {
		uint64_t csecs;

		csecs = exec;
		csecs |= ((uint64_t)load & 0xFF) << 32;
		csecs -= 0x336e996a00LL; /* Difference between 1900 and 1970 */
		unixtime->seconds = (unsigned int)((csecs / 100) & 0xFFFFFFFF);
		unixtime->useconds = (unsigned int)((csecs % 100) * 10000);
	}
}

static enum nstat get_fattr(char *path, struct fattr *fattr, struct server_conn *conn)
{
	int type, load, exec, len, attr;
	os_error *err;
	printf("getfattr %s\n",path);
	err = _swix(OS_File, _INR(0,1) | _OUT(0) | _OUTR(2,5), 17, path, &type, &load, &exec, &len, &attr);
	if (err) {
		/*log*/
		printf("error %s\n",err->errmess);
		return NFSERR_IO;
	} else if (type == 0) {
		return NFSERR_NOENT;
	} else {
		fattr->type = type == 3 ? (/*conn->image_as_file ? NFREG : */NFDIR) :
		              type == 2 ? NFDIR : NFREG;
		fattr->mode = fattr->type == NFDIR ? 040000 : 0100000;
		fattr->mode |= (attr & 0x01) << 8; /* Owner read */
		fattr->mode |= (attr & 0x02) << 6; /* Owner write */
		fattr->mode |= (attr & 0x10) << 1; /* Group read */
		fattr->mode |= (attr & 0x20) >> 1; /* Group write */
		fattr->mode |= (attr & 0x10) >> 2; /* Others read */
		fattr->mode |= (attr & 0x20) >> 4; /* Others write */
		fattr->nlink = 1;
		fattr->uid = 0;/*FIXME*/
		fattr->gid = 0;
		fattr->size = len;
		fattr->blocksize = 4096;
		fattr->rdev = 0;
		fattr->blocks = fattr->size % fattr->blocksize; /* Is this right? */
		fattr->fsid = 1;
		fattr->fileid = calc_fileid(path, NULL); /*FIXME?*/
		loadexec_to_timeval(load, exec, &(fattr->atime));
		loadexec_to_timeval(load, exec, &(fattr->ctime));
		loadexec_to_timeval(load, exec, &(fattr->mtime));
	}
	return NFS_OK;
}

/* Convert a unix mode to RISC OS attributes */
static unsigned int mode_to_attr(unsigned int mode)
{
	unsigned int attr;
	attr  = (mode & 0x100) >> 8; /* Owner read */
	attr |= (mode & 0x080) >> 6; /* Owner write */
	/* Set public read/write if either group or other bits are set */
	attr |= ((mode & 0x004) << 2) | ((mode & 0x020) >> 1); /* Public read */
	attr |= ((mode & 0x002) << 4) | ((mode & 0x010) << 1); /* Public write */
	return attr;
}

/* Convert a unix timestamp into a RISC OS load and execution address */
static void timeval_to_loadexec(struct ntimeval *unixtime, int filetype, unsigned int *load, unsigned int *exec)
{
	uint64_t csecs;

	csecs = unixtime->seconds;
	csecs *= 100;
	csecs += ((int64_t)unixtime->useconds / 10000);
	csecs += 0x336e996a00LL; /* Difference between 1900 and 1970 */
	*load = (unsigned int)((csecs >> 32) & 0xFF);
	*load |= (0xFFF00000 | ((filetype & 0xFFF) << 8));
	*exec = (unsigned int)(csecs & 0xFFFFFFFF);
}


static enum nstat set_attr(char *path, struct sattr *sattr, struct server_conn *conn)
{
	unsigned int load;
	unsigned int exec;
	int filetype;
	int type;
	int size;
	int attr;

	OR(_swix(OS_File, _INR(0,1) | _OUT(0) | _OUTR(2,5), 17, path, &type, &load, &exec, &size, &attr));
	if (type == 0) return NFSERR_NOENT;
	if (sattr->mode != -1) attr = mode_to_attr(sattr->mode);
	filetype = load >> 20;
	if (sattr->mtime.seconds != -1) timeval_to_loadexec(&(sattr->mtime), filetype, &load, &exec);

	OR(_swix(OS_File, _INR(0,3) | _IN(5), 1, path, load, exec, attr));

	if ((type & 1) && sattr->size != -1 && sattr->size != size) {
		/*FIXME - image files*/
		/*FIXME - open file if not already open, and set extent */
	}
	return NFS_OK;
}


static enum nstat diropargs_to_path(struct diropargs *where, char **path, int *filetype, struct server_conn *conn)
{
	int len;
	char *dirpath;

	NR(nfs2fh_to_path(&(where->dir), &dirpath, conn));

	len = strlen(dirpath);
	UR(*path = palloc(len + where->name.size + 2, conn->pool));

	memcpy(*path, dirpath, len);
	(*path)[len++] = '.';
	memcpy(*path + len, where->name.data, where->name.size);
	len += where->name.size;
	(*path)[len] = '\0';
	/*FIXME - handle ,xyz */
	if (filetype) *filetype = 0xfff;

	return NFS_OK;
}

enum accept_stat NFSPROC_NULL(struct server_conn *conn)
{
	printf("NFSPROC_NULL\n");

	(void)conn;

	return SUCCESS;
}

enum accept_stat NFSPROC_LOOKUP(struct diropargs *args, struct diropres *res, struct server_conn *conn)
{
	char *path;
	char *filename;
	size_t len;

	printf("NFSPROC_LOOKUP\n");
	NE(nfs2fh_to_path(&(args->dir), &path, conn));
	len = strlen(path) + args->name.size + 2;
	UE(filename = palloc(len, conn->pool));
	strcpy(filename, path);
	strcat(filename, ".");
	memcpy(filename + len - args->name.size - 1, args->name.data, args->name.size);
	filename[len - 1] = '\0';
	NE(path_to_nfs2fh(filename, &(res->u.diropok.file), conn));
	NE(get_fattr(filename, &(res->u.diropok.attributes), conn));
	return SUCCESS;
}

enum accept_stat NFSPROC_READDIR(struct readdirargs *args, struct readdirres *res, struct server_conn *conn)
{
	char *path;
	printf("NFSPROC_READDIR\n");
	/* cookie is an index into the directory. Have to do an osgbpb loop every time to find the index. But we can cache the last used one. */
	res->status = nfs2fh_to_path(&(args->dir), &path, conn);
	if (res->status == NFS_OK) {
		char buffer[1024];
		int read;
		int cookie = args->cookie;

		res->u.readdirok.entries = NULL;

		while (cookie != -1) {
			OE(_swix(OS_GBPB, _INR(0,6) | _OUTR(3,4),9, path, buffer, 1, cookie, sizeof(buffer), 0, &read, &cookie));
			if (read > 0) {
				struct entry *entry;
				UE(entry = malloc(sizeof(struct entry)));

				entry->fileid = calc_fileid(path, buffer);
				entry->name.size = strlen(buffer);
				entry->name.data = malloc(entry->name.size);/**/
				memcpy(entry->name.data, buffer, entry->name.size);
				entry->cookie = cookie;/**/
				entry->next = res->u.readdirok.entries;
				res->u.readdirok.entries = entry;
			}
		}
		res->u.readdirok.eof = TRUE;
	}
	return SUCCESS;
}

enum accept_stat NFSPROC_READ(struct readargs *args, struct readres *res, struct server_conn *conn)
{
	char *path;
	int handle;
	unsigned int read;
	char *data;

	printf("NFSPROC_READ\n");

	NE(nfs2fh_to_path(&(args->file), &path, conn));
	OE(_swix(OS_Find, _INR(0,1) | _OUT(0), 0x43, path, &handle));
	/*FIXME verify count is sensible (and offset?) */
	UE(data = palloc(args->count, conn->pool));
	/*FIXME close handle on error (done by handle timeout?)*/
	OE(_swix(OS_GBPB, _INR(0,4) | _OUT(3), 3, handle, data, args->count, args->offset, &read));
	res->u.resok.data.data = data;
	res->u.resok.data.size = args->count - read;
	OE(_swix(OS_Find, _INR(0,1), 0, handle));
	NE(get_fattr(path, &(res->u.resok.attributes), conn));
	return SUCCESS;
}

enum accept_stat NFSPROC_WRITE(struct writeargs *args, struct attrstat *res, struct server_conn *conn)
{
	char *path;
	int handle;

	printf("NFSPROC_WRITE\n");

	NE(nfs2fh_to_path(&(args->file), &path, conn));
	if (conn->export->ro) NE(NFSERR_ROFS);
	OE(_swix(OS_Find, _INR(0,1) | _OUT(0), 0xC3, path, &handle));
	/*FIXME close handle on error (done by handle timeout?)*/
	OE(_swix(OS_GBPB, _INR(0,4), 1, handle, args->data.data, args->data.size, args->offset));
	OE(_swix(OS_Find, _INR(0,1), 0, handle));
	NE(get_fattr(path, &(res->u.attributes), conn));
	return SUCCESS;
}

enum accept_stat NFSPROC_CREATE(struct createargs *args, struct createres *res, struct server_conn *conn)
{
	char *path;
	int filetype;
	int size;

	printf("NFSPROC_CREATE\n");

	NE(diropargs_to_path(&(args->where), &path, &filetype, conn));
	if (conn->export->ro) NE(NFSERR_ROFS);
	size = args->attributes.size == -1 ? 0 : args->attributes.size;
	if (size < 0) NE(NFSERR_FBIG);
	OE(_swix(OS_File, _INR(0,5), 11, path, filetype, 0, 0, size));
	NE(set_attr(path, &(args->attributes), conn));
	NE(path_to_nfs2fh(path, &(res->u.diropok.file), conn));
	NE(get_fattr(path, &(res->u.diropok.attributes), conn));

	return SUCCESS;
}

enum accept_stat NFSPROC_RMDIR(struct diropargs *args, struct removeres *res, struct server_conn *conn)
{
	char *path;

	printf("NFSPROC_RMDIR\n");

	NE(diropargs_to_path(args, &path, NULL, conn));
	if (conn->export->ro) NE(NFSERR_ROFS);
	OE(_swix(OS_File, _INR(0,1), 6, path));

	return SUCCESS;
}

enum accept_stat NFSPROC_REMOVE(struct diropargs *args, struct removeres *res, struct server_conn *conn)
{
	char *path;

	printf("NFSPROC_REMOVE\n");

	NE(diropargs_to_path(args, &path, NULL, conn));
	if (conn->export->ro) NE(NFSERR_ROFS);
	OE(_swix(OS_File, _INR(0,1), 6, path));

	return SUCCESS;
}

enum accept_stat NFSPROC_RENAME(struct renameargs *args, struct renameres *res, struct server_conn *conn)
{
	char *from;
	char *to;

	printf("NFSPROC_RENAME\n");

	NE(diropargs_to_path(&(args->from), &from, NULL, conn));
	if (conn->export->ro) NE(NFSERR_ROFS);
	NE(diropargs_to_path(&(args->to), &to, NULL, conn));
	OE(_swix(OS_FSControl, _INR(0,2), 25, from, to));
	/* FIXME set filetype if changed */
	return SUCCESS;
}


enum accept_stat NFSPROC_GETATTR(struct getattrargs *args, struct attrstat *res, struct server_conn *conn)
{
	char *path;

	printf("NFSPROC_GETATTR\n");

	NE(nfs2fh_to_path(&(args->fhandle), &path, conn));
	NE(get_fattr(path, &(res->u.attributes), conn));

	return SUCCESS;
}

enum accept_stat NFSPROC_MKDIR(struct mkdirargs *args, struct createres *res, struct server_conn *conn)
{
	char *path;

	printf("NFSPROC_MKDIR\n");

	NE(diropargs_to_path(&(args->where), &path, NULL, conn));
	if (conn->export->ro) NE(NFSERR_ROFS);
	OE(_swix(OS_File, _INR(0,1) | _IN(4), 8, path, 0));
	NE(set_attr(path, &(args->attributes), conn));
	NE(path_to_nfs2fh(path, &(res->u.diropok.file), conn));
	NE(get_fattr(path, &(res->u.diropok.attributes), conn));

	return SUCCESS;
}

enum accept_stat NFSPROC_SETATTR(struct sattrargs *args, struct sattrres *res, struct server_conn *conn)
{
	char *path;

	printf("NFSPROC_SETATTR\n");

	NE(nfs2fh_to_path(&(args->file), &path, conn));
	if (conn->export->ro) NE(NFSERR_ROFS);
	NE(set_attr(path, &(args->attributes), conn));
	NE(get_fattr(path, &(res->u.attributes), conn));

	return SUCCESS;
}

enum accept_stat NFSPROC_STATFS(struct statfsargs *args, struct statfsres *res, struct server_conn *conn)
{
	char *path;
	unsigned int freelo;
	unsigned int freehi;
	unsigned int sizelo;
	unsigned int sizehi;

	printf("NFSPROC_STATFS\n");

	/* Assume the block size is 4k for simplicity */
	res->u.info.bsize = 4096;

	NE(nfs2fh_to_path(&(args->fhandle), &path, conn));
	if (_swix(OS_FSControl, _INR(0,1) | _OUTR(0,1) | _OUTR(3,4), 55, path, &freelo, &freehi, &sizelo, &sizehi)) {
		OE(_swix(OS_FSControl, _INR(0,1) | _OUT(0) | _OUT(2), 49, path, &freelo, &sizelo));
		freehi = 0;
		sizehi = 0;
	}

	if (freehi & 0xFFFFF000) {
		res->u.info.bfree = 0xFFFFFFFF;
	} else {
		res->u.info.bfree = (freehi << 20) | (freelo >> 12);
	}
	res->u.info.bavail = res->u.info.bfree;

	if (sizehi & 0xFFFFF000) {
		res->u.info.blocks = 0xFFFFFFFF;
	} else {
		res->u.info.blocks = (sizehi << 20) | (sizelo >> 12);
	}

	res->u.info.tsize = conn->transfersize;

	return SUCCESS;
}

enum accept_stat NFSPROC_READLINK(struct readlinkargs *args, struct readlinkres *res, struct server_conn *conn)
{
	printf("NFSPROC_READLINK\n");

	(void)args;
	(void)res;
	(void)conn;

	return PROC_UNAVAIL;
}


enum accept_stat NFSPROC_LINK(struct linkargs *args, enum nstat *res, struct server_conn *conn)
{
	printf("NFSPROC_LINK\n");

	(void)args;
	(void)res;
	(void)conn;

	return PROC_UNAVAIL;
}

enum accept_stat NFSPROC_SYMLINK(struct symlinkargs *args, enum nstat *res, struct server_conn *conn)
{
	printf("NFSPROC_SYMLINK\n");

	(void)args;
	(void)res;
	(void)conn;

	return PROC_UNAVAIL;
}

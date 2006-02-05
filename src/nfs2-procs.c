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


#include "utils.h"


#include "nfs2-procs.h"
#include "filecache.h"


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
		syslogf(LOGNAME, LOG_ERROR, "Error: %x %s", err->errnum, err->errmess); \
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
		syslogf(LOGNAME, LOG_ERROR, "Error: %x %s", err->errnum, err->errmess); \
		return oserr_to_nfserr(err->errnum); \
	} \
} while (0)



static inline enum nstat nfs2fh_to_path(struct nfs_fh *fhandle, char **path, struct server_conn *conn)
{
	return fh_to_path(fhandle->data, FHSIZE, path, conn);
}

static inline enum nstat path_to_nfs2fh(char *path, struct nfs_fh *fhandle, struct server_conn *conn)
{
	unsigned int fhsize = FHSIZE;
	char *fh = fhandle->data;

	return path_to_fh(path, &fh, &fhsize, conn);
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

static enum nstat get_fattr(char *path, int filetype, struct fattr *fattr, struct server_conn *conn)
{
	int type, load, exec, len, attr;

	OR(_swix(OS_File, _INR(0,1) | _OUT(0) | _OUTR(2,5), 17, path, &type, &load, &exec, &len, &attr));

	if (type == 0) {
		return NFSERR_NOENT;
	} else {
		int ftype = (load & 0x000FFF00) >> 8;
		fattr->type = type == 3 ? (conn->export->imagefs ? NFDIR : NFREG) :
		              type == 2 ? NFDIR : NFREG;
		if ((fattr->type == NFREG) && (filetype != -1) && (filetype != ftype)) return NFSERR_NOENT;
		fattr->mode = fattr->type == NFDIR ? 040000 : 0100000;
		fattr->mode |= (attr & 0x01) << 8; /* Owner read */
		fattr->mode |= (attr & 0x02) << 6; /* Owner write */
		fattr->mode |= (attr & 0x10) << 1; /* Group read */
		fattr->mode |= (attr & 0x20) >> 1; /* Group write */
		fattr->mode |= (attr & 0x10) >> 2; /* Others read */
		fattr->mode |= (attr & 0x20) >> 4; /* Others write */
		fattr->nlink = 1;
		fattr->uid = conn->uid;
		fattr->gid = conn->gid;
		fattr->size = len;
		fattr->blocksize = 4096;
		fattr->blocks = fattr->size >> 12;
		fattr->rdev = 0;
		fattr->fsid = conn->export->exportnum;
		fattr->fileid = calc_fileid(path, NULL);
		loadexec_to_timeval(load, exec, &(fattr->atime));
		loadexec_to_timeval(load, exec, &(fattr->ctime));
		loadexec_to_timeval(load, exec, &(fattr->mtime));
	}
	return NFS_OK;
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
	filetype = (load & 0x000FFF00) >> 8;
	if (sattr->mtime.seconds != -1) timeval_to_loadexec(&(sattr->mtime), filetype, &load, &exec);

	OR(_swix(OS_File, _INR(0,3) | _IN(5), 1, path, load, exec, attr));

	if (((type == 1) || (type == 3 && conn->export->imagefs == 0)) && sattr->size != -1 && sattr->size != size) {
		/*FIXME - open file if not already open, and set extent */
	}
	return NFS_OK;
}





static enum nstat diropargs_to_path(struct diropargs *where, char **path, int *filetype, struct server_conn *conn)
{
	int len;
	char *dirpath;
	char buffer[MAX_PATHNAME];
	int leaflen;

	NR(nfs2fh_to_path(&(where->dir), &dirpath, conn));

	leaflen = filename_riscosify(where->name.data, where->name.size, buffer, sizeof(buffer), filetype, conn->export->defaultfiletype, conn->export->xyzext);

	len = strlen(dirpath);
	UR(*path = palloc(len + leaflen + 2, conn->pool));

	memcpy(*path, dirpath, len);
	(*path)[len++] = '.';
	memcpy(*path + len, buffer, leaflen);
	len += leaflen;
	(*path)[len] = '\0';

	return NFS_OK;
}

enum accept_stat NFSPROC_NULL(struct server_conn *conn)
{
	(void)conn;

	return SUCCESS;
}

enum accept_stat NFSPROC_LOOKUP(struct diropargs *args, struct diropres *res, struct server_conn *conn)
{
	char *path;
	int filetype;

	NE(diropargs_to_path(args, &path, &filetype, conn));
	NE(path_to_nfs2fh(path, &(res->u.diropok.file), conn));
	NE(get_fattr(path, filetype, &(res->u.diropok.attributes), conn));

	return SUCCESS;
}

enum accept_stat NFSPROC_READDIR(struct readdirargs *args, struct readdirres *res, struct server_conn *conn)
{
	char *path;
	int cookie = args->cookie;
	int bytes = 0;

	NE(nfs2fh_to_path(&(args->dir), &path, conn));

	res->u.readdirok.entries = NULL;
	res->u.readdirok.eof = FALSE;

	while (cookie != -1) {
		int read;
		char buffer[1024];

		/* We have to read one entry at a time, which is less
		   efficient than reading several, as we need to return
		   a cookie for every entry. */
		OE(_swix(OS_GBPB, _INR(0,6) | _OUTR(3,4), 10, path, buffer, 1, cookie, sizeof(buffer), 0, &read, &cookie));
		if (read > 0) {
			struct entry *entry;
			char *leaf = buffer + 20;
			unsigned int leaflen;
			int filetype;
			unsigned int load = ((unsigned int *)buffer)[0];
			int type;

			if ((load & 0xFFF00000) == 0xFFF00000) {
				filetype = (load & 0x000FFF00) >> 8;
			} else {
				filetype = conn->export->defaultfiletype;
			}
			UE(entry = palloc(sizeof(struct entry), conn->pool));

			entry->fileid = calc_fileid(path, leaf);
			UE(leaf = filename_unixify(leaf, strlen(leaf), &leaflen, conn->pool));
			type = ((unsigned int *)buffer)[4];
			if (type == 1 || (type == 3 && conn->export->imagefs == 0)) {
				UE(leaf = addfiletypeext(leaf, leaflen, 0, filetype, &leaflen, conn->export->defaultfiletype, conn->export->xyzext, conn->pool));
			}
			entry->name.data = leaf;
			entry->name.size = leaflen;
			entry->cookie = cookie;

			bytes += 16 + (leaflen + 3) & ~3;
			if (bytes > args->count) {
				return SUCCESS;
			}
			entry->next = res->u.readdirok.entries;
			res->u.readdirok.entries = entry;
		}
		if (cookie == -1) res->u.readdirok.eof = TRUE;
	}

	return SUCCESS;
}

enum accept_stat NFSPROC_READ(struct readargs *args, struct readres *res, struct server_conn *conn)
{
	char *path;
	int handle;
	unsigned int read;
	char *data;

	NE(nfs2fh_to_path(&(args->file), &path, conn));
	/*OE(open_file(path, &handle));*/
	/*FIXME verify count is sensible (and offset?) */
/*	UE(data = palloc(args->count, conn->pool));
	OE(_swix(OS_GBPB, _INR(0,4) | _OUT(3), 3, handle, data, args->count, args->offset, &read));
	res->u.resok.data.data = data;
	res->u.resok.data.size = args->count - read;*/
	NE(read_file(path, args->count, args->offset, &data, &read, NULL));
	res->u.resok.data.data = data;
	res->u.resok.data.size = read;
	NE(get_fattr(path, -1, &(res->u.resok.attributes), conn));
	return SUCCESS;
}

enum accept_stat NFSPROC_WRITE(struct writeargs *args, struct attrstat *res, struct server_conn *conn)
{
	char *path;
	int handle;

	NE(nfs2fh_to_path(&(args->file), &path, conn));
	if (conn->export->ro) NE(NFSERR_ROFS);
	OE(open_file(path, &handle, NULL));
	OE(_swix(OS_GBPB, _INR(0,4), 1, handle, args->data.data, args->data.size, args->offset));
	NE(get_fattr(path, -1, &(res->u.attributes), conn));

	return SUCCESS;
}

enum accept_stat NFSPROC_CREATE(struct createargs *args, struct createres *res, struct server_conn *conn)
{
	char *path;
	int filetype;
	int size;

	NE(diropargs_to_path(&(args->where), &path, &filetype, conn));
	if (conn->export->ro) NE(NFSERR_ROFS);
	size = args->attributes.size == -1 ? 0 : args->attributes.size;
	if (size < 0) NE(NFSERR_FBIG);
	/*FIXME - is file 0 extended?*/
	OE(_swix(OS_File, _INR(0,5), 11, path, filetype, 0, 0, size));
	NE(set_attr(path, &(args->attributes), conn));
	NE(path_to_nfs2fh(path, &(res->u.diropok.file), conn));
	NE(get_fattr(path, -1, &(res->u.diropok.attributes), conn));

	return SUCCESS;
}

enum accept_stat NFSPROC_RMDIR(struct diropargs *args, struct removeres *res, struct server_conn *conn)
{
	char *path;

	NE(diropargs_to_path(args, &path, NULL, conn));
	if (conn->export->ro) NE(NFSERR_ROFS);
	OE(_swix(OS_File, _INR(0,1), 6, path));

	return SUCCESS;
}

enum accept_stat NFSPROC_REMOVE(struct diropargs *args, struct removeres *res, struct server_conn *conn)
{
	char *path;

	NE(diropargs_to_path(args, &path, NULL, conn));
	if (conn->export->ro) NE(NFSERR_ROFS);
	OE(_swix(OS_File, _INR(0,1), 6, path));

	return SUCCESS;
}

enum accept_stat NFSPROC_RENAME(struct renameargs *args, struct renameres *res, struct server_conn *conn)
{
	char *from;
	char *to;
	int oldfiletype;
	int newfiletype;

	NE(diropargs_to_path(&(args->from), &from, &oldfiletype, conn));
	if (conn->export->ro) NE(NFSERR_ROFS);
	NE(diropargs_to_path(&(args->to), &to, &newfiletype, conn));
	if (strcmp(from, to) != 0) {
		OE(_swix(OS_FSControl, _INR(0,2), 25, from, to));
	}
	if (newfiletype != oldfiletype) {
		OE(_swix(OS_File, _INR(0,2), 18, to, newfiletype));
	}
	return SUCCESS;
}


enum accept_stat NFSPROC_GETATTR(struct getattrargs *args, struct attrstat *res, struct server_conn *conn)
{
	char *path;

	NE(nfs2fh_to_path(&(args->fhandle), &path, conn));
	NE(get_fattr(path, -1, &(res->u.attributes), conn));
	/*FIXME - fake directory timestamps */

	return SUCCESS;
}

enum accept_stat NFSPROC_MKDIR(struct mkdirargs *args, struct createres *res, struct server_conn *conn)
{
	char *path;
	int filetype;

	NE(diropargs_to_path(&(args->where), &path, &filetype, conn));
	if (conn->export->ro) NE(NFSERR_ROFS);
	OE(_swix(OS_File, _INR(0,1) | _IN(4), 8, path, 0));
	NE(set_attr(path, &(args->attributes), conn));
	NE(path_to_nfs2fh(path, &(res->u.diropok.file), conn));
	NE(get_fattr(path, -1, &(res->u.diropok.attributes), conn));

	return SUCCESS;
}

enum accept_stat NFSPROC_SETATTR(struct sattrargs *args, struct sattrres *res, struct server_conn *conn)
{
	char *path;

	NE(nfs2fh_to_path(&(args->file), &path, conn));
	if (conn->export->ro) NE(NFSERR_ROFS);
	NE(set_attr(path, &(args->attributes), conn));
	NE(get_fattr(path, -1, &(res->u.attributes), conn));

	return SUCCESS;
}

enum accept_stat NFSPROC_STATFS(struct statfsargs *args, struct statfsres *res, struct server_conn *conn)
{
	char *path;
	unsigned int freelo;
	unsigned int freehi;
	unsigned int sizelo;
	unsigned int sizehi;

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
	(void)args;
	(void)res;
	(void)conn;

	return PROC_UNAVAIL;
}


enum accept_stat NFSPROC_LINK(struct linkargs *args, enum nstat *res, struct server_conn *conn)
{
	(void)args;
	(void)res;
	(void)conn;

	return PROC_UNAVAIL;
}

enum accept_stat NFSPROC_SYMLINK(struct symlinkargs *args, enum nstat *res, struct server_conn *conn)
{
	(void)args;
	(void)res;
	(void)conn;

	return PROC_UNAVAIL;
}

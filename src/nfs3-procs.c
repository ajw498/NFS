/*
	$Id$
	$URL$

	NFSv3 procedures


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

#include "utils.h"
#include "filecache.h"
#include "nfs3-procs.h"

#define NF(x) do { \
	res->status = x; \
	if (res->status != NFS_OK) goto failure; \
} while (0)

#define UF(x) do { \
	if ((x) == NULL) { \
		res->status = NFSERR_IO; \
		goto failure; \
	} else { \
		res->status = NFS_OK; \
	} \
} while (0)

#define OF(x) do { \
	os_error *err = x; \
	if (err) { \
		res->status = oserr_to_nfserr(err->errnum); \
		syslogf(LOGNAME, LOG_ERROR, "Error: %x %s", err->errnum, err->errmess); \
		goto failure; \
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

static inline enum nstat nfs3fh_to_path(struct nfs_fh *fhandle, char **path, struct server_conn *conn)
{
	return fh_to_path(fhandle->data.data, fhandle->data.size, path, conn);
}

static inline enum nstat path_to_nfs3fh(char *path, struct nfs_fh *fhandle, struct server_conn *conn)
{
	fhandle->data.data = NULL;
	fhandle->data.size = NFS3_FHSIZE;

	return path_to_fh(path, &(fhandle->data.data), &(fhandle->data.size), conn);
}

static enum nstat diropargs_to_path(struct diropargs *where, char **path, int *filetype, struct server_conn *conn)
{
	int len;
	char *dirpath;
	char buffer[MAX_PATHNAME];
	int leaflen;

	NR(nfs3fh_to_path(&(where->dir), &dirpath, conn));

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

/* Convert a RISC OS load and execution address into a unix timestamp */
static void loadexec_to_timeval(unsigned int load, unsigned int exec, struct ntimeval *unixtime)
{
	if ((load & 0xFFF00000) != 0xFFF00000) {
		/* A real load/exec address */
		unixtime->seconds = -1;
		unixtime->nseconds = -1;
	} else {
		uint64_t csecs;

		csecs = exec;
		csecs |= ((uint64_t)load & 0xFF) << 32;
		csecs -= 0x336e996a00LL; /* Difference between 1900 and 1970 */
		unixtime->seconds = (unsigned int)((csecs / 100) & 0xFFFFFFFF);
		unixtime->nseconds = (unsigned int)((csecs % 100) * 10000000);
	}
}


/* Convert a unix timestamp into a RISC OS load and execution address */
static void timeval_to_loadexec(struct ntimeval *unixtime, int filetype, unsigned int *load, unsigned int *exec)
{
	uint64_t csecs;

	csecs = unixtime->seconds;
	csecs *= 100;
	csecs += ((int64_t)unixtime->nseconds / 10000000);
	csecs += 0x336e996a00LL; /* Difference between 1900 and 1970 */
	*load = (unsigned int)((csecs >> 32) & 0xFF);
	*load |= (0xFFF00000 | ((filetype & 0xFFF) << 8));
	*exec = (unsigned int)(csecs & 0xFFFFFFFF);
}

static void parse_fattr(char *path, char *leaf, int type, int load, int exec, int len, int attr, struct fattr *fattr, struct server_conn *conn)
{
	fattr->type = type == OBJ_IMAGE ? (conn->export->imagefs ? NFDIR : NFREG) :
	              type == OBJ_DIR ? NFDIR : NFREG;
	fattr->mode  = (attr & 0x01) << 8; /* Owner read */
	fattr->mode |= (attr & 0x02) << 6; /* Owner write */
	fattr->mode |= (attr & 0x10) << 1; /* Group read */
	fattr->mode |= (attr & 0x20) >> 1; /* Group write */
	fattr->mode |= (attr & 0x10) >> 2; /* Others read */
	fattr->mode |= (attr & 0x20) >> 4; /* Others write */
	fattr->nlink = 1;
	fattr->uid = conn->uid;
	fattr->gid = conn->gid;
	fattr->size = len;
	fattr->used = len;
	fattr->rdev.specdata1 = 0;
	fattr->rdev.specdata2 = 0;
	fattr->fsid = conn->export->exportnum;
	fattr->fileid = calc_fileid(path, leaf);
	loadexec_to_timeval(load, exec, &(fattr->atime));
	loadexec_to_timeval(load, exec, &(fattr->ctime));
	loadexec_to_timeval(load, exec, &(fattr->mtime));
}

static enum nstat get_fattr(char *path, int filetype, struct fattr *fattr, struct server_conn *conn)
{
	int type, load, exec, len, attr;

	OR(_swix(OS_File, _INR(0,1) | _OUT(0) | _OUTR(2,5), 17, path, &type, &load, &exec, &len, &attr));

	if (type == 0) {
		return NFSERR_NOENT;
	} else {
		int ftype = (load & 0x000FFF00) >> 8;
		if (((type == OBJ_FILE) || ((type == OBJ_IMAGE) && conn->export->imagefs)) &&
		    (filetype != -1) && (filetype != ftype)) return NFSERR_NOENT;
	}

	parse_fattr(path, NULL, type, load, exec, len, attr, fattr, conn);

	return NFS_OK;
}

static enum nstat set_attr(char *path, struct sattrguard3 *guard, struct sattr3 *sattr, struct wcc_data *obj_wcc, struct server_conn *conn)
{
	unsigned int load;
	unsigned int exec;
	unsigned int guardload;
	unsigned int guardexec;
	int filetype;
	int type;
	int size;
	int attr;

	OR(_swix(OS_File, _INR(0,1) | _OUT(0) | _OUTR(2,5), 17, path, &type, &load, &exec, &size, &attr));
	if (type == 0) return NFSERR_NOENT;
	filetype = (load & 0x000FFF00) >> 8;

	if (guard && guard->check == TRUE) {
		timeval_to_loadexec(&(guard->u.obj_ctime), filetype, &guardload, &guardexec);
		if (guardload != load || guardexec != exec) return NFSERR_NOT_SYNC;
	}

	if (obj_wcc) {
		obj_wcc->before.attributes_follow = TRUE;
		obj_wcc->before.u.attributes.size = size;
		loadexec_to_timeval(load, exec, &(obj_wcc->before.u.attributes.mtime));
		loadexec_to_timeval(load, exec, &(obj_wcc->before.u.attributes.ctime));
	}

	if (sattr->mode.set_it) attr = mode_to_attr(sattr->mode.u.mode);

	if (sattr->mtime.set_it) timeval_to_loadexec(&(sattr->mtime.u.mtime), filetype, &load, &exec);

	if (type == OBJ_IMAGE) type = conn->export->imagefs ? OBJ_DIR : OBJ_FILE;
	if (sattr->size.set_it) {
		size = sattr->size.u.size;
	}

	if (type == OBJ_FILE) {
		/* Write through the cache to ensure consistency */
		NR(filecache_setattr(path, load, exec, size, attr));
	} else {
		OR(_swix(OS_File, _INR(0,3) | _IN(5), 1, path, load, exec, attr));
	}

	if (obj_wcc) {
		obj_wcc->after.attributes_follow = TRUE;
		parse_fattr(path, NULL, type, load, exec, size, attr, &(obj_wcc->after.u.attributes), conn);
	}

	return NFS_OK;
}

enum accept_stat NFSPROC3_NULL(struct server_conn *conn)
{
	(void)conn;

	return SUCCESS;
}

enum accept_stat NFSPROC3_GETATTR(struct GETATTR3args *args, struct GETATTR3res *res, struct server_conn *conn)
{
	char *path;

	NF(nfs3fh_to_path(&(args->object), &path, conn));
	NF(get_fattr(path, -1, &(res->u.resok.obj_attributes), conn));
	/*FIXME - fake directory timestamps */

failure:
	return SUCCESS;
}

enum accept_stat NFSPROC3_SETATTR(struct sattrargs *args, struct sattrres *res, struct server_conn *conn)
{
	char *path;

	NF(nfs3fh_to_path(&(args->file), &path, conn));
	if (conn->export->ro) NF(NFSERR_ROFS);
	NF(set_attr(path, &(args->guard), &(args->attributes), &(res->u.resok.obj_wcc), conn));

	return SUCCESS;

failure:
	res->u.resfail.obj_wcc.before.attributes_follow = FALSE;
	res->u.resfail.obj_wcc.after.attributes_follow = FALSE;

	return SUCCESS;
}

enum accept_stat NFSPROC3_LOOKUP(struct diropargs *args, struct diropres *res, struct server_conn *conn)
{
	char *path;
	int filetype;

	NF(diropargs_to_path(args, &path, &filetype, conn));
	NF(path_to_nfs3fh(path, &(res->u.diropok.file), conn));
	res->u.diropok.obj_attributes.attributes_follow = TRUE;
	NF(get_fattr(path, filetype, &(res->u.diropok.obj_attributes.u.attributes), conn));
	res->u.diropok.dir_attributes.attributes_follow = FALSE; /*FIXME?*/

	return SUCCESS;

failure:
	res->u.resfail.dir_attributes.attributes_follow = FALSE;

	return SUCCESS;
}

enum accept_stat NFSPROC3_ACCESS(struct ACCESS3args *args, struct ACCESS3res *res, struct server_conn *conn)
{
	char *path;

	NF(nfs3fh_to_path(&(args->object), &path, conn));
	res->u.resok.obj_attributes.attributes_follow = TRUE;
	NF(get_fattr(path, -1, &(res->u.resok.obj_attributes.u.attributes), conn));
	res->u.resok.access = args->access & 0x1F;
	if (conn->export->ro) res->u.resok.access &= 0x3;

	return SUCCESS;

failure:
	res->u.resfail.obj_attributes.attributes_follow = FALSE;

	return SUCCESS;
}

enum accept_stat NFSPROC3_READLINK(struct readlinkargs *args, struct readlinkres *res, struct server_conn *conn)
{
	(void)args;
	(void)conn;

	res->status = NFSERR_NOTSUPP;
	res->u.resok.symlink_attributes.attributes_follow = FALSE;

	return SUCCESS;
}

enum accept_stat NFSPROC3_READ(struct readargs *args, struct readres *res, struct server_conn *conn)
{
	char *path;
	unsigned int read;
	int eof;
	char *data;
	unsigned int load;
	unsigned int exec;
	unsigned int size;
	unsigned int attr;

	NF(nfs3fh_to_path(&(args->file), &path, conn));
	NF(filecache_read(path, args->count, args->offset, &data, &read, &eof));
	res->u.resok.data.data = data;
	res->u.resok.data.size = read;
	res->u.resok.count = read;
	res->u.resok.eof = eof ? TRUE : FALSE;
	res->u.resok.file_attributes.attributes_follow = TRUE;
	NF(filecache_getattr(path, &load, &exec, &size, &attr, NULL));
	parse_fattr(path, NULL, OBJ_FILE, load, exec, size, attr, &(res->u.resok.file_attributes.u.attributes), conn);

	return SUCCESS;

failure:
	res->u.resfail.file_attributes.attributes_follow = FALSE;

	return SUCCESS;
}

enum accept_stat NFSPROC3_WRITE(struct writeargs *args, struct writeres *res, struct server_conn *conn)
{
	char *path;
	int sync = args->stable != UNSTABLE;
	int amount;
	unsigned int load;
	unsigned int exec;
	unsigned int size;
	unsigned int attr;
	int cached;

	NF(nfs3fh_to_path(&(args->file), &path, conn));
	if (conn->export->ro) NF(NFSERR_ROFS);

	res->u.resok.file_wcc.before.attributes_follow = FALSE;
	amount = args->count;
	if (amount > args->data.size) amount = args->data.size;

	NF(filecache_write(path, amount, args->offset, args->data.data, sync, res->u.resok.verf));

	res->u.resok.file_wcc.after.attributes_follow = TRUE;
	NF(filecache_getattr(path, &load, &exec, &size, &attr, NULL));
	parse_fattr(path, NULL, OBJ_FILE, load, exec, size, attr, &(res->u.resok.file_wcc.after.u.attributes), conn);

	res->u.resok.count = amount;
	res->u.resok.committed = sync ? FILE_SYNC : UNSTABLE;

	return SUCCESS;

failure:
	res->u.resfail.file_wcc.before.attributes_follow = FALSE;
	res->u.resfail.file_wcc.after.attributes_follow = FALSE;

	return SUCCESS;
}

enum accept_stat NFSPROC3_CREATE(struct createargs *args, struct createres *res, struct server_conn *conn)
{
	char *path;
	int filetype;
	int size = 0;
	int type;
	unsigned int verf;
	unsigned int exec;

	NF(diropargs_to_path(&(args->where), &path, &filetype, conn));
	if (conn->export->ro) NF(NFSERR_ROFS);

	switch (args->how.mode) {
	case GUARDED:
		OF(_swix(OS_File, _INR(0,1) | _OUT(0), 17, path, &type));
		if (type != OBJ_NONE) NF(NFSERR_EXIST);
		/* Fallthrough */
	case UNCHECKED:
		if (args->how.u.obj_attributes.size.set_it) size = args->how.u.obj_attributes.size.u.size;
		if (size < 0) NF(NFSERR_FBIG);
		OF(_swix(OS_File, _INR(0,5), 11, path, filetype, 0, 0, 0));
		NF(set_attr(path, NULL, &(args->how.u.obj_attributes), NULL, conn));
		break;
	case EXCLUSIVE:
		verf  = ((int *)args->how.u.verf)[0];
		verf ^= ((int *)args->how.u.verf)[1];
		OF(_swix(OS_File, _INR(0,1) | _OUT(0) | _OUT(3), 17, path, &type, &exec));
		if (type != OBJ_NONE) {
			if (exec != verf) NF(NFSERR_EXIST);
		} else {
			OF(_swix(OS_File, _INR(0,5), 11, path, filetype, 0, 0, 0));
			OF(_swix(OS_File, _INR(0,1) | _IN(3), 3, path, verf));
		}
	}
	res->u.diropok.obj.handle_follows = TRUE;
	NF(path_to_nfs3fh(path, &(res->u.diropok.obj.u.handle), conn));
	res->u.diropok.obj_attributes.attributes_follow = TRUE;
	NF(get_fattr(path, -1, &(res->u.diropok.obj_attributes.u.attributes), conn));
	res->u.diropok.dir_wcc.before.attributes_follow = FALSE; /*FIXME?*/
	res->u.diropok.dir_wcc.after.attributes_follow = FALSE;

	return SUCCESS;

failure:
	res->u.resfail.dir_wcc.before.attributes_follow = FALSE;
	res->u.resfail.dir_wcc.after.attributes_follow = FALSE;

	return SUCCESS;
}

enum accept_stat NFSPROC3_MKDIR(struct mkdirargs *args, struct createres *res, struct server_conn *conn)
{
	char *path;
	int filetype;

	NF(diropargs_to_path(&(args->where), &path, &filetype, conn));
	if (conn->export->ro) NF(NFSERR_ROFS);
	OF(_swix(OS_File, _INR(0,1) | _IN(4), 8, path, 0));
	NF(set_attr(path, NULL, &(args->attributes), NULL, conn));

	res->u.diropok.obj.handle_follows = TRUE;
	NF(path_to_nfs3fh(path, &(res->u.diropok.obj.u.handle), conn));
	res->u.diropok.obj_attributes.attributes_follow = TRUE;
	NF(get_fattr(path, -1, &(res->u.diropok.obj_attributes.u.attributes), conn));
	res->u.diropok.dir_wcc.before.attributes_follow = FALSE; /*FIXME?*/
	res->u.diropok.dir_wcc.after.attributes_follow = FALSE;

	return SUCCESS;

failure:
	res->u.resfail.dir_wcc.before.attributes_follow = FALSE;
	res->u.resfail.dir_wcc.after.attributes_follow = FALSE;

	return SUCCESS;
}

enum accept_stat NFSPROC3_SYMLINK(struct SYMLINK3args *args, struct SYMLINK3res *res, struct server_conn *conn)
{
	(void)args;
	(void)conn;

	res->status = NFSERR_NOTSUPP;
	res->u.resok.dir_wcc.before.attributes_follow = FALSE;
	res->u.resok.dir_wcc.after.attributes_follow = FALSE;

	return SUCCESS;
}

enum accept_stat NFSPROC3_MKNOD(struct MKNOD3args *args, struct MKNOD3res *res, struct server_conn *conn)
{
	(void)args;
	(void)conn;

	res->status = NFSERR_NOTSUPP;
	res->u.resok.dir_wcc.before.attributes_follow = FALSE;
	res->u.resok.dir_wcc.after.attributes_follow = FALSE;

	return SUCCESS;
}

enum accept_stat NFSPROC3_REMOVE(struct diropargs *args, struct removeres *res, struct server_conn *conn)
{
	char *path;

	NF(diropargs_to_path(args, &path, NULL, conn));
	if (conn->export->ro) NF(NFSERR_ROFS);
	OF(_swix(OS_File, _INR(0,1), 6, path));

	res->u.resok.dir_wcc.before.attributes_follow = FALSE;
	res->u.resok.dir_wcc.after.attributes_follow = FALSE;

	return SUCCESS;

failure:
	res->u.resfail.dir_wcc.before.attributes_follow = FALSE;
	res->u.resfail.dir_wcc.after.attributes_follow = FALSE;

	return SUCCESS;
}

enum accept_stat NFSPROC3_RMDIR(struct diropargs *args, struct removeres *res, struct server_conn *conn)
{
	char *path;

	NF(diropargs_to_path(args, &path, NULL, conn));
	if (conn->export->ro) NF(NFSERR_ROFS);
	OF(_swix(OS_File, _INR(0,1), 6, path));

	res->u.resok.dir_wcc.before.attributes_follow = FALSE;
	res->u.resok.dir_wcc.after.attributes_follow = FALSE;

	return SUCCESS;

failure:
	res->u.resfail.dir_wcc.before.attributes_follow = FALSE;
	res->u.resfail.dir_wcc.after.attributes_follow = FALSE;

	return SUCCESS;
}

enum accept_stat NFSPROC3_RENAME(struct renameargs *args, struct renameres *res, struct server_conn *conn)
{
	char *from;
	char *to;
	int oldfiletype;
	int newfiletype;

	NF(diropargs_to_path(&(args->from), &from, &oldfiletype, conn));
	if (conn->export->ro) NF(NFSERR_ROFS);
	NF(diropargs_to_path(&(args->to), &to, &newfiletype, conn));
	if (strcmp(from, to) != 0) {
		OF(_swix(OS_FSControl, _INR(0,2), 25, from, to));
	}
	if (newfiletype != oldfiletype) {
		OF(_swix(OS_File, _INR(0,2), 18, to, newfiletype));
	}

	res->u.resok.fromdir_wcc.before.attributes_follow = FALSE;
	res->u.resok.fromdir_wcc.after.attributes_follow = FALSE;
	res->u.resok.todir_wcc.before.attributes_follow = FALSE;
	res->u.resok.todir_wcc.after.attributes_follow = FALSE;

	return SUCCESS;

failure:
	res->u.resfail.fromdir_wcc.before.attributes_follow = FALSE;
	res->u.resfail.fromdir_wcc.after.attributes_follow = FALSE;
	res->u.resfail.todir_wcc.before.attributes_follow = FALSE;
	res->u.resfail.todir_wcc.after.attributes_follow = FALSE;

	return SUCCESS;
}

enum accept_stat NFSPROC3_LINK(struct LINK3args *args, struct LINK3res *res, struct server_conn *conn)
{
	(void)args;
	(void)conn;

	res->status = NFSERR_NOTSUPP;
	res->u.resok.file_attributes.attributes_follow = FALSE;
	res->u.resok.linkdir_wcc.before.attributes_follow = FALSE;
	res->u.resok.linkdir_wcc.after.attributes_follow = FALSE;

	return SUCCESS;
}

enum accept_stat NFSPROC3_READDIR(struct readdirargs *args, struct readdirres *res, struct server_conn *conn)
{
	char *path;
	int cookie = (int)args->cookie;
	int bytes = 0;
	struct entry **lastentry = &(res->u.readdirok.entries);

	NF(nfs3fh_to_path(&(args->dir), &path, conn));

	res->u.readdirok.entries = NULL;
	res->u.readdirok.eof = FALSE;
	memset(res->u.readdirok.cookieverf, 0, NFS3_COOKIEVERFSIZE);
	res->u.readdirok.dir_attributes.attributes_follow = FALSE;

	while (cookie != -1) {
		int read;
		char buffer[1024];

		/* We have to read one entry at a time, which is less
		   efficient than reading several, as we need to return
		   a cookie for every entry. */
		OF(_swix(OS_GBPB, _INR(0,6) | _OUTR(3,4), 10, path, buffer, 1, cookie, sizeof(buffer), 0, &read, &cookie));
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
			UF(entry = palloc(sizeof(struct entry), conn->pool));

			entry->fileid = calc_fileid(path, leaf);
			UF(leaf = filename_unixify(leaf, strlen(leaf), &leaflen, conn->pool));
			type = ((unsigned int *)buffer)[4];
			if (type == 1 || (type == 3 && conn->export->imagefs == 0)) {
				UF(leaf = addfiletypeext(leaf, leaflen, 0, filetype, &leaflen, conn->export->defaultfiletype, conn->export->xyzext, conn->pool));
			}
			entry->name.data = leaf;
			entry->name.size = leaflen;
			entry->cookie = cookie;

			bytes += 24 + (leaflen + 3) & ~3;
			if (bytes > args->count) {
				return SUCCESS;
			}
			entry->next = NULL;
			*lastentry = entry;
			lastentry = &(entry->next);
		}
		if (cookie == -1) res->u.readdirok.eof = TRUE;
	}

	return SUCCESS;

failure:
	res->u.resfail.dir_attributes.attributes_follow = FALSE;

	return SUCCESS;
}

enum accept_stat NFSPROC3_READDIRPLUS(struct readdirplusargs *args, struct readdirplusres *res, struct server_conn *conn)
{
	char *path;
	int cookie = (int)args->cookie;
	int bytes = 0;
	int tbytes = 0;
	int pathlen;
	char pathbuffer[MAX_PATHNAME];
	struct entryplus3 **lastentry = &(res->u.readdirok.entries);

	NF(nfs3fh_to_path(&(args->dir), &path, conn));
	pathlen = strlen(path);
	if (pathlen + 2 > MAX_PATHNAME) NF(NFSERR_NAMETOOLONG);
	memcpy(pathbuffer, path, pathlen);
	pathbuffer[pathlen] = '.';

	res->u.readdirok.entries = NULL;
	res->u.readdirok.eof = FALSE;
	memset(res->u.readdirok.cookieverf, 0, NFS3_COOKIEVERFSIZE);
	res->u.readdirok.dir_attributes.attributes_follow = FALSE;

	while (cookie != -1) {
		int read;
		char buffer[MAX_PATHNAME + 20];

		/* We have to read one entry at a time, which is less
		   efficient than reading several, as we need to return
		   a cookie for every entry. */
		OF(_swix(OS_GBPB, _INR(0,6) | _OUTR(3,4), 10, path, buffer, 1, cookie, sizeof(buffer), 0, &read, &cookie));
		if (read > 0) {
			struct entryplus3 *entry;
			char *leaf = buffer + 20;
			unsigned int leaflen;
			int filetype;
			unsigned int load = ((unsigned int *)buffer)[0];
			unsigned int exec = ((unsigned int *)buffer)[1];
			unsigned int len  = ((unsigned int *)buffer)[2];
			unsigned int attr = ((unsigned int *)buffer)[3];
			unsigned int type = ((unsigned int *)buffer)[4];

			if ((load & 0xFFF00000) == 0xFFF00000) {
				filetype = (load & 0x000FFF00) >> 8;
			} else {
				filetype = conn->export->defaultfiletype;
			}
			UF(entry = palloc(sizeof(struct entryplus3), conn->pool));

			entry->fileid = calc_fileid(path, leaf);
			UF(leaf = filename_unixify(leaf, strlen(leaf), &leaflen, conn->pool));
			if (type == 1 || (type == 3 && conn->export->imagefs == 0)) {
				UF(leaf = addfiletypeext(leaf, leaflen, 0, filetype, &leaflen, conn->export->defaultfiletype, conn->export->xyzext, conn->pool));
			}
			if (pathlen + 2 + leaflen > MAX_PATHNAME) NF(NFSERR_NAMETOOLONG);
			memcpy(pathbuffer + pathlen + 1, leaf, leaflen + 1);

			entry->name.data = leaf;
			entry->name.size = leaflen;
			entry->cookie = cookie;
			entry->name_handle.handle_follows = TRUE;
			NF(path_to_nfs3fh(pathbuffer, &(entry->name_handle.u.handle), conn));
			entry->name_attributes.attributes_follow = TRUE;
			parse_fattr(pathbuffer, NULL/*FIXME*/, type, load, exec, len, attr, &(entry->name_attributes.u.attributes), conn);

			bytes += 4 + (leaflen + 3) & ~3;
			if (bytes > args->count) {
				return SUCCESS;
			}
			tbytes += 168 + (leaflen + 3) & ~3;
			if (tbytes > args->maxcount) {
				return SUCCESS;
			}
			entry->next = NULL;
			*lastentry = entry;
			lastentry = &(entry->next);
		}
		if (cookie == -1) res->u.readdirok.eof = TRUE;
	}

	return SUCCESS;

failure:
	res->u.resfail.dir_attributes.attributes_follow = FALSE;

	return SUCCESS;
}

enum accept_stat NFSPROC3_FSSTAT(struct FSSTAT3args *args, struct FSSTAT3res *res, struct server_conn *conn)
{
	char *path;
	unsigned int freelo;
	unsigned int freehi;
	unsigned int sizelo;
	unsigned int sizehi;

	NF(nfs3fh_to_path(&(args->fsroot), &path, conn));
	res->u.resok.obj_attributes.attributes_follow = TRUE;
	NF(get_fattr(path, -1, &(res->u.resok.obj_attributes.u.attributes), conn));

	if (_swix(OS_FSControl, _INR(0,1) | _OUTR(0,1) | _OUTR(3,4), 55, path, &freelo, &freehi, &sizelo, &sizehi)) {
		OF(_swix(OS_FSControl, _INR(0,1) | _OUT(0) | _OUT(2), 49, path, &freelo, &sizelo));
		freehi = 0;
		sizehi = 0;
	}

	res->u.resok.tbytes = ((uint64)sizehi) << 32 | (uint64)sizelo;
	res->u.resok.fbytes = ((uint64)freehi) << 32 | (uint64)freelo;
	res->u.resok.abytes = res->u.resok.fbytes;
	res->u.resok.tfiles = res->u.resok.tbytes;
	res->u.resok.ffiles = res->u.resok.fbytes;
	res->u.resok.afiles = res->u.resok.abytes;
	res->u.resok.invarsec = 0;

	return SUCCESS;

failure:
	res->u.resfail.obj_attributes.attributes_follow = FALSE;

	return SUCCESS;
}

enum accept_stat NFSPROC3_FSINFO(struct fsinfoargs *args, struct fsinfores *res, struct server_conn *conn)
{
	char *path;

	NF(nfs3fh_to_path(&(args->fsroot), &path, conn));
	res->u.resok.obj_attributes.attributes_follow = TRUE;
	NF(get_fattr(path, -1, &(res->u.resok.obj_attributes.u.attributes), conn));
	res->u.resok.rtmax = conn->tcp ? MAX_DATABUFFER : MAX_UDPBUFFER; /*FIXME*/
	res->u.resok.rtpref = res->u.resok.rtmax;
	res->u.resok.rtmult = 4096;
	res->u.resok.wtmax = res->u.resok.rtmax;
	res->u.resok.wtpref = res->u.resok.rtpref;
	res->u.resok.wtmult = res->u.resok.rtmult;
	res->u.resok.dtpref = res->u.resok.rtmax;
	res->u.resok.maxfilesize = 0x7FFFFFFFLL;
	res->u.resok.time_delta.seconds = 0;
	res->u.resok.time_delta.nseconds = 10000000;
	res->u.resok.properties = FSF3_HOMOGENEOUS | FSF3_CANSETTIME;

	return SUCCESS;

failure:
	res->u.resfail.obj_attributes.attributes_follow = FALSE;

	return SUCCESS;
}

enum accept_stat NFSPROC3_PATHCONF(struct PATHCONF3args *args, struct PATHCONF3res *res, struct server_conn *conn)
{
	char *path;

	NF(nfs3fh_to_path(&(args->object), &path, conn));
	res->u.resok.obj_attributes.attributes_follow = TRUE;
	NF(get_fattr(path, -1, &(res->u.resok.obj_attributes.u.attributes), conn));
	res->u.resok.linkmax = 1;
	res->u.resok.name_max = MAX_PATHNAME;
	res->u.resok.no_trunc = TRUE;
	res->u.resok.chown_restricted = FALSE;
	res->u.resok.case_insensitive = TRUE;
	res->u.resok.case_preserving = TRUE;
	return SUCCESS;

failure:
	res->u.resfail.obj_attributes.attributes_follow = FALSE;

	return SUCCESS;
}

enum accept_stat NFSPROC3_COMMIT(struct COMMIT3args *args, struct COMMIT3res *res, struct server_conn *conn)
{
	char *path;
	unsigned int load;
	unsigned int exec;
	unsigned int size;
	unsigned int attr;
	int cached;

	NF(nfs3fh_to_path(&(args->file), &path, conn));

	res->u.resok.file_wcc.before.attributes_follow = FALSE;
	NF(filecache_commit(path, res->u.resok.verf));
	NF(filecache_getattr(path, &load, &exec, &size, &attr, &cached));
	if (cached) {
		res->u.resok.file_wcc.after.attributes_follow = TRUE;
		parse_fattr(path, NULL, OBJ_FILE, load, exec, size, attr, &(res->u.resok.file_wcc.after.u.attributes), conn);
	} else {
		res->u.resok.file_wcc.after.attributes_follow = FALSE;
	}

	return SUCCESS;

failure:
	res->u.resfail.file_wcc.before.attributes_follow = FALSE;
	res->u.resfail.file_wcc.after.attributes_follow = FALSE;

	return SUCCESS;
}


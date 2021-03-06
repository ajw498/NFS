/*
	NFSv3 procedures


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

#define NFS3

#include "moonfish.h"
#include "exports.h"
#include "utils.h"
#include "filecache.h"
#include "nfs3-procs.h"


static inline nstat nfs3fh_to_path(struct nfs_fh3 *fhandle, char **path, struct server_conn *conn)
{
	return fh_to_path(fhandle->data.data, fhandle->data.size, path, conn);
}

static inline nstat path_to_nfs3fh(char *path, struct nfs_fh3 *fhandle, struct server_conn *conn)
{
	fhandle->data.data = NULL;
	fhandle->data.size = NFS3_FHSIZE;

	return path_to_fh(path, &(fhandle->data.data), &(fhandle->data.size), conn);
}

static nstat diropargs_to_path(struct diropargs3 *where, char **path, int *filetype, struct server_conn *conn)
{
	int len;
	char *dirpath;
	static char buffer[MAX_PATHNAME];
	char *leaf = where->name.data;
	size_t leaflen = where->name.size;

	NR(nfs3fh_to_path(&(where->dir), &dirpath, conn));

	if (choices.fromenc != (iconv_t)-1) {
		char *encleaf;
		size_t encleaflen;
		static char buffer2[MAX_PATHNAME];

		encleaf = buffer2;
		encleaflen = sizeof(buffer2);
		if (iconv(choices.fromenc, &leaf, &leaflen, &encleaf, &encleaflen) == -1) {
			iconv(choices.fromenc, NULL, NULL, NULL, NULL);
			syslogf(LOGNAME, LOG_ERROR, "Error: Iconv failed (%d)", errno);
			if (errno == E2BIG) {
				NR(NFSERR_NAMETOOLONG);
			} else {
				NR(NFSERR_IO);
			}
		}

		leaf = buffer2;
		leaflen = sizeof(buffer2) - encleaflen;
	}

	leaflen = filename_riscosify(leaf, leaflen, buffer, sizeof(buffer), filetype, conn->export->defaultfiletype, conn->export->xyzext, choices.macforks, 0);

	len = strlen(dirpath);
	UR(*path = palloc(len + leaflen + 2, conn->pool));

	memcpy(*path, dirpath, len);
	(*path)[len++] = '.';
	memcpy(*path + len, buffer, leaflen);
	len += leaflen;
	(*path)[len] = '\0';

	return NFS_OK;
}

static void parse_fattr(char *path, int type, int load, int exec, int len, int attr, struct fattr3 *fattr, struct server_conn *conn)
{
	fattr->type = type == OBJ_IMAGE ? (conn->export->imagefs ? NFDIR : NFREG) :
	              type == OBJ_DIR ? NFDIR : NFREG;
	fattr->mode  = (attr & 0x01) << 8; /* Owner read */
	fattr->mode |= (attr & 0x02) << 6; /* Owner write */
	fattr->mode |= (attr & 0x10) << 1; /* Group read */
	fattr->mode |= (attr & 0x20) >> 1; /* Group write */
	fattr->mode |= (attr & 0x10) >> 2; /* Others read */
	fattr->mode |= (attr & 0x20) >> 4; /* Others write */
	if ((load & 0xFFFFFF00) == (0xFFF00000 | (UNIXEX_FILETYPE << 8))) {
		/* Add executable permissions for UnixEx files */
		fattr->mode |= (attr & 0x10) ? 0111 : 0100;
	}
	/* Apply the unumask to force access */
	fattr->mode |= conn->export->unumask;
	/* Set executable permissions for directories if they have read permission */
	if (fattr->type == NFDIR) fattr->mode |= (fattr->mode & 0444) >> 2;
	/* Remove bits requested by the umask */
	fattr->mode &= ~conn->export->umask;
	fattr->nlink = 1;
	fattr->uid = conn->uid;
	fattr->gid = conn->gid;
	fattr->size = len;
	fattr->used = len;
	fattr->rdev.specdata1 = 0;
	fattr->rdev.specdata2 = 0;
	fattr->fsid = conn->export->exportnum;
	fattr->fileid = calc_fileid(path, NULL);
	loadexec_to_timeval(load, exec, &(fattr->atime.seconds), &(fattr->atime.nseconds), 0);
	loadexec_to_timeval(load, exec, &(fattr->ctime.seconds), &(fattr->ctime.nseconds), 0);
	loadexec_to_timeval(load, exec, &(fattr->mtime.seconds), &(fattr->mtime.nseconds), 0);
}

static nstat get_fattr(char *path, int filetype, struct fattr3 *fattr, int *access, struct server_conn *conn)
{
	unsigned int type;
	unsigned int load;
	unsigned int exec;
	unsigned int len;
	unsigned int attr;
	int cached;

	NR(nfserr_removenfs4(filecache_getattr(path, &load, &exec, &len, &attr, &cached)));
	if (cached) {
		type = OBJ_FILE;
	} else {
		OR(_swix(OS_File, _INR(0,1) | _OUT(0) | _OUTR(2,5), 17, path, &type, &load, &exec, &len, &attr));
	}

	if (type == OBJ_IMAGE) type = conn->export->imagefs ? OBJ_DIR : OBJ_FILE;
	if (type == OBJ_NONE) {
		return NFSERR_NOENT;
	} else {
		int ftype = (load & 0x000FFF00) >> 8;
		if ((type == OBJ_FILE) && (conn->export->xyzext != NEVER) && (filetype != -1) && (filetype != ftype) && (ftype != UNIXEX_FILETYPE)) return NFSERR_NOENT;
	}

	if ((type == OBJ_DIR) && conn->export->fakedirtimes) {
		unsigned int block[2];

		block[0] = 3;
		OR(_swix(OS_Word, _INR(0, 1), 14, block));
		exec = block[0] & 0xFFFFFFC0;
		load = (load & 0xFFFFFF00) | (block[1] & 0xFF);
	}

	if (access && (type == OBJ_FILE)) {
		if ((attr & 0x1) == 0) *access &= ~ACCESS3_READ;
		if ((attr & 0x2) == 0) *access &= ~(ACCESS3_MODIFY | ACCESS3_EXTEND);
	}

	parse_fattr(path, type, load, exec, len, attr, fattr, conn);

	return NFS_OK;
}

static nstat set_attr(char *path, struct sattrguard3 *guard, struct sattr3 *sattr, struct wcc_data *obj_wcc, struct server_conn *conn)
{
	unsigned int load;
	unsigned int exec;
	unsigned int guardload;
	unsigned int guardexec;
	int filetype;
	int type;
	int attr;
	int size;
	int setsize;

	OR(_swix(OS_File, _INR(0,1) | _OUT(0) | _OUTR(2,5), 17, path, &type, &load, &exec, &size, &attr));
	if (type == 0) return NFSERR_NOENT;
	filetype = (load & 0x000FFF00) >> 8;

	if (guard && guard->check == TRUE) {
		timeval_to_loadexec(guard->u.obj_ctime.seconds, guard->u.obj_ctime.nseconds, filetype, &guardload, &guardexec, 0);
		if (guardload != load || guardexec != exec) return NFSERR_NOT_SYNC;
	}

	if (obj_wcc) {
		obj_wcc->before.attributes_follow = TRUE;
		obj_wcc->before.u.attributes.size = size;
		loadexec_to_timeval(load, exec, &(obj_wcc->before.u.attributes.mtime.seconds), &(obj_wcc->before.u.attributes.mtime.nseconds), 0);
		loadexec_to_timeval(load, exec, &(obj_wcc->before.u.attributes.ctime.seconds), &(obj_wcc->before.u.attributes.ctime.nseconds), 0);
	}

	if (sattr->mode.set_it) attr = mode_to_attr(sattr->mode.u.mode);

	if (sattr->mtime.set_it) timeval_to_loadexec(sattr->mtime.u.mtime.seconds, sattr->mtime.u.mtime.nseconds, filetype, &load, &exec, 0);

	if (type == OBJ_IMAGE) type = conn->export->imagefs ? OBJ_DIR : OBJ_FILE;

	setsize = sattr->size.set_it && (size != sattr->size.u.size);
	if (sattr->size.set_it) {
		if (sattr->size.u.size > 0x7FFFFFFFLL) NR(NFSERR_FBIG);
		size = (unsigned int)sattr->size.u.size;
	}

	if (type == OBJ_FILE) {
		/* Write through the cache to ensure consistency */
		NR(nfserr_removenfs4(filecache_setattr(path, STATEID_NONE, load, exec, attr, size, setsize)));
	}
	OR(_swix(OS_File, _INR(0,3) | _IN(5), 1, path, load, exec, attr));

	if (obj_wcc) {
		obj_wcc->after.attributes_follow = TRUE;
		parse_fattr(path, type, load, exec, size, attr, &(obj_wcc->after.u.attributes), conn);
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

	NF(nfs3fh_to_path(&(args->fhandle), &path, conn));
	NF(get_fattr(path, -1, &(res->u.resok.obj_attributes), NULL, conn));

failure:
	return SUCCESS;
}

enum accept_stat NFSPROC3_SETATTR(struct sattrargs3 *args, struct sattrres3 *res, struct server_conn *conn)
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

enum accept_stat NFSPROC3_LOOKUP(struct diropargs3 *args, struct diropres3 *res, struct server_conn *conn)
{
	char *path;
	int filetype;

	NF(diropargs_to_path(args, &path, &filetype, conn));
	NF(path_to_nfs3fh(path, &(res->u.diropok.file), conn));
	res->u.diropok.obj_attributes.attributes_follow = TRUE;
	NF(get_fattr(path, filetype, &(res->u.diropok.obj_attributes.u.attributes), NULL, conn));
	res->u.diropok.dir_attributes.attributes_follow = FALSE;

	return SUCCESS;

failure:
	res->u.resfail.dir_attributes.attributes_follow = FALSE;

	return SUCCESS;
}

enum accept_stat NFSPROC3_ACCESS(struct ACCESS3args *args, struct ACCESS3res *res, struct server_conn *conn)
{
	char *path;

	NF(nfs3fh_to_path(&(args->object), &path, conn));
	res->u.resok.access = args->access & 0x1F;
	if (conn->export->ro) res->u.resok.access &= ACCESS3_READ | ACCESS3_LOOKUP;
	res->u.resok.obj_attributes.attributes_follow = TRUE;
	NF(get_fattr(path, -1, &(res->u.resok.obj_attributes.u.attributes), &(res->u.resok.access), conn));

	return SUCCESS;

failure:
	res->u.resfail.obj_attributes.attributes_follow = FALSE;

	return SUCCESS;
}

enum accept_stat NFSPROC3_READLINK(struct readlinkargs3 *args, struct readlinkres3 *res, struct server_conn *conn)
{
	(void)args;
	(void)conn;

	res->status = NFSERR_NOTSUPP;
	res->u.resok.symlink_attributes.attributes_follow = FALSE;

	return SUCCESS;
}

enum accept_stat NFSPROC3_READ(struct readargs3 *args, struct readres3 *res, struct server_conn *conn)
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
	if (args->offset > 0x7FFFFFFFLL) NF(NFSERR_FBIG);
	NF(nfserr_removenfs4(filecache_read(path, STATEID_NONE, args->count, (unsigned int)args->offset, &data, &read, &eof)));
	res->u.resok.data.data = data;
	res->u.resok.data.size = read;
	res->u.resok.count = read;
	res->u.resok.eof = eof ? TRUE : FALSE;
	res->u.resok.file_attributes.attributes_follow = TRUE;
	NF(nfserr_removenfs4(filecache_getattr(path, &load, &exec, &size, &attr, NULL)));
	parse_fattr(path, OBJ_FILE, load, exec, size, attr, &(res->u.resok.file_attributes.u.attributes), conn);

	return SUCCESS;

failure:
	res->u.resfail.file_attributes.attributes_follow = FALSE;

	return SUCCESS;
}

enum accept_stat NFSPROC3_WRITE(struct writeargs3 *args, struct writeres3 *res, struct server_conn *conn)
{
	char *path;
	int sync = args->stable != UNSTABLE;
	int amount;
	unsigned int load;
	unsigned int exec;
	unsigned int size;
	unsigned int attr;

	NF(nfs3fh_to_path(&(args->file), &path, conn));
	if (conn->export->ro) NF(NFSERR_ROFS);

	res->u.resok.file_wcc.before.attributes_follow = FALSE;
	amount = args->count;
	if (amount > args->data.size) amount = args->data.size;

	if (args->offset > 0x7FFFFFFFLL) NF(NFSERR_FBIG);
	NF(nfserr_removenfs4(filecache_write(path, STATEID_NONE, amount, (unsigned int)args->offset, args->data.data, sync, res->u.resok.verf)));

	res->u.resok.file_wcc.after.attributes_follow = TRUE;
	NF(nfserr_removenfs4(filecache_getattr(path, &load, &exec, &size, &attr, NULL)));
	parse_fattr(path, OBJ_FILE, load, exec, size, attr, &(res->u.resok.file_wcc.after.u.attributes), conn);

	res->u.resok.count = amount;
	res->u.resok.committed = sync ? FILE_SYNC : UNSTABLE;

	return SUCCESS;

failure:
	res->u.resfail.file_wcc.before.attributes_follow = FALSE;
	res->u.resfail.file_wcc.after.attributes_follow = FALSE;

	return SUCCESS;
}

enum accept_stat NFSPROC3_CREATE(struct createargs3 *args, struct createres3 *res, struct server_conn *conn)
{
	char *path;
	int filetype;
	unsigned int size = 0;
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
		if (args->how.u.obj_attributes.size.set_it) size = (unsigned int)args->how.u.obj_attributes.size.u.size;
		if (size & 0x80000000) NF(NFSERR_FBIG);
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
	NF(get_fattr(path, -1, &(res->u.diropok.obj_attributes.u.attributes), NULL, conn));
	res->u.diropok.dir_wcc.before.attributes_follow = FALSE;
	res->u.diropok.dir_wcc.after.attributes_follow = FALSE;

	return SUCCESS;

failure:
	res->u.resfail.dir_wcc.before.attributes_follow = FALSE;
	res->u.resfail.dir_wcc.after.attributes_follow = FALSE;

	return SUCCESS;
}

enum accept_stat NFSPROC3_MKDIR(struct mkdirargs3 *args, struct createres3 *res, struct server_conn *conn)
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
	NF(get_fattr(path, -1, &(res->u.diropok.obj_attributes.u.attributes), NULL, conn));
	res->u.diropok.dir_wcc.before.attributes_follow = FALSE;
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

enum accept_stat NFSPROC3_REMOVE(struct diropargs3 *args, struct removeres3 *res, struct server_conn *conn)
{
	char *path;
	int filetype;

	NF(diropargs_to_path(args, &path, &filetype, conn));
	if (conn->export->ro) NF(NFSERR_ROFS);
	NF(filecache_closecache(path));
	OF(_swix(OS_File, _INR(0,1), 6, path));

	res->u.resok.dir_wcc.before.attributes_follow = FALSE;
	res->u.resok.dir_wcc.after.attributes_follow = FALSE;

	return SUCCESS;

failure:
	res->u.resfail.dir_wcc.before.attributes_follow = FALSE;
	res->u.resfail.dir_wcc.after.attributes_follow = FALSE;

	return SUCCESS;
}

enum accept_stat NFSPROC3_RMDIR(struct diropargs3 *args, struct removeres3 *res, struct server_conn *conn)
{
	char *path;
	int filetype;

	NF(diropargs_to_path(args, &path, &filetype, conn));
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

enum accept_stat NFSPROC3_RENAME(struct renameargs3 *args, struct renameres3 *res, struct server_conn *conn)
{
	char *from;
	char *to;
	int oldfiletype;
	int newfiletype;

	NF(diropargs_to_path(&(args->from), &from, &oldfiletype, conn));
	if (conn->export->ro) NF(NFSERR_ROFS);
	NF(filecache_closecache(from));
	NF(diropargs_to_path(&(args->to), &to, &newfiletype, conn));
	NF(filecache_closecache(to));
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

enum accept_stat NFSPROC3_READDIR(struct readdirargs3 *args, struct readdirres3 *res, struct server_conn *conn)
{
	char *path;
	int cookie = (int)args->cookie;
	int bytes = 0;
	struct entry3 **lastentry = &(res->u.readdirok.entries);

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
			struct entry3 *entry;
			char *leaf = buffer + 20;
			size_t leaflen;
			int filetype;
			unsigned int load = ((unsigned int *)buffer)[0];
			int type;

			if ((load & 0xFFF00000) == 0xFFF00000) {
				filetype = (load & 0x000FFF00) >> 8;
			} else {
				filetype = conn->export->defaultfiletype;
			}
			UF(entry = palloc(sizeof(struct entry3), conn->pool));

			entry->fileid = calc_fileid(path, leaf);
			UF(leaf = filename_unixify(leaf, strlen(leaf), &leaflen, 0, conn->pool));
			type = ((unsigned int *)buffer)[4];
			if (type == 1 || (type == 3 && conn->export->imagefs == 0)) {
				UF(leaf = addfiletypeext(leaf, leaflen, 0, filetype, &leaflen, conn->export->defaultfiletype, conn->export->xyzext, 1, choices.macforks, conn->pool));
			}

			if (choices.toenc != (iconv_t)-1) {
				char *encleaf;
				size_t encleaflen;
				static char buffer2[MAX_PATHNAME];

				encleaf = buffer2;
				encleaflen = sizeof(buffer2);
				if (iconv(choices.toenc, &leaf, &leaflen, &encleaf, &encleaflen) == -1) {
					iconv(choices.toenc, NULL, NULL, NULL, NULL);
					syslogf(LOGNAME, LOG_ERROR, "Error: Iconv failed (%d)", errno);
					if (errno == E2BIG) {
						NF(NFSERR_NAMETOOLONG);
					} else {
						NF(NFSERR_IO);
					}
				}

				encleaflen = sizeof(buffer2) - encleaflen;
				UF(entry->name.data = palloc(encleaflen, conn->pool));
				memcpy(entry->name.data, buffer2, encleaflen);
				entry->name.size = leaflen = encleaflen;
			} else {
				entry->name.data = leaf;
				entry->name.size = leaflen;
			}

			entry->cookie = cookie;

			bytes += 24 + ((leaflen + 3) & ~3);
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

enum accept_stat NFSPROC3_READDIRPLUS(struct readdirplusargs3 *args, struct readdirplusres3 *res, struct server_conn *conn)
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
			size_t leaflen;
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
			leaflen = strlen(leaf);
			if (pathlen + 2 + leaflen > MAX_PATHNAME) NF(NFSERR_NAMETOOLONG);
			memcpy(pathbuffer + pathlen + 1, leaf, leaflen + 1);
			UF(leaf = filename_unixify(leaf, leaflen, &leaflen, 0, conn->pool));
			if (type == OBJ_FILE || (type == OBJ_IMAGE && conn->export->imagefs == 0)) {
				UF(leaf = addfiletypeext(leaf, leaflen, 0, filetype, &leaflen, conn->export->defaultfiletype, conn->export->xyzext, 1, choices.macforks, conn->pool));
			}

			if (choices.toenc != (iconv_t)-1) {
				char *encleaf;
				size_t encleaflen;
				static char buffer2[MAX_PATHNAME];

				encleaf = buffer2;
				encleaflen = sizeof(buffer2);
				if (iconv(choices.toenc, &leaf, &leaflen, &encleaf, &encleaflen) == -1) {
					iconv(choices.toenc, NULL, NULL, NULL, NULL);
					syslogf(LOGNAME, LOG_ERROR, "Error: Iconv failed (%d)", errno);
					if (errno == E2BIG) {
						NF(NFSERR_NAMETOOLONG);
					} else {
						NF(NFSERR_IO);
					}
				}

				encleaflen = sizeof(buffer2) - encleaflen;
				UF(entry->name.data = palloc(encleaflen, conn->pool));
				memcpy(entry->name.data, buffer2, encleaflen);
				entry->name.size = leaflen = encleaflen;
			} else {
				entry->name.data = leaf;
				entry->name.size = leaflen;
			}

			entry->cookie = cookie;
			entry->name_handle.handle_follows = TRUE;
			NF(path_to_nfs3fh(pathbuffer, &(entry->name_handle.u.handle), conn));
			entry->name_attributes.attributes_follow = TRUE;
			parse_fattr(pathbuffer, type, load, exec, len, attr, &(entry->name_attributes.u.attributes), conn);

			bytes += 4 + ((leaflen + 3) & ~3);
			if (bytes > args->count) {
				return SUCCESS;
			}
			tbytes += 168 + ((leaflen + 3) & ~3);
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

enum accept_stat NFSPROC3_STATFS(struct FSSTAT3args *args, struct FSSTAT3res *res, struct server_conn *conn)
{
	char *path;
	unsigned int freelo;
	unsigned int freehi;
	unsigned int sizelo;
	unsigned int sizehi;

	NF(nfs3fh_to_path(&(args->fhandle), &path, conn));
	res->u.resok.obj_attributes.attributes_follow = TRUE;
	NF(get_fattr(path, -1, &(res->u.resok.obj_attributes.u.attributes), NULL, conn));

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
	NF(get_fattr(path, -1, &(res->u.resok.obj_attributes.u.attributes), NULL, conn));
	res->u.resok.rtmax = conn->tcp ? MFMAXDATABUFFER : conn->export->udpsize;
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
	NF(get_fattr(path, -1, &(res->u.resok.obj_attributes.u.attributes), NULL, conn));
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
	NF(nfserr_removenfs4(filecache_commit(path, res->u.resok.verf)));
	NF(nfserr_removenfs4(filecache_getattr(path, &load, &exec, &size, &attr, &cached)));
	if (cached) {
		res->u.resok.file_wcc.after.attributes_follow = TRUE;
		parse_fattr(path, OBJ_FILE, load, exec, size, attr, &(res->u.resok.file_wcc.after.u.attributes), conn);
	} else {
		res->u.resok.file_wcc.after.attributes_follow = FALSE;
	}

	return SUCCESS;

failure:
	res->u.resfail.file_wcc.before.attributes_follow = FALSE;
	res->u.resfail.file_wcc.after.attributes_follow = FALSE;

	return SUCCESS;
}


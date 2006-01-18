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


#include "nfs2-procs.h"

typedef _kernel_oserror os_error;

static void *palloc(size_t size, struct server_conn *conn)
{
	return malloc(size);
}

static enum nstat nfs2fh_to_path(struct nfs_fh *fhandle, char **path, struct server_conn *conn)
{
	*path = malloc(1024);
	if (*path == NULL) return NFSERR_IO;
	sprintf(*path, "RAM::0.$%s", fhandle->data);
	return NFS_OK;
}

static enum nstat path_to_nfs2fh(char *path, struct nfs_fh *fhandle, struct server_conn *conn)
{
	memset(fhandle->data, 0, FHSIZE);
	strcpy(fhandle->data, path + 8);
	return NFS_OK;
}

static int calc_fileid(char *path, char *leaf)
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
		fattr->type = type == 3 ? (conn->image_as_file ? NFREG : NFDIR) :
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

void NFSPROC_NULL(struct server_conn *conn)
{
	printf("NFSPROC_NULL\n");
}

void NFSPROC_LOOKUP(struct diropargs *args, struct diropres *res, struct server_conn *conn)
{
	char *path;
	char *filename;
	size_t len;

	printf("NFSPROC_LOOKUP\n");
	res->status = nfs2fh_to_path(&(args->dir), &path, conn);
	if (res->status != NFS_OK) return;
	len = strlen(path) + args->name.size + 2;
	filename = palloc(len, conn);
	if (filename == NULL) {
		res->status = NFSERR_IO;
		return;
	}
	strcpy(filename, path);
	strcat(filename, ".");
	memcpy(filename + len - args->name.size - 1, args->name.data, args->name.size);
	filename[len - 1] = '\0';
	res->status = path_to_nfs2fh(filename, &(res->u.diropok.file), conn);
	if (res->status != NFS_OK) return;
	res->status = get_fattr(filename, &(res->u.diropok.attributes), conn);
}

void NFSPROC_READDIR(struct readdirargs *args, struct readdirres *res, struct server_conn *conn)
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
			os_error *err;
			err = _swix(OS_GBPB, _INR(0,6) | _OUTR(3,4),9, path, buffer, 1, cookie, sizeof(buffer), 0, &read, &cookie);
			if (err) {
				res->status = NFSERR_IO;
				return;
			}
			if (read > 0) {
				struct entry *entry;
				entry = malloc(sizeof(struct entry));
				if (entry == NULL) {
					res->status = NFSERR_IO;
					return;
				}
				printf("adding %s\n",buffer);

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
}

void NFSPROC_READ(struct readargs *args, struct readres *res, struct server_conn *conn)
{
	printf("NFSPROC_READ\n");
}

void NFSPROC_WRITE(struct writeargs *args, struct attrstat *res, struct server_conn *conn)
{
	printf("NFSPROC_WRITE\n");
}

void NFSPROC_CREATE(struct createargs *args, struct createres *res, struct server_conn *conn)
{
	printf("NFSPROC_CREATE\n");
}

void NFSPROC_RMDIR(struct diropargs *args, struct removeres *res, struct server_conn *conn)
{
	printf("NFSPROC_RMDIR\n");
}

void NFSPROC_REMOVE(struct diropargs *args, struct removeres *res, struct server_conn *conn)
{
	printf("NFSPROC_REMOVE\n");
}

void NFSPROC_RENAME(struct renameargs *args, struct renameres *res, struct server_conn *conn)
{
	printf("NFSPROC_RENAME\n");
}


void NFSPROC_GETATTR(struct getattrargs *args, struct attrstat *res, struct server_conn *conn)
{
	char *path;
	os_error *err;

	printf("NFSPROC_GETATTR\n");
	res->status = nfs2fh_to_path(&(args->fhandle), &path, conn);
	if (res->status != NFS_OK) return;
	res->status = get_fattr(path, &(res->u.attributes), conn);
}

void NFSPROC_MKDIR(struct mkdirargs *args, struct createres *res, struct server_conn *conn)
{
	printf("NFSPROC_MKDIR\n");
}

void NFSPROC_SETATTR(struct sattrargs *args, struct sattrres *res, struct server_conn *conn)
{
	printf("NFSPROC_SETATTR\n");
}

void NFSPROC_STATFS(struct statfsargs *args, struct statfsres *res, struct server_conn *conn)
{
	printf("NFSPROC_STATFS\n");
	/*FIXME - get proper values*/
	res->status = NFS_OK;
	res->u.info.tsize = 4096;
	res->u.info.bsize = 4096;
	res->u.info.blocks = 4096;
	res->u.info.bfree = 4096;
	res->u.info.bavail = 4096;
}

void NFSPROC_READLINK(struct readlinkargs *args, struct readlinkres *res, struct server_conn *conn)
{
	printf("NFSPROC_READLINK\n");
}


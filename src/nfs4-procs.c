/*
	$Id$

	NFSv4 procedures


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

#define NFS4

#include "moonfish.h"
#include "exports.h"
#include "utils.h"
#include "filecache.h"
#include "nfs4-procs.h"
#include "nfs4-decode.h"

static char *currentfh;
static char *savedfh;

enum accept_stat nfs4_decode_proc(int proc, struct server_conn *conn)
{
	if (proc == 0) {
		/* NFSPROC4_NULL */
		return SUCCESS;
	} else if (proc == 1) {
		/* NFSPROC4_COMPOUND */
		struct COMPOUND4args args;
		struct COMPOUND4res res;
		char *initobuf = obuf;

		/*FIXME access control */

		currentfh = "";
		savedfh = "";
		conn->export = conn->exports;

		process_COMPOUND4args(INPUT, args);
		res.status = NFS_OK;
		res.tag = args.tag;
		res.numres = args.numargs;
		process_COMPOUND4res(OUTPUT, res);
		res.numres = 0;
		while (args.numargs-- > 0) {
			int compoundproc;
			nstat stat;

			process_int(INPUT, compoundproc);
			if (args.minorversion == 0) {
				process_int(OUTPUT, compoundproc);
				stat = nfs4_decode(compoundproc, conn);
			} else {
				stat = NFSERR_MINOR_VERS_MISMATCH;
			}
			if ((stat != NFSERR_OP_ILLEGAL) &&
			    (stat != NFSERR_BADXDR) &&
			    (stat != NFSERR_MINOR_VERS_MISMATCH)) {
				res.numres++;
			}
			if (stat != NFS_OK) {
				res.status = stat;
				obuf = initobuf;
				process_COMPOUND4res(OUTPUT, res);
				args.numargs = 0;
			}
		}
		return SUCCESS;

	}

	return PROC_UNAVAIL;

buffer_overflow:
	return GARBAGE_ARGS;
}

nstat NFS4_ACCESS(ACCESS4args *args, ACCESS4res *res, struct server_conn *conn)
{
	(void)conn;

	return NFS_OK;
}

nstat NFS4_SETCLIENTID(SETCLIENTID4args *args, SETCLIENTID4res *res, struct server_conn *conn)
{
	(void)conn;

	res->status = NFS_OK;
	res->u.resok4.clientid = 0;
	memset(res->u.resok4.setclientid_confirm, 0, NFS4_VERIFIER_SIZE);

	return NFS_OK;
}

nstat NFS4_SETCLIENTID_CONFIRM(SETCLIENTID_CONFIRM4args *args, SETCLIENTID_CONFIRM4res *res, struct server_conn *conn)
{
	(void)conn;

	res->status = NFS_OK;

	return NFS_OK;
}

nstat NFS4_RENEW(RENEW4args *args, RENEW4res *res, struct server_conn *conn)
{
	(void)conn;
	(void)args;

	return res->status = NFS_OK;
}

nstat NFS4_PUTROOTFH(PUTROOTFH4res *res, struct server_conn *conn)
{
	(void)conn;

	currentfh = "ADFS::4.$"; /**/

	res->status = NFS_OK;

	return NFS_OK;
}

nstat NFS4_GETFH(GETFH4res *res, struct server_conn *conn)
{
	N4(path_to_fh(currentfh, &(res->u.resok4.object.data), &(res->u.resok4.object.size), conn));

	return NFS_OK;
}

nstat NFS4_PUTFH(PUTFH4args *args, PUTFH4res *res, struct server_conn *conn)
{
	N4(fh_to_path(args->object.data, args->object.size, &currentfh, conn));

	return NFS_OK;
}

nstat NFS4_LOOKUP(LOOKUP4args *args, LOOKUP4res *res, struct server_conn *conn)
{
	int cfhlen = strlen(currentfh);
	char *newcfh;
	int type;

	/*FIXME UTF8-ness and sanity check input */
	if (args->objname.size == 0) return res->status = NFSERR_INVAL;

	U4(newcfh = palloc(cfhlen + args->objname.size + 2, conn->pool));
	memcpy(newcfh, currentfh, cfhlen);
	newcfh[cfhlen++] = '.';
	memcpy(newcfh + cfhlen, args->objname.data, args->objname.size);

	O4(_swix(OS_File, _INR(0,1) | _OUT(0), 17, newcfh, &type));
	if (type == OBJ_NONE) return res->status = NFSERR_NOENT;

	return NFS_OK;
}

#define setattrmask() do { \
  int attr = j + 32 * i; \
  res->u.resok4.obj_attributes.attrmask.data[attr > 31 ? 1 : 0] |= 1 << (attr & 0x1F); \
} while (0)

nstat NFS4_GETATTR(GETATTR4args *args, GETATTR4res *res, struct server_conn *conn)
{
	static char attrdata[1024]; /*FIXME - needs to be palloced */
	char *oldobuf = obuf;
	char *oldobufend = obufend;
	unsigned int type;
	unsigned int load;
	unsigned int exec;
	unsigned int len;
	unsigned int attr;
	int cached;
	unsigned int freelo;
	unsigned int freehi;
	unsigned int sizelo;
	unsigned int sizehi;
	int i;
	int j;

	obuf = attrdata;
	obufend = attrdata + 1024;/*FIXME*/


	N4(filecache_getattr(currentfh, &load, &exec, &len, &attr, &cached));
	if (cached) {
		type = OBJ_FILE;
	} else {
		O4(_swix(OS_File, _INR(0,1) | _OUT(0) | _OUTR(2,5), 17, currentfh, &type, &load, &exec, &len, &attr));
	}
	if (type == OBJ_IMAGE) type = conn->export->imagefs ? OBJ_DIR : OBJ_FILE;
	if (type == OBJ_NONE) {
		return res->status = NFSERR_NOENT;
/*	} else {
		int ftype = (load & 0x000FFF00) >> 8;
		if ((type == OBJ_FILE) && (filetype != -1) && (filetype != ftype)) return NFSERR_NOENT;*/
	}

	if ((args->attr_request.size >= 2) && (args->attr_request.data[1] &
	                                  ((1LL << FATTR4_SPACE_AVAIL) |
	                                   (1LL << FATTR4_SPACE_FREE) |
	                                   (1LL << FATTR4_SPACE_TOTAL) |
	                                   (1LL << FATTR4_SPACE_USED)))) {
		if (_swix(OS_FSControl, _INR(0,1) | _OUTR(0,1) | _OUTR(3,4), 55, currentfh, &freelo, &freehi, &sizelo, &sizehi)) {
			O4(_swix(OS_FSControl, _INR(0,1) | _OUT(0) | _OUT(2), 49, currentfh, &freelo, &sizelo));
			freehi = 0;
			sizehi = 0;
		}
	}

	U4(res->u.resok4.obj_attributes.attrmask.data = palloc(2 * sizeof(unsigned), conn->pool));
	res->u.resok4.obj_attributes.attrmask.size = 2;
	memset(res->u.resok4.obj_attributes.attrmask.data, 0, 2 * sizeof(unsigned));

	for (i = 0; i < args->attr_request.size; i++) {
		for (j = 0; j < 32; j++) {
			if (args->attr_request.data[i] & (1 << j)) {
				bool false = FALSE;
				bool true = TRUE;
				switch (j + 32 * i) {
				/* Mandatory attributes */
				case FATTR4_SUPPORTED_ATTRS: {
					uint32_t list[2];
					bitmap4 bitmap;
					setattrmask();
					list[0] = ((1U << FATTR4_SUPPORTED_ATTRS) | (1U << FATTR4_TYPE) | (1U << FATTR4_FH_EXPIRE_TYPE) | (1U << FATTR4_CHANGE) | (1U << FATTR4_SIZE) | (1U << FATTR4_LINK_SUPPORT) | (1U << FATTR4_SYMLINK_SUPPORT) |
					           (1U << FATTR4_NAMED_ATTR) | (1U << FATTR4_FSID) | (1U << FATTR4_UNIQUE_HANDLES) | (1U << FATTR4_LEASE_TIME) | (1U << FATTR4_RDATTR_ERROR) | (1U << FATTR4_FILEHANDLE) | (1U << FATTR4_ACLSUPPORT) |
					           (1U << FATTR4_CANSETTIME) | (1U << FATTR4_CASE_INSENSITIVE) | (1U << FATTR4_CASE_PRESERVING) | (1U << FATTR4_CHOWN_RESTRICTED) | (1U << FATTR4_HOMOGENEOUS) | (1U << FATTR4_MAXFILESIZE) |
					           (1U << FATTR4_MAXNAME) | (1U << FATTR4_MAXREAD) | (1U << FATTR4_MAXWRITE));
					list[1] = (uint32_t)((/*(1ULL << FATTR4_MIMETYPE) |*/ (1ULL << FATTR4_MODE) | (1ULL << FATTR4_NO_TRUNC) | /*(1ULL << FATTR4_OWNER) | (1ULL << FATTR4_OWNER_GROUP) |*/ (1ULL << FATTR4_SPACE_AVAIL) | (1ULL << FATTR4_SPACE_FREE) |
					                     (1ULL << FATTR4_SPACE_TOTAL) | (1ULL << FATTR4_SPACE_USED) | (1ULL << FATTR4_TIME_DELTA) | (1ULL << FATTR4_TIME_MODIFY)) >> 32);
					bitmap.data = list;
					bitmap.size = 2;
					process_array(OUTPUT, bitmap, uint32_t, 2);
					break;
				}
				case FATTR4_TYPE: {
					nfs_ftype4 ftype = type == OBJ_FILE ? NF4REG : NF4DIR;
					setattrmask();
					process_nfs_ftype4(OUTPUT, ftype);
					break;
				}
				case FATTR4_FH_EXPIRE_TYPE: {
					uint32_t fhtype = FH4_PERSISTENT;
					setattrmask();
					process_uint32(OUTPUT, fhtype);/*FIXME*/
					break;
				}
				case FATTR4_CHANGE: {
					uint64_t change = ((uint64_t)load << 32) | exec;
					setattrmask();
					process_uint64(OUTPUT, change);/*FIXME - fakedirtimes */
					break;
				}
				case FATTR4_SIZE: {
					uint64_t size = (uint64_t)len;
					setattrmask();
					process_uint64(OUTPUT, size);
					break;
				}
				case FATTR4_LINK_SUPPORT:
					setattrmask();
					process_bool(OUTPUT, false);
					break;
				case FATTR4_SYMLINK_SUPPORT:
					setattrmask();
					process_bool(OUTPUT, false);
					break;
				case FATTR4_NAMED_ATTR:
					setattrmask();
					process_bool(OUTPUT, false);
					break;
				case FATTR4_FSID: {
					fsid4 id;
					setattrmask();
					id.major = conn->export->exportnum;
					id.minor = 0;
					process_fsid4(OUTPUT, id);
					break;
				}
				case FATTR4_UNIQUE_HANDLES:
					setattrmask();
					process_bool(OUTPUT, true);
					break;
				case FATTR4_LEASE_TIME: {
					uint32_t lease = 100;
					setattrmask();
					process_uint32(OUTPUT, lease);/*FIXME*/
					break;
				}
				case FATTR4_RDATTR_ERROR:
					setattrmask();
					process_bool(OUTPUT, false); /*FIXME - what is this? */
					break;
				case FATTR4_FILEHANDLE: {
					nfs_fh4 fh = {NULL, NFS4_FHSIZE};
					setattrmask();
					N4(path_to_fh(currentfh, &(fh.data), &(fh.size), conn));
					process_nfs_fh4(OUTPUT, fh);
					break;
				}
				/* Recommended attributes */
				case FATTR4_ACLSUPPORT: {
					uint32_t aclsupport = 0;
					setattrmask();
					process_uint32(OUTPUT, aclsupport);
					break;
				}
				case FATTR4_CANSETTIME:
					setattrmask();
					process_bool(OUTPUT, true);
					break;
				case FATTR4_CASE_INSENSITIVE:
					setattrmask();
					process_bool(OUTPUT, true);
					break;
				case FATTR4_CASE_PRESERVING:
					setattrmask();
					process_bool(OUTPUT, true);
					break;
				case FATTR4_CHOWN_RESTRICTED:
					setattrmask();
					process_bool(OUTPUT, false);
					break;
				case FATTR4_HOMOGENEOUS:
					setattrmask();
					process_bool(OUTPUT, true);
					break;
				case FATTR4_MAXFILESIZE: {
					uint64_t size = 0x7FFFFFFFLL;
					setattrmask();
					process_uint64(OUTPUT, size);
					break;
				}
				case FATTR4_MAXNAME: {
					uint32_t size = MAX_PATHNAME;
					setattrmask();
					process_uint32(OUTPUT, size);
					break;
				}
				case FATTR4_MAXREAD: {
					uint64_t size = conn->tcp ? MAX_DATABUFFER : conn->export->udpsize;
					setattrmask();
					process_uint64(OUTPUT, size);
					break;
				}
				case FATTR4_MAXWRITE: {
					uint64_t size = conn->tcp ? MAX_DATABUFFER : conn->export->udpsize;
					setattrmask();
					process_uint64(OUTPUT, size);
					break;
				}
/*				case FATTR4_MIMETYPE:
					setattrmask();
					break;*/
				case FATTR4_MODE: {
					mode4 mode = 0;
					setattrmask();
					if (attr & 0x01) mode |= MODE4_RUSR;
					if (attr & 0x02) mode |= MODE4_WUSR;
					if (attr & 0x10) mode |= MODE4_RGRP | MODE4_ROTH;
					if (attr & 0x20) mode |= MODE4_WGRP | MODE4_WOTH;
					/* Apply the unumask to force access */
					mode |= conn->export->unumask;
					/* Set executable permissions for directories if they have read permission */
					if (type == OBJ_DIR) mode |= (mode & 0444) >> 2;
					/* Remove bits requested by the umask */
					mode &= ~conn->export->umask;
					process_mode4(OUTPUT, mode);
					break;
				}
				case FATTR4_NO_TRUNC:
					setattrmask();
					process_bool(OUTPUT, true);
					break;
/*				case FATTR4_OWNER:
					setattrmask();
					break;
				case FATTR4_OWNER_GROUP:
					setattrmask();
					break;*/
				case FATTR4_SPACE_AVAIL:
				case FATTR4_SPACE_FREE: {
					uint64_t space = ((uint64_t)freehi << 32) | freelo;
					setattrmask();
					process_uint64(OUTPUT, space);
					break;
				}
				case FATTR4_SPACE_TOTAL: {
					uint64_t space = ((uint64_t)freehi << 32) | freelo;
					setattrmask();
					process_uint64(OUTPUT, space);
					break;
				}
				case FATTR4_SPACE_USED: {
					uint64_t size = (uint64_t)len;
					setattrmask();
					process_uint64(OUTPUT, size);
					break;
				}
				case FATTR4_TIME_DELTA: {
					nfstime4 delta;
					setattrmask();
					delta.seconds = 0;
					delta.nseconds = 10000000;
					process_nfstime4(OUTPUT, delta);
					break;
				}
				case FATTR4_TIME_MODIFY: {
					nfstime4 mtime;
					setattrmask();
					loadexec_to_timeval(load, exec, ((struct ntimeval *)&mtime));
					process_nfstime4(OUTPUT, mtime);
					break;
				}
				}
			}
		}
	}

	res->u.resok4.obj_attributes.attr_vals.data = attrdata;
	res->u.resok4.obj_attributes.attr_vals.size = obuf - attrdata;

	obuf = oldobuf;
	obufend = oldobufend;

	return res->status = NFS_OK;

buffer_overflow:
	return res->status = NFSERR_RESOURCE;
}


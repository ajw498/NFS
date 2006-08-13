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
#include "clientid.h"
#include "state.h"
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

		/*FIXME host access control */

		currentfh = NULL;
		savedfh = "";
		conn->export = conn->exports;
		conn->nfs4 = 1;

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
				char *endobuf = obuf;
				res.status = stat;
				obuf = initobuf;
				process_COMPOUND4res(OUTPUT, res);
				args.numargs = 0;
				obuf = endobuf;
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
	unsigned int type;
	unsigned int load;
	unsigned int exec;
	unsigned int len;
	unsigned int attr;
	int cached;

	res->u.resok4.supported = args->access & 0x1F;
	res->u.resok4.access = args->access & 0x1F;
	if (conn->export->ro || (currentfh == NULL)) res->u.resok4.access &= ACCESS4_READ | ACCESS4_LOOKUP;

	if (currentfh) {
		N4(filecache_getattr(currentfh, &load, &exec, &len, &attr, &cached));
		if (cached) {
			type = OBJ_FILE;
		} else {
			O4(_swix(OS_File, _INR(0,1) | _OUT(0) | _OUTR(2,5), 17, currentfh, &type, &load, &exec, &len, &attr));
			if (type == OBJ_IMAGE) type = conn->export->imagefs ? OBJ_DIR : OBJ_FILE;
		}
		if (type == OBJ_NONE) return res->status = NFSERR_NOENT;

		if (type == OBJ_FILE) {
			if ((attr & 0x1) == 0) res->u.resok4.access &= ~ACCESS4_READ;
			if ((attr & 0x2) == 0) res->u.resok4.access &= ~(ACCESS4_MODIFY | ACCESS4_EXTEND);
		}
	}

	return res->status = NFS_OK;
}

nstat NFS4_SETCLIENTID(SETCLIENTID4args *args, SETCLIENTID4res *res, struct server_conn *conn)
{
	(void)conn;

	N4(clientid_setclientid(args->client.id.data, args->client.id.size, args->client.verifier, &(res->u.resok4.clientid)));
	memset(res->u.resok4.setclientid_confirm, 0, NFS4_VERIFIER_SIZE);
	return res->status = NFS_OK;
}

nstat NFS4_SETCLIENTID_CONFIRM(SETCLIENTID_CONFIRM4args *args, SETCLIENTID_CONFIRM4res *res, struct server_conn *conn)
{
	int zero[2] = {0, 0};

	(void)conn;
	if (memcmp(args->setclientid_confirm, zero, 8) != 0) return NFSERR_STALE_CLIENTID;
	N4(clientid_setclientidconfirm(args->clientid));
	return res->status = NFS_OK;
}

nstat NFS4_RENEW(RENEW4args *args, RENEW4res *res, struct server_conn *conn)
{
	(void)conn;

	N4(clientid_renew(args->clientid));
	return res->status = NFS_OK;
}

nstat NFS4_PUTPUBFH(PUTPUBFH4res *res, struct server_conn *conn)
{
	(void)conn;

	currentfh = NULL;
	conn->export = conn->exports;

	return res->status = NFS_OK;
}

nstat NFS4_PUTROOTFH(PUTROOTFH4res *res, struct server_conn *conn)
{
	(void)conn;

	currentfh = NULL;
	conn->export = conn->exports;

	return res->status = NFS_OK;
}

nstat NFS4_GETFH(GETFH4res *res, struct server_conn *conn)
{
	res->u.resok4.object.data = NULL;
	res->u.resok4.object.size = NFS4_FHSIZE;
	N4(path_to_fh(currentfh, &(res->u.resok4.object.data), &(res->u.resok4.object.size), conn));

	return NFS_OK;
}

nstat NFS4_PUTFH(PUTFH4args *args, PUTFH4res *res, struct server_conn *conn)
{
	N4(fh_to_path(args->object.data, args->object.size, &currentfh, conn));

	return NFS_OK;
}

static nstat lookup_filename(char *base, component4 *leafname, char **res, int *filetype, struct server_conn *conn)
{
	int leaflen;
	int cfhlen;
	static char buffer[FILENAME_MAX];
	char *filename;

	/*FIXME UTF8-ness */
	if (leafname->size == 0) return NFSERR_INVAL;

	leaflen = filename_riscosify(leafname->data, leafname->size, buffer, sizeof(buffer), filetype, conn->export->defaultfiletype, conn->export->xyzext);

	if (base == NULL) {
		/* base should only ever be NULL when called from LOOKUP, so
		   the currentfh is being updated. Make sure conn->export is
		   updated at the same time */
		struct export *export = conn->exports;
		while (export) {
			char *leaf = export->exportname;
			if (leaf && (leaf[0] == '/')) leaf++;
			if (leaf && (strcmp(buffer, leaf) == 0)) {
				*res = export->basedir;
				conn->export = export;
				return NFS_OK;
			}
			export = export->next;
		}
		return NFSERR_NOENT;
	} else {
		cfhlen = strlen(base);

		UR(filename = palloc(cfhlen + leaflen + 2, conn->pool));
		memcpy(filename, base, cfhlen);
		filename[cfhlen++] = '.';
		memcpy(filename + cfhlen, buffer, leaflen);
		filename[cfhlen + leafname->size] = '\0';
		*res = filename;
	}

	return NFS_OK;
}

nstat NFS4_LOOKUP(LOOKUP4args *args, LOOKUP4res *res, struct server_conn *conn)
{
	int type;
	int filetype;
	unsigned load;

	N4(lookup_filename(currentfh, &(args->objname), &currentfh, &filetype, conn));

	O4(_swix(OS_File, _INR(0,1) | _OUT(0) | _OUT(2), 17, currentfh, &type, &load));
	if (type == OBJ_NONE) return res->status = NFSERR_NOENT;
	if ((type != OBJ_DIR) && (filetype != -1) &&
	    ((load & 0xFFFFFF00) != (0xFFF00000 | (filetype << 8)))) return res->status = NFSERR_NOENT;

	return res->status = NFS_OK;
}

nstat NFS4_LOOKUPP(LOOKUPP4res *res, struct server_conn *conn)
{
	int type;
	char *lastdot;

	if (currentfh == NULL) return res->status = NFSERR_NOENT;

	O4(_swix(OS_File, _INR(0,1) | _OUT(0), 17, currentfh, &type));
	if ((type != OBJ_DIR) && (type != OBJ_IMAGE)) return res->status = NFSERR_NOTDIR;

	lastdot = strrchr(currentfh, '.');
	if ((lastdot == NULL) || (lastdot - currentfh < conn->export->basedirlen)) {
		currentfh = NULL;
		conn->export = conn->exports;
	} else {
		*lastdot = '\0';
	}

	return res->status = NFS_OK;
}

#define setattrmask() do { \
  int attr = j + 32 * i; \
  res->attrmask.data[attr > 31 ? 1 : 0] |= 1 << (attr & 0x1F); \
} while (0)

static uint64_t changeid = 0;

static nstat get_fattr(char *path, unsigned type, unsigned load, unsigned exec, unsigned len, unsigned attr, bitmap4 *args, fattr4 *res, struct server_conn *conn)
{
	char *attrdata;
	char *oldobuf = obuf;
	char *oldobufend = obufend;
	unsigned int freelo;
	unsigned int freehi;
	unsigned int sizelo;
	unsigned int sizehi;
	int i;
	int j;
	int filetype;

	UR(attrdata = palloc(1024, conn->pool));
	obuf = attrdata;
	obufend = attrdata + 1024;/*FIXME*/

	filetype = (load & 0xFFF00000) ? ((load & 0x000FFF00) >> 8) : 0xFFD;

	if (type == OBJ_IMAGE) type = conn->export->imagefs ? OBJ_DIR : OBJ_FILE;
	if (type == OBJ_NONE) {
		return NFSERR_NOENT;
/*	} else {
		int ftype = (load & 0x000FFF00) >> 8;
		if ((type == OBJ_FILE) && (filetype != -1) && (filetype != ftype)) return NFSERR_NOENT;*/
	}

	if ((args->size >= 2) && (args->data[1] &
	                          ((1LL << FATTR4_SPACE_AVAIL) |
	                           (1LL << FATTR4_SPACE_FREE) |
	                           (1LL << FATTR4_SPACE_TOTAL) |
	                           (1LL << FATTR4_SPACE_USED)))) {
		if (path == NULL) {
			freelo = freehi = 0;
			sizelo = sizehi = 0;
		} else {
			if (_swix(OS_FSControl, _INR(0,1) | _OUTR(0,1) | _OUTR(3,4), 55, path, &freelo, &freehi, &sizelo, &sizehi)) {
				OR(_swix(OS_FSControl, _INR(0,1) | _OUT(0) | _OUT(2), 49, path, &freelo, &sizelo));
				freehi = 0;
				sizehi = 0;
			}
		}
	}

	UR(res->attrmask.data = palloc(2 * sizeof(unsigned), conn->pool));
	res->attrmask.size = 2;
	memset(res->attrmask.data, 0, 2 * sizeof(unsigned));

	for (i = 0; i < args->size; i++) {
		for (j = 0; j < 32; j++) {
			if (args->data[i] & (1 << j)) {
				bool false = FALSE;
				bool true = TRUE;
				switch (j + 32 * i) {
				/* Mandatory attributes */
				case FATTR4_SUPPORTED_ATTRS: {
					uint32_t list[2];
					bitmap4 bitmap;
					setattrmask();
					list[0] = ((1U << FATTR4_SUPPORTED_ATTRS) | (1U << FATTR4_TYPE) | (1U << FATTR4_FH_EXPIRE_TYPE) | (1U << FATTR4_CHANGE) | (1U << FATTR4_SIZE) | (1U << FATTR4_LINK_SUPPORT) | (1U << FATTR4_SYMLINK_SUPPORT) |
					           (1U << FATTR4_NAMED_ATTR) | (1U << FATTR4_FSID) | (1U << FATTR4_UNIQUE_HANDLES) | (1U << FATTR4_LEASE_TIME) | (1U << FATTR4_FILEHANDLE) | (1U << FATTR4_ACLSUPPORT) |
					           (1U << FATTR4_CANSETTIME) | (1U << FATTR4_CASE_INSENSITIVE) | (1U << FATTR4_CASE_PRESERVING) | (1U << FATTR4_CHOWN_RESTRICTED) | (1U << FATTR4_FILEID) | (1U << FATTR4_HOMOGENEOUS) | (1U << FATTR4_MAXFILESIZE) |
					           (1U << FATTR4_MAXNAME) | (1U << FATTR4_MAXREAD) | (1U << FATTR4_MAXWRITE));
					list[1] = (uint32_t)(((1ULL << FATTR4_MIMETYPE) | (1ULL << FATTR4_MODE) | (1ULL << FATTR4_NO_TRUNC) | (1ULL << FATTR4_OWNER) | (1ULL << FATTR4_OWNER_GROUP) | (1ULL << FATTR4_SPACE_AVAIL) | (1ULL << FATTR4_SPACE_FREE) |
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
					uint64_t change = (((uint64_t)load & 0x000FFFFFULL) << 32) | exec;
					setattrmask();
					process_uint64(OUTPUT, change);/*FIXME - fakedirtimes */
					break;
				}
				case FATTR4_SIZE: {
					uint64_t size = (uint64_t)len;
					if (type == OBJ_DIR) break;
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
					uint32_t lease = LEASE_TIMEOUT;
					setattrmask();
					process_uint32(OUTPUT, lease);
					break;
				}
				case FATTR4_FILEHANDLE: {
					nfs_fh4 fh = {NULL, NFS4_FHSIZE};
					setattrmask();
					NR(path_to_fh(path, &(fh.data), &(fh.size), conn));
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
				case FATTR4_FILEID: {
					/* Strictly we shouldn't be providing
					   this, as it cannot be guaranteed
					   unique. However the Linux client
					   seems to forget files exist in
					   directory listings if this is
					   missing. */
					uint64_t fileid = calc_fileid(path, NULL);
					setattrmask();
					process_uint64(OUTPUT, fileid);
					break;
				}
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
				case FATTR4_MIMETYPE: {
					fattr4_mimetype mimetype;
					setattrmask();
					UR(mimetype.data = filetype_to_mimetype(filetype, conn->pool));
					mimetype.size = strlen(mimetype.data);
					process_array(OUTPUT, mimetype, char, mimetype.size);
					break;
				}
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
				case FATTR4_OWNER: {
					char str[10];
					fattr4_owner owner;
					setattrmask();
					snprintf(str, sizeof(str), "%d", conn->uid);
					owner.data = str;
					owner.size = strlen(str);
					process_array(OUTPUT, owner, char, owner.size);
					break;
				}
				case FATTR4_OWNER_GROUP: {
					char str[10];
					fattr4_owner_group group;
					setattrmask();
					snprintf(str, sizeof(str), "%d", conn->gid);
					group.data = str;
					group.size = strlen(str);
					process_array(OUTPUT, group, char, group.size);
					break;
				}
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
					if (type == OBJ_DIR) break;
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

	res->attr_vals.data = attrdata;
	res->attr_vals.size = obuf - attrdata;

	obuf = oldobuf;
	obufend = oldobufend;

	return NFS_OK;

buffer_overflow:
	return NFSERR_RESOURCE;
}


#define setrootattrs() do { \
	type = OBJ_DIR; \
	load = 0xFFFFFD00; \
	exec = 0x00000000; \
	len  = 0; \
	attr = 0; \
} while (0)

static nstat verify_fattr(char *path, int same, fattr4 *args, struct server_conn *conn)
{
	char *oldibuf = ibuf;
	char *oldibufend = ibufend;
	int differ = 0;
	int i;
	int j;
	int filetype;
	unsigned load;
	unsigned exec;
	unsigned type;
	unsigned attr;
	unsigned len;

	ibuf = args->attr_vals.data;
	ibufend = ibuf + args->attr_vals.size; /* FIXME - restore on error */

	if (path == NULL) {
		setrootattrs();
	} else {
		OR(_swix(OS_File, _INR(0,1) | _OUT(0) | _OUTR(2,5), 17, path, &type, &load, &exec, &len, &attr));
	}
	if (type == OBJ_NONE) return NFSERR_NOENT;

	if (type == OBJ_IMAGE) type = conn->export->imagefs ? OBJ_DIR : OBJ_FILE;

	filetype = (load & 0xFFF00000) ? ((load & 0x000FFF00) >> 8) : 0xFFD;

	for (i = 0; i < args->attrmask.size; i++) {
		for (j = 0; j < 32; j++) {
			if (args->attrmask.data[i] & (1 << j)) {
				switch (j + 32 * i) {
				/* Mandatory attributes */
				case FATTR4_TYPE: {
					nfs_ftype4 ftype;
					process_nfs_ftype4(INPUT, ftype);
					differ |= ftype != (type == OBJ_FILE ? NF4REG : NF4DIR);
					break;
				}
				case FATTR4_CHANGE: {
					uint64_t change;
					process_uint64(INPUT, change);/*FIXME - fakedirtimes? */
					differ |= change != ((((uint64_t)load & 0x000FFFFFULL) << 32) | exec);
					break;
				}
				case FATTR4_SPACE_USED:
				case FATTR4_SIZE: {
					uint64_t size;
					process_uint64(INPUT, size);
					differ |= (uint64_t)len != size;
					break;
				}
				case FATTR4_NAMED_ATTR: {
					bool attr;
					process_bool(INPUT, attr);
					differ |= attr != FALSE;
					break;
				}
				case FATTR4_FSID: {
					fsid4 id;
					process_fsid4(INPUT, id);
					differ |= id.major != conn->export->exportnum;
					differ |= id.minor != 0;
					break;
				}
				case FATTR4_FILEHANDLE: {
					nfs_fh4 fh = {NULL, NFS4_FHSIZE};
					nfs_fh4 fh2 = {NULL, NFS4_FHSIZE};
					NR(path_to_fh(path, &(fh.data), &(fh.size), conn));
					process_nfs_fh4(INPUT, fh2);
					if (fh.size == fh2.size) {
						differ |= memcmp(fh.data, fh2.data, fh.size);
					} else {
						differ |= 1;
					}
					break;
				}
				/* Recommended attributes */
				case FATTR4_MIMETYPE: {
					fattr4_mimetype mimetype;
					char *mimetype2 = filetype_to_mimetype(filetype, conn->pool);
					process_array(INPUT, mimetype, char, mimetype.size);
					if (mimetype.size == strlen(mimetype2)) {
						differ |= memcmp(mimetype2, mimetype.data, mimetype.size);
					} else {
						differ |= 1;
					}
					break;
				}
				case FATTR4_MODE: {
					mode4 mode = 0;
					mode4 mode2 = 0;
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
					process_mode4(INPUT, mode2);
					differ |= mode != mode2;
					break;
				}
				case FATTR4_OWNER: {
					char str[10];
					fattr4_owner owner;
					snprintf(str, sizeof(str), "%d", conn->uid);
					process_array(INPUT, owner, char, owner.size);
					if (owner.size == strlen(str)) {
						differ |= memcmp(str, owner.data, owner.size);
					} else {
						differ |= 1;
					}
					break;
				}
				case FATTR4_OWNER_GROUP: {
					char str[10];
					fattr4_owner_group group;
					snprintf(str, sizeof(str), "%d", conn->gid);
					process_array(INPUT, group, char, group.size);
					if (group.size == strlen(str)) {
						differ |= memcmp(str, group.data, group.size);
					} else {
						differ |= 1;
					}
					break;
				}
				case FATTR4_TIME_MODIFY: {
					nfstime4 mtime;
					nfstime4 mtime2;
					loadexec_to_timeval(load, exec, ((struct ntimeval *)&mtime));
					process_nfstime4(INPUT, mtime2);
					differ |= mtime.seconds != mtime2.seconds;
					differ |= mtime.nseconds != mtime2.nseconds;
					break;
				}
				default:
					/*FIXME: NFS4ERR_INVAL for write only attrs */
					return NFSERR_ATTRNOTSUPP;
				}
			}
		}
	}

	ibuf = oldibuf;
	ibufend = oldibufend;

	if (same && differ) return NFSERR_NOT_SAME;
	if (!same && !differ) return NFSERR_SAME;
	return NFS_OK;

buffer_overflow:
	return NFSERR_RESOURCE;
}

#define setfattrmask() do { \
  int attr2 = j + 32 * i; \
  if (res) res->data[attr2 > 31 ? 1 : 0] |= 1 << (attr2 & 0x1F); \
} while (0)

/*FIXME*/
static void foo(struct server_conn *conn)
{
					fattr4_acl acltmp;
					process_array2(INPUT, acltmp, nfsace4, OPAQUE_MAX);
buffer_overflow:
	return;
}

static nstat set_fattr(char *path, struct stateid *stateid, fattr4 *args, bitmap4 *res, int sizeonly, int filetype, struct server_conn *conn)
{
	char *oldibuf = ibuf;
	char *oldibufend = ibufend;
	int i;
	int j;
	unsigned int load;
	unsigned int exec;
	int type;
	int attr;
	int size;
	int setsize = 0;

	ibuf = args->attr_vals.data;
	ibufend = ibuf + args->attr_vals.size; /* FIXME - restore on error */

	if (path == NULL) return NFSERR_ROFS;

	OR(_swix(OS_File, _INR(0,1) | _OUT(0) | _OUTR(2,5), 17, path, &type, &load, &exec, &size, &attr));
	if (type == OBJ_NONE) return NFSERR_NOENT;
	filetype = (load & 0x000FFF00) >> 8;

	for (i = 0; i < args->attrmask.size; i++) {
		for (j = 0; j < 32; j++) {
			if (args->attrmask.data[i] & (1 << j)) {
				switch (j + 32 * i) {
				/* Mandatory attributes */
				case FATTR4_SIZE: {
					uint64_t fsize;
					if (type == OBJ_DIR) break;
					setfattrmask();
					process_uint64(INPUT, fsize);
					if (fsize > 0x7FFFFFFFULL) return NFSERR_FBIG;
					setsize = 1;
					size = (int)fsize;
					break;
				}
				/* Recommended attributes */
				case FATTR4_ACL: {
					foo(conn);
					break;
				}
				case FATTR4_ARCHIVE:
				case FATTR4_HIDDEN:
				case FATTR4_SYSTEM: {
					bool flagtmp;
					process_bool(INPUT, flagtmp);
					break;
				}
				case FATTR4_MIMETYPE:
				case FATTR4_OWNER:
				case FATTR4_OWNER_GROUP: {
					fattr4_owner_group grptmp;
					process_array2(INPUT, grptmp, char, OPAQUE_MAX);
					break;
				}
				case FATTR4_TIME_ACCESS_SET: {
					settime4 timetmp;
					process_settime4(INPUT, timetmp);
					break;
				}
				case FATTR4_TIME_BACKUP: {
					settime4 timetmp;
					process_settime4(INPUT, timetmp);
					break;
				}
				case FATTR4_MODE: {
					mode4 mode;
					setfattrmask();
					process_mode4(INPUT, mode);
					if (!sizeonly) attr = mode_to_attr(mode);
					break;
				}
				case FATTR4_TIME_CREATE: {
					nfstime4 mtime;
					setfattrmask();
					process_nfstime4(INPUT, mtime);
					if (!sizeonly) timeval_to_loadexec((struct ntimeval *)&mtime, filetype, &load, &exec);
					break;
				}
				case FATTR4_TIME_MODIFY_SET: {
					settime4 settime;
					setfattrmask();
					process_settime4(INPUT, settime);
					if (!sizeonly) {
						if (settime.set_it == SET_TO_CLIENT_TIME4) {
							timeval_to_loadexec((struct ntimeval *)&(settime.u.time), filetype, &load, &exec);
						} else {
							unsigned int block[2];
							block[0] = 3;
							OR(_swix(OS_Word, _INR(0, 1), 14, block));
							exec = block[0];
							load = (load & 0xFFFFFF00) | (block[1] & 0xFF);
						}
					}
					break;
				}
				}
			}
		}
	}

	ibuf = oldibuf;
	ibufend = oldibufend;

	if (type == OBJ_FILE) {
		/* Write through the cache to ensure consistency */
		NR(filecache_setattr(path, stateid, load, exec, attr, size, setsize));
	}
	OR(_swix(OS_File, _INR(0,3) | _IN(5), 1, path, load, exec, attr));

	return NFS_OK;

buffer_overflow:
	return NFSERR_RESOURCE;
}

nstat NFS4_GETATTR(GETATTR4args *args, GETATTR4res *res, struct server_conn *conn)
{
	unsigned int type;
	unsigned int load;
	unsigned int exec;
	unsigned int len;
	unsigned int attr;

	if (currentfh == NULL) {
		setrootattrs();
	} else {
		int cached;

		N4(filecache_getattr(currentfh, &load, &exec, &len, &attr, &cached));
		if (cached) {
			type = OBJ_FILE;
		} else {
			O4(_swix(OS_File, _INR(0,1) | _OUT(0) | _OUTR(2,5), 17, currentfh, &type, &load, &exec, &len, &attr));
		}
	}
	if (type == OBJ_IMAGE) type = conn->export->imagefs ? OBJ_DIR : OBJ_FILE;
	if (type == OBJ_NONE) {
		return res->status = NFSERR_NOENT;
/*	} else { FIXME?
		int ftype = (load & 0x000FFF00) >> 8;
		if ((type == OBJ_FILE) && (filetype != -1) && (filetype != ftype)) return NFSERR_NOENT;*/
	}

	N4(get_fattr(currentfh, type, load, exec, len, attr, &(args->attr_request), &(res->u.resok4.obj_attributes), conn));

	return res->status = NFS_OK;
}

nstat NFS4_READDIR(READDIR4args *args, READDIR4res *res, struct server_conn *conn)
{
	int cookie = (int)(args->cookie >> 2);
	int bytes = 0;
	int tbytes = 0;
	int pathlen = 0;
	char pathbuffer[MAX_PATHNAME];
	struct entry4 **lastentry = &(res->u.resok4.reply.entries);
	struct export *export = conn->exports;

	if (currentfh) {
		pathlen = strlen(currentfh);
		if (pathlen + 2 > MAX_PATHNAME) N4(NFSERR_NAMETOOLONG);
		memcpy(pathbuffer, currentfh, pathlen);
		pathbuffer[pathlen] = '.';
	} else {
		int count = 0;
		while (export && (count <= cookie)) {
			export = export->next;
			count++;
		}
	}

	res->u.resok4.reply.entries = NULL;
	res->u.resok4.reply.eof = FALSE;
	memset(res->u.resok4.cookieverf, 0, NFS4_VERIFIER_SIZE);

	while (cookie != -1) {
		int read;
		char buffer[MAX_PATHNAME + 20];

		/* We have to read one entry at a time, which is less
		   efficient than reading several, as we need to return
		   a cookie for every entry. */
		if (currentfh) {
			O4(_swix(OS_GBPB, _INR(0,6) | _OUTR(3,4), 10, currentfh, buffer, 1, cookie, sizeof(buffer), 0, &read, &cookie));
		} else {
			if (export == NULL) {
				cookie = -1;
				read = 0;
			} else {
				read = 1;
			}
		}
		if (read > 0) {
			struct entry4 *entry;
			char *leaf;
			unsigned leaflen;
			int filetype;
			unsigned load;
			unsigned exec;
			unsigned len;
			unsigned attr;
			unsigned type;

			if (currentfh) {
				load = ((unsigned int *)buffer)[0];
				exec = ((unsigned int *)buffer)[1];
				len  = ((unsigned int *)buffer)[2];
				attr = ((unsigned int *)buffer)[3];
				type = ((unsigned int *)buffer)[4];
				leaf = buffer + 20;
			} else {
				setrootattrs();
				leaf = export->exportname;
				if (leaf[0] == '/') leaf++;
				strcpy(pathbuffer, export->basedir);
				pathlen = export->basedirlen;
			}

			if ((load & 0xFFF00000) == 0xFFF00000) {
				filetype = (load & 0x000FFF00) >> 8;
			} else {
				filetype = conn->export->defaultfiletype;
			}
			U4(entry = palloc(sizeof(struct entry4), conn->pool));

			leaflen = strlen(leaf);
			if (currentfh) {
				if (pathlen + 2 + leaflen > MAX_PATHNAME) N4(NFSERR_NAMETOOLONG);
				memcpy(pathbuffer + pathlen + 1, leaf, leaflen + 1);
			}
			U4(leaf = filename_unixify(leaf, leaflen, &leaflen, conn->pool));
			if (type == OBJ_FILE || (type == OBJ_IMAGE && conn->export->imagefs == 0)) {
				U4(leaf = addfiletypeext(leaf, leaflen, 0, filetype, &leaflen, conn->export->defaultfiletype, conn->export->xyzext, conn->pool));
			}

			entry->name.data = leaf; /*FIXME - UTF8*/
			entry->name.size = leaflen;
			entry->cookie = ((uint64_t)cookie) << 2;
			N4(get_fattr(pathbuffer, type, load, exec, len, attr, &(args->attr_request), &(entry->attrs), conn));

			bytes += 4 + ((leaflen + 3) & ~3);
			if (bytes > args->dircount) {
				return res->status = NFS_OK;
			}
			tbytes += 168 + ((leaflen + 3) & ~3);
			if (tbytes > args->maxcount) {
				return res->status = NFS_OK;
			}
			entry->next = NULL;
			*lastentry = entry;
			lastentry = &(entry->next);
			if (currentfh == NULL) export = export->next;
		}
		if (cookie == -1) res->u.resok4.reply.eof = TRUE;
	}

	return res->status = NFS_OK;
}

nstat NFS4_RESTOREFH(RESTOREFH4res *res, struct server_conn *conn)
{
	(void)conn;

	if (savedfh[0] == '\0') return res->status = NFSERR_RESTOREFH;

	currentfh = savedfh;
	if (currentfh == NULL) conn->export = conn->exports;

	return res->status = NFS_OK;
}

nstat NFS4_SAVEFH(SAVEFH4res *res, struct server_conn *conn)
{
	(void)conn;

	savedfh = currentfh;

	return res->status = NFS_OK;
}

nstat NFS4_OPEN(OPEN4args *args, OPEN4res *res, struct server_conn *conn)
{
	int filetype = 0xFFF;/*FIXME*/
	int verf;
	char other[12];
	unsigned ownerseqid;
	int duplicate;
	int confirmrequired;
	struct open_owner *open_owner;

	if (currentfh == NULL) N4(NFSERR_ROFS);

	if ((args->share_access == 0) ||
	    (args->share_access & ~OPEN4_SHARE_ACCESS_BOTH) ||
	    (args->share_deny   & ~OPEN4_SHARE_ACCESS_BOTH)) N4(NFSERR_INVAL);

	if (graceperiod && (args->claim.claim != CLAIM_PREVIOUS)) N4(NFSERR_GRACE);

	NR(state_newopenowner(args->owner.clientid, args->owner.owner.data, args->owner.owner.size, args->seqid, &open_owner, &confirmrequired, &duplicate));

	if (duplicate) {
		res->status = open_owner->duplicate.open.status;
		res->u.resok4.stateid.seqid = open_owner->duplicate.open.stateidseq;
		memcpy(res->u.resok4.stateid.other, open_owner->duplicate.open.stateidother, 12);
		res->u.resok4.rflags = open_owner->duplicate.open.rflags;
		res->u.resok4.attrset.data = open_owner->duplicate.open.attrset;
		res->u.resok4.attrset.size = 2;
		res->u.resok4.cinfo.atomic = TRUE;
		res->u.resok4.cinfo.before = changeid;
		changeid++;
		res->u.resok4.cinfo.after = changeid;
		changeid++;
		res->u.resok4.delegation.delegation_type = OPEN_DELEGATE_NONE;
		return res->status;
	}

	switch (args->claim.claim) {
	case CLAIM_NULL:
		NF(lookup_filename(currentfh, &(args->claim.u.file), &currentfh, &filetype, conn));
		break;
	case CLAIM_PREVIOUS:
		if (args->claim.u.delegate_type != OPEN_DELEGATE_NONE) NF(NFSERR_RECLAIM_BAD);
		break;
	default:
		NF(NFSERR_RECLAIM_BAD);
	}

	UF(res->u.resok4.attrset.data = palloc(2 * sizeof(unsigned), conn->pool));
	res->u.resok4.attrset.size = 2;
	memset(res->u.resok4.attrset.data, 0, 2 * sizeof(unsigned));

	res->u.resok4.rflags = OPEN4_RESULT_LOCKTYPE_POSIX;
	res->u.resok4.cinfo.atomic = TRUE;
	res->u.resok4.cinfo.before = changeid;
	changeid++;
	res->u.resok4.cinfo.after = changeid;
	changeid++;
	res->u.resok4.delegation.delegation_type = OPEN_DELEGATE_NONE;


	if (args->openhow.opentype == OPEN4_CREATE) {
		/* Create the file */
		int type;
		unsigned exec;
		if (conn->export->ro) NF(NFSERR_ROFS);

		OF(_swix(OS_File, _INR(0,1) | _OUT(0) | _OUT(3), 17, currentfh, &type, &exec));

		switch (args->openhow.u.how.mode) {
		case GUARDED4:
			if (type != OBJ_NONE) NF(NFSERR_EXIST);
			/* Fallthrough */
		case UNCHECKED4:
			if (type == OBJ_NONE) {
				OF(_swix(OS_File, _INR(0,5), 11, currentfh, filetype, 0, 0, 0));
				NF(set_fattr(currentfh, STATEID_NONE, &(args->openhow.u.how.u.createattrs), &(res->u.resok4.attrset), 1, filetype, conn));
			} else {
				NF(set_fattr(currentfh, STATEID_NONE, &(args->openhow.u.how.u.createattrs), &(res->u.resok4.attrset), 0, filetype, conn));
			}
			break;
		case EXCLUSIVE4:
			verf  = ((int *)args->openhow.u.how.u.createverf)[0];
			verf ^= ((int *)args->openhow.u.how.u.createverf)[1];
			if (type != OBJ_NONE) {
				if (exec != verf) NF(NFSERR_EXIST);
			} else {
				OF(_swix(OS_File, _INR(0,5), 11, currentfh, filetype, 0, 0, 0));
				OF(_swix(OS_File, _INR(0,1) | _IN(3), 3, currentfh, verf));
			}
			res->u.resok4.attrset.data[1] |= 1 << (FATTR4_TIME_MODIFY & 0x1F);
		}

	}

	NF(filecache_open(currentfh, open_owner, args->share_access, args->share_deny, &ownerseqid, other));
	if (confirmrequired) res->u.resok4.rflags |= OPEN4_RESULT_CONFIRM;
	res->u.resok4.stateid.seqid = ownerseqid;
	memcpy(res->u.resok4.stateid.other, other, 12);

	open_owner->duplicate.open.stateidseq = res->u.resok4.stateid.seqid;
	memcpy(open_owner->duplicate.open.stateidother, res->u.resok4.stateid.other, 12);
	open_owner->duplicate.open.rflags = res->u.resok4.rflags;
	memcpy(open_owner->duplicate.open.attrset, res->u.resok4.attrset.data, 8);
	return open_owner->duplicate.open.status = res->status = NFS_OK;

failure:
	return open_owner->duplicate.open.status = res->status;
}

nstat NFS4_OPEN_CONFIRM(OPEN_CONFIRM4args *args, OPEN_CONFIRM4res *res, struct server_conn *conn)
{
	struct stateid *stateid;
	int duplicate;

	N4(state_getstateid(args->open_stateid.seqid, args->open_stateid.other, &stateid, conn));
	N4(state_checkopenseqid(stateid, args->seqid, 1, &duplicate));

	/* It isn't possible for the confirm to fail other than for bad
	   stateid or bad seqid, therefore we do not need to save the
	   response in case of a duplicate request */
	res->u.resok4.open_stateid = args->open_stateid;
	res->u.resok4.open_stateid.seqid++;
	return res->status = NFS_OK;
}

nstat NFS4_OPEN_DOWNGRADE(OPEN_DOWNGRADE4args *args, OPEN_DOWNGRADE4res *res, struct server_conn *conn)
{
	struct stateid *stateid;
	int duplicate;
	unsigned ownerseqid;

	N4(state_getstateid(args->open_stateid.seqid, args->open_stateid.other, &stateid, conn));
	N4(state_checkopenseqid(stateid, args->seqid, 0, &duplicate));
	if (duplicate) {
		res->status = stateid->open->open_owner->duplicate.open_downgrade.status;
	} else {
		res->status = stateid->open->open_owner->duplicate.open_downgrade.status = filecache_opendowngrade(currentfh, stateid, args->share_access, args->share_deny, &ownerseqid);
	}

	res->u.resok4.open_stateid = args->open_stateid;
	res->u.resok4.open_stateid.seqid = ownerseqid;

	return res->status;
}

nstat NFS4_CLOSE(CLOSE4args *args, CLOSE4res *res, struct server_conn *conn)
{
	struct stateid *stateid;
	int duplicate;

	N4(state_getstateid(args->open_stateid.seqid, args->open_stateid.other, &stateid, conn));
	N4(state_checkopenseqid(stateid, args->seqid, 0, &duplicate));
	if (duplicate) {
		res->status = stateid->open->open_owner->duplicate.close.status;
	} else {
		res->status = stateid->open->open_owner->duplicate.close.status = filecache_close(currentfh, stateid);
	}

	res->u.open_stateid = args->open_stateid;
	res->u.open_stateid.seqid++;

	return res->status;
}


nstat NFS4_READ(READ4args *args, READ4res *res, struct server_conn *conn)
{
	unsigned int read;
	int eof;
	char *data;
	struct stateid *stateid;

	if (currentfh == NULL) N4(NFSERR_ISDIR);

	if (args->offset > 0x7FFFFFFFULL) N4(NFSERR_FBIG);

	N4(state_getstateid(args->stateid.seqid, args->stateid.other, &stateid, conn));
	N4(filecache_read(currentfh, stateid, args->count, (unsigned int)args->offset, &data, &read, &eof));
	res->u.resok4.data.data = data;
	res->u.resok4.data.size = read;
	res->u.resok4.eof = eof ? TRUE : FALSE;

	return res->status = NFS_OK;
}

nstat NFS4_WRITE(WRITE4args *args, WRITE4res *res, struct server_conn *conn)
{
	int sync = args->stable != UNSTABLE4;
	struct stateid *stateid;

	if (currentfh == NULL) N4(NFSERR_ISDIR);

	if (conn->export->ro) N4(NFSERR_ROFS);

	if (args->offset > 0x7FFFFFFFULL) N4(NFSERR_FBIG);
	if (args->offset + args->data.size > 0x7FFFFFFFULL) N4(NFSERR_FBIG);

	N4(state_getstateid(args->stateid.seqid, args->stateid.other, &stateid, conn));
	N4(filecache_write(currentfh, stateid, args->data.size, (unsigned int)args->offset, args->data.data, sync, res->u.resok4.writeverf));

	res->u.resok4.count = args->data.size;
	res->u.resok4.committed = sync ? FILE_SYNC4 : UNSTABLE4;

	return res->status = NFS_OK;
}

nstat NFS4_COMMIT(COMMIT4args *args, COMMIT4res *res, struct server_conn *conn)
{
	(void)args;
	(void)conn;

	if (currentfh == NULL) N4(NFSERR_ISDIR);

	N4(filecache_commit(currentfh, res->u.resok4.writeverf));

	return res->status = NFS_OK;
}

nstat NFS4_CREATE(CREATE4args *args, CREATE4res *res, struct server_conn *conn)
{
	int filetype;
	int type;

	if (currentfh == NULL) N4(NFSERR_ROFS);

	N4(lookup_filename(currentfh, &(args->objname), &currentfh, &filetype, conn));

	O4(_swix(OS_File, _INR(0,1) | _OUT(0), 17, currentfh, &type));
	if (type != OBJ_NONE) N4(NFSERR_EXIST);

	if (args->objtype.type != NF4DIR) return res->status = NFSERR_NOTSUPP;

	O4(_swix(OS_File, _INR(0,1) | _IN(4), 8, currentfh, 0));

	U4(res->u.resok4.attrset.data = palloc(2 * sizeof(unsigned), conn->pool));
	res->u.resok4.attrset.size = 2;
	memset(res->u.resok4.attrset.data, 0, 2 * sizeof(unsigned));
	N4(set_fattr(currentfh, STATEID_NONE, &(args->createattrs), &(res->u.resok4.attrset), 0, filetype, conn));

	res->u.resok4.cinfo.atomic = TRUE;
	res->u.resok4.cinfo.before = changeid;
	changeid++;
	res->u.resok4.cinfo.after = changeid;
	changeid++;

	return res->status = NFS_OK;
}

nstat NFS4_DELEGPURGE(DELEGPURGE4args *args, DELEGPURGE4res *res, struct server_conn *conn)
{
	(void)args;
	(void)res;
	(void)conn;

	return res->status = NFSERR_NOTSUPP;
}

nstat NFS4_DELEGRETURN(DELEGRETURN4args *args, DELEGRETURN4res *res, struct server_conn *conn)
{
	(void)args;
	(void)res;
	(void)conn;

	return res->status = NFSERR_NOTSUPP;
}

nstat NFS4_LINK(LINK4args *args, LINK4res *res, struct server_conn *conn)
{
	(void)args;
	(void)res;
	(void)conn;

	return res->status = NFSERR_NOTSUPP;
}

nstat NFS4_OPENATTR(OPENATTR4args *args, OPENATTR4res *res, struct server_conn *conn)
{
	(void)args;
	(void)res;
	(void)conn;

	return res->status = NFSERR_NOTSUPP;
}

nstat NFS4_READLINK(READLINK4res *res, struct server_conn *conn)
{
	(void)res;
	(void)conn;

	return res->status = NFSERR_NOTSUPP;
}

nstat NFS4_REMOVE(REMOVE4args *args, REMOVE4res *res, struct server_conn *conn)
{
	char *path;

	if (currentfh == NULL) N4(NFSERR_ROFS);
	/* FIXME don't delete if open with share deny */
	if (conn->export->ro) N4(NFSERR_ROFS);

	N4(lookup_filename(currentfh, &(args->target), &path, NULL, conn));
	O4(_swix(OS_File, _INR(0,1), 6, path));

	res->u.resok4.cinfo.atomic = TRUE;
	res->u.resok4.cinfo.before = changeid;
	changeid++;
	res->u.resok4.cinfo.after = changeid;
	changeid++;

	return res->status = NFS_OK;
}

nstat NFS4_RENAME(RENAME4args *args, RENAME4res *res, struct server_conn *conn)
{
	char *from;
	char *to;
	int oldfiletype;
	int newfiletype;
	int oldtype;
	int newtype;

	if (currentfh == NULL) N4(NFSERR_ROFS);
	if (conn->export->ro) N4(NFSERR_ROFS);

	N4(lookup_filename(savedfh, &(args->oldname), &from, &oldfiletype, conn));
	N4(lookup_filename(currentfh, &(args->newname), &to, &newfiletype, conn));

	O4(_swix(OS_File, _INR(0,1) | _OUT(0), 17, from, &oldtype));
	O4(_swix(OS_File, _INR(0,1) | _OUT(0), 17, to, &newtype));

	if (oldtype == OBJ_NONE) N4(NFSERR_NOENT);
	if (oldtype == OBJ_IMAGE) oldtype = conn->export->imagefs ? OBJ_DIR : OBJ_FILE;
	if (newtype == OBJ_IMAGE) newtype = conn->export->imagefs ? OBJ_DIR : OBJ_FILE;

	if ((newtype != OBJ_NONE) && (oldtype != newtype)) N4(NFSERR_EXIST);

	if (strcmp(from, to) != 0) {
		if (newtype != OBJ_NONE) {
			/* FIXME check share deny stuff as for delete */
			O4(_swix(OS_File, _INR(0,1), 6, to));
		}
		O4(_swix(OS_FSControl, _INR(0,2), 25, from, to));
	}
	if (newfiletype != oldfiletype) {
		O4(_swix(OS_File, _INR(0,2), 18, to, newfiletype));
	}

	res->u.resok4.source_cinfo.atomic = TRUE;
	res->u.resok4.source_cinfo.before = changeid;
	changeid++;
	res->u.resok4.source_cinfo.after = changeid;
	changeid++;

	res->u.resok4.target_cinfo.atomic = TRUE;
	res->u.resok4.target_cinfo.before = changeid;
	changeid++;
	res->u.resok4.target_cinfo.after = changeid;
	changeid++;

	return res->status = NFS_OK;
}


nstat NFS4_SETATTR(SETATTR4args *args, SETATTR4res *res, struct server_conn *conn)
{
	struct stateid *stateid;

	res->attrsset.size = 0;

	if (currentfh == NULL) N4(NFSERR_ROFS);
	if (conn->export->ro) N4(NFSERR_ROFS);

	/* FIXME - should return error for unsupported attrs? */
	U4(res->attrsset.data = palloc(2 * sizeof(unsigned), conn->pool));
	res->attrsset.size = 2;
	memset(res->attrsset.data, 0, 2 * sizeof(unsigned));
	N4(state_getstateid(args->stateid.seqid, args->stateid.other, &stateid, conn));
	N4(set_fattr(currentfh, stateid, &(args->obj_attributes), &(res->attrsset), 0, -1, conn));

	return res->status = NFS_OK;
}

nstat NFS4_NVERIFY(NVERIFY4args *args, NVERIFY4res *res, struct server_conn *conn)
{
	return res->status = verify_fattr(currentfh, 0, &(args->obj_attributes), conn);
}

nstat NFS4_VERIFY(VERIFY4args *args, VERIFY4res *res, struct server_conn *conn)
{
	return res->status = verify_fattr(currentfh, 1, &(args->obj_attributes), conn);
}

nstat NFS4_LOCK(LOCK4args *args, LOCK4res *res, struct server_conn *conn)
{
	struct stateid *stateid;
	int write;
	struct stateid_other *other2;
	struct lock_stateid *lock_stateid;
	int duplicate;
	int deniedwrite;
	uint64_t deniedoffset;
	uint64_t deniedlength;
	uint64_t deniedclientid;
	char *deniedowner;
	int deniedownerlen;
	struct lock_owner *lock_owner;
	union duplicate *dup;

	if (currentfh == NULL) N4(NFSERR_ISDIR);

	if ((args->locktype == READ_LT) || (args->locktype == READW_LT)) {
		write = 0;
	} else if ((args->locktype != WRITE_LT) || (args->locktype != WRITEW_LT)) {
		write = 1;
	} else {
		return NFSERR_INVAL;
	}

	if (graceperiod && (args->reclaim == 0)) return NFSERR_GRACE;

	if (args->locker.new_lock_owner) {
		N4(state_getstateid(args->locker.u.open_owner.open_stateid.seqid, args->locker.u.open_owner.open_stateid.other, &stateid, conn));
		N4(state_checkopenseqid(stateid, args->locker.u.open_owner.open_seqid, 0, &duplicate));
		if (duplicate) goto duplicate;
		NF(state_newlockowner(args->locker.u.open_owner.lock_owner.clientid, args->locker.u.open_owner.lock_owner.owner.data, args->locker.u.open_owner.lock_owner.owner.size, args->locker.u.open_owner.lock_seqid, &lock_owner));
		/*FIXME check currentfh matches stateid? */
		NF(state_lock(stateid->open->file, stateid->open, lock_owner, write,
		              args->offset, args->length, &lock_stateid,
		              &deniedwrite, &deniedoffset, &deniedlength,
		              &deniedclientid, &deniedowner, &deniedownerlen));
	} else {
		N4(state_getstateid(args->locker.u.lock_owner.lock_stateid.seqid, args->locker.u.lock_owner.lock_stateid.other, &stateid, conn));
		N4(state_checklockseqid(stateid, args->locker.u.lock_owner.lock_seqid, &duplicate));
		if (duplicate) goto duplicate;
		NF(state_lock(stateid->open->file, stateid->open, stateid->lock->lock_owner,
		              write, args->offset, args->length, &lock_stateid,
		              &deniedwrite, &deniedoffset, &deniedlength,
		              &deniedclientid, &deniedowner, &deniedownerlen));
	}

	res->u.resok4.lock_stateid.seqid = lock_stateid->seqid;
	other2 = (struct stateid_other *)res->u.resok4.lock_stateid.other;
	other2->id = lock_stateid;
	other2->verifier = verifier[0];
	other2->lock = 1;

	if (args->locker.new_lock_owner) {
		dup = &(stateid->open->open_owner->duplicate);
	} else {
		dup = &(stateid->lock->lock_owner->duplicate);
	}
	dup->lock.status = NFS_OK;
	dup->lock.res.ok.seqid = lock_stateid->seqid;
	memcpy(dup->lock.res.ok.other, res->u.resok4.lock_stateid.other, sizeof(struct stateid_other));

	return res->status = NFS_OK;

failure:
	if (args->locker.new_lock_owner) {
		dup = &(stateid->open->open_owner->duplicate);
	} else {
		dup = &(stateid->lock->lock_owner->duplicate);
	}
	dup->lock.status = res->status;
	dup->lock.res.denied.write = deniedwrite;
	dup->lock.res.denied.offset = deniedoffset;
	dup->lock.res.denied.length = deniedlength;
	dup->lock.res.denied.clientid = deniedclientid;
	memcpy(dup->lock.res.denied.owner, deniedowner, deniedownerlen);
	dup->lock.res.denied.ownerlen = deniedownerlen;

	res->u.denied.locktype = deniedwrite ? WRITE_LT : READ_LT;
	res->u.denied.offset = deniedoffset;
	res->u.denied.length = deniedlength;
	res->u.denied.owner.clientid = deniedclientid;
	res->u.denied.owner.owner.data = deniedowner;
	res->u.denied.owner.owner.size = deniedownerlen;
	return res->status;

duplicate:
	if (args->locker.new_lock_owner) {
		dup = &(stateid->open->open_owner->duplicate);
	} else {
		dup = &(stateid->lock->lock_owner->duplicate);
	}
	res->status = dup->lock.status;
	if (res->status == NFS_OK) {
		res->u.resok4.lock_stateid.seqid = dup->lock.res.ok.seqid;
		memcpy(res->u.resok4.lock_stateid.other, dup->lock.res.ok.other, sizeof(struct stateid_other));
	} else {
		res->u.denied.locktype = dup->lock.res.denied.write ? WRITE_LT : READ_LT;
		res->u.denied.offset = dup->lock.res.denied.offset;
		res->u.denied.length = dup->lock.res.denied.length;
		res->u.denied.owner.clientid = dup->lock.res.denied.clientid;
		res->u.denied.owner.owner.data = dup->lock.res.denied.owner;
		res->u.denied.owner.owner.size = dup->lock.res.denied.ownerlen;
	}
	return res->status;
}

nstat NFS4_LOCKT(LOCKT4args *args, LOCKT4res *res, struct server_conn *conn)
{
	int write;
	struct lock_owner *lock_owner;
	struct openfile *file;
	int deniedwrite;
	uint64_t deniedoffset;
	uint64_t deniedlength;
	uint64_t deniedclientid;
	char *deniedowner;
	int deniedownerlen;

	(void)conn;

	if (currentfh == NULL) N4(NFSERR_ISDIR);

	if ((args->locktype == READ_LT) || (args->locktype == READW_LT)) {
		write = 0;
	} else if ((args->locktype != WRITE_LT) || (args->locktype != WRITEW_LT)) {
		write = 1;
	} else {
		return NFSERR_INVAL;
	}

	NR(state_getlockowner(args->owner.clientid, args->owner.owner.data, args->owner.owner.size, &lock_owner));
	if (lock_owner == NULL) return NFS_OK;

	NR(filecache_getfile(currentfh, &file));
	if (file == NULL) return NFS_OK;

	res->status = state_lock(file, NULL, lock_owner, write, args->offset, args->length,
	                         NULL, &deniedwrite, &deniedoffset, &deniedlength,
	                         &deniedclientid, &deniedowner, &deniedownerlen);

	if (res->status == NFSERR_DENIED) {
		res->u.denied.offset = deniedoffset;
		res->u.denied.length = deniedlength;
		res->u.denied.locktype = deniedwrite ? WRITE_LT : READ_LT;
		res->u.denied.owner.clientid = deniedclientid;
		res->u.denied.owner.owner.data = deniedowner;
		res->u.denied.owner.owner.size = deniedownerlen;
	}

	return res->status;
}

nstat NFS4_LOCKU(LOCKU4args *args, LOCKU4res *res, struct server_conn *conn)
{
	struct stateid *stateid;
	int duplicate;

	if ((args->locktype != READ_LT) &&
	    (args->locktype != WRITE_LT) &&
	    (args->locktype != READW_LT) &&
	    (args->locktype != WRITEW_LT)) {
		return NFSERR_INVAL;
	}

	if (currentfh == NULL) N4(NFSERR_ISDIR);

	N4(state_getstateid(args->lock_stateid.seqid, args->lock_stateid.other, &stateid, conn));
	N4(state_checklockseqid(stateid, args->seqid, &duplicate));
	if (duplicate) {
		res->u.lock_stateid = args->lock_stateid;
		res->u.lock_stateid.seqid = stateid->lock->lock_owner->duplicate.locku.seqid;
		return res->status = stateid->lock->lock_owner->duplicate.locku.status;
	}

	NF(state_unlock(stateid, args->offset, args->length));

	stateid->lock->seqid++;

	res->u.lock_stateid = args->lock_stateid;
	res->u.lock_stateid.seqid = stateid->lock->seqid;
	stateid->lock->lock_owner->duplicate.locku.seqid = stateid->lock->seqid;

failure:
	return stateid->lock->lock_owner->duplicate.locku.status = res->status;
}

nstat NFS4_RELEASE_LOCKOWNER(RELEASE_LOCKOWNER4args *args, RELEASE_LOCKOWNER4res *res, struct server_conn *conn)
{
	(void)conn;

	N4(state_releaselockowner(args->lock_owner.clientid, args->lock_owner.owner.data, args->lock_owner.owner.size));

	return res->status = NFS_OK;
}

nstat NFS4_SECINFO(SECINFO4args *args, SECINFO4res *res, struct server_conn *conn)
{
	(void)conn;
	(void)args;
	/*FIXME*/

	return res->status = NFSERR_NOTSUPP;
}


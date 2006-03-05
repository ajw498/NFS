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


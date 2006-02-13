/*
	$Id$
	$URL$

	Functions to implement decoding of recieved RPC calls.


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
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <socklib.h>
#include <time.h>
#include <sys/time.h>
#include <swis.h>
#include <sys/errno.h>
#include <unixlib.h>
#include <stdarg.h>

#include "rpc-structs.h"
#include "rpc-process1.h"
#include "rpc-process2.h"

#include "moonfish.h"
#include "utils.h"

#include "request-decode.h"


/* These point to the current buffer for tx or rx, and are used by
   the process_* macros to read/write data from/to */
char *ibuf;
char *ibufend;
char *obuf;
char *obufend;

static struct rpc_msg call_header;
static struct rpc_msg reply_header;

static void init_output(struct server_conn *conn)
{
	obuf = conn->reply;
	obufend = obuf + BUFFERSIZE;

	/* Leave room for the record marker */
	if (conn->tcp) obuf += 4;

	process_struct_rpc_msg(OUTPUT, reply_header, 0);
buffer_overflow:
	return;
}

void request_decode(struct server_conn *conn)
{
	ibuf = conn->request;
	ibufend = ibuf + conn->requestlen;
	process_struct_rpc_msg(INPUT, call_header, 0);
	reply_header.xid = call_header.xid;
	reply_header.body.mtype = REPLY;

	if (call_header.body.mtype != CALL || call_header.body.u.cbody.rpcvers != RPC_VERSION) {
		reply_header.body.u.rbody.stat = MSG_DENIED;
		reply_header.body.u.rbody.u.rreply.stat = RPC_MISMATCH;
		reply_header.body.u.rbody.u.rreply.u.mismatch_info.low = RPC_VERSION;
		reply_header.body.u.rbody.u.rreply.u.mismatch_info.high = RPC_VERSION;
		init_output(conn);
	} else {
		int hi;
		int lo;

		reply_header.body.u.rbody.stat = MSG_ACCEPTED;
		reply_header.body.u.rbody.u.areply.reply_data.stat = SUCCESS;

		init_output(conn);

		/* Get uid/gid ? */
	/*	if (conn->auth) {
			call_header.body.u.cbody.cred.flavor = AUTH_UNIX;
			call_header.body.u.cbody.cred.body.size = conn->authsize;
			call_header.body.u.cbody.cred.body.data = conn->auth;
		} */

		if (logging) syslogf(LOGNAME, LOG_ACCESS, "Access: xid %x prog %d vers %d proc %d",
		                     call_header.xid, call_header.body.u.cbody.prog,
		                     call_header.body.u.cbody.vers, call_header.body.u.cbody.proc);

		reply_header.body.u.rbody.u.areply.reply_data.stat = portmapper_decodebody(call_header.body.u.cbody.prog, call_header.body.u.cbody.vers, call_header.body.u.cbody.proc, &hi, &lo, conn);

		if (reply_header.body.u.rbody.u.areply.reply_data.stat == PROG_MISMATCH) {
			reply_header.body.u.rbody.u.areply.reply_data.u.mismatch_info.low = lo;
			reply_header.body.u.rbody.u.areply.reply_data.u.mismatch_info.high = hi;
		}
	}

	if (reply_header.body.u.rbody.u.areply.reply_data.stat != SUCCESS) {
		/* If the reply is not a success then reparse the header,
		   overwriting any garbage body */
		init_output(conn);
	}

	conn->replylen = obuf - conn->reply;

	if (conn->tcp) {
		/* Insert the record marker at the start of the buffer */
		int recordmarker = 0x80000000 | (conn->replylen - 4);
		obuf = conn->reply;
		obufend = obuf + 4;
		process_int(OUTPUT, recordmarker, 0);
	}

	return;

buffer_overflow:
	/* Should be impossible to reach here */
	conn->replylen = 0;
}



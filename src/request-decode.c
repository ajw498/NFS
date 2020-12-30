/*
	Functions to implement decoding of received RPC calls.


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



#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include <sys/time.h>
#include <swis.h>
#include <sys/errno.h>
#ifdef USE_TCPIPLIBS
# include <socklib.h>
# include <unixlib.h>
# include <riscos.h>
#endif
#include <stdarg.h>

#include "rpc-structs.h"
#include "rpc-process.h"

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
	obufend = obuf + MFBUFFERSIZE;

	/* Leave room for the record marker */
	if (conn->tcp) obuf += 4;

	process_rpc_msg(OUTPUT, &reply_header, conn->pool);
}

void request_decode(struct server_conn *conn)
{
	ibuf = conn->request;
	ibufend = ibuf + conn->requestlen;
	if (process_rpc_msg(INPUT, &call_header, conn->pool)) goto buffer_overflow;
	reply_header.xid = call_header.xid;
	reply_header.body.mtype = REPLY;

	conn->nfs4 = 0;

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

		/* Get the uid and gid */
		if (call_header.body.u.cbody.cred.flavor == AUTH_UNIX) {
			char *oldibuf = ibuf;
			char *oldibufend = ibufend;
			auth_unix auth;

			ibuf = call_header.body.u.cbody.cred.body.data;
			ibufend = ibuf + call_header.body.u.cbody.cred.body.size;
			if (process_auth_unix(INPUT, &auth, conn->pool)) goto buffer_overflow;
			ibuf = oldibuf;
			ibufend = oldibufend;

			conn->uid = auth.uid;
			conn->gid = auth.gid;
		} else {
			conn->uid = 0;
			conn->gid = 0;
		}

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
		unsigned recordmarker = 0x80000000 | (conn->replylen - 4);
		obuf = conn->reply;
		obufend = obuf + 4;
		process_unsigned(OUTPUT, &recordmarker, conn->pool);
	}

	return;

buffer_overflow:
	/* Should be impossible to reach here */
	conn->replylen = 0;
}



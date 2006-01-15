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

#include "portmapper-recv.h"
#include "mount1-recv.h"
#include "mount3-recv.h"

#include "rpc-decode.h"

/* The worstcase size of a header for read/write calls.
   If this value is not big enough, the data part will not be of
   an optimum size, but nothing bad will happen */
#define MAX_HDRLEN 416

#define MAX_DATABUFFER 32000 /*FIXME*/

/* The size to use for tx and rx buffers */
#define BUFFERSIZE (MAX_HDRLEN + MAX_DATABUFFER)



static struct rpc_msg call_header;
static struct rpc_msg reply_header;


/* Buffer to allocate linked list structures from. Should be significanly
   faster than using the RMA, doesn't require freeing each element of
   the list, and won't fragment */
static char malloc_buffer[MAX_DATABUFFER];
/* The start of the next location to return for linked list malloc */
static char *nextmalloc;

extern char *output_buf;

void init_output(void)
{
printf("init_output %d\n", reply_header.body.u.rbody.u.areply.reply_data.stat);
	buf = output_buf;
	bufend = buf + 1024;
	process_struct_rpc_msg(OUTPUT, reply_header, 0);
buffer_overflow:
	return;
}

void rpc_decode(void)
{
	process_struct_rpc_msg(INPUT, call_header, 0);
	reply_header.xid = call_header.xid;
	reply_header.body.mtype = REPLY;

	if (call_header.body.mtype != CALL || call_header.body.u.cbody.rpcvers != RPC_VERSION) {
		reply_header.body.u.rbody.stat = MSG_DENIED;
		reply_header.body.u.rbody.u.rreply.stat = RPC_MISMATCH;
		reply_header.body.u.rbody.u.rreply.u.mismatch_info.low = RPC_VERSION;
		reply_header.body.u.rbody.u.rreply.u.mismatch_info.high = RPC_VERSION;
		goto error;
	}

	reply_header.body.u.rbody.stat = MSG_ACCEPTED;
	reply_header.body.u.rbody.u.areply.reply_data.stat = SUCCESS;

	/* Get uid/gid ? */
/*	if (conn->auth) {
		call_header.body.u.cbody.cred.flavor = AUTH_UNIX;
		call_header.body.u.cbody.cred.body.size = conn->authsize;
		call_header.body.u.cbody.cred.body.data = conn->auth;
	} */

	printf("xid %x prog %d vers %d\n", call_header.xid,call_header.body.u.cbody.prog,call_header.body.u.cbody.vers);

	switch (call_header.body.u.cbody.prog) {
	case 100000/*PMAP_RPC_PROGRAM*/:
		switch (call_header.body.u.cbody.vers) {
		case 2/*PMAP_RPC_VERSION*/:
			reply_header.body.u.rbody.u.areply.reply_data.stat = portmapper_decode(call_header.body.u.cbody.proc);
			printf("ret %d\n", reply_header.body.u.rbody.u.areply.reply_data.stat);
			break;
		default:
			reply_header.body.u.rbody.u.areply.reply_data.stat = PROG_MISMATCH;
			reply_header.body.u.rbody.u.areply.reply_data.u.mismatch_info.low = 2;
			reply_header.body.u.rbody.u.areply.reply_data.u.mismatch_info.high = 2;
		}
		break;
	case 100005/*MOUNT_RPC_PROGRAM*/:
		switch (call_header.body.u.cbody.vers) {
		case 1:
			reply_header.body.u.rbody.u.areply.reply_data.stat = mount1_decode(call_header.body.u.cbody.proc);
			break;
		case 3:
			reply_header.body.u.rbody.u.areply.reply_data.stat = mount3_decode(call_header.body.u.cbody.proc);
			break;
		default:
			reply_header.body.u.rbody.u.areply.reply_data.stat = PROG_MISMATCH;
			reply_header.body.u.rbody.u.areply.reply_data.u.mismatch_info.low = 1;
			reply_header.body.u.rbody.u.areply.reply_data.u.mismatch_info.high = 3;
		}
		break;
	default:
		reply_header.body.u.rbody.u.areply.reply_data.stat = PROG_UNAVAIL;
	}

	if (reply_header.body.u.rbody.u.areply.reply_data.stat == SUCCESS) return;

error:
	init_output();
	return;

buffer_overflow:
	abort(); /*Should be impossible? FIXME*/
}



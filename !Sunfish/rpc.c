/*
	$Id$
	$URL$

	Functions to implement RPC calling. All network access is done in this file.


	Copyright (C) 2003 Alex Waugh
	
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


#include "rpc-calls.h"
#include "portmapper-calls.h"
#include "mount-calls.h"
#include "nfs-calls.h"
#include "pcnfsd-calls.h"
#include "rpc.h"


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

/* The worstcase size of a header for read/write calls.
   If this value is not big enough, the data part will not be of
   an optimum size, but nothing bad will happen */
#define MAX_HDRLEN 416

/* The size to use for tx and rx buffers */
#define BUFFERSIZE (MAX_HDRLEN + MAXDATA)


/* A monotonicly increasing transaction id */
static unsigned int xid;

static struct rpc_msg call_header;
static struct rpc_msg reply_header;

/* tx and rx buffers. rx is double buffered for readdir calls which need
   to a lookup for each file retured by the readdir call */
static char tx_buffer[BUFFERSIZE];
static char rx_buffer1[BUFFERSIZE];
static char rx_buffer2[BUFFERSIZE];

/* The buffer currently in use for rx */
static char *rx_buffer = rx_buffer1;

/* Buffer to allocate linked list structures from. Should be significanly
   faster than using the RMA, doesn't require freeing each element of
   the list, and won't fragment */
static char malloc_buffer[MAXDATA];
/* The start of the next location to return for linked list malloc */
static char *nextmalloc;

/* These point to the current buffer for tx or rx, and are used by
   the process_* macros to read/write data from/to */
char *buf;
char *bufend;

/* Swap the active rx buffer */
void swap_rxbuffers(void)
{
	rx_buffer = rx_buffer == rx_buffer1 ? rx_buffer2 : rx_buffer1;
}

/* Initialise parts of the header that are the same for all calls */
void rpc_init_header(void)
{
	xid = 1;
	call_header.body.mtype = CALL;
	call_header.body.u.cbody.rpcvers = RPC_VERSION;
	call_header.body.u.cbody.verf.flavor = AUTH_NULL;
	call_header.body.u.cbody.verf.body.size = 0;
}

#define Resolver_GetHost 0x46001

/* A version of gethostbyname that will timeout.
   Also handles IP addresses without needing a reverse lookup */
static os_error *gethostbyname_timeout(char *host, unsigned long timeout, struct hostent **hp)
{
	unsigned long starttime;
	unsigned long endtime;
	os_error *err;
	int errnum;
	int quad1, quad2, quad3, quad4;

	if (sscanf(host, "%d.%d.%d.%d", &quad1, &quad2, &quad3, &quad4) == 4) {
		/* Host is an IP address, so doesn't need resolving */
		static struct hostent hostent;
		static unsigned int addr;
		static char *addr_list = (char *)&addr;

		addr = quad1 | (quad2 << 8) | (quad3 << 16) | (quad4 << 24);
		hostent.h_addr_list = &addr_list;
		hostent.h_length = sizeof(addr);

		*hp = &hostent;
		return NULL;
	}

	err = _swix(OS_ReadMonotonicTime, _OUT(0), &starttime);
	if (err) return err;

	do {
		err = _swix(Resolver_GetHost, _IN(0) | _OUTR(0,1), host, &errnum, hp);
		if (err) return err;

		if (errnum != EINPROGRESS) break;

		err = _swix(OS_ReadMonotonicTime, _OUT(0), &endtime);
		if (err) return err;

	} while (endtime - starttime < timeout * 100);

	if (errnum == 0) return NULL; /* Host found */

	return gen_error(RPCERRBASE + 1, "Unable to resolve hostname '%s' (%d)", host, errnum);
}

/* Initialise for each mount */
os_error *rpc_init_connection(struct conn_info *conn)
{
	struct hostent *hp;
	os_error *err;

	conn->sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (conn->sock < 0) return gen_error(RPCERRBASE + 2,"Unable to open socket (%s)", xstrerror(errno));

	if (conn->localportmax != 0) {
		/* Use a specific local port */
		struct sockaddr_in name;
		int port = conn->localportmin;
		int ret;

		name.sin_family = AF_INET;
		name.sin_addr.s_addr = (int)htonl(INADDR_ANY);

		do {
			name.sin_port = htons(port++);
			ret = bind(conn->sock, (struct sockaddr *)&name, sizeof(name));
		} while (ret != 0 && port <= conn->localportmax);

		if (ret) return gen_error(RPCERRBASE + 14, "Unable to bind socket to local port (%s)", xstrerror(errno));
	}

	err = gethostbyname_timeout(conn->server, conn->timeout, &hp);
	if (err) return err;

	memcpy(&(conn->sockaddr.sin_addr), hp->h_addr, hp->h_length);
	conn->sockaddr.sin_family = AF_INET;
	conn->sockaddr.sin_port = 0;

	return NULL;
}

/* Close for each mount */
os_error *rpc_close_connection(struct conn_info *conn)
{
	if (close(conn->sock)) return gen_error(RPCERRBASE + 3, "Socket close failed (%s)", xstrerror(errno));
	return NULL;
}

/* Log an entire data packet */
static void logdata(int rx, char *buf, int len)
{
	int i;

	syslogf(LOGNAME, LOGDATA, "%s data (%d):", rx ? "rx" : "tx", len);
	for (i=0; i<(len & ~3); i+=4) syslogf(LOGNAME, LOGDATA, "  %.2x %.2x %.2x %.2x", buf[i], buf[i+1], buf[i+2], buf[i+3]);
	for (i=0; i<(len & 3); i++) syslogf(LOGNAME, LOGDATA, "  %.2x", buf[(len &~3) + i]);
}

/* Setup buffer and write call header to it */
void rpc_prepare_call(unsigned int prog, unsigned int vers, unsigned int proc, struct conn_info *conn)
{
	call_header.xid = xid++;
	call_header.body.u.cbody.prog = prog;
	call_header.body.u.cbody.vers = vers;
	call_header.body.u.cbody.proc = proc;

	if (conn->auth) {
		call_header.body.u.cbody.cred.flavor = AUTH_UNIX;
		call_header.body.u.cbody.cred.body.size = conn->authsize;
		call_header.body.u.cbody.cred.body.data = conn->auth;
	} else {
		call_header.body.u.cbody.cred.flavor = AUTH_NULL;
		call_header.body.u.cbody.cred.body.size = 0;
	}

	buf = tx_buffer;
	bufend = tx_buffer + sizeof(tx_buffer);

	nextmalloc = malloc_buffer;

	process_struct_rpc_msg(OUTPUT, call_header, 0);

buffer_overflow: /* Should be impossible, but prevent compiler complaining */
	return;
}

/* Send the already filled in tx buffer, the read the response and process
   the rpc reply header */
os_error *rpc_do_call(struct conn_info *conn)
{
	int len = 0;
	int port;
	int tries = 0;
	int ret;
	int tx_buflen = buf - tx_buffer;

	/* Choose the port to use */
	switch (call_header.body.u.cbody.prog) {
	case PMAP_RPC_PROGRAM:
		port = htons(conn->portmapper_port);
		break;
	case MOUNT_RPC_PROGRAM:
		port = htons(conn->mount_port);
		break;
	case PCNFSD_RPC_PROGRAM:
		port = htons(conn->pcnfsd_port);
		break;
	case NFS_RPC_PROGRAM:
	default:
		port = htons(conn->nfs_port);
		break;
	}

	/* Only connect the socket if the port is different from the last
	   time we used the socket */
	if (port != conn->sockaddr.sin_port) {
		conn->sockaddr.sin_port = port;
		if (connect(conn->sock, (struct sockaddr *)&(conn->sockaddr), sizeof(struct sockaddr_in)) < 0) {
			return gen_error(RPCERRBASE + 4, "Connect on socket failed (%s)", xstrerror(errno));
		}
	}

	/* Send the data, retrying if there is a timeout */
	do {
		fd_set rfds;
		struct timeval tv;

		if (enablelog) logdata(0, tx_buffer, tx_buflen);
		if (send(conn->sock, tx_buffer, tx_buflen, 0) == -1) {
			return gen_error(RPCERRBASE + 5, "Sending data failed (%s)", xstrerror(errno));
		}

		FD_ZERO(&rfds);
		FD_SET(conn->sock, &rfds);
		tv.tv_sec = conn->timeout;
		tv.tv_usec = 0;
		ret = select(conn->sock + 1, &rfds, NULL, NULL, &tv);
		if (ret == -1) return gen_error(RPCERRBASE + 6, "Select on socket failed (%s)", xstrerror(errno));
		if (ret > 0) {
			len = read(conn->sock, rx_buffer, sizeof(rx_buffer1));
			if (len == -1) return gen_error(RPCERRBASE + 7,"Read from socket failed (%s)", xstrerror(errno));

			if (enablelog) logdata(1, rx_buffer, len);
			buf = rx_buffer;
			bufend = rx_buffer + len;
			process_struct_rpc_msg(INPUT, reply_header, 0);

			if (reply_header.xid < call_header.xid) {
				/* Must be a duplicate from an earlier timeout */
				ret = 0;
				tries--;
			}
		}
	} while ((ret == 0) && (tries++ < conn->retries));
	if (ret == 0) return gen_error(RPCERRBASE + 8, "Connection timed out");

	/* Check that the RPC completed successfully */
	if (reply_header.xid != call_header.xid) return gen_error(RPCERRBASE + 9, "Unexpected response (xid mismatch)");
	if (reply_header.body.mtype != REPLY) return gen_error(RPCERRBASE + 10, "Unexpected response (not an RPC reply)");
	if (reply_header.body.u.rbody.stat != MSG_ACCEPTED) {
		if (reply_header.body.u.rbody.u.rreply.stat == AUTH_ERROR) {
			return gen_error(RPCERRBASE + 11, "RPC message rejected (Authentication error)");
		} else {
			return gen_error(RPCERRBASE + 12, "RPC message rejected");
		}
	}
	if (reply_header.body.u.rbody.u.areply.reply_data.stat != SUCCESS) {
		return gen_error(RPCERRBASE + 13, "RPC failed (%d)", reply_header.body.u.rbody.u.areply.reply_data.stat);
	}
	return NULL;

buffer_overflow:
	return rpc_buffer_overflow();
}

/* Allocate a chunk from the linklist buffer */
void *llmalloc(size_t size)
{
	void *mem;

	if (nextmalloc + size > malloc_buffer + sizeof(malloc_buffer)) {
		mem = NULL;
	} else {
		mem = nextmalloc;
		nextmalloc += size;
	}

	return mem;
}


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

/* Size of the fifo to use for pipelining. Must be at least 2, to allow
   READDIR to double buffer. Greater than 2 has negligible increase in
   performance. */
#define FIFOSIZE 2

/* An xid indicating the buffer entry is unused */
#define UNALLOCATED 0


/* A monotonicly increasing transaction id */
static unsigned int nextxid;

static struct rpc_msg call_header;
static struct rpc_msg reply_header;


struct buffer_list {
	int len;
	char buffer[BUFFERSIZE];
};

struct fifo_entry {
	unsigned int xid;
	unsigned int retries;
	struct buffer_list tx;
	struct buffer_list *rx;
};

/* FIFO for keeping track of requests and their replies */
static struct fifo_entry fifo[FIFOSIZE];
static int head;
static int tail;

/* Pool of rx buffers */
static struct buffer_list buffers[FIFOSIZE];

/* The position within to buffer list to start looking for a free rx buffer */
static int freebufferstart;

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
	freebufferstart++;
	if (freebufferstart >= FIFOSIZE) freebufferstart = 0;
}

void rpc_resetfifo(void)
{
	int i;

	head = 0;
	tail = 0;
	freebufferstart = 0;
	for (i = 0; i < FIFOSIZE; i++) {
		fifo[i].xid = UNALLOCATED;
		buffers[i].len = 0;
	}
}

/* Initialise parts of the header that are the same for all calls */
void rpc_init_header(void)
{
	rpc_resetfifo();

	nextxid = 1;

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
	call_header.xid = nextxid++;
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

	buf = fifo[head].tx.buffer;
	bufend = buf + BUFFERSIZE;
	fifo[head].xid = call_header.xid;

	nextmalloc = malloc_buffer;

	process_struct_rpc_msg(OUTPUT, call_header, 0);

buffer_overflow: /* Should be impossible, but prevent compiler complaining */
	return;
}

/* Send the already filled in tx buffer, the read the response and process
   the rpc reply header */
os_error *rpc_do_call(struct conn_info *conn, enum callctl calltype)
{
	int ret;

	if (calltype == TXBLOCKING || calltype == TXNONBLOCKING) {
		int port;

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

		fifo[head].tx.len = buf - fifo[head].tx.buffer;
		fifo[head].retries = 0;
		fifo[head].rx = NULL;
		if (enablelog) logdata(0, fifo[head].tx.buffer, fifo[head].tx.len);
		if (send(conn->sock, fifo[head].tx.buffer, fifo[head].tx.len, 0) == -1) {
			return gen_error(RPCERRBASE + 5, "Sending data failed (%s)", xstrerror(errno));
		}
		head++;
		if (head >= FIFOSIZE) head = 0;

		if (fifo[head].xid != UNALLOCATED && fifo[tail].rx == NULL) {
			/* The fifo is full, so turn this into a blocking call */
			if (calltype == TXNONBLOCKING) calltype = TXBLOCKING;
			if (calltype == RXNONBLOCKING) calltype = RXBLOCKING;
		}
	}

	if (fifo[tail].rx == NULL) {
		/* If we don't have an entry to return straight away then try
		   and read one */

		do {
			fd_set rfds;
			struct timeval tv;
			int freebuffer;
	
			/* Search for a free buffer entry.
			   There is guaranteed to be at least one */
			for (freebuffer = freebufferstart; freebuffer < freebufferstart + FIFOSIZE; freebuffer++) {
				if (buffers[freebuffer % FIFOSIZE].len == 0) {
					freebuffer = freebuffer % FIFOSIZE;
					break;
				}
			}
	
			FD_ZERO(&rfds);
			FD_SET(conn->sock, &rfds);
			tv.tv_sec = (calltype == TXNONBLOCKING || calltype == RXNONBLOCKING) ? 0 : conn->timeout;
			tv.tv_usec = 0;
			ret = select(conn->sock + 1, &rfds, NULL, NULL, &tv);
			if (ret == -1) return gen_error(RPCERRBASE + 6, "Select on socket failed (%s)", xstrerror(errno));
			if (ret > 0) {
				int len;
				int i;
	
				len = read(conn->sock, buffers[freebuffer].buffer, BUFFERSIZE);
				if (len == -1) return gen_error(RPCERRBASE + 7,"Read from socket failed (%s)", xstrerror(errno));
	
				if (enablelog) logdata(1, buffers[freebuffer].buffer, len);
				buf = buffers[freebuffer].buffer;
				bufend = buf + len;
				process_struct_rpc_msg(INPUT, reply_header, 0);
				/*FIXME: we only need the xid, so don't bother to parse the whole header */
	
				/* Check to see if it a reply that we are waiting for */
				for (i = 0; i < FIFOSIZE; i++) {
					if (reply_header.xid == fifo[i].xid) {
						fifo[i].rx = &(buffers[freebuffer]);
						buffers[freebuffer].len = len;
						break;
					}
				}
			} else if (calltype == TXBLOCKING || calltype == RXBLOCKING) {
				/* No data recived, so retransmit the oldest fifo entry if it
				   is still pending, unless we are non blocking */
	
				if (fifo[tail].rx == NULL && fifo[tail].retries > conn->retries) {
					return gen_error(RPCERRBASE + 8, "Connection timed out");
				}
	
				if (enablelog) logdata(0, fifo[tail].tx.buffer, fifo[tail].tx.len);
				if (send(conn->sock, fifo[tail].tx.buffer, fifo[tail].tx.len, 0) == -1) {
					return gen_error(RPCERRBASE + 5, "Sending data failed (%s)", xstrerror(errno));
				}
				fifo[tail].retries++;
			}
		} while ((calltype == TXBLOCKING || calltype == RXBLOCKING) && fifo[tail].rx == NULL);
	}

	/* Return if the oldest request has not been recieved. This will only
	   ever be the case if we are non-blocking */
	if (fifo[tail].rx == NULL) return ERR_WOULDBLOCK;

	buf = fifo[tail].rx->buffer;
	bufend = buf + fifo[tail].rx->len;
	process_struct_rpc_msg(INPUT, reply_header, 0);
	/* Check that the RPC completed successfully */
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

	/* Remove this entry from the fifo */
	fifo[tail].rx->len = 0;
	fifo[tail].xid = UNALLOCATED;
	tail++;
	if (tail >= FIFOSIZE) tail = 0;

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


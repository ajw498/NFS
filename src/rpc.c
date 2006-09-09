/*
	$Id$

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
#ifdef NFS3
#include "mount3-calls.h"
#include "nfs3-calls.h"
#else
#include "mount1-calls.h"
#include "nfs2-calls.h"
#endif
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

#include "sunfish.h"
#include "utils.h"


/* An xid indicating the buffer entry is unused */
#define UNALLOCATED 0


/* A monotonicly increasing transaction id */
static unsigned int nextxid;


/* A pool of rx buffers. There should be the same number of rx buffers
   as possible outstanding calls */
struct buffer_list {
	int len;
	char buffer[BUFFERSIZE];
};

static struct buffer_list buffers[FIFOSIZE];

/* A FIFO for keeping track of requests and their replies. We need to
   remember the entire call buffer in case the call gets lost and we
   need to retransmit */
struct fifo_entry {
	unsigned int xid;
	unsigned int retries;
	struct buffer_list tx;
	struct buffer_list *rx;
};

static struct fifo_entry fifo[FIFOSIZE];
static unsigned int head;
static unsigned int tail;

/* The position within to buffer list to start looking for a free rx buffer */
static unsigned int freebufferstart;


/* These point to the current buffer for tx or rx, and are used by
   the process_* macros to read/write data from/to */
char *ibuf;
char *ibufend;
char *obuf;
char *obufend;

#define INVALIDBUFFER (-1)

/* The current rx buffer we are in the middle of recieving into */
static int currentbuffer = INVALIDBUFFER;

/* Execute a send, repeating until the data is sent or a timeout occurs */
static int blocking_send(int sock, char *buf, int len, long timeout)
{
	time_t t = clock();
	int ret;

	do {
		trigger_callback();
		ret = send(sock, buf, len, 0);
		if (ret > 0 && ret < len) {
			buf += ret;
			len -= ret;
			ret = -1;
			errno = EWOULDBLOCK;
		}
	} while (ret == -1 && (errno == EWOULDBLOCK || errno == ENOTCONN) && clock() < (t + (timeout * CLOCKS_PER_SEC)));
	return ret;
}

/* Reset all fifo entries and rx buffers to an unallocated state */
void rpc_resetfifo(void)
{
	int i;

	head = 0;
	tail = 0;
	freebufferstart = 0;
	currentbuffer = INVALIDBUFFER;
	for (i = 0; i < FIFOSIZE; i++) {
		fifo[i].xid = UNALLOCATED;
		fifo[i].rx = NULL;
		buffers[i].len = 0;
	}
}

/* Close the socket, ignoring any errors (it will be reopened on the next request) */
#define gen_rpc_error(conn, ...) (\
	(conn->tcp ? rpc_close_connection(conn) : NULL),\
	gen_error(__VA_ARGS__))


/* Swap the active rx buffer */
void swap_rxbuffers(void)
{
	freebufferstart++;
	if (freebufferstart >= FIFOSIZE) freebufferstart = 0;
}

/* Initialise parts of the header that are the same for all calls */
void rpc_init_header(void)
{
	rpc_resetfifo();

	nextxid = 1;
}

static os_error *rpc_create_socket(struct conn_info *conn)
{
	int on = 1;

	conn->sock = socket(AF_INET, conn->tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
	if (conn->sock < 0) return gen_error(RPCERRBASE + 2,"Unable to open socket (%s)", xstrerror(errno));

	/* Make the socket non-blocking */
	if (ioctl(conn->sock, FIONBIO, &on) < 0) {
		return gen_error(RPCERRBASE + 6,"Unable to ioctl (%s)", xstrerror(errno));
	}

	if (conn->localportmax != 0) {
		/* Use a specific local port */
		struct sockaddr_in name;
		int port = conn->localportmin;
		int ret;

		memset(&name, 0, sizeof(name));
		name.sin_family = AF_INET;
		name.sin_addr.s_addr = (int)htonl(INADDR_ANY);

		do {
			name.sin_port = htons(port++);
			ret = bind(conn->sock, (struct sockaddr *)&name, sizeof(name));
		} while (ret != 0 && port <= conn->localportmax);

		if (ret) return gen_error(RPCERRBASE + 14, "Unable to bind socket to local port (%s)", xstrerror(errno));
	}

	return NULL;
}

/* Initialise for each mount */
os_error *rpc_init_connection(struct conn_info *conn)
{
	struct hostent *hp;
	os_error *err;

	err = rpc_create_socket(conn);
	if (err) return err;

	err = gethostbyname_timeout(conn->server, conn->timeout, &hp);
	if (err) return err;

	memset(&(conn->sockaddr), 0, sizeof(conn->sockaddr));
	memcpy(&(conn->sockaddr.sin_addr), hp->h_addr, hp->h_length);
	conn->sockaddr.sin_family = AF_INET;
	conn->sockaddr.sin_port = 0;

	return NULL;
}

/* Close for each mount */
os_error *rpc_close_connection(struct conn_info *conn)
{
	int ret;

	if (conn->sock < 0) return NULL;

	ret = close(conn->sock);
	conn->sock = -1;

	if (ret) return gen_error(RPCERRBASE + 3, "Socket close failed (%s)", xstrerror(errno));

	return NULL;
}

/* Log an entire data packet */
static void logdata(int rx, char *buf, int len)
{
	int i;

	syslogf(LOGNAME, LOGDATASUMMARY, "%s data (%d): xid %.2x%.2x%.2x%.2x", rx ? "rx" : "tx", len, buf[0], buf[1], buf[2], buf[3]);
	for (i=0; i<(len & ~3); i+=4) syslogf(LOGNAME, LOGDATA, "  %.2x %.2x %.2x %.2x", buf[i], buf[i+1], buf[i+2], buf[i+3]);
	for (i=0; i<(len & 3); i++) syslogf(LOGNAME, LOGDATA, "  %.2x", buf[(len &~3) + i]);
}

/* Fills as much of the buffer as it can without blocking */
static os_error *rpc_fillbuffer(char *buf, int buflen, int *currentlen, int *earlyexit, struct conn_info *conn)
{
	int len;

	*earlyexit = 0;

	trigger_callback();
	len = read(conn->sock, buf + *currentlen, buflen - *currentlen);

	if (len == -1) {
		if (errno == EWOULDBLOCK) {
			len = 0;
		} else if (errno == ECONNRESET || errno == ETIMEDOUT) {
			/* The server has closed the connection (probably due
			   to a period of inactivity).
			   Exit early, effectively causing a timeout without
			   the wait. The retry will reconnect the socket. */
			len = 0;
			*earlyexit = 1;
		} else {
			return gen_rpc_error(conn, RPCERRBASE + 7,"Read from socket failed (%s)", xstrerror(errno));
		}
	}

	*currentlen += len;

	return NULL;
}

/* Reads data into the current buffer, and returns the current buffer if the entire reply has been recieved */
static os_error *rpc_readdata(int blocking, int *buffer, struct conn_info *conn)
{
	static int currentrecordoffset;
	static int currentoffset;
	static int recordsize;
	static int lastrecord;
	static int currentrmoffset;
	os_error *err;
	time_t t = clock();
	int earlyexit = 0;

	if (currentbuffer == INVALIDBUFFER) {
		/* Search for a free buffer entry.
		   There should always be at least one */
		for (currentbuffer = freebufferstart; currentbuffer < freebufferstart + FIFOSIZE; currentbuffer++) {
			if (buffers[currentbuffer % FIFOSIZE].len == 0) {
				currentbuffer %= FIFOSIZE;
				break;
			}
		}
		if (currentbuffer >= FIFOSIZE) return gen_rpc_error(conn, RPCERRBASE + 9, "No free rxbuffer found");
		currentoffset = 0;
		currentrecordoffset = 0;
		currentrmoffset = 0;
		if (conn->tcp) {
			recordsize = 0;
			lastrecord = 0;
		} else {
			recordsize = BUFFERSIZE;
			lastrecord = 1;
		}
	}

	do {
		if (recordsize == 0) {
			static char rmbuffer[sizeof(int)];

			/* Read the record marker if this is a tcp stream */
			err = rpc_fillbuffer(rmbuffer, sizeof(int), &currentrmoffset, &earlyexit, conn);
			if (err) return err;

			if (currentrmoffset == sizeof(int)) {
				recordsize  = (rmbuffer[3]);
				recordsize |= (rmbuffer[2]) << 8;
				recordsize |= (rmbuffer[1]) << 16;
				recordsize |= (rmbuffer[0] & 0x7F) << 24;
				lastrecord  = (rmbuffer[0] & 0x80) >> 7;
				currentrmoffset = 0;
			}
		}

		if (recordsize + currentoffset > BUFFERSIZE) return gen_rpc_error(conn, RPCERRBASE + 15,"RPC reply too big");

		if (recordsize > 0) {
			err = rpc_fillbuffer(buffers[currentbuffer].buffer + currentoffset, recordsize, &currentrecordoffset, &earlyexit, conn);
			if (err) return err;
		}

		if (currentrecordoffset >= (conn->tcp ? recordsize : 1)) {
			currentoffset += currentrecordoffset;
			recordsize = 0;
			currentrecordoffset = 0;
		}
	} while (blocking && !earlyexit && !(recordsize == 0 && lastrecord) && (clock() < (t + (conn->timeout * CLOCKS_PER_SEC))));

	if (recordsize == 0 && lastrecord) {
		/* We have read the entire reply */
		buffers[currentbuffer].len = currentoffset;
		*buffer = currentbuffer;
		currentbuffer = INVALIDBUFFER;
	} else {
		/* We have read part, or none, of the reply */
		*buffer = INVALIDBUFFER;
	}
	return NULL;
}

/* Setup buffer and write call header to it */
void rpc_prepare_call(unsigned int prog, unsigned int vers, unsigned int proc, struct conn_info *conn)
{
	struct rpc_msg call_header;

	call_header.body.mtype = CALL;
	call_header.body.u.cbody.rpcvers = RPC_VERSION;
	call_header.body.u.cbody.verf.flavor = AUTH_NULL;
	call_header.body.u.cbody.verf.body.size = 0;

	call_header.xid = nextxid++;
	if (nextxid == UNALLOCATED) nextxid++;
	
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

	obuf = fifo[head].tx.buffer;
	obufend = obuf + BUFFERSIZE;

	/* Leave room for the record marker */
	if (conn->tcp) obuf += 4;

	fifo[head].xid = call_header.xid;
	fifo[head].rx = NULL;

	process_rpc_msg(OUTPUT, &call_header, conn->pool);
}

static os_error *rpc_connect_socket(struct conn_info *conn)
{
	os_error *err;
	int ret;

	if (conn->tcp) {
		err = rpc_close_connection(conn);
		if (err) return err;
		err = rpc_create_socket(conn);
		if (err) return err;
	}

	ret = connect(conn->sock, (struct sockaddr *)&(conn->sockaddr), sizeof(struct sockaddr_in));
	if (ret == -1 && errno != EINPROGRESS) {
		return gen_error(RPCERRBASE + 4, "Connect on socket failed (%s)", xstrerror(errno));
	}
	return NULL;
}

/* Send the already filled in tx buffer, the read the response and process
   the rpc reply header */
os_error *rpc_do_call(int prog, enum callctl calltype, struct conn_info *conn)
{
	os_error *err;
	struct rpc_msg reply_header;

	if (calltype == TXBLOCKING || calltype == TXNONBLOCKING) {
		int port;
		int ret;
		unsigned int recordmarker;

		/* Choose the port to use */
		switch (prog) {
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
		   time we used the socket, or it is not yet connected at all */
		if (port != conn->sockaddr.sin_port || conn->sock == -1) {
			conn->sockaddr.sin_port = port;

			err = rpc_connect_socket(conn);
			if (err) return err;
		}

		fifo[head].tx.len = obuf - fifo[head].tx.buffer;
		fifo[head].retries = 0;
		fifo[head].rx = NULL;

		if (conn->tcp) {
			/* Insert the record marker at the start of the buffer */
			recordmarker = 0x80000000 | (fifo[head].tx.len - 4);
			obuf = fifo[head].tx.buffer;
			obufend = obuf + 4;
			if (process_unsigned(OUTPUT, &recordmarker, conn->pool)) goto buffer_overflow;
		}

		if (enablelog) logdata(0, fifo[head].tx.buffer, fifo[head].tx.len);
		ret = blocking_send(conn->sock, fifo[head].tx.buffer, fifo[head].tx.len, conn->timeout);
		if (ret == -1) {
			if (errno == ENOTCONN) {
				return gen_rpc_error(conn, RPCERRBASE + 4, "Connect on socket failed (connection timed out)");
			} else {
				return gen_rpc_error(conn, RPCERRBASE + 5, "Sending data failed (%s)", xstrerror(errno));
			}
		}

		head++;
		if (head >= FIFOSIZE) head = 0;

		if (fifo[head].xid != UNALLOCATED && fifo[tail].rx == NULL) {
			/* The fifo is now full, so turn this into a blocking call */
			if (calltype == TXNONBLOCKING) calltype = TXBLOCKING;
			if (calltype == RXNONBLOCKING) calltype = RXBLOCKING;
		}
	}

	if (fifo[tail].rx == NULL) {
		/* If we don't have an entry to return straight away then try
		   and read one */

		do {
			int readbuffer;

			err = rpc_readdata((calltype == TXBLOCKING || calltype == RXBLOCKING), &readbuffer, conn);
			if (err) return err;

			if (readbuffer != INVALIDBUFFER) {
				int i;
				int xid;

				if (enablelog) logdata(1, buffers[readbuffer].buffer, buffers[readbuffer].len);

				/* Check to see if it a reply that we are waiting for, and
				   fill in the appropriate fifo entry. Ignore any unknown
				   replies (They could be replies from earlier calls that
				   we retransmitted because of timeouts) */
				ibuf = buffers[readbuffer].buffer;
				ibufend = ibuf + buffers[readbuffer].len;
				if (process_int(INPUT, &xid, conn->pool)) goto buffer_overflow;

				for (i = 0; i < FIFOSIZE; i++) {
					if (xid == fifo[i].xid) {
						fifo[i].rx = &(buffers[readbuffer]);
						break;
					}
				}
				if (i == FIFOSIZE) buffers[readbuffer].len = 0; /* Clear buffer, it wasn't one we were waiting for */
			} else if (calltype == TXBLOCKING || calltype == RXBLOCKING) {
				int ret;

				/* No data recived, so retransmit the oldest fifo entry if it
				   is still pending, unless we are non blocking */

				if (conn->tcp) {
					/* Close and reconnect the socket for TCP connections */
					err = rpc_connect_socket(conn);
					if (err) return err;
				}

				fifo[tail].retries++;
				if (fifo[tail].rx == NULL && fifo[tail].retries > conn->retries) {
					return gen_rpc_error(conn, RPCERRBASE + 8, "Connection timed out");
				}

				if (enablelog) logdata(0, fifo[tail].tx.buffer, fifo[tail].tx.len);
				ret = blocking_send(conn->sock, fifo[tail].tx.buffer, fifo[tail].tx.len, conn->timeout);
				if (ret == -1) {
					return gen_rpc_error(conn, RPCERRBASE + 5, "Sending data failed (%s)", xstrerror(errno));
				}
			}
		} while ((calltype == TXBLOCKING || calltype == RXBLOCKING) && fifo[tail].rx == NULL);
	}

	/* Return if the oldest request has not been recieved. This will only
	   ever be the case if we are non-blocking */
	if (fifo[tail].rx == NULL) return ERR_WOULDBLOCK;

	/* Setup buffers and parse header */
	ibuf = fifo[tail].rx->buffer;
	ibufend = ibuf + fifo[tail].rx->len;
	if (process_rpc_msg(INPUT, &reply_header, conn->pool)) goto buffer_overflow;

	/* Check that the RPC completed successfully */
	if (reply_header.body.mtype != REPLY) return gen_error(RPCERRBASE + 10, "Unexpected response (not an RPC reply)");
	if (reply_header.body.u.rbody.stat != MSG_ACCEPTED) {
		if (reply_header.body.u.rbody.u.rreply.stat == AUTH_ERROR) {
			return gen_error(RPCERRBASE + 11, "RPC message rejected (Authentication error)");
		} else {
			return gen_error(RPCERRBASE + 12, "RPC message rejected (%d)", reply_header.body.u.rbody.u.rreply.stat);
		}
	}
	if (reply_header.body.u.rbody.u.areply.reply_data.stat != SUCCESS) {
		return gen_error(RPCERRBASE + 13, "RPC failed (%d)", reply_header.body.u.rbody.u.areply.reply_data.stat);
	}

	/* Remove this entry from the fifo */
	fifo[tail].rx->len = 0;
	fifo[tail].xid = UNALLOCATED;
	fifo[tail].rx = NULL;
	tail++;
	if (tail >= FIFOSIZE) tail = 0;

	return NULL;

buffer_overflow:
	return rpc_buffer_overflow();
}



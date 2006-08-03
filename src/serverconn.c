/*
	$Id$

	Server connection handling code.


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
#include <ctype.h>


#include "rpc-structs.h"
#include "rpc-process1.h"
#include "rpc-process2.h"

#include "portmapper-decode.h"
#include "mount1-decode.h"
#include "mount3-decode.h"
#include "nfs2-decode.h"
#include "nfs3-decode.h"

#include "moonfish.h"
#include "utils.h"
#include "request-decode.h"
#include "exports.h"
#include "serverconn.h"
#include "filecache.h"
#include "clientid.h"


#define MAXCONNS 5
#define CONNTIMEOUT (5*60*CLOCKS_PER_SEC)


static struct server_conn conns[MAXCONNS];

static int udpsock = -1;
static int tcpsock = -1;
static int nfsudpsock = -1;
static int nfstcpsock = -1;

/* Global memory pool */
static struct pool *gpool = NULL;

static struct export *exports;


static int now;

static int conn_create_socket(int port, int tcp)
{
	int on = 1;
	int sock;

	sock = socket(AF_INET, tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
	if (sock < 0) {
		syslogf(LOGNAME, LOG_SERIOUS, "Unable to open socket (%s)", xstrerror(errno));
		return -1;
	}

	/* Make the socket non-blocking */
	if (ioctl(sock, FIONBIO, &on) < 0) {
		syslogf(LOGNAME, LOG_SERIOUS, "Unable to ioctl (%s)", xstrerror(errno));
		close(sock);
		return -1;
	}

	/* Make the socket generate an event */
	if (ioctl(sock, FIOASYNC, &on) < 0) {
		syslogf(LOGNAME, LOG_SERIOUS, "Unable to ioctl (%s)", xstrerror(errno));
		close(sock);
		return -1;
	}

	/* Prevent bind from failing if we have recently close the connection */
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
		syslogf(LOGNAME, LOG_SERIOUS, "Unable to setsockopt (%s)", xstrerror(errno));
		close(sock);
		return -1;
	}

	struct sockaddr_in name;

	memset(&name, 0, sizeof(name));
	name.sin_family = AF_INET;
	name.sin_addr.s_addr = (int)htonl(INADDR_ANY);

	name.sin_port = htons(port);
	if (bind(sock, (struct sockaddr *)&name, sizeof(name))) {
		syslogf(LOGNAME, LOG_SERIOUS, "Unable to bind socket to local port (%s)", xstrerror(errno));
		close(sock);
		return -1;
	}

	if (tcp && listen(sock, 5)) {
		syslogf(LOGNAME, LOG_SERIOUS, "Unable to listen on socket (%s)", xstrerror(errno));
		close(sock);
		return -1;
	}

	return sock;
}

static void close_conn(struct server_conn *conn)
{
	conn->state = IDLE;
	pclear(conn->pool);
	if (conn->tcp) {
		close(conn->socket);
	}
}

/* Returns non-zero when data was read */
static int udp_read(int sock)
{
	int i;
	int activity = 0;
	int active = 0;

	do {

		for (i = 0; i < MAXCONNS; i++) {
			if (conns[i].state == IDLE) break;
		}
		if (i == MAXCONNS) return activity;

		conns[i].hostaddrlen = sizeof(struct sockaddr);
		conns[i].requestlen = recvfrom(sock, conns[i].request, BUFFERSIZE, 0, (struct sockaddr *)&(conns[i].hostaddr), &(conns[i].hostaddrlen));

		active = 0;
		if (conns[i].requestlen > 0) {
			conns[i].state = DECODE;
			conns[i].socket = sock;
			conns[i].tcp = 0;
			conns[i].time = now;
			conns[i].suppressreply = 0;
			conns[i].host = conns[i].hostaddr.sin_addr.s_addr;
			activity |= 1;
			active = 1;
		} else if (conns[i].requestlen == -1 && errno != EWOULDBLOCK) {
			syslogf(LOGNAME, LOG_ERROR, "Error reading from socket (%s)",xstrerror(errno));
			close_conn(&(conns[i]));
			return activity;
		}
	} while (active);

	return activity;
}

static void udp_write(struct server_conn *conn)
{
	if (conn->replylen > 0) {
		int len;
		len = sendto(conn->socket, conn->reply, conn->replylen, 0, (struct sockaddr *)&(conn->hostaddr), conn->hostaddrlen);
		if (len > 0) {
			conn->replysent = conn->replylen;
		} else if (len == -1 && errno != EWOULDBLOCK) {
			syslogf(LOGNAME, LOG_ERROR, "Error writing to socket (%s)",xstrerror(errno));
			close_conn(conn);
		}
	}
	if (conn->replysent >= conn->replylen) {
		close_conn(conn);
	}
}

/* Returns non-zero when data was read */
static int tcp_accept(int sock)
{
	int i;

	/* Find a free connection entry */
	for (i = 0; i < MAXCONNS; i++) {
		if (conns[i].state == IDLE) break;
	}
	/* If there are no free entries then don't accept any new connections */
	if (i == MAXCONNS) return 0;

	conns[i].hostaddrlen = sizeof(struct sockaddr);
	conns[i].socket = accept(sock, (struct sockaddr *)&(conns[i].hostaddr), &(conns[i].hostaddrlen));
	if (conns[i].socket != -1) {
		conns[i].state = READLEN;
		conns[i].tcp = 1;
		conns[i].time = now;
		conns[i].requestlen = 0;
		conns[i].requestread = 0;
		conns[i].suppressreply = 0;
		conns[i].host = conns[i].hostaddr.sin_addr.s_addr;
		return 1;
	} else if (errno != EWOULDBLOCK) {
		syslogf(LOGNAME, LOG_ERROR, "Error calling accept on socket (%s)",xstrerror(errno));
	}
	return 0;
}

static int tcp_read(struct server_conn *conn)
{
	int len;

	if (conn->state == READLEN) {
		len = 4 - (conn->requestread - conn->requestlen);
		len = read(conn->socket, conn->request + conn->requestread, len);
		if (len == 0) {
			close_conn(conn);
		} else if (len != -1) {
			conn->requestread += len;
			if (conn->requestread - conn->requestlen == 4) {
				conn->requestlen  = (conn->request[3]);
				conn->requestlen |= (conn->request[2]) << 8;
				conn->requestlen |= (conn->request[1]) << 16;
				conn->requestlen |= (conn->request[0] & 0x7F) << 24;
				conn->lastrecord  = (conn->request[0] & 0x80) >> 7;
				conn->requestread -= 4;
				conn->state = READ;
				if (conn->requestread + conn->requestlen > BUFFERSIZE) close_conn(conn);
			}
		} else if (errno != EWOULDBLOCK) {
			syslogf(LOGNAME, LOG_ERROR, "Error reading from socket (%s)",xstrerror(errno));
			close_conn(conn);
		}
	}

	if (conn->state == READ) {
		len = conn->requestlen - conn->requestread;
		len = read(conn->socket, conn->request + conn->requestread, len);
		if (len == 0) {
			close_conn(conn);
		} else if (len != -1) {
			conn->time = now;
			conn->requestread += len;
			if (conn->requestread == conn->requestlen) {
				conn->state = conn->lastrecord ? DECODE : READLEN;
			}
			return 1;
		} else if (errno != EWOULDBLOCK) {
			syslogf(LOGNAME, LOG_ERROR, "Error reading from socket (%s)",xstrerror(errno));
			close_conn(conn);
		}
	}
	return 0;
}

static int tcp_write(struct server_conn *conn)
{
	int activity = 0;

	if (conn->replylen > 0) {
		int len;

		len = write(conn->socket, conn->reply + conn->replysent, conn->replylen - conn->replysent);
		if (len != -1) {
			conn->time = now;
			conn->replysent += len;
			activity = 1;
		} else if (errno != EWOULDBLOCK) {
			syslogf(LOGNAME, LOG_ERROR, "Error writing to socket (%s)",xstrerror(errno));
			close_conn(conn);
			return activity;
		}
	}

	if (conn->replysent >= conn->replylen) {
		conn->time = now;
		conn->requestlen = 0;
		conn->requestread = 0;
		conn->suppressreply = 0;
		conn->state = READLEN;
		pclear(conn->pool);
	}
	return activity;
}

int conn_validsocket(int sock)
{
	int i;

	if (sock == udpsock || sock == tcpsock) return 1;
	if (sock == nfsudpsock || sock == nfstcpsock) return 1;

	for (i = 0; i < MAXCONNS; i++) {
		if (conns[i].tcp && (conns[i].state != IDLE)) {
			if (conns[i].socket == sock) return 1;
		}
	}

	return 0;
}

/* Called from the internet event handler on a broken socket event */
int conn_brokensocket(int sock)
{
	int i;

	for (i = 0; i < MAXCONNS; i++) {
		if (conns[i].tcp && (conns[i].state != IDLE)) {
			if (conns[i].socket == sock) {
				/* We can't do much from within the event
				   handler, so just set the timeout so that
				   the normal cleanup code will get rid of
				   the connection next time it is called */
				conns[i].time -= CONNTIMEOUT + 1;
				return 1;
			}
		}
	}
	return 0;
}

/* Poll all open connections. Returns bit 0 set if there was read or write
   activity, bit 1 set if there was a pending write. Returns 0 if nothing
   is happening. */
int conn_poll(void)
{
	int i;
	now = clock();
	int activity = 0;

	/* Accept any new connections */
	activity |= udp_read(udpsock);
	activity |= udp_read(nfsudpsock);
	activity |= tcp_accept(tcpsock);
	activity |= tcp_accept(nfstcpsock);

	/* Read any data waiting to be read */
	for (i = 0; i < MAXCONNS; i++) {
		if (conns[i].tcp && (conns[i].state == READ || conns[i].state == READLEN)) {
			activity |= tcp_read(&(conns[i]));
		}
	}

	/* Decode any requests that have been fully recieved */
	for (i = 0; i < MAXCONNS; i++) {
		if (conns[i].state == DECODE) {
			conns[i].export = NULL;
			request_decode(&(conns[i]));
			if (conns[i].suppressreply) {
				close_conn(&conns[i]);
			} else {
				conns[i].replysent = 0;
				conns[i].state = WRITE;
			}
		}
	}

	/* Write any data waiting to be sent */
	for (i = 0; i < MAXCONNS; i++) {
		if (conns[i].state == WRITE) {
			activity |= 2;
			if (conns[i].tcp) {
				activity |= tcp_write(&(conns[i]));
			} else {
				udp_write(&(conns[i]));
			}
		}
	}

	/* Reap any connections that time out */
	for (i = 0; i < MAXCONNS; i++) {
		if (conns[i].state != IDLE && (now - conns[i].time) > CONNTIMEOUT) {
			close_conn(&(conns[i]));
		}
	}

	/* Reap any open filehandles that haven't been accessed recently */
	filecache_reap(0);
	clientid_reap(0);

	return activity;
}

int conn_init(void)
{
	int i;

	UR(gpool = pinit(NULL));

	/* Don't quit if the exports file can't be read */
	exports = parse_exports_file(gpool);

	for (i = 0; i < MAXCONNS; i++) {
		conns[i].state = IDLE;
		conns[i].exports = exports;
		/* Allocate buffers, adding an extra 4 for the record marker */
		UR(conns[i].request = palloc(BUFFERSIZE + 4, gpool));
		UR(conns[i].reply = palloc(BUFFERSIZE + 4, gpool));
		UR(conns[i].pool = pinit(gpool));
		conns[i].gpool = gpool;
	}

	filecache_init();
	clientid_init();

	BR(portmapper_set(100000, 2, IPPROTO_UDP, 111,  portmapper_decode, gpool));
	BR(portmapper_set(100000, 2, IPPROTO_TCP, 111,  portmapper_decode, gpool));
	BR(portmapper_set(100005, 1, IPPROTO_UDP, 111,  mount1_decode, gpool));
	BR(portmapper_set(100005, 1, IPPROTO_TCP, 111,  mount1_decode, gpool));
	BR(portmapper_set(100005, 3, IPPROTO_UDP, 111,  mount3_decode, gpool));
	BR(portmapper_set(100005, 3, IPPROTO_TCP, 111,  mount3_decode, gpool));
	BR(portmapper_set(100003, 2, IPPROTO_UDP, 2049, nfs2_decode, gpool));
	BR(portmapper_set(100003, 2, IPPROTO_TCP, 2049, nfs2_decode, gpool));
	BR(portmapper_set(100003, 3, IPPROTO_UDP, 2049, nfs3_decode, gpool));
	BR(portmapper_set(100003, 3, IPPROTO_TCP, 2049, nfs3_decode, gpool));
	BR(portmapper_set(100003, 4, IPPROTO_UDP, 2049, nfs4_decode_proc, gpool));
	BR(portmapper_set(100003, 4, IPPROTO_TCP, 2049, nfs4_decode_proc, gpool));

	udpsock = conn_create_socket(111, 0);
	if (udpsock == -1) goto error;

	tcpsock = conn_create_socket(111, 1);
	if (tcpsock == -1) goto error;

	nfsudpsock = conn_create_socket(2049, 0);
	if (nfsudpsock == -1) goto error;

	nfstcpsock = conn_create_socket(2049, 1);
	if (nfstcpsock == -1) goto error;

	return 0;

error:
	if (udpsock != -1) close(udpsock);
	if (tcpsock != -1) close(tcpsock);
	if (nfsudpsock != -1) close(nfsudpsock);
	if (nfstcpsock != -1) close(nfstcpsock);
	return 1;
}

void conn_close(void)
{
	int i;

	clientid_reap(1);
	filecache_reap(1);

	for (i = 0; i < MAXCONNS; i++) {
		if (conns[i].state != IDLE) {
			close_conn(&(conns[i]));
		}
	}

	if (udpsock != -1) close(udpsock);
	if (tcpsock != -1) close(tcpsock);
	if (nfsudpsock != -1) close(nfsudpsock);
	if (nfstcpsock != -1) close(nfstcpsock);

	if (gpool) pfree(gpool);
}


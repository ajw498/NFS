/*
	$Id$
	$URL$

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

#include "utils.h"
#include "request-decode.h"
#include "exports.h"
#include "serverconn.h"


static int udpsock = -1;
static int tcpsock = -1;


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

void reap_files(int all);

static struct export *exports;

#define MAXCONNS 5

static struct server_conn conns[MAXCONNS];

#define MAXREQUEST 32*1024
/*FIXME*/

static int now;

#define UDPTRANSFERSIZE 4096
#define TCPTRANSFERSIZE 32*1024

/* Returns non-zero when data was read */
static int udp_read(void)
{
	int i;

	for (i = 0; i < MAXCONNS; i++) {
		if (conns[i].state == IDLE) break;
	}
	if (i == MAXCONNS) return 0;

	conns[i].hostaddrlen = sizeof(struct sockaddr);
	conns[i].requestlen = recvfrom(udpsock, conns[i].request, MAXREQUEST, 0, (struct sockaddr *)&(conns[i].hostaddr), &(conns[i].hostaddrlen));

	if (conns[i].requestlen > 0) {
		conns[i].state = DECODE;
		conns[i].socket = udpsock;
		conns[i].tcp = 0;
		conns[i].time = now;
		conns[i].transfersize = UDPTRANSFERSIZE;
		conns[i].suppressreply = 0;
		conns[i].host = conns[i].hostaddr.sin_addr.s_addr;
		return 1;
	} else if (conns[i].requestlen == -1 && errno != EWOULDBLOCK) {
		syslogf(LOGNAME, LOG_ERROR, "Error reading from socket (%s)",xstrerror(errno));
		/* FIXME close connection */
	}
	return 0;
}

/* Returns non-zero when data was read */
static int udp_write(struct server_conn *conn)
{
	int ret = 0;

	if (conn->replylen > 0) {
		int len;
/*		logdata(0, output_buf, buf - output_buf); */
		len = sendto(udpsock, conn->reply, conn->replylen, 0, (struct sockaddr *)&(conn->hostaddr), conn->hostaddrlen);
		if (len > 0) {
			conn->replysent = conn->replylen;
			ret = 1;
		} else if (len == -1 && errno != EWOULDBLOCK) {
			syslogf(LOGNAME, LOG_ERROR, "Error writing to socket (%s)",xstrerror(errno));
			/* FIXME close connection */
		}
	}
	if (conn->replysent >= conn->replylen) {
		conn->state = IDLE;
		pclear(conn->pool);
	}
	return ret;
}

/* Returns non-zero when data was read */
static int tcp_accept(void)
{
	int i;

	for (i = 0; i < MAXCONNS; i++) {
		if (conns[i].state == IDLE) break;
	}
	if (i == MAXCONNS) return 0;

	conns[i].hostaddrlen = sizeof(struct sockaddr);
	conns[i].socket = accept(tcpsock, (struct sockaddr *)&(conns[i].hostaddr), &(conns[i].hostaddrlen));
	if (conns[i].socket != -1) {
		conns[i].state = READLEN;
		conns[i].tcp = 1;
		conns[i].time = now;
		conns[i].transfersize = TCPTRANSFERSIZE;
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

static void tcp_read(struct server_conn *conn)
{
	int len;

	if (conn->state == READLEN) {
		len = 4 - (conn->requestread - conn->requestlen);
		len = read(conn->socket, conn->request + conn->requestread, len);
		if (len != -1) {
			conn->requestread += len;
			if (conn->requestread - conn->requestlen == 4) {
				conn->requestlen  = (conn->request[3]);
				conn->requestlen |= (conn->request[2]) << 8;
				conn->requestlen |= (conn->request[1]) << 16;
				conn->requestlen |= (conn->request[0] & 0x7F) << 24;
				conn->lastrecord  = (conn->request[0] & 0x80) >> 7;
				conn->requestread -= 4;
				conn->state = READ;
			}
		} else if (errno != EWOULDBLOCK) {
			syslogf(LOGNAME, LOG_ERROR, "Error reading from socket (%s)",xstrerror(errno));
			/* FIXME close connection */
		}
	}

	if (conn->state == READ) {
		len = conn->requestlen - conn->requestread;
		len = read(conn->socket, conn->request + conn->requestread, len);
		if (len != -1) {
			conn->time = now;
			conn->requestread += len;
			if (conn->requestread == conn->requestlen) {
				conn->state = conn->lastrecord ? DECODE : READLEN;
			}
		} else if (errno != EWOULDBLOCK) {
			syslogf(LOGNAME, LOG_ERROR, "Error reading from socket (%s)",xstrerror(errno));
			/* FIXME close connection */
		}
	}
}

static void tcp_write(struct server_conn *conn)
{
	if (conn->replylen > 0) {
		int len;

		len = write(conn->socket, conn->reply + conn->replysent, conn->replylen - conn->replysent);
		if (len != -1) {
			conn->time = now;
			conn->replysent += len;
		} else if (errno != EWOULDBLOCK) {
			syslogf(LOGNAME, LOG_ERROR, "Error writing to socket (%s)",xstrerror(errno));
			/* FIXME close connection */
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
}

int conn_validsocket(int sock)
{
	int i;

	if (sock == udpsock || sock == tcpsock) return 1;

	for (i = 0; i < MAXCONNS; i++) {
		if (conns[i].tcp && (conns[i].state != IDLE)) {
			if (conns[i].socket == sock) return 1;
		}
	}

	return 0;
}

int conn_poll(void)
{
	int i;
	now = clock();
	int activity;

	/* Accept any new connections */
	activity = udp_read();
	activity |= tcp_accept();

	/* Read any data waiting to be read */
	for (i = 0; i < MAXCONNS; i++) {
		if (conns[i].tcp && (conns[i].state == READ || conns[i].state == READLEN)) {
			tcp_read(&(conns[i]));
		}
	}

	/* Decode any requests that have been fully recieved */
	for (i = 0; i < MAXCONNS; i++) {
		if (conns[i].state == DECODE) {
			conns[i].export = NULL;
			request_decode(&(conns[i]));
			if (conns[i].suppressreply) {
				conns[i].state = IDLE;
				pclear(conns[i].pool);
				if (conns[i].tcp) close(conns[i].socket);
			} else {
				conns[i].replysent = 0;
				conns[i].state = WRITE;
			}
		}
	}

	/* Write any data waiting to be sent */
	for (i = 0; i < MAXCONNS; i++) {
		if (conns[i].state == WRITE) {
			if (conns[i].tcp) {
				tcp_write(&(conns[i]));
			} else {
				activity |= udp_write(&(conns[i]));
			}
		}
	}

	/* Reap any connections that time out */
	for (i = 0; i < MAXCONNS; i++) {
		if (conns[i].state != IDLE && (now - conns[i].time) > 10*CLOCKS_PER_SEC) {
			conns[i].state = IDLE;
			pclear(conns[i].pool);
			if (conns[i].tcp) {
				shutdown(conns[i].socket, 2);
				close(conns[i].socket);
			}
		}
	}

	/* Reap any open filehandles that haven't been accessed recently */
	reap_files(0);

	return activity ? 0 : 1; /*FIXME*/
}


/* Global memory pool */
static struct pool *gpool = NULL;

typedef enum accept_stat (*decode_proc)(int proc, struct server_conn *conn);
enum bool portmapper_set(int prog, int vers, int prot, int port, decode_proc decode, struct pool *pool);

#define UR(x) do { \
	if ((x) == NULL) { \
		syslogf(LOGNAME, LOG_MEM, OUTOFMEM); \
		return 1; \
	} \
} while (0)

#define BR(x) do { \
	if ((x) == FALSE) return 1; \
} while (0)

int conn_init(void)
{
	int i;

	UR(gpool = pinit(NULL));

	UR(exports = parse_exports_file(gpool));

	for (i = 0; i < MAXCONNS; i++) {
		conns[i].state = IDLE;
		conns[i].exports = exports;
		UR(conns[i].request = palloc(MAXREQUEST + 4, gpool));
		UR(conns[i].reply = palloc(MAXREQUEST + 4, gpool));
		UR(conns[i].pool = pinit(gpool));
		conns[i].gpool = gpool;
	}

	BR(portmapper_set(100000, 2, 6,  111, portmapper_decode, gpool));
	BR(portmapper_set(100000, 2, 17, 111, portmapper_decode, gpool));
	BR(portmapper_set(100005, 1, 6,  111, mount1_decode, gpool));
	BR(portmapper_set(100005, 1, 17, 111, mount1_decode, gpool));
	BR(portmapper_set(100003, 2, 6,  111, nfs2_decode, gpool));
	BR(portmapper_set(100003, 2, 17, 111, nfs2_decode, gpool));

	udpsock = conn_create_socket(111, 0);
	if (udpsock == -1) return 1;

	tcpsock = conn_create_socket(111,1);
	if (tcpsock == -1) {
		close(udpsock);
		return 1;
	}

	return 0;
}

void conn_close(void)
{
	reap_files(1);
	if (udpsock != -1) close(udpsock);
	if (tcpsock != -1) close(tcpsock);
	if (gpool) pfree(gpool);
}


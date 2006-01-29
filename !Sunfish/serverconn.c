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

#include "portmapper-recv.h"
#include "mount1-recv.h"
#include "mount3-recv.h"
#include "nfs2-recv.h"

#include "rpc-decode.h"
#include "exports.h"
#include "serverconn.h"


#define RPCERRBASE 1


static int udpsock = -1;
static int tcpsock = -1;


/* Generate a RISC OS error block based on the given number and message */
static os_error *gen_error(int num, char *msg, ...)
{
	static os_error module_err_buf;
	va_list ap;

	
	va_start(ap, msg);
	vsnprintf(module_err_buf.errmess, sizeof(module_err_buf.errmess), msg, ap);
	va_end(ap);
	module_err_buf.errnum = num;
	return &module_err_buf;
}

static os_error *rpc_create_socket(int port, int tcp)
{
	int on = 1;
	int sock;

	sock = socket(AF_INET, tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
	if (sock < 0) return gen_error(RPCERRBASE + 2,"Unable to open socket (%s)", xstrerror(errno));

/*	if (setsockopt(udpsock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == EOF)  return 0;*/

	/* Make the socket non-blocking */
	if (ioctl(sock, FIONBIO, &on) < 0) {
		return gen_error(RPCERRBASE + 6,"Unable to ioctl (%s)", xstrerror(errno));
	}
/*	ioctl(udpsock, FIOSLEEPTW, &on);*/

	struct sockaddr_in name;

	memset(&name, 0, sizeof(name));
	name.sin_family = AF_INET;
	name.sin_addr.s_addr = (int)htonl(INADDR_ANY);

	name.sin_port = htons(port);
	if (bind(sock, (struct sockaddr *)&name, sizeof(name))) return gen_error(RPCERRBASE + 14, "Unable to bind socket to local port (%s)", xstrerror(errno));

	/* FIXME is backlog of 8 sufficient? */
	if (tcp && listen(sock, 8)) return gen_error(RPCERRBASE + 14, "Unable to listen on socket (%s)", xstrerror(errno));

	if (tcp) tcpsock = sock; else udpsock = sock;

	return NULL;
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
		printf("udpsock read error %d\n",errno);
	}
	return 0;
}

static void udp_write(struct server_conn *conn)
{
	if (conn->replylen > 0) {
		int len;
/*		logdata(0, output_buf, buf - output_buf); */
		len = sendto(udpsock, conn->reply, conn->replylen, 0, (struct sockaddr *)&(conn->hostaddr), conn->hostaddrlen);
		if (len > 0) {
			conn->replysent = conn->replylen;
		}
	}
	if (conn->replysent >= conn->replylen) {
		conn->state = IDLE;
		pclear(conn->pool);
	}
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
		printf("tcpsock accept error %d %s\n",errno,xstrerror(errno));
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
			printf("tcpsock read error %d\n",errno);
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
			printf("tcpsock read error %d\n",errno);
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
			printf("tcpsock write error %d\n",errno);
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

void conn_poll(void)
{
	int i;
	now = clock();

	/* Accept any new connections */
	while (udp_read());
	while (tcp_accept());

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
			rpc_decode(&(conns[i]));
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
				udp_write(&(conns[i]));
			}
		}
	}

	/* Reap any connections that time out */
	for (i = 0; i < MAXCONNS; i++) {
		if (conns[i].state != IDLE && (now - conns[i].time) > 10*CLOCKS_PER_SEC) {
			conns[i].state = IDLE;
			pclear(conns[i].pool);
			if (conns[i].tcp) {
				printf("socket timeout\n");
				shutdown(conns[i].socket, 2);
				close(conns[i].socket);
			}
		}
	}

	/* Reap any open filehandles that haven't been accessed recently */
	reap_files(0);
}


static struct pool *gpool = NULL;

typedef enum accept_stat (*decode_proc)(int proc, struct server_conn *conn);
enum bool portmapper_set(int prog, int vers, int prot, int port, decode_proc decode, struct pool *pool);

int conn_init(void)
{
	int i;
	os_error *err;

	gpool = pinit();
	if (gpool == NULL) {
		printf("Couldn't alloc gpool\n");
		return 1;
	}

	exports = parse_exports_file(gpool);
	if (exports == NULL) {
		printf("Couldn't read any exports\n");
		return 1;
	}

	for (i = 0; i < MAXCONNS; i++) {
		conns[i].state = IDLE;
		conns[i].exports = exports;
		conns[i].request = palloc(MAXREQUEST + 4, gpool);
		conns[i].reply = palloc(MAXREQUEST + 4, gpool);
		conns[i].pool = pinit();
		conns[i].gpool = gpool;
		if (conns[i].pool == NULL || conns[i].request == NULL || conns[i].reply == NULL) {
			printf("Couldn't allocate memory\n");
			return 1;
		}
	}

	portmapper_set(100000, 2, 6,  111, portmapper_decode, gpool);
	portmapper_set(100000, 2, 17, 111, portmapper_decode, gpool);
	portmapper_set(100005, 1, 6,  111, mount1_decode, gpool);
	portmapper_set(100005, 1, 17, 111, mount1_decode, gpool);
	portmapper_set(100003, 2, 6,  111, nfs2_decode, gpool);
	portmapper_set(100003, 2, 17, 111, nfs2_decode, gpool);

	err = rpc_create_socket(111,0);
	if (err) {
		if (udpsock != -1) close(udpsock);
		if (tcpsock != -1) close(tcpsock);
		printf("err udp: %s\n",err->errmess);
		return 1;
	}
	err = rpc_create_socket(111,1);
	if (err) {
		if (udpsock != -1) close(udpsock);
		if (tcpsock != -1) close(tcpsock);
		printf("err tcp: %s\n",err->errmess);
		return 1;
	}
	return 0;
}

void conn_close(void)
{
	reap_files(1);
	if (udpsock != -1) close(udpsock);
	if (tcpsock != -1) close(tcpsock);
	/*free_exports(exports);
	pfree(gpool);*/
}


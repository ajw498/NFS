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
#include <ctype.h>
#include <unistd.h>


#include "rpc-structs.h"
#include "rpc-process.h"

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


#define CONNTIMEOUT (5*60*CLOCKS_PER_SEC)


static struct server_conn *conns = NULL;

static int udpsock = -1;
static int tcpsock = -1;
static int nfsudpsock = -1;
static int nfstcpsock = -1;

/* Global memory pool */
static struct pool *gpool = NULL;

static struct export *exports;

struct choices choices = {
	.encoding = NULL,
	.portmapper = 1,
	.nfs2 = 1,
	.nfs3 = 1,
	.nfs4 = 1,
	.udp = 1,
	.tcp = 1,
	.toutf8 = (iconv_t)-1,
	.fromutf8 = (iconv_t)-1,
	.toenc = (iconv_t)-1,
	.fromenc = (iconv_t)-1,
	.macforks = 0,
};

static int now;

static struct server_conn *new_conn(void)
{
	struct server_conn *conn = conns;

	/* Search for idle connections to reuse */
	while (conn) {
		if (conn->state == IDLE) return conn;
		conn = conn->next;
	}

	/* None found, so allocate a new one */
	UU(conn = malloc(sizeof(struct server_conn)));
	conn->state = IDLE;
	conn->exports = exports;
	conn->request = NULL;
	conn->reply = NULL;
	conn->pool = NULL;

	/* Allocate buffers, adding an extra 4 for the record marker */
	if ((conn->request = malloc(MFBUFFERSIZE + 4)) == NULL) goto err;
	if ((conn->reply = malloc(MFBUFFERSIZE + 4)) == NULL) goto err;
	if ((conn->pool = pinit(NULL)) == NULL) goto err;
	conn->gpool = gpool;

	conn->next = conns;
	conns = conn;
	return conn;

err:
	if (conn->request) free(conn->request);
	if (conn->reply) free(conn->reply);
	if (conn->pool) pfree(conn->pool);
	return NULL;
}

static void free_conn(struct server_conn *conn)
{
	free(conn->request);
	free(conn->reply);
	pfree(conn->pool);
	free(conn);
}

static void release_conn(struct server_conn *freeconn, int leaveidle)
{
	struct server_conn *conn = conns;
	struct server_conn **prevconn = &conns;
	struct server_conn **prev = NULL;
	int numidle = 0;

	if (freeconn->tcp) {
		close(freeconn->socket);
	}

	/* Search for idle connections to reuse */
	while (conn) {
		if (conn->state == IDLE) numidle++;
		if (conn == freeconn) prev = prevconn;
		prevconn = &(conn->next);
		conn = conn->next;
	}

	if (numidle >= leaveidle) {
		/* Remove from list and free */
		*prev = freeconn->next;
		free_conn(freeconn);
	} else {
		/* Leave in list to be reused later */
		pclear(freeconn->pool);
		freeconn->state = IDLE;
	}
}

static void close_conn(struct server_conn *freeconn)
{
	release_conn(freeconn, 2);
}

static void close_all_conns(void)
{
	while (conns) release_conn(conns, 0);
}

static int conn_create_socket(int port, int tcp)
{
	int on = 1;
	int sock;

	sock = socket(AF_INET, tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
	if (sock < 0) {
		syslogf(LOGNAME, LOG_SERIOUS, "Unable to open socket (%s)", strerror(errno));
		return -1;
	}

	/* Make the socket non-blocking */
	if (ioctl(sock, FIONBIO, &on) < 0) {
		syslogf(LOGNAME, LOG_SERIOUS, "Unable to ioctl (%s)", strerror(errno));
		close(sock);
		return -1;
	}

	/* Make the socket generate an event */
	if (ioctl(sock, FIOASYNC, &on) < 0) {
		syslogf(LOGNAME, LOG_SERIOUS, "Unable to ioctl (%s)", strerror(errno));
		close(sock);
		return -1;
	}

	/* Prevent bind from failing if we have recently close the connection */
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
		syslogf(LOGNAME, LOG_SERIOUS, "Unable to setsockopt (%s)", strerror(errno));
		close(sock);
		return -1;
	}

	struct sockaddr_in name;

	memset(&name, 0, sizeof(name));
	name.sin_family = AF_INET;
	name.sin_addr.s_addr = (int)htonl(INADDR_ANY);

	name.sin_port = htons(port);
	if (bind(sock, (struct sockaddr *)&name, sizeof(name))) {
		syslogf(LOGNAME, LOG_SERIOUS, "Unable to bind socket to local port (%s)", strerror(errno));
		close(sock);
		return -1;
	}

	if (tcp && listen(sock, 5)) {
		syslogf(LOGNAME, LOG_SERIOUS, "Unable to listen on socket (%s)", strerror(errno));
		close(sock);
		return -1;
	}

	return sock;
}

/* Returns non-zero when data was read */
static int udp_read(int sock)
{
	int activity = 0;
	int active = 0;

	if (sock == -1) return 0;

	do {

		struct server_conn *conn = new_conn();
		if (conn == NULL) {
			syslogf(LOGNAME, LOG_MEM, OUTOFMEM);
			return activity;
		}

		conn->hostaddrlen = sizeof(struct sockaddr);
		conn->requestlen = recvfrom(sock, conn->request, MFBUFFERSIZE, 0, (struct sockaddr *)&(conn->hostaddr), &(conn->hostaddrlen));

		active = 0;
		if (conn->requestlen > 0) {
			conn->state = DECODE;
			conn->socket = sock;
			conn->tcp = 0;
			conn->time = now;
			conn->suppressreply = 0;
			conn->host = conn->hostaddr.sin_addr.s_addr;
			activity |= 1;
			active = 1;
		} else if (conn->requestlen == -1 && errno != EWOULDBLOCK) {
			syslogf(LOGNAME, LOG_ERROR, "Error reading from socket (%s)",strerror(errno));
			close_conn(conn);
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
			syslogf(LOGNAME, LOG_ERROR, "Error writing to socket (%s)",strerror(errno));
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
	if (sock == -1) return 0;

	struct server_conn *conn = new_conn();
	if (conn == NULL) {
		syslogf(LOGNAME, LOG_MEM, OUTOFMEM);
		return 0;
	}

	conn->hostaddrlen = sizeof(struct sockaddr);
	conn->socket = accept(sock, (struct sockaddr *)&(conn->hostaddr), &(conn->hostaddrlen));
	if (conn->socket != -1) {
		conn->state = READLEN;
		conn->tcp = 1;
		conn->time = now;
		conn->requestlen = 0;
		conn->requestread = 0;
		conn->suppressreply = 0;
		conn->host = conn->hostaddr.sin_addr.s_addr;
		return 1;
	} else if (errno != EWOULDBLOCK) {
		syslogf(LOGNAME, LOG_ERROR, "Error calling accept on socket (%s)",strerror(errno));
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
				if (conn->requestread + conn->requestlen > MFBUFFERSIZE) {
					syslogf(LOGNAME, LOG_ERROR, "Request size bigger than %d, dropping connection",MFBUFFERSIZE);
					close_conn(conn);
				}
			}
		} else if (errno != EWOULDBLOCK) {
			syslogf(LOGNAME, LOG_ERROR, "Error reading from socket (%s)",strerror(errno));
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
			syslogf(LOGNAME, LOG_ERROR, "Error reading from socket (%s)",strerror(errno));
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
			syslogf(LOGNAME, LOG_ERROR, "Error writing to socket (%s)",strerror(errno));
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
	struct server_conn *conn = conns;

	if (sock == udpsock || sock == tcpsock) return 1;
	if (sock == nfsudpsock || sock == nfstcpsock) return 1;

	while (conn) {
		if ((conn->state != IDLE) && conn->tcp) {
			if (conn->socket == sock) return 1;
		}
		conn = conn->next;
	}

	return 0;
}

/* Called from the internet event handler on a broken socket event */
int conn_brokensocket(int sock)
{
	struct server_conn *conn = conns;

	while (conn) {
		if ((conn->state != IDLE) && conn->tcp) {
			if (conn->socket == sock) {
				/* We can't do much from within the event
				   handler, so just set the timeout so that
				   the normal cleanup code will get rid of
				   the connection next time it is called */
				conn->time -= CONNTIMEOUT + 1;
				return 1;
			}
		}
		conn = conn->next;
	}
	return 0;
}

/* Poll all open connections. Returns bit 0 set if there was read or write
   activity, bit 1 set if there was a pending write. Returns 0 if nothing
   is happening. */
int conn_poll(void)
{
	now = clock();
	int activity = 0;
	struct server_conn *conn = conns;

	/* Accept any new connections */
	activity |= udp_read(udpsock);
	activity |= udp_read(nfsudpsock);
	activity |= tcp_accept(tcpsock);
	activity |= tcp_accept(nfstcpsock);

	/* Read any data waiting to be read */
	while (conn) {
		struct server_conn *next = conn->next;
		if ((conn->state == READ || conn->state == READLEN) && conn->tcp) {
			activity |= tcp_read(conn);
		}
		conn = next;
	}

	/* Decode any requests that have been fully received */
	conn = conns;
	while (conn) {
		struct server_conn *next = conn->next;
		if (conn->state == DECODE) {
			conn->export = NULL;
			request_decode(conn);
			if (conn->suppressreply) {
				close_conn(conn);
			} else {
				conn->replysent = 0;
				conn->state = WRITE;
			}
		}
		conn = next;
	}

	/* Write any data waiting to be sent */
	conn = conns;
	while (conn) {
		struct server_conn *next = conn->next;
		if (conn->state == WRITE) {
			activity |= 2;
			if (conn->tcp) {
				activity |= tcp_write(conn);
			} else {
				udp_write(conn);
			}
		}
		conn = next;
	}

	/* Reap any connections that time out */
	conn = conns;
	while (conn) {
		struct server_conn *next = conn->next;
		if (conn->state != IDLE && (now - conn->time) > CONNTIMEOUT) {
			close_conn(conn);
		}
		conn = next;
	}

	/* Reap any open filehandles that haven't been accessed recently */
	filecache_reap(0);
	clientid_reap(0);

	return activity;
}

#define CHECK(str) (strncasecmp(opt,str,sizeof(str) - 1)==0)

static enum bool read_choices(struct pool *pool)
{
	char line[1024];
	FILE *file;

	file = fopen(CHOICESREAD, "r");
	if (file == NULL) {
		syslogf(LOGNAME, LOG_SERIOUS, "Unable to open choices file (%s)", CHOICESREAD);
		return TRUE; /* Don't prevent startup, just use defaults */
	}

	while (fgets(line, sizeof(line), file)) {
		char *ch = line;
		char *opt;
		char *val;

		while (isspace(*ch)) ch++;
		if (*ch == '#') *ch = '\0';
		opt = ch;
		while (*ch && (*ch != ':')) ch++;
		if (*ch) *ch++ = '\0';
		while (isspace(*ch)) ch++;
		val = ch;
		while (*ch && isprint(*ch)) ch++;
		*ch = '\0';

		if (CHECK("Encoding")) {
			if (*val) {
				choices.encoding = pstrdup(val, pool);
				if (choices.encoding == NULL) {
					syslogf(LOGNAME, LOG_MEM, OUTOFMEM);
					return FALSE;
				}
			}
		} else if (CHECK("NFS2")) {
			choices.nfs2 = atoi(val);
		} else if (CHECK("NFS3")) {
			choices.nfs3 = atoi(val);
		} else if (CHECK("NFS4")) {
			choices.nfs4 = atoi(val);
		} else if (CHECK("Portmapper")) {
			choices.portmapper = atoi(val);
		} else if (CHECK("UDP")) {
			choices.udp = atoi(val);
		} else if (CHECK("TCP")) {
			choices.tcp = atoi(val);
		} else if (CHECK("Macforks")) {
			choices.macforks = atoi(val);
		}
	}
	fclose(file);

	if ((choices.udp || choices.tcp) == 0) {
		syslogf(LOGNAME, LOG_SERIOUS, "Both UDP and TCP disabled, nothing to do");
		return FALSE;
	}
	if (choices.nfs2 || choices.nfs3) choices.portmapper = 1;
	if ((choices.nfs2 || choices.nfs3 || choices.nfs4 || choices.portmapper) == 0) {
		syslogf(LOGNAME, LOG_SERIOUS, "No NFS protocols enabled, nothing to do");
		return FALSE;
	}
	return TRUE;
}

#define open_encoding(dest, to, from) do { \
	dest = iconv_open(to, from); \
	if (dest == (iconv_t)-1) { \
		syslogf(LOGNAME, LOG_SERIOUS, "Iconv unable to convert encodings from %s to %s  (%d)", from, to, errno); \
		goto error; \
	} \
} while (0)

int conn_init(void)
{
	char *local;

	UR(gpool = pinit(NULL));

	/* Don't quit if the exports file can't be read */
	exports = parse_exports_file(gpool);

	filecache_init();

	BR(read_choices(gpool));

	if (choices.portmapper) BR(portmapper_set(100000, 2, IPPROTO_UDP, 111,  portmapper_decode, gpool));
	if (choices.portmapper) BR(portmapper_set(100000, 2, IPPROTO_TCP, 111,  portmapper_decode, gpool));
	if (choices.udp && choices.nfs2) BR(portmapper_set(100005, 1, IPPROTO_UDP, 111,  mount1_decode, gpool));
	if (choices.tcp && choices.nfs2) BR(portmapper_set(100005, 1, IPPROTO_TCP, 111,  mount1_decode, gpool));
	if (choices.udp && choices.nfs3) BR(portmapper_set(100005, 3, IPPROTO_UDP, 111,  mount3_decode, gpool));
	if (choices.tcp && choices.nfs3) BR(portmapper_set(100005, 3, IPPROTO_TCP, 111,  mount3_decode, gpool));
	if (choices.udp && choices.nfs2) BR(portmapper_set(100003, 2, IPPROTO_UDP, 2049, nfs2_decode, gpool));
	if (choices.tcp && choices.nfs2) BR(portmapper_set(100003, 2, IPPROTO_TCP, 2049, nfs2_decode, gpool));
	if (choices.udp && choices.nfs3) BR(portmapper_set(100003, 3, IPPROTO_UDP, 2049, nfs3_decode, gpool));
	if (choices.tcp && choices.nfs3) BR(portmapper_set(100003, 3, IPPROTO_TCP, 2049, nfs3_decode, gpool));
	if (choices.udp && choices.nfs4) BR(portmapper_set(100003, 4, IPPROTO_UDP, 2049, nfs4_decode_proc, gpool));
	if (choices.tcp && choices.nfs4) BR(portmapper_set(100003, 4, IPPROTO_TCP, 2049, nfs4_decode_proc, gpool));

	if (choices.portmapper) {
		udpsock = conn_create_socket(111, 0);
		if (udpsock == -1) goto error;
	}

	if (choices.portmapper) {
		tcpsock = conn_create_socket(111, 1);
		if (tcpsock == -1) goto error;
	}

	if (choices.udp) {
		nfsudpsock = conn_create_socket(2049, 0);
		if (nfsudpsock == -1) goto error;
	}

	if (choices.tcp) {
		nfstcpsock = conn_create_socket(2049, 1);
		if (nfstcpsock == -1) goto error;
	}

	local = encoding_getlocal();
	if (choices.nfs4) {
		if (strcmp(local, "UTF-8") != 0) {
			open_encoding(choices.toutf8, "UTF-8", local);
			open_encoding(choices.fromutf8, local, "UTF-8");
		}
	}
	if ((choices.nfs2 || choices.nfs3) && choices.encoding) {
		if (strcmp(choices.encoding, local) != 0) {
			open_encoding(choices.toenc, choices.encoding, local);
			open_encoding(choices.fromenc, local, choices.encoding);
		}
	}

	return 0;

error:
	if (udpsock != -1) close(udpsock);
	if (tcpsock != -1) close(tcpsock);
	if (nfsudpsock != -1) close(nfsudpsock);
	if (nfstcpsock != -1) close(nfstcpsock);
	if (choices.toutf8 != (iconv_t)-1)   iconv_close(choices.toutf8);
	if (choices.fromutf8 != (iconv_t)-1) iconv_close(choices.fromutf8);
	if (choices.toenc != (iconv_t)-1)    iconv_close(choices.toenc);
	if (choices.fromenc != (iconv_t)-1)  iconv_close(choices.fromenc);
	return 1;
}

void conn_close(void)
{
	filecache_savegrace();
	clientid_reap(1);
	filecache_reap(1);

	close_all_conns();

	if (udpsock != -1) close(udpsock);
	if (tcpsock != -1) close(tcpsock);
	if (nfsudpsock != -1) close(nfsudpsock);
	if (nfstcpsock != -1) close(nfstcpsock);
	if (choices.toutf8 != (iconv_t)-1)   iconv_close(choices.toutf8);
	if (choices.fromutf8 != (iconv_t)-1) iconv_close(choices.fromutf8);
	if (choices.toenc != (iconv_t)-1)    iconv_close(choices.toenc);
	if (choices.fromenc != (iconv_t)-1)  iconv_close(choices.fromenc);

	if (gpool) pfree(gpool);
}


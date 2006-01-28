/*
	$Id$
	$URL$

	Prototype server code that can run in a taskwindow rather than as a module.


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

#include "rpc-decode.h"

/* The worstcase size of a header for read/write calls.
   If this value is not big enough, the data part will not be of
   an optimum size, but nothing bad will happen */
#define MAX_HDRLEN 416

#define MAX_DATABUFFER 32000 /*FIXME*/

/* The size to use for tx and rx buffers */
#define BUFFERSIZE (MAX_HDRLEN + MAX_DATABUFFER)

typedef _kernel_oserror os_error;

#define RPCERRBASE 1



/* Buffer to allocate linked list structures from. Should be significanly
   faster than using the RMA, doesn't require freeing each element of
   the list, and won't fragment */
static char malloc_buffer[MAX_DATABUFFER];
/* The start of the next location to return for linked list malloc */
static char *nextmalloc;


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


/* Log an entire data packet */
static void logdata(int rx, char *buf, int len)
{
	int i;

	printf("%s data (%d): xid %.2x%.2x%.2x%.2x\n", rx ? "rx" : "tx", len, buf[0], buf[1], buf[2], buf[3]);
	for (i=0; i<(len & ~3); i+=4) printf("  %.2x %.2x %.2x %.2x\n", buf[i], buf[i+1], buf[i+2], buf[i+3]);
	for (i=0; i<(len & 3); i++) printf("  %.2x\n", buf[(len &~3) + i]);
}



/* Allocate a chunk from the linklist buffer */
void *llmalloc(int size)
{
	void *mem;

	abort();

	if (nextmalloc + size > malloc_buffer + sizeof(malloc_buffer)) {
		mem = NULL;
	} else {
		mem = nextmalloc;
		nextmalloc += size;
	}

	return mem;
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

#define CHECK(str) (strncasecmp(opt,str,sizeof(str) - 1)==0)


static struct export *parse_line(char *line)
{
	char *basedir;
	char *exportname;
	char *host;
	struct export *export;
	static int exportnum = 0;
	char *maskstart;

	while (isspace(*line)) line++;
	if (*line == '#' || *line == '\0') return NULL;
	basedir = line;
	while (*line && !isspace(*line)) line++;
	if (*line) *line++ = '\0';
	while (isspace(*line)) line++;
	exportname = line;
	while (*line && !isspace(*line)) line++;
	if (*line) *line++ = '\0';
	while (isspace(*line)) line++;
	host = line;
	while (*line && *line != '(') line++;
	if (*line) *line++ = '\0';

	export = malloc(sizeof(struct export));
	if (export == NULL) {
		/*FIXME*/
		return NULL;
	}

	/* Set defaults */
	export->ro = 1;
	export->matchuid = 1;
	export->uid = 0;
	export->matchuid = 1;
	export->uid = 0;
	export->imagefs = 0;
	export->defaultfiletype = 0xFFF;
	export->xyzext = NEEDED;
	export->next = NULL;
	export->pathentry = NULL;
	memset(export->hosts, 0, sizeof(unsigned int) * MAXHOSTS);
	export->pool = pinit();
	if (export->pool == NULL) {
		/*FIXME*/
		free(export);
		return NULL;
	}

	export->basedir = strdup(basedir);
	if (export->basedir == NULL) {
		/*FIXME*/
		pfree(export->pool);
		free(export);
		return NULL;
	}
	export->basedirlen = strlen(basedir);
	export->exportnum = exportnum++;
	/*FIXME if exportnum > 128 then error */

	export->exportname = strdup(exportname);
	if (export->basedir == NULL) {
		/*FIXME*/
		free(export->basedir);
		pfree(export->pool);
		free(export);
		return NULL;
	}

	maskstart = strchr(host, '/');
	if (maskstart) {
		int maskbits;

		*maskstart++ = '\0';
		maskbits = atoi(maskstart);
		export->mask = 0xFFFFFFFF >> (32 - maskbits);
	} else {
		export->mask = 0xFFFFFFFF;
	}

	if (strcmp(host, "*") == 0) {
		export->host = 0;
		export->mask = 0;
	} else {
		struct hostent *hp;
		if (gethostbyname_timeout(host, 5, &hp)) {
			printf("couldn't resolve\n");
			abort();
		}
		memcpy(&(export->host), hp->h_addr, hp->h_length);
	}

	while (*line) {
		char *opt;
		while (isspace(*line)) line++;
		opt = line;
		while (*line && *line != ')' && *line != ',') line++;
		if (*line) *line++ = '\0';
		if (CHECK("rw")) {
			export->ro = 0;
		} else if (CHECK("ro")) {
			export->ro = 1;
		} else if (CHECK("uid") && opt[3] == '=') {
			export->matchuid = 1;
			export->uid = atoi(opt + 4);
		} else if (CHECK("gid") && opt[3] == '=') {
			export->matchgid = 1;
			export->gid = atoi(opt + 4);
		} else if (CHECK("imagefs")) {
			export->imagefs = 1;
		} else if (CHECK("defaultfiletype") && opt[15] == '=') {
			export->defaultfiletype = (int)strtol(opt + 16, NULL, 16);
		} else if (CHECK("addext") && opt[6] == '=') {
			export->xyzext = atoi(opt + 7);
		}
	}

	return export;
}

static struct export *parse_config_file(void)
{
	char line[1024];
	FILE *file;
	struct export *exports = NULL;

	file = fopen("Choices:Moonfish.exports","r");
	if (file == NULL) {
		printf("Can't open exports file\n");
		return NULL;
	}

	while (fgets(line, sizeof(line), file)) {
		struct export *export;

		export = parse_line(line);
		if (export) {
			export->next = exports;
			exports = export;
		}
	}
	fclose(file);

	return exports;
}

static void free_exports(struct export *export)
{
	while (export) {
		struct export *next;

		free(export->basedir);
		free(export->exportname);
		next = export->next;
		free(export);
		export = next;
	}
}

void reap_files(int all);

static struct export *exports;

#define MAXCONNS 5

struct server_conn conns[MAXCONNS];

#define MAXREQUEST 32*1024
/*FIXME*/

static int now;

#define UDPTRANSFERSIZE 4096
#define TCPTRANSFERSIZE 32*1024

/* Returns non-zero when data was read */
int udp_read(void)
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
		conns[i].host = conns[i].hostaddr.sin_addr.s_addr;
		return 1;
	} else if (conns[i].requestlen == -1 && errno != EWOULDBLOCK) {
		printf("udpsock read error %d\n",errno);
	}
	return 0;
}

void udp_write(struct server_conn *conn)
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
int tcp_accept(void)
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
		conns[i].host = conns[i].hostaddr.sin_addr.s_addr;
		return 1;
	} else if (errno != EWOULDBLOCK) {
		printf("tcpsock accept error %d\n",errno);
	}
	return 0;
}

void tcp_read(struct server_conn *conn)
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
			conn->requestread += len;
			if (conn->requestread == conn->requestlen) {
				conn->state = conn->lastrecord ? DECODE : READLEN;
			}
		} else if (errno != EWOULDBLOCK) {
			printf("tcpsock read error %d\n",errno);
		}
	}
}

void tcp_write(struct server_conn *conn)
{
	if (conn->replylen > 0) {
		int len;

		len = write(conn->socket, conn->reply + conn->replysent, conn->replylen - conn->replysent);
		if (len != -1) {
			conn->replysent += len;
		} else if (errno != EWOULDBLOCK) {
			printf("tcpsock write error %d\n",errno);
		}
	}

	if (conn->replysent >= conn->replylen) {
		conn->state = IDLE;
		pclear(conn->pool);
		close(conn->socket);
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
			conns[i].replysent = 0;
			conns[i].state = WRITE;
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
			if (conns[i].tcp) close(conns[i].socket);
		}
	}

	/* Reap any open filehandles that haven't been accessed recently */
	reap_files(0);
}


struct pool *gpool = NULL;

int main(void)
{
	int i;

	gpool = pinit();
	if (gpool == NULL) {
		printf("Couldn't alloc gpool\n");
		return 1;
	}

	exports = parse_config_file();
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
		if (conns[i].pool == NULL || conns[i].request == NULL || conns[i].reply == NULL) {
			printf("Couldn't allocate memory\n");
			return 1;
		}
	}

	rpc_create_socket(111,0);
	rpc_create_socket(111,1);

	while (1) {
		int ch;

		_swix(OS_Byte, _INR(0,2) | _OUT(2), 129, 10, 0, &ch);
		if (ch != 255) break;
		conn_poll();
	}
	reap_files(1);
	if (udpsock != -1) close(udpsock);
	if (tcpsock != -1) close(tcpsock);
	free_exports(exports);
	return 0;
}


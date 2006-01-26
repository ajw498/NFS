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


static int sock = -1;


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

	sock = socket(AF_INET, tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
	if (sock < 0) return gen_error(RPCERRBASE + 2,"Unable to open socket (%s)", xstrerror(errno));

/*	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == EOF)  return 0;*/

	/* Make the socket non-blocking */
/*	if (ioctl(sock, FIONBIO, &on) < 0) {
		return gen_error(RPCERRBASE + 6,"Unable to ioctl (%s)", xstrerror(errno));
	}*/
	ioctl(sock, FIOSLEEPTW, &on);

	struct sockaddr_in name;

	memset(&name, 0, sizeof(name));
	name.sin_family = AF_INET;
	name.sin_addr.s_addr = (int)htonl(INADDR_ANY);

	name.sin_port = htons(port);
	if (bind(sock, (struct sockaddr *)&name, sizeof(name))) return gen_error(RPCERRBASE + 14, "Unable to bind socket to local port (%s)", xstrerror(errno));

	/* FIXME is backlog of 8 sufficient? */
	if (tcp && listen(sock, 8)) return gen_error(RPCERRBASE + 14, "Unable to listen on socket (%s)", xstrerror(errno));

/*FIXME close sock on error? */
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

#define CHECK(str) (strncasecmp(opt,str,sizeof(str))==0)

int calc_fileid(char *path, char *leaf);

static struct export *parse_line(char *line)
{
	char *basedir;
	char *exportname;
	char *host;
	struct export *export;
	static int exportnum = 0;

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
	export->host = 0;
	export->mask = 0xFFFFFFFF;
	export->imagefs = 0;
	export->next = NULL;
	export->pathentry = NULL;
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
	export->basedirhash = calc_fileid(basedir, NULL) & 0xFF;
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

int main(void)
{
	int len;
	char tmp_buf[32*1024];
	struct sockaddr host;
	int addrlen;
	struct server_conn conn;

	conn.transfersize = 4096;
	conn.pool = pinit();
	conn.exports = parse_config_file();
	if (conn.exports == NULL) {
		printf("Couldn't read any exports\n");
		return 1;
	}

	rpc_create_socket(111,0);
/*	sock = accept(sock, &host, &addrlen);
	len = read(sock, tmp_buf , 1024);*/
	while (1) {

		addrlen = sizeof(struct sockaddr);
		len = recvfrom(sock, tmp_buf, 32*1024, 0, &host, &addrlen);
		printf("%d bytes read\n",len);
		if (len <= 0) break;
		conn.request = tmp_buf;
		conn.requestlen = len;
		conn.export = NULL;
		rpc_decode(&conn);
		if (conn.reply) {
/*			logdata(0, output_buf, buf - output_buf); */
			sendto(sock, conn.reply, conn.replylen, 0, &host, addrlen);
		}
	}
	if (sock != -1) close(sock);
	free_exports(conn.exports);
	return 0;
}


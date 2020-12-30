/*
	Server connection handling code.


	Copyright (C) 2006 Alex Waugh

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/


#ifndef SERVERCONN_H
#define SERVERCONN_H


#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <iconv.h>

#include "pools.h"

enum conn_state {
	IDLE,
	READLEN,
	READ,
	DECODE,
	WRITE
};

struct choices {
	char *encoding;
	int portmapper;
	int nfs2;
	int nfs3;
	int nfs4;
	int udp;
	int tcp;
	iconv_t toutf8;
	iconv_t fromutf8;
	iconv_t toenc;
	iconv_t fromenc;
	int macforks;
};

extern struct choices choices;

struct server_conn {
	struct export *export;
	struct export *exports;
	enum conn_state state;
	int tcp;
	int socket;
	struct sockaddr_in hostaddr;
#ifdef USE_TCPIPLIBS
	int hostaddrlen;
#else
	socklen_t hostaddrlen;
#endif
	clock_t time;
	int uid;
	int gid;
	unsigned int host;
	struct pool *pool;
	struct pool *gpool;
	char *request;
	int requestlen;
	int requestread;
	int lastrecord;
	char *reply;
	int replylen;
	int replysent;
	int suppressreply;
	int nfs4;
	struct server_conn *next;
};

int conn_init(void);

int conn_validsocket(int sock);

int conn_brokensocket(int sock);

int conn_poll(void);

void conn_close(void);

#endif


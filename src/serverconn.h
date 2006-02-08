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


#ifndef SERVERCONN_H
#define SERVERCONN_H


#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "pools.h"

enum conn_state {
	IDLE,
	READLEN,
	READ,
	DECODE,
	WRITE
};

struct server_conn {
	struct export *export;
	struct export *exports;
	enum conn_state state;
	int tcp;
	int socket;
	struct sockaddr_in hostaddr;
	int hostaddrlen;
	clock_t time;
	int transfersize;
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
};

int conn_init(void);

int conn_validsocket(int sock);

int conn_poll(void);

void conn_close(void);

#endif


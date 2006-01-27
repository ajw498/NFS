/*
	$Id$
	$URL$

	Functions to implement decoding of recieved RPC calls.


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

#ifndef RPC_DECODE_H
#define RPC_DECODE_H

#include "pools.h"
#include "sunfish.h"

#define PATHENTRIES 32

struct pathentry {
	char *name;
	struct pathentry *next;
};

/* Define the maximum numer of hosts to remember for mount dump */
#define MAXHOSTS 10


struct export {
	char *basedir;
	size_t basedirlen;
	int exportnum;
	char *exportname;
	int ro;
	int matchuid;
	int uid;
	int matchgid;
	int gid;
	unsigned int host;
	unsigned int mask;
	unsigned int hosts[MAXHOSTS];
	int imagefs;
	int defaultfiletype;
	int xyzext;
	struct pool *pool;
	struct pathentry *pathentry;
	struct export *next;
};

struct server_conn {
	struct export *export;
	struct export *exports;
	int tcp;
	int transfersize;
	int uid;
	int gid;
	unsigned int host;
	struct pool *pool;
	char *request;
	int requestlen;
	char *reply;
	int replylen;
};

void *llmalloc(int size);

void init_output(struct server_conn *conn);

void rpc_decode(struct server_conn *conn);

#endif


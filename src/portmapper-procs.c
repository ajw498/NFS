/*
	$Id$
	$URL$

	Portmapper procedures


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

#include "portmapper-procs.h"

typedef enum accept_stat (*decode_proc)(int proc, struct server_conn *conn);

struct program {
	struct mapping map;
	decode_proc decode;
	struct program *next;
};

static struct program *programs = NULL;

enum bool portmapper_set(int prog, int vers, int prot, int port, decode_proc decode, struct pool *pool)
{
	struct program *program = programs;

	while (program) {
		if (program->map.prog == prog &&
		    program->map.vers == vers &&
		    program->map.prot == prot) {
			return FALSE;
		}
		program = program->next;
	}
	program = palloc(sizeof(struct program), pool);
	if (program == NULL) {
		syslogf(LOGNAME, LOG_MEM, OUTOFMEM);
		return FALSE;
	}
	program->map.prog = prog;
	program->map.vers = vers;
	program->map.prot = prot;
	program->map.port = port;
	program->decode = decode;
	program->next = programs;
	programs = program;
	return TRUE;
}

static enum bool portmapper_unset(int prog, int vers)
{
	struct program *program = programs;
	struct program **prev = &programs;
	enum bool res = FALSE;

	while (program) {
		if (program->map.prog == prog &&
		    program->map.vers == vers) {
			res = TRUE;
			*prev = program->next;
		} else {
			prev = &(program->next);
		}
		program = program->next;
	}

	return res;
}

enum accept_stat portmapper_decodebody(int prog, int vers, int proc, int *hi, int *lo, struct server_conn *conn)
{
	struct program *program = programs;
	enum accept_stat res = PROG_UNAVAIL;

	*hi = 0;
	*lo = 100;

	while (program) {
		if (program->map.prog == prog) {
			res = PROG_MISMATCH;
			if (program->map.vers == vers && program->decode) {
				return program->decode(proc, conn);
			} else if (program->map.vers > *hi) {
				*hi = program->map.vers;
			} else if (program->map.vers < *lo) {
				*lo = program->map.vers;
			}
		}
		program = program->next;
	}
	return res;
}


enum accept_stat PMAPPROC_NULL(struct server_conn *conn)
{
	return SUCCESS;
}

enum accept_stat PMAPPROC_SET(struct mapping *args, enum bool *res, struct server_conn *conn)
{
	/* Only allow localhost to register mappings */
	if (conn->host == 0x0100007F) {
		*res = portmapper_set(args->prog, args->vers, args->prot, args->port, NULL, conn->gpool);
	} else {
		*res = FALSE;
	}
	return SUCCESS;
}

enum accept_stat PMAPPROC_UNSET(struct mapping *args, enum bool *res, struct server_conn *conn)
{
	/* Only allow localhost to deregister mappings */
	if (conn->host == 0x0100007F) {
		*res = portmapper_unset(args->prog, args->vers);
	} else {
		*res = FALSE;
	}
	return SUCCESS;
}

enum accept_stat PMAPPROC_GETPORT(struct mapping *args, int *res, struct server_conn *conn)
{
	struct program *program = programs;

	while (program) {
		if (program->map.prog == args->prog &&
		    program->map.vers == args->vers &&
		    program->map.prot == args->prot) {
			*res = program->map.port;
			return SUCCESS;
		}
		program = program->next;
	}

	*res = 0;
	return SUCCESS;
}

enum accept_stat PMAPPROC_DUMP(struct pmaplist2 *res, struct server_conn *conn)
{
	struct program *program = programs;
	struct pmaplist *list;

	res->list = NULL;

	while (program) {
		list = palloc(sizeof(struct pmaplist), conn->pool);
		if (list == NULL) break;
		list->map = program->map;
		list->next = res->list;
		res->list = list;
		program = program->next;
	}

	return SUCCESS;
}

#define MAX_UDPBUFFER 4096

enum accept_stat PMAPPROC_CALLIT(struct call_args *args, struct call_result *res, struct server_conn *conn)
{
	char *oldibuf = ibuf;
	char *oldibufend = ibufend;
	char *oldobuf = obuf;
	char *oldobufend = obufend;
	char *buf;
	int hi;
	int lo;
	enum accept_stat ret = PROG_UNAVAIL;
	static int count = 0;

	if (count || conn->tcp) {
		/* Prevent recursive callit calls */
		conn->suppressreply = 1;
		return ret;
	}
	count++;

	ibuf = args->args.data;
	ibufend = ibuf + args->args.size;
	buf = palloc(MAX_UDPBUFFER, conn->pool);
	if (buf == NULL) {
		conn->suppressreply = 1;
	} else {
		obuf = buf;
		obufend = obuf + MAX_UDPBUFFER;
		ret = portmapper_decodebody(args->prog, args->vers, args->proc, &hi, &lo, conn);
		if (ret == SUCCESS) {
			res->port = 111;
			res->res.data = buf;
			res->res.size = obuf - buf;
		} else {
			conn->suppressreply = 1;
		}
	}

	ibuf = oldibuf;
	ibufend = oldibufend;
	obuf = oldobuf;
	obufend = oldobufend;

	count--;

	return ret;
}


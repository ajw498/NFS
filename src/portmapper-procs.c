/*
	Portmapper procedures


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

#include "moonfish.h"
#include "portmapper-procs.h"
#include "request-decode.h"
#include "pools.h"
#include "utils.h"


struct program {
	struct mapping map;
	decode_proc decode;
	struct program *next;
};

static struct program *programs = NULL;

/* Returns true if successful, false if it failed or the program was already registered */
enum bool portmapper_set(int prog, int vers, int prot, int port, decode_proc decode, struct pool *pool)
{
	struct program *program = programs;

	/* Check the program isn't already registered */
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

/* Returns false if program not registered */
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
	*lo = 0x7FFFFFFF;

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
	(void) conn;

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

enum accept_stat PMAPPROC_GETPORT(struct mapping *args, unsigned *res, struct server_conn *conn)
{
	struct program *program = programs;

	(void)conn;

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
		/* Prevent non-UDP and recursive callit calls */
		conn->suppressreply = 1;
		return ret;
	}
	count++;

	ibuf = args->args.data;
	ibufend = ibuf + args->args.size;
	buf = palloc(MFBUFFERSIZE, conn->pool);
	if (buf == NULL) {
		conn->suppressreply = 1;
	} else {
		obuf = buf;
		obufend = obuf + MFBUFFERSIZE;
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


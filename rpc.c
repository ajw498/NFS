/*
	$Id$

	RPC calling functions
*/

#include "rpc-calls.h"
#include "portmapper-calls.h"
#include "mount-calls.h"
#include "nfs-calls.h"
#include "rpc.h"


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


static unsigned int xid;

static struct rpc_msg call_header;
static struct rpc_msg reply_header;
static char tx_buffer[8192];
static char rx_buffer1[8192];
static char rx_buffer2[8192];
static char *rx_buffer = rx_buffer1;
static char malloc_buffer[8192];
static char *nextmalloc;
static char *tx_buffer_end = tx_buffer + sizeof(tx_buffer);
/*static char *rx_buffer_end = rx_buffer + sizeof(rx_buffer);*/
char *buf, *bufend;

void swap_rxbuffers(void)
{
	rx_buffer = rx_buffer == rx_buffer1 ? rx_buffer2 : rx_buffer1;
}

/* Initialise parts of the header that are the same for all calls */
void rpc_init_header(void)
{
	xid = 1;
/*	printf("init xid = %d\n",xid); */
	call_header.body.mtype = CALL;
	call_header.body.u.cbody.rpcvers = RPC_VERSION;
	call_header.body.u.cbody.cred.flavor = AUTH_UNIX;
	call_header.body.u.cbody.verf.flavor = AUTH_NULL;
	call_header.body.u.cbody.verf.body.size = 0;
}

/* Setup buffer and write call header to it */
void rpc_prepare_call(unsigned int prog, unsigned int vers, unsigned int proc, struct conn_info *conn)
{
	conn = conn;
	call_header.xid = xid++;
	call_header.body.u.cbody.prog = prog;
	call_header.body.u.cbody.vers = vers;
	call_header.body.u.cbody.proc = proc;
	call_header.body.u.cbody.cred.body.size = conn->authsize;
	call_header.body.u.cbody.cred.body.data = conn->auth;
	buf = tx_buffer;
	bufend = tx_buffer_end;
	nextmalloc = malloc_buffer;
	process_struct_rpc_msg(OUTPUT, call_header, 0);
buffer_overflow: /* Should be impossible, but... */
	return;
}

os_error *rpc_init_connection(struct conn_info *conn)
{
	struct hostent *hp;

	conn->sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (conn->sock < 0) return gen_error(RPCERRBASE + 0,"Unable to open socket (%s)", xstrerror(errno));

	hp = gethostbyname(conn->server);
	if (hp == NULL) return gen_error(RPCERRBASE + 1, "Unable to resolve hostname '%s' (%s)", conn->server, xstrerror(errno));

	memcpy(&(conn->sockaddr.sin_addr), hp->h_addr, hp->h_length);
	conn->sockaddr.sin_family = AF_INET;
	conn->sockaddr.sin_port = 0;
	return NULL;
}

os_error *rpc_close_connection(struct conn_info *conn)
{
	if (socketclose(conn->sock)) return gen_error(RPCERRBASE + 2, "Socketclose failed (%s)", xstrerror(errno));
	return NULL;
}

os_error *rpc_do_call(struct conn_info *conn)
{
	int len = 0;
	int port;
	int tries = 0;
	int ret;

	switch (call_header.body.u.cbody.prog) {
	case PMAP_RPC_PROGRAM:
		port = htons(conn->portmapper_port);
		break;
	case MOUNT_RPC_PROGRAM:
		port = htons(conn->mount_port);
		break;
	case NFS_RPC_PROGRAM:
	default:
		port = htons(conn->nfs_port);
		break;
	}

	if (port != conn->sockaddr.sin_port) {
		conn->sockaddr.sin_port = port;
		if (connect(conn->sock, (struct sockaddr *)&(conn->sockaddr), sizeof(struct sockaddr_in)) < 0) {
			return gen_error(RPCERRBASE + 3, "Connect on socket failed (%s)", xstrerror(errno));
		}
	}

	do {
		fd_set rfds;
		struct timeval tv;

		if (send(conn->sock, tx_buffer, buf - tx_buffer, 0) == -1) {
			return gen_error(RPCERRBASE + 4, "Sending data failed (%s)", xstrerror(errno));
		}

		FD_ZERO(&rfds);
		FD_SET(conn->sock, &rfds);
		tv.tv_sec = conn->timeout;
		tv.tv_usec = 0;
		ret = select(conn->sock + 1, &rfds, NULL, NULL, &tv);
		if (ret == -1) return gen_error(RPCERRBASE + 5, "Select on socket failed (%s)", xstrerror(errno));
		if (ret > 0) {
			len = socketread(conn->sock, rx_buffer, sizeof(rx_buffer1));
			if (len == -1) return gen_error(RPCERRBASE + 6,"Read from socket failed (%s)", xstrerror(errno));

			buf = rx_buffer;
			bufend = rx_buffer + len;
			/* Check len > header size? */
			process_struct_rpc_msg(INPUT, reply_header, 0);

			if (reply_header.xid < call_header.xid) {
				/* Must be a duplicate from an earlier timeout */
				ret = 0;
				tries--;
			}
		}
	} while ((ret == 0) && (tries++ < conn->retries));
	if (ret == 0) return gen_error(RPCERRBASE + 7, "Connection timed out");

	if (reply_header.xid != call_header.xid) return gen_error(RPCERRBASE + 8, "Unexpected response (xid mismatch)");
	if (reply_header.body.mtype != REPLY) return gen_error(RPCERRBASE + 9, "Unexpected response (not an rpc reply)");
	if (reply_header.body.u.rbody.stat != MSG_ACCEPTED) {
		if (reply_header.body.u.rbody.u.rreply.stat == AUTH_ERROR) {
			return gen_error(RPCERRBASE + 10, "RPC message rejected (Authentication error)");
		} else {
			return gen_error(RPCERRBASE + 11, "RPC message rejected");
		}
	}
	if (reply_header.body.u.rbody.u.areply.reply_data.stat != SUCCESS) {
		return gen_error(RPCERRBASE + 12, "RPC failed (%d)", reply_header.body.u.rbody.u.areply.reply_data.stat);
	}
	return NULL;

buffer_overflow:
	return rpc_buffer_overflow();
}

void *llmalloc(size_t size)
{
	void *mem = nextmalloc;
	nextmalloc += size; /* error checking? */
	return mem;
}

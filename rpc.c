/*
	$Id: $

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
#include <netinet/in.h>
#include <netdb.h>
#include <socklib.h>
#include <time.h>


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
static char auth_unix[] = {0,0,0,23, 0,0,0,7, 'c','a','r','a','m','e','l',0, 0,0,0,0, 0,0,0,0, 0,0,0,1, 0,0,0,0};

void swap_rxbuffers(void)
{
	rx_buffer = rx_buffer == rx_buffer1 ? rx_buffer2 : rx_buffer1;
}

/* Initialise parts of the header that are the same for all calls */
void rpc_init_header(void)
{
	xid = time(NULL);
/*	printf("init xid = %d\n",xid); */
	call_header.body.mtype = CALL;
	call_header.body.u.cbody.rpcvers = RPC_VERSION;
	call_header.body.u.cbody.cred.flavor = AUTH_UNIX; /**/
	call_header.body.u.cbody.cred.body.size = sizeof(auth_unix);
	call_header.body.u.cbody.cred.body.data = auth_unix;
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
	buf = tx_buffer;
	bufend = tx_buffer_end;
	nextmalloc = malloc_buffer;
	process_struct_rpc_msg(OUTPUT, call_header, 0);
buffer_overflow: /* Should be impossible, but... */
	return;
}

os_error err_buf = {1, ""};

#define rpc_error(msg) do { \
 strcpy(err_buf.errmess,msg); \
 return &err_buf; \
} while (0)

os_error *rpc_do_call(struct conn_info *conn)
{
	int sock;
	struct sockaddr_in name;
	struct hostent *hp;
	int len;
	int port;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) rpc_error("opening dgram socket");

	hp = gethostbyname(conn->server);
	if (hp == NULL) rpc_error("Unable to resolve hostname");

	memcpy(&name.sin_addr, hp->h_addr, hp->h_length);
	name.sin_family = AF_INET;
	switch (call_header.body.u.cbody.prog) {
	case PMAP_RPC_PROGRAM:
		port = conn->portmapper_port;
		break;
	case MOUNT_RPC_PROGRAM:
		port = conn->mount_port;
		break;
	case NFS_RPC_PROGRAM:
		port = conn->nfs_port;
		break;
	default:
		rpc_error("Unknown rpc program");
	}
	name.sin_port = htons(port);
	if (connect(sock, (struct sockaddr *)&name, sizeof(name)) < 0) rpc_error("connect failed");

/*	{
		char *i;
		printf("call:\n");
		for (i = tx_buffer; i < buf; i++) {
			printf("0x%X ",*i);
			if (((int)i)%4 == 3) printf("\n");
		}
	}*/

	if (send(sock, tx_buffer, buf - tx_buffer, 0) == -1) rpc_error("send failed");

	len = socketread(sock, rx_buffer, sizeof(rx_buffer1));
	if (len == -1) rpc_error("socketread failed");

/*	{
		int i;
		printf("reply:\n");
		for (i = 0; i < len; i++) {
			printf("0x%X ",rx_buffer[i]);
			if (i%4 == 3) printf("\n");
		}
		printf("\n");
	}  */

	if (socketclose(sock)) rpc_error("socketclose failed");

	buf = rx_buffer;
	bufend = rx_buffer + len;
	process_struct_rpc_msg(INPUT, reply_header, 0);

	/*printf("xid = %d\n",reply_header.xid);*/
	if (reply_header.body.mtype != REPLY) {
		/*printf("type = %d\n",reply_header.body.mtype);*/
		rpc_error("Not a reply");
	}
	if (reply_header.body.u.rbody.stat == MSG_ACCEPTED) {
		/*printf("msg accepted\n");*/
	} else {
		/*printf("reply_stat = %d\n",reply_header.body.u.rbody.stat);*/
		rpc_error("msg denied");
	}
	if (reply_header.body.u.rbody.u.areply.reply_data.stat == SUCCESS) {
		/*printf("call successfull\n");*/
	} else {
		/*printf("accept_stat = %d\n",reply_header.body.u.rbody.u.areply.reply_data.stat);*/
		rpc_error("call failed");
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

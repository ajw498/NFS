/*
	$Id: $

	RPC calling functions
*/

#include "rpc-calls.h"
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

#define RPC_VERSION 2

static unsigned int xid;

static struct rpc_msg call_header;
static struct rpc_msg reply_header;
static char tx_buffer[8192];
static char rx_buffer[8192];
static char *tx_buffer_end = tx_buffer + sizeof(tx_buffer);
static char *rx_buffer_end = rx_buffer + sizeof(rx_buffer);
char *buf, *bufend;
static char auth_unix[] = {0,0,0,23, 0,0,0,7, 'c','a','r','a','m','e','l',0, 0,0,0,0, 0,0,0,0, 0,0,0,1, 0,0,0,0};


void init_header(void)
{
	xid = time(NULL);
	printf("init xid = %d\n",xid);
	call_header.body.mtype = CALL;
	call_header.body.u.cbody.rpcvers = RPC_VERSION;
	call_header.body.u.cbody.cred.flavor = AUTH_UNIX; /**/
	call_header.body.u.cbody.cred.body.size = sizeof(auth_unix);
	call_header.body.u.cbody.cred.body.data = auth_unix;
	call_header.body.u.cbody.verf.flavor = AUTH_NULL; /**/
	call_header.body.u.cbody.verf.body.size = 0;
}

void prepare_call(unsigned int prog, unsigned int vers, unsigned int proc, struct conn_info *conn)
{
	call_header.xid = xid++;
	call_header.body.u.cbody.prog = prog;
	call_header.body.u.cbody.vers = vers;
	call_header.body.u.cbody.proc = proc;
	buf = tx_buffer;
	bufend = tx_buffer_end;
	process_struct_rpc_msg(OUTPUT, call_header, 0);
}

#define rpc_error(msg) do { \
	printf("%s\n",msg); \
	exit(1); \
} while (0)

int do_call(struct conn_info *conn)
{
	int sock;
	struct sockaddr_in name;
	struct hostent *hp;
	int len;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("opening dgram socket");
		exit(1);
	}

	hp = gethostbyname("mint");
	if (hp == NULL) {
		printf("unknown host");
		exit(1);
	}
	memcpy(&name.sin_addr, hp->h_addr, hp->h_length);
	name.sin_family = AF_INET;
	name.sin_port = htons(call_header.body.u.cbody.prog == 100005 ? 1027 : call_header.body.u.cbody.prog == 100000 ? 111 : 2049);
	if (connect(sock, (struct sockaddr *)&name, sizeof(name)) < 0) {
		printf("connect errno=%d\n",errno);
		exit(1);
	}

/*	{
		char *i;
		printf("call:\n");
		for (i = tx_buffer; i < buf; i++) {
			printf("0x%X ",*i);
			if (((int)i)%4 == 3) printf("\n");
		}
	}*/
	if (send(sock, tx_buffer, buf - tx_buffer, 0) == -1) {
		printf("send errno=%d\n",errno);
		/*perror("sending datagram message");*/
		exit(1);
	}
	len = socketread(sock, rx_buffer, sizeof(rx_buffer));
	if (len == -1) {
		printf("read errno=%d\n",errno);
	} else {
		int i;
		printf("reply:\n");
		for (i = 0; i < len; i++) {
			printf("0x%X ",rx_buffer[i]);
			if (i%4 == 3) printf("\n");
		}
		printf("\n");
	}
	socketclose(sock);

	buf = rx_buffer;
	bufend = rx_buffer + len;
	process_struct_rpc_msg(INPUT, reply_header, 0);
	printf("xid = %d\n",reply_header.xid);
	if (reply_header.body.mtype != REPLY) {
		printf("type = %d\n",reply_header.body.mtype);
		rpc_error("Not a reply");
	}
	if (reply_header.body.u.rbody.stat == MSG_ACCEPTED) {
		printf("msg accepted\n");
	} else {
		printf("reply_stat = %d\n",reply_header.body.u.rbody.stat);
		rpc_error("msg denied");
	}
	if (reply_header.body.u.rbody.u.areply.reply_data.stat == SUCCESS) {
		printf("call successfull\n");
	} else {
		printf("accept_stat = %d\n",reply_header.body.u.rbody.u.areply.reply_data.stat);
		rpc_error("call failed");
	}
	return 0;
}

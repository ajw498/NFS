/*
	$Id: $

	RPC calling functions
*/

#include nfsstructs.h
#include processstructs.h
#include processunions.h

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <socklib.h>

#define RPC_VERSION 2

static unsigned int xid = 0;

static struct rpc_msg call_header;
static struct rpc_msg reply_header;
static char tx_buffer[8192];
static char rx_buffer[8192];
static char *tx_buffer_end = tx_buffer + sizeof(tx_buffer);
static char *rx_buffer_end = rx_buffer + sizeof(rx_buffer);

void init_header(void)
{
	call_header.body.u.cbody.mtype = CALL;
	call_header.body.u.cbody.rpcvers = RPC_VERSION;
	call_header.body.u.cbody.proc = NFS_RPC_PROGRAM;
	call_header.body.u.cbody.vers = NFS_RPC_VERSION;
	call_header.body.u.cbody.cred.flavor = AUTH_NULL; /**/
	call_header.body.u.cbody.cred.body.size = 0;
	call_header.body.u.cbody.verf.flavor = AUTH_NULL; /**/
	call_header.body.u.cbody.verf.body.size = 0;
}

void prepare_call(unsigned int proc)
{
	call_header.xid = xid++;
	call_header.body.u.cbody.proc = proc;
	buf = tx_buffer;
	bufend = tx_buffer_end;
	process_struct_rpc_msg(OUTPUT,call_header);
}

int do_call(void)
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
	name.sin_port = htons(2049);
	if (connect(sock, (struct sockaddr *)&name, sizeof(name)) < 0) {
		printf("connect errno=%d\n",errno);
		exit(1);
	}

	if (send(sock, tx_buffer, sizeof(buf - tx_buffer), 0) == -1) {
		printf("send errno=%d\n",errno);
		/*perror("sending datagram message");*/
		exit(1);
	}
	len = socketread(sock, rx_buffer, sizeof(rx_buffer));
	if (len == -1) {
		printf("read errno=%d\n",errno);
		//(0 && perror("sending datagram message"));
	} else {
		int i;
		for (i = 0; i < len; i++) {
			printf("0x%X ",buf[i]);
			if (i%4 == 3) printf("\n");
		}
		printf("\n");
	}
	socketclose(sock);

	buf = rx_buffer;
	bufend = rx_buffer_end;
}


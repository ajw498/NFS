#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <socklib.h>

#define DATA "hello"

static char buf[1024];

struct rpc_msg {
	int xid;
	int mtype;
	struct {
		int rpcvers;
		int prog;
		int vers;
		int proc;
		int authtype;
		int authlen;
		int verftype;
		int verflen;
	} cbody;
};

static struct rpc_msg data;


int main(void)
{
	int sock;
	struct sockaddr_in name;
	struct hostent *hp;
	int len;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("opening dgram socket");
		return 1;
	}

	hp = gethostbyname("mint");
	if (hp == NULL) {
		printf("unknown host");
		return 1;
	}
	memcpy(&name.sin_addr, hp->h_addr, hp->h_length);
	name.sin_family = AF_INET;
	name.sin_port = htons(2049);
	if (connect(sock, (struct sockaddr *)&name, sizeof(name)) < 0) {
		printf("connect errno=%d\n",errno);
		return 1;
	}

	data.xid = (int)htonl(55);
	data.mtype = (int)htonl(0);

	data.cbody.rpcvers = (int)htonl(2);
	data.cbody.prog = (int)htonl(100003);
	data.cbody.vers = (int)htonl(3);
	data.cbody.proc = (int)htonl(0);

	data.cbody.authtype = 0;
	data.cbody.authlen = 0;
	data.cbody.verftype = 0;
	data.cbody.verflen = 0;

	if (send(sock, &data, sizeof(data), 0) == -1) {
		printf("send errno=%d\n",errno);
		/*perror("sending datagram message");*/
		return 1;
	}
	len = socketread(sock, buf, sizeof(buf));
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
}


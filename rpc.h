/*
	$Id: $

	RPC calling functions
*/

#ifndef RPC_H
#define RPC_H

#include "kernel.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define FHSIZE 32
/**/

typedef _kernel_oserror os_error;  /* Neatify */

struct conn_info {
	char *server;
	unsigned int portmapper_port;
	unsigned int mount_port;
	unsigned int nfs_port;
	/*struct auth_unix auth;  */
	char *export;
	char rootfh[FHSIZE];
	char *config;
	struct sockaddr_in sockaddr;
	int sock;
	long timeout;
	int retries;
};

void rpc_init_header(void);

void rpc_prepare_call(unsigned int prog, unsigned int vers, unsigned int proc, struct conn_info *conn);

os_error *rpc_do_call(struct conn_info *conn);

extern os_error err_buf;
#define rpc_buffer_overflow() (strcpy(err_buf.errmess,"Buffer overflow"),&err_buf)

void *llmalloc(size_t size);

void swap_rxbuffers(void);

os_error *rpc_init_connection(struct conn_info *conn);
os_error *rpc_close_connection(struct conn_info *conn);

#endif

/*
	$Id: $

	RPC calling functions
*/

#ifndef RPC_H
#define RPC_H

#include "kernel.h"

typedef _kernel_oserror os_error;  /* Neatify */

struct conn_info {
	char *server;
	unsigned int portmapper_port;
	unsigned int mount_port;
	unsigned int nfs_port;
	/*struct auth_unix auth;  */
	char *export;
};

void rpc_init_header(void);

void rpc_prepare_call(unsigned int prog, unsigned int vers, unsigned int proc, struct conn_info *conn);

os_error *rpc_do_call(struct conn_info *conn);

extern os_error err_buf;
#define rpc_buffer_overflow() (strcpy(err_buf.errmess,"Buffer overflow"),&err_buf)

#endif

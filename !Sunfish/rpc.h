/*
	$Id$
	$URL$

	RPC calling functions
*/

#ifndef RPC_H
#define RPC_H

#include "imageentry_common.h"


void rpc_init_header(void);

void rpc_prepare_call(unsigned int prog, unsigned int vers, unsigned int proc, struct conn_info *conn);

os_error *rpc_do_call(struct conn_info *conn);

#define rpc_buffer_overflow() gen_error(RPCBUFOVERFLOW, RPCBUFOVERFLOWMESS)

void *llmalloc(size_t size);

void swap_rxbuffers(void);

os_error *rpc_init_connection(struct conn_info *conn);

os_error *rpc_close_connection(struct conn_info *conn);



#endif

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

enum callctl {
	TXBLOCKING,    /* Transmit request and block until a reply is recieved */
	RXBLOCKING,    /* Block until a reply is recieved, do not transmit anything */
	TXNONBLOCKING, /* Transmit request, return a reply only if one is available */
	RXNONBLOCKING  /* Unused */
};

#define ERR_WOULDBLOCK ((os_error *)1)

os_error *rpc_do_call(struct conn_info *conn, enum callctl calltype);

void rpc_resetfifo(void);

#define rpc_buffer_overflow() gen_error(RPCBUFOVERFLOW, RPCBUFOVERFLOWMESS)

void *llmalloc(size_t size);

void swap_rxbuffers(void);

os_error *rpc_init_connection(struct conn_info *conn);

os_error *rpc_close_connection(struct conn_info *conn);



#endif

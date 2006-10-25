/*
	$Id$

	RPC calling functions
*/

#ifndef RPC_H
#define RPC_H

#include "utils.h"


os_error *rpc_prepare_call(unsigned int prog, unsigned int vers, unsigned int proc, struct conn_info *conn);

enum callctl {
	TXBLOCKING,    /* Transmit request and block until a reply is recieved */
	RXBLOCKING,    /* Block until a reply is recieved, do not transmit anything */
	TXNONBLOCKING, /* Transmit request, return a reply only if one is available */
	RXNONBLOCKING  /* Unused */
};

#define ERR_WOULDBLOCK ((os_error *)1)

os_error *rpc_do_call(int prog, enum callctl calltype, void *extradata, int extralen, struct conn_info *conn);

void rpc_free_request_entry(struct conn_info *conn);

void rpc_free_all_buffers(void);

#define rpc_buffer_overflow() gen_error(RPCBUFOVERFLOW, RPCBUFOVERFLOWMESS)

void rpc_hold_rxbuffer(void);

char *rpc_get_last_host(void);

os_error *rpc_init_connection(struct conn_info *conn);

os_error *rpc_close_connection(struct conn_info *conn);



#endif

/*
	RPC calling functions

        Copyright (C) 2003 Alex Waugh

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef RPC_H
#define RPC_H

#include "utils.h"
#include "clientconn.h"

os_error *rpc_prepare_call(unsigned int prog, unsigned int vers, unsigned int proc, struct conn_info *conn);

enum callctl {
	TXBLOCKING,    /* Transmit request and block until a reply is received */
	RXBLOCKING,    /* Block until a reply is received, do not transmit anything */
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

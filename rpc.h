/*
	$Id: $

	RPC calling functions
*/

#ifndef RPC_H
#define RPC_H

struct conn_info {
	struct opaque_auth *cred;
	int sock; /*?*/
};

void init_header(void);

void prepare_call(unsigned int prog, unsigned int vers, unsigned int proc, struct conn_info *conn);

int do_call(struct conn_info *conn);

#endif

/*
	$Id: $

	Bodgetastic readdir fns
*/

#include "nfs-calls.h"
#include "readdir.h"


/* A special case to read a linked list */
os_error *NFSPROC_READDIR_entry(struct readdirok_entry *res, struct conn_info *conn)
{
	conn = conn;
	process_union_readdirok_entry(INPUT, (*res), 0);
	return NULL;
buffer_overflow:
	return rpc_buffer_overflow();
}

os_error *NFSPROC_READDIR_eof(struct readdirok *res, struct conn_info *conn)
{
	conn = conn;
	process_struct_readdirok(INPUT, (*res), 0);
	return NULL;
buffer_overflow:
	return rpc_buffer_overflow();
}


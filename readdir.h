/*
	$Id: $

	Bodgetastic readdir fns
*/

/*#include "nfs-calls.h"*/


int NFSPROC_READDIR_entry(struct readdirok_entry *res, struct conn_info *conn);
int NFSPROC_READDIR_eof(struct readdirok *res, struct conn_info *conn);

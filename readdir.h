/*
	$Id: $

	Bodgetastic readdir fns
*/

/*#include "nfs-calls.h"*/


os_error *NFSPROC_READDIR_entry(struct readdirok_entry *res, struct conn_info *conn);
os_error *NFSPROC_READDIR_eof(struct readdirok *res, struct conn_info *conn);

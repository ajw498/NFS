/*
	$Id: $

	RPC calling functions
*/

#include "nfs-calls.h"
#include "mount-calls.h"
#include "rpc.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
	string dir = {11,"/nfssandbox"};
	struct fhstatus res;
	init_header();
	NFSPROC3_NULL(NULL);
	MNTPROC_EXPORT(NULL,NULL);
	MNTPROC_MNT(&dir, &res, NULL);
	return 0;
}


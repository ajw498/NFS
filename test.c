/*
	$Id: $

	RPC calling functions
*/

#include "nfs-calls.h"
#include "mount-calls.h"
#include "rpc.h"
#include "readdir.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
	string dir = {11,"/nfssandbox"};
	struct fhstatus res;
	struct readdirargs rddir;
	struct readdirres rdres;
	struct readdirok_entry entry;
	struct readdirok eof;
	init_header();
	NFSPROC3_NULL(NULL);
	/*MNTPROC_EXPORT(NULL,NULL);*/
	MNTPROC_MNT(&dir, &res, NULL);
	memcpy(rddir.dir, res.u.directory, FHSIZE);
	rddir.count = 1024;
	rddir.cookie = 0;
	NFSPROC_READDIR(&rddir, &rdres, NULL);
	do {
		NFSPROC_READDIR_entry(&entry,NULL);
		if (entry.opted) {
			int i;
			/*printf("%d\n",entry.u.u.name.size);*/
			for (i=0;i<entry.u.u.name.size;i++) {
				printf("%c",entry.u.u.name.data[i]);
			}
			printf("\n");
		}
	} while (entry.opted);
	NFSPROC_READDIR_eof(&eof, NULL);
	printf("eof=%d\n",eof.eof);
/*	rddir.cookie = 12;
	NFSPROC_READDIR(&rddir, &rdres, NULL);
	rddir.cookie = 24;
	NFSPROC_READDIR(&rddir, &rdres, NULL); */
	return 0;
}


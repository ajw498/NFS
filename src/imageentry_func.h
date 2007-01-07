/*
	$Id$

	Routines for ImageEntry_Func
*/


#include "imageentry_common.h"



os_error *func_newimage(unsigned int fileswitchhandle, char *config, struct conn_info **myhandle);

void free_conn_info(struct conn_info *conn);

ENTRYFUNCDECL(os_error *, func_newimage_mount, (struct conn_info *conn));

ENTRYFUNCDECL(os_error *, func_closeimage, (struct conn_info *conn));

ENTRYFUNCDECL(os_error *, func_readdirinfo, (int info, char *dirname, char *buffer, int numobjs, int start, int buflen, struct conn_info *conn, int *objsread, int *continuepos));

ENTRYFUNCDECL(os_error *, func_rename, (char *oldfilename, char *newfilename, struct conn_info *conn, int *renamefailed));

ENTRYFUNCDECL(os_error *, func_free, (char *filename, struct conn_info *conn, unsigned *freelo, unsigned *freehi, unsigned *biggestobj, unsigned *sizelo, unsigned *sizehi, unsigned *usedlo, unsigned *usedhi));

os_error *func_readboot(int *option);


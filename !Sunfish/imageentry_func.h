/*
	$Id$
	$URL$

	Routines for ImageEntry_Func
*/


#include "imageentry_common.h"



os_error *func_newimage(unsigned int fileswitchhandle, 	struct conn_info **myhandle);

os_error *func_closeimage(struct conn_info *conn);

os_error *func_readdirinfo(int info, char *dirname, char *buffer, int numobjs, int start, int buflen, struct conn_info *conn, int *objsread, int *continuepos);

os_error *func_rename(char *oldfilename, char *newfilename, struct conn_info *conn, int *renamefailed);



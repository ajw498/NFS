/*
	$Id: $

	Routines for ImageEntry_Func
*/


#include "rpc.h"


os_error *func_newimage(unsigned int fileswitchhandle, 	struct conn_info **myhandle);

os_error *func_closeimage(struct conn_info *conn);

os_error *func_readdirinfo(int info, const char *dirname, void *buffer, int numobjs, int start, int buflen, struct conn_info *conn, int *objsread, int *continuepos);

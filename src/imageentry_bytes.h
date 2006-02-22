/*
	$Id$

	Routines for ImageEntry_Get/PutBytes
*/


#include "imageentry_common.h"

ENTRYFUNCDECL(os_error *, get_bytes, (struct file_handle *handle, char *buffer, unsigned len, unsigned offset));

ENTRYFUNCDECL(os_error *, put_bytes, (struct file_handle *handle, char *buffer, unsigned len, unsigned offset));

ENTRYFUNCDECL(os_error *, writebytes, (struct commonfh *fhandle, char *buffer, unsigned int len, unsigned int offset, struct conn_info *conn));


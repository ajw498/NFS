/*
	$Id$

	Routines for ImageEntry_Get/PutBytes
*/


#include "imageentry_common.h"

os_error *get_bytes(struct file_handle *handle, char *buffer, unsigned len, unsigned offset);

os_error *put_bytes(struct file_handle *handle, char *buffer, unsigned len, unsigned offset);

os_error *writebytes(char *fhandle, char *buffer, unsigned int len, unsigned int offset, struct conn_info *conn);

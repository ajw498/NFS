/*
	$Id$

	Routines for ImageEntry_Open/Close
*/


#include "imageentry_common.h"


ENTRYFUNCDECL(os_error *, open_file, (char *filename, int access, struct conn_info *conn, unsigned int *file_info_word, struct file_handle **internal_handle, unsigned int *fileswitchbuffersize, unsigned int *extent, unsigned int *allocatedspace));

ENTRYFUNCDECL(os_error *, close_file, (struct file_handle *handle, unsigned int load, unsigned int exec));


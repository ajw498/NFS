/*
	$Id$

	Routines for ImageEntry_Args
*/


#include "imageentry_common.h"


os_error *args_zeropad(struct file_handle *handle, unsigned int offset, unsigned int size);

os_error *args_writeextent(struct file_handle *handle, unsigned int extent);

os_error *args_ensuresize(struct file_handle *handle, unsigned int size, unsigned int *actualsize);

os_error *args_readdatestamp(struct file_handle *handle, unsigned int *load, unsigned int *exec);

os_error *args_readallocatedsize(struct file_handle *handle, unsigned int *size);

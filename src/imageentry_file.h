/*
	$Id$

	Routines for ImageEntry_File
*/


#include "imageentry_common.h"


ENTRYFUNCDECL(os_error *, file_readcatinfo, (char *filename, struct conn_info *conn, int *objtype, unsigned int *load, unsigned int *exec, int *len, int *attr));

ENTRYFUNCDECL(os_error *, file_writecatinfo, (char *filename, unsigned int load, unsigned int exec, int attr, struct conn_info *conn));

ENTRYFUNCDECL(os_error *, file_savefile, (char *filename, unsigned int load, unsigned int exec, char *buffer, char *buffer_end, struct conn_info *conn, char **leafname));

ENTRYFUNCDECL(os_error *, file_delete, (char *filename, struct conn_info *conn, int *objtype, unsigned int *load, unsigned int *exec, int *len, int *attr));

ENTRYFUNCDECL(os_error *, file_createfile, (char *filename, unsigned int load, unsigned int exec, char *buffer, char *buffer_end, char **leafname, struct conn_info *conn));

ENTRYFUNCDECL(os_error *, file_createdir, (char *filename, unsigned int load, unsigned int exec, struct conn_info *conn));



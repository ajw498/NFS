/*
	Routines for ImageEntry_File

        Copyright (C) 2003 Alex Waugh

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/


#include "imageentry_common.h"


ENTRYFUNCDECL(os_error *, file_readcatinfo, (char *filename, struct conn_info *conn, int *objtype, unsigned int *load, unsigned int *exec, int *len, int *attr));

ENTRYFUNCDECL(os_error *, file_writecatinfo, (char *filename, unsigned int load, unsigned int exec, int attr, struct conn_info *conn));

ENTRYFUNCDECL(os_error *, file_savefile, (char *filename, unsigned int load, unsigned int exec, char *buffer, char *buffer_end, struct conn_info *conn, char **leafname));

ENTRYFUNCDECL(os_error *, file_delete, (char *filename, struct conn_info *conn, int *objtype, unsigned int *load, unsigned int *exec, int *len, int *attr));

ENTRYFUNCDECL(os_error *, file_createfile, (char *filename, unsigned int load, unsigned int exec, char *buffer, char *buffer_end, char **leafname, struct conn_info *conn));

ENTRYFUNCDECL(os_error *, file_createdir, (char *filename, unsigned int load, unsigned int exec, struct conn_info *conn));



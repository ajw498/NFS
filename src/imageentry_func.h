/*
	Routines for ImageEntry_Func

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



os_error *func_newimage(unsigned int fileswitchhandle, char *config, struct conn_info **myhandle);

void free_conn_info(struct conn_info *conn);

ENTRYFUNCDECL(os_error *, func_newimage_mount, (struct conn_info *conn));

ENTRYFUNCDECL(os_error *, func_closeimage, (struct conn_info *conn));

ENTRYFUNCDECL(os_error *, func_readdirinfo, (int info, char *dirname, char *buffer, int numobjs, int start, int buflen, struct conn_info *conn, int *objsread, int *continuepos));

ENTRYFUNCDECL(os_error *, func_rename, (char *oldfilename, char *newfilename, struct conn_info *conn, int *renamefailed));

ENTRYFUNCDECL(os_error *, func_free, (char *filename, struct conn_info *conn, unsigned *freelo, unsigned *freehi, unsigned *biggestobj, unsigned *sizelo, unsigned *sizehi, unsigned *usedlo, unsigned *usedhi));

os_error *func_readboot(int *option);


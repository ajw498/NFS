/*
	Common bits for all entry points

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

#ifndef IMAGEENTRY_COMMON_H
#define IMAGEENTRY_COMMON_H


#include <kernel.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifdef NFS3
#include "nfs3-structs.h"
#else
#include "nfs2-structs.h"
#endif

#include "utils.h"
#include "clientconn.h"


#ifdef NFS3
#define commonfh_to_fh(fh, common) do { \
	(fh).data.data = (common).handle; \
	(fh).data.size = (common).size; \
} while (0)
#else
#define commonfh_to_fh(fh, common) do { \
	memcpy((fh).data, (common).handle, FHSIZE); \
} while (0)
#endif

#ifdef NFS3
#define fh_to_commonfh(common, fh) do { \
	memcpy((common).handle, (fh).data.data, (fh).data.size); \
	(common).size = (fh).data.size; \
} while (0)
#else
#define fh_to_commonfh(common, fh) do { \
	memcpy((common).handle, (fh).data, FHSIZE); \
} while (0)
#endif

struct objinfo {
	struct commonfh objhandle;
#ifdef NFS3
	struct fattr3 attributes;
#else
	struct fattr attributes;
#endif
};

/* Generate a RISC OS error block based on the NFS status code */
ENTRYFUNCDECL(os_error *, gen_nfsstatus_error, (nstat stat));

/* Convert a leafname into an nfs handle, following symlinks as necessary */
ENTRYFUNCDECL(os_error *, leafname_to_finfo, (char *leafname, size_t *len, int simple, int followsymlinks, struct commonfh *dirhandle, struct objinfo **finfo, nstat *status, struct conn_info *conn));

/* Convert a full filename/dirname into an nfs handle */
ENTRYFUNCDECL(os_error *, filename_to_finfo, (char *filename, int followsymlinks, struct objinfo **dinfo, struct objinfo **finfo, char **leafname, int *filetype, int *extfound, struct conn_info *conn));

#ifdef NFS3
ENTRYFUNCDECL(void, loadexec_to_setmtime, (unsigned int load, unsigned int exec, struct set_mtime *mtime));
#endif

#endif


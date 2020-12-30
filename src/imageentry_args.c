/*
	Routines for ImageEntry_Args


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


#include <string.h>

#include "imageentry_args.h"
#include "imageentry_bytes.h"

#ifdef NFS3
#include "nfs3-calls.h"
#else
#include "nfs2-calls.h"
#endif


os_error *ENTRYFUNC(args_zeropad) (struct file_handle *handle, unsigned int offset, unsigned int size)
{
	static char zeros[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	os_error *err;

	/* This is going to be pretty slow, but any sensible program won't
	   be calling this entry point */
	while (size > sizeof(zeros)) {
		err = ENTRYFUNC(writebytes) (&(handle->fhandle), zeros, sizeof(zeros), offset, handle, handle->conn);
		if (err) return err;

		size -= sizeof(zeros);
		offset += sizeof(zeros);
	}

	if (size > 0) {
		err = ENTRYFUNC(writebytes) (&(handle->fhandle), zeros, size, offset, handle, handle->conn);
		if (err) return err;
	}

	return NULL;
}

static os_error *writeextent(struct file_handle *handle, unsigned int extent)
{
	os_error *err;
#ifdef NFS3
	struct sattrargs3 args;
	struct sattrres3 res;
#else
	struct sattrargs args;
	struct sattrres res;
#endif

	commonfh_to_fh(args.file, handle->fhandle);
#ifdef NFS3
	args.attributes.mode.set_it = FALSE;
	args.attributes.uid.set_it = FALSE;
	args.attributes.gid.set_it = FALSE;
	args.attributes.atime.set_it = DONT_CHANGE;
	args.attributes.mtime.set_it = DONT_CHANGE;
	args.attributes.size.set_it = TRUE;
	args.attributes.size.u.size = extent;
	args.guard.check = FALSE;
#else
	args.attributes.mode = NOVALUE;
	args.attributes.uid = NOVALUE;
	args.attributes.gid = NOVALUE;
	args.attributes.atime.seconds = NOVALUE;
	args.attributes.atime.useconds = NOVALUE;
	args.attributes.mtime.seconds = NOVALUE;
	args.attributes.mtime.useconds = NOVALUE;
	args.attributes.size = extent;
#endif
	err = NFSPROC(SETATTR, (&args, &res, handle->conn));
	if (err) return err;
	if (res.status != NFS_OK) return ENTRYFUNC(gen_nfsstatus_error) (res.status);

	return NULL;
}

os_error *ENTRYFUNC(args_writeextent) (struct file_handle *handle, unsigned int extent)
{
	os_error *err;

	err = writeextent(handle, extent);
	if (err) return err;

	if (extent > handle->extent) {
		err = ENTRYFUNC(args_zeropad) (handle, handle->extent, extent - handle->extent);
		if (err) return err;
	}

	handle->extent = extent;

	return NULL;
}

os_error *ENTRYFUNC(args_ensuresize) (struct file_handle *handle, unsigned int size, unsigned int *actualsize)
{
	os_error *err;

	if (size > handle->extent) {
		err = writeextent(handle, size);
		if (err) return err;

		handle->extent = size;
	}

	*actualsize = (handle->extent + (FAKE_BLOCKSIZE - 1)) & ~(FAKE_BLOCKSIZE - 1);

	return NULL;
}

os_error *ENTRYFUNC(args_readdatestamp) (struct file_handle *handle, unsigned int *load, unsigned int *exec)
{
	*load = handle->load;
	*exec = handle->exec;

	return NULL;
}

os_error *ENTRYFUNC(args_readallocatedsize) (struct file_handle *handle, unsigned int *size)
{
	*size = (handle->extent + (FAKE_BLOCKSIZE - 1)) & ~(FAKE_BLOCKSIZE - 1);

	return NULL;
}



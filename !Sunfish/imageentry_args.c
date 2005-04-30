/*
	$Id$
	$URL$

	Routines for ImageEntry_Args


	Copyright (C) 2003 Alex Waugh
	
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	
	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#include <string.h>

#include "imageentry_args.h"
#include "imageentry_bytes.h"

#ifdef NFS3
#include "nfs3-calls.h"
#else
#include "nfs2-calls.h"
#endif


os_error *args_zeropad(struct file_handle *handle, unsigned int offset, unsigned int size)
{
	static char zeros[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	os_error *err;

	/* This is going to be pretty slow, but any sensible program won't
	   be calling this entry point */
	while (size > sizeof(zeros)) {
		err = writebytes(&(handle->fhandle), zeros, sizeof(zeros), offset, handle->conn);
		if (err) return err;

		size -= sizeof(zeros);
		offset += sizeof(zeros);
	}

	if (size > 0) {
		err = writebytes(&(handle->fhandle), zeros, size, offset, handle->conn);
		if (err) return err;
	}

	return NULL;
}

static os_error *writeextent(struct file_handle *handle, unsigned int extent)
{
	os_error *err;
	struct sattrargs args;
#ifdef NFS3
	struct sattrres res;
#else
	struct attrstat res;
#endif

	args.file = handle->fhandle;
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
	if (res.status != NFS_OK) return gen_nfsstatus_error(res.status);

	return NULL;
}

os_error *args_writeextent(struct file_handle *handle, unsigned int extent)
{
	os_error *err;

	err = writeextent(handle, extent);
	if (err) return err;

	if (extent > handle->extent) {
		err = args_zeropad(handle, handle->extent, extent - handle->extent);
		if (err) return err;
	}

	handle->extent = extent;

	return NULL;
}

os_error *args_ensuresize(struct file_handle *handle, unsigned int size, unsigned int *actualsize)
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

os_error *args_readdatestamp(struct file_handle *handle, unsigned int *load, unsigned int *exec)
{
	*load = handle->load;
	*exec = handle->exec;

	return NULL;
}

os_error *args_readallocatedsize(struct file_handle *handle, unsigned int *size)
{
	*size = (handle->extent + (FAKE_BLOCKSIZE - 1)) & ~(FAKE_BLOCKSIZE - 1);

	return NULL;
}



/*
	$Id$

	Routines for ImageEntry_Open/Close


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


#include <stdlib.h>
#include <string.h>

#include "imageentry_openclose.h"

#ifdef NFS3
#include "nfs3-calls.h"
#else
#include "nfs2-calls.h"
#endif


/* Open a file. The handle returned is a pointer to a struct holding the
   NFS handle and any other info needed */
os_error *ENTRYFUNC(open_file) (char *filename, int access, struct conn_info *conn,
                    unsigned int *file_info_word,
                    struct file_handle **internal_handle,
                    unsigned int *fileswitchbuffersize, unsigned int *extent,
                    unsigned int *allocatedspace)
{
	struct file_handle *handle;
	struct objinfo *dinfo;
	struct objinfo *finfo;
	struct objinfo createinfo;
#ifdef NFS3
	struct createargs3 createargs;
	struct createres3 createres;
#else
	struct createargs createargs;
	struct createres createres;
#endif
	os_error *err;
	char *leafname;
	int filetype;

	err = ENTRYFUNC(filename_to_finfo) (filename, 1, &dinfo, &finfo, &leafname, &filetype, NULL, conn);
	if (err) return err;

	if (finfo == NULL) {
		if (access == 1) {
			/* Create and open for update */
			if (dinfo) {
				commonfh_to_fh(createargs.where.dir, dinfo->objhandle);
			} else {
				commonfh_to_fh(createargs.where.dir, conn->rootfh);
			}
			createargs.where.name.data = leafname;
			createargs.where.name.size = strlen(leafname);
#ifdef NFS3
			createargs.how.mode = GUARDED;
			createargs.how.u.obj_attributes.mode.set_it = TRUE;
			createargs.how.u.obj_attributes.mode.u.mode = 0x00008000 | (0666 & ~(conn->umask));
			createargs.how.u.obj_attributes.uid.set_it = FALSE;
			createargs.how.u.obj_attributes.gid.set_it = FALSE;
			createargs.how.u.obj_attributes.size.set_it = TRUE;
			createargs.how.u.obj_attributes.size.u.size = 0;
			createargs.how.u.obj_attributes.atime.set_it = SET_TO_SERVER_TIME;
			createargs.how.u.obj_attributes.mtime.set_it = SET_TO_SERVER_TIME;
#else
			createargs.attributes.mode = 0x00008000 | (0666 & ~(conn->umask));
			createargs.attributes.uid = NOVALUE;
			createargs.attributes.gid = NOVALUE;
			createargs.attributes.size = 0;
			createargs.attributes.atime.seconds = NOVALUE;
			createargs.attributes.atime.useconds = NOVALUE;
			createargs.attributes.mtime.seconds = NOVALUE;
			createargs.attributes.mtime.useconds = NOVALUE;
#endif

			err = NFSPROC(CREATE, (&createargs, &createres, conn));
			if (err) return err;
			if (createres.status != NFS_OK) return ENTRYFUNC(gen_nfsstatus_error) (createres.status);

#ifdef NFS3
			if (createres.u.diropok.obj.handle_follows == FALSE
			    || createres.u.diropok.obj_attributes.attributes_follow == FALSE) {
				gen_error(NOATTRS, NOATTRSMESS);
			}

			fh_to_commonfh(createinfo.objhandle, createres.u.diropok.obj.u.handle);
			createinfo.attributes = createres.u.diropok.obj_attributes.u.attributes;
#else
			fh_to_commonfh(createinfo.objhandle, createres.u.diropok.file);
			createinfo.attributes = createres.u.diropok.attributes;
#endif
			finfo = &createinfo;
			filetype = conn->defaultfiletype;
		} else {
			/* File not found */
			*internal_handle = 0;
			return NULL;
		}
	}

	if (finfo->attributes.type != NFREG) return gen_error(OPENCLOSEERRBASE + 0, "Cannot open a non-regular file");

	handle = malloc(sizeof(struct file_handle));
	if (handle == NULL) return gen_error(NOMEM, NOMEMMESS);

	handle->conn = conn;
	handle->fhandle = finfo->objhandle;
	handle->extent = filesize(finfo->attributes.size);
#ifdef NFS3
	timeval_to_loadexec(finfo->attributes.mtime.seconds, finfo->attributes.mtime.nseconds, filetype, &(handle->load), &(handle->exec), 0);
#else
	timeval_to_loadexec(finfo->attributes.mtime.seconds, finfo->attributes.mtime.useconds, filetype, &(handle->load), &(handle->exec), 1);
#endif
	*internal_handle = handle;
	/* It is too difficult to determine if we will have permission to read
	   or write the file so pretend that we can and return an error on read
	   or write if necessary */
	*file_info_word = 0xC0000000;
	*extent = filesize(finfo->attributes.size);
	*fileswitchbuffersize = FAKE_BLOCKSIZE;
	*allocatedspace = (*extent + (FAKE_BLOCKSIZE - 1)) & ~(FAKE_BLOCKSIZE - 1);
	return NULL;
}

os_error *ENTRYFUNC(close_file) (struct file_handle *handle, unsigned int load, unsigned int exec)
{
	/* The filetype shouldn't have changed since the file was opened and
	   the server will set the datestamp for us, therefore there is not
	   much point in us explicitly updating the attributes.
	   Additionally, the operation could fail if we don't have permission
	   even though the file was successfully open for reading. */
	load = load;
	exec = exec;

	free(handle);

	return NULL;
}


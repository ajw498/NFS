/*
	$Id$
	$URL$

	Routines for ImageEntry_GetBytes and ImageEntry_PutBytes


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

#include "imageentry_bytes.h"

#include "nfs-calls.h"


/* Read a number of bytes from the open file */
os_error *get_bytes(struct file_handle *handle, char *buffer, unsigned int len, unsigned int offset)
{
	os_error *err;
	struct readargs args;
	struct readres res;

	if (handle->type != NFREG) {
		return gen_error(BYTESERRBASE + 1,"Cannot read data from a non-regular file");
	}

	args.totalcount = 0; /* Unused in NFS2 */
	memcpy(args.file, handle->fhandle, FHSIZE);

	do {
		args.offset = offset;
		args.count = len;
		if (args.count > handle->conn->maxdatabuffer) args.count = handle->conn->maxdatabuffer;
		offset += args.count;
		
		err = NFSPROC_READ(&args, &res, handle->conn);
		if (err) return err;
		if (res.status != NFS_OK) return gen_nfsstatus_error(res.status);

		if (res.u.data.size > args.count) {
			return gen_error(BYTESERRBASE + 0,"Read returned more data than expected");
		}

		memcpy(buffer, res.u.data.data, res.u.data.size);
		buffer += args.count;
		len -= args.count;
	} while (len > 0 && res.u.data.size == args.count);

	return NULL;
}

/* Write a number of bytes to the open file.
   Used by put_bytes and file_savefile */
os_error *writebytes(char *fhandle, char *buffer, unsigned int len, unsigned int offset, struct conn_info *conn)
{
	os_error *err;
	struct writeargs args;
	struct attrstat res;

	args.beginoffset = 0; /* Unused in NFS2 */
	args.totalcount = 0;  /* Unused in NFS2 */
	memcpy(args.file, fhandle, FHSIZE);

	do {
		args.offset = offset;
		args.data.size = len;
		if (args.data.size > conn->maxdatabuffer) args.data.size = conn->maxdatabuffer;
		offset += args.data.size;
		len -= args.data.size;
		args.data.data = buffer;
		buffer += args.data.size;

		err = NFSPROC_WRITE(&args, &res, conn);
		if (err) return err;
		if (res.status != NFS_OK) return gen_nfsstatus_error(res.status);
	} while (len > 0);

	return NULL;
}

/* Write a number of bytes to the open file */
os_error *put_bytes(struct file_handle *handle, char *buffer, unsigned int len, unsigned int offset)
{
	if (handle->type != NFREG) {
		return gen_error(BYTESERRBASE + 2,"Cannot write data to a non-regular file");
	}

	return writebytes(handle->fhandle, buffer, len, offset, handle->conn);
}



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

#ifdef NFS3
#include "nfs3-calls.h"
#else
#include "nfs2-calls.h"
#endif


/* Read a number of bytes from the open file */
os_error *get_bytes(struct file_handle *handle, char *buffer, unsigned int len, unsigned int offset)
{
	os_error *err;
	struct readargs args;
	struct readres res;
	char *bufferend = buffer + len;
	int outstanding = 0;

	if (handle->type != NFREG) {
		return gen_error(BYTESERRBASE + 1,"Cannot read data from a non-regular file");
	}

#ifndef NFS3
	args.totalcount = 0; /* Unused in NFS2 */
#endif
	args.file = handle->fhandle;

	while (len > 0 || outstanding > 0) {
		args.offset = offset;
		args.count = len;
		if (args.count > handle->conn->maxdatabuffer) args.count = handle->conn->maxdatabuffer;
		offset += args.count;
		len -= args.count;
		if (args.count > 0) outstanding++;

		err = NFSPROC(READ, (&args, &res, handle->conn, handle->conn->pipelining ? (args.count > 0 ? TXNONBLOCKING : RXBLOCKING) : TXBLOCKING));
		if (err != ERR_WOULDBLOCK) {
			struct opaque *data;

			if (err) return err;
			if (res.status != NFS_OK) return gen_nfsstatus_error(res.status);

#ifdef NFS3
			data = &(res.u.resok.data);
#else
			data = &(res.u.data);
#endif
			outstanding--;
			if (buffer + data->size > bufferend) {
				return gen_error(BYTESERRBASE + 0,"Read returned more data than expected");
			}
			/* FIXME - check data size == count, and check EOF */
			memcpy(buffer, data->data, data->size);
			buffer += data->size;
		}
	}

	return NULL;
}

/* Write a number of bytes to the open file.
   Used by put_bytes and file_savefile */
os_error *writebytes(struct nfs_fh *fhandle, char *buffer, unsigned int len, unsigned int offset, struct conn_info *conn)
{
	os_error *err;
	struct writeargs args;
#ifdef NFS3
	struct writeres res;
#else
	struct attrstat res;
#endif
	int outstanding = 0;

#ifndef NFS3
	args.beginoffset = 0; /* Unused in NFS2 */
	args.totalcount = 0;  /* Unused in NFS2 */
#endif
	args.file = *fhandle;

	while (len > 0 || outstanding > 0) {
		args.offset = offset;
		args.data.size = len;
#ifdef NFS3
		args.count = len;
		args.stable = FILE_SYNC; /* FIXME - Use unstable and COMMIT on close? */
#endif
		if (args.data.size > conn->maxdatabuffer) args.data.size = conn->maxdatabuffer;
		/* FIXME should also truncate to wtmax from FSINFO */
		offset += args.data.size;
		len -= args.data.size;
		args.data.data = buffer;
		buffer += args.data.size;
		if (args.data.size > 0) outstanding++;

		err = NFSPROC(WRITE, (&args, &res, conn, conn->pipelining ? (args.data.size > 0 ? TXNONBLOCKING : RXBLOCKING) : TXBLOCKING));
		/*FIXME - cope with count being less than requested */
		if (err != ERR_WOULDBLOCK) {
			if (err) return err;
			if (res.status != NFS_OK) return gen_nfsstatus_error(res.status);
			outstanding--;
		}
	}

	return NULL;
}

/* Write a number of bytes to the open file */
os_error *put_bytes(struct file_handle *handle, char *buffer, unsigned int len, unsigned int offset)
{
	if (handle->type != NFREG) {
		return gen_error(BYTESERRBASE + 2,"Cannot write data to a non-regular file");
	}

	if (offset + len > handle->extent) handle->extent = offset + len;

	return writebytes(&(handle->fhandle), buffer, len, offset, handle->conn);
}



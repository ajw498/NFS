/*
	$Id$

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
os_error *ENTRYFUNC(get_bytes) (struct file_handle *handle, char *buffer, unsigned int len, unsigned int offset)
{
	os_error *err;
	struct readargs args;
	struct readres res;
	char *bufferend = buffer + len;
	int outstanding = 0;
	int reqsizes[FIFOSIZE];
	int reqtail = 0;

#ifndef NFS3
	args.totalcount = 0; /* Unused in NFS2 */
#endif
	commonfh_to_fh(args.file, handle->fhandle);

	while (len > 0 || outstanding > 0) {
		args.offset = offset;
		args.count = len;
		if (args.count > handle->conn->maxdatabuffer) args.count = handle->conn->maxdatabuffer;
		offset += args.count;
		len -= args.count;
		if (args.count > 0) outstanding++;
		reqsizes[reqtail++] = args.count;

		err = NFSPROC(READ, (&args, &res, handle->conn, handle->conn->pipelining ? (args.count > 0 ? TXNONBLOCKING : RXBLOCKING) : TXBLOCKING));
		if (err != ERR_WOULDBLOCK) {
			struct opaque *data;

			if (err) return err;
			if (res.status != NFS_OK) return ENTRYFUNC(gen_nfsstatus_error) (res.status);

			data = &(res.u.resok.data);
			outstanding--;
			if (buffer + data->size > bufferend) {
				return gen_error(BYTESERRBASE + 0,"Read returned more data than expected");
			}

#ifdef NFS3
			if (data->size < reqsizes[0] && res.u.resok.eof == 0) {
				return gen_error(BYTESERRBASE + 1,"Read returned less data than expected");
			}
#endif
			for (int i = 0; i < FIFOSIZE - 1; i++) reqsizes[i] = reqsizes[i+1];
			reqtail--;

			memcpy(buffer, data->data, data->size);
			buffer += data->size;
		}
	}

	return NULL;
}

/* Write a number of bytes to the open file.
   Used by put_bytes and file_savefile */
os_error *ENTRYFUNC(writebytes) (struct commonfh *fhandle, char *buffer, unsigned int len, unsigned int offset, struct conn_info *conn)
{
	os_error *err;
	struct writeargs args;
#ifdef NFS3
	struct writeres res;
#else
	struct attrstat res;
#endif
	int outstanding = 0;
	int reqsizes[FIFOSIZE];
	int reqtail = 0;

#ifndef NFS3
	args.beginoffset = 0; /* Unused in NFS2 */
	args.totalcount = 0;  /* Unused in NFS2 */
#endif
	commonfh_to_fh(args.file, *fhandle);

	while (len > 0 || outstanding > 0) {
		args.offset = offset;
		args.data.size = len;
		if (args.data.size > conn->maxdatabuffer) args.data.size = conn->maxdatabuffer;
		offset += args.data.size;
		len -= args.data.size;
		args.data.data = buffer;
		buffer += args.data.size;
		if (args.data.size > 0) outstanding++;
#ifdef NFS3
		args.count = args.data.size;
		args.stable = FILE_SYNC;
#endif
		reqsizes[reqtail++] = args.data.size;

		err = NFSPROC(WRITE, (&args, &res, conn, conn->pipelining ? (args.data.size > 0 ? TXNONBLOCKING : RXBLOCKING) : TXBLOCKING));
		if (err != ERR_WOULDBLOCK) {
			if (err) return err;
			if (res.status != NFS_OK) return ENTRYFUNC(gen_nfsstatus_error) (res.status);
			outstanding--;
#ifdef NFS3
			if (res.u.resok.count < reqsizes[0]) {
				return gen_error(BYTESERRBASE + 1,"Write wrote less data than expected");
			}
#endif
			for (int i = 0; i < FIFOSIZE - 1; i++) reqsizes[i] = reqsizes[i+1];
			reqtail--;
		}
	}

	return NULL;
}

/* Write a number of bytes to the open file */
os_error *ENTRYFUNC(put_bytes) (struct file_handle *handle, char *buffer, unsigned int len, unsigned int offset)
{
	if (offset + len > handle->extent) handle->extent = offset + len;

	return ENTRYFUNC(writebytes) (&(handle->fhandle), buffer, len, offset, handle->conn);
}



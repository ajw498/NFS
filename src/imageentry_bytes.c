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

/* Create special cases for writes so that we can pass the data direct to the
   sendmsg call without having to copy it. We can't do this with reads because
   we don't know which reply we are going to recieve next at the point when we
   allocate the buffer .*/
#ifdef NFS3
static os_error *NFSPROC3_FASTWRITE(writeargs3 *args, writeres3 *res, struct conn_info *conn, enum callctl calltype)
{
	os_error *err;

	err = rpc_prepare_call(NFS_RPC_PROGRAM, NFS_RPC_VERSION, 7, conn);
	if (err) return err;
	if (process_nfs_fh3(OUTPUT, &(args->file), conn->pool))       return rpc_buffer_overflow();
	if (process_offset3(OUTPUT, &(args->offset), conn->pool))     return rpc_buffer_overflow();
	if (process_count3(OUTPUT, &(args->count), conn->pool))       return rpc_buffer_overflow();
	if (process_stable_how(OUTPUT, &(args->stable), conn->pool))  return rpc_buffer_overflow();
	if (process_unsigned(OUTPUT, &(args->data.size), conn->pool)) return rpc_buffer_overflow();
	err = rpc_do_call(NFS_RPC_PROGRAM, calltype, args->data.data, args->data.size, conn);
	if (err) return err;
	if (process_writeres3(INPUT, res, conn->pool)) return rpc_buffer_overflow();
	return NULL;
}
#else
static os_error *NFSPROC_FASTWRITE(writeargs *args, attrstat *res, struct conn_info *conn, enum callctl calltype)
{
	os_error *err;

	err = rpc_prepare_call(NFS_RPC_PROGRAM, NFS_RPC_VERSION, 8, conn);
	if (err) return err;
	if (process_nfs_fh(OUTPUT, &(args->file), conn->pool))          return rpc_buffer_overflow();
	if (process_unsigned(OUTPUT, &(args->beginoffset), conn->pool)) return rpc_buffer_overflow();
	if (process_unsigned(OUTPUT, &(args->offset), conn->pool))      return rpc_buffer_overflow();
	if (process_unsigned(OUTPUT, &(args->totalcount), conn->pool))  return rpc_buffer_overflow();
	if (process_unsigned(OUTPUT, &(args->data.size), conn->pool))   return rpc_buffer_overflow();
	err = rpc_do_call(NFS_RPC_PROGRAM, calltype, args->data.data, args->data.size, conn);
	if (err) return err;
	if (process_attrstat(INPUT, res, conn->pool)) return rpc_buffer_overflow();
	return NULL;
}
#endif

void *fastmemcpy(void *ptr1, void *ptr2, size_t n);

/* Read a number of bytes from the open file */
os_error *ENTRYFUNC(get_bytes) (struct file_handle *handle, char *buffer, unsigned int len, unsigned int offset)
{
	os_error *err;
#ifdef NFS3
	struct readargs3 args;
	struct readres3 res;
#else
	struct readargs args;
	struct readres res;
#endif
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
		err = NFSPROC(READ, (&args, &res, handle->conn, ((outstanding < FIFOSIZE) && handle->conn->pipelining) ? (args.count > 0 ? TXNONBLOCKING : RXBLOCKING) : TXBLOCKING));
		if (err != ERR_WOULDBLOCK) {
			if (err) return err;
			if (res.status != NFS_OK) return ENTRYFUNC(gen_nfsstatus_error) (res.status);

			outstanding--;
			if (buffer + res.u.resok.data.size > bufferend) {
				return gen_error(BYTESERRBASE + 0,"Read returned more data than expected");
			}

#ifdef NFS3
			if (res.u.resok.data.size < reqsizes[0] && res.u.resok.eof == 0) {
				return gen_error(BYTESERRBASE + 1,"Read returned less data than expected");
			}
#endif
			for (int i = 0; i < FIFOSIZE - 1; i++) reqsizes[i] = reqsizes[i+1];
			reqtail--;

			fastmemcpy(buffer, res.u.resok.data.data, res.u.resok.data.size);
			buffer += res.u.resok.data.size;
		}
	}

	return NULL;
}

/* Write a number of bytes to the open file.
   Used by put_bytes and file_savefile */
os_error *ENTRYFUNC(writebytes) (struct commonfh *fhandle, char *buffer, unsigned int len, unsigned int offset, struct file_handle *handle, struct conn_info *conn)
{
	os_error *err;
#ifdef NFS3
	struct writeargs3 args;
	struct writeres3 res;
#else
	struct writeargs args;
	struct attrstat res;
#endif
	int outstanding = 0;
	int reqsizes[FIFOSIZE];
	int reqtail = 0;

#ifndef NFS3
	args.beginoffset = 0; /* Unused in NFS2 */
	args.totalcount = 0;  /* Unused in NFS2 */
	handle = handle;
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
		args.stable = handle ? UNSTABLE : FILE_SYNC;
#endif
		reqsizes[reqtail++] = args.data.size;

		err = NFSPROC(FASTWRITE, (&args, &res, conn, ((outstanding < FIFOSIZE) && conn->pipelining) ? (args.data.size > 0 ? TXNONBLOCKING : RXBLOCKING) : TXBLOCKING));
		if (err != ERR_WOULDBLOCK) {
			if (err) return err;
			if (res.status != NFS_OK) return ENTRYFUNC(gen_nfsstatus_error) (res.status);
			outstanding--;
#ifdef NFS3
			if (res.u.resok.count < reqsizes[0]) {
				return gen_error(BYTESERRBASE + 1,"Write wrote less data than expected");
			}
			if (handle && res.u.resok.committed != FILE_SYNC) {
				if (handle->commitneeded) {
					if (memcmp(res.u.resok.verf, handle->verf, NFS3_WRITEVERFSIZE) != 0) return gen_error(OPENCLOSEERRBASE + 1, "Server has rebooted while file open - data may have been lost");
				} else {
					handle->commitneeded = 1;
					memcpy(handle->verf, res.u.resok.verf, NFS3_WRITEVERFSIZE);
				}
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

	return ENTRYFUNC(writebytes) (&(handle->fhandle), buffer, len, offset, handle, handle->conn);
}



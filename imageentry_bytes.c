/*
	$Id$

	Routines for ImageEntry_Get/PutBytes
*/


#include <string.h>

#include "imageentry_func.h"

#include "nfs-calls.h"


/*FIXME*/
#define BYTESERRBASE 20


#define MAX_PAYLOAD 7000
/*FIXME*/


/* Read a number of bytes from the open file */
os_error *get_bytes(struct file_handle *handle, char *buffer, unsigned int len, unsigned int offset)
{
	os_error *err;
	struct readargs args;
	struct readres res;

	args.totalcount = 0; /* Unused in NFS2 */
	memcpy(args.file, handle->fhandle, FHSIZE);

	do {
		args.offset = offset;
		args.count = len;
		if (args.count > MAX_PAYLOAD) args.count = MAX_PAYLOAD;
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
   Used byt put_bytes and file_savefile */
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
		if (args.data.size > MAX_PAYLOAD) args.data.size = MAX_PAYLOAD;
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
	return writebytes(handle->fhandle, buffer, len, offset, handle->conn);
}



/*
	$Id$

	Routines for ImageEntry_Open/Close
*/


#include <stdlib.h>
#include <string.h>

#include "imageentry_func.h"

#include "nfs-calls.h"

#define NOMEM 1
#define NOMEMMESS "Out of memory"


#define FAKE_BLOCKSIZE 1024


/* Open a file. The handle returned is a pointer to a struct holding the
   NFS handle and any other info needed */
os_error *open_file(char *filename, int access, struct conn_info *conn,
                    unsigned int *file_info_word,
                    struct file_handle **internal_handle,
                    unsigned int *fileswitchbuffersize, unsigned int *extent,
                    unsigned int *allocatedspace)
{
	struct file_handle *handle;
    struct diropok *dinfo;
    struct diropok *finfo;
    struct createargs createargs;
    struct diropres createres;
    os_error *err;
    char *leafname;
    int filetype;

	err = filename_to_finfo(filename, &dinfo, &finfo, &leafname, &filetype, NULL, conn);
	if (err) return err;

	if (finfo == NULL) {
		if (access == 1) {
			/* Create and open for update */
			memcpy(createargs.where.dir, dinfo ? dinfo->file : conn->rootfh, FHSIZE);
			createargs.where.name.data = leafname;
			createargs.where.name.size = strlen(leafname);
			createargs.attributes.mode = 0x00008000 | (0666 & ~(conn->umask));
			createargs.attributes.uid = NOVALUE;
			createargs.attributes.gid = NOVALUE;
			createargs.attributes.size = 0;
			createargs.attributes.atime.seconds = NOVALUE;
			createargs.attributes.atime.useconds = NOVALUE;
			createargs.attributes.mtime.seconds = NOVALUE;
			createargs.attributes.mtime.useconds = NOVALUE;

			err = NFSPROC_CREATE(&createargs, &createres, conn);
			if (err) return err;
			if (createres.status != NFS_OK) return gen_nfsstatus_error(createres.status);

			finfo = &(createres.u.diropok);
			filetype = conn->defaultfiletype;
		} else {
			/* File not found */
			*internal_handle = 0;
			return NULL;
		}
	}

	handle = malloc(sizeof(struct file_handle));
	if (handle == NULL) return gen_error(NOMEM, NOMEMMESS);

	handle->conn = conn;
	memcpy(handle->fhandle, finfo->file, FHSIZE);
	handle->extent = finfo->attributes.size;
	timeval_to_loadexec(&(finfo->attributes.mtime), filetype, &(handle->load), &(handle->exec));

	*internal_handle = handle;
	/* It is too difficult to determine if we will have permission to read
	   or write the file so pretend that we can and return an error on read
	   or write if necessary */
	*file_info_word = 0xC0000000;
	*extent = finfo->attributes.size;
	*fileswitchbuffersize = FAKE_BLOCKSIZE;
	*allocatedspace = (*extent + (FAKE_BLOCKSIZE - 1)) & ~(FAKE_BLOCKSIZE - 1);
	return NULL;
}

os_error *close_file(struct file_handle *handle, unsigned int load, unsigned int exec)
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


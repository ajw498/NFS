/*
	$Id$

	Routines for ImageEntry_File
*/


#include <string.h>

#include "imageentry_file.h"
#include "imageentry_bytes.h"

#include "nfs-calls.h"



os_error *file_readcatinfo(char *filename, struct conn_info *conn, int *objtype, unsigned int *load, unsigned int *exec, int *len, int *attr)
{
    struct diropok *finfo;
    os_error *err;
    int filetype;

	err = filename_to_finfo(filename, NULL, &finfo, NULL, &filetype, NULL, conn);
	if (err) return err;

	if (finfo == NULL) {
		*objtype = OBJ_NONE;
		return NULL;
	}

	*objtype = finfo->attributes.type == NFDIR ? OBJ_DIR : OBJ_FILE;
	timeval_to_loadexec(&(finfo->attributes.mtime), filetype, load, exec);
	*len = finfo->attributes.size;
	*attr = mode_to_attr(finfo->attributes.mode);

	return NULL;
}

os_error *file_writecatinfo(char *filename, unsigned int load, unsigned int exec, int attr, struct conn_info *conn)
{
    struct diropok *finfo;
    struct diropok *dinfo;
	struct sattrargs sattrargs;
	struct attrstat sattrres;
    os_error *err;
    int filetype;
    int newfiletype;
    int extfound;
    char *leafname;

	err = filename_to_finfo(filename, &dinfo, &finfo, &leafname, &filetype, &extfound, conn);
	if (err) return err;

	if (finfo == NULL) return gen_nfsstatus_error(NFSERR_NOENT);

	if ((load & 0xFFF00000) == 0xFFF00000) {
		newfiletype = (load & 0xFFF00) >> 8;
	} else {
		newfiletype = conn->defaultfiletype;
	}

	/* If the filetype has changed we may need to rename the file */
	if (conn->xyzext != NEVER && newfiletype != filetype) {
	    struct renameargs renameargs;
	    enum nstat renameres;

		memcpy(renameargs.from.dir, dinfo ? dinfo->file : conn->rootfh, FHSIZE);
		renameargs.from.name.data = leafname;
		renameargs.from.name.size = strlen(leafname);

		memcpy(renameargs.to.dir, renameargs.from.dir, FHSIZE);
		renameargs.to.name.data = addfiletypeext(leafname, renameargs.from.name.size, extfound, newfiletype, &(renameargs.to.name.size), conn);

		err = NFSPROC_RENAME(&renameargs, &renameres, conn);
		if (err) return err;
		if (renameres != NFS_OK) return gen_nfsstatus_error(renameres);
	}

	/* Set the datestamp and attributes */
	memcpy(sattrargs.file, finfo->file, FHSIZE);
	sattrargs.attributes.mode = attr_to_mode(attr, finfo->attributes.mode, conn);
	sattrargs.attributes.uid = NOVALUE;
	sattrargs.attributes.gid = NOVALUE;
	sattrargs.attributes.size = NOVALUE;
	sattrargs.attributes.atime.seconds = NOVALUE;
	sattrargs.attributes.atime.useconds = NOVALUE;
	loadexec_to_timeval(load, exec, &(sattrargs.attributes.mtime));

	err = NFSPROC_SETATTR(&sattrargs, &sattrres, conn);
	if (err) return err;
	if (sattrres.status != NFS_OK) gen_nfsstatus_error(sattrres.status);

	return NULL;
}

/* Create a new file or directory */
static os_error *createobj(char *filename, int dir, unsigned int load, unsigned int exec, char *buffer, char *buffer_end, struct conn_info *conn, char **fhandle, char **leafname)
{
    struct diropok *dinfo;
    os_error *err;
    struct createargs createargs;
    struct diropres createres;
    int newfiletype;

	if ((load & 0xFFF00000) == 0xFFF00000) {
		newfiletype = (load & 0xFFF00) >> 8;
	} else {
		newfiletype = conn->defaultfiletype;
	}

	err = filename_to_finfo(filename, &dinfo, NULL, leafname, NULL, NULL, conn);
	if (err) return err;

	memcpy(createargs.where.dir, dinfo ? dinfo->file : conn->rootfh, FHSIZE);

	if (dir) {
		createargs.where.name.data = *leafname;
		createargs.where.name.size = strlen(*leafname);
	} else {
		/* We may need to add a ,xyz extension */
		createargs.where.name.data = addfiletypeext(*leafname, strlen(*leafname), 0, newfiletype, &(createargs.where.name.size), conn);
	}

	createargs.attributes.mode = 0x00008000 | ((dir ? 0777 : 0666) & ~(conn->umask));
	createargs.attributes.uid = NOVALUE;
	createargs.attributes.gid = NOVALUE;
	createargs.attributes.size = buffer_end - buffer;
	createargs.attributes.atime.seconds = NOVALUE;
	createargs.attributes.atime.useconds = NOVALUE;
	loadexec_to_timeval(load, exec, &(createargs.attributes.mtime));

	if (dir) {
		err = NFSPROC_MKDIR(&createargs, &createres, conn);
	} else {
		err = NFSPROC_CREATE(&createargs, &createres, conn);
	}
	if (err) return err;
	if (createres.status != NFS_OK) return gen_nfsstatus_error(createres.status);

	if (fhandle) *fhandle = createres.u.diropok.file;

	return NULL;
}

os_error *file_createfile(char *filename, unsigned int load, unsigned int exec, char *buffer, char *buffer_end, struct conn_info *conn)
{
	char *leafname;

	return createobj(filename, 0, load, exec, buffer, buffer_end, conn, NULL, &leafname);
}

os_error *file_createdir(char *filename, unsigned int load, unsigned int exec, struct conn_info *conn)
{
	char *leafname;
	
	return createobj(filename, 1, load, exec, 0, 0, conn, NULL, &leafname);
}

/* Save a block of memory as a file */
os_error *file_savefile(char *filename, unsigned int load, unsigned int exec, char *buffer, char *buffer_end, struct conn_info *conn, char **leafname)
{
    os_error *err;
    char *fhandle;

	err = createobj(filename, 0, load, exec, buffer, buffer_end, conn, &fhandle, leafname);
	if (err) return err;

	return writebytes(fhandle, buffer, buffer_end - buffer, 0, conn);
}

/* Delete a file or directory */
os_error *file_delete(char *filename, struct conn_info *conn, int *objtype, unsigned int *load, unsigned int *exec, int *len, int *attr)
{
    struct diropok *dinfo;
    struct diropok *finfo;
    os_error *err;
    char *leafname;
    int filetype;

	err = filename_to_finfo(filename, &dinfo, &finfo, &leafname, &filetype, NULL, conn);
	if (err) return err;

	if (finfo == NULL) {
		/* Object not found */
		*objtype = 0;
	} else {
	    struct diropargs removeargs;
	    enum nstat removeres;

		memcpy(removeargs.dir, dinfo ? dinfo->file : conn->rootfh, FHSIZE);
		removeargs.name.data = leafname;
		removeargs.name.size = strlen(leafname);

		if (finfo->attributes.type == NFDIR) {
			err = NFSPROC_RMDIR(&removeargs, &removeres, conn);
		} else {
			err = NFSPROC_REMOVE(&removeargs, &removeres, conn);
		}
		if (err) return err;
		if (removeres != NFS_OK) return gen_nfsstatus_error(removeres);

		/* Treat all special files as if they were regular files */
		*objtype = finfo->attributes.type == NFDIR ? OBJ_DIR : OBJ_FILE;
		timeval_to_loadexec(&(finfo->attributes.mtime), filetype, load, exec);
		*len = finfo->attributes.size;
		*attr = mode_to_attr(finfo->attributes.mode);
	}

	return NULL;
}

os_error *file_readblocksize(unsigned int *size)
{
	*size = FAKE_BLOCKSIZE;

	return NULL;
}



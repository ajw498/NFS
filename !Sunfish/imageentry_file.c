/*
	$Id$
	$URL$

	Routines for ImageEntry_File


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

#include "imageentry_file.h"
#include "imageentry_bytes.h"

#ifdef NFS3
#include "nfs3-calls.h"
#else
#include "nfs2-calls.h"
#endif

#include "sunfish.h"


os_error *ENTRYFUNC(file_readcatinfo) (char *filename, struct conn_info *conn, int *objtype, unsigned int *load, unsigned int *exec, int *len, int *attr)
{
    struct objinfo *finfo;
    os_error *err;
    int filetype;

	err = ENTRYFUNC(filename_to_finfo) (filename, 1, NULL, &finfo, NULL, &filetype, NULL, conn);
	if (err) return err;

	if (finfo == NULL) {
		*objtype = OBJ_NONE;
		return NULL;
	}

	*objtype = finfo->attributes.type == NFDIR ? OBJ_DIR : OBJ_FILE;
	ENTRYFUNC(timeval_to_loadexec) (&(finfo->attributes.mtime), filetype, load, exec);
	*len = filesize(finfo->attributes.size);
	*attr = mode_to_attr(finfo->attributes.mode);

	return NULL;
}

os_error *ENTRYFUNC(file_writecatinfo) (char *filename, unsigned int load, unsigned int exec, int attr, struct conn_info *conn)
{
	struct objinfo *finfo;
	struct objinfo *dinfo;
	struct sattrargs sattrargs;
	struct sattrres sattrres;
	os_error *err;
	int filetype;
	int newfiletype;
	int extfound;
	char *leafname;

	err = ENTRYFUNC(filename_to_finfo) (filename, 1, &dinfo, &finfo, &leafname, &filetype, &extfound, conn);
	if (err) return err;

	if (finfo == NULL) return ENTRYFUNC(gen_nfsstatus_error) (NFSERR_NOENT);

	if ((load & 0xFFF00000) == 0xFFF00000) {
		newfiletype = (load & 0xFFF00) >> 8;
	} else {
		newfiletype = conn->defaultfiletype;
	}

	if (finfo->attributes.type == NFDIR) {
		/* It seems that RISC OS assumes the owner read/write bits are set
		   for directories, yet doesn't set them. */
		attr |= 3;
	}
	
	/* If the filetype has changed we may need to rename the file */
	if (conn->xyzext != NEVER && newfiletype != filetype && finfo->attributes.type == NFREG) {
		struct renameargs renameargs;
		struct renameres renameres;

		if (dinfo) {
			commonfh_to_fh(renameargs.from.dir, dinfo->objhandle);
		} else {
			commonfh_to_fh(renameargs.from.dir, conn->rootfh);
		}
		renameargs.from.name.data = leafname;
		renameargs.from.name.size = strlen(leafname);

		renameargs.to.dir = renameargs.from.dir;
		renameargs.to.name.data = addfiletypeext(leafname, renameargs.from.name.size, extfound, newfiletype, &(renameargs.to.name.size), conn);

		err = NFSPROC(RENAME, (&renameargs, &renameres, conn));
		if (err) return err;
		if (renameres.status != NFS_OK) return ENTRYFUNC(gen_nfsstatus_error) (renameres.status);
	}

	/* Set the datestamp and attributes */
	commonfh_to_fh(sattrargs.file, finfo->objhandle);
#ifdef NFS3
	sattrargs.attributes.mode.set_it = TRUE;
	sattrargs.attributes.mode.u.mode = attr_to_mode(attr, finfo->attributes.mode, conn);
	sattrargs.attributes.uid.set_it = FALSE;
	sattrargs.attributes.gid.set_it = FALSE;
	sattrargs.attributes.atime.set_it = DONT_CHANGE;
	ENTRYFUNC(loadexec_to_setmtime) (load, exec, &(sattrargs.attributes.mtime));
	sattrargs.attributes.size.set_it = FALSE;
	sattrargs.guard.check = FALSE;
#else
	sattrargs.attributes.mode = attr_to_mode(attr, finfo->attributes.mode, conn);
	sattrargs.attributes.uid = NOVALUE;
	sattrargs.attributes.gid = NOVALUE;
	sattrargs.attributes.size = NOVALUE;
	sattrargs.attributes.atime.seconds = NOVALUE;
	sattrargs.attributes.atime.useconds = NOVALUE;
	ENTRYFUNC(loadexec_to_timeval) (load, exec, &(sattrargs.attributes.mtime));
#endif

	err = NFSPROC(SETATTR, (&sattrargs, &sattrres, conn));
	if (err) return err;
	if (sattrres.status != NFS_OK) ENTRYFUNC(gen_nfsstatus_error) (sattrres.status);

	return NULL;
}

/* Create a new file */
static os_error *createfile(char *filename, unsigned int load, unsigned int exec, char *buffer, char *buffer_end, struct conn_info *conn, struct commonfh **fhandle, char **leafname)
{
	struct objinfo *dinfo;
	struct objinfo *finfo;
	os_error *err;
	struct createargs createargs;
	struct createres createres;
	static struct commonfh fh;
	int filetype;
	int newfiletype;
	int extfound;

	if ((load & 0xFFF00000) == 0xFFF00000) {
		newfiletype = (load & 0xFFF00) >> 8;
	} else {
		newfiletype = conn->defaultfiletype;
	}

	err = ENTRYFUNC(filename_to_finfo) (filename, 1, &dinfo, &finfo, leafname, &filetype, &extfound, conn);
	if (err) return err;

	if (dinfo) {
		commonfh_to_fh(createargs.where.dir, dinfo->objhandle);
	} else {
		commonfh_to_fh(createargs.where.dir, conn->rootfh);
	}

	/* We may need to add a ,xyz extension */
	createargs.where.name.data = addfiletypeext(*leafname, strlen(*leafname), extfound, newfiletype, &(createargs.where.name.size), conn);

	/* If a file already exists then we must overwrite it */
	if (finfo && finfo->attributes.type == NFREG) {
		struct sattrargs sattrargs;
		struct sattrres sattrres;

		/* If the filetype has changed we may need to rename the file */
		if (conn->xyzext != NEVER && newfiletype != filetype) {
			struct renameargs renameargs;
			struct renameres renameres;

			if (dinfo) {
				commonfh_to_fh(renameargs.from.dir, dinfo->objhandle);
			} else {
				commonfh_to_fh(renameargs.from.dir, conn->rootfh);
			}
			renameargs.from.name.data = *leafname;
			renameargs.from.name.size = strlen(*leafname);

			renameargs.to.dir = renameargs.from.dir;
			renameargs.to.name.data = createargs.where.name.data;
			renameargs.to.name.size = createargs.where.name.size;

			err = NFSPROC(RENAME, (&renameargs, &renameres, conn));
			if (err) return err;
			if (renameres.status != NFS_OK) return ENTRYFUNC(gen_nfsstatus_error) (renameres.status);
		}

		/* Set the file size */
		commonfh_to_fh(sattrargs.file, finfo->objhandle);
#ifdef NFS3
		sattrargs.attributes.mode.set_it = FALSE;
		sattrargs.attributes.uid.set_it = FALSE;
		sattrargs.attributes.gid.set_it = FALSE;
		sattrargs.attributes.atime.set_it = DONT_CHANGE;
		sattrargs.attributes.mtime.set_it = DONT_CHANGE;
		sattrargs.attributes.size.set_it = TRUE;
		sattrargs.attributes.size.u.size = buffer_end - buffer;
		sattrargs.guard.check = FALSE;
#else
		sattrargs.attributes.mode = NOVALUE;
		sattrargs.attributes.uid = NOVALUE;
		sattrargs.attributes.gid = NOVALUE;
		sattrargs.attributes.size = buffer_end - buffer;
		sattrargs.attributes.mtime.seconds = NOVALUE;
		sattrargs.attributes.mtime.useconds = NOVALUE;
		sattrargs.attributes.atime.seconds = NOVALUE;
		sattrargs.attributes.atime.useconds = NOVALUE;
#endif
		err = NFSPROC(SETATTR, (&sattrargs, &sattrres, conn));
		if (err) return err;
		if (sattrres.status != NFS_OK) return ENTRYFUNC(gen_nfsstatus_error) (sattrres.status);

		if (fhandle) *fhandle = &(finfo->objhandle);

		return NULL;
	}

#ifdef NFS3
	createargs.how.mode = GUARDED;
	createargs.how.u.obj_attributes.mode.set_it = TRUE;
	createargs.how.u.obj_attributes.mode.u.mode = 0x00008000 | (0666 & ~(conn->umask));
	createargs.how.u.obj_attributes.uid.set_it = FALSE;
	createargs.how.u.obj_attributes.gid.set_it = FALSE;
	createargs.how.u.obj_attributes.size.set_it = TRUE;
	createargs.how.u.obj_attributes.size.u.size = buffer_end - buffer;
	createargs.how.u.obj_attributes.atime.set_it = SET_TO_SERVER_TIME;
	ENTRYFUNC(loadexec_to_setmtime) (load, exec, &(createargs.how.u.obj_attributes.mtime));
#else
	createargs.attributes.mode = 0x00008000 | (0666 & ~(conn->umask));
	createargs.attributes.uid = NOVALUE;
	createargs.attributes.gid = NOVALUE;
	createargs.attributes.size = buffer_end - buffer;
	createargs.attributes.atime.seconds = NOVALUE;
	createargs.attributes.atime.useconds = NOVALUE;
	ENTRYFUNC(loadexec_to_timeval) (load, exec, &(createargs.attributes.mtime));
#endif
	err = NFSPROC(CREATE, (&createargs, &createres, conn));

	if (err) return err;
	if (createres.status != NFS_OK) return ENTRYFUNC(gen_nfsstatus_error) (createres.status);

#ifdef NFS3
	if (createres.u.diropok.obj.handle_follows == 0) return gen_error(NOATTRS, NOATTRSMESS);
	fh_to_commonfh(fh, createres.u.diropok.obj.u.handle);
#else
	fh_to_commonfh(fh, createres.u.diropok.file);
#endif
	if (fhandle) *fhandle = &fh;

	return NULL;
}

os_error *ENTRYFUNC(file_createfile) (char *filename, unsigned int load, unsigned int exec, char *buffer, char *buffer_end, struct conn_info *conn)
{
	char *leafname;

	return createfile(filename, load, exec, buffer, buffer_end, conn, NULL, &leafname);
}

os_error *ENTRYFUNC(file_createdir) (char *filename, unsigned int load, unsigned int exec, struct conn_info *conn)
{
	struct objinfo *dinfo;
	struct objinfo *finfo;
	os_error *err;
	struct createres createres;
	struct mkdirargs mkdirargs;
	char *leafname;

	err = ENTRYFUNC(filename_to_finfo) (filename, 1, &dinfo, &finfo, &leafname, NULL, NULL, conn);
	if (err) return err;

	if (dinfo) {
		commonfh_to_fh(mkdirargs.where.dir, dinfo->objhandle);
	} else {
		commonfh_to_fh(mkdirargs.where.dir, conn->rootfh);
	}

	mkdirargs.where.name.data = leafname;
	mkdirargs.where.name.size = strlen(leafname);

#ifdef NFS3
	mkdirargs.attributes.mode.set_it = TRUE;
	mkdirargs.attributes.mode.u.mode = 0x00008000 | (0777 & ~(conn->umask));
	mkdirargs.attributes.uid.set_it = FALSE;
	mkdirargs.attributes.gid.set_it = FALSE;
	mkdirargs.attributes.size.set_it = FALSE;
	mkdirargs.attributes.atime.set_it = SET_TO_SERVER_TIME;
	ENTRYFUNC(loadexec_to_setmtime) (load, exec, &(mkdirargs.attributes.mtime));
#else
	mkdirargs.attributes.mode = 0x00008000 | (0777 & ~(conn->umask));
	mkdirargs.attributes.uid = NOVALUE;
	mkdirargs.attributes.gid = NOVALUE;
	mkdirargs.attributes.size = NOVALUE;
	mkdirargs.attributes.atime.seconds = NOVALUE;
	mkdirargs.attributes.atime.useconds = NOVALUE;
	ENTRYFUNC(loadexec_to_timeval) (load, exec, &(mkdirargs.attributes.mtime));
#endif

	err = NFSPROC(MKDIR, (&mkdirargs, &createres, conn));
	if (err) return err;
	if (createres.status != NFS_OK) return ENTRYFUNC(gen_nfsstatus_error) (createres.status);

	return NULL;
}

/* Save a block of memory as a file */
os_error *ENTRYFUNC(file_savefile) (char *filename, unsigned int load, unsigned int exec, char *buffer, char *buffer_end, struct conn_info *conn, char **leafname)
{
	os_error *err;
	struct commonfh *fhandle;

	err = createfile(filename, load, exec, buffer, buffer_end, conn, &fhandle, leafname);
	if (err) return err;

	return ENTRYFUNC(writebytes) (fhandle, buffer, buffer_end - buffer, 0, conn);
}

/* Delete a file or directory */
os_error *ENTRYFUNC(file_delete) (char *filename, struct conn_info *conn, int *objtype, unsigned int *load, unsigned int *exec, int *len, int *attr)
{
	struct objinfo *dinfo;
	struct objinfo *finfo;
	os_error *err;
	char *leafname;
	int filetype;

	err = ENTRYFUNC(filename_to_finfo) (filename, 0, &dinfo, &finfo, &leafname, &filetype, NULL, conn);
	if (err) return err;

	if (finfo == NULL) {
		/* Object not found */
		*objtype = 0;
	} else {
		struct diropargs removeargs;
		struct removeres removeres;

		if (dinfo) {
			commonfh_to_fh(removeargs.dir, dinfo->objhandle);
		} else {
			commonfh_to_fh(removeargs.dir, conn->rootfh);
		}
		removeargs.name.data = leafname;
		removeargs.name.size = strlen(leafname);

		if (finfo->attributes.type == NFDIR) {
			err = NFSPROC(RMDIR, (&removeargs, &removeres, conn));
		} else {
			err = NFSPROC(REMOVE, (&removeargs, &removeres, conn));
		}
		if (err) return err;
		switch (removeres.status) {
			case NFS_OK:
				break;
			case NFSERR_EXIST:
			case NFSERR_NOTEMPTY:
				/* We must return a specific error for this case otherwise
				   Filer_Action may not delete directories properly */
				return gen_error(ERRDIRNOTEMPTY,"Directory not empty");
			default:
				return ENTRYFUNC(gen_nfsstatus_error) (removeres.status);
		}

		/* Treat all special files as if they were regular files */
		*objtype = finfo->attributes.type == NFDIR ? OBJ_DIR : OBJ_FILE;
		ENTRYFUNC(timeval_to_loadexec) (&(finfo->attributes.mtime), filetype, load, exec);
		*len = filesize(finfo->attributes.size);
		*attr = mode_to_attr(finfo->attributes.mode);
	}

	return NULL;
}




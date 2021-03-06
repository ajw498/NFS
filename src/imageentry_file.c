/*
	Routines for ImageEntry_File


	Copyright (C) 2003 Alex Waugh

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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
#ifdef NFS3
	timeval_to_loadexec(finfo->attributes.mtime.seconds, finfo->attributes.mtime.nseconds, filetype, load, exec, 0);
#else
	timeval_to_loadexec(finfo->attributes.mtime.seconds, finfo->attributes.mtime.useconds, filetype, load, exec, 1);
#endif
	*len = filesize(finfo->attributes.size);
	*attr = mode_to_attr(finfo->attributes.mode);

	return NULL;
}

os_error *ENTRYFUNC(file_writecatinfo) (char *filename, unsigned int load, unsigned int exec, int attr, struct conn_info *conn)
{
	struct objinfo *finfo;
	struct objinfo *dinfo;
#ifdef NFS3
	struct sattrargs3 sattrargs;
	struct sattrres3 sattrres;
#else
	struct sattrargs sattrargs;
	struct sattrres sattrres;
#endif
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

	if (conn->unixexfiletype) {
		if ((newfiletype == UNIXEX_FILETYPE) && (filetype != UNIXEX_FILETYPE)) {
			/* The filetype is changing to UnixEx, so set the executable bit(s).
			   The group and other x bits are only set if the corresponding r bit will be set */
			finfo->attributes.mode |= (attr & 0x10) ? 0111 : 0100;
		} else if ((newfiletype != UNIXEX_FILETYPE) && (filetype == UNIXEX_FILETYPE)) {
			/* The filetype is changing from UnixEx, so remove the executable bits. */
			finfo->attributes.mode &= ~0111;
		}
	}

	/* If the filetype has changed we may need to rename the file */
	if (conn->xyzext != NEVER && newfiletype != filetype && finfo->attributes.type == NFREG) {
#ifdef NFS3
		struct renameargs3 renameargs;
		struct renameres3 renameres;
#else
		struct renameargs renameargs;
		struct renameres renameres;
#endif

		if (dinfo) {
			commonfh_to_fh(renameargs.from.dir, dinfo->objhandle);
		} else {
			commonfh_to_fh(renameargs.from.dir, conn->rootfh);
		}
		renameargs.from.name.data = leafname;
		renameargs.from.name.size = strlen(leafname);

		renameargs.to.dir = renameargs.from.dir;
		renameargs.to.name.data = addfiletypeext(leafname, renameargs.from.name.size, extfound, newfiletype, (size_t *)&(renameargs.to.name.size), conn->defaultfiletype, conn->xyzext, conn->unixexfiletype, 0, conn->pool);

		err = NFSPROC(RENAME, (&renameargs, &renameres, conn));
		if (err) return err;
		if (renameres.status != NFS_OK) return ENTRYFUNC(gen_nfsstatus_error) (renameres.status);
	}

	/* Set the datestamp and attributes */
	commonfh_to_fh(sattrargs.file, finfo->objhandle);
#ifdef NFS3
	sattrargs.attributes.mode.set_it = TRUE;
	sattrargs.attributes.mode.u.mode = attr_to_mode(attr, finfo->attributes.mode, conn->umask, conn->unumask);
	sattrargs.attributes.uid.set_it = FALSE;
	sattrargs.attributes.gid.set_it = FALSE;
	sattrargs.attributes.atime.set_it = DONT_CHANGE;
	ENTRYFUNC(loadexec_to_setmtime) (load, exec, &(sattrargs.attributes.mtime));
	sattrargs.attributes.size.set_it = FALSE;
	sattrargs.guard.check = FALSE;
#else
	sattrargs.attributes.mode = attr_to_mode(attr, finfo->attributes.mode, conn->umask, conn->unumask);
	sattrargs.attributes.uid = NOVALUE;
	sattrargs.attributes.gid = NOVALUE;
	sattrargs.attributes.size = NOVALUE;
	sattrargs.attributes.atime.seconds = NOVALUE;
	sattrargs.attributes.atime.useconds = NOVALUE;
	loadexec_to_timeval(load, exec, &(sattrargs.attributes.mtime.seconds), &(sattrargs.attributes.mtime.useconds), 0);
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
#ifdef NFS3
	struct createargs3 createargs;
	struct createres3 createres;
#else
	struct createargs createargs;
	struct createres createres;
#endif
	struct commonfh *fh;
	int filetype;
	int newfiletype;
	int extfound;
	int newmode;

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

	if (conn->unixexfiletype && (newfiletype == UNIXEX_FILETYPE) && (filetype != UNIXEX_FILETYPE)) {
		/* The filetype is changing to UnixEx, so set the executable bits. */
		newmode = 0x00008000 | (0777 & ~(conn->umask));
	} else {
		newmode = 0x00008000 | (0666 & ~(conn->umask));
	}

	/* We may need to add a ,xyz extension */
	createargs.where.name.data = addfiletypeext(*leafname, strlen(*leafname), extfound, newfiletype, (size_t *)&(createargs.where.name.size), conn->defaultfiletype, conn->xyzext, conn->unixexfiletype, 0, conn->pool);

	/* If a file already exists then we must overwrite it */
	if (finfo && finfo->attributes.type == NFREG) {
#ifdef NFS3
		struct sattrargs3 sattrargs;
		struct sattrres3 sattrres;
#else
		struct sattrargs sattrargs;
		struct sattrres sattrres;
#endif

		/* If the filetype has changed we may need to rename the file */
		if (conn->xyzext != NEVER && newfiletype != filetype) {
#ifdef NFS3
			struct renameargs3 renameargs;
			struct renameres3 renameres;
#else
			struct renameargs renameargs;
			struct renameres renameres;
#endif

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

		/* Set the file size and time stamp */
		commonfh_to_fh(sattrargs.file, finfo->objhandle);
#ifdef NFS3
		sattrargs.attributes.mode.set_it = FALSE;
		sattrargs.attributes.uid.set_it = FALSE;
		sattrargs.attributes.gid.set_it = FALSE;
		sattrargs.attributes.atime.set_it = DONT_CHANGE;
		ENTRYFUNC(loadexec_to_setmtime) (load, exec, &(sattrargs.attributes.mtime));
		sattrargs.attributes.size.set_it = TRUE;
		sattrargs.attributes.size.u.size = buffer_end - buffer;
		sattrargs.guard.check = FALSE;
#else
		sattrargs.attributes.mode = NOVALUE;
		sattrargs.attributes.uid = NOVALUE;
		sattrargs.attributes.gid = NOVALUE;
		sattrargs.attributes.size = buffer_end - buffer;
		sattrargs.attributes.atime.seconds = NOVALUE;
		sattrargs.attributes.atime.useconds = NOVALUE;
		loadexec_to_timeval(load, exec, &(sattrargs.attributes.mtime.seconds), &(sattrargs.attributes.mtime.useconds), 1);
#endif
		err = NFSPROC(SETATTR, (&sattrargs, &sattrres, conn));
		if (err) return err;
		if (sattrres.status != NFS_OK) return ENTRYFUNC(gen_nfsstatus_error) (sattrres.status);

		if (fhandle) *fhandle = &(finfo->objhandle);

		return NULL;
	}

#ifdef NFS3
	createargs.how.mode = UNCHECKED;
	createargs.how.u.obj_attributes.mode.set_it = TRUE;
	createargs.how.u.obj_attributes.mode.u.mode = newmode;
	createargs.how.u.obj_attributes.uid.set_it = FALSE;
	createargs.how.u.obj_attributes.gid.set_it = FALSE;
	createargs.how.u.obj_attributes.size.set_it = FALSE;
	createargs.how.u.obj_attributes.atime.set_it = DONT_CHANGE;
	ENTRYFUNC(loadexec_to_setmtime) (load, exec, &(createargs.how.u.obj_attributes.mtime));
#else
	createargs.attributes.mode = newmode;
	createargs.attributes.uid = NOVALUE;
	createargs.attributes.gid = NOVALUE;
	createargs.attributes.size = NOVALUE;
	createargs.attributes.atime.seconds = NOVALUE;
	createargs.attributes.atime.useconds = NOVALUE;
	loadexec_to_timeval(load, exec, &(createargs.attributes.mtime.seconds), &(createargs.attributes.mtime.useconds), 1);
#endif
	err = NFSPROC(CREATE, (&createargs, &createres, conn));

	if (err) return err;
	if (createres.status != NFS_OK) return ENTRYFUNC(gen_nfsstatus_error) (createres.status);

	if ((fh = palloc(sizeof(struct commonfh), conn->pool)) == NULL) return gen_error(NOMEM, NOMEMMESS);
#ifdef NFS3
	if (createres.u.diropok.obj.handle_follows == 0) return gen_error(NOATTRS, NOATTRSMESS);
	fh_to_commonfh(*fh, createres.u.diropok.obj.u.handle);
#else
	fh_to_commonfh(*fh, createres.u.diropok.file);
#endif
	if (fhandle) *fhandle = fh;

	return NULL;
}

os_error *ENTRYFUNC(file_createfile) (char *filename, unsigned int load, unsigned int exec, char *buffer, char *buffer_end, char **leafname, struct conn_info *conn)
{
	return createfile(filename, load, exec, buffer, buffer_end, conn, NULL, leafname);
}

os_error *ENTRYFUNC(file_createdir) (char *filename, unsigned int load, unsigned int exec, struct conn_info *conn)
{
	struct objinfo *dinfo;
	struct objinfo *finfo;
	os_error *err;
#ifdef NFS3
	struct createres3 createres;
	struct mkdirargs3 mkdirargs;
#else
	struct createres createres;
	struct mkdirargs mkdirargs;
#endif
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
	loadexec_to_timeval(load, exec, &(mkdirargs.attributes.mtime.seconds), &(mkdirargs.attributes.mtime.useconds), 1);
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
#ifdef NFS3
	struct sattrargs3 sattrargs;
	struct sattrres3 sattrres;
#else
	struct sattrargs sattrargs;
	struct sattrres sattrres;
#endif

	err = createfile(filename, load, exec, buffer, buffer_end, conn, &fhandle, leafname);
	if (err) return err;

	err = ENTRYFUNC(writebytes) (fhandle, buffer, buffer_end - buffer, 0, NULL, conn);
	if (err) return err;

	/* Set the datestamp. Although this will have already been done by
	   createfile, writebytes will have updated it. */
	commonfh_to_fh(sattrargs.file, *fhandle);
#ifdef NFS3
	sattrargs.attributes.mode.set_it = FALSE;
	sattrargs.attributes.uid.set_it = FALSE;
	sattrargs.attributes.gid.set_it = FALSE;
	sattrargs.attributes.atime.set_it = DONT_CHANGE;
	ENTRYFUNC(loadexec_to_setmtime) (load, exec, &(sattrargs.attributes.mtime));
	sattrargs.attributes.size.set_it = FALSE;
	sattrargs.guard.check = FALSE;
#else
	sattrargs.attributes.mode = NOVALUE;
	sattrargs.attributes.uid = NOVALUE;
	sattrargs.attributes.gid = NOVALUE;
	sattrargs.attributes.size = NOVALUE;
	sattrargs.attributes.atime.seconds = NOVALUE;
	sattrargs.attributes.atime.useconds = NOVALUE;
	loadexec_to_timeval(load, exec, &(sattrargs.attributes.mtime.seconds), &(sattrargs.attributes.mtime.useconds), 1);
#endif
	err = NFSPROC(SETATTR, (&sattrargs, &sattrres, conn));
	if (err) return err;
	if (sattrres.status != NFS_OK) return ENTRYFUNC(gen_nfsstatus_error) (sattrres.status);

	return NULL;
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
#ifdef NFS3
		struct diropargs3 removeargs;
		struct removeres3 removeres;
#else
		struct diropargs removeargs;
		struct removeres removeres;
#endif

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
#ifdef NFS3
		timeval_to_loadexec(finfo->attributes.mtime.seconds, finfo->attributes.mtime.nseconds, filetype, load, exec, 0);
#else
		timeval_to_loadexec(finfo->attributes.mtime.seconds, finfo->attributes.mtime.useconds, filetype, load, exec, 1);
#endif
		*len = filesize(finfo->attributes.size);
		*attr = mode_to_attr(finfo->attributes.mode);
	}

	return NULL;
}




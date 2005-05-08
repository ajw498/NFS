/*
	$Id$
	$URL$

	Routines for ImageEntry_Func


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
#include <ctype.h>
#include <string.h>
#include <swis.h>
#include <unixlib.h>
#include <unistd.h>
#include <sys/param.h>

#include "imageentry_func.h"

#include "moduledefs.h"

#include "sunfish.h"

#include "rpc-calls.h"
#ifdef NFS3
#include "nfs3-calls.h"
#include "mount3-calls.h"
#else
#include "nfs2-calls.h"
#include "mount1-calls.h"
#endif
#include "portmapper-calls.h"
#include "pcnfsd-calls.h"


/* The format of info returned for OS_GBPB 10 */
struct dir_entry {
	unsigned int load;
	unsigned int exec;
	unsigned int len;
	unsigned int attr;
	unsigned int type;
};

os_error *ENTRYFUNC(func_closeimage) (struct conn_info *conn)
{
	string dir;
	os_error *err;

	dir.size = strlen(conn->export);
	dir.data = conn->export;
	err = MNTPROC(UMNT, (&dir, conn));
	if (err && err != ERR_WOULDBLOCK && enablelog) log_error(err);

	/* Close socket etc. */
	err = rpc_close_connection(conn);
	if (err && enablelog) log_error(err);

	if (conn->logging) enablelog--;

	free_conn_info(conn);
	/* Don't return an error as neither of the above is essential,
	   and giving an error will prevent killing the module */
	return NULL;
}

os_error *ENTRYFUNC(func_newimage_mount) (struct conn_info *conn)
{
	string dir;
	struct mountres mountres;
	os_error *err;

	/* Get a filehandle for the root directory */
	dir.size = strlen(conn->export);
	dir.data = conn->export;
	err = MNTPROC(MNT, (&dir, &mountres, conn));
	if (err) return err;
	if (mountres.status != 0) {
		/* status is an errno value. Probably the same as an NFS status value. */
		return ENTRYFUNC(gen_nfsstatus_error) ((enum nstat)mountres.status);
	}
	fh_to_commonfh(conn->rootfh, mountres.u.directory);

	return NULL;
}


/* Read directory entries and possibly info */
os_error *ENTRYFUNC(func_readdirinfo) (int info, char *dirname, char *buffer, int numobjs, int start, int buflen, struct conn_info *conn, int *objsread, int *continuepos)
{
	char *bufferpos;
	struct readdirargs rddir;
	struct readdirres rdres;
	struct entry *direntry = NULL;
	os_error *err;
	int cookie = 0;
	int dirpos = 0;

	bufferpos = buffer;
	*objsread = 0;
#ifdef NFS3
	memset(rddir.cookieverf, 0, NFS3_COOKIEVERFSIZE);
#endif

	if (start != 0 && start == conn->laststart && strcmp(dirname, conn->lastdir) == 0) {
		/* Used cached values */
		commonfh_to_fh(rddir.dir, conn->lastdirhandle);
		cookie = conn->lastcookie;
#ifdef NFS3
		memcpy(rddir.cookieverf, conn->lastcookieverf, NFS3_COOKIEVERFSIZE);
#endif
		dirpos = start;
	} else if (dirname[0] == '\0') {
		/* An empty dirname specifies the root directory */
		commonfh_to_fh(rddir.dir, conn->rootfh);
	} else {
		struct objinfo *finfo;

		/* Find the handle of the directory */
		err = ENTRYFUNC(filename_to_finfo) (dirname, 1, NULL, &finfo, NULL, NULL, NULL, conn);
		if (err) return err;
		if (finfo == NULL) return ENTRYFUNC(gen_nfsstatus_error) (NFSERR_NOENT);

		commonfh_to_fh(rddir.dir, finfo->objhandle);
	}

	while (dirpos <= start) {
		rddir.count = conn->maxdatabuffer;
		rddir.cookie = cookie;
		err = NFSPROC(READDIR, (&rddir, &rdres, conn));
		if (err) return err;
		if (rdres.status != NFS_OK) return ENTRYFUNC(gen_nfsstatus_error) (rdres.status);
	
		/* We may need to do a lookup on each filename, but this would
		   corrupt the rx buffer which holds our list, so swap buffers */
		swap_rxbuffers();
	
#ifdef NFS3
		memcpy(rddir.cookieverf, rdres.u.readdirok.cookieverf, NFS3_COOKIEVERFSIZE);
#endif
		direntry = rdres.u.readdirok.entries;
		while (direntry && *objsread < numobjs) {
			if (direntry->name.size == 0) {
				/* Ignore null names */
			} else if (direntry->name.size == 1 && direntry->name.data[0] == '.') {
				/* current dir */
			} else if (direntry->name.size == 2 && direntry->name.data[0] == '.' && direntry->name.data[1] == '.') {
				/* parent dir */
			} else if (direntry->name.data[0] == '.' && (conn->hidden == 0 || (conn->hidden == 2 && dirname[0] == '\0'))) {
				/* Ignore hidden files if so configured */
			} else {
				if (dirpos >= start) {
					struct dir_entry *info_entry = NULL;
					int filetype;
					int len;
		
					if (info) {
						info_entry = (struct dir_entry *)bufferpos;
						bufferpos += sizeof(struct dir_entry);
					}
		
					/* Check there is room in the output buffer. */
					if (bufferpos > buffer + buflen) break;
		
					/* Copy leafname into output buffer, translating some
					   chars and stripping any ,xyz */
					len = filename_riscosify(direntry->name.data, direntry->name.size, bufferpos, buffer + buflen - bufferpos, &filetype, conn);
					if (len == 0) break; /* Buffer overflowed */
		
					bufferpos += len;
		
					if (info) {
						struct objinfo *lookupres;
						enum nstat status;
						struct commonfh fh;
		
						bufferpos = (char *)(((int)bufferpos + 3) & ~3);
		
						fh_to_commonfh(fh, rddir.dir);

						/* Lookup file attributes.
						   READDIRPLUS in NFS3 would eliminate this call. */
						err = ENTRYFUNC(leafname_to_finfo) (direntry->name.data, &(direntry->name.size), 1, 1, &fh, &lookupres, &status, conn);
						if (err) return err;
						if (status == NFS_OK && lookupres->attributes.type != NFLNK) {
							ENTRYFUNC(timeval_to_loadexec) (&(lookupres->attributes.mtime), filetype, &(info_entry->load), &(info_entry->exec));
							info_entry->len = lookupres->attributes.size;
							info_entry->attr = mode_to_attr(lookupres->attributes.mode);
							info_entry->type = lookupres->attributes.type == NFDIR ? OBJ_DIR : OBJ_FILE;
						} else {
							/* An error occured, either the file no longer exists, or
							   it is a symlink that doesn't point to a valid object. */
							info_entry->load = 0xDEADDEAD;
							info_entry->exec = 0xDEADDEAD;
							info_entry->len = 0;
							info_entry->attr = 0;
							info_entry->type = OBJ_FILE;
						}
					}
	
					(*objsread)++;
				}
				dirpos++;
			}
			cookie = direntry->cookie;
			direntry = direntry->next;
		}
		if (rdres.u.readdirok.eof) break;
	}

	if (direntry == NULL && rdres.u.readdirok.eof) {
		*continuepos = -1;
		conn->laststart = 0;
		/* If there are entries still in the buffer that won't fit in the
		   output buffer then we should cache them to save rerequesting
		   them on the next iteration */
	} else {
		int len;

		/* Ideally, we would just return the cookie to the user, but
		   RISC OS seems to treat any value with the top bit set
		   as if it were -1
		   Also Filer_Action seems to make assumptions that it
		   really shouldn't, and treats the value being the position
		   within the directory */
		*continuepos = dirpos;
		conn->lastcookie = cookie;
		fh_to_commonfh(conn->lastdirhandle, rddir.dir);
#ifdef NFS3
		memcpy(conn->lastcookieverf, rdres.u.readdirok.cookieverf, NFS3_COOKIEVERFSIZE);
#endif
		len = strlen(dirname);
		if (len < MAXNAMLEN) {
			memcpy(conn->lastdir, dirname, len + 1);
			conn->laststart = dirpos;
		} else {
			/* No room to cache dirname, so don't bother */
			conn->laststart = 0;
		}
		/* At this point we could speculatively request the next set of
		   entries to reduce the latency on the next request */
	}

	return NULL;
}


/* Rename a file or directory */
os_error *ENTRYFUNC(func_rename) (char *oldfilename, char *newfilename, struct conn_info *conn, int *renamefailed)
{
	struct objinfo *dinfo;
	struct objinfo *finfo;
	struct renameargs renameargs;
	struct renameres renameres;
	os_error *err;
	char *leafname;
	static char oldleafname[MAXNAMLEN];
	int filetype;
	int dirnamelen;
	unsigned int leafnamelen;

	err = ENTRYFUNC(filename_to_finfo) (oldfilename, 1, &dinfo, &finfo, &leafname, &filetype, NULL, conn);
	if (err) return err;
	if (finfo == NULL) return ENTRYFUNC(gen_nfsstatus_error) (NFSERR_NOENT);

	if (dinfo) {
		commonfh_to_fh(renameargs.from.dir, dinfo->objhandle);
	} else {
		commonfh_to_fh(renameargs.from.dir, conn->rootfh);
	}
	strcpy(oldleafname, leafname);
	renameargs.from.name.data = oldleafname;
	renameargs.from.name.size = strlen(oldleafname);

	dirnamelen = strlen(oldfilename) - renameargs.from.name.size;

	if (strncmp(oldfilename, newfilename, dirnamelen) == 0 &&
	    newfilename[dirnamelen] &&
	    strchr(newfilename + dirnamelen + 1, '.') == NULL) {
		/* Both files are in the same directory */
		renameargs.to.dir = renameargs.from.dir;
		leafname = newfilename + dirnamelen;
		leafname = filename_unixify(leafname, strlen(leafname), &leafnamelen);
	} else {
		/* Files are in different directories, so find the handle of the new dir */
		err = ENTRYFUNC(filename_to_finfo) (newfilename, 1, &dinfo, NULL, &leafname, NULL, NULL, conn);
		if (err) return err;

		leafnamelen = strlen(leafname);
		if (dinfo) {
			commonfh_to_fh(renameargs.to.dir, dinfo->objhandle);
		} else {
			commonfh_to_fh(renameargs.to.dir, conn->rootfh);
		}
	}

	/* Add ,xyz on if necessary to preserve filetype */
	renameargs.to.name.data = addfiletypeext(leafname, leafnamelen, 0, filetype, &(renameargs.to.name.size), conn);

	err = NFSPROC(RENAME, (&renameargs, &renameres, conn));
	if (err) return err;
	if (renameres.status != NFS_OK) return ENTRYFUNC(gen_nfsstatus_error) (renameres.status);

	*renamefailed = 0;
	return NULL;
}


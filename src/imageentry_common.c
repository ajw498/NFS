/*
	Common routines shared between entry points


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


#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <swis.h>
#include <stdarg.h>
#include <stdio.h>
#ifdef USE_TCPIPLIBS
# include <unixlib.h>
# include <riscos.h>
#else
# include <strings.h>
#endif

#include "imageentry_common.h"
#include "imageentry_func.h"

#include "sunfish.h"

#ifdef NFS3
#include "nfs3-calls.h"
#else
#include "nfs2-calls.h"
#endif

#include "rpc.h"


/* Generate a RISC OS error block based on the NFS status code */
os_error *ENTRYFUNC(gen_nfsstatus_error) (nstat stat)
{
	char *str;

	switch (stat) {
		case NFSERR_PERM: str = "Not owner"; break;
		case NFSERR_NOENT: str = "No such file or directory"; break;
		case NFSERR_IO: str = "IO error"; break;
		case NFSERR_NXIO: str = "No such device or address"; break;
		case NFSERR_ACCES: str = "Permission denied"; break;
		case NFSERR_EXIST: str = "File already exists"; break;
		case NFSERR_NODEV: str = "No such device"; break;
		case NFSERR_NOTDIR: str = "Not a directory"; break;
		case NFSERR_ISDIR: str = "Is a directory"; break;
		case NFSERR_FBIG: str = "File too large"; break;
		case NFSERR_NOSPC: str = "No space left on device"; break;
		case NFSERR_ROFS: str = "Read only filesystem"; break;
		case NFSERR_NAMETOOLONG: str = "File name too long"; break;
		case NFSERR_NOTEMPTY: str = "Directory not empty"; break;
		case NFSERR_DQUOT: str = "Disc quota exceeded"; break;
		case NFSERR_STALE: str = "Stale NFS filehandle"; break;
#ifdef NFS3
		case NFSERR_XDEV: str = "Attempt to do a cross-device hard link"; break;
		case NFSERR_INVAL: str = "Invalid argument"; break;
		case NFSERR_MLINK: str = "Too many hard links"; break;
		case NFSERR_BADHANDLE: str = "Illegal NFS file handle"; break;
		case NFSERR_NOT_SYNC: str = "Update synchronization mismatch"; break;
		case NFSERR_BAD_COOKIE: str = "Stale cookie"; break;
		case NFSERR_NOTSUPP: str = "Operation is not supported"; break;
		case NFSERR_TOOSMALL: str = "Buffer or request is too small"; break;
		case NFSERR_SERVERFAULT: str = "Server fault"; break;
		case NFSERR_BADTYPE: str = "Bad type"; break;
		case NFSERR_JUKEBOX: str = "Jukebox error"; break;
#endif
		default: str = "Unknown error"; break;
	}
	return gen_error(NFSSTATBASE, "NFS call failed (%s)", str);
}

/* Find leafname,xyz given leafname by enumerating the entire directory
   until a matching file is found. If there are two matching files, the
   first found is used. This function would benefit in many cases by some
   cacheing of leafnames and extensions. */
static os_error *lookup_leafname(struct commonfh *dhandle, char *leafname, int leafnamelen, struct opaque **found, struct conn_info *conn)
{
#ifdef NFS3
	struct readdirargs3 rddir;
	struct readdirres3 rdres;
	struct entry3 *direntry;
#else
	struct readdirargs rddir;
	struct readdirres rdres;
	struct entry *direntry;
#endif
	os_error *err;

	if ((*found = palloc(sizeof(struct opaque), conn->pool)) == NULL) return gen_error(NOMEM, NOMEMMESS);

	commonfh_to_fh(rddir.dir, *dhandle);
	rddir.count = conn->maxdatabuffer;
	rddir.cookie = 0;
#ifdef NFS3
	memset(rddir.cookieverf, 0, NFS3_COOKIEVERFSIZE);
#endif

	do {
		err = NFSPROC(READDIR, (&rddir, &rdres, conn));
		if (err) return err;
		if (rdres.status != NFS_OK) return ENTRYFUNC(gen_nfsstatus_error) (rdres.status);

#ifdef NFS3
		memcpy(rddir.cookieverf, rdres.u.readdirok.cookieverf, NFS3_COOKIEVERFSIZE);
#endif

		direntry = rdres.u.readdirok.entries;
		while (direntry) {
			rddir.cookie = direntry->cookie;

			if (!conn->casesensitive && direntry->name.size == leafnamelen) {
				if (strcasecmp(direntry->name.data, leafname) == 0) {
					(*found)->size = direntry->name.size;
					(*found)->data = pmemdup(direntry->name.data, direntry->name.size, conn->pool);
					if ((*found)->data == NULL) return gen_error(NOMEM, NOMEMMESS);
					return NULL;
				}
			}

			if (conn->xyzext != NEVER
			    && direntry->name.size == leafnamelen + sizeof(",xyz") - 1) {

				if (direntry->name.data[leafnamelen] == ','
				     && isxdigit(direntry->name.data[leafnamelen + 1])
				     && isxdigit(direntry->name.data[leafnamelen + 2])
				     && isxdigit(direntry->name.data[leafnamelen + 3])) {
					if (conn->casesensitive) {
						if (strncmp(direntry->name.data, leafname, leafnamelen) == 0) {
							(*found)->size = direntry->name.size;
							(*found)->data = pmemdup(direntry->name.data, direntry->name.size, conn->pool);
							if ((*found)->data == NULL) return gen_error(NOMEM, NOMEMMESS);
							return NULL;
						}
					} else {
						if (strncasecmp(direntry->name.data, leafname, leafnamelen) == 0) {
							(*found)->size = direntry->name.size;
							(*found)->data = pmemdup(direntry->name.data, direntry->name.size, conn->pool);
							if ((*found)->data == NULL) return gen_error(NOMEM, NOMEMMESS);
							return NULL;
						}
					}
				}
			}
			direntry = direntry->next;
		}
	} while (!rdres.u.readdirok.eof);

	*found = NULL;
	return NULL;
}

/* Convert a leafname into nfs handle and attributes.
   May follow symlinks if needed.
   Returns a pointer to objinfo data, and updates the leafname in place (but only if simple is not set). */
os_error *ENTRYFUNC(leafname_to_finfo) (char *leafname, size_t *len, int simple, int followsymlinks, struct commonfh *dirhandle, struct objinfo **finfo, nstat *status, struct conn_info *conn)
{
#ifdef NFS3
	struct diropargs3 lookupargs;
	struct diropres3 lookupres;
#else
	struct diropargs lookupargs;
	struct diropres lookupres;
#endif
	struct objinfo *retinfo;
	os_error *err;
	int follow;
	int stale = 0;

	if ((retinfo = palloc(sizeof(struct objinfo), conn->pool)) == NULL) return gen_error(NOMEM, NOMEMMESS);

	if (leafname[0]) {
restart:
		lookupargs.name.data = leafname;
		lookupargs.name.size = *len;
		commonfh_to_fh(lookupargs.dir, *dirhandle);

		err = NFSPROC(LOOKUP, (&lookupargs, &lookupres, conn));
		if (err) return err;
		if ((lookupres.status == NFSERR_STALE) && !stale) {
			stale = 1;
			ENTRYFUNC(func_newimage_mount) (conn);
			goto restart;
		}

		if (!simple && lookupres.status == NFSERR_NOENT) {
			struct opaque *file;

			err = lookup_leafname(dirhandle, leafname, *len, &file, conn);
			if (err) return err;
			if (file == NULL) {
				*status = NFSERR_NOENT;
				return NULL;
			}

			lookupargs.name.data = file->data;
			lookupargs.name.size = file->size;
			memcpy(leafname, file->data, file->size);
			*len = file->size;
			leafname[*len] = '\0';

			err = NFSPROC(LOOKUP, (&lookupargs, &lookupres, conn));
			if (err) return err;
		}

		if (lookupres.status != NFS_OK) {
			*status = lookupres.status;
			return NULL;
		}

		fh_to_commonfh(retinfo->objhandle, lookupres.u.diropok.file);
#ifdef NFS3
		if (lookupres.u.diropok.obj_attributes.attributes_follow == FALSE) {
			return gen_error(NOATTRS, NOATTRSMESS);
		}
		retinfo->attributes = lookupres.u.diropok.obj_attributes.u.attributes;
#else
		retinfo->attributes = lookupres.u.diropok.attributes;
#endif
	} else {
#ifdef NFS3
		struct GETATTR3args getattrargs;
		struct GETATTR3res getattrres;
#else
		struct getattrargs getattrargs;
		struct attrstat getattrres;
#endif

restart2:
		commonfh_to_fh(getattrargs.fhandle, *dirhandle);

		err = NFSPROC(GETATTR, (&getattrargs, &getattrres, conn));
		if (err) return err;
		if ((getattrres.status == NFSERR_STALE) && !stale) {
			stale = 1;
			ENTRYFUNC(func_newimage_mount) (conn);
			goto restart2;
		}
		if (getattrres.status != NFS_OK) {
			*status = lookupres.status;
			return NULL;
		}

		retinfo->objhandle = *dirhandle;
#ifdef NFS3
		retinfo->attributes = lookupres.u.diropok.obj_attributes.u.attributes = getattrres.u.resok.obj_attributes;
#else
		retinfo->attributes = lookupres.u.diropok.attributes = getattrres.u.attributes;
#endif
		commonfh_to_fh(lookupres.u.diropok.file, *dirhandle);
	}
	*finfo = retinfo;

	follow = followsymlinks ? conn->followsymlinks : 0;

#ifdef NFS3
	while (follow > 0 && lookupres.u.diropok.obj_attributes.u.attributes.type == NFLNK) {
		struct readlinkargs3 linkargs;
		struct readlinkres3 linkres;
#else
	while (follow > 0 && lookupres.u.diropok.attributes.type == NFLNK) {
		struct readlinkargs linkargs;
		struct readlinkres linkres;
#endif
		char *segment;
		unsigned int segmentmaxlen;
		static char link[MAX_PATHNAME];

		linkargs.fhandle = lookupres.u.diropok.file;
		err = NFSPROC(READLINK, (&linkargs, &linkres, conn));
		if (err) return err;
		if (linkres.status != NFS_OK) {
			*status = linkres.status;
			return NULL;
		}

#ifdef NFS3
		if (lookupres.u.diropok.obj_attributes.attributes_follow == FALSE) {
			return gen_error(NOATTRS, NOATTRSMESS);
		}
#endif

		segmentmaxlen = linkres.u.resok.data.size;
		if (segmentmaxlen > MAX_PATHNAME) segmentmaxlen = MAX_PATHNAME;
		memcpy(link, linkres.u.resok.data.data, segmentmaxlen);
		segment = link;
		if (segmentmaxlen > 0 && segment[0] == '/') {
			int exportlen = strlen(conn->exportname);

			/* An absolute link. We can only handle these if they are under
			   the exported directory */
			if (segmentmaxlen > exportlen && strncmp(segment, conn->exportname, exportlen) == 0) {
				segment += exportlen;
				segmentmaxlen -= exportlen;
				while (segmentmaxlen > 0 && *segment == '/') {
					segment++;
					segmentmaxlen--;
				}
				dirhandle = &(conn->rootfh);
			} else {
				/* Not within the export. Return the details
				   of the symlink instead. If we returned
				   NFSERR_NOENT then you wouldn't be able
				   to delete the link. */
				*status = NFS_OK;
				return NULL;
			}
		}

		/* segment must now be a link relative to dirhandle */
		while (segmentmaxlen > 0) {
			int i = 0;

			while (i < segmentmaxlen && segment[i] != '/') i++;
	
			lookupargs.name.data = segment;
			lookupargs.name.size = i;
			commonfh_to_fh(lookupargs.dir, *dirhandle);

			err = NFSPROC(LOOKUP, (&lookupargs, &lookupres, conn));
			if (err) return err;
			if (lookupres.status != NFS_OK) {
				if (lookupres.status == NFSERR_NOENT) {
					/* Return the details for the last
					   valid symlink */
					*status = NFS_OK;
				} else {
					*status = lookupres.status;
				}
				return NULL;
			}
			fh_to_commonfh(retinfo->objhandle, lookupres.u.diropok.file);

#ifdef NFS3
			if (lookupres.u.diropok.obj_attributes.u.attributes.type == NFDIR) {
#else
			if (lookupres.u.diropok.attributes.type == NFDIR) {
#endif
				dirhandle = &(retinfo->objhandle);
			}
			while (i < segmentmaxlen && segment[i] == '/') i++;
			segment += i;
			segmentmaxlen -= i;
		}
#ifdef NFS3
		if (lookupres.u.diropok.obj_attributes.attributes_follow == FALSE) {
			return gen_error(NOATTRS, NOATTRSMESS);
		}
		retinfo->attributes = lookupres.u.diropok.obj_attributes.u.attributes;
#else
		retinfo->attributes = lookupres.u.diropok.attributes;
#endif
		follow--;
	}

	*status = NFS_OK;

	return NULL;
}

/* Convert a full filename/dirname into a leafname, and find the nfs handle
   for the file/dir and for the directory containing it.
   This function would really benefit from some cacheing.
   In theory it should deal with wildcarded names, but that is just too much
   hassle. And, really, that should be the job of fileswitch. */
os_error *ENTRYFUNC(filename_to_finfo) (char *filename, int followsymlinks, struct objinfo **dinfo, struct objinfo **finfo, char **leafname, int *filetype, int *extfound, struct conn_info *conn)
{
	char *start = filename;
	char *end;
	struct commonfh dirhandle;
	struct objinfo *dirinfo;
	char *segmentname;
	size_t segmentlen;
	struct objinfo *segmentinfo;
	nstat status;
	os_error *err;

	if ((dirinfo = palloc(sizeof(struct objinfo), conn->pool)) == NULL) return gen_error(NOMEM, NOMEMMESS);
	dirhandle = conn->rootfh;

	if (dinfo) *dinfo = NULL; /* A NULL directory indicates the root directory */
	if (finfo) *finfo = NULL; /* A NULL file indicates it wasn't found */
	if (extfound) *extfound = 0; /* Was an explicit ,xyz found */

	do {
		end = start;
		/* Find end of dirname segment */
		while (*end && *end != '.') end++;

		segmentname = filename_unixify(start, end - start, &segmentlen, conn->escapewin, conn->pool);

		if (conn->toenc != (iconv_t)-1) {
			char *encleaf;
			size_t encleaflen;
			static char buffer2[MAX_PATHNAME];

			encleaf = buffer2;
			encleaflen = sizeof(buffer2);
			if (iconv(conn->toenc, &segmentname, &segmentlen, &encleaf, &encleaflen) == -1) {
				iconv(conn->toenc, NULL, NULL, NULL, NULL);
				return gen_error(ICONVERR, "Iconv failed when converting a filename (%d)", errno);
			}

			encleaflen = sizeof(buffer2) - encleaflen;
			if ((segmentname = palloc(encleaflen + sizeof(",xyz"), conn->pool)) == NULL) return gen_error(NOMEM, NOMEMMESS);
			memcpy(segmentname, buffer2, encleaflen);
			segmentname[encleaflen] = '\0';
			segmentlen = encleaflen;
		}

		if (leafname) *leafname = segmentname;

		err = ENTRYFUNC(leafname_to_finfo) (segmentname, &segmentlen, 0, followsymlinks || (*end != '\0'), &dirhandle, &segmentinfo, &status, conn);
		if (err) return err;

		/* It is not an error if the file isn't found, but the
		   containing directory must exist */
		if (*end == '\0' && status == NFSERR_NOENT) break;

		if (dinfo == NULL && (status == NFSERR_NOENT || status == NFSERR_NOTDIR)) {
			/* If the caller wants info on the directory then it is an error if
			   the dir isn't found, but if they only want info on the file
			   (eg. read cat info) then it is not an error if we can't find it */
			return NULL;
		}

		if (status != NFS_OK) return ENTRYFUNC(gen_nfsstatus_error) (status);

		if (*end != '\0') {
			if (segmentinfo->attributes.type != NFDIR) {
				/* Every segment except the leafname should be a directory */
				if (dinfo) {
					return ENTRYFUNC(gen_nfsstatus_error) (NFSERR_NOTDIR);
				} else {
					return NULL;
				}
			}
			if (dinfo) {
				*dirinfo = *segmentinfo;
				*dinfo = dirinfo;
			}
		}
		/* Use memmove rather than just assign or memcpy to avoid
		   a bug in Norcroft 5.54 which generates an infinite loop */
		if (*end != '\0') memmove(&dirhandle, &(segmentinfo->objhandle), sizeof(struct commonfh));
		start = end + 1;
	} while (*end != '\0');

	/* By this point, the directory has been found, the file may or
	   may not have been found */

	/* There's no need to continue if the caller doesn't care about the file */
	if (finfo == NULL) return NULL;

	if (status != NFS_OK) return NULL;

	*finfo = segmentinfo;

	if (filetype) {
		if (segmentinfo->attributes.type == NFDIR) {
			*filetype = DIR_FILETYPE;
		} else if (conn->unixexfiletype && ((segmentinfo->attributes.mode & 0111) != 0)) {
			/* One of the executable bits is set, so set the filetype to UnixEx */
			*filetype = UNIXEX_FILETYPE;
			if (segmentlen > 4 && segmentname[segmentlen - 4] == ','
			          && isxdigit(segmentname[segmentlen - 3])
			          && isxdigit(segmentname[segmentlen - 2])
			          && isxdigit(segmentname[segmentlen - 1])
			          && extfound) *extfound = 1;
		} else if (conn->xyzext == NEVER) {
			/* Not configured to use the mimetype */
			*filetype = conn->defaultfiletype;
		} else {
			/* Work out the filetype */
			if (segmentlen > 4 && segmentname[segmentlen - 4] == ','
			          && isxdigit(segmentname[segmentlen - 3])
			          && isxdigit(segmentname[segmentlen - 2])
			          && isxdigit(segmentname[segmentlen - 1])) {
				/* There is an ,xyz extension */
				*filetype = (int)strtol(segmentname + segmentlen - 3, NULL, 16);

				if (extfound) *extfound = 1;
			} else {
				/* No ,xyz extension, so use mimemap if possible */
				int lastdot = segmentlen;
				while (lastdot > 0 && segmentname[lastdot] != '.') lastdot--;
	
				if (lastdot) {
					*filetype = ext_to_filetype(segmentname + lastdot + 1, conn->defaultfiletype);
				} else {
					/* No ,xyz and no extension, so use the default */
					*filetype = conn->defaultfiletype;
				}
			}
		}
	}

	return NULL;
}

#ifdef NFS3
void ENTRYFUNC(loadexec_to_setmtime) (unsigned int load, unsigned int exec, struct set_mtime *mtime)
{
	if ((load & 0xFFF00000) != 0xFFF00000) {
		mtime->set_it = DONT_CHANGE;
	} else {
		mtime->set_it = SET_TO_CLIENT_TIME;
		loadexec_to_timeval(load, exec, &(mtime->u.mtime.seconds), &(mtime->u.mtime.nseconds), 0);
	}
}
#endif



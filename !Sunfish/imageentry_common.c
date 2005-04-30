/*
	$Id$
	$URL$

	Common routines shared between entry points


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
#include <stdarg.h>
#include <stdio.h>
#include <unixlib.h>

#include "imageentry_func.h"

#include "sunfish.h"

#ifdef NFS3
#include "nfs3-calls.h"
#else
#include "nfs2-calls.h"
#endif

#include "rpc.h"


/* Generate a RISC OS error block based on the given number and message */
os_error *gen_error(int num, char *msg, ...)
{
	static os_error module_err_buf;
	va_list ap;

	rpc_resetfifo();
	
	va_start(ap, msg);
	vsnprintf(module_err_buf.errmess, sizeof(module_err_buf.errmess), msg, ap);
	va_end(ap);
	module_err_buf.errnum = num;
	return &module_err_buf;
}

/* Generate a RISC OS error block based on the NFS status code */
os_error *gen_nfsstatus_error(enum nstat stat)
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
		default: str = "Unknown error"; break; /*FIXME*/
	}
	return gen_error(NFSSTATBASE, "NFS call failed (%s)", str);
}

char *filename_unixify(char *name, unsigned int len, unsigned int *newlen)
{
	static char namebuffer[MAXNAMLEN + 1];
	int i;
	int j;

	/* Truncate if there is not enough buffer space. This is slightly
	   pessimistic if the name has escape sequences */
	if (len > MAXNAMLEN) len = MAXNAMLEN;

	for (i = 0, j = 0; i < len; i++) {
		switch (name[i]) {
			case '/':
				namebuffer[j++] = '.';
				break;
			case 160: /*hard space*/
				namebuffer[j++] = ' ';
				break;
			case '?':
				if (i + 2 < len && isxdigit(name[i+1]) && !islower(name[i+1]) && isxdigit(name[i+2]) && !islower(name[i+2])) {
					/* An escape sequence */
					i++;
					namebuffer[j] = (name[i] > '9' ? name[i] - 'A' + 10 : name[i] - '0') << 4;
					i++;
					namebuffer[j++] |= name[i] > '9' ? name[i] - 'A' + 10 : name[i] - '0';
				} else {
					namebuffer[j++] = name[i];
				}
				break;
			default:
				namebuffer[j++] = name[i];
		}
	}

	namebuffer[j] = '\0';
	*newlen = j;

	return namebuffer;
}

#define MimeMap_Translate 0x50B00
#define MMM_TYPE_RISCOS 0
#define MMM_TYPE_RISCOS_STRING 1
#define MMM_TYPE_MIME 2
#define MMM_TYPE_DOT_EXTN 3

/* Use MimeMap to lookup a filetype from an extension */
static int lookup_mimetype(char *ext, struct conn_info *conn)
{
	os_error *err;
	int filetype;

	/* Try MimeMap to get a filetype, use the default type if the call fails */
	err = _swix(MimeMap_Translate,_INR(0,2) | _OUT(3), MMM_TYPE_DOT_EXTN, ext, MMM_TYPE_RISCOS, &filetype);
	if (err) filetype = conn->defaultfiletype;

	return filetype;
}


int filename_riscosify(char *name, int namelen, char *buffer, int buflen, int *filetype, struct conn_info *conn)
{
	int i;
	int j;
	char *dotext = NULL;

	*filetype = -1;

	for (i = 0, j = 0; (i < namelen) && (j + 3 < buflen); i++) {
		if (name[i] == '.') {
			buffer[j++] = '/';
			dotext = buffer + j;
		} else if (name[i] == ' ') {
			buffer[j++] = 160; /* spaces to hard spaces */
		} else if (name[i] == ',') {
			if (conn->xyzext != NEVER && namelen - i == 4
			     && isxdigit(name[i+1])
			     && isxdigit(name[i+2])
			     && isxdigit(name[i+3])) {
				char tmp[4];

				tmp[0] = name[i+1];
				tmp[1] = name[i+2];
				tmp[2] = name[i+3];
				tmp[3] = '\0';
				*filetype = (int)strtol(tmp, NULL, 16);
				namelen -= sizeof(",xyz") - 1;
			} else {
				buffer[j++] = name[i];
			}
		} else if (name[i] == '!') {
			buffer[j++] = name[i];
		} else if (name[i] < '\''
				|| name[i] == '/'
				|| name[i] == ':'
				|| name[i] == '?'
				|| name[i] == '@'
				|| name[i] == '\\'
				|| name[i] == 127
				|| name[i] == 160) {
			int val;

			/* Turn illegal chars into ?XX escape sequences */
			buffer[j++] = '?';
			val = ((name[i] & 0xF0) >> 4);
			buffer[j++] = val < 10 ? val + '0' : (val - 10) + 'A';
			val = (name[i] & 0x0F);
			buffer[j++] = val < 10 ? val + '0' : (val - 10) + 'A';
		} else {
			/* All other chars translate unchanged */
			buffer[j++] = name[i];
		}
	}

	if (i < namelen) return 0; /* Buffer overflow */

	buffer[j++] = '\0';

	if (conn->xyzext != NEVER) {
		if (*filetype == -1) {
			/* No ,xyz found */
			if (dotext) {
				*filetype = lookup_mimetype(dotext, conn);
			} else {
				/* No ,xyz and no extension, so use default */
				*filetype = conn->defaultfiletype;
			}
		}
	} else {
		*filetype = conn->defaultfiletype;
	}

	return j;
}

/* Find leafname,xyz given leafname by enumerating the entire directory
   until a matching file is found. If there are two matching files, the
   first found is used. This function would benefit in many cases by some
   cacheing of leafnames and extensions. The returned leafname is a pointer
   to static data and should be copied before any subsequent calls. */
static os_error *lookup_leafname(struct nfs_fh *dhandle, char *leafname, int leafnamelen, struct opaque **found, struct conn_info *conn)
{
	static struct readdirargs rddir;
	static struct readdirres rdres;
	struct entry *direntry;
	os_error *err;

	rddir.dir = *dhandle;
	rddir.count = conn->maxdatabuffer;
	rddir.cookie = 0;
#ifdef NFS3
	memset(rddir.cookieverf, 0, NFS3_COOKIEVERFSIZE);
	/*FIXME and handle a bad cookie error? */
#endif

	do {
		err = NFSPROC(READDIR, (&rddir, &rdres, conn));
		if (err) return err;
		if (rdres.status != NFS_OK) return gen_nfsstatus_error(rdres.status);

#ifdef NFS3
		memcpy(rddir.cookieverf, rdres.u.readdirok.cookieverf, NFS3_COOKIEVERFSIZE);
#endif

		direntry = rdres.u.readdirok.entries;
		while (direntry) {
			rddir.cookie = direntry->cookie;

			if (!conn->casesensitive && direntry->name.size == leafnamelen) {
				if (strcasecmp(direntry->name.data, leafname) == 0) {
					*found = &(direntry->name);
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
							*found = &(direntry->name);
							return NULL;
						}
					} else {
						if (strncasecmp(direntry->name.data, leafname, leafnamelen) == 0) {
							*found = &(direntry->name);
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
   Returns a pointer to static data, and updates the leafname in place. */
os_error *leafname_to_finfo(char *leafname, unsigned int *len, int simple, int followsymlinks, struct nfs_fh *dirhandle, struct objinfo **finfo, enum nstat *status, struct conn_info *conn)
{
	struct diropargs lookupargs;
	struct diropres lookupres;
	static struct objinfo retinfo;
	os_error *err;
	int follow;

	lookupargs.name.data = leafname;
	lookupargs.name.size = *len;
	lookupargs.dir = *dirhandle;

	err = NFSPROC(LOOKUP, (&lookupargs, &lookupres, conn));
	if (err) return err;

	if (!simple && lookupres.status == NFSERR_NOENT) {
		struct opaque *file;

		err = lookup_leafname(dirhandle, leafname, *len, &file, conn);
		if (err) return err;
		if (file == NULL) {
			*status = NFSERR_NOENT;
			return NULL;
		}

		lookupargs.name = *file;
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

#ifdef NFS3
	if (lookupres.u.diropok.obj_attributes.attributes_follow == FALSE) {
		gen_error(NOATTRS, NOATTRSMESS);
	}
#endif

	retinfo.objhandle = lookupres.u.diropok.file;
#ifdef NFS3
	memcpy(retinfo.handledata, lookupres.u.diropok.file.data.data, lookupres.u.diropok.file.data.size);
	retinfo.objhandle.data.data = retinfo.handledata;
	retinfo.attributes = lookupres.u.diropok.obj_attributes.u.attributes;
#else
	retinfo.attributes = lookupres.u.diropok.attributes;
#endif
	*finfo = &retinfo;

	follow = followsymlinks ? conn->followsymlinks : 0;

#ifdef NFS3
	while (follow > 0 && lookupres.u.diropok.obj_attributes.u.attributes.type == NFLNK) {
#else
	while (follow > 0 && lookupres.u.diropok.attributes.type == NFLNK) {
#endif
		struct readlinkargs linkargs;
		struct readlinkres linkres;
		char *segment;
		unsigned int segmentmaxlen;
		static char link[MAXNAMLEN/*MAXPATHLEN*/];

		linkargs.fhandle = lookupres.u.diropok.file;
		err = NFSPROC(READLINK, (&linkargs, &linkres, conn));
		if (err) return err;
		if (linkres.status != NFS_OK) {
			*status = linkres.status;
			return NULL;
		}

#ifdef NFS3
		if (lookupres.u.diropok.obj_attributes.attributes_follow == FALSE) {
			gen_error(NOATTRS, NOATTRSMESS);
		}
#endif

#ifdef NFS3
		memcpy(link, linkres.u.resok.data.data, linkres.u.resok.data.size);
#else
		memcpy(link, linkres.u.data.data, linkres.u.data.size);
#endif
		segment = link;
#ifdef NFS3
		segmentmaxlen = linkres.u.resok.data.size;
#else
		segmentmaxlen = linkres.u.data.size;
#endif
		if (segmentmaxlen > 0 && segment[0] == '/') {
			int exportlen = strlen(conn->export);

			/* An absolute link. We can only handle these if they are under
			   the exported directory */
			if (segmentmaxlen > exportlen && strncmp(segment, conn->export, exportlen) == 0) {
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
			lookupargs.dir = *dirhandle;

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
#ifdef NFS3
			if (lookupres.u.diropok.obj_attributes.attributes_follow == FALSE) {
				gen_error(NOATTRS, NOATTRSMESS);
			}
			retinfo.attributes = lookupres.u.diropok.obj_attributes.u.attributes;
			memcpy(retinfo.handledata, lookupres.u.diropok.file.data.data, lookupres.u.diropok.file.data.size); 
			retinfo.objhandle.data.size = lookupres.u.diropok.file.data.size;
			retinfo.objhandle.data.data = retinfo.handledata;
#else
			retinfo.attributes = lookupres.u.diropok.attributes;
			retinfo.objhandle = lookupres.u.diropok.file;
#endif

#ifdef NFS3
			if (lookupres.u.diropok.obj_attributes.u.attributes.type == NFDIR) {
#else
			if (lookupres.u.diropok.attributes.type == NFDIR) {
#endif
				dirhandle = &(lookupres.u.diropok.file);
			}
			while (i < segmentmaxlen && segment[i] == '/') i++;
			segment += i;
			segmentmaxlen -= i;
		}
		follow--;
	}

	*status = NFS_OK;

	return NULL;
}

/* Convert a full filename/dirname into a leafname, and find the nfs handle
   for the file/dir and for the directory containing it.
   Returns a pointer to static data.
   This function would really benefit from some cacheing.
   In theory it should deal with wildcarded names, but that is just too much
   hassle. And, really, that should be the job of fileswitch. */
os_error *filename_to_finfo(char *filename, int followsymlinks, struct objinfo **dinfo, struct objinfo **finfo, char **leafname, int *filetype, int *extfound, struct conn_info *conn)
{
	char *start = filename;
	char *end;
	struct nfs_fh dirhandle;
	static struct objinfo dirinfo;
	char *segmentname;
	unsigned int segmentlen;
	struct objinfo *segmentinfo;
	enum nstat status;
	os_error *err;

	dirhandle = conn->rootfh;

	if (dinfo) *dinfo = NULL; /* A NULL directory indicates the root directory */
	if (finfo) *finfo = NULL; /* A NULL file indicates it wasn't found */
	if (extfound) *extfound = 0; /* Was an explicit ,xyz found */

	do {
		end = start;
		/* Find end of dirname segment */
		while (*end && *end != '.') end++;

		segmentname = filename_unixify(start, end - start, &segmentlen);

		if (leafname) *leafname = segmentname;

		err = leafname_to_finfo(segmentname, &segmentlen, 0, followsymlinks || (*end != '\0'), &dirhandle, &segmentinfo, &status, conn);
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

		if (status != NFS_OK) return gen_nfsstatus_error(status);

		if (*end != '\0') {
			if (segmentinfo->attributes.type != NFDIR) {
				/* Every segment except the leafname should be a directory */
				if (dinfo) {
					return gen_nfsstatus_error(NFSERR_NOTDIR);
				} else {
					return NULL;
				}
			}
			if (dinfo) {
				dirinfo = *segmentinfo;
				*dinfo = &dirinfo;
			}
		}

		if (*end != '\0') dirhandle = segmentinfo->objhandle;
		start = end + 1;
	} while (*end != '\0');

	/* By this point, the directory has been found, the file may or
	   may not have been found */

	/* There's no need to continue if the caller doesn't care about the file */
	if (finfo == NULL) return NULL;

	if (status != NFS_OK) return NULL;

	*finfo = segmentinfo;

	if (filetype) {
		if (conn->xyzext == NEVER) {
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
					*filetype = lookup_mimetype(segmentname + lastdot + 1, conn);
				} else {
					/* No ,xyz and no extension, so use the default */
					*filetype = conn->defaultfiletype;
				}
			}
		}
	}

	return NULL;
}

/* Convert a unix timestamp into a RISC OS load and execution address */
void timeval_to_loadexec(struct ntimeval *unixtime, int filetype, unsigned int *load, unsigned int *exec)
{
	uint64_t csecs;

	csecs = unixtime->seconds;
	csecs *= 100;
#ifdef NFS3
	csecs += ((int64_t)unixtime->nseconds / 10000000);
#else
	csecs += ((int64_t)unixtime->useconds / 10000);
#endif
	csecs += 0x336e996a00; /* Difference between 1900 and 1970 */
	*load = (unsigned int)((csecs >> 32) & 0xFF);
	*load |= (0xFFF00000 | ((filetype & 0xFFF) << 8));
	*exec = (unsigned int)(csecs & 0xFFFFFFFF);
}

/* Convert a RISC OS load and execution address into a unix timestamp */
void loadexec_to_timeval(unsigned int load, unsigned int exec, struct ntimeval *unixtime)
{
	if ((load & 0xFFF00000) != 0xFFF00000) {
		/* A real load/exec address */
		unixtime->seconds = -1;
#ifdef NFS3
		unixtime->nseconds = -1; /*FIXME*/
#else
		unixtime->useconds = -1;
#endif
	} else {
		uint64_t csecs;

		csecs = exec;
		csecs |= ((uint64_t)load & 0xFF) << 32;
		csecs -= 0x336e996a00; /* Difference between 1900 and 1970 */
		unixtime->seconds = (unsigned int)((csecs / 100) & 0xFFFFFFFF);
#ifdef NFS3
		unixtime->nseconds = (unsigned int)((csecs % 100) * 10000000);
#else
		unixtime->useconds = (unsigned int)((csecs % 100) * 10000);
#endif
	}
}

/* Convert a unix mode to RISC OS attributes */
unsigned int mode_to_attr(unsigned int mode)
{
	unsigned int attr;
	attr  = (mode & 0x100) >> 8; /* Owner read */
	attr |= (mode & 0x080) >> 6; /* Owner write */
	/* Set public read/write if either group or other bits are set */
	attr |= ((mode & 0x004) << 2) | ((mode & 0x020) >> 1); /* Public read */
	attr |= ((mode & 0x002) << 4) | ((mode & 0x010) << 1); /* Public write */
	return attr;
}

/* Convert RISC OS attributes to a unix mode */
unsigned int attr_to_mode(unsigned int attr, unsigned int oldmode, struct conn_info *conn)
{
	unsigned int newmode;
	newmode = oldmode & ~0666; /* Preserve existing type and execute bits */
	newmode |= conn->unumask;
	newmode |= (attr & 0x01) << 8; /* Owner read */
	newmode |= (attr & 0x02) << 6; /* Owner write */
	newmode |= (attr & 0x10) << 1; /* Group read */
	newmode |= (attr & 0x20) >> 1; /* Group write */
	newmode |= (attr & 0x10) >> 2; /* Others read */
	newmode |= (attr & 0x20) >> 4; /* Others write */
	newmode &= ~(conn->umask);
	return newmode;
}


char *addfiletypeext(char *leafname, unsigned int len, int extfound, int newfiletype, unsigned int *newlen, struct conn_info *conn)
{
    static char newleafname[MAXNAMLEN];

	if (conn->xyzext == NEVER) {
		if (newlen) *newlen = len;
		return leafname;
	} else {
		/* Check if we need a new extension */
	    int extneeded = 0;

	    if (extfound) len -= sizeof(",xyz") - 1;

		if (conn->xyzext == ALWAYS) {
			/* Always add ,xyz */
			extneeded = 1;
		} else {
			/* Only add an extension if needed */
			char *ext;

			ext = strrchr(leafname, '.');
			if (ext) {
				int mimefiletype;
				int extlen = len - (ext - leafname) - 1;

				memcpy(newleafname, ext + 1, extlen);
				newleafname[extlen] = '\0';
				mimefiletype = lookup_mimetype(newleafname, conn);

				if (mimefiletype == newfiletype) {
					/* Don't need an extension */
					extneeded = 0;
				} else {
					/* A new ,xyz is needed */
					extneeded = 1;
				}
			} else {
				if (newfiletype == conn->defaultfiletype) {
					/* Don't need an extension */
					extneeded = 0;
				} else {
					/* A new ,xyz is needed */
					extneeded = 1;
				}
			}
		}

		memcpy(newleafname, leafname, len);
		if (extneeded) {
			snprintf(newleafname + len, sizeof(",xyz"), ",%.3x", newfiletype);
			*newlen = len + sizeof(",xyz") - 1;
		} else {
			newleafname[len] = '\0';
			*newlen = len;
		}
	}

	return newleafname;
}


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

#include "imageentry_func.h"

#include "nfs-calls.h"



/* Generate a RISC OS error block based on the given number and message */
os_error *gen_error(int num, char *msg, ...)
{
	static os_error module_err_buf;
	va_list ap;

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
		default: str = "Unknown error"; break;
	}
	return gen_error(NFSSTATBASE, "NFS call failed (%s)", str);
}

static char *filename_unixify(char *name, unsigned int len, unsigned int *newlen)
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
	char *lastdot = NULL;

	*filetype = -1;

	for (i = 0, j = 0; (i < namelen) && (j + 3 < buflen); i++) {
		if (name[i] == '.') {
			buffer[j++] = '/';
			lastdot = name + i;
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
				|| name[i] == ';'
				|| name[i] == '?'
				|| name[i] == '@'
				|| name[i] == '\\'
				|| name[i] == 127) {
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
	    	if (lastdot) {
				*filetype = lookup_mimetype(lastdot + 1, conn);
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
static os_error *lookup_leafname(char *dhandle, struct opaque *leafname, struct opaque **found, struct conn_info *conn)
{
	static struct readdirargs rddir;
	static struct readdirres rdres;
	struct entry *direntry;
	os_error *err;

	memcpy(rddir.dir, dhandle, FHSIZE);
	rddir.count = conn->maxdatabuffer;
	rddir.cookie = 0;

	do {
		err = NFSPROC_READDIR(&rddir, &rdres, conn);
		if (err) return err;
		if (rdres.status != NFS_OK) return gen_nfsstatus_error(rdres.status);

		direntry = rdres.u.readdirok.entries;
		while (direntry) {
			rddir.cookie = direntry->cookie;

			if (direntry->name.size == leafname->size + sizeof(",xyz") - 1) {
				if (strncmp(direntry->name.data, leafname->data, leafname->size) == 0) {
					if (direntry->name.data[leafname->size] == ','
					     && isxdigit(direntry->name.data[leafname->size + 1])
					     && isxdigit(direntry->name.data[leafname->size + 2])
					     && isxdigit(direntry->name.data[leafname->size + 3])) {
						*found = &(direntry->name);
						return NULL;
					}
				}
			}
			direntry = direntry->next;
		}
	} while (!rdres.u.readdirok.eof);

	*found = NULL;
	return NULL;
}

/* Convert a full filename/dirname into a leafname, and find the nfs handle
   for the file/dir and for the directory containing it.
   Returns a pointer to static data.
   This function would really benefit from some cacheing.
   In theory it should deal with wildcarded names, but that is just too much
   hassle. And, really, that should be the job of fileswitch. */
os_error *filename_to_finfo(char *filename, struct diropok **dinfo, struct diropok **finfo, char **leafname, int *filetype, int *extfound, struct conn_info *conn)
{
	char *start = filename;
	char *end;
	char *dirhandle = conn->rootfh;
	struct diropargs current;
	static struct diropres next;
	static struct diropok dirinfo;
	os_error *err;

	if (dinfo) *dinfo = NULL; /* A NULL directory indicates the root directory */
	if (finfo) *finfo = NULL; /* A NULL file indicates it wasn't found */
	if (extfound) *extfound = 0; /* Was an explicit ,xyz found */

	do {
		end = start;
		/* Find end of dirname segment */
		while (*end && *end != '.') end++;

		current.name.data = filename_unixify(start, end - start, &(current.name.size));

		if (leafname) *leafname = current.name.data;

		memcpy(current.dir, dirhandle, FHSIZE);
		err = NFSPROC_LOOKUP(&current, &next, conn);
		if (err) return err;

		/* It is not an error if the file isn't found, but the
		   containing directory must exist */
		if (*end == '\0' && next.status == NFSERR_NOENT) break;

		if (dinfo == NULL && (next.status == NFSERR_NOENT || next.status == NFSERR_NOTDIR)) {
			/* If the caller wants info on the directory then it is an error if
			   the dir isn't found, but if they only want info on the file
			   (eg. read cat info) then it is not an error if we can't find it */
			return NULL;
		}

		if (next.status != NFS_OK) return gen_nfsstatus_error(next.status);

		if (*end != '\0') {
			if (next.u.diropok.attributes.type != NFDIR) {
				/* Every segment except the leafname must be a directory */
				if (dinfo) {
					return gen_nfsstatus_error(NFSERR_NOTDIR);
				} else {
					return NULL;
				}
			}
			if (dinfo) {
				dirinfo = next.u.diropok;
				*dinfo = &dirinfo;
			}
		}

		dirhandle = next.u.diropok.file;
		start = end + 1;
	} while (*end != '\0');

	/* By this point, the directory has been found, the file may or
	   may not have been found (either because it doesn't exist, or
	   because we need to add an ,xyz extension) */

	/* There's no need to continue if the caller doesn't care about the file */
	if (finfo == NULL) return NULL;

	if (next.status == NFSERR_NOENT) {
		if (conn->xyzext != NEVER) {
			struct opaque *file;
	
			err = lookup_leafname(dirhandle, &(current.name), &file, conn);
			if (err) return err;
	
			if (file) {
				/* A matching file,xyz was found */
				current.name.data = file->data;
				current.name.size = file->size;
				if (leafname && file->size <= MAXNAMLEN) {
					/* Update the leafname. This is a bit icky as it copies
					   into unixify's private buffer, but it saves allocating
					   a new buffer */
					(*leafname)[file->size - 4] = ',';
					(*leafname)[file->size - 3] = file->data[file->size - 3];
					(*leafname)[file->size - 2] = file->data[file->size - 2];
					(*leafname)[file->size - 1] = file->data[file->size - 1];
					(*leafname)[file->size - 0] = '\0';
				}
	
				if (filetype) *filetype = (int)strtol(file->data + file->size - 3, NULL, 16);

				/* Lookup the handle with the new leafname */
				err = NFSPROC_LOOKUP(&current, &next, conn);
				if (err) return err;
				if (next.status != NFS_OK) return gen_nfsstatus_error(next.status);
	
				*finfo = &(next.u.diropok);

				if (extfound) *extfound = 1;
			}
		}
	} else {
		/* The file was found without needing to add ,xyz */
		*finfo = &(next.u.diropok);

		/* Work out the filetype */
		if (filetype) {
			if (conn->xyzext != NEVER) {
				int lastdot = current.name.size;
				while (lastdot > 0 && current.name.data[lastdot] != '.') lastdot--;
	
				/* current.name.data is pointing to unixify's buffer and is 0 terminated */
		    	if (lastdot) {
		    		*filetype = lookup_mimetype(current.name.data + lastdot + 1, conn);
				} else {
					/* No ,xyz and no extension, so use the default */
					*filetype = conn->defaultfiletype;
				}
			} else {
				/* No ,xyz and not configured to use the mimetype */
				*filetype = conn->defaultfiletype;
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
	csecs += ((int64_t)unixtime->useconds / 10000);
	csecs += 0x336e996a00; /* Difference between 1900 and 1970 */
	*load = (unsigned int)((csecs >> 32) & 0xFF);
	*load |= (0xFFF00000 | ((filetype & 0xFFF) << 8));
	*exec = (unsigned int)(csecs & 0xFFFFFFFF);
}

/* Convert a RISC OS load and execution address into a unix timestamp */
void loadexec_to_timeval(unsigned int load, unsigned int exec, struct ntimeval *unixtime)
{
	if ((load & 0xFFF00000) == 0xFFF00000) {
		/* A real load/exec address */
		unixtime->seconds = -1;
		unixtime->useconds = -1;
	} else {
		uint64_t csecs;

		csecs = exec;
		csecs |= ((uint64_t)load & 0xFF) << 32;
		csecs -= 0x336e996a00; /* Difference between 1900 and 1970 */
		unixtime->seconds = (unsigned int)((csecs / 100) & 0xFFFFFFFF);
		unixtime->useconds = (unsigned int)((csecs % 100) * 10000);
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


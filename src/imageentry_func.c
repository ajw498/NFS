/*
	$Id$

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
struct dir10_entry {
	unsigned load;
	unsigned exec;
	unsigned len;
	unsigned attr;
	unsigned type;
};

/* The format of info returned for OS_GBPB 11 */
struct dir11_entry {
	unsigned load;
	unsigned exec;
	unsigned len;
	unsigned attr;
	unsigned type;
	unsigned internal;
	char time[5];
};

os_error *ENTRYFUNC(func_closeimage) (struct conn_info *conn)
{
#ifdef NFS3
	mountargs3 dir;
#else
	mountargs dir;
#endif
	os_error *err;

	dir.dirpath.size = strlen(conn->exportname);
	dir.dirpath.data = conn->exportname;
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
#ifdef NFS3
	mountargs3 dir;
	struct mountres3 mountres;
	struct fsinfoargs fsinfoargs;
	struct fsinfores fsinfores;
#else
	mountargs dir;
	struct mountres mountres;
#endif
	os_error *err;

	/* Get a filehandle for the root directory */
	dir.dirpath.size = strlen(conn->exportname);
	dir.dirpath.data = conn->exportname;
	err = MNTPROC(MNT, (&dir, &mountres, conn));
	if (err) return err;
	if (mountres.status != 0) {
		/* status is an errno value. Probably the same as an NFS status value. */
		return ENTRYFUNC(gen_nfsstatus_error) ((nstat)mountres.status);
	}
	fh_to_commonfh(conn->rootfh, mountres.u.mountinfo.fhandle);
#ifdef NFS3
	commonfh_to_fh(fsinfoargs.fsroot, conn->rootfh);
	err = NFSPROC(FSINFO, (&fsinfoargs, &fsinfores, conn));
	if (err) return err;
	if (fsinfores.status != NFS_OK) return ENTRYFUNC(gen_nfsstatus_error) (fsinfores.status);

	if (conn->maxdatabuffer > fsinfores.u.resok.rtmax) conn->maxdatabuffer = fsinfores.u.resok.rtmax;
	if (conn->maxdatabuffer > fsinfores.u.resok.wtmax) conn->maxdatabuffer = fsinfores.u.resok.wtmax;
#else
	if (conn->maxdatabuffer > NFS2MAXDATA) conn->maxdatabuffer = NFS2MAXDATA;
#endif

	return NULL;
}


/* Read directory entries and possibly info */
os_error *ENTRYFUNC(func_readdirinfo) (int info, char *dirname, char *buffer, int numobjs, int start, int buflen, struct conn_info *conn, int *objsread, int *continuepos)
{
	char *bufferpos;
#ifdef NFS3
	struct readdirplusargs3 rddir;
	struct readdirplusres3 rdres;
	struct entryplus3 *direntry = NULL;
#else
	struct readdirargs rddir;
	struct readdirres rdres;
	struct entry *direntry = NULL;
#endif
	os_error *err;
	uint64_t cookie = 0;
	int dirpos = 0;
	int stale = 0;

	bufferpos = buffer;
	*objsread = 0;
#ifdef NFS3
	memset(rddir.cookieverf, 0, NFS3_COOKIEVERFSIZE);
#endif

restart:
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
		static struct commonfh fh;

		/* Find the handle of the directory */
		err = ENTRYFUNC(filename_to_finfo) (dirname, 1, NULL, &finfo, NULL, NULL, NULL, conn);
		if (err) return err;
		if (finfo == NULL) return ENTRYFUNC(gen_nfsstatus_error) (NFSERR_NOENT);

		fh = finfo->objhandle;
		commonfh_to_fh(rddir.dir, fh);
	}

	while (dirpos <= start) {
		rddir.count = conn->maxdatabuffer;
#ifdef NFS3
		rddir.maxcount = conn->maxdatabuffer;
		rddir.cookie = cookie;
		err = NFSPROC(READDIRPLUS, (&rddir, &rdres, conn));
#else
		rddir.cookie = (int)cookie;
		err = NFSPROC(READDIR, (&rddir, &rdres, conn));
#endif
		if (err) return err;
		if ((rdres.status == NFSERR_STALE) && !stale) {
			stale = 1;
			ENTRYFUNC(func_newimage_mount) (conn);
			goto restart;
		}
		stale = 1;
		if (rdres.status != NFS_OK) return ENTRYFUNC(gen_nfsstatus_error) (rdres.status);

		/* We may need to do a lookup on each filename, but this would
		   corrupt the rx buffer which holds our list, so swap buffers */
		rpc_hold_rxbuffer();

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
					struct dir10_entry *info_entry = (struct dir10_entry *)bufferpos;
					struct dir11_entry *fullinfo_entry = (struct dir11_entry *)bufferpos;
					int filetype;
					char *leafname;
					unsigned len;

					if (info == 1) {
						bufferpos += sizeof(struct dir10_entry);
					} else if (info == 2) {
						bufferpos += 29; /* Would be sizeof(struct dir11_entry) (but minus the padding)*/
					}

					/* Check there is room in the output buffer. */
					if (bufferpos > buffer + buflen) break;

					leafname = direntry->name.data;
					len = direntry->name.size;

					if (conn->fromenc != (iconv_t)-1) {
						char *encleaf;
						unsigned encleaflen;
						static char buffer2[MAX_PATHNAME];

						encleaf = buffer2;
						encleaflen = sizeof(buffer2);
						if (iconv(conn->fromenc, &leafname, &len, &encleaf, &encleaflen) == -1) {
							iconv(conn->fromenc, NULL, NULL, NULL, NULL);
							return gen_error(ICONVERR, "Iconv failed when converting a filename (%d)", errno);
						}

						leafname = buffer2;
						len = sizeof(buffer2) - encleaflen;
					}

					/* Copy leafname into output buffer, translating some
					   chars and stripping any ,xyz */
					len = filename_riscosify(leafname, len, bufferpos, buffer + buflen - bufferpos, &filetype, conn->defaultfiletype, conn->xyzext, 0, conn->escapewin);
					if (len == 0) break; /* Buffer overflowed */

					bufferpos += len + 1;

					if (info) {
						struct objinfo *lookupres;
						nstat status;
						struct commonfh fh;

						bufferpos = (char *)(((int)bufferpos + 3) & ~3);

#ifdef NFS3
						if (direntry->name_attributes.attributes_follow && direntry->name_attributes.u.attributes.type != NFLNK) {
							if (direntry->name_attributes.u.attributes.type == NFDIR) {
								filetype = DIR_FILETYPE;
							} else if (conn->unixexfiletype && ((direntry->name_attributes.u.attributes.mode & 0111) != 0)) {
								filetype = UNIXEX_FILETYPE;
							}
							timeval_to_loadexec(direntry->name_attributes.u.attributes.mtime.seconds, direntry->name_attributes.u.attributes.mtime.nseconds, filetype, &(info_entry->load), &(info_entry->exec), 0);
							info_entry->len = filesize(direntry->name_attributes.u.attributes.size);
							info_entry->attr = mode_to_attr(direntry->name_attributes.u.attributes.mode);
							info_entry->type = direntry->name_attributes.u.attributes.type == NFDIR ? OBJ_DIR : OBJ_FILE;
						} else {
#endif
							fh_to_commonfh(fh, rddir.dir);
	
							/* Lookup file attributes. */
							err = ENTRYFUNC(leafname_to_finfo) (direntry->name.data, &(direntry->name.size), 1, 1, &fh, &lookupres, &status, conn);
							if (err) return err;
							if (status == NFS_OK && lookupres->attributes.type != NFLNK) {
								if (lookupres->attributes.type == NFDIR) {
									filetype = DIR_FILETYPE;
								} else if (conn->unixexfiletype && ((lookupres->attributes.mode & 0111) != 0)) {
									filetype = UNIXEX_FILETYPE;
								}
#ifdef NFS3
								timeval_to_loadexec(lookupres->attributes.mtime.seconds, lookupres->attributes.mtime.nseconds, filetype, &(info_entry->load), &(info_entry->exec), 0);
#else
								timeval_to_loadexec(lookupres->attributes.mtime.seconds, lookupres->attributes.mtime.useconds, filetype, &(info_entry->load), &(info_entry->exec), 1);
#endif
								info_entry->len = filesize(lookupres->attributes.size);
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
#ifdef NFS3
						}
#endif
						if (info == 2) {
							fullinfo_entry->internal = 0;
							fullinfo_entry->time[0] = (fullinfo_entry->exec & 0x000000FF);
							fullinfo_entry->time[1] = (fullinfo_entry->exec & 0x0000FF00) >> 8;
							fullinfo_entry->time[2] = (fullinfo_entry->exec & 0x00FF0000) >> 16;
							fullinfo_entry->time[3] = (fullinfo_entry->exec & 0xFF000000) >> 24;
							fullinfo_entry->time[4] = (fullinfo_entry->load & 0x000000FF);
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
		if (len < MAX_PATHNAME) {
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
#ifdef NFS3
	struct renameargs3 renameargs;
	struct renameres3 renameres;
#else
	struct renameargs renameargs;
	struct renameres renameres;
#endif
	os_error *err;
	char *leafname;
	static char oldleafname[MAX_PATHNAME];
	int filetype;
	int dirnamelen; /* The length of the directory name including the last dot */
	unsigned int leafnamelen;
	int sourcetype;
	struct commonfh sourcedir;

	leafname = strrchr(oldfilename, '.');
	if (leafname) {
		dirnamelen = (leafname - oldfilename) + 1;
	} else {
		dirnamelen = 0;
	}

	err = ENTRYFUNC(filename_to_finfo) (oldfilename, 1, &dinfo, &finfo, &leafname, &filetype, NULL, conn);
	if (err) return err;
	if (finfo == NULL) return ENTRYFUNC(gen_nfsstatus_error) (NFSERR_NOENT);

	sourcetype = finfo->attributes.type;

	if (dinfo) {
		sourcedir = dinfo->objhandle;
		commonfh_to_fh(renameargs.from.dir, sourcedir);
	} else {
		commonfh_to_fh(renameargs.from.dir, conn->rootfh);
	}
	strcpy(oldleafname, leafname);
	renameargs.from.name.data = oldleafname;
	renameargs.from.name.size = strlen(oldleafname);

	if (strncmp(oldfilename, newfilename, dirnamelen) == 0 &&
	    newfilename[dirnamelen] &&
	    strchr(newfilename + dirnamelen, '.') == NULL) {
		/* Both files are in the same directory */
		renameargs.to.dir = renameargs.from.dir;
		leafname = newfilename + dirnamelen;
		leafname = filename_unixify(leafname, strlen(leafname), &leafnamelen, conn->escapewin, conn->pool);

		if (conn->toenc != (iconv_t)-1) {
			char *encleaf;
			unsigned encleaflen;
			static char buffer2[MAX_PATHNAME];

			encleaf = buffer2;
			encleaflen = sizeof(buffer2);
			if (iconv(conn->toenc, &leafname, &leafnamelen, &encleaf, &encleaflen) == -1) {
				iconv(conn->toenc, NULL, NULL, NULL, NULL);
				return gen_error(ICONVERR, "Iconv failed when converting a filename (%d)", errno);
			}

			encleaflen = sizeof(buffer2) - encleaflen;
			if ((leafname = palloc(encleaflen + sizeof(",xyz"), conn->pool)) == NULL) return gen_error(NOMEM, NOMEMMESS);
			memcpy(leafname, buffer2, encleaflen);
			leafnamelen = encleaflen;
		}
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

	if (sourcetype == NFDIR) {
		renameargs.to.name.data = leafname;
		renameargs.to.name.size = leafnamelen;
	} else {
		/* Add ,xyz on if necessary to preserve filetype */
		renameargs.to.name.data = addfiletypeext(leafname, leafnamelen, 0, filetype, &(renameargs.to.name.size), conn->defaultfiletype, conn->xyzext, conn->unixexfiletype, 0, conn->pool);
	}

	err = NFSPROC(RENAME, (&renameargs, &renameres, conn));
	if (err) return err;
	if (renameres.status != NFS_OK) return ENTRYFUNC(gen_nfsstatus_error) (renameres.status);

	*renamefailed = 0;
	return NULL;
}


/* Read free space */
os_error *ENTRYFUNC(func_free) (char *filename, struct conn_info *conn, unsigned *freelo, unsigned *freehi, unsigned *biggestobj, unsigned *sizelo, unsigned *sizehi, unsigned *usedlo, unsigned *usedhi)
{
	struct objinfo *finfo;
#ifdef NFS3
	struct FSSTAT3args args;
	struct FSSTAT3res res;
#else
	struct statfsargs args;
	struct statfsres res;
#endif
	os_error *err;
	uint64_t free;
	uint64_t size;

	err = ENTRYFUNC(filename_to_finfo) (filename, 1, NULL, &finfo, NULL, NULL, NULL, conn);
	if (err) return err;
	if (finfo == NULL) return ENTRYFUNC(gen_nfsstatus_error) (NFSERR_NOENT);

	commonfh_to_fh(args.fhandle, finfo->objhandle);
	err = NFSPROC(STATFS, (&args, &res, conn));
	if (err) return err;
	if (res.status != NFS_OK) return ENTRYFUNC(gen_nfsstatus_error) (res.status);

#ifdef NFS3
	free = res.u.resok.fbytes;
	size = res.u.resok.tbytes;
#else
	free = ((uint64_t)res.u.info.bfree) * ((uint64_t)res.u.info.bsize);
	size = ((uint64_t)res.u.info.blocks) * ((uint64_t)res.u.info.bsize);
#endif
	if (freehi) {
		*freehi = (unsigned)(free >> 32);
		*freelo = (unsigned)(free & 0xFFFFFFFF);
	} else {
		if ((free >> 32) != 0ULL) {
			*freelo = 0xFFFFFFFF;
		} else {
			*freelo = (unsigned)free;
		}
	}

	if (biggestobj) *biggestobj = (freehi && *freehi) ? 0x7FFFFFFF : *freelo;

	if (sizehi) {
		*sizehi = (unsigned)(size >> 32);
		*sizelo = (unsigned)(size & 0xFFFFFFFF);
	} else {
		if ((size >> 32) != 0ULL) {
			*sizelo = 0xFFFFFFFF;
		} else {
			*sizelo = (unsigned)size;
		}
	}

	if (usedlo) {
		uint64_t used = size - free;
		if (usedhi) {
			*usedhi = (unsigned)(used >> 32);
			*usedlo = (unsigned)(used & 0xFFFFFFFF);
		} else {
			if ((used >> 32) != 0ULL) {
				*usedlo = 0xFFFFFFFF;
			} else {
				*usedlo = (unsigned)used;
			}
		}
	}

	return NULL;
}

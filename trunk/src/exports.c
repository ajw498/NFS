/*
	$Id$

	exports file parsing.


	Copyright (C) 2006 Alex Waugh
	
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



#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <swis.h>
#include <sys/errno.h>
#include <unixlib.h>
#include <stdarg.h>
#include <ctype.h>

#include "moonfish.h"
#include "utils.h"
#include "rpc-decode.h"
#include "exports.h"
#include "serverconn.h"


int logging = 0;

#define CHECK(str) (strncasecmp(opt,str,sizeof(str) - 1)==0)

static struct export *parse_line(char *line, struct pool *pool)
{
	char *basedir;
	char *exportname;
	char *host;
	struct export *export;
	static int exportnum = 1; /* 0 is reserved for the NFS4 root */
	char *maskstart;

	while (isspace(*line)) line++;
	if (*line == '#' || *line == '\0') return NULL;
	basedir = line;
	while (*line && !isspace(*line)) line++;
	if (*line) *line++ = '\0';
	while (isspace(*line)) line++;
	exportname = line;
	while (*line && !isspace(*line)) line++;
	if (*line) *line++ = '\0';
	while (isspace(*line)) line++;
	host = line;
	while (*line && *line != '(') line++;
	if (*line) *line++ = '\0';

	UU(export = palloc(sizeof(struct export), pool));

	/* Set defaults */
	export->ro = 1;
	export->matchuid = 1;
	export->uid = 0;
	export->matchgid = 1;
	export->gid = 0;
	export->imagefs = 0;
	export->defaultfiletype = 0xFFF;
	export->xyzext = NEEDED;
	export->fakedirtimes = 0;
	export->udpsize = 4096;
	export->umask = 0;
	export->unumask = 0;
	export->next = NULL;
	export->pathentry = NULL;
	memset(export->hosts, 0, sizeof(unsigned int) * MAXHOSTS);
	UU(export->pool = pinit(pool));

	UU(export->basedir = pstrdup(basedir, pool));
	export->basedirlen = strlen(basedir);
	export->exportnum = exportnum++;
	if (exportnum > 127) {
		syslogf(LOGNAME, LOG_SERIOUS, "Only 127 export directories are supported");
		return NULL;
	}

	UU(export->exportname = pstrdup(exportname, pool));
	/* Check the export name only contains alphanumeric or _ characters,
	   and an optional / at the start */
	if (*exportname == '/') exportname++;
	do {
		if (!isalnum(*exportname) && (*exportname != '_')) {
			syslogf(LOGNAME, LOG_SERIOUS, "Export name '%s' should only contain alphanumeric and _ characters, and optionally start with a /", export->exportname);
			return NULL;
		}
	} while (*++exportname);

	maskstart = strchr(host, '/');
	if (maskstart) {
		int maskbits;

		*maskstart++ = '\0';
		maskbits = atoi(maskstart);
		export->mask = 0xFFFFFFFF >> (32 - maskbits);
	} else {
		export->mask = 0xFFFFFFFF;
	}

	if (strcmp(host, "*") == 0) {
		export->host = 0;
		export->mask = 0;
	} else {
		struct hostent *hp;
		ER(gethostbyname_timeout(host, 500, &hp));
		memcpy(&(export->host), hp->h_addr, hp->h_length);
	}

	while (*line) {
		char *opt;
		while (isspace(*line)) line++;
		opt = line;
		while (*line && *line != ')' && *line != ',') line++;
		if (*line) *line++ = '\0';
		if (CHECK("rw")) {
			export->ro = 0;
		} else if (CHECK("ro")) {
			export->ro = 1;
		} else if (CHECK("uid") && opt[3] == '=') {
			export->matchuid = 0;
			export->uid = atoi(opt + 4);
		} else if (CHECK("gid") && opt[3] == '=') {
			export->matchgid = 0;
			export->gid = atoi(opt + 4);
		} else if (CHECK("udpsize") && opt[7] == '=') {
			export->udpsize = atoi(opt + 8);
			if (export->udpsize > 8192) export->udpsize = 8192;
		} else if (CHECK("imagefs")) {
			export->imagefs = 1;
		} else if (CHECK("fakedirtimes")) {
			export->fakedirtimes = 1;
		} else if (CHECK("logging")) {
			logging = 1;
		} else if (CHECK("umask") && opt[5] == '=') {
			export->umask = (int)strtol(opt + 6, NULL, 8) & 0777;
		} else if (CHECK("unumask") && opt[7] == '=') {
			export->unumask = (int)strtol(opt + 8, NULL, 8) & 0777;
		} else if (CHECK("defaultfiletype") && opt[15] == '=') {
			export->defaultfiletype = (int)strtol(opt + 16, NULL, 16);
		} else if (CHECK("addext") && opt[6] == '=') {
			export->xyzext = atoi(opt + 7);
		}
	}

	return export;
}

static struct export root = {
	.ro = 1,
	.matchuid = 0,
	.uid = 0,
	.matchgid = 0,
	.gid = 0,
	.imagefs = 0,
	.defaultfiletype = 0xFFF,
	.xyzext = NEEDED,
	.fakedirtimes = 0,
	.udpsize = 4096,
	.umask = 0,
	.unumask = 0,
	.next = NULL,
	.pathentry = NULL,
	.pool = NULL,
	.basedir = NULL,
	.basedirlen = 0,
	.exportnum = 0,
	.exportname = NULL,
	.host = 0,
	.mask = 0,
};

struct export *parse_exports_file(struct pool *pool)
{
	char line[1024];
	FILE *file;
	struct export *exports = NULL;

	file = fopen(EXPORTSREAD, "r");
	if (file == NULL) {
		syslogf(LOGNAME, LOG_SERIOUS, "Unable to open exports file (%s)", EXPORTSREAD);
		return NULL;
	}

	while (fgets(line, sizeof(line), file)) {
		struct export *export;

		export = parse_line(line, pool);
		if (export) {
			export->next = exports;
			exports = export;
		}
	}
	fclose(file);

	/* Add the NFS4 root export to the head of the list */
	root.next = exports;
	exports = &root;

	return exports;
}

static nstat copypath(char *dest, int destlen, char *src, int srclen)
{
	while (srclen > 0 && *src) {
		if (destlen < 1) return NFSERR_NAMETOOLONG;
		if (*src < '!' ||
		    *src == '#' ||
		    *src == '$' ||
		    *src == '%' ||
		    *src == '&' ||
		    *src == ':' ||
		    *src == '@' ||
		    *src == '^' ||
		    *src == '\\' ||
		    *src == 127) return NFSERR_STALE;
		*dest++ = *src++;
		destlen--;
		srclen--;
	}
	if (destlen < 1) return NFSERR_NAMETOOLONG;
	*dest = '\0';
	return NFS_OK;
}

int calc_fileid(char *path, char *leaf)
{
	int fileid = 0;
	char *dot = leaf ? "." : NULL;

	do {
		while (path && *path) {
			int hash;
			fileid = (fileid << 4) + *path++;
			hash = fileid & 0xf0000000;
			if (hash) {
				fileid = fileid ^ (hash >> 24);
				fileid = fileid ^ hash;
			}
		}
		path = dot;
		dot = leaf;
		leaf = NULL;
	} while (path);

	return fileid;
}

nstat fh_to_path(char *fhandle, int fhandlelen, char **path, struct server_conn *conn)
{
	int extendedfh;
	int exportnum;
	int hash;

	extendedfh = ((((unsigned char)(fhandle[0])) & 0x80) != 0);
	exportnum = fhandle[0] & 0x7F;
	hash = fhandle[1];

	if ((exportnum == 0) && (conn->nfs4 == 0)) return NFSERR_STALE;

	if ((conn->export == NULL) || conn->nfs4) {
		conn->export = conn->exports;
		while (conn->export && (conn->export->exportnum != exportnum)) {
			conn->export = conn->export->next;
		}
		if (conn->export == NULL) return NFSERR_STALE;
	}

	if ((conn->host & conn->export->mask) != conn->export->host) return NFSERR_ACCESS;

	if (conn->export->matchuid == 0) conn->uid = conn->export->uid;
	if (conn->export->matchgid == 0) conn->gid = conn->export->gid;

	if (extendedfh) {
		char *dest;
		int pathremain;
		int fhremain;
		char *fh;
		struct pathentry *pathentry = conn->export->pathentry;

		UR(*path = palloc(MAX_PATHNAME, conn->pool));
		dest = *path;
		memcpy(dest, conn->export->basedir, conn->export->basedirlen);
		dest += conn->export->basedirlen;
		pathremain = MAX_PATHNAME - conn->export->basedirlen;
		fhremain = fhandlelen - 2;
		fh = fhandle + 2;
		while (fhremain > 0) {
			if (*fh == 0xFF) {
				if (pathremain < 1) return NFSERR_NAMETOOLONG;
				*dest++ = '\0';
				fhremain = 0;
			} else if (*fh == 0xFE) {
				fh++;
				fhremain--;
				*dest++ = '.';
				pathremain--;
				NR(copypath(dest, pathremain, fh, fhremain));
				fhremain = 0;
			} else {
				int namelen;
				int index;
				if (*fh & 0x80) {
					index = (*fh++ & 0x7F) << 8;
					index |= *fh++;
					fhremain -= 2;
				} else {
					index = *fh++;
					fhremain--;
				}
				if (pathentry == NULL) return conn->nfs4 ? NFSERR_FHEXPIRED : NFSERR_STALE;
				while (index >= PATHENTRIES) {
					pathentry = pathentry[PATHENTRIES].next;
					if (pathentry == NULL) return conn->nfs4 ? NFSERR_FHEXPIRED : NFSERR_STALE;
					index -= PATHENTRIES;
				}
				if (pathentry[index].name == NULL) return conn->nfs4 ? NFSERR_FHEXPIRED : NFSERR_STALE;
				namelen = strlen(pathentry[index].name);
				if (pathremain < namelen + 2) return NFSERR_NAMETOOLONG;
				pathremain -= namelen + 1;
				*dest++ = '.';
				memcpy(dest, pathentry[index].name, namelen);
				dest += namelen;
				*dest = '\0';
				pathentry = pathentry[index].next;
			}
		}
	} else if (exportnum == 0) {
		*path = NULL;
	} else {
		char *dest;

		UR(*path = palloc(conn->export->basedirlen + fhandlelen, conn->pool));
		dest = *path;
		memcpy(dest, conn->export->basedir, conn->export->basedirlen);
		dest += conn->export->basedirlen;
		if (fhandle[2]) *dest++ = '.';
		NR(copypath(dest, MAX_PATHNAME - (conn->export->basedirlen + 1), fhandle + 2, fhandlelen - 2));
	}

	if ((calc_fileid(*path, NULL) & 0xFF) != hash) return NFSERR_STALE;

	return NFS_OK;
}

nstat path_to_fh(char *path, char **fhandle, unsigned int *fhandlelen, struct server_conn *conn)
{
	size_t len;

	if (*fhandle == NULL) {
		UR(*fhandle = palloc(*fhandlelen, conn->pool));
	}
	memset(*fhandle, 0, *fhandlelen);
	len = path ? strlen(path) : 0;
	if ((len <= conn->export->basedirlen) ||
	    (memcmp(path, conn->export->basedir, conn->export->basedirlen) != 0) ||
	    ((path[conn->export->basedirlen] != '.') &&
	     (path[conn->export->basedirlen] != '\0'))) {
		/* This should only ever happen if we are called from mount
		   to get the root filehandle */
		(*fhandle)[0] = conn->export->exportnum;
		(*fhandle)[1] = calc_fileid(conn->export->basedir, NULL) & 0xFF;
		*fhandlelen = 8;
		return NFS_OK;
	}
	len -= conn->export->basedirlen + 1;
	if (len > *fhandlelen - 2) {
		char *fh = *fhandle;
		int fhremain;
		struct pathentry **pathentryptr;
		struct pathentry *pathentry;

		*fh++ = conn->export->exportnum | 0x80;
		*fh++ = calc_fileid(path, NULL) & 0xFF;
		fhremain = *fhandlelen - 2;
		path += conn->export->basedirlen + 1;
		pathentryptr = &(conn->export->pathentry);
		pathentry = conn->export->pathentry;
		while (len > 0) {
			char *ch;
			int segmentlen;
			int index;
			int i;

			if (len < fhremain) {
				*fh++ = 0xFE;
				memcpy(fh, path, len);
				return NFS_OK;
			}
			ch = path;
			while (*ch && *ch != '.') ch++;
			segmentlen = ch - path;
			if (*ch == '.') ch++;
			if (pathentry == NULL) {
				UR(pathentry = *pathentryptr = palloc(sizeof(struct pathentry) * (PATHENTRIES + 1), conn->export->pool));
				memset(pathentry, 0, sizeof(struct pathentry) * (PATHENTRIES + 1));
			}
			index = 0;
			i = PATHENTRIES;
			while (i == PATHENTRIES) {
				for (i = 0; i < PATHENTRIES && pathentry[i].name; i++) {
					if (strncmp(pathentry[i].name, path, segmentlen) == 0) break;
				}
				index += i;
				if (i == PATHENTRIES) {
					if (pathentry[i].next == NULL) {
						UR(pathentry[i].next = palloc(sizeof(struct pathentry) * (PATHENTRIES + 1), conn->export->pool));
						memset(pathentry[i].next, 0, sizeof(struct pathentry) * (PATHENTRIES + 1));
					}
					pathentry = pathentry[i].next;
				}
			}
			if (pathentry[i].name == NULL) {
				UR(pathentry[i].name = palloc(segmentlen + 1, conn->export->pool));
				memcpy(pathentry[i].name, path, segmentlen);
				pathentry[i].name[segmentlen] = '\0';
			}
			pathentryptr = &(pathentry[i].next);
			pathentry = pathentry[i].next;
			if (index < 0x80) {
				if (fhremain < 1) NR(NFSERR_NAMETOOLONG);
				*fh++ = index;
				fhremain--;
				len -= (ch - path);
				path = ch;
			} else if (index < 0x7EFF) {
				if (fhremain < 2) NR(NFSERR_NAMETOOLONG);
				*fh++ = ((index & 0x7F00) >> 8) | 0x80;
				*fh++ = (index & 0xFF);
				fhremain -= 2;
				len -= (ch - path);
				path = ch;
			} else {
				NR(NFSERR_NAMETOOLONG);
			}
		}
		if (fhremain > 0) *fh++ = 0xFF;
		*fhandlelen -= fhremain;
	} else {
		(*fhandle)[0] = conn->export->exportnum;
		(*fhandle)[1] = calc_fileid(path, NULL) & 0xFF;
		memcpy(*fhandle + 2, path + conn->export->basedirlen + 1, len);
		/* Terminated by the earlier memset */
		*fhandlelen = 2 + len;
	}

	/* Ethereal doesn't seem to like short filehandles, so make the
	   minimum size 8 bytes to aid debugging */
	if (*fhandlelen < 8) *fhandlelen = 8;

	return NFS_OK;
}





/*
	$Id$
	$URL$

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
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <socklib.h>
#include <time.h>
#include <sys/time.h>
#include <swis.h>
#include <sys/errno.h>
#include <unixlib.h>
#include <stdarg.h>
#include <ctype.h>

#include "utils.h"
#include "rpc-decode.h"
#include "exports.h"
#include "serverconn.h"


#define Resolver_GetHost 0x46001

/* A version of gethostbyname that will timeout.
   Also handles IP addresses without needing a reverse lookup */
static os_error *gethostbyname_timeout(char *host, unsigned long timeout, struct hostent **hp)
{
	unsigned long starttime;
	unsigned long endtime;
	os_error *err;
	int errnum;
	int quad1, quad2, quad3, quad4;

	if (sscanf(host, "%d.%d.%d.%d", &quad1, &quad2, &quad3, &quad4) == 4) {
		/* Host is an IP address, so doesn't need resolving */
		static struct hostent hostent;
		static unsigned int addr;
		static char *addr_list = (char *)&addr;

		addr = quad1 | (quad2 << 8) | (quad3 << 16) | (quad4 << 24);
		hostent.h_addr_list = &addr_list;
		hostent.h_length = sizeof(addr);

		*hp = &hostent;
		return NULL;
	}

	err = _swix(OS_ReadMonotonicTime, _OUT(0), &starttime);
	if (err) return err;

	do {
		err = _swix(Resolver_GetHost, _IN(0) | _OUTR(0,1), host, &errnum, hp);
		if (err) return err;

		if (errnum != EINPROGRESS) break;

		err = _swix(OS_ReadMonotonicTime, _OUT(0), &endtime);
		if (err) return err;

	} while (endtime - starttime < timeout * 100);

	if (errnum == 0) return NULL; /* Host found */

	return gen_error(RPCERRBASE + 1, "Unable to resolve hostname '%s' (%d)", host, errnum);
}

int logging = 0;

#define CHECK(str) (strncasecmp(opt,str,sizeof(str) - 1)==0)

#define UE(x) do { \
	if ((x) == NULL) { \
		syslogf(LOGNAME, LOG_MEM, OUTOFMEM); \
		return NULL; \
	} \
} while (0)

#define UR(x) do { \
	if ((x) == NULL) { \
		syslogf(LOGNAME, LOG_MEM, OUTOFMEM); \
		return NFSERR_IO; \
	} \
} while (0)

#define ER(x) do { \
	os_error *err = x; \
	if (x) { \
		syslogf(LOGNAME, LOG_SERIOUS, "%s", err->errmess); \
		return NULL; \
	} \
} while (0)

#define NR(x) do { \
	enum nstat status = x; \
	if (status != NFS_OK) return status; \
} while (0)

static struct export *parse_line(char *line, struct pool *pool)
{
	char *basedir;
	char *exportname;
	char *host;
	struct export *export;
	static int exportnum = 0;
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

	UE(export = palloc(sizeof(struct export), pool));

	/* Set defaults */
	export->ro = 1;
	export->matchuid = 1;
	export->uid = 0;
	export->matchuid = 1;
	export->uid = 0;
	export->imagefs = 0;
	export->defaultfiletype = 0xFFF;
	export->xyzext = NEEDED;
	export->next = NULL;
	export->pathentry = NULL;
	memset(export->hosts, 0, sizeof(unsigned int) * MAXHOSTS);
	UE(export->pool = pinit(pool));

	UE(export->basedir = pstrdup(basedir, pool));
	export->basedirlen = strlen(basedir);
	export->exportnum = exportnum++;
	if (exportnum > 128) {
		syslogf(LOGNAME, LOG_SERIOUS, "Only 128 export directories are supported");
		return NULL;
	}

	UE(export->exportname = pstrdup(exportname, pool));

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
		ER(gethostbyname_timeout(host, 5, &hp));
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
			export->matchuid = 1;
			export->uid = atoi(opt + 4);
		} else if (CHECK("gid") && opt[3] == '=') {
			export->matchgid = 1;
			export->gid = atoi(opt + 4);
		} else if (CHECK("imagefs")) {
			export->imagefs = 1;
		} else if (CHECK("logging")) {
			logging = 1;
		} else if (CHECK("defaultfiletype") && opt[15] == '=') {
			export->defaultfiletype = (int)strtol(opt + 16, NULL, 16);
		} else if (CHECK("addext") && opt[6] == '=') {
			export->xyzext = atoi(opt + 7);
		}
	}

	return export;
}

#define EXPORTSFILE "Choices:Moonfish.exports"

struct export *parse_exports_file(struct pool *pool)
{
	char line[1024];
	FILE *file;
	struct export *exports = NULL;

	file = fopen(EXPORTSFILE, "r");
	if (file == NULL) {
		syslogf(LOGNAME, LOG_SERIOUS, "Unable to open exports file (%s)", EXPORTSFILE);
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

	return exports;
}

static enum nstat copypath(char *dest, int destlen, char *src, int srclen)
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

enum nstat fh_to_path(char *fhandle, int fhandlelen, char **path, struct server_conn *conn)
{
	int extendedfh;
	int exportnum;
	int hash;

	extendedfh = ((((unsigned char)(fhandle[0])) & 0x80) != 0);
	exportnum = fhandle[0] & 0x7F;
	hash = fhandle[1];

	if (conn->export == NULL) {
		conn->export = conn->exports;
		while ((conn->export->exportnum != exportnum) && conn->export) {
			conn->export = conn->export->next;
		}
		if (conn->export == NULL) return NFSERR_STALE;
	}

	if ((conn->host & conn->export->mask) != conn->export->host) return NFSERR_ACCES;

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
				if (pathentry == NULL) return NFSERR_STALE;
				while (index >= PATHENTRIES) {
					pathentry = pathentry[PATHENTRIES].next;
					if (pathentry == NULL) return NFSERR_STALE;
					index -= PATHENTRIES;
				}
				if (pathentry[index].name == NULL) return NFSERR_STALE;
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

enum nstat path_to_fh(char *path, char **fhandle, unsigned int *fhandlelen, struct server_conn *conn)
{
	size_t len;

	if (*fhandle == NULL) {
		UR(*fhandle = palloc(*fhandlelen, conn->pool));
	}
	memset(*fhandle, 0, *fhandlelen);
	len = strlen(path);
	if ((len < conn->export->basedirlen) ||
	    (memcmp(path, conn->export->basedir, conn->export->basedirlen) != 0) ||
	    ((path[conn->export->basedirlen] != '.') &&
	     (path[conn->export->basedirlen] != '\0'))) {
		/* This should only ever happen if we are called from mount
		   to get the root filehandle */
		(*fhandle)[0] = conn->export->exportnum;
		(*fhandle)[1] = calc_fileid(conn->export->basedir, NULL) & 0xFF;
		*fhandlelen = 2;
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

	return NFS_OK;
}





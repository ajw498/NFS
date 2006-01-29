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


#include "rpc-decode.h"
#include "exports.h"
#include "serverconn.h"

#define RPCERRBASE 1

/* Generate a RISC OS error block based on the given number and message */
static os_error *gen_error(int num, char *msg, ...)
{
	static os_error module_err_buf;
	va_list ap;

	
	va_start(ap, msg);
	vsnprintf(module_err_buf.errmess, sizeof(module_err_buf.errmess), msg, ap);
	va_end(ap);
	module_err_buf.errnum = num;
	return &module_err_buf;
}

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



#define CHECK(str) (strncasecmp(opt,str,sizeof(str) - 1)==0)


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

	export = palloc(sizeof(struct export), pool);
	if (export == NULL) {
		/*FIXME*/
		return NULL;
	}

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
	export->pool = pinit();
	if (export->pool == NULL) {
		/*FIXME*/
		free(export);
		return NULL;
	}

	export->basedir = strdup(basedir);
	if (export->basedir == NULL) {
		/*FIXME*/
		pfree(export->pool);
		free(export);
		return NULL;
	}
	export->basedirlen = strlen(basedir);
	export->exportnum = exportnum++;
	/*FIXME if exportnum > 128 then error */

	export->exportname = strdup(exportname);
	if (export->basedir == NULL) {
		/*FIXME*/
		free(export->basedir);
		pfree(export->pool);
		free(export);
		return NULL;
	}

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
		if (gethostbyname_timeout(host, 5, &hp)) {
			printf("couldn't resolve\n");
			abort();
		}
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
		} else if (CHECK("defaultfiletype") && opt[15] == '=') {
			export->defaultfiletype = (int)strtol(opt + 16, NULL, 16);
		} else if (CHECK("addext") && opt[6] == '=') {
			export->xyzext = atoi(opt + 7);
		}
	}

	return export;
}

struct export *parse_exports_file(struct pool *pool)
{
	char line[1024];
	FILE *file;
	struct export *exports = NULL;

	file = fopen("Choices:Moonfish.exports","r");
	if (file == NULL) {
		printf("Can't open exports file\n");
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



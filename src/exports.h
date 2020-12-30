/*
	exports file parsing.


	Copyright (C) 2006 Alex Waugh

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

#ifndef EXPORTS_H
#define EXPORTS_H


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <swis.h>
#include <sys/errno.h>
#ifdef USE_TCPIPLIBS
# include <unixlib.h>
# include <riscos.h>
#endif
#include <stdarg.h>
#include <ctype.h>

#include "pools.h"
#include "utils.h"
#include "serverconn.h"

#define PATHENTRIES 32

struct pathentry {
	char *name;
	struct pathentry *next;
};

/* Define the maximum numer of hosts to remember for mount dump */
#define MAXHOSTS 10

struct export {
	char *basedir;
	size_t basedirlen;
	int exportnum;
	char *exportname;
	int ro;
	int matchuid;
	int uid;
	int matchgid;
	int gid;
	unsigned int host;
	unsigned int mask;
	unsigned int hosts[MAXHOSTS];
	int imagefs;
	int defaultfiletype;
	int xyzext;
	int fakedirtimes;
	int umask;
	int unumask;
	int udpsize;
	struct pool *pool;
	struct pathentry *pathentry;
	struct export *next;
};


struct export *parse_exports_file(struct pool *pool);

int calc_fileid(char *path, char *leaf);

nstat fh_to_path(char *fhandle, int fhandlelen, char **path, struct server_conn *conn);

nstat path_to_fh(char *path, char **fhandle, unsigned int *fhandlelen, struct server_conn *conn);

#endif


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

#ifndef EXPORTS_H
#define EXPORTS_H


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <swis.h>
#include <sys/errno.h>
#include <unixlib.h>
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

enum nstat fh_to_path(char *fhandle, int fhandlelen, char **path, struct server_conn *conn);

enum nstat path_to_fh(char *path, char **fhandle, unsigned int *fhandlelen, struct server_conn *conn);

#endif


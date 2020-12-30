/*
	Common bits for the client connection

        Copyright (C) 2007 Alex Waugh

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

#ifndef CLIENTCONN_H
#define CLIENTCONN_H


#include <kernel.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/errno.h>
#include <iconv.h>

#include "pools.h"


struct commonfh {
	char handle[MAXFHSIZE];
	int size;
};

/* All information associated with an open file */
struct file_handle {
	struct conn_info *conn;
	struct commonfh fhandle;
	unsigned int extent;
	unsigned int load;
	unsigned int exec;
	int dir;
	int commitneeded;
	char verf[8];
};

/* An tx or rx buffer. */
struct buffer_list {
	int len;
	int readlen;
	int lastsegment;
	int position;
	char buffer[SFBUFFERSIZE];
	struct buffer_list *next;
};

/* All infomation associated with a connection */
struct conn_info {
	char *server;
	unsigned int portmapper_port;
	unsigned int mount_port;
	unsigned int nfs_port;
	unsigned int pcnfsd_port;
	char *exportname;
	struct commonfh rootfh;
	char *config;
	struct sockaddr_in sockaddr;
	int sock;
	int connected;
	long timeout;
	int retries;
	int hidden;
	unsigned int umask;
	unsigned int unumask;
	unsigned int numgids;
	unsigned int uid;
	unsigned int gid;
	unsigned int gids[MAX_GIDS];
	char *username;
	char *password;
	int logging;
	char *auth;
	int authsize;
	char *machinename;
	int defaultfiletype;
	int xyzext;
	int localportmin;
	int localportmax;
	int maxdatabuffer;
	int followsymlinks;
	int pipelining;
	int casesensitive;
	int unixexfiletype;
	int escapewin;
	char lastdir[MAX_PATHNAME];
	uint64_t lastcookie;
	char lastcookieverf[NFS3_COOKIEVERFSIZE];
	int laststart;
	struct commonfh lastdirhandle;
	int tcp;
	int nfs3;
	iconv_t toenc;
	iconv_t fromenc;
	struct pool *pool;
	int txmutex;
	struct buffer_list *rxmutex;
	int reference;
};

extern int enablelog;

#endif


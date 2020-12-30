/*
	Browse for available mounts


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

#ifndef BROWSE_H
#define BROWSE_H

#ifdef __cplusplus
extern "C" {
#endif

struct hostinfo {
	int valid;
	unsigned mount1tcpport;
	unsigned mount1udpport;
	unsigned mount3tcpport;
	unsigned mount3udpport;
	unsigned nfs2tcpport;
	unsigned nfs2udpport;
	unsigned nfs3tcpport;
	unsigned nfs3udpport;
	unsigned pcnfsdtcpport;
	unsigned pcnfsdudpport;
	char ip[16];
	char host[256];
};

struct exportinfo {
	char *exportname;
	struct exportinfo *next;
};

enum broadcast_type {
	IDLE,
	BROADCAST,
	LISTEN,
	CLOSE,
	HOST
};

char *browse_gethost(struct hostinfo *info, enum broadcast_type type, const char *hostname);

char *browse_getexports(const char *host, unsigned port, unsigned mount3, unsigned tcp, struct exportinfo **ret);

char *browse_lookuppassword(const char *host, unsigned port, unsigned tcp, const char *username, const char *password, unsigned *uid, unsigned *umask, char *gids, size_t gidsize);

#ifdef __cplusplus
}
#endif

#endif

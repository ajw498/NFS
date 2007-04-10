/*
	$Id$

	Browse for available mounts


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
	unsigned pcnfsdtcpport;
	unsigned pcnfsdudpport;
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

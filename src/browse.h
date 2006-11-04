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

#ifdef __cplusplus
extern "C" {
#endif

struct hostinfo {
	int valid;
	unsigned mount1tcpport;
	unsigned mount1udpport;
	unsigned mount3tcpport;
	unsigned mount3udpport;
	char host[256];
};

char *browse_gethost(struct hostinfo *info, int type);

char *browse_getexports(char *host, unsigned port, unsigned mount3, unsigned tcp, char **ret);

#ifdef __cplusplus
}
#endif

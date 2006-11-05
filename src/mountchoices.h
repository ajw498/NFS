/*
	$Id$

	Frontend for creating mount files


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

#ifndef MOUNTCHOICES_H
#define MOUNTCHOICES_H

#define STRMAX 256

class mount {
public:
	bool showhidden;
	int followsymlinks;
	bool casesensitive;
	bool unixex;
	int defaultfiletype;
	int addext;
	int unumask;
	int portmapperport;
	int nfsport;
	int pcnfsdport;
	int mountport;
	int maxdatabuffer;
	bool pipelining;
	int timeout;
	int retries;
	bool logging;
	char server[STRMAX];
	char exportname[STRMAX];
	char machinename[STRMAX];
	char username[STRMAX];
	char password[STRMAX];
	int localportmin;
	int localportmax;
	bool uidvalid;
	int uid;
	char gids[STRMAX];
	int umask;
	bool usepcnfsd;
	bool tcp;
	bool nfs3;
	char leafname[STRMAX];
	char encoding[STRMAX];

	mount();
	void save(const char *host, const char *mountname);
	void load(const char *host, const char *mountname);
	void genfilename(const char *host, const char *mountname, char *buffer, int len);
};

#endif


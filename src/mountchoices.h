/*
	$Id$

	Mount choices reading and writing


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

#include <string>
#include <vector>

using std::string;

class mountchoices {
public:
	int showhidden;
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
	bool logging;
	string server;
	string exportname;
	string machinename;
	string username;
	string password;
	int localportmin;
	int localportmax;
	bool uidvalid;
	unsigned uid;
	string gids;
	unsigned umask;
	bool usepcnfsd;
	bool tcp;
	bool nfs3;
	string encoding;

	mountchoices();
	void save(const string& filename);
	void load(const string& filename);
	string stringsave();
	string genfilename(const string& host, const string& mountname);
};

struct alias {
	string alias;
	string host;
	string dir;
};

class aliases {
public:
	void save();
	void load();
	void add(const string& aliasname, const string& host, const string& dir);
	void gethost(const string& aliasname, string& host, string& dir);
	string getalias(const string& host, const string& dir);
private:
	std::vector<alias> data;
};

#endif


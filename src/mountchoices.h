/*
	Mount choices reading and writing


	Copyright (C) 2003 Alex Waugh

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
	bool escapewin;

	mountchoices();
	void save(const string& filename);
	void load(const string& filename);
	string stringsave();
	string genfilename(const string& host, const string& ip, const string& mountname);
private:
	string genfilename2(const string& host, const string& mountname);
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


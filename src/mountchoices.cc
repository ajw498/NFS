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

#include "rtk/os/exception.h"
#include "rtk/os/os.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <kernel.h>
#include <stdarg.h>
#include <stddef.h>
#include <ctype.h>

#include "sunfishdefs.h"
#include "sunfish.h"
#include "mountchoices.h"

using namespace std;

#define STRMAX 256

extern "C" {
// These are declared in unixlib.h but without C linkage
int strcasecmp(char const*, char const*);
int strncasecmp(char const*, char const*, unsigned int);
}

mountchoices::mountchoices(void) :
	showhidden(1),
	followsymlinks(5),
	casesensitive(false),
	unixex(false),
	defaultfiletype(0xFFF),
	addext(1),
	unumask(0600),
	portmapperport(0),
	nfsport(0),
	pcnfsdport(0),
	mountport(0),
	maxdatabuffer(0),
	pipelining(false),
	timeout(20),
	logging(false),
	localportmin(0),
	localportmax(0),
	uidvalid(false),
	uid(0),
	umask(022),
	usepcnfsd(false),
	tcp(false),
	nfs3(false),
	encoding("No conversion"),
	escapewin(false)
{
}

#define SAVEPARENT "<Choices$Write>.Sunfish"
#define SAVEDIR SAVEPARENT ".mountsave"

string mountchoices::genfilename(const string& host, const string& ip, const string& mountname)
{
	string filename;
	unsigned int type;

	filename = genfilename2(ip, mountname);
	rtk::os::OS_File17(filename.c_str(), &type, NULL, NULL, NULL, NULL);
	if (type == 0) {
		filename = genfilename2(host, mountname);
	}
	return filename;
}

string mountchoices::genfilename2(const string& host, const string& mountname)
{
	string filename = SAVEDIR ".";
	for (size_t i = 0; i < host.length(); i++) {
		if (isalnum(host[i]) || (host[i] == '/')) {
			filename += host[i];
		} else {
			filename += '?';
		}
	}
	for (size_t i = 0; i < mountname.length(); i++) {
		if (isalnum(mountname[i]) || (mountname[i] == '/')) {
			filename += mountname[i];
		} else {
			filename += '?';
		}
	}
	return filename;
}

#include <sstream>
#include <iomanip>

string mountchoices::stringsave()
{
	ostringstream str;
	str << "Protocol: NFS" << (nfs3 ? "3" : "2");
	str << "\nServer: " << server;
	str << "\nExport: " << exportname;
	if (usepcnfsd) {
		str << "\nPassword: " << password;
		str << "\nUsername: " << username;
	} else if (uidvalid) {
		int gid;
		char *othergids;
		str << "\nuid: " << uid;
		gid = (int)strtol(gids.c_str(), &othergids, 10);
		str << "\ngid: " << gid;
		str << "\ngids: " << othergids;
		str << "\numask: " << oct << setw(3) << setfill('0') << umask << dec;
	}
	str << "\nTransport: " << (tcp ? "TCP" : "UDP");
	str << "\nShowHidden: " << showhidden;
	str << "\nFollowSymlinks: " << followsymlinks;
	str << "\nCaseSensitive: " << casesensitive;
	str << "\nUnixEx: " << unixex;
	str << "\nTranslateWin: " << escapewin;
	str << "\nDefaultFiletype: " << hex << uppercase << setw(3) << setfill('0') << defaultfiletype << dec;
	str << "\nAddExt: " << addext;
	str << "\nunumask: " << oct << setw(3) << setfill('0') << unumask << dec;
	if (portmapperport) str << "\nPortmapperPort: " << portmapperport;
	if (nfsport) str << "\nNFSPort: " << nfsport;
	if (pcnfsdport) str << "\nPCNFSDPort: " << pcnfsdport;
	if (mountport) str << "\nMountPort: " << mountport;
	if (localportmin && localportmin) str << "\nLocalPort: " << localportmin << " " << localportmax;
	if (machinename[0]) str << "\nMachineName: " << machinename;
	if ((strcasecmp(encoding.c_str(), "No conversion") != 0) && encoding.length()) str << "\nEncoding: " << encoding;

	str << "\nMaxDataBuffer: " << maxdatabuffer;
	str << "\nPipelining: " << pipelining;
	str << "\nTimeout: " << timeout;
	str << "\nLogging: " << logging;

	return str.str();
}

void mountchoices::save(const string& filename)
{
	FILE *file;
	rtk::os::OS_File8(SAVEPARENT, 0);
	rtk::os::OS_File8(SAVEDIR, 0);
	file = fopen(filename.c_str(), "w");
	if (file == NULL) throw rtk::os::exception(_kernel_last_oserror());

	string data = stringsave();
	fwrite(data.data(), data.size(), 1, file);
	fclose(file);

	rtk::os::OS_File18(filename.c_str(), SUNFISH_FILETYPE);
}

#define CHECK(str) (strncasecmp(line,str,sizeof(str))==0)

void mountchoices::load(const string& filename)
{
	FILE *file;
	char buffer[STRMAX];

	file = fopen(filename.c_str(), "r");
	if (file == NULL) return;

	while (fgets(buffer, STRMAX, file) != NULL) {
		char *val;
		char *line;

		/* Strip trailing whitespace */
		val = buffer;
		while (*val) val++;
		while (val > buffer && isspace(val[-1])) val--;
		*val = '\0';

		/* Strip leading whitespace */
		line = buffer;
		while (isspace(*line)) line++;

		/* Find the end of the field */
		val = line;
		while (*val && *val != ':') val++;
		if (*val) *val++ = '\0';
		/* Find the start of the value */
		while (isspace(*val)) val++;

		if (CHECK("#")) {
			/* A comment */
		} else if (CHECK("Protocol")) {
			nfs3 = strcasecmp(val, "NFS3") == 0;
		} else if (CHECK("Server")) {
			server = val;
		} else if (CHECK("MachineName")) {
			machinename = val;
		} else if (CHECK("Encoding")) {
			encoding = val;
		} else if (CHECK("PortMapperPort")) {
			portmapperport = (int)strtol(val, NULL, 10);
		} else if (CHECK("MountPort")) {
			mountport = (int)strtol(val, NULL, 10);
		} else if (CHECK("NFSPort")) {
			nfsport = (int)strtol(val, NULL, 10);
		} else if (CHECK("PCNFSDPort")) {
			pcnfsdport = (int)strtol(val, NULL, 10);
		} else if (CHECK("Transport")) {
			tcp = strcasecmp(val, "tcp") == 0;
		} else if (CHECK("Export")) {
			exportname = val;
		} else if (CHECK("UID")) {
			usepcnfsd = false;
			uidvalid = true;
			uid = (int)strtol(val, NULL, 10);
		} else if (CHECK("GID") || CHECK("GIDs")) {
			gids += val;
		} else if (CHECK("Username")) {
			usepcnfsd = 1;
			username = val;
		} else if (CHECK("Password")) {
			password = val;
		} else if (CHECK("Logging")) {
			logging = (int)strtol(val, NULL, 10);
		} else if (CHECK("umask")) {
			umask = 07777 & (int)strtol(val, NULL, 8); /* umask is specified in octal */
		} else if (CHECK("unumask")) {
			unumask = 07777 & (int)strtol(val, NULL, 8); /* unumask is specified in octal */
		} else if (CHECK("ShowHidden")) {
			showhidden = (int)strtol(val, NULL, 10);
		} else if (CHECK("Timeout")) {
			timeout = (int)strtol(val, NULL, 10);
		} else if (CHECK("DefaultFiletype")) {
			defaultfiletype = 0xFFF & (int)strtol(val, NULL, 16);
		} else if (CHECK("AddExt")) {
			addext = (int)strtol(val, NULL, 10);
		} else if (CHECK("LocalPort")) {
			char *end;

			localportmin = (int)strtol(val, &end, 10);
			localportmax = (int)strtol(end, NULL, 10);
			if (localportmax == 0) localportmax = localportmin;
		} else if (CHECK("Pipelining")) {
			pipelining = (int)strtol(val, NULL, 10);
		} else if (CHECK("MaxDataBuffer")) {
			maxdatabuffer = (int)strtol(val, NULL, 10);
			if (maxdatabuffer > MAXDATABUFFER_TCP_MAX) {
				maxdatabuffer = MAXDATABUFFER_TCP_MAX;
			}
			maxdatabuffer = (maxdatabuffer + (MAXDATABUFFER_INCR - 1)) & ~(MAXDATABUFFER_INCR - 1);
		} else if (CHECK("FollowSymlinks")) {
			followsymlinks = (int)strtol(val, NULL, 10);
		} else if (CHECK("CaseSensitive")) {
			casesensitive = (int)strtol(val, NULL, 10);
		} else if (CHECK("UnixEx")) {
			unixex = (int)strtol(val, NULL, 10);
		} else if (CHECK("TranslateWin")) {
			escapewin = (int)strtol(val, NULL, 10);
		}
	}
	fclose(file);
}


void aliases::save()
{
	FILE *file;
	file = fopen("<Choices$Dir>.Sunfish.aliases", "w");
	if (file == NULL) throw rtk::os::exception(_kernel_last_oserror());

	for (vector<alias>::iterator i = data.begin(); i != data.end(); ++i) {
		fprintf(file, "%s#%s#%s\n", i->alias.c_str(), i->host.c_str(), i->dir.c_str());
	}
	fclose(file);
}

void aliases::load()
{
	FILE *file;
	char buffer[STRMAX];

	file = fopen("Choices:Sunfish.aliases", "r");
	if (file == NULL) return;

	while (fgets(buffer, STRMAX, file) != NULL) {
		char *val;
		char *aliasname;
		char *host;
		char *dir;

		aliasname = val = buffer;
		while (*val && *val != '#' && *val != '\n') val++;
		if (*val) *val++ = '\0';
		host = val;
		while (*val && *val != '#' && *val != '\n') val++;
		if (*val) *val++ = '\0';
		dir = val;
		while (*val && *val != '\n') val++;
		if (*val) *val++ = '\0';

		alias newalias;
		newalias.alias = aliasname;
		newalias.host = host;
		newalias.dir = dir;
		data.push_back(newalias);
	}
	fclose(file);
}

void aliases::add(const string& aliasname, const string& host, const string& dir)
{
	bool added = false;

	// Search for existing entries to use
	// This relies on the aliases in the vector being unique,
	// and the host-dir pairs being unique
	for (vector<alias>::iterator i = data.begin(); i != data.end(); ++i) {
		if ((i->alias == aliasname) || ((i->host == host) && (i->dir == dir))) {
			if (added) {
				data.erase(i);
				break;
			} else {
				i->alias = aliasname;
				i->host = host;
				i->dir = dir;
				added = true;
			}
		}
	}
	if (!added) {
		alias newalias;
		newalias.alias = aliasname;
		newalias.host = host;
		newalias.dir = dir;
		data.push_back(newalias);
	}
}

void aliases::gethost(const string& aliasname, string& host, string& dir)
{
	for (vector<alias>::iterator i = data.begin(); i != data.end(); ++i) {
		if (i->alias == aliasname) {
			host = i->host;
			dir = i->dir;
			return;
		}
	}
	host = "";
	dir = "";
}

string aliases::getalias(const string& host, const string& dir)
{
	for (vector<alias>::iterator i = data.begin(); i != data.end(); ++i) {
		if ((i->host == host) && (i->dir == dir)) return i->alias;
	}
	return "";
}

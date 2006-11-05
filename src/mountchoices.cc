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
	unumask(0), // FIXME?
	portmapperport(0),
	nfsport(0),
	pcnfsdport(0),
	mountport(0),
	maxdatabuffer(0),
	pipelining(false),
	timeout(3),
	retries(2),
	logging(false),
	localportmin(0),
	localportmax(0),
	uidvalid(false),
	uid(0),
	umask(022),
	usepcnfsd(false),
	tcp(false),
	nfs3(false)
{
	server[0] = '\0';
	exportname[0] = '\0';
	machinename[0] = '\0';
	username[0] = '\0';
	password[0] = '\0';
	gids[0] = '\0';
	leafname[0] = '\0';
	strcpy(encoding, "No conversion");
}

string mountchoices::genfilename(const string& host, const string& mountname)
{
	string filename = "Sunfish:mounts.auto.";
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

void mountchoices::save(const string& filename)
{
	FILE *file;
	file = fopen(filename.c_str(), "w");
	if (file == NULL) throw rtk::os::exception(_kernel_last_oserror());

	fprintf(file,"Protocol: NFS%s\n", nfs3 ? "3" : "2");
	fprintf(file,"Server: %s\n",server);
	fprintf(file,"Export: %s\n",exportname);
	if (usepcnfsd) {
		fprintf(file,"Password: %s\n",password);
		fprintf(file,"Username: %s\n",username);
	} else {
		int gid;
		char *othergids;
		fprintf(file,"uid: %d\n",uid);
		gid = (int)strtol(gids, &othergids, 10);
		fprintf(file,"gid: %d\n",gid);
		fprintf(file,"gids: %s\n",othergids);
		fprintf(file,"umask: %.3o\n",umask);
	}
	fprintf(file,"Transport: %s\n",tcp ? "TCP" : "UDP");
	fprintf(file,"ShowHidden: %d\n",showhidden);
	fprintf(file,"FollowSymlinks: %d\n",followsymlinks);
	fprintf(file,"CaseSensitive: %d\n",casesensitive);
	fprintf(file,"UnixEx: %d\n",unixex);
	fprintf(file,"DefaultFiletype: %.3X\n",defaultfiletype);
	fprintf(file,"AddExt: %d\n",addext);
	fprintf(file,"unumask: %.3o\n",unumask);
	if (portmapperport) fprintf(file,"PortmapperPort: %d\n",portmapperport);
	if (nfsport) fprintf(file,"NFSPort: %d\n",nfsport);
	if (pcnfsdport) fprintf(file,"PCNFSDPort: %d\n",pcnfsdport);
	if (mountport) fprintf(file,"MountPort: %d\n",mountport);
	if (localportmin && localportmin) fprintf(file,"LocalPort: %d %d\n",localportmin,localportmax);
	if (machinename[0]) fprintf(file,"MachineName: %s\n",machinename);
	if ((strcasecmp(encoding, "No conversion") != 0) && encoding[0]) fprintf(file,"Encoding: %s\n", encoding);

	fprintf(file,"MaxDataBuffer: %d\n",maxdatabuffer);
	fprintf(file,"Pipelining: %d\n",pipelining);
	fprintf(file,"Timeout: %d\n",timeout);
	fprintf(file,"Retries: %d\n",retries);
	fprintf(file,"Logging: %d\n",logging);
	fclose(file);

	rtk::os::OS_File18(filename.c_str(), SUNFISH_FILETYPE);
}

#define CHECK(str) (strncasecmp(line,str,sizeof(str))==0)

void mountchoices::load(const string& filename)
{
	FILE *file;
	char buffer[STRMAX];

//	snprintf(leafname, STRMAX, "%s", mountname);

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
			nfs3 = /*case*/strcmp(val, "NFS3") == 0;
		} else if (CHECK("Server")) {
			strcpy(server, val);
		} else if (CHECK("MachineName")) {
			strcpy(machinename, val);
		} else if (CHECK("Encoding")) {
			strcpy(encoding, val);
		} else if (CHECK("PortMapperPort")) {
			portmapperport = (int)strtol(val, NULL, 10);
		} else if (CHECK("MountPort")) {
			mountport = (int)strtol(val, NULL, 10);
		} else if (CHECK("NFSPort")) {
			nfsport = (int)strtol(val, NULL, 10);
		} else if (CHECK("PCNFSDPort")) {
			pcnfsdport = (int)strtol(val, NULL, 10);
		} else if (CHECK("Transport")) {
			tcp = /*case*/strcmp(val, "tcp") == 0;
		} else if (CHECK("Export")) {
			strcpy(exportname, val);
		} else if (CHECK("UID")) {
			usepcnfsd = false;
			uidvalid = true;
			uid = (int)strtol(val, NULL, 10);
		} else if (CHECK("GID") || CHECK("GIDs")) {
			strncat(gids, val, STRMAX - strlen(gids));
			gids[STRMAX - 1] = '\0';
		} else if (CHECK("Username")) {
			usepcnfsd = 1;
			strcpy(username, val);
		} else if (CHECK("Password")) {
			strcpy(password, val);
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
		} else if (CHECK("Retries")) {
			retries = (int)strtol(val, NULL, 10);
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
		}
	}
	fclose(file);
}


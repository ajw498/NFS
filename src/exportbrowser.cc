/*
	$Id$

	Browser for exports provided by a host


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

#include "rtk/os/wimp.h"

#include "exportbrowser.h"
#include "browse.h"
#include "sunfish.h"
#include "sunfishdefs.h"
#include "newfe.h"


using namespace std;
using namespace rtk;
using namespace rtk::desktop;
using rtk::graphics::point;
using rtk::graphics::box;


exportbrowser::exportbrowser(hostinfo host) :
	info(host)
{
	char *err;

	title(info.host);

	char *exports[256];
	err = browse_getexports(info.host, info.mount1udpport, 1, 0, exports);
	if (err) throw err;
	for (int j = 0; exports[j]; j++) {
		add_icon(exports[j], "file_1b6");
	}
}

void exportbrowser::doubleclick(const std::string& item)
{
	string filename = "Sunfish:mounts.auto."+item;
//	ofstream mfile(filename.c_str());
//	if (!mfile) throw "Cannot open file";
//	mfile<<"Protocol: NFS3\nServer: "/*<<info.host*/<<"\nExport: "<<item;
//	mfile<<"\nTransport: UDP\n";
//	mfile<<"Foo";
//	mfile.close();

/*	os::OS_File8("Sunfish:mounts.auto");

	FILE *mfile = fopen(filename.c_str(), "w");
	if (!mfile) throw "Cannot open file";
	fprintf(mfile, "Protocol: NFS3\nServer: %s\nExport: %s\nTransport: UDP\n", info.host, item.c_str());
	fclose(mfile);

	os::OS_File18(filename.c_str(), SUNFISH_FILETYPE);

	string cmd = "Filer_OpenDir "+filename;
	os::Wimp_StartTask(cmd.c_str(), NULL); */
	sunfish *app = static_cast<sunfish *>(parent_application());
	app->ggetuid.setup(info, item, *app);
}

exportbrowser::~exportbrowser()
{

}


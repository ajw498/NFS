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

void exportbrowser::open_menu(const std::string& item, bool selection, rtk::events::mouse_click& ev)
{
	menuitem = item;

	menu.title("Export");
	menu.add(display,0);
	menu.add(edititem,1);
	menu.add(clear,2);
	menu.add(refresh,3);
	display.text("Display");
	display.enabled(false);
	edititem.text(selection ? "Selection" : "Edit '"+item+"'");
	edititem.enabled(!selection && item.compare("") != 0);
	edititem.attach_submenu(edit);
	clear.text("Clear selection");
	clear.enabled(item.compare("") != 0);
	refresh.text("Refresh").enabled(false);

	edit.title("Edit export");
	edit.add(namemount, 0);
	edit.add(filenames, 1);
	edit.add(connection, 2);
	edit.add(uids, 3);

	namemount.text("Name export...");
	filenames.text("Filename and filetype choices...");
	connection.text("Connection choices...");
	uids.text("User id choices...");

	menu.show(ev);
}

void exportbrowser::handle_event(rtk::events::menu_selection& ev)
{
	if (ev.target() == &clear) {
		for (int i = icons.size() - 1; i >= 0; i--) icons[i]->selected(false);
	} else if (ev.target() == &refresh) {
		//broadcast();
	} else if (ev.target() == &namemount) {
	} else if (ev.target() == &filenames) {
		filenameswin.load(info.host, menuitem);
		parent_application()->add(filenameswin, point(2*640,2*512));
		// bring to front if already open?
	} else if (ev.target() == &connection) {
		connectionwin.load(info.host, menuitem);
		parent_application()->add(connectionwin, point(2*640,2*512));
	} else if (ev.target() == &uids) {
		uidswin.load(info.host, menuitem);
		parent_application()->add(uidswin, point(2*640,2*512));
	}
}

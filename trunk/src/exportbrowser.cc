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
#include "sunfish-frontend.h"
#include "utils.h"

#include <sstream>

using namespace std;
using namespace rtk;
using namespace rtk::desktop;
using rtk::graphics::point;
using rtk::graphics::box;


exportbrowser::exportbrowser(hostinfo host, int port, bool tcp, int version) :
	info(host),
	usetcp(tcp),
	nfsversion(version),
	useport(port),
	connectionwin(((version >= 3) && tcp) ? 32768 : 8192)
{
	ostringstream details;
	details << "NFS" << version << " ";
	details << (tcp ? "TCP " : "UDP ");
	details << info.host;
	title(details.str());
}

void exportbrowser::open(sunfish& app)
{
	point pos;

	if (parent_application()) {
		// We are already open, so find the current position
		pos = origin();
	} else {
		// Open window near mouse position.
		os::pointer_info_get blk;
		os::Wimp_GetPointerInfo(blk);
		pos = blk.p - point(64,0);
	}
	app.add(*this,pos);
	smallicons(app.smallicons());
}

void exportbrowser::refresh()
{
	char *err;
	struct exportinfo *res;

	remove_all_icons();

	err = browse_getexports(info.host, useport, nfsversion == 3, usetcp, &res);
	if (err) throw err;
	while (res) {
		add_icon(res->exportname, "file_1b6");
		res = res->next;
	}
}

void exportbrowser::doubleclick(const std::string& item, rtk::events::mouse_click& ev)
{
	sunfish& app = *static_cast<sunfish *>(parent_application());
	app.ggetuid.setup(info, item, usetcp, nfsversion, app);
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
	menu.add(refreshwin,3);
	display.text("Display");
	display.attach_submenu(displaymenu);
	edititem.text(selection ? "Selection" : "Edit '"+item+"'");
	edititem.enabled(!selection && item.compare("") != 0);
	edititem.attach_submenu(edit);
	clear.text("Clear selection");
	clear.enabled(item.compare("") != 0);
	refreshwin.text("Refresh");

	displaymenu.title("Display");
	displaymenu.add(largeiconsitem, 0);
	displaymenu.add(smalliconsitem, 1);

	sunfish& app = *static_cast<sunfish *>(parent_application());
	largeiconsitem.text("Large icons");
	largeiconsitem.tick(!app.smallicons());
	smalliconsitem.text("Small icons");
	smalliconsitem.tick(app.smallicons());

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
	sunfish& app = *static_cast<sunfish *>(parent_application());

	if (ev.target() == &clear) {
		for (int i = icons.size() - 1; i >= 0; i--) icons[i]->selected(false);
	} else if (ev.target() == &refreshwin) {
		refresh();
	} else if (ev.target() == &namemount) {
		namewin.open(info.host, menuitem, app);
	} else if (ev.target() == &filenames) {
		filenameswin.open(info.host, menuitem, app);
	} else if (ev.target() == &connection) {
		connectionwin.open(info.host, menuitem, app);
	} else if (ev.target() == &uids) {
		uidswin.open(info.host, menuitem, app);
	} else if (ev.target() == &largeiconsitem) {
		largeiconsitem.tick(true);
		smalliconsitem.tick(false);
		smallicons(false);
		app.smallicons(false);
	} else if (ev.target() == &smalliconsitem) {
		largeiconsitem.tick(false);
		smalliconsitem.tick(true);
		smallicons(true);
		app.smallicons(true);
	}
}

void exportbrowser::handle_event(rtk::events::save_to_app& ev)
{
	if (application* app = parent_application()) {
		saveop.load(info.host, ev.leafname(), usetcp, nfsversion);
		app->add(saveop);
		char buf[256];
		int len = ev.leafname().length();
		char src[len];
		for (int i = 0; i < len; i++) {
			if (ev.leafname()[i] == '/') {
				src[i] = '.';
			} else {
				src[i] = ev.leafname()[i];
			}
		}
		len = filename_riscosify(src, len, buf, sizeof(buf), NULL, 0xFFF, NEVER, 0, 0);
		if (len) {
			// Change dots back to slashes, as we want to fit the
			// whole pathname as the leafname
			for (int i = 0; i < len; i++) {
				if (buf[i] == '.') buf[i] = '/';
			}
			events::save_to_app ev(saveop, buf);
			ev.post();
		}
	}

	// Deselect all icons
	for (int i = icons.size() - 1; i >= 0; i--) icons[i]->selected(false);
}

exportsave::exportsave() :
	mount(0)
{
	filetype(SUNFISH_FILETYPE);
}

void exportsave::get_block(const void** data,size_type* count)
{
	if (done) {
		// If finished then return an empty block.
		if (data) *data=0;
		if (count) *count=0;
	} else {
		string str = mount->stringsave();
		if (data) *data = str.data();
		if (count) *count = str.size();
		done = true;
	}
}

void exportsave::load(const std::string& host, const std::string& exportname, bool tcp, int version)
{
	if (mount) delete mount;
	mount = new mountchoices;
	mount->load(mount->genfilename(host, exportname));
	mount->server = host;
	mount->exportname = exportname;
	mount->nfs3 = version == 3;
	mount->tcp = tcp;
}


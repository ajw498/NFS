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


using namespace std;
using namespace rtk;
using namespace rtk::desktop;
using rtk::graphics::point;
using rtk::graphics::box;


exportbrowser::exportbrowser(hostinfo host) :
	info(host)
{
	title(info.host);
	smallicons(false); //FIXME
}

void exportbrowser::refresh(int port, bool tcp, int version)
{
	char *err;
	struct exportinfo *res;

	usetcp = tcp;
	nfsversion = version;
	useport = port;

	remove_all_icons();

	err = browse_getexports(info.host, port, version == 3, tcp, &res);
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
		refresh(useport, usetcp, nfsversion);
	} else if (ev.target() == &namemount) {
		namewin.open(info.host, menuitem, app);
	} else if (ev.target() == &filenames) {
		filenameswin.load(info.host, menuitem);
		app.add(filenameswin, point(2*640,2*512));
		// bring to front if already open?
	} else if (ev.target() == &connection) {
		connectionwin.open(info.host, menuitem, app);
	} else if (ev.target() == &uids) {
		uidswin.load(info.host, menuitem);
		app.add(uidswin, point(2*640,2*512));
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

void exportbrowser::drag_ended(bool adjust, rtk::events::user_drag_box& ev)
{
	icon *src = static_cast<icon *>(ev.target());

	if (application* app = parent_application()) {
		app->add(saveop);
		events::save_to_app ev(saveop, src->text()); // FIXME sanitise leafname
		ev.post();
	}
	//FIXME clear selection
	// handle selection drag

	// This method should be a save_to_app handler

}

exportsave::exportsave()
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
//		mount.load();
		string str = mount.stringsave();
		if (data) *data = str.data();
		if (count) *count = str.size();
		done = true;
	}
}


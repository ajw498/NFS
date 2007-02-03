/*
	$Id$

	Frontend for browsing and creating mounts


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

#include "rtk/desktop/application.h"
#include "rtk/desktop/menu_item.h"
#include "rtk/desktop/menu.h"
#include "rtk/desktop/info_dbox.h"
#include "rtk/desktop/ibar_icon.h"
#include "rtk/desktop/label.h"
#include "rtk/desktop/writable_field.h"
#include "rtk/desktop/action_button.h"
#include "rtk/desktop/default_button.h"
#include "rtk/desktop/grid_layout.h"
#include "rtk/desktop/row_layout.h"
#include "rtk/desktop/column_layout.h"
#include "rtk/events/menu_selection.h"
#include "rtk/events/close_window.h"
#include "rtk/events/null_reason.h"
#include "rtk/os/wimp.h"

#include "sunfish.h"
#include "sunfishdefs.h"

#include "editname.h"
#include "mountchoices.h"
#include "sunfish-frontend.h"

#include <sstream>
#include <iomanip>

editname::editname()
{
	title("Name mount");
	close_icon(false);

	description.text("Set the local discname of the mount");
	name.text("",256);
	cancel.text("Cancel");
	savebutton.text("Save");

	layout1.margin(16).ygap(8);
	layout1.add(description);
	layout1.add(name);
	layout1.add(layout2);

	layout2.xgap(16);
	layout2.add(cancel);
	layout2.add(savebutton);

	add(layout1);
}

void editname::open(const string& host, const string& exportname, sunfish& app)
{
	load(host, exportname, app);

	// Open window near mouse position.
	os::pointer_info_get blk;
	os::Wimp_GetPointerInfo(blk);

	app.add(*this, blk.p - point(64,0));

	name.set_caret_position(point(),-1,name.text().length());
}

void editname::load(const string& host, const string& exportname, sunfish& app)
{
	if (host != "") {
		hostname = host;
		exportdir = exportname;
	}

	name.text(app.hostaliases.getalias(hostname, exportdir));
}

void editname::save()
{
	sunfish& app = *static_cast<sunfish *>(parent_application());

	app.hostaliases.add(name.text(), hostname, exportdir);

	app.hostaliases.save();
}

void editname::handle_event(events::mouse_click& ev)
{
	sunfish& app = *static_cast<sunfish *>(parent_application());
	if (ev.buttons() == 2) {
	} else if (ev.target() == &savebutton) {
		save();
		if (ev.buttons() == 4) remove();
	} else if (ev.target() == &cancel) {
		if (ev.buttons() == 4) {
			remove();
		} else {
			string none;
			load(none, none, app);
		}
	}
}

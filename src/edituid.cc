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
#include "rtk/desktop/filer_window.h"
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

#include "edituid.h"
#include "mountchoices.h"

#include <sstream>

edituid::edituid()
{
	title("UID and permissions choices");
	close_icon(false);

	uidlabel.text("uid");
	gidslabel.text("gids");
	umasklabel.text("umask");
	unumasklabel.text("unumask");
	cancel.text("Cancel");
	savebutton.text("Save");

	layout1.margin(16).ygap(8);
	layout1.add(layout2);
	layout1.add(layout3);

	layout2.xgap(8).ygap(8);
	layout2.add(uidlabel, 0, 0);
	layout2.add(uid, 1, 0);
	layout2.add(gidslabel, 0, 1);
	layout2.add(gids, 1, 1);
	layout2.add(umasklabel, 0, 2);
	layout2.add(umask, 1, 2);
	layout2.add(unumasklabel, 0, 3);
	layout2.add(unumask, 1, 3);

	layout3.xgap(16);
	layout3.add(cancel);
	layout3.add(savebutton);

	add(layout1);
}


void edituid::load(const string& host, string& exportname)
{
	mountchoices mountinfo;
	if (host.length() > 0) {
		filename = mountinfo.genfilename(host, exportname);
	}
	mountinfo.load(filename);

	ostringstream str;
	if (mountinfo.uidvalid) str<<mountinfo.uid;
	uid.text(str.str());
	str.str("");
	if (mountinfo.uidvalid) str<<mountinfo.gids;
	gids.text(str.str());
	str.str("");
	str.setf(ios_base::oct, ios_base::basefield);
	str.width(3);
	str.fill('0');
	str<<mountinfo.umask;
	umask.text(str.str());
	str.str("");
	str.width(3);
	str.fill('0');
	str<<mountinfo.unumask;
	unumask.text(str.str());
}

void edituid::save()
{
	mountchoices mountinfo;
	mountinfo.load(filename);

	mountinfo.uid = atoi(uid.text().c_str());
	strcpy(mountinfo.gids, uid.text().c_str());
	mountinfo.umask = strtol(umask.text().c_str(), NULL, 8);
	mountinfo.unumask = strtol(unumask.text().c_str(), NULL, 8);

	mountinfo.save(filename);
}

void edituid::handle_event(events::mouse_click& ev)
{
	if (ev.buttons() == 2) {
	} else if (ev.target() == &savebutton) {
		save();
		if (ev.buttons() == 4) remove();
	} else if (ev.target() == &cancel) {
		if (ev.buttons() == 4) {
			remove();
		} else {
			string none;
			load(none, none);
		}
	}
}

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

#include "editconnection.h"
#include "mountchoices.h"

#include <sstream>

editconnection::editconnection()
{
	title("Connection choices");
	close_icon(false);

	//maxdata.labeltext("Max data buffer size"); // Units?
	pipelining.text("Pipeline requests to increase speed");
	timeoutlabel.text("Timeout");
	timeoutunits.text("seconds");
	retrieslabel.text("Retries");
	logging.text("Log debug information to syslog");
	cancel.text("Cancel");
	savebutton.text("Save");

	layout1.margin(16).ygap(8);
	layout1.add(pipelining);
	layout1.add(layout2);
	layout1.add(logging);
	layout1.add(layout3);

	layout2.xgap(8).ygap(8);
	layout2.add(timeoutlabel, 0, 0);
	layout2.add(timeout, 1, 0);
	layout2.add(timeoutunits, 2, 0);
	layout2.add(retrieslabel, 0, 1);
	layout2.add(retries, 1, 1);

	layout3.xgap(16);
	layout3.add(cancel);
	layout3.add(savebutton);

	add(layout1);
}


void editconnection::load(const string& host, string& exportname)
{
	mountchoices mountinfo;
	if (host.length() > 0) {
		filename = mountinfo.genfilename(host, exportname);
	}
	mountinfo.load(filename);

	pipelining.selected(mountinfo.pipelining);
	ostringstream time;
	time<<mountinfo.timeout;
	timeout.text(time.str());
	ostringstream tries;
	tries<<mountinfo.retries;
	retries.text(tries.str());
	logging.selected(mountinfo.logging);
}

void editconnection::save()
{
	mountchoices mountinfo;
	mountinfo.load(filename);

	mountinfo.pipelining = pipelining.selected();
	mountinfo.timeout = atoi(timeout.text().c_str());
	mountinfo.retries = atoi(retries.text().c_str());
	mountinfo.logging = logging.selected();

	mountinfo.save(filename);
}

void editconnection::handle_event(events::mouse_click& ev)
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

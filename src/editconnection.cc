/*
	$Id$

	Edit connection related choices


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

#include "editconnection.h"
#include "mountchoices.h"
#include "sunfish-frontend.h"

#include <sstream>

editconnection::editconnection(int maxbuf) :
	maxdata(1024, maxbuf, 1024),
	timeout(1, 999, 1)
{
	title("Connection choices");
	close_icon(false);

	maxdata.label("Max data buffer size");
	maxdata.units("KB");
	pipelining.text("Pipeline requests to increase speed");
	timeout.label("Timeout");
	timeout.units("seconds");
	logging.text("Log debug information to syslog");
	cancel.text("Cancel");
	savebutton.text("Save");

	layout1.margin(16).ygap(16);
	layout1.add(pipelining);
	layout1.add(maxdata);
	layout1.add(timeout);
	layout1.add(logging);
	layout1.add(layout3);

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

	maxdata.value(mountinfo.maxdatabuffer ? mountinfo.maxdatabuffer : 4096);
	maxdata.snap();
	pipelining.selected(mountinfo.pipelining);
	timeout.value(mountinfo.timeout);
	logging.selected(mountinfo.logging);
}

void editconnection::open(const string& host, string& exportname, sunfish& app)
{
	load(host, exportname);
	// Open window near mouse position.
	os::pointer_info_get blk;
	os::Wimp_GetPointerInfo(blk);
	if (blk.p.x() + min_bbox().xsize() + 4 > app.bbox().xmax()) {
		blk.p.x(app.bbox().xmax() - (min_bbox().xsize() + 4));
	} else {
		blk.p -= point(64,0);
	}
	if (blk.p.y() - min_bbox().ysize() < 0) {
		blk.p.y(min_bbox().ysize());
	}

	app.add(*this,blk.p);

	set_caret_position(point(),-1,0);
}

void editconnection::save()
{
	mountchoices mountinfo;
	mountinfo.load(filename);

	mountinfo.maxdatabuffer = maxdata.value();
	mountinfo.pipelining = pipelining.selected();
	mountinfo.timeout = timeout.value();
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

void editconnection::handle_event(events::key_pressed& ev)
{
	if (ev.code() == 13) {
		// Return
		save();
		remove();
	} else if (ev.code() == 27) {
		// Escape
		remove();
	}
}

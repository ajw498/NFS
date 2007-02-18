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

editconnection::editconnection() :
	maxdata(1024, 8192, 1024)
{
	title("Connection choices");
	close_icon(false);

	maxdata.label("Max data buffer size");
	maxdata.units("KB");
	pipelining.text("Pipeline requests to increase speed");
	timeoutlabel.text("Timeout").xbaseline(xbaseline_right);
	timeoutlabel.xfit(false);
	timeoutunits.text("seconds");
	retrieslabel.text("Retries").xbaseline(xbaseline_right);
	retrieslabel.xfit(false);
	timeout.min_size(point(48,0));
	timeout.validation(timeout.validation()+";A0-9");
	retries.min_size(point(48,0));
	retries.validation(timeout.validation()+";A0-9");
	logging.text("Log debug information to syslog");
	cancel.text("Cancel");
	savebutton.text("Save");

	layout1.margin(16).ygap(16);
	layout1.add(pipelining);
	layout1.add(maxdata);
	layout1.add(layout2);
	layout1.add(logging);
	layout1.add(layout3);

	layout2.xgap(8).ygap(8).xbaseline(xbaseline_left);
	layout2.add(timeoutlabel, 0, 0);
	layout2.add(timeout, 1, 0);
	layout2.add(timeoutunits, 2, 0);
	layout2.add(retrieslabel, 0, 1);
	layout2.add(retries, 1, 1);
	layout2.xfit(false);

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
	pipelining.selected(mountinfo.pipelining);
	ostringstream time;
	time<<mountinfo.timeout;
	timeout.text(time.str());
	ostringstream tries;
	tries<<mountinfo.retries;
	retries.text(tries.str());
	logging.selected(mountinfo.logging);
}

void editconnection::open(const string& host, string& exportname, sunfish& app)
{
	load(host, exportname);
	// Open window near mouse position.
	os::pointer_info_get blk;
	os::Wimp_GetPointerInfo(blk);
	blk.p -= point(64,0);

	app.add(*this,blk.p);

	timeout.set_caret_position(point(),-1,timeout.text().length());
}

void editconnection::save()
{
	mountchoices mountinfo;
	mountinfo.load(filename);

	mountinfo.maxdatabuffer = maxdata.value();
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

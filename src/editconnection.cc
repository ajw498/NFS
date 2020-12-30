/*
	Edit connection related choices


	Copyright (C) 2006 Alex Waugh

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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

	maxdata.label("Data buffer size");
	maxdata.units("bytes");
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


void editconnection::load(const string& host, const string& ip, string& exportname)
{
	mountchoices mountinfo;
	if (host.length() > 0) {
		filename = mountinfo.genfilename(host, ip, exportname);
	}
	mountinfo.load(filename);

	maxdata.value(mountinfo.maxdatabuffer ? mountinfo.maxdatabuffer : 4096);
	maxdata.snap();
	pipelining.selected(mountinfo.pipelining);
	timeout.value(mountinfo.timeout);
	logging.selected(mountinfo.logging);
}

void editconnection::open(const string& host, const string& ip, string& exportname, sunfish& app)
{
	load(host, ip, exportname);
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
			load(none, none, none);
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

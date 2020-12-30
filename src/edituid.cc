/*
	Edit UID/permissions related choices


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


#include "sunfish-frontend.h"

#include "edituid.h"
#include "mountchoices.h"

#include <sstream>
#include <iomanip>

edituid::edituid()
{
	title("ID and permissions choices");
	close_icon(false);

	uidlabel.text("UID").xbaseline(xbaseline_right);
	uidlabel.xfit(false);
	gidslabel.text("GIDs").xbaseline(xbaseline_right);
	gidslabel.xfit(false);
	umasklabel.text("umask").xbaseline(xbaseline_right);
	umasklabel.xfit(false);
	unumasklabel.text("unumask").xbaseline(xbaseline_right);
	unumasklabel.xfit(false);
	umaskoctal.text("(octal)");
	unumaskoctal.text("(octal)");
	cancel.text("Cancel");
	savebutton.text("Save");

	uid.min_size(point(200,0));
	uid.text("", 10);
	gids.text("", 30);
	umask.text("", 3);
	unumask.text("", 3);
	uid.validation(uid.validation() + ";A0-9");
	gids.validation(gids.validation() + ";A0-9 ");
	umask.validation(umask.validation() + ";A0-7");
	unumask.validation(unumask.validation() + ";A0-7");

	layout1.margin(16).ygap(16);
	layout1.add(layout2);
	layout1.add(layout3);

	layout2.xgap(8).ygap(8);
	layout2.add(uidlabel, 0, 0);
	layout2.add(uid, 1, 0);
	layout2.add(gidslabel, 0, 1);
	layout2.add(gids, 1, 1);
	layout2.add(umasklabel, 0, 2);
	layout2.add(umask, 1, 2);
	layout2.add(umaskoctal, 2, 2);
	layout2.add(unumasklabel, 0, 3);
	layout2.add(unumask, 1, 3);
	layout2.add(unumaskoctal, 2, 3);

	layout3.xgap(16);
	layout3.add(cancel);
	layout3.add(savebutton);

	add(layout1);
}

void edituid::open(const string& host, const string& ip, string& exportname, sunfish& app)
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
	uid.set_caret_position(point(),-1,uid.text().length());
}

void edituid::load(const string& host, const string& ip, string& exportname)
{
	mountchoices mountinfo;
	if (host.length() > 0) {
		filename = mountinfo.genfilename(host, ip, exportname);
	}
	mountinfo.load(filename);

	ostringstream str;
	if (mountinfo.uidvalid) str<<mountinfo.uid;
	uid.text(str.str());
	str.str("");
	if (mountinfo.uidvalid) str<<mountinfo.gids;
	gids.text(str.str());
	str.str("");
	str << oct << setw(3) << setfill('0') << mountinfo.umask;
	umask.text(str.str());
	str.str("");
	str << oct << setw(3) << setfill('0') << mountinfo.unumask;
	unumask.text(str.str());
}

void edituid::save()
{
	mountchoices mountinfo;
	mountinfo.load(filename);

	mountinfo.uid = atoi(uid.text().c_str());
	mountinfo.uidvalid = true;
	mountinfo.gids = gids.text();
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
			load(none, none, none);
		}
	}
}

void edituid::handle_event(events::key_pressed& ev)
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

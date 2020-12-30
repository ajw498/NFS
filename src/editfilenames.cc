/*
	Edit filename and filetype related choices


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

#include "editfilenames.h"
#include "mountchoices.h"

#include <sstream>

using rtk::graphics::point;
using rtk::graphics::box;


editfilenames::editfilenames()
{
	title("Filename and filetype choices");
	close_icon(false);
	showalways.text("Always show hidden files").esg(1);
	showroot.text("Show hidden files except in root of mount").esg(1);
	shownever.text("Never show hidden files").esg(1);
	casesensitive.text("Case sensitive");
	encoding.label("Encoding");
	encoding.menutitle("Encoding");
	encoding.add("No conversion");
	encoding.add("UTF-8");
	encoding.add("ISO-8859-1");
	encoding.add("ISO-8859-2");
	encoding.add("ISO-8859-3");
	encoding.add("ISO-8859-4");
	encoding.add("ISO-8859-5");
	encoding.add("ISO-8859-6");
	encoding.add("ISO-8859-7");
	encoding.add("ISO-8859-8");
	encoding.add("ISO-IR-182");
	encoding.add("CP866");
	encoding.value("                    ");
	encoding.writable(true);
	defaultfiletypelabel.text("Default filetype");
	defaultfiletype.min_size(point(96,0));
	defaultfiletype.validation(defaultfiletype.validation()+";A0-9A-Fa-f");
	defaultfiletype.text("",3);
	extalways.text("Always add ,xyz extensions").esg(2);
	extneeded.text("Add ,xyz extensions only when needed").esg(2);
	extnever.text("Never add ,xyz extensions").esg(2);
	unixex.text("Set filetype to UnixEx for executable files");
	escapewin.text("Translate characters that are illegal on Windows");
	followsymlinks.text("Follow symbolic links");
	cancel.text("Cancel");
	savebutton.text("Save");
	filenames.text("Filenames");
	filenames.add(layout5);
	filetypes.text("Filetypes");
	filetypes.add(layout6);

	layout1.margin(16).ygap(16);
	layout1.add(filenames);
	layout1.add(filetypes);
	layout1.add(layout4);

	layout2.xgap(8).xfit(false);
	layout2.xbaseline(xbaseline_left);
	layout2.add(defaultfiletypelabel);
	layout2.add(defaultfiletype);

	layout4.xgap(16);
	layout4.add(cancel);
	layout4.add(savebutton);

	layout5.ygap(8);
	layout5.add(showalways);
	layout5.add(showroot);
	layout5.add(shownever);
	layout5.add(casesensitive);
	layout5.add(escapewin);
	layout5.add(followsymlinks);
	layout5.add(encoding);

	layout6.ygap(8);
	layout6.add(layout2);
	layout6.add(extalways);
	layout6.add(extneeded);
	layout6.add(extnever);
	layout6.add(unixex);

	add(layout1);
}

void editfilenames::open(const string& host, const string& ip, string& exportname, sunfish& app)
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
	defaultfiletype.set_caret_position(point(),-1,defaultfiletype.text().length());
}

void editfilenames::load(const string& host, const string& ip, string& exportname)
{
	mountchoices mountinfo;
	if (host.length() > 0) {
		filename = mountinfo.genfilename(host, ip, exportname);
	}
	mountinfo.load(filename);

	showalways.selected(mountinfo.showhidden == 1);
	showroot.selected(mountinfo.showhidden == 2);
	shownever.selected(mountinfo.showhidden == 0);
	casesensitive.selected(mountinfo.casesensitive);
	encoding.value(mountinfo.encoding);
	ostringstream ftype;
	ftype << hex << uppercase << mountinfo.defaultfiletype;
	defaultfiletype.text(ftype.str());
	extalways.selected(mountinfo.addext == 2);
	extneeded.selected(mountinfo.addext == 1);
	extnever.selected(mountinfo.addext == 0);
	unixex.selected(mountinfo.unixex);
	escapewin.selected(mountinfo.escapewin);
	followsymlinks.selected(mountinfo.followsymlinks > 0);
}

void editfilenames::save()
{
	mountchoices mountinfo;
	mountinfo.load(filename);

	if (showalways.selected()) {
		mountinfo.showhidden = 1;
	} else if (showroot.selected()) {
		mountinfo.showhidden = 2;
	} else if (shownever.selected()) {
		mountinfo.showhidden = 0;
	}
	mountinfo.casesensitive = casesensitive.selected();
	mountinfo.encoding = encoding.value();
	mountinfo.defaultfiletype = strtoul(defaultfiletype.text().c_str(), NULL, 16);
	if (extalways.selected()) {
		mountinfo.addext = 2;
	} else if (extneeded.selected()) {
		mountinfo.addext = 1;
	} else if (extnever.selected()) {
		mountinfo.addext = 0;
	}
	mountinfo.unixex = unixex.selected();
	mountinfo.escapewin = escapewin.selected();
	if (followsymlinks.selected()) {
		mountinfo.followsymlinks = 5;
	} else {
		mountinfo.followsymlinks = 0;
	}

	mountinfo.save(filename);
}

void editfilenames::handle_event(events::mouse_click& ev)
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

void editfilenames::handle_event(events::key_pressed& ev)
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


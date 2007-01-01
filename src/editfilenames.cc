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

#include "editfilenames.h"
#include "mountchoices.h"


using namespace std;
using namespace rtk;
using namespace rtk::desktop;
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
//	encoding.labeltext("Encoding");
	defaultfiletypelabel.text("Default filetype");
	defaultfiletype.min_size(point(96,0));
	defaultfiletype.validation(defaultfiletype.validation()+";A0-9A-Fa-f");
	defaultfiletype.text("",3);
	extalways.text("Always add ,xyz extensions").esg(2);
	extneeded.text("Add ,xyz extensions only when needed").esg(2);
	extnever.text("Never add ,xyz extensions").esg(2);
	unixex.text("Set filetype to UnixEx for executable files");
	followsymlinks.text("Follow symlinks");
	symlinklevels.min_size(point(64,0));
	symlinklevels.validation(symlinklevels.validation()+";A0-9");
	symlinklevels.text("",2);
	symlinklabel.text("levels");
	cancel.text("Cancel");
	savebutton.text("Save");
	filenames.text("Filenames");
	filenames.add(layout5);
	filetypes.text("Filetypes");
	filetypes.add(layout6);

	layout1.margin(16).ygap(8);
	layout1.add(filenames);
	layout1.add(filetypes);
	layout1.add(layout4);

	layout2.xgap(8).xfit(false);
	layout2.xbaseline(xbaseline_left);
	layout2.add(defaultfiletypelabel);
	layout2.add(defaultfiletype);

	layout3.xgap(8).xfit(false);
	layout3.xbaseline(xbaseline_left);
	layout3.add(followsymlinks);
	layout3.add(symlinklevels);
	layout3.add(symlinklabel);

	layout4.xgap(16);
	layout4.add(cancel);
	layout4.add(savebutton);

	layout5.ygap(8);
	layout5.add(showalways);
	layout5.add(showroot);
	layout5.add(shownever);
	layout5.add(casesensitive);
	layout5.add(layout3);

	layout6.ygap(8);
	layout6.add(layout2);
	layout6.add(extalways);
	layout6.add(extneeded);
	layout6.add(extnever);
	layout6.add(unixex);

	add(layout1);
}

#include <sstream>

void editfilenames::load(const string& host, string& exportname)
{
	mountchoices mountinfo;
	if (host.length() > 0) {
		filename = mountinfo.genfilename(host, exportname);
	}
	mountinfo.load(filename);

	showalways.selected(mountinfo.showhidden == 1);
	showroot.selected(mountinfo.showhidden == 0);
	shownever.selected(mountinfo.showhidden == 2);
	casesensitive.selected(mountinfo.casesensitive);
	ostringstream ftype;
	ftype << hex << uppercase << mountinfo.defaultfiletype;
	defaultfiletype.text(ftype.str());
	extalways.selected(mountinfo.addext == 2);
	extneeded.selected(mountinfo.addext == 1);
	extnever.selected(mountinfo.addext == 0);
	unixex.selected(mountinfo.unixex);
	followsymlinks.selected(mountinfo.followsymlinks > 0);
	ostringstream sym;
	sym << mountinfo.followsymlinks;
	symlinklevels.text(sym.str());
}

void editfilenames::save()
{
	mountchoices mountinfo;
	mountinfo.load(filename);

	if (showalways.selected()) {
		mountinfo.showhidden = 1;
	} else if (showroot.selected()) {
		mountinfo.showhidden = 0;
	} else if (shownever.selected()) {
		mountinfo.showhidden = 2;
	}
	mountinfo.casesensitive = casesensitive.selected();
	mountinfo.defaultfiletype = strtoul(defaultfiletype.text().c_str(), NULL, 16);
	if (extalways.selected()) {
		mountinfo.addext = 2;
	} else if (extneeded.selected()) {
		mountinfo.addext = 1;
	} else if (extnever.selected()) {
		mountinfo.addext = 0;
	}
	mountinfo.unixex = unixex.selected();
	if (followsymlinks.selected()) {
		mountinfo.followsymlinks = atoi(symlinklevels.text().c_str());
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
			load(none, none);
		}
	}
}

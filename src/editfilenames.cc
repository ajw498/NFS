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
	extalways.text("Always add ,xyz extensions").esg(2);
	extneeded.text("Add ,xyz extensions only when needed").esg(2);
	extnever.text("Never add ,xyz extensions").esg(2);
	unixex.text("Set filetype to UnixEx for executable files");
	followsymlinks.text("Follow symlinks");
	symlinklabel.text("levels");
	cancel.text("Cancel");
	savebutton.text("Save");

	layout1.margin(16).ygap(8);
	layout1.add(showalways);
	layout1.add(showroot);
	layout1.add(shownever);
	layout1.add(casesensitive);
	layout1.add(layout2);
	layout1.add(extalways);
	layout1.add(extneeded);
	layout1.add(extnever);
	layout1.add(unixex);
	layout1.add(layout3);
	layout1.add(layout4);

	layout2.xgap(8);
	layout2.add(defaultfiletypelabel);
	layout2.add(defaultfiletype);

	layout3.xgap(16);
	layout3.add(followsymlinks);
	layout3.add(symlinklevels);
	layout3.add(symlinklabel);

	layout4.xgap(16);
	layout4.add(cancel);
	layout4.add(savebutton);

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

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
	save.text("Save");

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
	layout4.add(save);

	add(layout1);
}

void editfilenames::load(string filename)
{
	mountchoices mountinfo;
	mountinfo.load(filename);
	showalways.selected(mountinfo.showhidden == 1);
	showroot.selected(mountinfo.showhidden == 0);
	shownever.selected(mountinfo.showhidden == 2);
	casesensitive.selected(mountinfo.casesensitive);
	//filetype;
	extalways.selected(mountinfo.addext == 2);
	extneeded.selected(mountinfo.addext == 1);
	extnever.selected(mountinfo.addext == 0);
	unixex.selected(mountinfo.unixex);
	followsymlinks.selected(mountinfo.followsymlinks > 0);
	//followsymlinks
}

void editfilenames::handle_event(events::mouse_click& ev)
{
//	syslogf("editfilenames::handle_event");
//	if (ev.buttons() == 2) {
//	syslogf("editfilenames::handle_event menu");
//	} else if (ev.target() == &save) {
//	syslogf("editfilenames::handle_event save");
//		mountdetails->uid = atoi(uid.text().c_str());
//		strcpy(mountdetails->gids, gid.text().c_str());
//		mountdetails->uidvalid = true;
//		mountdetails->save(host.host, exportname.c_str());
//		char filename[256];
//		mountdetails->genfilename(host.host, exportname.c_str(), filename, sizeof(filename));
//		string cmd = "Filer_OpenDir ";
//		cmd += filename;
//		os::Wimp_StartTask(cmd.c_str());
//		if (ev.buttons() == 4) remove();
//	} else if (ev.target() == &cancel) {
//	syslogf("editfilenames::handle_event cancel");
//		remove();
//	}
}

editfilenames::~editfilenames()
{
//	delete mountdetails;
}

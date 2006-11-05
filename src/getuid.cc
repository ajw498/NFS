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

#include <fstream>
#include <iostream>

#include "sunfish.h"
#include "sunfishdefs.h"

#include "getuid.h"


using namespace std;
using namespace rtk;
using namespace rtk::desktop;
using rtk::graphics::point;
using rtk::graphics::box;


getuid::getuid() :
	mountdetails(0)
{
	title("Enter uid thing");
	uidlabel.text("User id");
	uid.text("Wibble",10);
	gid.text("",4);
	gidlabel.text("Group id");
	explain.text("Explanation about uid and gids goes here");
	cancel.text("Cancel");
	set.text("Set");
	save.text("Save");
	layout1.margin(16).ygap(8);
	layout1.add(layout2);
	layout1.add(explain);
	layout1.add(layout3);
	layout2.ygap(8);
	layout2.add(uidlabel,0,0);
	layout2.add(gidlabel,0,1);
	layout2.add(uid,1,0);
	layout2.add(gid,1,1);
	layout3.xgap(16);
	layout3.add(cancel);
	layout3.add(set);
	layout3.add(save);
	add(layout1);
}

void getuid::setup(const hostinfo& info, string name, application& app)
{
	delete mountdetails;
	mountdetails = 0;
	host = info;
	exportname = name;
	mountdetails = new mountchoices;
	string filename = mountdetails->genfilename(host.host, exportname);
	mountdetails->load(filename);
	strcpy(mountdetails->server, host.host);
	strcpy(mountdetails->exportname, exportname.c_str());
	if (mountdetails->uidvalid) {
		string cmd = "Filer_OpenDir ";
		cmd += filename;
		os::Wimp_StartTask(cmd.c_str());
	} else {
		app.add(*this,point(640,512));
	}
}

#include "newfe.h"

void getuid::handle_event(events::mouse_click& ev)
{
	syslogf("getuid::handle_event");
	if (ev.buttons() == 2) {
	syslogf("getuid::handle_event menu");
	} else if (ev.target() == &save) {
	syslogf("getuid::handle_event save");
		mountdetails->uid = atoi(uid.text().c_str());
		strcpy(mountdetails->gids, gid.text().c_str());
		mountdetails->uidvalid = true;
		string filename = mountdetails->genfilename(host.host, exportname);
		mountdetails->save(filename);
		string cmd = "Filer_OpenDir ";
		cmd += filename;
		os::Wimp_StartTask(cmd.c_str());
		if (ev.buttons() == 4) remove();
	} else if (ev.target() == &cancel) {
	syslogf("getuid::handle_event cancel");
		remove();
	}
}

getuid::~getuid()
{
	delete mountdetails;
}

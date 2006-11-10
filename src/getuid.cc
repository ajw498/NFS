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


getuid::getuid()
{
	title("Enter uid thing");
	close_icon(false);

	uid.text("",30);
	gid.text("",30);
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
	mountchoices mountdetails;

	host = info;
	exportname = name;
	string filename = mountdetails.genfilename(host.host, exportname);
	mountdetails.load(filename);
	if (mountdetails.uidvalid ||
	    mountdetails.username[0] ||
	    (info.mount1udpport == 111) ||
	    (info.mount1tcpport == 111) ||
	    (info.mount3udpport == 111) ||
	    (info.mount3tcpport == 111)) {
		// Assume that any server with the mount protocol on port 111
		// is a RISC OS machine, and therefore doesn't need a valid
		// uid/gid. If this assumption is wrong, then the user can
		// always set the uid/gid from the export choices.
		string cmd = "Filer_OpenDir ";
		cmd += filename;
		os::Wimp_StartTask(cmd.c_str());
	} else if (info.pcnfsdudpport || info.pcnfsdtcpport) {
		uidlabel.text("Username");
		gidlabel.text("Password");
		uid.validation("Kta;Pptr_write");
		gid.validation("D*;Kta;Pptr_write");
		explain.text("");
		app.add(*this,point(640,512));
	} else {
		uidlabel.text("User id");
		gidlabel.text("Group id");
		uid.validation("A0-9;Kta;Pptr_write");
		gid.validation("A0-9 ;Kta;Pptr_write");
		explain.text("Some systems may not require specific id values, in which case leave them blank");
		app.add(*this,point(640,512));
	}
	uid.text("");
	gid.text("");
}

#include "newfe.h"

void getuid::handle_event(events::mouse_click& ev)
{
	if (ev.buttons() == 2) {
	} else if (ev.target() == &save) {
		mountchoices mountdetails;
		string filename = mountdetails.genfilename(host.host, exportname);
		mountdetails.load(filename);
		strcpy(mountdetails.server, host.host);
		strcpy(mountdetails.exportname, exportname.c_str());

		if (host.pcnfsdudpport || host.pcnfsdtcpport) {
			char *err;
			bool tcp = host.pcnfsdudpport == 0;
			int port = tcp ? host.pcnfsdtcpport : host.pcnfsdudpport;

			err = browse_lookuppassword(host.host, port, tcp, uid.text().c_str(), gid.text().c_str(), &mountdetails.uid, &mountdetails.umask, mountdetails.gids, sizeof(mountdetails.gids));
			if (err) throw err;
		} else {
			mountdetails.uid = atoi(uid.text().c_str());
			strcpy(mountdetails.gids, gid.text().c_str());
		}
		mountdetails.uidvalid = true;
		mountdetails.save(filename);

		string cmd = "Filer_OpenDir ";
		cmd += filename;
		os::Wimp_StartTask(cmd.c_str());

		if (ev.buttons() == 4) remove();
	} else if (ev.target() == &cancel) {
		uid.text("");
		gid.text("");
		if (ev.buttons() == 4) remove();
	}
}

getuid::~getuid()
{
}

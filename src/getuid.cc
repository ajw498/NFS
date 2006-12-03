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
#include "sunfish-frontend.h"



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

void sunfish_mount(const char *discname, const char *specialfield, const char *config);

void getuid::setup(const hostinfo& info, string name, bool tcp, int version, sunfish& app)
{
	mountchoices mountdetails;

	// Open window near mouse position.
	os::pointer_info_get blk;
	os::Wimp_GetPointerInfo(blk);
	blk.p -= point(64,0);

	host = info;
	exportname = name;
	usetcp = tcp;
	nfsversion = version;
	string filename = mountdetails.genfilename(host.host, exportname);
	mountdetails.load(filename);
	strcpy(mountdetails.server, host.host);
	strcpy(mountdetails.exportname, exportname.c_str());
	mountdetails.tcp = tcp;
	mountdetails.nfs3 = version == 3;
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
		create_mount(mountdetails, app);
	} else if (info.pcnfsdudpport || info.pcnfsdtcpport) {
		uidlabel.text("Username");
		gidlabel.text("Password");
		uid.validation("Kta;Pptr_write");
		gid.validation("D*;Kta;Pptr_write");
		explain.text("");
		app.add(*this,blk.p);
	} else {
		uidlabel.text("User id");
		gidlabel.text("Group id");
		uid.validation("A0-9;Kta;Pptr_write");
		gid.validation("A0-9 ;Kta;Pptr_write");
		explain.text("Some systems may not require specific id values, in which case leave them blank");
		app.add(*this,blk.p);
	}
	uid.text("");
	gid.text("");
}

void getuid::handle_event(events::mouse_click& ev)
{
	if (ev.buttons() == 2) {
	} else if ((ev.target() == &save) || (ev.target() == &set)) {
		mountchoices mountdetails;
		string filename = mountdetails.genfilename(host.host, exportname);
		mountdetails.load(filename);
		strcpy(mountdetails.server, host.host);
		strcpy(mountdetails.exportname, exportname.c_str());
		mountdetails.tcp = usetcp;
		mountdetails.nfs3 = nfsversion == 3;

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
		if (ev.target() == &save) mountdetails.save(filename);
		create_mount(mountdetails, *static_cast<sunfish *>(parent_application()));

		if (ev.buttons() == 4) remove();
	} else if (ev.target() == &cancel) {
		uid.text("");
		gid.text("");
		if (ev.buttons() == 4) remove();
	}
}

void getuid::create_mount(mountchoices& mountdetails, sunfish& app)
{
	string cmd = "Filer_OpenDir Sunfish";
	string mountname;
	string sf;
	if (mountdetails.nicename[0]) {
		sunfish_mount(mountdetails.nicename, NULL, mountdetails.stringsave().c_str());
		cmd += "::";
		cmd += mountdetails.nicename;
		mountname = mountdetails.nicename;
	} else {
		sunfish_mount(exportname.c_str(), host.host, mountdetails.stringsave().c_str());
		cmd += "#";
		cmd += host.host;
		cmd += "::";
		cmd += exportname;
		mountname = mountdetails.exportname;
		sf = host.host;
	}
	cmd += ".$";
	os::Wimp_StartTask(cmd.c_str());

	app.add_mounticon(mountname, sf);
}

getuid::~getuid()
{
}


#include <swis.h>

void sunfish_mount(const char *discname, const char *specialfield, const char *config)
{
	_kernel_oserror *err = _swix(Sunfish_Mount, _INR(0,2), discname, specialfield, config);
	if (err) throw err->errmess;
}

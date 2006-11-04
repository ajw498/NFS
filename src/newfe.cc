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

#include "browse.h"

#include <stdarg.h>

void syslogf(char *fmt, ...);

using namespace std;
using namespace rtk;
using namespace rtk::desktop;
using rtk::graphics::point;
using rtk::graphics::box;

class getuid:
	public window
{
public:
	getuid();
	void show(const hostinfo& info, string name);
private:
	hostinfo host;
	string exportname;
	icon uidlabel;
	icon gidlabel;
	writable_field uid;
	writable_field gid;
	icon explain;
	action_button cancel;
	action_button set;
	default_button save;
	column_layout layout1;
	grid_layout layout2;
	row_layout layout3;
};

getuid::getuid()
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

void getuid::show(const hostinfo& info, string name)
{
	host = info;
	exportname = name;
}


class hostbrowser:
	public rtk::desktop::filer_window,
	public rtk::events::null_reason::handler
{
public:
	hostbrowser();
	~hostbrowser();
	void broadcast();
	void handle_event(rtk::events::close_window& ev) { parent_application()->terminate(); }
	void handle_event(rtk::events::null_reason& ev);
	void open_menu(const std::string& item, bool selection, rtk::events::mouse_click& ev);
//	void drag_ended(bool adjust, rtk::events::user_drag_box& ev) {}
	void doubleclick(const std::string& item);
private:
	time_t broadcasttime;
	int broadcasttype;
	map<string, hostinfo> hostinfos;

	rtk::desktop::menu menu;
	rtk::desktop::menu_item item0;
	rtk::desktop::menu_item item1;

};


class sunfish:
	public rtk::desktop::application
{
public:
	sunfish();
	getuid ggetuid;
private:
	hostbrowser _window;
	rtk::desktop::ibar_icon ibicon;
	rtk::desktop::menu ibmenu;
	rtk::desktop::menu_item ibinfo;
	rtk::desktop::menu_item ibquit;
	rtk::desktop::prog_info_dbox proginfo;
};

sunfish app;

class exportbrowser:
	public rtk::desktop::filer_window
{
public:
	exportbrowser(hostinfo host);
	~exportbrowser();
//	void handle_event(rtk::events::close_window& ev) { parent_application()->terminate(); }
//	void open_menu(const std::string& item, bool selection, rtk::events::mouse_click& ev);
//	void drag_ended(bool adjust, rtk::events::user_drag_box& ev) {}
	void doubleclick(const std::string& item);
private:
	hostinfo info;
	rtk::desktop::menu menu;
	rtk::desktop::menu_item item0;
	rtk::desktop::menu_item item1;

};

exportbrowser::exportbrowser(hostinfo host) :
	info(host)
{
	char *err;

	title(info.host);

	char *exports[256];
	err = browse_getexports(info.host, info.mount1udpport, 1, 0, exports);
	if (err) throw err;
	for (int j = 0; exports[j]; j++) {
		add_icon(exports[j], "file_1b6");
	}
}

void exportbrowser::doubleclick(const std::string& item)
{
	string filename = "Sunfish:mounts.auto."+item;
//	ofstream mfile(filename.c_str());
//	if (!mfile) throw "Cannot open file";
//	mfile<<"Protocol: NFS3\nServer: "/*<<info.host*/<<"\nExport: "<<item;
//	mfile<<"\nTransport: UDP\n";
//	mfile<<"Foo";
//	mfile.close();

/*	os::OS_File8("Sunfish:mounts.auto");

	FILE *mfile = fopen(filename.c_str(), "w");
	if (!mfile) throw "Cannot open file";
	fprintf(mfile, "Protocol: NFS3\nServer: %s\nExport: %s\nTransport: UDP\n", info.host, item.c_str());
	fclose(mfile);

	os::OS_File18(filename.c_str(), SUNFISH_FILETYPE);

	string cmd = "Filer_OpenDir "+filename;
	os::Wimp_StartTask(cmd.c_str(), NULL); */
	app.add(app.ggetuid,point(640,512));
}

exportbrowser::~exportbrowser()
{

}


void hostbrowser::doubleclick(const std::string& item)
{
	exportbrowser *eb = new exportbrowser(hostinfos[item]);
	// Find centre of desktop.
	rtk::graphics::box dbbox(bbox());
	rtk::graphics::point dcentre((dbbox.xmin()+dbbox.xmax())/2,
		(dbbox.ymin()+dbbox.ymax())/2);

	// Find centre of window.
	rtk::graphics::box cbbox(eb->bbox());
	rtk::graphics::point ccentre((cbbox.xmin()+cbbox.xmax())/2,
		(cbbox.ymin()+cbbox.ymax())/2);

	// Open window at centre of desktop.

	parent_application()->add(*eb, dcentre-ccentre);
}

hostbrowser::hostbrowser()
{
	title("NFS servers");
	min_x_size(300);
}

void hostbrowser::broadcast()
{
	broadcasttime = clock();
	broadcasttype = 0;

		if (rtk::desktop::application* app=parent_application()) {
	app->register_null(*this);
	}
}

void hostbrowser::handle_event(rtk::events::null_reason& ev)
{
	char *err;

	if (clock() > broadcasttime + 100) {
		parent_application()->deregister_null(*this);
		err = browse_gethost(NULL, 2);
		if (err) throw err;
	} else {
		hostinfo info;
		err = browse_gethost(&info, broadcasttype);
		broadcasttype = 1;
		if (err) throw err;

		if (info.valid) {
			map<string,hostinfo>::iterator i = hostinfos.find(info.host);
			if (i != hostinfos.end()) {
				//remove;
			} else {
				hostinfos[info.host] = info;
				add_icon(info.host, "fileserver");
			}
		}
	}
}

hostbrowser::~hostbrowser()
{
}

void hostbrowser::open_menu(const std::string& item, bool selection, rtk::events::mouse_click& ev)
{
/*When menu closed, deselect temp selection. claim menusdeleted event from window? */
	menu.title("Filer");
	item0.text(selection ? "Selection" : "File '"+item+"'");
	item1.text("Foo...");
	menu.add(item0,0);
	menu.add(item1,1);
	menu.show(ev);
}



sunfish::sunfish():
	application("Sunfish newfe")
{

	// Find centre of desktop.
	rtk::graphics::box dbbox(bbox());
	rtk::graphics::point dcentre((dbbox.xmin()+dbbox.xmax())/2,
		(dbbox.ymin()+dbbox.ymax())/2);

	// Find centre of window.
	rtk::graphics::box cbbox(_window.bbox());
	rtk::graphics::point ccentre((cbbox.xmin()+cbbox.xmax())/2,
		(cbbox.ymin()+cbbox.ymax())/2);

	// Open window at centre of desktop.
	add(_window,dcentre-ccentre);
	_window.broadcast();

	proginfo.add("Name","Sunfish");
	proginfo.add("Purpose","Mount NFS servers");
	proginfo.add("Author","© Alex Waugh, 2003-2006");
	proginfo.add("Version",Module_VersionString " (" Module_Date ")");
	ibinfo.text("Info");
	ibinfo.attach_dbox(proginfo);
	ibquit.text("Quit");
	ibmenu.title("Sunfish");
	ibmenu.add(ibinfo);
	ibmenu.add(ibquit);

	ibicon.text("Sunfish").hcentre(true);
	ibicon.text_and_sprite(true).validation("S!sunfish");
	ibicon.attach_menu(ibmenu);
	add(ibicon);
}


int main(void)
{
	app.run();
	return 0;
}

#include <swis.h>

void syslogf(char *fmt, ...)
{
	static char syslogbuf[1024];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(syslogbuf, sizeof(syslogbuf), fmt, ap);
	va_end(ap);

	/* Ignore any errors, as there's not much we can do with them */
	_swix(0x4c880, _INR(0,2), "newfe", syslogbuf, 5);
}

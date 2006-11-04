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
#include "hostbrowser.h"
#include "exportbrowser.h"

using namespace std;

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


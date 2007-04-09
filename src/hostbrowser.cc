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
#include "sunfish-frontend.h"


using namespace std;
using namespace rtk::graphics;


void hostbrowser::openexportbrowser(hostinfo *info, bool tcp, int version)
{
	int port = 0;
	int vers = 0;

	if (tcp  && (version >= 3) && (port == 0)) {
		port = info->mount3tcpport;
		vers = 3;
	}
	if (tcp  && (version >= 2) && (port == 0)) {
		port = info->mount1tcpport;
		vers = 2;
	}
	if (tcp  && (port == 0)) tcp = false;
	if (!tcp && (version >= 3) && (port == 0)) {
		port = info->mount3udpport;
		vers = 3;
	}
	if (!tcp && (version >= 2) && (port == 0)) {
		port = info->mount1udpport;
		vers = 2;
	}

	if (port == 0) throw "No suitable mount service found on remote server";

	sunfish& app = *static_cast<sunfish *>(parent_application());

	exportbrowser *eb = new exportbrowser(*info);
	eb->refresh(port, tcp, vers);
	eb->open(app);

	app.exportbrowsers.push_back(eb);
}

void hostbrowser::doubleclick(const std::string& item, rtk::events::mouse_click& ev)
{
	openexportbrowser(&(hostinfos[item]), true, 4);
}

hostbrowser::hostbrowser() :
	searchwin(*this)
{
	title("NFS servers");
}

void hostbrowser::broadcast()
{
	broadcasttime = clock();
	broadcasttype = BROADCAST;

	if (rtk::desktop::application* app=parent_application()) {
		app->register_null(*this);
	}

	remove_all_icons();
	hostinfos.erase(hostinfos.begin(),hostinfos.end());

	for (vector<string>::iterator i = extrahosts.begin(); i != extrahosts.end(); ++i) {
		hostinfo info;
		char *err = browse_gethost(&info, HOST, i->c_str());
		if (err) {
			extrahosts.erase(i);
			throw err;
		}

		if (info.valid) {
			hostinfos[info.host] = info;
			add_icon(info.host, "fileserver");
		}
	}
	broadcasttime = clock();
}

void hostbrowser::open(sunfish& app)
{
	point pos;

	if (parent_application()) {
		// We are already open, so find the current position
		pos = origin();
	} else {
		// Find centre of desktop.
		box dbbox(app.bbox());
		point dcentre((dbbox.xmin()+dbbox.xmax())/2, (dbbox.ymin()+dbbox.ymax())/2);

		pos = dcentre;
	}
	app.add(*this,pos);
	smallicons(app.smallicons());
	broadcast();
}

void hostbrowser::search(string host)
{
	extrahosts.push_back(host);
	broadcast();
}

void hostbrowser::handle_event(rtk::events::null_reason& ev)
{
	char *err;

	if ((clock() > broadcasttime + 100) && (broadcasttype == LISTEN)) {
		parent_application()->deregister_null(*this);
		err = browse_gethost(NULL, CLOSE, NULL);
		if (err) throw err;
	} else {
		hostinfo info;
		err = browse_gethost(&info, broadcasttype, NULL);
		broadcasttype = LISTEN;
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
	menuinfo = &(hostinfos[item]);

	menu.title("Server");
	menu.add(display,0);
	menu.add(browseitem,1);
	menu.add(searchfor,2);
	menu.add(clear,3);
	menu.add(refresh,4);
	display.text("Display");
	display.attach_submenu(displaymenu);
	browseitem.text(selection ? "Selection" : "Browse '"+item+"'");
	browseitem.enabled(!selection && item.compare("") != 0);
	browseitem.attach_submenu(transport);
	searchfor.text("Search for host");
	searchfor.attach_dbox(searchwin);
	clear.text("Clear selection");
	clear.enabled(item.compare("") != 0);
	refresh.text("Refresh");

	displaymenu.title("Display");
	displaymenu.add(largeiconsitem, 0);
	displaymenu.add(smalliconsitem, 1);

	sunfish& app = *static_cast<sunfish *>(parent_application());
	largeiconsitem.text("Large icons");
	largeiconsitem.tick(!app.smallicons());
	smalliconsitem.text("Small icons");
	smalliconsitem.tick(app.smallicons());

	transport.title("Transport");
	transport.add(udp, 0);
	transport.add(tcp, 1);
	udp.text("UDP");
	udp.attach_submenu(udpprotocol);
	udp.enabled(menuinfo->mount1udpport || menuinfo->mount3udpport);
	tcp.text("TCP");
	tcp.attach_submenu(tcpprotocol);
	tcp.enabled(menuinfo->mount1tcpport || menuinfo->mount3tcpport);

	udpprotocol.title("Protocol");
	udpprotocol.add(udpnfs2, 0);
	udpprotocol.add(udpnfs3, 1);
	udpprotocol.add(udpnfs4, 2);
	udpnfs2.text("NFSv2");
	udpnfs2.enabled(menuinfo->mount1udpport);
	udpnfs3.text("NFSv3");
	udpnfs3.enabled(menuinfo->mount3udpport);
	udpnfs4.text("NFSv4");
	udpnfs4.enabled(false);

	tcpprotocol.title("Protocol");
	tcpprotocol.add(tcpnfs2, 0);
	tcpprotocol.add(tcpnfs3, 1);
	tcpprotocol.add(tcpnfs4, 2);
	tcpnfs2.text("NFSv2");
	tcpnfs2.enabled(menuinfo->mount1tcpport);
	tcpnfs3.text("NFSv3");
	tcpnfs3.enabled(menuinfo->mount3tcpport);
	tcpnfs4.text("NFSv4");
	tcpnfs4.enabled(false);


	menu.show(ev);
}

void hostbrowser::handle_event(rtk::events::menu_selection& ev)
{
	sunfish& app = *static_cast<sunfish *>(parent_application());

	if (ev.target() == &clear) {
		for (int i = icons.size() - 1; i >= 0; i--) icons[i]->selected(false);
	} else if (ev.target() == &refresh) {
		broadcast();
	} else if (ev.target() == &largeiconsitem) {
		largeiconsitem.tick(true);
		smalliconsitem.tick(false);
		smallicons(false);
		app.smallicons(false);
	} else if (ev.target() == &smalliconsitem) {
		largeiconsitem.tick(false);
		smalliconsitem.tick(true);
		smallicons(true);
		app.smallicons(true);
	} else if (ev.target() == &browseitem) {
		openexportbrowser(menuinfo, true, 4);
	} else if (ev.target() == &udp) {
		openexportbrowser(menuinfo, false, 4);
	} else if (ev.target() == &tcp) {
		openexportbrowser(menuinfo, true, 4);
	} else if (ev.target() == &udpnfs2) {
		openexportbrowser(menuinfo, false, 2);
	} else if (ev.target() == &udpnfs3) {
		openexportbrowser(menuinfo, false, 3);
	} else if (ev.target() == &udpnfs4) {
		openexportbrowser(menuinfo, false, 4);
	} else if (ev.target() == &tcpnfs2) {
		openexportbrowser(menuinfo, true, 2);
	} else if (ev.target() == &tcpnfs3) {
		openexportbrowser(menuinfo, true, 3);
	} else if (ev.target() == &tcpnfs4) {
		openexportbrowser(menuinfo, true, 4);
	}
}


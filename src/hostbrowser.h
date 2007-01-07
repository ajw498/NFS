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

#ifndef HOSTBROWSER_H
#define HOSTBROWSER_H

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

#include <map>
#include <string>

#include "sunfish.h"
#include "sunfishdefs.h"

#include "browse.h"
#include "searchhost.h"

class sunfish;

class hostbrowser:
	public rtk::desktop::filer_window,
	public rtk::events::null_reason::handler,
	public rtk::events::menu_selection::handler
{
public:
	hostbrowser();
	~hostbrowser();
	void broadcast();
	void handle_event(rtk::events::null_reason& ev);
	void handle_event(rtk::events::menu_selection& ev);
	void open_menu(const std::string& item, bool selection, rtk::events::mouse_click& ev);
//	void drag_ended(bool adjust, rtk::events::user_drag_box& ev) {}
	void doubleclick(const std::string& item, rtk::events::mouse_click& ev);
	void search(std::string host);
	void open(sunfish& app);
private:
	void openexportbrowser(hostinfo *info, bool tcp, int version);
	hostinfo *menuinfo;
	time_t broadcasttime;
	enum broadcast_type broadcasttype;
	std::map<std::string, hostinfo> hostinfos;
	std::vector<std::string> extrahosts;
	searchhost searchwin;

	rtk::desktop::menu menu;
	rtk::desktop::menu_item display;
	rtk::desktop::menu_item browseitem;
	rtk::desktop::menu_item searchfor;
	rtk::desktop::menu_item clear;
	rtk::desktop::menu_item refresh;

	rtk::desktop::menu transport;
	rtk::desktop::menu_item udp;
	rtk::desktop::menu_item tcp;

	rtk::desktop::menu udpprotocol;
	rtk::desktop::menu_item udpnfs2;
	rtk::desktop::menu_item udpnfs3;
	rtk::desktop::menu_item udpnfs4;

	rtk::desktop::menu tcpprotocol;
	rtk::desktop::menu_item tcpnfs2;
	rtk::desktop::menu_item tcpnfs3;
	rtk::desktop::menu_item tcpnfs4;

};

#endif


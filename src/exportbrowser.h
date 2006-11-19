/*
	$Id$

	Browser for exports provided by a host


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

#ifndef EXPORTBROWSER_H
#define EXPORTBROWSER_H

#include "rtk/desktop/menu_item.h"
#include "rtk/desktop/menu.h"
#include "rtk/desktop/filer_window.h"
#include "rtk/events/menu_selection.h"
#include "rtk/events/save_to_app.h"
#include "rtk/transfer/save.h"


#include "browse.h"
#include "editfilenames.h"
#include "editconnection.h"
#include "edituid.h"
#include "mountchoices.h"


class exportsave:
	public rtk::transfer::save
{
public:
	exportsave();
protected:
	void start() { done = false; }
	void get_block(const void** data,size_type* count);
	void finish() {}
	size_type estsize() { return 1024; }
private:
	mountchoices mount;
	bool done;
};


class exportbrowser:
	public rtk::desktop::filer_window,
	public rtk::events::menu_selection::handler
{
public:
	exportbrowser(hostinfo host);
	~exportbrowser();
	void refresh(int port, bool tcp, int version);
	void open_menu(const std::string& item, bool selection, rtk::events::mouse_click& ev);
	void drag_ended(bool adjust, rtk::events::user_drag_box& ev);
	void doubleclick(const std::string& item, rtk::events::mouse_click& ev);
	void handle_event(rtk::events::menu_selection& ev);
private:
	exportsave saveop;

	hostinfo info;
	bool usetcp;
	int nfsversion;
	int useport;

	std::string menuitem;

	rtk::desktop::menu menu;
	rtk::desktop::menu_item display;
	rtk::desktop::menu_item edititem;
	rtk::desktop::menu_item clear;
	rtk::desktop::menu_item refreshwin;

	rtk::desktop::menu displaymenu;
	rtk::desktop::menu_item largeiconsitem;
	rtk::desktop::menu_item smalliconsitem;

	rtk::desktop::menu edit;
	rtk::desktop::menu_item namemount;
	rtk::desktop::menu_item filenames;
	rtk::desktop::menu_item connection;
	rtk::desktop::menu_item uids;

	editfilenames filenameswin;
	editconnection connectionwin;
	edituid uidswin;
};

#endif

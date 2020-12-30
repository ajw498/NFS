/*
	Frontend for browsing hosts


	Copyright (C) 2006 Alex Waugh

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef HOSTBROWSER_H
#define HOSTBROWSER_H

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
#include "filer_window.h"

#include <map>
#include <string>

#include "sunfish.h"
#include "sunfishdefs.h"

#include "browse.h"
#include "searchhost.h"

class sunfish;

class hostbrowser:
	public filer_window,
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
	void doubleclick(const std::string& item, rtk::events::mouse_click& ev);
	void search(std::string host);
	void open(sunfish& app);
private:
	void openexportbrowser(hostinfo *info, bool udp, bool tcp, int version);
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

	rtk::desktop::menu displaymenu;
	rtk::desktop::menu_item largeiconsitem;
	rtk::desktop::menu_item smalliconsitem;

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


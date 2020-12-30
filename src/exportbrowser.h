/*
	Browser for exports provided by a host


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

#ifndef EXPORTBROWSER_H
#define EXPORTBROWSER_H

#include "rtk/desktop/menu_item.h"
#include "rtk/desktop/menu.h"
#include "rtk/events/menu_selection.h"
#include "rtk/events/save_to_app.h"
#include "rtk/transfer/save.h"
#include "filer_window.h"


#include "browse.h"
#include "editfilenames.h"
#include "editconnection.h"
#include "edituid.h"
#include "editname.h"
#include "mountchoices.h"


class exportsave:
	public rtk::transfer::save
{
public:
	exportsave();
	void load(const std::string& host, const std::string& ip, const std::string& exportname, bool tcp, int version);
protected:
	void start() { done = false; }
	void get_block(const void** data,size_type* count);
	void finish() {}
	size_type estsize() { return 1024; }
private:
	mountchoices *mount;
	bool done;
};


class exportbrowser:
	public filer_window,
	public rtk::events::menu_selection::handler,
	public rtk::events::save_to_app::handler
{
public:
	exportbrowser(hostinfo host, int port, bool tcp, int version);
	~exportbrowser();
	void refresh();
	void open(sunfish& app);
	void open_menu(const std::string& item, bool selection, rtk::events::mouse_click& ev);
	void doubleclick(const std::string& item, rtk::events::mouse_click& ev);
	void handle_event(rtk::events::menu_selection& ev);
	void handle_event(rtk::events::save_to_app& ev);
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
	editname namewin;
};

#endif


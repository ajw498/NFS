/*
	Frontend for browsing and creating mounts


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

class ibicon:
	public rtk::desktop::ibar_icon,
	public rtk::events::menu_selection::handler,
	public rtk::events::datasave::handler,
	public rtk::events::dataload::handler
{
public:
	ibicon(const std::string& icontext, const std::string& special, bool usetcp, int version);
	void handle_event(rtk::events::menu_selection& ev);
	void handle_event(rtk::events::mouse_click& ev);
	void handle_event(rtk::events::datasave& ev);
	void handle_event(rtk::events::dataload& ev);
	std::string filename();
	void opendir();
	void closedir();
	void mount(const char *config);
	void dismount();
	rtk::desktop::menu_item ibdismount;
	rtk::desktop::menu_item ibsave;
	std::string specialfield;
	bool tcp;
	int nfsversion;
private:
	rtk::desktop::menu ibmenu;
	rtk::desktop::menu_item ibinfo;
	rtk::desktop::menu_item ibhelp;
	rtk::desktop::menu_item ibbrowse;
	rtk::desktop::menu_item ibfree;
	rtk::desktop::menu_item ibquit;
	rtk::desktop::prog_info_dbox proginfo;
};


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

#include "getuid.h"
#include "hostbrowser.h"
#include "exportbrowser.h"
#include "ibicon.h"
#include "mountchoices.h"

#include <stdarg.h>

extern "C" {
void syslogf(char *, int,char *fmt, ...);
}

class sunfish:
	public rtk::desktop::application,
	public rtk::events::menu_selection::handler,
	public rtk::events::close_window::handler
{
public:
	sunfish();
	void handle_event(rtk::events::menu_selection& ev);
	void handle_event(rtk::events::close_window& ev);
	ibicon *add_mounticon(const std::string &name, const std::string &specialfield, bool usetcp, int nfsversion, bool& found);
	void remove_mounticon(ibicon *icon);
	void getmounts();
	getuid ggetuid;
	hostbrowser browserwin;
	ibicon ibaricon;
	std::vector<ibicon *> ibaricons;
	std::vector<exportbrowser *> exportbrowsers;
	void smallicons(bool small);
	bool smallicons() { return usesmallicons; }
	aliases hostaliases;
private:
	bool usesmallicons;
};


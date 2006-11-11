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

#include "getuid.h"
#include "hostbrowser.h"
#include "exportbrowser.h"
#include "ibicon.h"

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
	void add_mounticon(const std::string &name);
	getuid ggetuid;
	hostbrowser _window;
	ibicon ibaricon;
	std::vector<ibicon *> ibaricons;
	std::vector<exportbrowser *> exportbrowsers;
private:
};


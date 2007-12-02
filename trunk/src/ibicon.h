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


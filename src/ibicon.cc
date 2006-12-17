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


#include "sunfish-frontend.h"


using namespace std;
using namespace rtk;
using namespace rtk::desktop;
using rtk::graphics::point;
using rtk::graphics::box;


ibicon::ibicon(const std::string& icontext, const std::string& special):
	specialfield(special)
{
	proginfo.add("Name", "Sunfish");
	proginfo.add("Purpose", "Mount NFS servers");
	proginfo.add("Author", "© Alex Waugh, 2003-2006");
	proginfo.add("Version", Module_VersionString " (" Module_Date ")");
	ibinfo.text("Info");
	ibinfo.attach_dbox(proginfo);
	ibhelp.text("Help...");
	ibbrowse.text("Browse..");
	ibsave.text("Save mounts");
	ibdismount.text("Dismount");
	ibfree.text("Free...");
	ibquit.text("Quit");
	ibmenu.title("Sunfish");
	ibmenu.add(ibinfo);
	ibmenu.add(ibhelp);
	ibmenu.add(ibbrowse);
	ibmenu.add(ibsave);
	ibmenu.add(ibdismount);
	ibmenu.add(ibfree);
	ibmenu.add(ibquit);

	if (icontext == "") {
		ibdismount.enabled(false);
		ibfree.enabled(false);
		sprite_name("!Sunfish").hcentre(true);
	} else {
		text(icontext).hcentre(true);
		text_and_sprite(true).validation("Sfile_1b6");
	}
	attach_menu(ibmenu);
	iconbar_position(-5).iconbar_priority(0x10000000);
}

void ibicon::opendir()
{
	string cmd = "Filer_OpenDir Sunfish";

	if (specialfield.length() > 0) {
		cmd += "#";
		cmd += specialfield;
	}
	cmd += "::";
	cmd += text();
	cmd += ".$";
	os::Wimp_StartTask(cmd.c_str());
}

void ibicon::closedir()
{
	string cmd = "Filer_CloseDir Sunfish";

	if (specialfield.length() > 0) {
		cmd += "#";
		cmd += specialfield;
	}
	cmd += "::";
	cmd += text();
	cmd += ".$";
	os::Wimp_StartTask(cmd.c_str());
}

void ibicon::handle_event(rtk::events::menu_selection& ev)
{
	sunfish *app = static_cast<sunfish *>(parent_application());

	if (ev.target() == &ibhelp) {
		os::Wimp_StartTask("Filer_Run <Sunfish$Dir>.!Help");
	} else if (ev.target() == &ibbrowse) {
		// Find centre of desktop.
		rtk::graphics::box dbbox(app->bbox());
		rtk::graphics::point dcentre((dbbox.xmin()+dbbox.xmax())/2,
			(dbbox.ymin()+dbbox.ymax())/2);

		// Find centre of window.
		rtk::graphics::box cbbox(app->_window.bbox());
		rtk::graphics::point ccentre((cbbox.xmin()+cbbox.xmax())/2,
			(cbbox.ymin()+cbbox.ymax())/2);

		// Open window at centre of desktop.
		app->add(app->_window,dcentre-ccentre);
		app->_window.broadcast();
	} else if (ev.target() == &ibdismount) {
		closedir();
		dismount();
	} else if (ev.target() == &ibquit) {
		app->terminate();
	}
}

void ibicon::handle_event(rtk::events::mouse_click& ev)
{
	sunfish *app = static_cast<sunfish *>(parent_application());

	if (ev.target() == this) {
		if (ev.buttons() == 2) {
			ibmenu.show(ev);
		} else if ((ev.buttons() == 4) && text_and_sprite()) {
			opendir();
		} else {
			// Find centre of desktop.
			rtk::graphics::box dbbox(app->bbox());
			rtk::graphics::point dcentre((dbbox.xmin()+dbbox.xmax())/2,
				(dbbox.ymin()+dbbox.ymax())/2);

			// Find centre of window.
			rtk::graphics::box cbbox(app->_window.bbox());
			rtk::graphics::point ccentre((cbbox.xmin()+cbbox.xmax())/2,
				(cbbox.ymin()+cbbox.ymax())/2);

			// Open window at centre of desktop.
			app->add(app->_window,dcentre-ccentre);
			app->_window.broadcast();
		}
	}
}

#include <swis.h>

void ibicon::mount(const char *config)
{
	_kernel_oserror *err = _swix(Sunfish_Mount, _INR(0,2), text().c_str(), specialfield.length() ? specialfield.c_str() : NULL, config);
	if (err) throw err->errmess;
}

void ibicon::dismount()
{
	_kernel_oserror *err = _swix(Sunfish_Dismount, _INR(0,1), text().c_str(), specialfield.length() ? specialfield.c_str() : NULL);
	if (err) throw err->errmess;
}

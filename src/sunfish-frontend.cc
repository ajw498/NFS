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





sunfish::sunfish():
	application("Sunfish newfe"),
	ibaricon("", "")
{
	add(ibaricon);
	getmounts();
}

void sunfish::add_mounticon(const std::string &name, const std::string &specialfield)
{
	for (unsigned i = 0; i < ibaricons.size(); i++) {
		if ((ibaricons[i]->text() == name) && (ibaricons[i]->specialfield == specialfield)) return;
	}

	ibicon *i = new ibicon(name, specialfield);
	if (ibaricons.size() > 0) {
		if (ibaricons[ibaricons.size() - 1]->layout_valid()) {
			i->iconbar_position(-4);
			i->iconbar_priority(ibaricons[ibaricons.size() - 1]->handle());
		}
	} else {
		ibaricon.remove();
	}
	ibaricons.push_back(i);
	add(*i);
}

void sunfish_dismount(const char *discname, const char *specialfield);

void sunfish::handle_event(rtk::events::menu_selection& ev)
{
	for (vector<ibicon*>::iterator i = ibaricons.begin(); i != ibaricons.end(); i++) {
		if (ev.target() == &((*i)->ibdismount)) {
			//
			if (ibaricons.size() == 1) add(ibaricon);
			string cmd = "Filer_CloseDir Sunfish";
			const char *sf = NULL;
			if ((*i)->specialfield.length()) {
				cmd += "#";
				cmd += (*i)->specialfield;
				sf = (*i)->specialfield.c_str();
			}
			cmd += "::";
			cmd += (*i)->text();
			cmd += ".$";
			os::Wimp_StartTask(cmd.c_str());

			sunfish_dismount((*i)->text().c_str(), sf);
			delete *i;
			ibaricons.erase(i);
			break;
		}
	}
}

void sunfish::handle_event(rtk::events::close_window& ev)
{
	for (vector<exportbrowser*>::iterator i = exportbrowsers.begin(); i != exportbrowsers.end(); i++) {
		if (ev.target() == *i) {
			delete *i;
			exportbrowsers.erase(i);
			break;
		}
	}
}

void sunfish::smallicons(bool small)
{
	if (small != usesmallicons) {
		usesmallicons = small;
		// FIXME save to choices file
	}
}

int main(void)
{
	sunfish app;
	app.run();
	return 0;
}

#include <swis.h>

//void syslogf(char *fmt, ...)
//{
//	static char syslogbuf[1024];
//	va_list ap;
//
//	va_start(ap, fmt);
//	vsnprintf(syslogbuf, sizeof(syslogbuf), fmt, ap);
//	va_end(ap);
//
//	/* Ignore any errors, as there's not much we can do with them */
//	_swix(0x4c880, _INR(0,2), "newfe", syslogbuf, 5);
//}

extern "C" {

void __cyg_profile_func_enter(int *a,int b)
	__attribute__ ((no_instrument_function));
void __cyg_profile_func_exit(int a,int b)
	__attribute__ ((no_instrument_function));

void __cyg_profile_func_enter(int *a,int b)
{
  a--;
  if ((*a & 0xffffff00) == 0xff000000)
    {
      char *name;
      name = ((char *)(a)) - (*a & 0xff);
_swix(0x4c880, _INR(0,2), "newfei", name, 5);
    }
}

void __cyg_profile_func_exit(int a,int b)
	{ _swix(0x4c880, _INR(0,2), "newfei", "exit", 5);
	}

}

#include <swis.h>

void sunfish_dismount(const char *discname, const char *specialfield)
{
	_kernel_oserror *err = _swix(Sunfish_Dismount, _INR(0,1), discname, specialfield);
	if (err) throw err->errmess;
}

void sunfish::getmounts()
{
	_kernel_oserror *err;
	int start = 0;
	do {
		char *discname;
		char *specialfield;

		err = _swix(Sunfish_ListMounts, _IN(0) | _OUTR(0,2), start, &start, &discname, &specialfield);
		if (err) throw err->errmess;

		if (discname) {
			if (specialfield == NULL) specialfield = "";
			syslogf("Wibble", 3, "Start %d '%s' '%s'",start, discname, specialfield);
			add_mounticon(discname, specialfield);
		}
	} while (start);
}

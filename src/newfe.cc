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


#include "newfe.h"


using namespace std;
using namespace rtk;
using namespace rtk::desktop;
using rtk::graphics::point;
using rtk::graphics::box;


sunfish app;



sunfish::sunfish():
	application("Sunfish newfe")
{

	// Find centre of desktop.
	rtk::graphics::box dbbox(bbox());
	rtk::graphics::point dcentre((dbbox.xmin()+dbbox.xmax())/2,
		(dbbox.ymin()+dbbox.ymax())/2);

	// Find centre of window.
	rtk::graphics::box cbbox(_window.bbox());
	rtk::graphics::point ccentre((cbbox.xmin()+cbbox.xmax())/2,
		(cbbox.ymin()+cbbox.ymax())/2);

	// Open window at centre of desktop.
	add(_window,dcentre-ccentre);
	_window.broadcast();

	proginfo.add("Name","Sunfish");
	proginfo.add("Purpose","Mount NFS servers");
	proginfo.add("Author","� Alex Waugh, 2003-2006");
	proginfo.add("Version",Module_VersionString " (" Module_Date ")");
	ibinfo.text("Info");
	ibinfo.attach_dbox(proginfo);
	ibquit.text("Quit");
	ibmenu.title("Sunfish");
	ibmenu.add(ibinfo);
	ibmenu.add(ibquit);

	ibicon.text("Sunfish").hcentre(true);
	ibicon.text_and_sprite(true).validation("S!sunfish");
	ibicon.attach_menu(ibmenu);
	add(ibicon);
}


int main(void)
{
	app.run();
	return 0;
}

#include <swis.h>

void syslogf(char *fmt, ...)
{
	static char syslogbuf[1024];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(syslogbuf, sizeof(syslogbuf), fmt, ap);
	va_end(ap);

	/* Ignore any errors, as there's not much we can do with them */
	_swix(0x4c880, _INR(0,2), "newfe", syslogbuf, 5);
}

void syslog(char *fmt)
{
	/* Ignore any errors, as there's not much we can do with them */
	
}

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


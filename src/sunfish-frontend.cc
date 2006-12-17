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
	hostaliases.load();
	getmounts();
}

ibicon *sunfish::add_mounticon(const std::string &name, const std::string &specialfield, bool& found)
{
	for (unsigned i = 0; i < ibaricons.size(); i++) {
		if ((ibaricons[i]->text() == name) && (ibaricons[i]->specialfield == specialfield)) {
			found = true;
			return ibaricons[i];
		}
	}

	ibicon *i = new ibicon(name, specialfield);
	found = false;
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
	return i;
}

#include <stdio.h>

void sunfish::handle_event(rtk::events::menu_selection& ev)
{
	for (vector<ibicon*>::iterator i = ibaricons.begin(); i != ibaricons.end(); i++) {
		if (ev.target() == &((*i)->ibdismount)) {
			// Remove icon. Dismounting is handled by the ibicon
			if (ibaricons.size() == 1) add(ibaricon);
			delete *i;
			ibaricons.erase(i);
			break;
		} else if (ev.target() == &((*i)->ibsave)) {
			FILE *file = fopen("<Choices$Write>.Sunfish.savemounts","w");
			if (file == NULL) throw "Cannot open file";
			for (vector<ibicon*>::iterator j = ibaricons.begin(); j != ibaricons.end(); j++) {
				fprintf(file, "%s\n%s\n", (*j)->text().c_str(), (*j)->specialfield.c_str());
			}
			fclose(file);
			break;
		}
	}
	if (ev.target() == &(ibaricon.ibsave)) {
		FILE *file = fopen("<Choices$Write>.Sunfish.savemounts","w");
		if (file == NULL) throw "Cannot open file";
		fprintf(file, "\n\n");
		fclose(file);
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
#include "rtk/os/wimp.h"


void sunfish::getmounts()
{
	_kernel_oserror *err;
	bool found;
	int start = 0;
	do {
		const char *discname;
		const char *specialfield;

		err = _swix(Sunfish_ListMounts, _IN(0) | _OUTR(0,2), start, &start, &discname, &specialfield);
		/* Ignore errors */
		if (err) break;

		if (discname) add_mounticon(discname, specialfield ? specialfield : "", found);
	} while (start);

	FILE *file = fopen("Choices:Sunfish.savemounts","r");
	if (file) {
		char discname[256];
		char specialfield[256];
		while (1) {
			if (fgets(discname, 256, file) == NULL) break;
			discname[255] = '\0';
			char *end = discname;
			while (*end && *end != '\n') end++;
			if (*end == '\n') *end = '\0';
			if (fgets(specialfield, 256, file) == NULL) break;
			specialfield[255] = '\0';
			end = specialfield;
			while (*end && *end != '\n') end++;
			if (*end == '\n') *end = '\0';

			string hostname;
			string exportname;
			if (specialfield[0]) {
				hostname = specialfield;
				exportname = discname;
			} else {
				hostaliases.gethost(discname, hostname, exportname);
				if (hostname == "") continue;
			}
			mountchoices mountdetails;
			string filename = mountdetails.genfilename(hostname, exportname);
			mountdetails.load(filename);
			strcpy(mountdetails.server, hostname.c_str());
			strcpy(mountdetails.exportname, exportname.c_str());
			/* FIXME mountdetails.tcp = usetcp;
			mountdetails.nfs3 = nfsversion == 3;*/

			try {
				ibicon *icon = add_mounticon(discname, specialfield, found);
				if (!found) icon->mount(mountdetails.stringsave().c_str());
			}
			catch (...) {
				/* Ignore errors */
				//FIXME syslog them?
			}
		}

		fclose(file);
	}
}

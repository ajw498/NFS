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


#include "sunfish-frontend.h"

#include <algorithm>

#include <stdio.h>
#include <kernel.h>

using namespace std;
using namespace rtk;
using namespace rtk::desktop;
using rtk::graphics::point;
using rtk::graphics::box;





sunfish::sunfish():
	application("Sunfish"),
	ibaricon("", "", false, 2)
{
	hostaliases.load();
	getmounts();
	if (ibaricons.size() == 0) add(ibaricon);

	FILE *file = fopen("<Choices$Write>.Sunfish.choices","r");
	if (file != NULL) {
		char buffer[256];
		while (fgets(buffer, sizeof(buffer), file) != NULL) {
			if (strncmp("smallicons:", buffer, sizeof("smallicons:") - 1) == 0) {
				usesmallicons = atoi(buffer + sizeof("smallicons:") - 1);
			}
		}
		fclose(file);
	}
}

ibicon *sunfish::add_mounticon(const std::string &name, const std::string &specialfield, bool usetcp, int nfsversion, bool& found)
{
	for (unsigned i = 0; i < ibaricons.size(); i++) {
		if ((ibaricons[i]->text() == name) && (ibaricons[i]->specialfield == specialfield)) {
			found = true;
			return ibaricons[i];
		}
	}

	ibicon *i = new ibicon(name, specialfield, usetcp, nfsversion);
	found = false;
	if (ibaricons.size() > 0) {
		if (ibaricons[ibaricons.size() - 1]->layout_valid()) {
			i->position(-4);
			i->priority(ibaricons[ibaricons.size() - 1]->handle());
		}
	} else {
		ibaricon.remove();
	}
	ibaricons.push_back(i);
	add(*i);
	return i;
}


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
				fprintf(file, "%s\n%s\n%d\n%d\n", (*j)->text().c_str(), (*j)->specialfield.c_str(), (*j)->tcp, (*j)->nfsversion);
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

		FILE *file = fopen("<Choices$Write>.Sunfish.choices","w");
		if (file != NULL) {
			fprintf(file, "smallicons: %d\n\n", small);
			fclose(file);
		}
	}
}


int main(void)
{
	// Check correct module version is loaded. Do this here rather than
	// the !Run file to avoid hardcoding the version number
	int ret = _kernel_oscli("RMEnsure Sunfish " Module_VersionString " RMLoad <Sunfish$Dir>.Sunfish");
	if (ret != -2) ret = _kernel_oscli("RMEnsure Sunfish " Module_VersionString " Error xyz");
	if (ret == -2) {
		os::Wimp_ReportError(1, "Sunfish " Module_VersionString " module not found", "Sunfish", 0, 0);
		return 1;
	}

	sunfish app;
	app.run();
	return 0;
}


#include <swis.h>
#include "rtk/os/wimp.h"


void sunfish::getmounts()
{
	bool found;

	FILE *file = fopen("Choices:Sunfish.savemounts","r");
	if (file) {
		char discname[256];
		char specialfield[256];
		char buf[256];
		while (1) {
			if (fgets(discname, sizeof(discname), file) == NULL) break;
			discname[255] = '\0';
			char *end = discname;
			while (*end && *end != '\n') end++;
			if (*end == '\n') *end = '\0';
			if (fgets(specialfield, sizeof(specialfield), file) == NULL) break;
			specialfield[255] = '\0';
			end = specialfield;
			while (*end && *end != '\n') end++;
			if (*end == '\n') *end = '\0';
			if (fgets(buf, sizeof(buf), file) == NULL) break;
			bool usetcp = atoi(buf);
			if (fgets(buf, sizeof(buf), file) == NULL) break;
			int nfsversion = atoi(buf);

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
			mountdetails.server = hostname;
			mountdetails.exportname = exportname;
			mountdetails.tcp = usetcp;
			mountdetails.nfs3 = nfsversion == 3;

			ibicon *icon = 0;
			try {
				icon = add_mounticon(discname, specialfield, usetcp, nfsversion, found);
				if (!found) icon->mount(mountdetails.stringsave().c_str());
			}
			catch (...) {
				if (icon) {
					delete icon;
					for (vector<ibicon*>::iterator i = ibaricons.begin(); i != ibaricons.end(); ++i) {
						if (*i == icon) {
							ibaricons.erase(i);
							break;
						}
					}
				}
				syslogf(LOGNAME, LOGERROR, "Unable to mount %s %s", specialfield, discname);
			}
		}

		fclose(file);
	}

	_kernel_oserror *err;
	int start = 0;

	do {
		const char *discname;
		const char *specialfield;

		err = _swix(Sunfish_ListMounts, _IN(0) | _OUTR(0,2), start, &start, &discname, &specialfield);
		/* Ignore errors */
		if (err) break;

		if (discname) add_mounticon(discname, specialfield ? specialfield : "", false, 2, found);
	} while (start);
}

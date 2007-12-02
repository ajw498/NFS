/*
	$Id$

	Search for host transient dialogue box


	Copyright (C) 2007 Alex Waugh
	
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

#include "rtk/os/wimp.h"

#include "hostbrowser.h"

searchhost::searchhost(hostbrowser& browser) :
	parent(browser)
{
	title("Search for host");
	close_icon(false);
	back_icon(false);

	description.text("Hostname or IP address");
	name.text("",256);
	name.min_size(point(240,0));
	cancel.text("Cancel");
	savebutton.text("Search");

	layout1.margin(16).ygap(16);
	layout1.add(layout2);
	layout1.add(layout3);

	layout2.xgap(8);
	layout2.add(description);
	layout2.add(name);

	layout3.xgap(16);
	layout3.add(cancel);
	layout3.add(savebutton);

	add(layout1);
}

void searchhost::handle_event(events::mouse_click& ev)
{
	if (ev.buttons() == 2) {
	} else if (ev.target() == &savebutton) {
		parent.search(name.text());
		if (ev.buttons() == 4) os::Wimp_CreateMenu(-1, point());
	} else if (ev.target() == &cancel) {
		if (ev.buttons() == 4) {
			os::Wimp_CreateMenu(-1, point());
		} else {
			name.text("");
		}
	}
}

void searchhost::handle_event(events::key_pressed& ev)
{
	if (ev.code() == 13) {
		parent.search(name.text());
		os::Wimp_CreateMenu(-1, point());
		ev.processed(true);
	}
}

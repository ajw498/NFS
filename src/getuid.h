/*
	$Id$

	Dialogue box for getting the UID/GID or username/password


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

#ifndef GETUID_H
#define GETUID_H

#include "rtk/desktop/window.h"
#include "rtk/desktop/writable_field.h"
#include "rtk/desktop/action_button.h"
#include "rtk/desktop/default_button.h"
#include "rtk/desktop/grid_layout.h"
#include "rtk/desktop/row_layout.h"
#include "rtk/desktop/column_layout.h"
#include "rtk/events/mouse_click.h"
#include "rtk/events/key_pressed.h"

#include "button_row_layout.h"

#include "browse.h"
#include "mountchoices.h"


using namespace rtk;
using namespace rtk::desktop;

class sunfish;

class getuid:
	public window,
	public events::mouse_click::handler,
	public events::key_pressed::handler
{
public:
	getuid();

	// Setup the mount details and open with window if needed
	void setup(const hostinfo& info, std::string name, bool tcp, int version, sunfish& parent);

	void handle_event(events::mouse_click& ev);
	void handle_event(events::key_pressed& ev);

private:
	// Use the current details to create the mount details
	void use(bool save);

	// Create a mount with the given details
	void create_mount(mountchoices& mountdetails, sunfish& app);

	hostinfo host;
	string exportname;
	bool usetcp;
	int nfsversion;
	icon uidlabel;
	icon gidlabel;
	writable_field uid;
	writable_field gid;
	icon explain1;
	icon explain2;
	action_button cancel;
	action_button set;
	default_button save;
	column_layout layout1;
	grid_layout layout2;
	button_row_layout layout3;
	column_layout layout4;
};

#endif


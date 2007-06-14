/*
	$Id$

	Edit UID/permissions related choices


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

#ifndef EDITUID_H
#define EDITUID_H

#include "rtk/desktop/window.h"
#include "rtk/desktop/writable_field.h"
#include "rtk/desktop/action_button.h"
#include "rtk/desktop/default_button.h"
#include "rtk/desktop/radio_button.h"
#include "rtk/desktop/option_button.h"
#include "rtk/desktop/grid_layout.h"
#include "rtk/desktop/row_layout.h"
#include "rtk/desktop/button_row_layout.h"
#include "rtk/desktop/column_layout.h"
#include "rtk/events/mouse_click.h"
#include "rtk/events/key_pressed.h"


using namespace std;
using namespace rtk;
using namespace rtk::desktop;

class sunfish;

class edituid:
	public window,
	public events::mouse_click::handler,
	public events::key_pressed::handler
{
public:
	edituid();
	void open(const string& host, string& exportname, sunfish& app);
	void handle_event(events::mouse_click& ev);
	void handle_event(events::key_pressed& ev);
private:
	void load(const string& host, string& exportname);
	void save();
	string filename;

	icon uidlabel;
	icon gidslabel;
	icon umasklabel;
	icon unumasklabel;
	writable_field uid;
	writable_field gids;
	writable_field umask;
	writable_field unumask;
	icon umaskoctal;
	icon unumaskoctal;

	action_button cancel;
	default_button savebutton;
	column_layout layout1;
	grid_layout layout2;
	button_row_layout layout3;
};

#endif


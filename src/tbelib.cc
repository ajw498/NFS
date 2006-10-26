/*
	$Id$

	Frontend for creating mount files


	Copyright (C) 2003 Alex Waugh
	
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <kernel.h>
#include <stdarg.h>
#include <stddef.h>
#include <ctype.h>
#include <unixlib.h>

#include "Event.h"

#include "oslib/gadget.h"
#include "oslib/menu.h"
#include "oslib/radiobutton.h"
#include "oslib/optionbutton.h"
#include "oslib/stringset.h"
#include "oslib/writablefield.h"
#include "oslib/numberrange.h"
#include "oslib/window.h"
#include "oslib/iconbar.h"
#include "oslib/proginfo.h"

#include "tbelib.h"

void gadget::faded(void)
{
	gadget_flags flags;
	xgadget_get_flags(0, parent->objectid, gadgetid, &flags);
	flags |= gadget_FADED;
	xgadget_set_flags(0, parent->objectid, gadgetid, flags);
}

void gadget::unfaded(void)
{
	gadget_flags flags;
	xgadget_get_flags(0, parent->objectid, gadgetid, &flags);
	flags &= ~gadget_FADED;
	xgadget_set_flags(0, parent->objectid, gadgetid, flags);
}

void gadget::faded(osbool set)
{
	gadget_flags flags;
	xgadget_get_flags(0, parent->objectid, gadgetid, &flags);
	if (set) flags |= gadget_FADED; else flags &= ~gadget_FADED;
	xgadget_set_flags(0, parent->objectid, gadgetid, flags);
}


void writablefield::operator=(const char *value)
{
	/*Error if parentid not yet set */
	xwritablefield_set_value(0, parent->objectid, gadgetid, value);
}

void writablefield::operator=(const int value)
{
	char tmp[15];
	/*Error if parentid not yet set */
	snprintf(tmp, sizeof(tmp), "%d", value);
	xwritablefield_set_value(0, parent->objectid, gadgetid, tmp);
}

writablefield::operator char*()
{
	static char buf[1024];
	xwritablefield_get_value(0, parent->objectid, gadgetid, buf, sizeof(buf), NULL);
	return buf;
}

writablefield::operator int()
{
	char buf[1024];
	xwritablefield_get_value(0, parent->objectid, gadgetid, buf, sizeof(buf), NULL);
	return atoi(buf);
}


void stringset::operator=(const char *value)
{
	/*Error if parentid not yet set */
	xstringset_set_selected_string(0, parent->objectid, gadgetid, value);
}

void stringset::operator=(const int value)
{
	char tmp[15];
	/*Error if parentid not yet set */
	snprintf(tmp, sizeof(tmp), "%d", value);
	xstringset_set_selected_string(0, parent->objectid, gadgetid, tmp);
}

stringset::operator char*()
{
	static char buf[1024];
	xstringset_get_selected_string(0, parent->objectid, gadgetid, buf, sizeof(buf), NULL);
	return buf;
}

stringset::operator int()
{
	char buf[1024];
	xstringset_get_selected_string(0, parent->objectid, gadgetid, buf, sizeof(buf), NULL);
	return atoi(buf);
}



void numberrange::operator=(const int value)
{
	xnumberrange_set_value(0, parent->objectid, gadgetid, value);
}

void numberrange::setupperbound(const int value)
{
	xnumberrange_set_bounds(numberrange_BOUND_UPPER, parent->objectid, gadgetid, 0, value, 0, 0);
}

numberrange::operator int()
{
	int value;
	xnumberrange_get_value(0, parent->objectid, gadgetid, &value);
	return value;
}


void radiobutton::operator=(const osbool value)
{
	/*Error if parentid not yet set */
	xradiobutton_set_state(0, parent->objectid, gadgetid, value);
}

radiobutton::operator osbool()
{
	osbool value;
	xradiobutton_get_state(0, parent->objectid, gadgetid, &value, NULL);
	return value;
}


void optionbutton::operator=(const osbool value)
{
	/*Error if parentid not yet set */
	xoptionbutton_set_state(0, parent->objectid, gadgetid, value);
}

optionbutton::operator osbool()
{
	osbool value;
	xoptionbutton_get_state(0, parent->objectid, gadgetid, &value);
	return value;
}

window::window(const char *name, bool isautocreated)
{
	objectname = name;
	objectid = 0;

	autocreated = isautocreated;
	if (isautocreated) {

	/*ERROR*/event_register_toolbox_handler(event_ANY, action_OBJECT_AUTO_CREATED, wautocreated, this);
	} else  {
		xtoolbox_create_object(0, (toolbox_id)name, &objectid);
	}
}

window::~window()
{
	if (!autocreated) {
		xtoolbox_delete_object(0, objectid);
	}
}

osbool window::wautocreated(bits event_code, toolbox_action *event, toolbox_block *id_block,void *handle)
{
	window *thiswin = static_cast<window*>(handle);

	if (strcmp(event->data.created.name,thiswin->objectname) == 0) {
		thiswin->objectid = id_block->this_obj;
		/*ERROR*/event_deregister_toolbox_handler(event_ANY, action_OBJECT_AUTO_CREATED, wautocreated, handle);

		/*ERROR*/event_register_toolbox_handler(id_block->this_obj, action_ANY, wevent, handle);

		/*ERROR*/event_register_toolbox_handler(id_block->this_obj, action_WINDOW_ABOUT_TO_BE_SHOWN, wshow, handle);
		/*ERROR*/event_register_toolbox_handler(id_block->this_obj, action_WINDOW_DIALOGUE_COMPLETED, whide, handle);
		return 0;//1; FIXME
	}

	return 0;
}

void window::set_title(const char *name)
{
	xwindow_set_title(0, objectid, name);
}

osbool window::wevent(bits event_code, toolbox_action *event, toolbox_block *id_block,void *handle)
{
	window *thiswin = static_cast<window*>(handle);

	return thiswin->customevent(event_code);
}

osbool window::whide(bits event_code, toolbox_action *event, toolbox_block *id_block,void *handle)
{
	window *thiswin = static_cast<window*>(handle);

	if (thiswin->abouttobehidden() && !thiswin->autocreated) ;//delete thiswin;
	return TRUE;
}

void window::show(void)
{
	/**/xtoolbox_show_object(0, objectid, toolbox_POSITION_DEFAULT, NULL, toolbox_NULL_OBJECT, toolbox_NULL_COMPONENT);
}

void window::hide(void)
{
	/**/xtoolbox_hide_object(0, objectid);
}



osbool window::wshow(bits event_code, toolbox_action *event, toolbox_block *id_block,void *handle)
{
	window *thiswin = static_cast<window*>(handle);

	thiswin->abouttobeshown();
	return TRUE;
}






application::application(const char *dirname)
{
	int toolbox_events[] = {0};
	int wimp_messages[] = {0};
	static messagetrans_control_block messages;
	static toolbox_block id_block;

	if (xtoolbox_initialise(0, 310, (wimp_message_list*)wimp_messages,
							(toolbox_action_list*)toolbox_events,
							dirname, &messages, &id_block, 0, 0, 0)
							!= NULL) exit(EXIT_FAILURE);
	event_initialise(&id_block);
	event_set_mask(1+256);
}

void application::poll(void)
{
	wimp_event_no event_code;
	static wimp_block poll_block;
	event_poll(&event_code, &poll_block, 0);
}


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

#include "oslib/osfile.h"
#include "oslib/osfscontrol.h"
#include "oslib/osgbpb.h"
#include "oslib/wimp.h"
#include "oslib/filer.h"


class window
{
public:
	window(const char *name);
//	virtual ~window() {}//why?
	toolbox_o objectid;
	void show(void);
	void hide(void);
protected:
	virtual void abouttobeshown(void) { }
	virtual void abouttobehidden(void) { }
	virtual osbool customevent(bits event_code) { return FALSE; }
private:
	static osbool wautocreated(bits event_code, toolbox_action *event, toolbox_block *id_block,void *handle);
	static osbool wshow(bits event_code, toolbox_action *event, toolbox_block *id_block,void *handle);
	static osbool whide(bits event_code, toolbox_action *event, toolbox_block *id_block,void *handle);
	static osbool wevent(bits event_code, toolbox_action *event, toolbox_block *id_block,void *handle);
//	osbool event(bits event_code);
	const char *objectname;
};

/*void window::register_event(int event, eventfn fn)
{
	events[event] = fn;
} */

class gadget
{
public:
	gadget(window *parentwin, int id) { gadgetid = id; parent = parentwin; }
	void faded(void);
	void unfaded(void);
	void faded(osbool set);

private:
protected:
	int gadgetid;
	window *parent;
};


class writablefield:
	public gadget
{
public:
	writablefield(window *parentwin, int id) : gadget(parentwin, id) { }
	void operator=(const char *value);
	void operator=(const int value);
	operator char*();
	operator int();
};



class stringset:
	public gadget
{
public:
	stringset(window *parentwin, int id) : gadget(parentwin, id) { }
	void operator=(const char *value);
	void operator=(const int value);
	operator char*();
	operator int();
};

class numberrange:
	public gadget
{
public:
	numberrange(window *parentwin, int id) : gadget(parentwin, id) { }
	void operator=(const int value);
	operator int();
	void setupperbound(const int value);
};

class radiobutton:
	public gadget
{
public:
	radiobutton(window *parentwin, int id) : gadget(parentwin, id) { }
	void operator=(const osbool value);
	operator osbool();
};


class optionbutton:
	public gadget
{
public:
	optionbutton(window *parentwin, int id) : gadget(parentwin, id) { }
	void operator=(const osbool value);
	operator osbool();
};


class application
{
public:
	application(const char *dirname);
	void poll(void);
private:
};


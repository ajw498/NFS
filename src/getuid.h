/*
	Dialogue box for getting the UID/GID or username/password


	Copyright (C) 2006 Alex Waugh

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef GETUID_H
#define GETUID_H

#include "rtk/desktop/window.h"
#include "rtk/desktop/writable_field.h"
#include "rtk/desktop/action_button.h"
#include "rtk/desktop/default_button.h"
#include "rtk/desktop/grid_layout.h"
#include "rtk/desktop/row_layout.h"
#include "rtk/desktop/button_row_layout.h"
#include "rtk/desktop/column_layout.h"
#include "rtk/events/mouse_click.h"
#include "rtk/events/key_pressed.h"


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


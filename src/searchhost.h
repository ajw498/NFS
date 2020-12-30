/*
	Search for host transient dialogue box


	Copyright (C) 2007 Alex Waugh

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

#ifndef SEARCHHOST_H
#define SEARCHHOST_H

#include "rtk/desktop/window.h"
#include "rtk/desktop/writable_field.h"
#include "rtk/desktop/action_button.h"
#include "rtk/desktop/default_button.h"
#include "rtk/desktop/row_layout.h"
#include "rtk/desktop/column_layout.h"
#include "rtk/events/mouse_click.h"
#include "rtk/events/key_pressed.h"


using namespace std;
using namespace rtk;
using namespace rtk::desktop;
using namespace rtk::graphics;

class hostbrowser;

class searchhost:
	public window,
	public events::mouse_click::handler,
	public events::key_pressed::handler
{
public:
	searchhost(hostbrowser& browser);
	void handle_event(events::mouse_click& ev);
	void handle_event(events::key_pressed& ev);
private:
	hostbrowser& parent;

	icon description;
	writable_field name;

	action_button cancel;
	default_button savebutton;
	column_layout layout1;
	row_layout layout2;
	row_layout layout3;
};

#endif


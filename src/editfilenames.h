/*
	Edit filename and filetype related choices


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

#ifndef EDITFILENAMES_H
#define EDITFILENAMES_H

#include "rtk/desktop/window.h"
#include "rtk/desktop/writable_field.h"
#include "rtk/desktop/action_button.h"
#include "rtk/desktop/default_button.h"
#include "rtk/desktop/radio_button.h"
#include "rtk/desktop/option_button.h"
#include "rtk/desktop/labelled_box.h"
#include "rtk/desktop/string_set.h"
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

class editfilenames:
	public window,
	public events::mouse_click::handler,
	public events::key_pressed::handler
{
public:
	editfilenames();
	void open(const string& host, const string& ip, string& exportname, sunfish& app);
	void handle_event(events::mouse_click& ev);
	void handle_event(events::key_pressed& ev);
private:
	void load(const string& host, const string& ip, string& exportname);
	void save();
	string filename;

	labelled_box filenames;
	labelled_box filetypes;
	radio_button showalways;
	radio_button showroot;
	radio_button shownever;
	option_button casesensitive;
	string_set encoding;
	icon defaultfiletypelabel;
	writable_field defaultfiletype;
	radio_button extalways;
	radio_button extneeded;
	radio_button extnever;
	option_button unixex;
	option_button followsymlinks;
	option_button escapewin;
	action_button cancel;
	default_button savebutton;
	column_layout layout1;
	row_layout layout2;
	button_row_layout layout4;
	column_layout layout5;
	column_layout layout6;
};

#endif


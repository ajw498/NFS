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

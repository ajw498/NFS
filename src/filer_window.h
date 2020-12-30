/*
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

#ifndef _FILER_WINDOW
#define _FILER_WINDOW

#include <vector>

#include "rtk/os/os.h"
#include "rtk/desktop/window.h"
#include "rtk/desktop/icon.h"
#include "rtk/events/mouse_click.h"
#include "rtk/events/close_window.h"
#include "rtk/events/user_drag_box.h"
#include "filer_window_layout.h"

class filer_window:
	public rtk::desktop::window,
	public rtk::events::mouse_click::handler,
	public rtk::events::user_drag_box::handler
{
public:
	filer_window(rtk::graphics::point size=rtk::graphics::point(640,512));
	~filer_window();
	virtual void reformat(const point& origin,const box& pbbox);
	void handle_event(rtk::events::mouse_click& ev);
	void handle_event(rtk::events::user_drag_box& ev);
	filer_window_layout layout;
	std::vector<rtk::desktop::icon *> icons;

	filer_window& add_icon(const std::string& text, const std::string& sprite, int index=-1);
	filer_window& remove_icon(int index);
	filer_window& remove_all_icons();

	filer_window& allowdrag(bool allowdrag) { _allowdrag = allowdrag; return *this; }
	filer_window& allowselection(bool allowselection) { _allowselection = allowselection; return *this; }
	filer_window& smallicons(bool smallicons);

	virtual void open_menu(const std::string& item, bool selection, rtk::events::mouse_click& ev) {}
	virtual void doubleclick(const std::string& item, rtk::events::mouse_click& ev) {}
private:
	void start_drag(rtk::events::mouse_click& ev, rtk::desktop::icon* ic);
	bool _icondrag;
	bool _adjustdrag;
	bool _allowdrag;
	bool _allowselection;
	bool _smallicons;
};

#endif

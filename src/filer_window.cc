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
#include "filer_window.h"
#include "rtk/events/save_to_app.h"



filer_window::filer_window(rtk::graphics::point size):
	window(size),
	_allowdrag(true),
	_allowselection(true),
	_smallicons(false)
{
	min_win_size(point(1,0));
	ignore_extent(false);
	y_scroll_bar(true);
	adjust_icon(true);
	toggle_icon(true);

	layout.min_cell_width(172).min_cell_height(124);
	layout.min_layout_width(450);//FIXME
	layout.xgap(16).margin(rtk::graphics::box(-8,0,8,10));
	layout.xbaseline(component::xbaseline_left);
	layout.ybaseline(component::ybaseline_top);
	add(layout);
}

filer_window::~filer_window()
{
	for (unsigned i = icons.size(); i > 0; i--)
	{
		remove_icon(i - 1);
	}
}

void filer_window::handle_event(rtk::events::user_drag_box& ev)
{
	if (_icondrag) {
		icon *srcicon = static_cast<icon *>(ev.target());
		rtk::events::save_to_app saveev(*this, srcicon->text());
		saveev.post();
	} else {
		rtk::graphics::box dbox = ev.dbox();
		// The drag box depends on the direction the drag took place, so swap if backwards
		if (dbox.xmin() > dbox.xmax()) {
			int t = dbox.xmin();
			dbox.xmin(dbox.xmax());
			dbox.xmax(t);
		}
		if (dbox.ymin() > dbox.ymax()) {
			int t = dbox.ymin();
			dbox.ymin(dbox.ymax());
			dbox.ymax(t);
		}
		dbox -= layout.origin();

		// Select all icons inside the box
		for (unsigned i = 0; i < icons.size(); i++) {
			rtk::graphics::box ibbox = icons[i]->bbox() + icons[i]->origin();
			if ((ibbox.xmin() < dbox.xmax()) &&
			    (ibbox.xmax() > dbox.xmin()) &&
			    (ibbox.ymin() < dbox.ymax()) &&
			    (ibbox.ymax() > dbox.ymin())) {
				icons[i]->selected(!icons[i]->selected());
			}
		}
	}
}


void filer_window::start_drag(rtk::events::mouse_click& ev, rtk::desktop::icon* ic)
{
	if (ic && _allowdrag) {
		_icondrag = true;
		// The icon being dragged always gets selected, even if an adjust drag
		ic->selected(true);
		int flags;
		rtk::os::OS_Byte161(28,&flags);
		if (flags & 2) {
			int selected = 0;
			for (unsigned i = 0; i < icons.size(); i++) {
				if (icons[i]->selected()) {
					if (++selected > 1) break;
				}
			}
			std::string sprite("package");
			if (selected == 1) {
				sprite = ic->validation();
				sprite.erase(0, 1);
			}

			ic->drag_sprite(rtk::graphics::box(ev.position(),ev.position()), (rtk::os::sprite_area*)1, sprite);
		} else {
			ic->drag(ic->bbox(),5);
		}
	} else if ((ic == NULL) && _allowselection) {
		_icondrag = false;
		rtk::graphics::point p = ev.position() + layout.origin();
		drag(rtk::graphics::box(p,p),bbox(),6);
		/* FIXME? clip box to window, but allow pointer outside in y axis (use wimp_dragbox extra flags) */
	}
}

void filer_window::handle_event(rtk::events::mouse_click& ev)
{
	rtk::desktop::icon* ic=dynamic_cast<rtk::desktop::icon*>(ev.target());

	switch (ev.buttons()) {
	case 2: {
		/* If none selected, then temporarily select one under pointer */
		int selected = 0;
		for (unsigned i = 0; i < icons.size(); i++) {
			if (icons[i]->selected()) {
				ic = icons[i];
				if (++selected > 1) break;
			}
		}
		open_menu(ic ? ic->text() : "", selected > 1, ev); /*handle more than one*/
		break;
		}
	case 1:
		/* Doubleclick adjust */
		if (ic) {
			ic->selected(false);
			doubleclick(ic->text(), ev);
			rtk::events::close_window closeev(*this);
			closeev.post();
		}
		break;
	case 16*1:
		/* Drag adjust */
		_adjustdrag = true;
		start_drag(ev, ic);
		break;
	case 256*1:
		/* Click adjust */
		if (ic && _allowselection) ic->selected(!ic->selected());
		break;
	case 4:
		/* Doubleclick select */
		if (ic) {
			ic->selected(false);
			doubleclick(ic->text(), ev);
		}
		break;
	case 16*4:
		/* Drag select */
		_adjustdrag = false;
		start_drag(ev, ic);
		break;
	case 256*4:
		/* Click select */
		for (unsigned i = 0; i < icons.size(); i++) icons[i]->selected(false);
		if (ic && _allowselection) ic->selected(true);
		break;
	}
}

filer_window& filer_window::add_icon(const std::string& text, const std::string& sprite, int index)
{
	rtk::desktop::icon *icon;
	icon = new rtk::desktop::icon;
	icon->text(text).text_and_sprite(true).validation("S"+sprite);
	icon->rjustify(false).button(10).half_size(_smallicons);
	icon->hcentre(!_smallicons).vcentre(_smallicons);
	icon->yfit(false);
	icon->xfit(false);
	icon->xbaseline(_smallicons ? component::xbaseline_left : component::xbaseline_centre);

	if (index < 0) index = icons.size();
	if ((unsigned)index >= icons.size()) icons.resize(index + 1, 0);

	icons[index] = icon;
	layout.add(*icon, index);

	// Expand window to fit new icon. Ideally, this would only happen if
	// the window is already open at maximum extent, but that is quite
	// hard to determine in all situations.
	box ibbox = layout.ideal_bbox();
	box obbox;
	obbox.xmax(ibbox.xsize());
	obbox.ymin(-ibbox.ysize());

	basic_window::reformat(origin(), obbox);

	return *this;
}

filer_window& filer_window::remove_icon(int index)
{
	if ((index < 0) || ((unsigned)index >= icons.size())) return *this;

	icons[index]->remove();
	delete icons[index];
	icons[index] = 0;
	//FIXME move other entries down to fill gap (and alter gridwrap_layout to do the same)

	if ((unsigned)index == icons.size() - 1) icons.resize(index, 0);

	return *this;
}

filer_window& filer_window::remove_all_icons()
{
	for (unsigned i = 0; i < icons.size(); i++) delete icons[i];

	icons.resize(0);
	layout.cells(0);

	return *this;
}

void filer_window::reformat(const point& origin,const box& pbbox)
{
	rtk::graphics::box mbbox = pbbox;
	if (mbbox.ysize() > layout.bbox().ysize()) {
		mbbox.ymin(mbbox.ymax()-layout.bbox().ysize());
	}
	if (mbbox.xsize() > layout.bbox().xsize()) {
		mbbox.xmax(mbbox.xmin()+layout.bbox().xsize());
	}
	basic_window::reformat(origin, mbbox);
}

filer_window& filer_window::smallicons(bool smallicons)
{
	if (smallicons != _smallicons) {
		for (unsigned i = 0; i < icons.size(); i++) {
			icons[i]->half_size(smallicons).hcentre(!smallicons).vcentre(smallicons);
			icons[i]->xbaseline(smallicons ? component::xbaseline_left : component::xbaseline_centre);
			// large hcentre, no rjustify, no vcentre
			// small no hcentre, no rjustify, vcentre
		}
		layout.min_cell_height(smallicons ? 44 : 124);
		layout.invalidate();
	}
	_smallicons = smallicons;
	return *this;
}



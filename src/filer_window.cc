
#include "filer_window.h"


filer_window::filer_window(rtk::graphics::point size):
	window(size),
	_allowdrag(true),
	_allowselection(true),
	_smallicons(false)
{
	min_win_size(point(1,0));
	y_scroll_bar(true);
	adjust_icon(true);
	toggle_icon(true);

	layout.min_width(172).min_height(124);
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
		drag_ended(_adjustdrag, ev);
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
		/*handle more than one*/
	} else if ((ic == NULL) && _allowselection) {
		_icondrag = false;
		rtk::graphics::point p = ev.position() + layout.origin();
		drag(rtk::graphics::box(p,p),bbox(),6);
		/*clip box to window, but allow pointer outside in y axis (use wimp_dragbox extra flags) */
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
	icon->text(text).rjustify(false).button(10).half_size(_smallicons);
	icon->text(text).hcentre(!_smallicons).vcentre(_smallicons);
	icon->text_and_sprite(true).validation("S"+sprite);
	icon->yfit(false);
	icon->xfit(false);
	icon->xbaseline(_smallicons ? component::xbaseline_left : component::xbaseline_centre);

	if (index < 0) index = icons.size();
	if ((unsigned)index >= icons.size()) icons.resize(index + 1, 0);

	icons[index] = icon;
	layout.add(*icon, index);
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

rtk::graphics::box filer_window::max_bbox() const
{
	rtk::graphics::box mbbox(rtk::desktop::window::max_bbox());
	mbbox.xmax(layout.min_bbox().xsize());
	mbbox.ymin(-layout.bbox().ysize());
	return mbbox;
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
		layout.min_height(smallicons ? 44 : 124);
		layout.invalidate(); // FIXME should this get done automagically
	}
	_smallicons = smallicons;
	return *this;
}



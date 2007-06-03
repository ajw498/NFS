
#ifndef _RTK_DESKTOP_STRING_SET
#define _RTK_DESKTOP_STRING_SET

#include "rtk/desktop/sizeable_component.h"
#include "rtk/desktop/icon.h"
#include "rtk/desktop/selection_field.h"

namespace rtk {
namespace desktop {

/** A class to allow a selection from a number range.
 * This consists of an optional text label, the value display
 * (which may optionally be writable), up and down adjuster arrows,
 * and optionally a units label.
 * The value_type template parameter specifies the type of the value range.
 * It will typically be an int, but can be anything that a linear_sequence
 * will accept.
 */
template<class value_type>
class string_set:
	public sizeable_component,
	public events::mouse_click::handler,
	public events::menu_selection::handler
{
private:
	/** The class from which this one is derived. */
	typedef sizeable_component inherited;

	/** The bounding box. */
	box _bbox;

	/** The icon used as the text label. */
	icon _label;

	/** The down arrow. */
	icon _menuicon;

	/** The icon used as the units label. */
	icon _units;

	/** The value display field */
	writable_field _value;

	std::vector<menu_item *> _menu_items;
	menu _menu;

public:
	/** Construct number range.
	 * @param min the minimum value of the sequence
	 * @param max the maximum value of the sequence
	 * @param step the step size of the sequence
	*/
	string_set();

	/** Destroy number range. */
	virtual ~string_set()
		{ remove(); }

	virtual box auto_bbox() const;
	virtual box bbox() const;
	virtual void resize() const;
	virtual void reformat(const point& origin,const box& pbbox);
	virtual void unformat();
	virtual void redraw(gcontext& context,const box& clip);
	void handle_event(rtk::events::mouse_click& ev);
	void handle_event(rtk::events::menu_selection& ev);

	/** Get the currently selected value.
	 * @return the current value
	 */
	value_type value() const
		{ return _value.text(); }

	/** Get label icon text.
	 * @return the label icon text
	 */
	string label() const
		{ return _label.text(); }

	/** Get units icon text.
	 * @return the units icon text
	 */
	string units() const
		{ return _units.text(); }

	/** Get the minimum value of the range.
	 * @return the minimum value
	 */
//	value_type min() const
//		{ return _seq.min(); }

	/** Get the maximum value of the range.
	 * @return the maximum value
	 */
//	value_type max() const
//		{ return _seq.max(); }

	/** Get the step value of the range.
	 * @return the step value
	 */
//	value_type step() const
//		{ return _seq.step(); }

	/** Get writable flag.
	 * @return true if the field content is directly editable by the user,
	 *  otherwise false
	 */
//	bool writable() const
//		{ return _value.writable(); }

	/** Set the current value.
	 * @param value the value to set
	 * @return a reference to this
	 */
	string_set& value(value_type value)
		{ _value.text(value); return *this; }

	string_set& menutitle(std::string value)
		{ _menu.title(value); return *this; }

	/** Set label icon text.
	 * @param text the required label text
	 * @param capacity the required capacity (defaults to length of
	 *  specified icon text)
	 * @return a reference to this
	 */
	string_set& label(const string& text,icon::size_type capacity=0)
		{ _label.text(text,capacity); return *this; }

	string_set& add(const value_type& value)
		{ //_seq.push_back(value);
		menu_item *m = new menu_item;
		m->text(/*rtk::util::lexical_cast<std::string>*/(value));
		_menu_items.push_back(m);
		_menu.add(*m);
		return *this; }

	/** Set units icon text.
	 * @param text the required units text
	 * @param capacity the required capacity (defaults to length of
	 *  specified icon text)
	 * @return a reference to this
	 */
	string_set& units(const string& text,icon::size_type capacity=0)
		{ _units.text(text,capacity); return *this; }

	/** Snap to an allowed value.
	 * This function ensures that the displayed field value is a member
	 * of the sequence of allowed values.
	 * @return a reference to this
	 */
	string_set& snap();

	/** Set writable flag.
	 * @param writable true if the field is required to be directly
	 *  writable by the user, otherwise false
	 * @return a reference to this
	 */
	string_set& writable(bool writable)
		{ /*_value.writable(writable);*/ return *this; }

};

template<class value_type> void string_set<value_type>::handle_event(rtk::events::mouse_click& ev)
{
	if (ev.buttons() == 2) return;
	if (ev.target() == &_menuicon) {
		_menu.redirect(this);
		point pos;
		pos.x(bbox().xmax() + 10);// FIXME why 10 needed?
		pos.y(_menuicon.bbox().ymax());
		point offset=_menu.min_bbox().xminymax();
		parent_application(pos)->add(_menu, pos - offset);
	}
}

template<class value_type> void string_set<value_type>::handle_event(rtk::events::menu_selection& ev)
{
	for (unsigned i = 0; i < _menu_items.size(); i++) {
		if (ev.target() == _menu_items[i]) {
			value(_menu_items[i]->text());
			break;
		}
	}
}

template<class value_type> string_set<value_type>::string_set()
{
	_label.xbaseline(xbaseline_left);
	_label.ybaseline(ybaseline_centre);
	_value.xbaseline(xbaseline_left);
	_value.ybaseline(ybaseline_centre);
	_menuicon.xbaseline(xbaseline_left);
	_menuicon.ybaseline(ybaseline_centre);
	_units.xbaseline(xbaseline_left);
	_units.ybaseline(ybaseline_centre);

//	_value.writable(false);

	_menuicon.button(3);
	_menuicon.text_and_sprite(true);
	_menuicon.validation("R5;Sgright,pgright");

	link_child(_label);
	link_child(_units);
	link_child(_menuicon);
	link_child(_value);
}

template<class value_type> box string_set<value_type>::auto_bbox() const
{
	box lbbox=_label.min_bbox();
	box vbbox=_value.min_bbox();
	box mbbox=_menuicon.min_bbox();
	box nbbox=_units.min_bbox();

	ybaseline_set ybs;
	ybs.add(lbbox,_label.ybaseline());
	ybs.add(vbbox,_value.ybaseline());
	ybs.add(mbbox,_menuicon.ybaseline());
	ybs.add(nbbox,_units.ybaseline());

	box abbox;
	abbox.ymax(ybs.ysize());
	// The width should be enough to include all components, plus a gap
	// of 8 units between the value and the adjuster arrows
	abbox.xmax(lbbox.xsize()+vbbox.xsize()+8+mbbox.xsize()+nbbox.xsize());

	// Translate to external origin and return.
	abbox-=external_origin(abbox,xbaseline_left,ybaseline_bottom);
	return abbox;
}

template<class value_type> box string_set<value_type>::bbox() const
{
	return _bbox;
}

template<class value_type> void string_set<value_type>::resize() const
{
	_label.resize();
	_value.resize();
	_menuicon.resize();
	_units.resize();

	inherited::resize();
}

template<class value_type> void string_set<value_type>::reformat(const point& origin,const box& pbbox)
{
	// Fit bounding box to parent.
	box bbox=fit(pbbox);

	// Update origin and bounding box of this component, force redraw
	// if necessary.  (This must happen before reformat() is called for
	// the children.)
	bool moved=(origin!=this->origin())||(bbox!=this->bbox());
	if (moved) force_redraw(true);
	_bbox=bbox;
	inherited::reformat(origin,bbox);
	if (moved) force_redraw(true);

	box lbbox=_label.min_bbox();
	box vbbox=_value.min_bbox();
	box mbbox=_menuicon.min_bbox();
	box nbbox=_units.min_bbox();

	// The label, down, up and units icons should remain minimum size
	// The value icon should fill all remaining space
	vbbox.xmax(vbbox.xmin()+bbox.xsize()-(lbbox.xsize()+mbbox.xsize()+nbbox.xsize()));

	int offset=bbox.xmin();
	_label.reformat(point(offset,0),lbbox);
	offset+=lbbox.xsize();
	_value.reformat(point(offset,0),vbbox);
	offset+=vbbox.xsize()+8;
	_menuicon.reformat(point(offset,0),mbbox);
	offset+=mbbox.xsize();
	_units.reformat(point(offset,0),nbbox);
}

template<class value_type> void string_set<value_type>::unformat()
{
	_label.unformat();
	_value.unformat();
	_menuicon.unformat();
	_units.unformat();
	inherited::unformat();
}

template<class value_type> void string_set<value_type>::redraw(gcontext& context,const box& clip)
{
	_label.redraw(context,clip);
	_value.redraw(context,clip);
	_menuicon.redraw(context,clip);
	_units.redraw(context,clip);
	inherited::redraw(context,clip);
}

//template<class value_type> string_set<value_type>& string_set<value_type>::snap()
//{
////	if (_value.value() > max()) _value.value(max());
////	if (_value.value() < min()) _value.value(min());
//	_value.snap();
//	return *this;
//}

} /* namespace desktop */
} /* namespace rtk */


#endif

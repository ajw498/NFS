// This file is part of the RISC OS Toolkit (RTK).
// Copyright © 2003-2005 Graham Shaw.
// Distribution and use are subject to the GNU Lesser General Public License,
// a copy of which may be found in the file !RTK.Copyright.

#ifndef _RTK_DESKTOP_BUTTON_ROW_LAYOUT
#define _RTK_DESKTOP_BUTTON_ROW_LAYOUT

#include <vector>

#include "rtk/desktop/sizeable_component.h"

namespace rtk {
namespace desktop {

/** A layout class for arranging components in a row.
 * It is possible to specify the gap to be placed between cells and the
 * margin to be placed around the layout as a whole.  Cells that share
 * the same horizontal baseline are aligned with each other. The layout
 * will expand to fill any extra space made available to it. Note that
 * the algorithm used to do this is implementation-dependent, so may
 * change in future versions of this library.
 */
class button_row_layout:
	public sizeable_component
{
private:
	/** The class from which this one is derived. */
	typedef sizeable_component inherited;
public:
	/** A type for representing cell counts or indices. */
	typedef unsigned int size_type;

	/** A null value for use in place of a cell index. */
	static const size_type npos=static_cast<size_type>(-1);
private:
	/** Vector containing child pointer for each cell. */
	std::vector<component*> _components;

	/** Vector containing position of left edge of each cell with
	 * respect to origin of layout.  The position of the right edge
	 * is obtained by subtracting _xgap from the left edge of the
	 * following cell.  A hypothetical value for _xmin[xcells()] is
	 * included in the vector so that the position of the right edge
	 * of the last cell can be determined. */
	std::vector<int> _xmin;

	/** The cached y-baseline set for the layout. */
	mutable ybaseline_set _ybs;

	/** The size of gap to be placed between cells. */
	int _xgap;

	/** The margin to be placed around the whole layout. */
	box _margin;

	/** The current bounding box. */
	box _bbox;
	mutable int _bordersize;
	mutable int _maxxsize;
public:
	/** Construct row layout.
	 * @param xcells the required number of cells (defaults to 0)
	 */
	button_row_layout(size_type xcells=0);

	/** Destroy row layout. */
	virtual ~button_row_layout();

	virtual box auto_bbox() const;
	virtual component* find(const point& p) const;
	virtual box bbox() const;
	virtual void resize() const;
	virtual void reformat(const point& origin,const box& pbbox);
	virtual void unformat();
	virtual void redraw(gcontext& context,const box& clip);
protected:
	virtual void remove_notify(component& c);
public:
	/** Get number of cells.
	 * @return the number of cells
	 */
	size_type xcells() const
		{ return _components.size(); }

	/** Set number of cells.
	 * @param xcells the required number of cells
	 * @return a reference to this
	 */
	button_row_layout& cells(size_type xcells);

	/** Add component to layout.
	 * If a component already exists at the specified position
	 * then it is replaced.
	 * @param c the component to be added
	 * @param x the cell index (defaults to npos, meaning new cell)
	 * @return a reference to this
	 */
	button_row_layout& add(component& c,size_type x=npos);

	/** Get size of gap between cells.
	 * @return the size of gap between cells
	 */
	int xgap() const
		{ return _xgap; }

	/** Set size of gap between cells.
	 * @param xgap the required gap between cells
	 * @return a reference to this
	 */
	button_row_layout& xgap(int xgap);

	/** Get margin around layout.
	 * @return a box indicating the margin width for each side of the layout
	 */
	box margin() const
		{ return _margin; }

	/** Set margin around layout.
	 * @param margin a box specifying the required margin width for each
	 *  side of the layout
	 * @return a reference to this
	 */
	button_row_layout& margin(const box& margin);

	/** Set margin around layout.
	 * @param margin an integer specifying the required margin width for
	 *  all sides of the layout
	 * @return a reference to this
	 */
	button_row_layout& margin(int margin);
};

} /* namespace desktop */
} /* namespace rtk */

#endif

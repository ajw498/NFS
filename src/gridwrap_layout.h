
#ifndef _GRIDWRAP_LAYOUT
#define _GRIDWRAP_LAYOUT

#include <vector>

#include "rtk/desktop/sizeable_component.h"

using namespace rtk;
using namespace rtk::desktop;

/** A layout class for arranging components in a grid that wraps to fit
 * the available space.
 * It is possible to specify the horizontal or vertical gap to be placed
 * between cells and the margin to be placed around the layout as a whole.
 * Cells that share the same horizontal or vertical baseline are aligned
 * with each other. The layout will expand to fill any horizontal space
 * made available to it, and will alter the number of columns to fit.
 * The components will be arranged in order left to right, top to bottom.
 */
class gridwrap_layout:
	public component
{
private:
	/** The class from which this one is derived. */
	typedef component inherited;
public:
	/** A type for representing cell counts or indices. */
	typedef unsigned int size_type;
private:
	/** The number of cells. */
	size_type _cells;

	/** The number of cells wide */
	size_type _xcells;
	/** The number of cells high */
	size_type _ycells;

	/** Vector containing child pointer for each cell. */
	std::vector<component*> _components;

	/** Cached x-baseline set for all cells. */
	mutable xbaseline_set _xbs;

	/** Cached y-baseline set for all cells. */
	mutable ybaseline_set _ybs;

	/** The valid flag for _xbs and _ybs.
	 * This is a necessary but not a sufficient condition:
	 * size_valid() must be true too.
	 */
	mutable bool _baselines_valid;

	/** The size of gap to be placed between columns. */
	int _xgap;

	/** The size of gap to be placed between rows. */
	int _ygap;

	/** The minimum width of each cell. */
	int _minwidth;

	/** The minimum height of each cell. */
	int _minheight;

	/** The margin to be placed around the whole layout. */
	box _margin;

	/** The current bounding box. */
	box _bbox;

public:
	/** Construct wrapping grid layout.
	 * @param cells the required number of cells (defaults to 0)
	 */
	gridwrap_layout(size_type cells=0);

	/** Destroy grid layout. */
	virtual ~gridwrap_layout();

	virtual box min_bbox() const;
	virtual box min_wrap_bbox(const box& wbox) const;
	box ideal_bbox() const;
	virtual component* find(const point& p) const;
	virtual box bbox() const;
	virtual void resize() const;
	virtual void reformat(const point& origin,const box& pbbox);
	virtual void unformat();
	virtual void redraw(gcontext& context,const box& clip);
	virtual void invalidate();
protected:
	virtual void remove_notify(component& c);
private:
	void update_baselines() const;
	void reflow(box& bbox, bool shrinkx, size_type& xcells, size_type& ycells) const;
public:
	/** Get number of cells.
	 * @return the number of cells
	 */
	size_type cells() const
		{ return _cells; }

	/** Set number of cells.
	 * @param cells the required number of cells
	 * @return a reference to this
	 */
	gridwrap_layout& cells(size_type cells);

	/** Add component to layout.
	 * If a component already exists at the specified position
	 * then it is replaced. If the position is greater than the
	 * number of cells present then the number of cells is
	 * increased as required.
	 * @param c the component to be added
	 * @param i the cell index
	 * @return a reference to this
	 */
	gridwrap_layout& add(component& c,size_type i);

	/** Get size of gap between columns.
	 * @return the size of gap between columns
	 */
	int xgap() const
		{ return _xgap; }

	/** Get size of gap between rows.
	 * @return the size of gap between rows
	 */
	int ygap() const
		{ return _ygap; }

	/** Get minimum width for each cell.
	 * @return the minimum width for cells
	 */
	int min_width() const
		{ return _minwidth; }

	/** Get minimum height for each cell.
	 * @return the minimum height for cells
	 */
	int min_height() const
		{ return _minheight; }

	/** Set size of gap between columns.
	 * @param xgap the required gap between columns
	 * @return a reference to this
	 */
	gridwrap_layout& xgap(int xgap);

	/** Set size of gap between rows.
	 * @param ygap the required gap between rows
	 * @return a reference to this
	 */
	gridwrap_layout& ygap(int ygap);

	/** Set minimum width of each cell.
	 * @param min_width the required minimum width
	 * @return a reference to this
	 */
	gridwrap_layout& min_width(int min_width);

	/** Set minimum height of each cell.
	 * @param min_height the required minimum height
	 * @return a reference to this
	 */
	gridwrap_layout& min_height(int min_height);

	/** Get margin around layout.
	 * @return a box indicating the margin width for each side of the layout
	 */
	const box& margin() const
		{ return _margin; }

	/** Set margin around layout.
	 * @param margin a box specifying the required margin width for each
	 *  side of the layout
	 * @return a reference to this
	 */
	gridwrap_layout& margin(const box& margin);

	/** Set margin around layout.
	 * @param margin an integer specifying the required margin width for
	 *  all sides of the layout
	 * @return a reference to this
	 */
	gridwrap_layout& margin(int margin);
};

#endif

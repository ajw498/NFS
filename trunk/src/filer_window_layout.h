
#ifndef _FILER_WINDOW_LAYOUT
#define _FILER_WINDOW_LAYOUT

#include <vector>

#include "rtk/desktop/sizeable_component.h"

using namespace rtk;
using namespace rtk::desktop;

/** A layout class for arranging components in a grid that wraps to fit
 * the available horizontal space in the parent window.
 * Although the components wrap to fit the width available, the layout
 * remains wide enough to hold all the components in a single row.
 * The class is primarily intended to be used by the filer_window class
 * to implement a filer like window.  
 * It is possible to specify the horizontal or vertical gap to be placed
 * between cells and the margin to be placed around the layout as a whole.
 * Cells that share the same horizontal or vertical baseline are aligned
 * with each other.
 * The components will be arranged in order left to right, top to bottom.
 */
class filer_window_layout:
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
	int _mincellwidth;

	/** The minimum height of each cell. */
	int _mincellheight;

	/** The minimum width of the layout. */
	int _minlayoutwidth;

	/** The margin to be placed around the whole layout. */
	box _margin;

	/** The current bounding box. */
	box _bbox;

public:
	/** Construct filer window layout. */
	filer_window_layout();

	/** Destroy filer window layout. */
	virtual ~filer_window_layout();

	virtual box min_bbox() const;
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
public:
	/** Get the bounding box if the layout were to be arranged in the
	 * ideal size of 4 columns.
	 * @return the ideal bounding box
	 */
	box ideal_bbox() const;

	/** Get number of cells.
	 * @return the number of cells
	 */
	size_type cells() const
		{ return _cells; }

	/** Set number of cells.
	 * @param cells the required number of cells
	 * @return a reference to this
	 */
	filer_window_layout& cells(size_type cells);

	/** Add component to layout.
	 * If a component already exists at the specified position
	 * then it is replaced. If the position is greater than the
	 * number of cells present then the number of cells is
	 * increased as required.
	 * @param c the component to be added
	 * @param i the cell index
	 * @return a reference to this
	 */
	filer_window_layout& add(component& c,size_type i);

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
	int min_cell_width() const
		{ return _mincellwidth; }

	/** Get minimum height for each cell.
	 * @return the minimum height for cells
	 */
	int min_cell_height() const
		{ return _mincellheight; }

	/** Get minimum width for the total layout.
	 * @return the minimum width for the layout
	 */
	int min_layout_width() const
		{ return _minlayoutwidth; }

	/** Set size of gap between columns.
	 * @param xgap the required gap between columns
	 * @return a reference to this
	 */
	filer_window_layout& xgap(int xgap);

	/** Set size of gap between rows.
	 * @param ygap the required gap between rows
	 * @return a reference to this
	 */
	filer_window_layout& ygap(int ygap);

	/** Set minimum width of each cell.
	 * @param width the required minimum width
	 * @return a reference to this
	 */
	filer_window_layout& min_cell_width(int width);

	/** Set minimum height of each cell.
	 * @param height the required minimum height
	 * @return a reference to this
	 */
	filer_window_layout& min_cell_height(int height);

	/** Set minimum width of the total layout.
	 * @param width the required minimum width
	 * @return a reference to this
	 */
	filer_window_layout& min_layout_width(int width);

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
	filer_window_layout& margin(const box& margin);

	/** Set margin around layout.
	 * @param margin an integer specifying the required margin width for
	 *  all sides of the layout
	 * @return a reference to this
	 */
	filer_window_layout& margin(int margin);
};

#endif

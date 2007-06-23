
#include <algorithm>
#include <functional>

#include "rtk/graphics/gcontext.h"
#include "rtk/desktop/basic_window.h"
#include "gridwrap_layout.h"

using std::min;
using std::max;

const int minwidth = 400;

#define FILERWIN

gridwrap_layout::gridwrap_layout(size_type cells):
	_cells(cells),
	_xcells(0),
	_components(cells,0),
	_baselines_valid(false),
	_xgap(0),
	_ygap(0),
	_minwidth(0),
	_minheight(0)
{}

gridwrap_layout::~gridwrap_layout()
{
	for (std::vector<component*>::iterator i=_components.begin();
		i!=_components.end();++i)
	{
		if (component* c=*i) c->remove();
	}
	remove();
}

void gridwrap_layout::invalidate()
{
	_xcells = 0;
	_baselines_valid=false;
	inherited::invalidate();
}

// Cache the baselines of all child components, so the size of a cell can be calculated
void gridwrap_layout::update_baselines() const
{
	_xbs = xbaseline_set();
	_ybs = ybaseline_set();
	for (std::vector<component*>::const_iterator i=_components.begin();
		i!=_components.end();++i)
	{
		if (*i)
		{
			box mcbbox = (*i)->min_bbox();
			_xbs.add(mcbbox, (*i)->xbaseline());
			_ybs.add(mcbbox, (*i)->ybaseline());
		}
	}
	_baselines_valid=true;
}

// Calculate width and height, without any wrapping.
box gridwrap_layout::min_bbox() const
{
	if (!size_valid()) resize();

	if (!_baselines_valid) update_baselines();

	// Calculate size when arranged as a single row
	int xsize = _cells * (_xbs.offset(xbaseline_left,xbaseline_right,_minwidth) + _xgap) - _xgap;
	int ysize = _ybs.offset(ybaseline_bottom,ybaseline_top,_minheight);

	// Give a minimum width
	if (xsize < minwidth) xsize = minwidth;

	// Construct minimum bounding box, with respect to top left-hand
	// corner of layout.
	box abbox(0,-ysize,xsize,0);
	abbox+=_margin;

	// Translate to external origin and return.
	abbox-=external_origin(abbox,xbaseline_left,ybaseline_top);

	return abbox;
}

// Calculate width and height, with wrapping.
box gridwrap_layout::min_wrap_bbox(const box& wbox) const
{
	// Update _baselines_valid.
	if (!size_valid()) resize();

	if (!_baselines_valid) update_baselines();

	size_type xcells, ycells;
	box bbox(wbox);
	reflow(bbox,true,xcells,ycells);
	return bbox;
}

box gridwrap_layout::ideal_bbox() const
{
	if (!size_valid()) resize();

	if (!_baselines_valid) update_baselines();

	// Calculate size when arranged as a single row
	int xcells = _cells;
	if (xcells > 4) xcells = 4;
	int ycells = (_cells + (xcells - 1)) / xcells;

	int xsize = xcells * (_xbs.offset(xbaseline_left,xbaseline_right,_minwidth) + _xgap) - _xgap;
	int ysize = ycells * (_ybs.offset(ybaseline_bottom,ybaseline_top,_minheight) + _ygap) - _ygap;

	// Give a minimum width
	if (xsize < minwidth) xsize = minwidth;

	// Construct minimum bounding box, with respect to top left-hand
	// corner of layout.
	box abbox(0,-ysize,xsize,0);
	abbox+=_margin;

	// Translate to external origin and return.
	abbox-=external_origin(abbox,xbaseline_left,ybaseline_top);

	return abbox;
}

component* gridwrap_layout::find(const point& p) const
{
	// Find the nearest cell
	int xmaxcell = _xbs.offset(xbaseline_left,xbaseline_right,_minwidth);
	int ymaxcell = _ybs.offset(ybaseline_bottom,ybaseline_top,_minheight);
	size_type x = (p.x()-(_bbox.xmin() + _margin.xmin()))/(xmaxcell + _xgap);
	size_type y = -(p.y()-(_bbox.ymax()-_margin.ymax()))/(ymaxcell + _ygap);
	size_type i = x + y * _xcells;
	if (i>=_cells) return 0;

	// See if the point is within that cell
	component* c=_components[i];
	if (!c) return 0;
	box cbbox=c->bbox()+c->origin();
	if (!(p<=cbbox)) return 0;
	return c;
}

box gridwrap_layout::bbox() const
{
	return _bbox;
}

void gridwrap_layout::resize() const
{
	if (!size_valid())
	{
		for (std::vector<component*>::const_iterator i=_components.begin();
			i!=_components.end();++i)
		{
			if (component* c=*i) c->resize();
		}
		// Invalidate the cached baselines
		// (to be recalculated if and when they are needed).
		_baselines_valid=false;
		inherited::resize();
	}
}

// Reflow the grid to fit within bbox. Returns the number of cells wide and
// high, and updated the bbox
void gridwrap_layout::reflow(box& bbox, bool shrinkx, size_type& xcells, size_type& ycells) const
{
	// Remove margin.
	box ibox(bbox-_margin);

	xcells = 1;
	ycells = 1;
	if (_cells == 0) return;

	// Find the maximum number of cells we can fit in the x direction
	xcells = (ibox.xsize() + _xgap) / (_xbs.offset(xbaseline_left,xbaseline_right,_minwidth) + _xgap);

#ifdef FILERWIN
	int excess = (ibox.xsize() + _xgap) - (xcells * (_xbs.offset(xbaseline_left,xbaseline_right,_minwidth) + _xgap));
	if (!shrinkx && ((xcells < _cells) || (excess == 0))) xcells -= 1;
#endif

	if (xcells < 1) xcells = 1;
	if (xcells > _cells) xcells = _cells;

	ycells = (_cells + (xcells - 1)) / xcells;

	// Work out the excess space, and align suitably within
	int xspare = ibox.xsize() - (xcells * _xbs.offset(xbaseline_left,xbaseline_right,_minwidth) + (xcells - 1) * _xgap);
	int yspare = ibox.ysize() - (ycells * _ybs.offset(ybaseline_bottom,ybaseline_top,_minheight) + (ycells - 1) * _ygap);

	switch (xbaseline())
	{
	case xbaseline_left:
		bbox.xmax(bbox.xmax() - xspare);
		break;
	case xbaseline_right:
		bbox.xmin(bbox.xmin() + xspare);
		break;
	case xbaseline_centre:
		bbox.xmin(bbox.xmin() + xspare/2);
		bbox.xmax(bbox.xmax() - xspare/2);
		break;
	default:
		break;
	}

#ifdef FILERWIN
	// Make x size 1 cell wider than needed, to allow the window to expand
	bbox.xmax(bbox.xmax()+_xbs.offset(xbaseline_left,xbaseline_right,_minwidth) + _xgap);
	// Restrict the maximum x size to be enough to hold all cells, plus
	// 1 pixel (to allow detection of when the window is full width)
	int xsize = _margin.xsize() + _cells * (_xbs.offset(xbaseline_left,xbaseline_right,_minwidth) + _xgap) - _xgap + 2;
	// Give a minimum width
	if ((xsize < minwidth) && (ycells < 2)) xsize = minwidth;
	if (bbox.xsize() > xsize) bbox.xmax(bbox.xmin()+xsize);
#endif

	switch (ybaseline())
	{
	case ybaseline_top:
		bbox.ymin(bbox.ymin() + yspare);
		break;
	case ybaseline_bottom:
		bbox.ymax(bbox.ymax() - yspare);
		break;
	case ybaseline_centre:
		bbox.ymax(bbox.ymax() - yspare/2);
		bbox.ymin(bbox.ymin() + yspare/2);
		break;
	default:
		break;
	}
}

void gridwrap_layout::reformat(const point& origin,const box& pbbox)
{
	// Fit bounding box to parent.
	box bbox=pbbox;

	size_type oldxcells=_xcells;
	reflow(bbox,false,_xcells,_ycells);

	// Update origin and bounding box of this component, force redraw
	// if necessary.  (This must happen before reformat() is called for
	// any children.)
	bool moved=(origin!=this->origin())||(_xcells!=oldxcells);
	if (moved) force_redraw(true);
	_bbox=bbox;
	inherited::reformat(origin,bbox);
	if (moved) force_redraw(true);

	// No need to reformat all the children unless the layout has actually changed
	if (_xcells != oldxcells)
	{
		// Place children.
		size_type x=0;
		size_type y=0;
		for (size_type i=0;i<_cells;i++)
		{
			if (component* c=_components[i])
			{
				// Construct bounding box for cell with respect to
				// origin of layout.
				int xmaxcell = _xbs.offset(xbaseline_left,xbaseline_right,_minwidth);
				int ymaxcell = _ybs.offset(ybaseline_bottom,ybaseline_top,_minheight);
				box cbbox(  (xmaxcell + _xgap) * x,
				          - ((ymaxcell + _ygap) * y + ymaxcell),
				            (xmaxcell + _xgap) * (x+1) -_xgap,
				          - (ymaxcell + _ygap) * y);

				cbbox -= _margin.xminymax();

				// Calculate offset from top left-hand corner of cell
				// to origin of child.
				int xoffset=_xbs.offset(xbaseline_left,c->xbaseline(),
					cbbox.xsize());
				int yoffset=_ybs.offset(ybaseline_bottom,c->ybaseline(),
					cbbox.ysize());
				point coffset(xoffset,yoffset);

				// Calculate origin of cell with respect to origin of
				// layout.
				point cpos(cbbox.xminymin()+coffset);

				// Reformat child.
				c->reformat(cpos,cbbox-cpos);
			}

			if (++x >= _xcells)
			{
				x = 0;
				y++;
			}
		}
#ifdef FILERWIN
		// Invalidate the parent to allow the work area to shrink if needed.
		// When this reformat is called again because of this, the _xcells
		// won't have changed, so we won't reformat all children again and
		// get into an infinite loop 
		basic_window* parent=parent_work_area();
		if (parent) parent->invalidate();
#endif
	}
}

void gridwrap_layout::unformat()
{
	for (std::vector<component*>::iterator i=_components.begin();
		i!=_components.end();++i)
	{
		if (component* c=*i) c->unformat();
	}
}

void gridwrap_layout::redraw(gcontext& context,const box& clip)
{
	// Find which cells are within the clip box
	int xmaxcell = _xbs.offset(xbaseline_left,xbaseline_right,_minwidth);
	int ymaxcell = _ybs.offset(ybaseline_bottom,ybaseline_top,_minheight);
	size_type x0 = (clip.xmin()-(_bbox.xmin() + _margin.xmin()))/(xmaxcell + _xgap);
	size_type x1 = (clip.xmax()-(_bbox.xmin() + _margin.xmin()))/(xmaxcell + _xgap);
	size_type y0 = -(clip.ymax()-(_bbox.ymax() - _margin.ymax()))/(ymaxcell + _ygap);
	size_type y1 = -(clip.ymin()-(_bbox.ymax() - _margin.ymax()))/(ymaxcell + _ygap);
	if (x1>=_xcells) x1=_xcells-1;
	if (y1>=_ycells) y1=_ycells-1;

	// Redraw children.
	for (size_type x=x0;x<=x1;x++)
	{
		for (size_type y=y0;y<=y1;y++)
		{
			size_type i = y * _xcells + x;
			if (i<_cells)
			{
				if (component *c=_components[i])
				{
					point cpos(c->origin());
					context+=cpos;
					c->redraw(context,clip-cpos);
					context-=cpos;
				}
			}
		}
	}
	inherited::redraw(context,clip);
}

void gridwrap_layout::remove_notify(component& c)
{
	std::vector<component*>::iterator f=
		std::find(_components.begin(),_components.end(),&c);
	if (f!=_components.end())
	{
		*f=0;
		_xcells = 0;
		invalidate();
	}
}

gridwrap_layout& gridwrap_layout::cells(size_type cells)
{
	for (size_type i=min(cells,_components.size());i!=_components.size();++i)
		if (component* c=_components[i]) c->remove();
	_components.resize(cells,0);
	_cells = cells;
	_xcells = 0;
	invalidate();
	return *this;
}

gridwrap_layout& gridwrap_layout::add(component& c,size_type i)
{
	if (i>=_cells) cells(i+1);

	if (_components[i]) _components[i]->remove();
	_components[i]=&c;
	link_child(c);
	invalidate();
	return *this;
}

gridwrap_layout& gridwrap_layout::xgap(int xgap)
{
	_xgap=xgap;
	invalidate();
	return *this;
}

gridwrap_layout& gridwrap_layout::ygap(int ygap)
{
	_ygap=ygap;
	invalidate();
	return *this;
}

gridwrap_layout& gridwrap_layout::min_width(int min_width)
{
	_minwidth=min_width;
	invalidate();
	return *this;
}

gridwrap_layout& gridwrap_layout::min_height(int min_height)
{
	_minheight=min_height;
	invalidate();
	return *this;
}

gridwrap_layout& gridwrap_layout::margin(const box& margin)
{
	_margin=margin;
	invalidate();
	return *this;
}

gridwrap_layout& gridwrap_layout::margin(int margin)
{
	_margin=box(-margin,-margin,margin,margin);
	invalidate();
	return *this;
}



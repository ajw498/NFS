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

#include <algorithm>
#include <functional>

#include "rtk/graphics/gcontext.h"
#include "rtk/desktop/basic_window.h"
#include "filer_window_layout.h"

filer_window_layout::filer_window_layout():
	_xcells(0),
	_baselines_valid(false),
	_xgap(0),
	_ygap(0),
	_mincellwidth(0),
	_mincellheight(0),
	_minlayoutwidth(0)
{
}

filer_window_layout::~filer_window_layout()
{
	for (std::vector<component*>::iterator i=_components.begin();
		i!=_components.end();++i)
	{
		if (component* c=*i) c->remove();
	}
	remove();
}

void filer_window_layout::invalidate()
{
	_xcells=0;
	_baselines_valid=false;
	inherited::invalidate();
}

// Cache the baselines of all child components, so the size of a cell
// can be calculated
void filer_window_layout::update_baselines() const
{
	_xbs=xbaseline_set();
	_ybs=ybaseline_set();
	for (std::vector<component*>::const_iterator i=_components.begin();
		i!=_components.end();++i)
	{
		if (*i)
		{
			box mcbbox=(*i)->min_bbox();
			_xbs.add(mcbbox, (*i)->xbaseline());
			_ybs.add(mcbbox, (*i)->ybaseline());
		}
	}
	_baselines_valid=true;
}

box filer_window_layout::min_bbox() const
{
	if (!size_valid()) resize();

	if (!_baselines_valid) update_baselines();

	// Calculate maximum x size when arranged as a single row
	// This ensures that the parent can expand to the full width
	int xsize = _cells * (_xbs.offset(xbaseline_left,xbaseline_right,_mincellwidth) + _xgap) - _xgap;
	// Calculate y size based on height when wrapped
	int ysize = _ycells * (_ybs.offset(ybaseline_bottom,ybaseline_top,_mincellheight) + _ygap) - _ygap;

	// Increase to meet the minimum width
	if (xsize < _minlayoutwidth) xsize = _minlayoutwidth;

	// Construct minimum bounding box, with respect to top left-hand
	// corner of layout.
	box abbox(0,-ysize,xsize,0);
	abbox+=_margin;

	// Translate to external origin and return.
	abbox-=external_origin(abbox,xbaseline_left,ybaseline_top);

	return abbox;
}

box filer_window_layout::ideal_bbox() const
{
	if (!size_valid()) resize();

	if (!_baselines_valid) update_baselines();

	// Calculate size when arranged as 4 columns
	int xcells=_cells;
	if (xcells>4) xcells = 4;
	int ycells=(_cells+(xcells-1))/xcells;

	int xsize = xcells*(_xbs.offset(xbaseline_left,xbaseline_right,_mincellwidth)+_xgap)-_xgap;
	int ysize = ycells*(_ybs.offset(ybaseline_bottom,ybaseline_top,_mincellheight)+_ygap)-_ygap;

	// Give a minimum width
	if (xsize<_minlayoutwidth) xsize=_minlayoutwidth;

	// Construct minimum bounding box, with respect to top left-hand
	// corner of layout.
	box abbox(0,-ysize,xsize,0);
	abbox+=_margin;

	// Translate to external origin and return.
	abbox-=external_origin(abbox,xbaseline_left,ybaseline_top);

	return abbox;
}

component* filer_window_layout::find(const point& p) const
{
	// Find the nearest cell
	int xmaxcell = _xbs.offset(xbaseline_left,xbaseline_right,_mincellwidth);
	int ymaxcell = _ybs.offset(ybaseline_bottom,ybaseline_top,_mincellheight);
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

box filer_window_layout::bbox() const
{
	return _bbox;
}

void filer_window_layout::resize() const
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

void filer_window_layout::reformat(const point& origin,const box& pbbox)
{
	size_type oldxcells=_xcells;

	// Get parent bbox, so we can wrap to fit within
	box ibox;
	basic_window* parent=parent_work_area();
	if (parent) ibox=parent->bbox();

	// Remove margin.
	ibox-=_margin;

	// Find the maximum number of cells we can fit in the x direction
	_xcells=(ibox.xsize()+_xgap)/(_xbs.offset(xbaseline_left,xbaseline_right,_mincellwidth)+_xgap);
	if (_xcells>_cells) _xcells=_cells;
	if (_xcells<1) _xcells=1;

	// Find the number of rows needed for the calculated width
	_ycells=(_cells+(_xcells-1))/_xcells;
	if (_ycells<1) _ycells=1;

	box bbox=min_bbox();

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
				int xmaxcell = _xbs.offset(xbaseline_left,xbaseline_right,_mincellwidth);
				int ymaxcell = _ybs.offset(ybaseline_bottom,ybaseline_top,_mincellheight);
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

			if (++x>=_xcells)
			{
				x=0;
				y++;
			}
		}

		// Invalidate the parent to allow the work area to shrink if needed.
		// When this reformat is called again because of this, the _xcells
		// won't have changed, so we won't reformat all children again and
		// get into an infinite loop 
		basic_window* parent=parent_work_area();
		if (parent) parent->invalidate();
	}
}

void filer_window_layout::unformat()
{
	for (std::vector<component*>::iterator i=_components.begin();
		i!=_components.end();++i)
	{
		if (component* c=*i) c->unformat();
	}
}

void filer_window_layout::redraw(gcontext& context,const box& clip)
{
	// Find which cells are within the clip box
	int xmaxcell = _xbs.offset(xbaseline_left,xbaseline_right,_mincellwidth);
	int ymaxcell = _ybs.offset(ybaseline_bottom,ybaseline_top,_mincellheight);
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

void filer_window_layout::remove_notify(component& c)
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

filer_window_layout& filer_window_layout::cells(size_type cells)
{
	for (size_type i=std::min(cells,_components.size());i!=_components.size();++i)
		if (component* c=_components[i]) c->remove();
	_components.resize(cells,0);
	_cells = cells;
	_xcells = 0;
	invalidate();
	return *this;
}

filer_window_layout& filer_window_layout::add(component& c,size_type i)
{
	if (i>=_cells) cells(i+1);

	if (_components[i]) _components[i]->remove();
	_components[i]=&c;
	link_child(c);
	invalidate();
	return *this;
}

filer_window_layout& filer_window_layout::xgap(int xgap)
{
	_xgap=xgap;
	invalidate();
	return *this;
}

filer_window_layout& filer_window_layout::ygap(int ygap)
{
	_ygap=ygap;
	invalidate();
	return *this;
}

filer_window_layout& filer_window_layout::min_cell_width(int width)
{
	_mincellwidth=width;
	invalidate();
	return *this;
}

filer_window_layout& filer_window_layout::min_cell_height(int height)
{
	_mincellheight=height;
	invalidate();
	return *this;
}

filer_window_layout& filer_window_layout::min_layout_width(int width)
{
	_minlayoutwidth=width;
	invalidate();
	return *this;
}

filer_window_layout& filer_window_layout::margin(const box& margin)
{
	_margin=margin;
	invalidate();
	return *this;
}

filer_window_layout& filer_window_layout::margin(int margin)
{
	_margin=box(-margin,-margin,margin,margin);
	invalidate();
	return *this;
}



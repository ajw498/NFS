// This file is part of the RISC OS Toolkit (RTK).
// Copyright © 2003-2005 Graham Shaw.
// Distribution and use are subject to the GNU Lesser General Public License,
// a copy of which may be found in the file !RTK.Copyright.

#include <algorithm>
#include <functional>

#include "rtk/util/divider.h"
#include "rtk/graphics/gcontext.h"
#include "rtk/desktop/icon.h"
#include "button_row_layout.h"

namespace rtk {
namespace desktop {

using std::min;
using std::max;
using rtk::util::divider;

button_row_layout::button_row_layout(size_type xcells):
	_components(xcells,0),
	_xmin(xcells+1,0),
	_xgap(0),
	_bordersize(0),
	_maxxsize(0)
{}

button_row_layout::~button_row_layout()
{
	for (std::vector<component*>::iterator i=_components.begin();
		i!=_components.end();++i)
	{
		if (component* c=*i) c->remove();
	}
	remove();
}

box button_row_layout::auto_bbox() const
{
	// Determine number of cells.
	size_type xcells=_components.size();

	// Reset y-baseline set.
	_ybs=ybaseline_set();

	// Request min_bbox for each cell.  Incorporate into y-baseline set
	// and total width.
	_bordersize=0;
	_maxxsize=0;
	std::vector<component*>::const_iterator i=_components.begin();
	for (size_type x=0;x!=xcells;++x)
	{
		if (component* c=*i++)
		{
			box mcbbox=c->min_bbox();
			_ybs.add(mcbbox,c->ybaseline());
			int xsize;
			if (icon *i=dynamic_cast<icon*>(c))
			{
				xsize=i->content_box().xsize();
				_bordersize+=i->border_box().xsize();
			}
			else
			{
				xsize=mcbbox.xsize();
			}
			if (xsize>_maxxsize) _maxxsize=xsize;
		}
	}

	// Calculate combined height.
	int ysize=_ybs.ysize();

	int xsize=_maxxsize*xcells+_bordersize;

	// Add gaps to total width.
	if (xcells) xsize+=(xcells-1)*_xgap;

	// Add margin to width and height.
	xsize+=_margin.xsize();
	ysize+=_margin.ysize();

	// Construct minimum bounding box, with respect to top left-hand
	// corner of layout.
	box abbox(0,-ysize,xsize,0);

	// Translate to external origin and return.
	abbox-=external_origin(abbox,xbaseline_left,ybaseline_top);
	return abbox;
}

component* button_row_layout::find(const point& p) const
{
	std::vector<int>::const_iterator xf=
		lower_bound(_xmin.begin(),_xmin.end(),p.x()+1,std::less<int>());
	if (xf==_xmin.begin()) return 0;
	size_type x=(xf-_xmin.begin())-1;
	component* c=_components[x];
	if (!c) return 0;
	box cbbox=c->bbox()+c->origin();
	if (!(p<=cbbox)) return 0;
	return c;
}

box button_row_layout::bbox() const
{
	return _bbox;
}

void button_row_layout::resize() const
{
	for (std::vector<component*>::const_iterator i=_components.begin();
		i!=_components.end();++i)
	{
		if (component* c=*i) c->resize();
	}
	inherited::resize();
}

void button_row_layout::reformat(const point& origin,const box& pbbox)
{
	// Fit bounding box to parent.
	box bbox=fit(pbbox);

	// Determine number of cells.
	size_type xcells=_components.size();

	// Update origin and bounding box of this component, force redraw
	// if necessary.  (This must happen before reformat() is called for
	// any children.)
	bool moved=(origin!=this->origin())||(bbox!=this->bbox());
	if (moved) force_redraw(true);
	_bbox=bbox;
	inherited::reformat(origin,bbox);
	if (moved) force_redraw(true);

	// Remove margin.
	box ibox(_bbox-_margin);

//	box abbox=auto_bbox();

	// Calculate excess.
//	int xexcess=_bbox.xsize()-abbox.xsize();
//	if (xexcess<0) xexcess=0;
//	int xspread=abbox.xsize();
//	xspread-=_margin.xsize();
//	if (xcells) xspread-=(xcells-1)*_xgap;
//	if (xspread<0) xspread=0;
//	divider xdiv(xexcess,xspread);

//FIXME xcells==0?
	divider xdiv(ibox.xsize()+_xgap-_bordersize, xcells);

	// Set minimum x-coordinate for each cell.
	int xpos=ibox.xmin();
	_xmin[0]=xpos;
	for (size_type x=0;x!=xcells;++x)
	{
		int border=0;
		if (component* c=_components[x])
		{
			if (icon *i=dynamic_cast<icon*>(c))
			{
				border=i->border_box().xsize();
			}
		}
		xpos+=xdiv(1)+border;
		_xmin[x+1]=xpos;
	}

	// Place children.
	for (size_type x=0;x!=xcells;++x)
	{
		if (component* c=_components[x])
		{
			// Create x-baseline set for just this cell.
			xbaseline_set xbs;
			xbs.add(c->min_bbox(),c->xbaseline());

			// Construct bounding box for cell with respect to origin
			// of layout.
			box cbbox(_xmin[x],ibox.ymin(),_xmin[x+1]-_xgap,ibox.ymax());

			// Calculate offset from top left-hand corner of cell to
			// origin of child.
			int xoffset=xbs.offset(xbaseline_left,
				c->xbaseline(),cbbox.xsize());
			int yoffset=_ybs.offset(ybaseline_bottom,
				c->ybaseline(),cbbox.ysize());
			point coffset(xoffset,yoffset);

			// Calculate origin of cell with respect to origin of layout.
			point cpos(cbbox.xminymin()+coffset);

			// Reformat child.
			c->reformat(cpos,cbbox-cpos);
		}
	}
}

void button_row_layout::unformat()
{
	for (std::vector<component*>::iterator i=_components.begin();
		i!=_components.end();++i)
	{
		if (component* c=*i) c->unformat();
	}
}

void button_row_layout::redraw(gcontext& context,const box& clip)
{
	// Look for the first column with a right edge which overlaps (or is to
	// the right of) the clip box: _xmin[x0+1] -_xgap > clip.xmin().
	std::vector<int>::iterator xf0=upper_bound(
		_xmin.begin(),_xmin.end(),clip.xmin()+_xgap,std::less<int>());
	size_type x0=xf0-_xmin.begin();
	if (x0) --x0;

	// Look for the first column with a left edge which is to the right of
	// the clip box: _xmin[x1] >= clip.xmax().
	std::vector<int>::iterator xf1=lower_bound(
		_xmin.begin(),_xmin.end(),clip.xmax(),std::less<int>());
	size_type x1=xf1-_xmin.begin();
	if (x1>_components.size()) x1=_components.size();

	// Redraw children.
	// For safety, use an inequality in the for-loop.
	for (size_type x=x0;x<x1;++x)
	{
		if (component* c=_components[x])
		{
			point cpos=c->origin();
			context+=cpos;
			c->redraw(context,clip-cpos);
			context-=cpos;
		}
	}
	inherited::redraw(context,clip);
}

void button_row_layout::remove_notify(component& c)
{
	std::vector<component*>::iterator f=
		std::find(_components.begin(),_components.end(),&c);
	if (f!=_components.end())
	{
		*f=0;
		invalidate();
	}
}

button_row_layout& button_row_layout::cells(size_type xcells)
{
	for (size_type i=min(xcells,_components.size());i!=_components.size();++i)
		if (component* c=_components[i]) c->remove();
	_components.resize(xcells,0);
	_xmin.resize(xcells+1,0);
	invalidate();
	return *this;
}

button_row_layout& button_row_layout::add(component& c,size_type x)
{
	if (x==npos) x=_components.size();
	if (x>=_components.size()) cells(x+1);
	if (_components[x]) _components[x]->remove();
	_components[x]=&c;
	link_child(c);
	invalidate();
	return *this;
}

button_row_layout& button_row_layout::xgap(int xgap)
{
	_xgap=xgap;
	invalidate();
	return *this;
}

button_row_layout& button_row_layout::margin(const box& margin)
{
	_margin=margin;
	invalidate();
	return *this;
}

button_row_layout& button_row_layout::margin(int margin)
{
	_margin=box(-margin,-margin,margin,margin);
	invalidate();
	return *this;
}

} /* namespace desktop */
} /* namespace rtk */

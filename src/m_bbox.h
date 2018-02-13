//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	  Nil.
//	  
//-----------------------------------------------------------------------------

#ifndef __M_BBOX_H__
#define __M_BBOX_H__

#include <float.h>
#include "vectors.h"
#include "m_fixed.h"

struct line_t;
struct node_t;

class FBoundingBox
{
public:
	FBoundingBox()
	{
		ClearBox();
	}

	FBoundingBox(double left, double bottom, double right, double top)
	{
		m_Box[BOXTOP] = top;
		m_Box[BOXLEFT] = left;
		m_Box[BOXRIGHT] = right;
		m_Box[BOXBOTTOM] = bottom;
	}

	FBoundingBox(double x, double y, double radius)
	{
		setBox(x, y, radius);
	}


	void setBox(double x, double y, double radius)
	{
		m_Box[BOXTOP] = y + radius;
		m_Box[BOXLEFT] = x - radius;
		m_Box[BOXRIGHT] = x + radius;
		m_Box[BOXBOTTOM] = y - radius;
	}

	void ClearBox ()
	{
		m_Box[BOXTOP] = m_Box[BOXRIGHT] = -FLT_MAX;
		m_Box[BOXBOTTOM] = m_Box[BOXLEFT] = FLT_MAX;
	}

	// Returns a bounding box that encloses both bounding boxes
	FBoundingBox operator | (const FBoundingBox &box2) const
	{
		return FBoundingBox(m_Box[BOXLEFT] < box2.m_Box[BOXLEFT] ? m_Box[BOXLEFT] : box2.m_Box[BOXLEFT],
							m_Box[BOXBOTTOM] < box2.m_Box[BOXBOTTOM] ? m_Box[BOXBOTTOM] : box2.m_Box[BOXBOTTOM],
							m_Box[BOXRIGHT] > box2.m_Box[BOXRIGHT] ? m_Box[BOXRIGHT] : box2.m_Box[BOXRIGHT],
							m_Box[BOXTOP] > box2.m_Box[BOXTOP] ? m_Box[BOXTOP] : box2.m_Box[BOXTOP]);
	}

	void AddToBox(const DVector2 &pos);

	inline double Top () const { return m_Box[BOXTOP]; }
	inline double Bottom () const { return m_Box[BOXBOTTOM]; }
	inline double Left () const { return m_Box[BOXLEFT]; }
	inline double Right () const { return m_Box[BOXRIGHT]; }

	bool inRange(const line_t *ld) const;

	int BoxOnLineSide (const line_t *ld) const;

	void Set(int index, double value) {m_Box[index] = value;}

protected:
	double m_Box[4];
};


#endif //__M_BBOX_H__

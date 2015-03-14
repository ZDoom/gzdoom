// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//	  Nil.
//	  
//-----------------------------------------------------------------------------

#ifndef __M_BBOX_H__
#define __M_BBOX_H__

#include "doomtype.h"

struct line_t;
struct node_t;

class FBoundingBox
{
public:
	FBoundingBox()
	{
		ClearBox();
	}

	FBoundingBox(fixed_t left, fixed_t bottom, fixed_t right, fixed_t top)
	{
		m_Box[BOXTOP] = top;
		m_Box[BOXLEFT] = left;
		m_Box[BOXRIGHT] = right;
		m_Box[BOXBOTTOM] = bottom;
	}

	FBoundingBox(fixed_t x, fixed_t y, fixed_t radius);

	void ClearBox ()
	{
		m_Box[BOXTOP] = m_Box[BOXRIGHT] = FIXED_MIN;
		m_Box[BOXBOTTOM] = m_Box[BOXLEFT] = FIXED_MAX;
	}

	// Returns a bounding box that encloses both bounding boxes
	FBoundingBox operator | (const FBoundingBox &box2) const
	{
		return FBoundingBox(m_Box[BOXLEFT] < box2.m_Box[BOXLEFT] ? m_Box[BOXLEFT] : box2.m_Box[BOXLEFT],
							m_Box[BOXBOTTOM] < box2.m_Box[BOXBOTTOM] ? m_Box[BOXBOTTOM] : box2.m_Box[BOXBOTTOM],
							m_Box[BOXRIGHT] > box2.m_Box[BOXRIGHT] ? m_Box[BOXRIGHT] : box2.m_Box[BOXRIGHT],
							m_Box[BOXTOP] > box2.m_Box[BOXTOP] ? m_Box[BOXTOP] : box2.m_Box[BOXTOP]);
	}

	void AddToBox (fixed_t x, fixed_t y);

	inline fixed_t Top () const { return m_Box[BOXTOP]; }
	inline fixed_t Bottom () const { return m_Box[BOXBOTTOM]; }
	inline fixed_t Left () const { return m_Box[BOXLEFT]; }
	inline fixed_t Right () const { return m_Box[BOXRIGHT]; }

	int BoxOnLineSide (const line_t *ld) const;

	void Set(int index, fixed_t value) {m_Box[index] = value;}

protected:
	fixed_t m_Box[4];
};


#endif //__M_BBOX_H__

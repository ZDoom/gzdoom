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
// $Log:$
//
// DESCRIPTION:
//		Main loop menu stuff.
//		Random number LUT.
//		Default Config File.
//		PCX Screenshots.
//
//-----------------------------------------------------------------------------

#include "m_bbox.h"
#include "p_local.h"

//==========================================================================
//
//
//
//==========================================================================

FBoundingBox::FBoundingBox(fixed_t x, fixed_t y, fixed_t radius)
{
	m_Box[BOXTOP] = (fixed_t)MIN<SQWORD>((SQWORD)y + radius, FIXED_MAX);
	m_Box[BOXLEFT] = (fixed_t)MAX<SQWORD>((SQWORD)x - radius, FIXED_MIN);
	m_Box[BOXRIGHT] = (fixed_t)MIN<SQWORD>((SQWORD)x + radius, FIXED_MAX);
	m_Box[BOXBOTTOM] = (fixed_t)MAX<SQWORD>((SQWORD)y - radius, FIXED_MIN);
}


//==========================================================================
//
//
//
//==========================================================================

void FBoundingBox::AddToBox (fixed_t x, fixed_t y)
{
	if (x < m_Box[BOXLEFT])
		m_Box[BOXLEFT] = x;
	if (x > m_Box[BOXRIGHT])
		m_Box[BOXRIGHT] = x;

	if (y < m_Box[BOXBOTTOM])
		m_Box[BOXBOTTOM] = y;
	if (y > m_Box[BOXTOP])
		m_Box[BOXTOP] = y;
}

//==========================================================================
//
// FBoundingBox :: BoxOnLineSide
//
// Considers the line to be infinite
// Returns side 0 or 1, -1 if box crosses the line.
//
//==========================================================================

int FBoundingBox::BoxOnLineSide (const line_t *ld) const
{
	int p1;
	int p2;
		
	if (ld->dx == 0)
	{ // ST_VERTICAL
		p1 = m_Box[BOXRIGHT] < ld->v1->x;
		p2 = m_Box[BOXLEFT] < ld->v1->x;
		if (ld->dy < 0)
		{
			p1 ^= 1;
			p2 ^= 1;
		}
	}
	else if (ld->dy == 0)
	{ // ST_HORIZONTAL:
		p1 = m_Box[BOXTOP] > ld->v1->y;
		p2 = m_Box[BOXBOTTOM] > ld->v1->y;
		if (ld->dx < 0)
		{
			p1 ^= 1;
			p2 ^= 1;
		}
	}
	else if ((ld->dy ^ ld->dx) >= 0)
	{ // ST_POSITIVE:
		p1 = P_PointOnLineSide (m_Box[BOXLEFT], m_Box[BOXTOP], ld);
		p2 = P_PointOnLineSide (m_Box[BOXRIGHT], m_Box[BOXBOTTOM], ld);
	}
	else
	{ // ST_NEGATIVE:
		p1 = P_PointOnLineSide (m_Box[BOXRIGHT], m_Box[BOXTOP], ld);
		p2 = P_PointOnLineSide (m_Box[BOXLEFT], m_Box[BOXBOTTOM], ld);
	}

	return (p1 == p2) ? p1 : -1;
}


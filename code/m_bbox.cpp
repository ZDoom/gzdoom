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

IMPLEMENT_CLASS (DBoundingBox)

DBoundingBox::DBoundingBox ()
{
	ClearBox ();
}

void DBoundingBox::ClearBox ()
{
	m_Box[BOXTOP] = m_Box[BOXRIGHT] = MININT;
	m_Box[BOXBOTTOM] = m_Box[BOXLEFT] = MAXINT;
}

void DBoundingBox::AddToBox (fixed_t x, fixed_t y)
{
	if (x < m_Box[BOXLEFT])
		m_Box[BOXLEFT] = x;
	else if (x > m_Box[BOXRIGHT])
		m_Box[BOXRIGHT] = x;

	if (y < m_Box[BOXBOTTOM])
		m_Box[BOXBOTTOM] = y;
	else if (y > m_Box[BOXTOP])
		m_Box[BOXTOP] = y;
}






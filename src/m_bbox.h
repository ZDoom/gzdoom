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
#include "m_fixed.h"


// Bounding box coordinate storage.
enum
{
	BOXTOP,
	BOXBOTTOM,
	BOXLEFT,
	BOXRIGHT
};		// bbox coordinates


class FBoundingBox
{
public:
	FBoundingBox();

	void ClearBox ();
	void AddToBox (fixed_t x, fixed_t y);

	inline fixed_t Top () { return m_Box[BOXTOP]; }
	inline fixed_t Bottom () { return m_Box[BOXBOTTOM]; }
	inline fixed_t Left () { return m_Box[BOXLEFT]; }
	inline fixed_t Right () { return m_Box[BOXRIGHT]; }

protected:
	fixed_t m_Box[4];
};


#endif //__M_BBOX_H__

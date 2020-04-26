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
//		bounding box class
//
//-----------------------------------------------------------------------------

#include "m_bbox.h"
#include "p_local.h"
#include "p_maputl.h"

//==========================================================================
//
//
//
//==========================================================================

void FBoundingBox::AddToBox (const DVector2 &pos)
{
	if (pos.X < m_Box[BOXLEFT])
		m_Box[BOXLEFT] = pos.X;
	if (pos.X > m_Box[BOXRIGHT])
		m_Box[BOXRIGHT] = pos.X;

	if (pos.Y < m_Box[BOXBOTTOM])
		m_Box[BOXBOTTOM] = pos.Y;
	if (pos.Y > m_Box[BOXTOP])
		m_Box[BOXTOP] = pos.Y;
}


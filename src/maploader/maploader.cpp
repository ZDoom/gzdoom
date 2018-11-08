//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2018 Christoph Oelckers
// Copyright 2010 James Haley
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
//		Map loader
//

#include "p_setup.h"
#include "maploader.h"
#include "files.h"
#include "i_system.h"


//===========================================================================
//
// P_LoadVertexes
//
//===========================================================================

void MapLoader::LoadVertexes (MapData * map)
{
	// Determine number of vertices:
	//	total lump length / vertex record length.
	unsigned numvertexes = map->Size(ML_VERTEXES) / sizeof(mapvertex_t);

	if (numvertexes == 0)
	{
		I_Error ("Map has no vertices.\n");
	}

	// Allocate memory for buffer.
	vertexes.Alloc(numvertexes);

	auto &fr = map->Reader(ML_VERTEXES);
		
	// Copy and convert vertex coordinates, internal representation as fixed.
	for (auto &v : vertexes)
	{
		int16_t x = fr.ReadInt16();
		int16_t y = fr.ReadInt16();

		v.set(double(x), double(y));
	}
}


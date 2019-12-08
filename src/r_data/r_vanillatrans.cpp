// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2017 Rachael Alexanderson
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** r_vanillatrans.cpp
** Figures out whether to turn off transparency for certain native game objects
**
**/

#include "templates.h"
#include "c_cvars.h"
#include "w_wad.h"
#include "doomtype.h"
#ifdef _DEBUG
#include "c_dispatch.h"
#endif

bool r_UseVanillaTransparency;
CVAR (Int, r_vanillatrans, 0, CVAR_ARCHIVE)

namespace
{
	bool firstTime = true;
	bool foundDehacked = false;
	bool foundDecorate = false;
	bool foundZScript = false;
}
#ifdef _DEBUG
CCMD (debug_checklumps)
{
	Printf("firstTime: %d\n", firstTime);
	Printf("foundDehacked: %d\n", foundDehacked);
	Printf("foundDecorate: %d\n", foundDecorate);
	Printf("foundZScript: %d\n", foundZScript);
}
#endif

void UpdateVanillaTransparency()
{
	firstTime = true;
}

bool UseVanillaTransparency()
{
	if (firstTime)
	{
		int lastlump = 0;
		Wads.FindLump("ZSCRIPT", &lastlump); // ignore first ZScript
		if (Wads.FindLump("ZSCRIPT", &lastlump) == -1) // no loaded ZScript
		{
			lastlump = 0;
			foundDehacked = Wads.FindLump("DEHACKED", &lastlump) != -1;
			lastlump = 0;
			foundDecorate = Wads.FindLump("DECORATE", &lastlump) != -1;
			foundZScript = false;
		}
		else
		{
			foundZScript = true;
			foundDehacked = false;
			foundDecorate = false;
		}
		firstTime = false;
	}

	switch (r_vanillatrans)
	{
		case 0: return false;
		case 1: return true;
		default:
		if (foundDehacked)
			return true;
		if (foundDecorate)
			return false;
		return r_vanillatrans == 3;
	}
}

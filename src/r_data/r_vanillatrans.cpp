/* 
**---------------------------------------------------------------------------
**
** Copyright(C) 2017 Rachael Alexanderson
** All rights reserved.
**
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/
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

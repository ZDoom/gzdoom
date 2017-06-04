
#include "templates.h"
#include "c_cvars.h"
#include "w_wad.h"
#ifdef _DEBUG
#include "c_dispatch.h"
#endif

CVAR (Int, r_vanillatrans, 2, CVAR_ARCHIVE)

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

#pragma once

#include "v_palette.h"
#include "templates.h"

struct Colormap;

int hw_CalcLightLevel(int lightlevel, int rellight, bool weapon);
PalEntry hw_CalcLightColor(int light, PalEntry pe, int blendfactor);
float hw_GetFogDensity(int lightlevel, PalEntry fogcolor, int sectorfogdensity);
bool hw_CheckFog(sector_t *frontsector, sector_t *backsector);

inline int hw_ClampLight(int lightlevel)
{
	return clamp(lightlevel, 0, 255);
}


#pragma once

#include "c_cvars.h"
#include "v_palette.h"
#include "templates.h"
#include "r_utility.h"

struct Colormap;

int hw_CalcLightLevel(int lightlevel, int rellight, bool weapon, int blendfactor);
PalEntry hw_CalcLightColor(int light, PalEntry pe, int blendfactor);
float hw_GetFogDensity(int lightlevel, PalEntry fogcolor, int sectorfogdensity, int blendfactor);
bool hw_CheckFog(sector_t *frontsector, sector_t *backsector);

inline int hw_ClampLight(int lightlevel)
{
	return clamp(lightlevel, 0, 255);
}

EXTERN_CVAR(Int, gl_weaponlight);

inline	int getExtraLight()
{
	return r_viewpoint.extralight * gl_weaponlight;
}


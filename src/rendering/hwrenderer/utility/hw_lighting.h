#pragma once

#include "c_cvars.h"
#include "v_palette.h"
#include "templates.h"
#include "r_utility.h"

struct Colormap;

inline int hw_ClampLight(int lightlevel)
{
	return clamp(lightlevel, 0, 255);
}

EXTERN_CVAR(Int, gl_weaponlight);

inline	int getExtraLight()
{
	return r_viewpoint.extralight * gl_weaponlight;
}


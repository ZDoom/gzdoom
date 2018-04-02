
#ifndef __GLC_DATA_H
#define __GLC_DATA_H

#include "doomtype.h"
#include "vectors.h"
#include "r_utility.h"

#include "r_defs.h"
#include "a_sharedglobal.h"
#include "c_cvars.h"

EXTERN_CVAR(Int, gl_weaponlight);

inline	int getExtraLight()
{
	return r_viewpoint.extralight * gl_weaponlight;
}

#endif

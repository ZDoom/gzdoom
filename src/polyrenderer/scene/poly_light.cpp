/*
**  Polygon Doom software renderer
**  Copyright (c) 2016 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include <stdlib.h>
#include "templates.h"
#include "doomdef.h"
#include "sbar.h"
#include "poly_light.h"
#include "polyrenderer/poly_renderer.h"

void PolyLightVisibility::SetVisibility(FViewWindow &viewwindow, float vis)
{
	GlobVis = R_GetGlobVis(viewwindow, vis);
}

fixed_t PolyLightVisibility::LightLevelToShade(int lightlevel, bool foggy)
{
	bool nolightfade = !foggy && ((PolyRenderer::Instance()->Level->flags3 & LEVEL3_NOLIGHTFADE));
	if (nolightfade)
	{
		return (MAX(255 - lightlevel, 0) * NUMCOLORMAPS) << (FRACBITS - 8);
	}
	else
	{
		// Convert a light level into an unbounded colormap index (shade). Result is
		// fixed point. Why the +12? I wish I knew, but experimentation indicates it
		// is necessary in order to best reproduce Doom's original lighting.
		return (NUMCOLORMAPS * 2 * FRACUNIT) - ((lightlevel + 12) * (FRACUNIT*NUMCOLORMAPS / 128));
	}
}

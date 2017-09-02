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

#pragma once

#include "swrenderer/scene/r_light.h"

struct FViewWindow;

// Keep using the software renderer's camera light class, for now.
// The DFrameBuffer abstraction relies on this being globally shared
typedef swrenderer::CameraLight PolyCameraLight;

class PolyLightVisibility
{
public:
	void SetVisibility(FViewWindow &viewwindow, float vis);

	double WallGlobVis(bool foggy) const { return (NoLightFade && !foggy) ? 0.0 : GlobVis; }
	double SpriteGlobVis(bool foggy) const { return (NoLightFade && !foggy) ? 0.0 : GlobVis; }
	double ParticleGlobVis(bool foggy) const { return (NoLightFade && !foggy) ? 0.0 : GlobVis * 0.5; }

	// The vis value to pass into the GETPALOOKUP or LIGHTSCALE macros
	double WallVis(double screenZ, bool foggy) const { return WallGlobVis(foggy) / screenZ; }
	double SpriteVis(double screenZ, bool foggy) const { return SpriteGlobVis(foggy) / screenZ; }
	double ParticleVis(double screenZ, bool foggy) const { return ParticleGlobVis(foggy) / screenZ; }

	static fixed_t LightLevelToShade(int lightlevel, bool foggy);

private:
	double GlobVis = 0.0f;
	bool NoLightFade = false;
};

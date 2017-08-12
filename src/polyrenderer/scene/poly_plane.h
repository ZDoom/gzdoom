/*
**  Handling drawing a plane (ceiling, floor)
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

#include "polyrenderer/drawers/poly_triangle.h"

class PolyDrawSectorPortal;
class PolyCull;

class RenderPolyPlane
{
public:
	static void RenderPlanes(const TriMatrix &worldToClip, const PolyClipPlane &clipPlane, PolyCull &cull, subsector_t *sub, uint32_t stencilValue, double skyCeilingHeight, double skyFloorHeight, std::vector<std::unique_ptr<PolyDrawSectorPortal>> &sectorPortals);

private:
	struct UVTransform
	{
		UVTransform(const FTransform &transform, FTexture *tex);

		float GetU(float x, float y) const;
		float GetV(float x, float y) const;

		float xscale;
		float yscale;
		float cosine;
		float sine;
		float xOffs, yOffs;
	};

	void Render3DFloor(const TriMatrix &worldToClip, const PolyClipPlane &clipPlane, subsector_t *sub, uint32_t stencilValue, bool ceiling, F3DFloor *fakefloor);
	void Render(const TriMatrix &worldToClip, const PolyClipPlane &clipPlane, PolyCull &cull, subsector_t *sub, uint32_t stencilValue, bool ceiling, double skyHeight, std::vector<std::unique_ptr<PolyDrawSectorPortal>> &sectorPortals);
	TriVertex PlaneVertex(vertex_t *v1, double height, const UVTransform &transform);
};

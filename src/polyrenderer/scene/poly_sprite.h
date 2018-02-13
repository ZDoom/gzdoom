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

#include "polyrenderer/drawers/poly_triangle.h"

class RenderPolySprite
{
public:
	void Render(PolyRenderThread *thread, const TriMatrix &worldToClip, const PolyClipPlane &clipPlane, AActor *thing, subsector_t *sub, uint32_t stencilValue, float t1, float t2);

	static bool GetLine(AActor *thing, DVector2 &left, DVector2 &right);
	static bool IsThingCulled(AActor *thing);
	static FTexture *GetSpriteTexture(AActor *thing, /*out*/ bool &flipX);

private:
	static double PerformSpriteClipAdjustment(AActor *thing, const DVector2 &thingpos, double spriteheight, double z);
	static double GetSpriteFloorZ(AActor *thing, const DVector2 &thingpos);
	static double GetSpriteCeilingZ(AActor *thing, const DVector2 &thingpos);
	static void SetDynlight(AActor *thing, PolyDrawArgs &args);
};

class PolyTranslucentThing : public PolyTranslucentObject
{
public:
	PolyTranslucentThing(AActor *thing, subsector_t *sub, uint32_t subsectorDepth, double dist, float t1, float t2, uint32_t stencilValue) : PolyTranslucentObject(subsectorDepth, dist), thing(thing), sub(sub), SpriteLeft(t1), SpriteRight(t2), StencilValue(stencilValue) { }

	void Render(PolyRenderThread *thread, const TriMatrix &worldToClip, const PolyClipPlane &portalPlane) override
	{
		if ((thing->renderflags & RF_SPRITETYPEMASK) == RF_WALLSPRITE)
		{
			RenderPolyWallSprite wallspr;
			wallspr.Render(thread, worldToClip, portalPlane, thing, sub, StencilValue + 1);
		}
		else
		{
			RenderPolySprite spr;
			spr.Render(thread, worldToClip, portalPlane, thing, sub, StencilValue + 1, SpriteLeft, SpriteRight);
		}
	}

	AActor *thing = nullptr;
	subsector_t *sub = nullptr;
	float SpriteLeft = 0.0f;
	float SpriteRight = 1.0f;
	uint32_t StencilValue = 0;
};

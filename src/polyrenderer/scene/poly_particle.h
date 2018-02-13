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
#include "p_effect.h"

class RenderPolyParticle
{
public:
	void Render(PolyRenderThread *thread, const TriMatrix &worldToClip, const PolyClipPlane &clipPlane, particle_t *particle, subsector_t *sub, uint32_t stencilValue);

private:
	static uint8_t *GetParticleTexture();

	enum
	{
		NumParticleTextures = 3,
		ParticleTextureSize = 64
	};
};

class PolyTranslucentParticle : public PolyTranslucentObject
{
public:
	PolyTranslucentParticle(particle_t *particle, subsector_t *sub, uint32_t subsectorDepth, uint32_t stencilValue) : PolyTranslucentObject(subsectorDepth, 0.0), particle(particle), sub(sub), StencilValue(stencilValue) { }

	void Render(PolyRenderThread *thread, const TriMatrix &worldToClip, const PolyClipPlane &portalPlane) override
	{
		RenderPolyParticle spr;
		spr.Render(thread, worldToClip, portalPlane, particle, sub, StencilValue + 1);
	}

	particle_t *particle = nullptr;
	subsector_t *sub = nullptr;
	uint32_t StencilValue = 0;
};

/*
**  Particle drawing
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
#include "r_data/r_translate.h"
#include "poly_particle.h"
#include "polyrenderer/poly_renderer.h"
#include "swrenderer/scene/r_light.h"

void RenderPolyParticle::Render(const TriMatrix &worldToClip, const Vec4f &clipPlane, particle_t *particle, subsector_t *sub, uint32_t subsectorDepth, uint32_t stencilValue)
{
	DVector3 pos = particle->Pos;
	double psize = particle->size / 8.0;
	double zpos = pos.Z;

	DVector2 points[2] =
	{
		{ pos.X - ViewSin * psize, pos.Y + ViewCos * psize },
		{ pos.X + ViewSin * psize, pos.Y - ViewCos * psize }
	};

	TriVertex *vertices = PolyVertexBuffer::GetVertices(4);
	if (!vertices)
		return;

	bool foggy = false;
	int actualextralight = foggy ? 0 : extralight << 4;

	std::pair<float, float> offsets[4] =
	{
		{ 0.0f,  1.0f },
		{ 1.0f,  1.0f },
		{ 1.0f,  0.0f },
		{ 0.0f,  0.0f },
	};

	for (int i = 0; i < 4; i++)
	{
		auto &p = (i == 0 || i == 3) ? points[0] : points[1];

		vertices[i].x = (float)p.X;
		vertices[i].y = (float)p.Y;
		vertices[i].z = (float)(zpos + psize * (2.0 * offsets[i].second - 1.0));
		vertices[i].w = 1.0f;
		vertices[i].varying[0] = (float)(offsets[i].first);
		vertices[i].varying[1] = (float)(1.0f - offsets[i].second);
	}

	// int color = (particle->color >> 24) & 0xff; // pal index, I think
	bool fullbrightSprite = particle->bright != 0;
	swrenderer::CameraLight *cameraLight = swrenderer::CameraLight::Instance();

	PolyDrawArgs args;

	args.uniforms.globvis = (float)swrenderer::LightVisibility::Instance()->ParticleGlobVis();

	if (fullbrightSprite || cameraLight->FixedLightLevel() >= 0 || cameraLight->FixedColormap())
	{
		args.uniforms.light = 256;
		args.uniforms.flags = TriUniforms::fixed_light;
	}
	else
	{
		args.uniforms.light = (uint32_t)((sub->sector->lightlevel + actualextralight) / 255.0f * 256.0f);
		args.uniforms.flags = 0;
	}
	args.uniforms.subsectorDepth = subsectorDepth;

	uint32_t alpha = (uint32_t)clamp(particle->alpha * 255.0f + 0.5f, 0.0f, 255.0f);

	if (swrenderer::RenderViewport::Instance()->RenderTarget->IsBgra())
	{
		args.uniforms.color = (alpha << 24) | (particle->color & 0xffffff);
	}
	else
	{
		args.uniforms.color = ((uint32_t)particle->color) >> 24;
		args.uniforms.srcalpha = alpha;
		args.uniforms.destalpha = 255 - alpha;
	}

	args.objectToClip = &worldToClip;
	args.vinput = vertices;
	args.vcount = 4;
	args.mode = TriangleDrawMode::Fan;
	args.ccw = true;
	args.stenciltestvalue = stencilValue;
	args.stencilwritevalue = stencilValue;
	args.SetColormap(sub->sector->ColorMap);
	args.SetClipPlane(clipPlane.x, clipPlane.y, clipPlane.z, clipPlane.w);
	args.subsectorTest = true;
	args.writeStencil = false;
	args.writeSubsector = false;
	args.blendmode = TriBlendMode::AlphaBlend;
	PolyTriangleDrawer::draw(args);
}

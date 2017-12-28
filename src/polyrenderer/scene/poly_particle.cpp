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
#include "r_data/r_translate.h"
#include "poly_particle.h"
#include "polyrenderer/poly_renderer.h"
#include "polyrenderer/scene/poly_light.h"
#include "polyrenderer/poly_renderthread.h"

EXTERN_CVAR(Int, gl_particles_style)

void RenderPolyParticle::Render(PolyRenderThread *thread, const TriMatrix &worldToClip, const PolyClipPlane &clipPlane, particle_t *particle, subsector_t *sub, uint32_t stencilValue)
{
	double timefrac = r_viewpoint.TicFrac;
	if (paused || bglobal.freeze)
		timefrac = 0.;
	DVector3 pos = particle->Pos + (particle->Vel * timefrac);
	double psize = particle->size / 8.0;
	double zpos = pos.Z;

	const auto &viewpoint = PolyRenderer::Instance()->Viewpoint;

	DVector2 points[2] =
	{
		{ pos.X - viewpoint.Sin * psize, pos.Y + viewpoint.Cos * psize },
		{ pos.X + viewpoint.Sin * psize, pos.Y - viewpoint.Cos * psize }
	};

	TriVertex *vertices = thread->FrameMemory->AllocMemory<TriVertex>(4);

	bool foggy = false;
	int actualextralight = foggy ? 0 : viewpoint.extralight << 4;

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
		vertices[i].u = (float)(offsets[i].first);
		vertices[i].v = (float)(1.0f - offsets[i].second);
	}

	bool fullbrightSprite = particle->bright != 0;
	int lightlevel = fullbrightSprite ? 255 : sub->sector->lightlevel + actualextralight;

	PolyDrawArgs args;
	args.SetLight(GetColorTable(sub->sector->Colormap), lightlevel, PolyRenderer::Instance()->Light.ParticleGlobVis(foggy), fullbrightSprite);
	args.SetDepthTest(true);
	args.SetColor(particle->color | 0xff000000, particle->color >> 24);
	args.SetStyle(TriBlendMode::Shaded, particle->alpha, 1.0 - particle->alpha);
	args.SetTransform(&worldToClip);
	args.SetFaceCullCCW(true);
	args.SetStencilTestValue(stencilValue);
	args.SetWriteStencil(false);
	args.SetWriteDepth(false);
	args.SetClipPlane(0, clipPlane);
	args.SetTexture(GetParticleTexture(), ParticleTextureSize, ParticleTextureSize);
	args.DrawArray(thread, vertices, 4, PolyDrawMode::TriangleFan);
}

uint8_t *RenderPolyParticle::GetParticleTexture()
{
	static uint8_t particle_texture[NumParticleTextures][ParticleTextureSize * ParticleTextureSize];
	static bool first_call = true;
	if (first_call)
	{
		double center = ParticleTextureSize * 0.5f;
		for (int y = 0; y < ParticleTextureSize; y++)
		{
			for (int x = 0; x < ParticleTextureSize; x++)
			{
				double dx = (center - x - 0.5f) / center;
				double dy = (center - y - 0.5f) / center;
				double dist2 = dx * dx + dy * dy;
				double round_alpha = clamp<double>(1.7f - dist2 * 1.7f, 0.0f, 1.0f);
				double smooth_alpha = clamp<double>(1.1f - dist2 * 1.1f, 0.0f, 1.0f);

				particle_texture[0][x + y * ParticleTextureSize] = 255;
				particle_texture[1][x + y * ParticleTextureSize] = (int)(round_alpha * 255.0f + 0.5f);
				particle_texture[2][x + y * ParticleTextureSize] = (int)(smooth_alpha * 255.0f + 0.5f);
			}
		}
		first_call = false;
	}
	return particle_texture[MIN<int>(gl_particles_style, NumParticleTextures)];
}

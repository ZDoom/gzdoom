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
#include "poly_model.h"
#include "polyrenderer/poly_renderer.h"
#include "polyrenderer/scene/poly_light.h"
#include "polyrenderer/poly_renderthread.h"
#include "r_data/r_vanillatrans.h"
#include "actorinlines.h"
#include "i_time.h"

void PolyRenderModel(PolyRenderThread *thread, const Mat4f &worldToClip, uint32_t stencilValue, float x, float y, float z, FSpriteModelFrame *smf, AActor *actor)
{
	PolyModelRenderer renderer(thread, worldToClip, stencilValue);
	renderer.AddLights(actor);
	renderer.RenderModel(x, y, z, smf, actor);
}

void PolyRenderHUDModel(PolyRenderThread *thread, const Mat4f &worldToClip, uint32_t stencilValue, DPSprite *psp, float ofsx, float ofsy)
{
	PolyModelRenderer renderer(thread, worldToClip, stencilValue);
	renderer.RenderHUDModel(psp, ofsx, ofsy);
}

/////////////////////////////////////////////////////////////////////////////

PolyModelRenderer::PolyModelRenderer(PolyRenderThread *thread, const Mat4f &worldToClip, uint32_t stencilValue) : Thread(thread), WorldToClip(worldToClip), StencilValue(stencilValue)
{
}

void PolyModelRenderer::AddLights(AActor *actor)
{
	if (gl_lights && actor)
	{
		auto &addedLights = Thread->AddedLightsArray;

		addedLights.Clear();

		float x = (float)actor->X();
		float y = (float)actor->Y();
		float z = (float)actor->Center();
		float radiusSquared = (float)(actor->renderradius * actor->renderradius);

		BSPWalkCircle(x, y, radiusSquared, [&](subsector_t *subsector) // Iterate through all subsectors potentially touched by actor
		{
			FLightNode * node = subsector->lighthead;
			while (node) // check all lights touching a subsector
			{
				ADynamicLight *light = node->lightsource;
				if (light->visibletoplayer && !(light->flags2&MF2_DORMANT) && (!(light->lightflags&LF_DONTLIGHTSELF) || light->target != actor) && !(light->lightflags&LF_DONTLIGHTACTORS))
				{
					int group = subsector->sector->PortalGroup;
					DVector3 pos = light->PosRelative(group);
					float radius = (float)(light->GetRadius() + actor->renderradius);
					double dx = pos.X - x;
					double dy = pos.Y - y;
					double dz = pos.Z - z;
					double distSquared = dx * dx + dy * dy + dz * dz;
					if (distSquared < radius * radius) // Light and actor touches
					{
						if (std::find(addedLights.begin(), addedLights.end(), light) == addedLights.end()) // Check if we already added this light from a different subsector
						{
							addedLights.Push(light);
						}
					}
				}
				node = node->nextLight;
			}
		});

		NumLights = addedLights.Size();
		Lights = Thread->FrameMemory->AllocMemory<PolyLight>(NumLights);
		for (int i = 0; i < NumLights; i++)
		{
			ADynamicLight *lightsource = addedLights[i];

			bool is_point_light = (lightsource->lightflags & LF_ATTENUATE) != 0;

			uint32_t red = lightsource->GetRed();
			uint32_t green = lightsource->GetGreen();
			uint32_t blue = lightsource->GetBlue();

			PolyLight &light = Lights[i];
			light.x = (float)lightsource->X();
			light.y = (float)lightsource->Y();
			light.z = (float)lightsource->Z();
			light.radius = 256.0f / lightsource->GetRadius();
			light.color = (red << 16) | (green << 8) | blue;
			if (is_point_light)
				light.radius = -light.radius;
		}
	}
}

void PolyModelRenderer::BeginDrawModel(AActor *actor, FSpriteModelFrame *smf, const VSMatrix &objectToWorldMatrix, bool mirrored)
{
	ModelActor = actor;
	const_cast<VSMatrix &>(objectToWorldMatrix).copy(ObjectToWorld.Matrix);
	SetTransform();

	if (actor->RenderStyle == LegacyRenderStyles[STYLE_Normal] || !!(smf->flags & MDL_DONTCULLBACKFACES))
		PolyTriangleDrawer::SetTwoSided(Thread->DrawQueue, true);
	PolyTriangleDrawer::SetCullCCW(Thread->DrawQueue, !mirrored);
}

void PolyModelRenderer::EndDrawModel(AActor *actor, FSpriteModelFrame *smf)
{
	if (actor->RenderStyle == LegacyRenderStyles[STYLE_Normal] || !!(smf->flags & MDL_DONTCULLBACKFACES))
		PolyTriangleDrawer::SetTwoSided(Thread->DrawQueue, false);
	PolyTriangleDrawer::SetCullCCW(Thread->DrawQueue, true);

	ModelActor = nullptr;
}

IModelVertexBuffer *PolyModelRenderer::CreateVertexBuffer(bool needindex, bool singleframe)
{
	return new PolyModelVertexBuffer(needindex, singleframe);
}

void PolyModelRenderer::SetVertexBuffer(IModelVertexBuffer *buffer)
{
}

void PolyModelRenderer::ResetVertexBuffer()
{
}

VSMatrix PolyModelRenderer::GetViewToWorldMatrix()
{
	Mat4f swapYZ = Mat4f::Null();
	swapYZ.Matrix[0 + 0 * 4] = 1.0f;
	swapYZ.Matrix[1 + 2 * 4] = 1.0f;
	swapYZ.Matrix[2 + 1 * 4] = 1.0f;
	swapYZ.Matrix[3 + 3 * 4] = 1.0f;

	VSMatrix worldToView;
	worldToView.loadMatrix((PolyRenderer::Instance()->Scene.CurrentViewpoint->WorldToView * swapYZ).Matrix);
	
	VSMatrix objectToWorld;
	worldToView.inverseMatrix(objectToWorld);
	return objectToWorld;
}

void PolyModelRenderer::BeginDrawHUDModel(AActor *actor, const VSMatrix &objectToWorldMatrix, bool mirrored)
{
	ModelActor = actor;
	const_cast<VSMatrix &>(objectToWorldMatrix).copy(ObjectToWorld.Matrix);
	SetTransform();
	PolyTriangleDrawer::SetWeaponScene(Thread->DrawQueue, true);

	if (actor->RenderStyle == LegacyRenderStyles[STYLE_Normal])
		PolyTriangleDrawer::SetTwoSided(Thread->DrawQueue, true);
	PolyTriangleDrawer::SetCullCCW(Thread->DrawQueue, mirrored);
}

void PolyModelRenderer::EndDrawHUDModel(AActor *actor)
{
	ModelActor = nullptr;
	PolyTriangleDrawer::SetWeaponScene(Thread->DrawQueue, false);

	if (actor->RenderStyle == LegacyRenderStyles[STYLE_Normal])
		PolyTriangleDrawer::SetTwoSided(Thread->DrawQueue, false);
	PolyTriangleDrawer::SetCullCCW(Thread->DrawQueue, true);
}

void PolyModelRenderer::SetInterpolation(double interpolation)
{
	InterpolationFactor = (float)interpolation;
}

void PolyModelRenderer::SetMaterial(FTexture *skin, bool clampNoFilter, int translation)
{
	SkinTexture = skin;
}

void PolyModelRenderer::SetTransform()
{
	Mat4f swapYZ = Mat4f::Null();
	swapYZ.Matrix[0 + 0 * 4] = 1.0f;
	swapYZ.Matrix[1 + 2 * 4] = 1.0f;
	swapYZ.Matrix[2 + 1 * 4] = 1.0f;
	swapYZ.Matrix[3 + 3 * 4] = 1.0f;
	ObjectToWorld = swapYZ * ObjectToWorld;

	PolyTriangleDrawer::SetTransform(Thread->DrawQueue, Thread->FrameMemory->NewObject<Mat4f>(WorldToClip * ObjectToWorld), Thread->FrameMemory->NewObject<Mat4f>(ObjectToWorld));
}

void PolyModelRenderer::DrawArrays(int start, int count)
{
	const auto &viewpoint = PolyRenderer::Instance()->Viewpoint;

	bool foggy = false;
	int actualextralight = foggy ? 0 : viewpoint.extralight << 4;
	sector_t *sector = ModelActor->Sector;

	bool fullbrightSprite = ((ModelActor->renderflags & RF_FULLBRIGHT) || (ModelActor->flags5 & MF5_BRIGHT));
	int lightlevel = fullbrightSprite ? 255 : ModelActor->Sector->lightlevel + actualextralight;

	PolyDrawArgs args;
	args.SetLight(GetColorTable(sector->Colormap, sector->SpecialColors[sector_t::sprites], true), lightlevel, PolyRenderer::Instance()->Light.SpriteGlobVis(foggy), fullbrightSprite);
	args.SetLights(Lights, NumLights);
	args.SetStencilTestValue(StencilValue);
	args.SetClipPlane(0, PolyClipPlane());
	args.SetStyle(ModelActor->RenderStyle, ModelActor->Alpha, ModelActor->fillcolor, ModelActor->Translation, SkinTexture, fullbrightSprite);
	args.SetDepthTest(true);
	args.SetWriteDepth(true);
	args.SetWriteStencil(false);
	args.DrawArray(Thread->DrawQueue, VertexBuffer + start, count);
}

void PolyModelRenderer::DrawElements(int numIndices, size_t offset)
{
	const auto &viewpoint = PolyRenderer::Instance()->Viewpoint;

	bool foggy = false;
	int actualextralight = foggy ? 0 : viewpoint.extralight << 4;
	sector_t *sector = ModelActor->Sector;

	bool fullbrightSprite = ((ModelActor->renderflags & RF_FULLBRIGHT) || (ModelActor->flags5 & MF5_BRIGHT));
	int lightlevel = fullbrightSprite ? 255 : ModelActor->Sector->lightlevel + actualextralight;

	PolyDrawArgs args;
	args.SetLight(GetColorTable(sector->Colormap, sector->SpecialColors[sector_t::sprites], true), lightlevel, PolyRenderer::Instance()->Light.SpriteGlobVis(foggy), fullbrightSprite);
	args.SetLights(Lights, NumLights);
	args.SetStencilTestValue(StencilValue);
	args.SetClipPlane(0, PolyClipPlane());
	args.SetStyle(ModelActor->RenderStyle, ModelActor->Alpha, ModelActor->fillcolor, ModelActor->Translation, SkinTexture, fullbrightSprite);
	args.SetDepthTest(true);
	args.SetWriteDepth(true);
	args.SetWriteStencil(false);
	args.DrawElements(Thread->DrawQueue, VertexBuffer, IndexBuffer + offset / sizeof(unsigned int), numIndices);
}

/////////////////////////////////////////////////////////////////////////////

PolyModelVertexBuffer::PolyModelVertexBuffer(bool needindex, bool singleframe)
{
}

PolyModelVertexBuffer::~PolyModelVertexBuffer()
{
}

FModelVertex *PolyModelVertexBuffer::LockVertexBuffer(unsigned int size)
{
	mVertexBuffer.Resize(size);
	return &mVertexBuffer[0];
}

void PolyModelVertexBuffer::UnlockVertexBuffer()
{
}

unsigned int *PolyModelVertexBuffer::LockIndexBuffer(unsigned int size)
{
	mIndexBuffer.Resize(size);
	return &mIndexBuffer[0];
}

void PolyModelVertexBuffer::UnlockIndexBuffer()
{
}

void PolyModelVertexBuffer::SetupFrame(FModelRenderer *renderer, unsigned int frame1, unsigned int frame2, unsigned int size)
{
	PolyModelRenderer *polyrenderer = (PolyModelRenderer *)renderer;

	if (frame1 == frame2 || size == 0 || polyrenderer->InterpolationFactor == 0.f)
	{
		TriVertex *vertices = polyrenderer->Thread->FrameMemory->AllocMemory<TriVertex>(size);

		for (unsigned int i = 0; i < size; i++)
		{
			vertices[i] =
			{
				mVertexBuffer[frame1 + i].x,
				mVertexBuffer[frame1 + i].y,
				mVertexBuffer[frame1 + i].z,
				1.0f,
				mVertexBuffer[frame1 + i].u,
				mVertexBuffer[frame1 + i].v
			};
		}

		polyrenderer->VertexBuffer = vertices;
		polyrenderer->IndexBuffer = &mIndexBuffer[0];
	}
	else
	{
		TriVertex *vertices = polyrenderer->Thread->FrameMemory->AllocMemory<TriVertex>(size);

		float frac = polyrenderer->InterpolationFactor;
		float inv_frac = 1.0f - frac;
		for (unsigned int i = 0; i < size; i++)
		{
			vertices[i].x = mVertexBuffer[frame1 + i].x * inv_frac + mVertexBuffer[frame2 + i].x * frac;
			vertices[i].y = mVertexBuffer[frame1 + i].y * inv_frac + mVertexBuffer[frame2 + i].y * frac;
			vertices[i].z = mVertexBuffer[frame1 + i].z * inv_frac + mVertexBuffer[frame2 + i].z * frac;
			vertices[i].w = 1.0f;
			vertices[i].u = mVertexBuffer[frame1 + i].u;
			vertices[i].v = mVertexBuffer[frame1 + i].v;
		}

		polyrenderer->VertexBuffer = vertices;
		polyrenderer->IndexBuffer = &mIndexBuffer[0];
	}
}

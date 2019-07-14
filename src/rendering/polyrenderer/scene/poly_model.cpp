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

	renderer.sector = actor->Sector;
	renderer.RenderStyle = actor->RenderStyle;
	renderer.RenderAlpha = (float)actor->Alpha;
	if (!renderer.RenderStyle.IsVisible(renderer.RenderAlpha))
		return;

	bool foggy = false;
	int actualextralight = foggy ? 0 : PolyRenderer::Instance()->Viewpoint.extralight << 4;
	bool fullbrightSprite = ((actor->renderflags & RF_FULLBRIGHT) || (actor->flags5 & MF5_BRIGHT));
	renderer.lightlevel = fullbrightSprite ? 255 : actor->Sector->lightlevel + actualextralight;
	renderer.visibility = PolyRenderer::Instance()->Light.SpriteGlobVis(foggy);

	renderer.fillcolor = actor->fillcolor;
	renderer.Translation = actor->Translation;

	renderer.AddLights(actor);
	renderer.RenderModel(x, y, z, smf, actor, PolyRenderer::Instance()->Viewpoint.TicFrac);
	PolyTriangleDrawer::SetModelVertexShader(thread->DrawQueue, -1, -1, 0.0f);
	PolyTriangleDrawer::SetTransform(thread->DrawQueue, thread->FrameMemory->NewObject<Mat4f>(worldToClip), nullptr);
}

static bool isBright(DPSprite *psp)
{
	if (psp != nullptr && psp->GetState() != nullptr)
	{
		bool disablefullbright = false;
		FTextureID lump = sprites[psp->GetSprite()].GetSpriteFrame(psp->GetFrame(), 0, 0., nullptr);
		if (lump.isValid())
		{
			FTexture * tex = TexMan.GetPalettedTexture(lump, true);
			if (tex) disablefullbright = tex->isFullbrightDisabled();
		}
		return psp->GetState()->GetFullbright() && !disablefullbright;
	}
	return false;
}

void PolyRenderHUDModel(PolyRenderThread *thread, const Mat4f &worldToClip, uint32_t stencilValue, DPSprite *psp, float ofsx, float ofsy)
{
	PolyModelRenderer renderer(thread, worldToClip, stencilValue);

	AActor *playermo = players[consoleplayer].camera;
	auto rs = psp->GetRenderStyle(playermo->RenderStyle, playermo->Alpha);
	renderer.sector = playermo->Sector;
	renderer.RenderStyle = rs.first;
	renderer.RenderAlpha = rs.second;
	if (psp->Flags & PSPF_FORCEALPHA) renderer.RenderAlpha = 0.0f;
	if (!renderer.RenderStyle.IsVisible(renderer.RenderAlpha))
		return;

	bool foggy = false;
	int actualextralight = foggy ? 0 : PolyRenderer::Instance()->Viewpoint.extralight << 4;
	bool fullbrightSprite = isBright(psp);
	renderer.lightlevel = fullbrightSprite ? 255 : playermo->Sector->lightlevel + actualextralight;
	renderer.visibility = PolyRenderer::Instance()->Light.SpriteGlobVis(foggy);

	PalEntry ThingColor = (playermo->RenderStyle.Flags & STYLEF_ColorIsFixed) ? playermo->fillcolor : 0xffffff;
	ThingColor.a = 255;

	renderer.fillcolor = fullbrightSprite ? ThingColor : ThingColor.Modulate(playermo->Sector->SpecialColors[sector_t::sprites]);
	renderer.Translation = 0xffffffff;// playermo->Translation;

	renderer.RenderHUDModel(psp, ofsx, ofsy);
	PolyTriangleDrawer::SetModelVertexShader(thread->DrawQueue, -1, -1, 0.0f);
}

/////////////////////////////////////////////////////////////////////////////

PolyModelRenderer::PolyModelRenderer(PolyRenderThread *thread, const Mat4f &worldToClip, uint32_t stencilValue) : Thread(thread), WorldToClip(worldToClip), StencilValue(stencilValue)
{
}

void PolyModelRenderer::AddLights(AActor *actor)
{
	if (r_dynlights && actor)
	{
		auto &addedLights = Thread->AddedLightsArray;

		addedLights.Clear();

		float x = (float)actor->X();
		float y = (float)actor->Y();
		float z = (float)actor->Center();
		float actorradius = (float)actor->RenderRadius();
		float radiusSquared = actorradius * actorradius;

		BSPWalkCircle(actor->Level, x, y, radiusSquared, [&](subsector_t *subsector) // Iterate through all subsectors potentially touched by actor
		{
			FLightNode * node = subsector->section->lighthead;
			while (node) // check all lights touching a subsector
			{
				FDynamicLight *light = node->lightsource;
				if (light->ShouldLightActor(actor))
				{
					int group = subsector->sector->PortalGroup;
					DVector3 pos = light->PosRelative(group);
					float radius = (float)(light->GetRadius() + actorradius);
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
			FDynamicLight *lightsource = addedLights[i];

			bool is_point_light = lightsource->IsAttenuated();

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
}

IModelVertexBuffer *PolyModelRenderer::CreateVertexBuffer(bool needindex, bool singleframe)
{
	return new PolyModelVertexBuffer(needindex, singleframe);
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
	const_cast<VSMatrix &>(objectToWorldMatrix).copy(ObjectToWorld.Matrix);
	SetTransform();
	PolyTriangleDrawer::SetWeaponScene(Thread->DrawQueue, true);

	if (actor->RenderStyle == LegacyRenderStyles[STYLE_Normal])
		PolyTriangleDrawer::SetTwoSided(Thread->DrawQueue, true);
	PolyTriangleDrawer::SetCullCCW(Thread->DrawQueue, mirrored);
}

void PolyModelRenderer::EndDrawHUDModel(AActor *actor)
{
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
	SkinTexture = skin? skin->GetSoftwareTexture() : nullptr;
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
	PolyDrawArgs args;
	auto nc = !!(sector->Level->flags3 & LEVEL3_NOCOLOREDSPRITELIGHTING);
	args.SetLight(GetSpriteColorTable(sector->Colormap, sector->SpecialColors[sector_t::sprites], nc), lightlevel, visibility, fullbrightSprite);	args.SetLights(Lights, NumLights);
	args.SetStencilTestValue(StencilValue);
	args.SetClipPlane(0, PolyClipPlane());
	args.SetStyle(RenderStyle, RenderAlpha, fillcolor, Translation, SkinTexture, fullbrightSprite);
	args.SetDepthTest(true);
	args.SetWriteDepth(true);
	args.SetWriteStencil(false);
	PolyTriangleDrawer::DrawArray(Thread->DrawQueue, args, VertexBuffer + start, count);
}

void PolyModelRenderer::DrawElements(int numIndices, size_t offset)
{
	PolyDrawArgs args;
	auto nc = !!(sector->Level->flags3 & LEVEL3_NOCOLOREDSPRITELIGHTING);
	args.SetLight(GetSpriteColorTable(sector->Colormap, sector->SpecialColors[sector_t::sprites], nc), lightlevel, visibility, fullbrightSprite);	args.SetLights(Lights, NumLights);
	args.SetStencilTestValue(StencilValue);
	args.SetClipPlane(0, PolyClipPlane());
	args.SetStyle(RenderStyle, RenderAlpha, fillcolor, Translation, SkinTexture, fullbrightSprite);
	args.SetDepthTest(true);
	args.SetWriteDepth(true);
	args.SetWriteStencil(false);
	PolyTriangleDrawer::DrawElements(Thread->DrawQueue, args, VertexBuffer, IndexBuffer + offset / sizeof(unsigned int), numIndices);
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
	polyrenderer->VertexBuffer = mVertexBuffer.Size() ? &mVertexBuffer[0] : nullptr;
	polyrenderer->IndexBuffer = mIndexBuffer.Size() ? &mIndexBuffer[0] : nullptr;
	PolyTriangleDrawer::SetModelVertexShader(polyrenderer->Thread->DrawQueue, frame1, frame2, polyrenderer->InterpolationFactor);
}

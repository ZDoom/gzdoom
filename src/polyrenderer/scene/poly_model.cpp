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

void gl_FlushModels();
bool polymodelsInUse;

void PolyRenderModel(PolyRenderThread *thread, const TriMatrix &worldToClip, const PolyClipPlane &clipPlane, uint32_t stencilValue, float x, float y, float z, FSpriteModelFrame *smf, AActor *actor)
{
	PolyModelRenderer renderer(thread, worldToClip, clipPlane, stencilValue);
	renderer.RenderModel(x, y, z, smf, actor);
}

void PolyRenderHUDModel(PolyRenderThread *thread, const TriMatrix &worldToClip, const PolyClipPlane &clipPlane, uint32_t stencilValue, DPSprite *psp, float ofsx, float ofsy)
{
	PolyModelRenderer renderer(thread, worldToClip, clipPlane, stencilValue);
	renderer.RenderHUDModel(psp, ofsx, ofsy);
}

/////////////////////////////////////////////////////////////////////////////

PolyModelRenderer::PolyModelRenderer(PolyRenderThread *thread, const TriMatrix &worldToClip, const PolyClipPlane &clipPlane, uint32_t stencilValue) : Thread(thread), WorldToClip(worldToClip), ClipPlane(clipPlane), StencilValue(stencilValue)
{
	if (!polymodelsInUse)
	{
		gl_FlushModels();
		polymodelsInUse = true;
	}
}

void PolyModelRenderer::BeginDrawModel(AActor *actor, FSpriteModelFrame *smf, const VSMatrix &objectToWorldMatrix)
{
	ModelActor = actor;
	const_cast<VSMatrix &>(objectToWorldMatrix).copy(ObjectToWorld.matrix);
}

void PolyModelRenderer::EndDrawModel(AActor *actor, FSpriteModelFrame *smf)
{
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
	TriMatrix swapYZ = TriMatrix::null();
	swapYZ.matrix[0 + 0 * 4] = 1.0f;
	swapYZ.matrix[1 + 2 * 4] = 1.0f;
	swapYZ.matrix[2 + 1 * 4] = 1.0f;
	swapYZ.matrix[3 + 3 * 4] = 1.0f;

	VSMatrix worldToView;
	worldToView.loadMatrix((PolyRenderer::Instance()->WorldToView * swapYZ).matrix);
	
	VSMatrix objectToWorld;
	worldToView.inverseMatrix(objectToWorld);
	return objectToWorld;
}

void PolyModelRenderer::BeginDrawHUDModel(AActor *actor, const VSMatrix &objectToWorldMatrix)
{
	ModelActor = actor;
	const_cast<VSMatrix &>(objectToWorldMatrix).copy(ObjectToWorld.matrix);
}

void PolyModelRenderer::EndDrawHUDModel(AActor *actor)
{
	ModelActor = nullptr;
}

void PolyModelRenderer::SetInterpolation(double interpolation)
{
	InterpolationFactor = (float)interpolation;
}

void PolyModelRenderer::SetMaterial(FTexture *skin, bool clampNoFilter, int translation)
{
	SkinTexture = skin;
}

void PolyModelRenderer::DrawArrays(int start, int count)
{
	const auto &viewpoint = PolyRenderer::Instance()->Viewpoint;

	bool foggy = false;
	int actualextralight = foggy ? 0 : viewpoint.extralight << 4;
	sector_t *sector = ModelActor->Sector;

	bool fullbrightSprite = ((ModelActor->renderflags & RF_FULLBRIGHT) || (ModelActor->flags5 & MF5_BRIGHT));
	int lightlevel = fullbrightSprite ? 255 : ModelActor->Sector->lightlevel + actualextralight;

	TriMatrix swapYZ = TriMatrix::null();
	swapYZ.matrix[0 + 0 * 4] = 1.0f;
	swapYZ.matrix[1 + 2 * 4] = 1.0f;
	swapYZ.matrix[2 + 1 * 4] = 1.0f;
	swapYZ.matrix[3 + 3 * 4] = 1.0f;

	TriMatrix *transform = Thread->FrameMemory->NewObject<TriMatrix>();
	*transform = WorldToClip * swapYZ * ObjectToWorld;

	PolyDrawArgs args;
	args.SetLight(GetColorTable(sector->Colormap, sector->SpecialColors[sector_t::sprites], true), lightlevel, PolyRenderer::Instance()->Light.SpriteGlobVis(foggy), fullbrightSprite);
	args.SetTransform(transform);
	args.SetFaceCullCCW(true);
	args.SetStencilTestValue(StencilValue);
	args.SetClipPlane(0, PolyClipPlane());
	args.SetStyle(TriBlendMode::TextureOpaque);
	args.SetTexture(SkinTexture);
	args.SetDepthTest(true);
	args.SetWriteDepth(true);
	args.SetWriteStencil(false);
	args.DrawArray(Thread, VertexBuffer + start, count);
}

void PolyModelRenderer::DrawElements(int numIndices, size_t offset)
{
	const auto &viewpoint = PolyRenderer::Instance()->Viewpoint;

	bool foggy = false;
	int actualextralight = foggy ? 0 : viewpoint.extralight << 4;
	sector_t *sector = ModelActor->Sector;

	bool fullbrightSprite = ((ModelActor->renderflags & RF_FULLBRIGHT) || (ModelActor->flags5 & MF5_BRIGHT));
	int lightlevel = fullbrightSprite ? 255 : ModelActor->Sector->lightlevel + actualextralight;

	TriMatrix swapYZ = TriMatrix::null();
	swapYZ.matrix[0 + 0 * 4] = 1.0f;
	swapYZ.matrix[1 + 2 * 4] = 1.0f;
	swapYZ.matrix[2 + 1 * 4] = 1.0f;
	swapYZ.matrix[3 + 3 * 4] = 1.0f;

	TriMatrix *transform = Thread->FrameMemory->NewObject<TriMatrix>();
	*transform = WorldToClip * swapYZ * ObjectToWorld;

	PolyDrawArgs args;
	args.SetLight(GetColorTable(sector->Colormap, sector->SpecialColors[sector_t::sprites], true), lightlevel, PolyRenderer::Instance()->Light.SpriteGlobVis(foggy), fullbrightSprite);
	args.SetTransform(transform);
	args.SetFaceCullCCW(true);
	args.SetStencilTestValue(StencilValue);
	args.SetClipPlane(0, PolyClipPlane());
	args.SetStyle(TriBlendMode::TextureOpaque);
	args.SetTexture(SkinTexture);
	args.SetDepthTest(true);
	args.SetWriteDepth(true);
	args.SetWriteStencil(false);
	args.DrawElements(Thread, VertexBuffer, IndexBuffer + offset / sizeof(unsigned int), numIndices);
}

double PolyModelRenderer::GetTimeFloat()
{
	return (float)I_msTime() * (float)TICRATE / 1000.0f;
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

	if (true)//if (frame1 == frame2 || size == 0 || polyrenderer->InterpolationFactor == 0.f)
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
		for (unsigned int i = 0; i < size; i++)
		{
			vertices[i].x = mVertexBuffer[frame1 + i].x * (1.0f - frac) + mVertexBuffer[frame2 + i].x * frac;
			vertices[i].y = mVertexBuffer[frame1 + i].y * (1.0f - frac) + mVertexBuffer[frame2 + i].y * frac;
			vertices[i].z = mVertexBuffer[frame1 + i].z * (1.0f - frac) + mVertexBuffer[frame2 + i].z * frac;
			vertices[i].w = 1.0f;
			vertices[i].u = mVertexBuffer[frame1 + i].u;
			vertices[i].v = mVertexBuffer[frame1 + i].v;
		}

		polyrenderer->VertexBuffer = vertices;
		polyrenderer->IndexBuffer = &mIndexBuffer[0];
	}
}

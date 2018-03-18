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
#include "r_model.h"
#include "r_data/r_vanillatrans.h"
#include "actorinlines.h"
#include "i_time.h"
#include "swrenderer/r_memory.h"
#include "swrenderer/r_swcolormaps.h"
#include "swrenderer/viewport/r_viewport.h"
#include "swrenderer/scene/r_light.h"

void gl_FlushModels();
extern bool polymodelsInUse;

namespace swrenderer
{
	void RenderModel::Project(RenderThread *thread, float x, float y, float z, FSpriteModelFrame *smf, AActor *actor)
	{
		// transform the origin point
		double tr_x = x - thread->Viewport->viewpoint.Pos.X;
		double tr_y = y - thread->Viewport->viewpoint.Pos.Y;
		double tz = tr_x * thread->Viewport->viewpoint.TanCos + tr_y * thread->Viewport->viewpoint.TanSin;

		// thing is behind view plane?
		if (tz < MINZ)
			return;

		// too far off the side?
		double tx = tr_x * thread->Viewport->viewpoint.Sin - tr_y * thread->Viewport->viewpoint.Cos;
		if (fabs(tx / 64) > fabs(tz))
			return;

		RenderModel *vis = thread->FrameMemory->NewObject<RenderModel>(x, y, z, smf, actor, float(1 / tz));
		thread->SpriteList->Push(vis);
	}

	RenderModel::RenderModel(float x, float y, float z, FSpriteModelFrame *smf, AActor *actor, float idepth) : x(x), y(y), z(z), smf(smf), actor(actor)
	{
		this->idepth = idepth;
	}

	void RenderModel::Render(RenderThread *thread, short *cliptop, short *clipbottom, int minZ, int maxZ, Fake3DTranslucent clip3DFloor)
	{
		SWModelRenderer renderer(thread);
		renderer.RenderModel(x, y, z, smf, actor);
	}

	/////////////////////////////////////////////////////////////////////////////

	void RenderHUDModel(RenderThread *thread, DPSprite *psp, float ofsx, float ofsy)
	{
		SWModelRenderer renderer(thread);
		renderer.RenderHUDModel(psp, ofsx, ofsy);
	}

	/////////////////////////////////////////////////////////////////////////////

	SWModelRenderer::SWModelRenderer(RenderThread *thread) : Thread(thread)
	{
		if (polymodelsInUse)
		{
			gl_FlushModels();
			polymodelsInUse = false;
		}
	}

	void SWModelRenderer::BeginDrawModel(AActor *actor, FSpriteModelFrame *smf, const VSMatrix &objectToWorldMatrix)
	{
		ModelActor = actor;
		const_cast<VSMatrix &>(objectToWorldMatrix).copy(ObjectToWorld.matrix);
	}

	void SWModelRenderer::EndDrawModel(AActor *actor, FSpriteModelFrame *smf)
	{
		ModelActor = nullptr;
	}

	IModelVertexBuffer *SWModelRenderer::CreateVertexBuffer(bool needindex, bool singleframe)
	{
		return new SWModelVertexBuffer(needindex, singleframe);
	}

	void SWModelRenderer::SetVertexBuffer(IModelVertexBuffer *buffer)
	{
	}

	void SWModelRenderer::ResetVertexBuffer()
	{
	}

	VSMatrix SWModelRenderer::GetViewToWorldMatrix()
	{
		// Calculate the WorldToView matrix as it would have looked like without yshearing:
		const auto &Viewpoint = Thread->Viewport->viewpoint;
		const auto &Viewwindow = Thread->Viewport->viewwindow;
		double radPitch = Viewpoint.Angles.Pitch.Normalized180().Radians();
		double angx = cos(radPitch);
		double angy = sin(radPitch) * level.info->pixelstretch;
		double alen = sqrt(angx*angx + angy*angy);
		float adjustedPitch = (float)asin(angy / alen);
		float adjustedViewAngle = (float)(Viewpoint.Angles.Yaw - 90).Radians();
		float ratio = Viewwindow.WidescreenRatio;
		float fovratio = (Viewwindow.WidescreenRatio >= 1.3f) ? 1.333333f : ratio;
		float fovy = (float)(2 * DAngle::ToDegrees(atan(tan(Viewpoint.FieldOfView.Radians() / 2) / fovratio)).Degrees);
		TriMatrix altWorldToView =
			TriMatrix::rotate(adjustedPitch, 1.0f, 0.0f, 0.0f) *
			TriMatrix::rotate(adjustedViewAngle, 0.0f, -1.0f, 0.0f) *
			TriMatrix::scale(1.0f, level.info->pixelstretch, 1.0f) *
			TriMatrix::swapYZ() *
			TriMatrix::translate((float)-Viewpoint.Pos.X, (float)-Viewpoint.Pos.Y, (float)-Viewpoint.Pos.Z);

		TriMatrix swapYZ = TriMatrix::null();
		swapYZ.matrix[0 + 0 * 4] = 1.0f;
		swapYZ.matrix[1 + 2 * 4] = 1.0f;
		swapYZ.matrix[2 + 1 * 4] = 1.0f;
		swapYZ.matrix[3 + 3 * 4] = 1.0f;

		VSMatrix worldToView;
		worldToView.loadMatrix((altWorldToView * swapYZ).matrix);

		VSMatrix objectToWorld;
		worldToView.inverseMatrix(objectToWorld);
		return objectToWorld;
	}

	void SWModelRenderer::BeginDrawHUDModel(AActor *actor, const VSMatrix &objectToWorldMatrix)
	{
		ModelActor = actor;
		const_cast<VSMatrix &>(objectToWorldMatrix).copy(ObjectToWorld.matrix);
	}

	void SWModelRenderer::EndDrawHUDModel(AActor *actor)
	{
		ModelActor = nullptr;
	}

	void SWModelRenderer::SetInterpolation(double interpolation)
	{
		InterpolationFactor = (float)interpolation;
	}

	void SWModelRenderer::SetMaterial(FTexture *skin, bool clampNoFilter, int translation)
	{
		SkinTexture = skin;
	}

	void SWModelRenderer::DrawArrays(int start, int count)
	{
		const auto &viewpoint = Thread->Viewport->viewpoint;

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
		*transform = Thread->Viewport->WorldToClip * swapYZ * ObjectToWorld;

		PolyDrawArgs args;
		args.SetLight(GetColorTable(sector->Colormap, sector->SpecialColors[sector_t::sprites], true), lightlevel, Thread->Light->SpriteGlobVis(foggy), fullbrightSprite);
		args.SetTransform(transform);
		args.SetFaceCullCCW(true);
		args.SetClipPlane(0, PolyClipPlane());
		args.SetStyle(TriBlendMode::TextureOpaque);

		if (Thread->Viewport->RenderTarget->IsBgra())
			args.SetTexture((const uint8_t *)SkinTexture->GetPixelsBgra(), SkinTexture->GetWidth(), SkinTexture->GetHeight());
		else
			args.SetTexture(SkinTexture->GetPixels(DefaultRenderStyle()), SkinTexture->GetWidth(), SkinTexture->GetHeight());

		args.SetDepthTest(true);
		args.SetWriteDepth(true);
		args.SetWriteStencil(false);
		args.DrawArray(Thread->DrawQueue, VertexBuffer + start, count);
	}

	void SWModelRenderer::DrawElements(int numIndices, size_t offset)
	{
		const auto &viewpoint = Thread->Viewport->viewpoint;

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
		*transform = Thread->Viewport->WorldToClip * swapYZ * ObjectToWorld;

		PolyDrawArgs args;
		args.SetLight(GetColorTable(sector->Colormap, sector->SpecialColors[sector_t::sprites], true), lightlevel, Thread->Light->SpriteGlobVis(foggy), fullbrightSprite);
		args.SetTransform(transform);
		args.SetFaceCullCCW(true);
		args.SetClipPlane(0, PolyClipPlane());
		args.SetStyle(TriBlendMode::TextureOpaque);

		if (Thread->Viewport->RenderTarget->IsBgra())
			args.SetTexture((const uint8_t *)SkinTexture->GetPixelsBgra(), SkinTexture->GetWidth(), SkinTexture->GetHeight());
		else
			args.SetTexture(SkinTexture->GetPixels(DefaultRenderStyle()), SkinTexture->GetWidth(), SkinTexture->GetHeight());

		args.SetDepthTest(true);
		args.SetWriteDepth(true);
		args.SetWriteStencil(false);
		args.DrawElements(Thread->DrawQueue, VertexBuffer, IndexBuffer + offset / sizeof(unsigned int), numIndices);
	}

	double SWModelRenderer::GetTimeFloat()
	{
		return (double)screen->FrameTime * (double)TICRATE / 1000.0;
	}

	/////////////////////////////////////////////////////////////////////////////

	SWModelVertexBuffer::SWModelVertexBuffer(bool needindex, bool singleframe)
	{
	}

	SWModelVertexBuffer::~SWModelVertexBuffer()
	{
	}

	FModelVertex *SWModelVertexBuffer::LockVertexBuffer(unsigned int size)
	{
		mVertexBuffer.Resize(size);
		return &mVertexBuffer[0];
	}

	void SWModelVertexBuffer::UnlockVertexBuffer()
	{
	}

	unsigned int *SWModelVertexBuffer::LockIndexBuffer(unsigned int size)
	{
		mIndexBuffer.Resize(size);
		return &mIndexBuffer[0];
	}

	void SWModelVertexBuffer::UnlockIndexBuffer()
	{
	}

	void SWModelVertexBuffer::SetupFrame(FModelRenderer *renderer, unsigned int frame1, unsigned int frame2, unsigned int size)
	{
		SWModelRenderer *polyrenderer = (SWModelRenderer *)renderer;

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
}


#include "polyrenderer/backend/poly_renderstate.h"
#include "polyrenderer/backend/poly_framebuffer.h"
#include "polyrenderer/backend/poly_hwtexture.h"
#include "templates.h"
#include "doomstat.h"
#include "r_data/colormaps.h"
#include "hwrenderer/scene/hw_skydome.h"
#include "hwrenderer/scene/hw_viewpointuniforms.h"
#include "hwrenderer/dynlights/hw_lightbuffer.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "hwrenderer/utility/hw_clock.h"
#include "hwrenderer/data/flatvertices.h"
#include "hwrenderer/data/hw_viewpointbuffer.h"
#include "hwrenderer/data/shaderuniforms.h"
#include "swrenderer/r_swcolormaps.h"

static PolyDrawMode dtToDrawMode[] =
{
	PolyDrawMode::Triangles, // DT_Points
	PolyDrawMode::Triangles, // DT_Lines
	PolyDrawMode::Triangles, // DT_Triangles
	PolyDrawMode::TriangleFan, // DT_TriangleFan
	PolyDrawMode::TriangleStrip, // DT_TriangleStrip
};

PolyRenderState::PolyRenderState()
{
	Reset();
}

void PolyRenderState::ClearScreen()
{
	screen->mViewpoints->Set2D(*this, SCREENWIDTH, SCREENHEIGHT);
	SetColor(0, 0, 0);
	Draw(DT_TriangleStrip, FFlatVertexBuffer::FULLSCREEN_INDEX, 4, true);
}

void PolyRenderState::Draw(int dt, int index, int count, bool apply)
{
	if (apply)
		Apply();

	auto fb = GetPolyFrameBuffer();
	TriVertex *vertices = fb->GetFrameMemory()->AllocMemory<TriVertex>(count);
	static_cast<PolyVertexBuffer*>(mVertexBuffer)->CopyVertices(vertices, count, index);
	PolyTriangleDrawer::DrawArray(fb->GetDrawCommands(), args, vertices, count, dtToDrawMode[dt]);
}

void PolyRenderState::DrawIndexed(int dt, int index, int count, bool apply)
{
	if (apply)
		Apply();

	auto fb = GetPolyFrameBuffer();
	TriVertex *vertices = fb->GetFrameMemory()->AllocMemory<TriVertex>(count);
	static_cast<PolyVertexBuffer*>(mVertexBuffer)->CopyIndexed(vertices, (uint32_t *)mIndexBuffer->Memory(), count, index);
	PolyTriangleDrawer::DrawArray(fb->GetDrawCommands(), args, vertices, count, dtToDrawMode[dt]);
}

bool PolyRenderState::SetDepthClamp(bool on)
{
	bool lastValue = mDepthClamp;
	mDepthClamp = on;
	return lastValue;
}

void PolyRenderState::SetDepthMask(bool on)
{
	args.SetWriteDepth(on);
}

void PolyRenderState::SetDepthFunc(int func)
{
}

void PolyRenderState::SetDepthRange(float min, float max)
{
}

void PolyRenderState::SetColorMask(bool r, bool g, bool b, bool a)
{
	args.SetWriteColor(r || g || b || a);
}

void PolyRenderState::SetStencil(int offs, int op, int flags)
{
}

void PolyRenderState::SetCulling(int mode)
{
}

void PolyRenderState::EnableClipDistance(int num, bool state)
{
}

void PolyRenderState::Clear(int targets)
{
	//if (targets & CT_Color)
	//	PolyTriangleDrawer::ClearColor(GetPolyFrameBuffer()->GetDrawCommands());
	if (targets & CT_Depth)
		PolyTriangleDrawer::ClearDepth(GetPolyFrameBuffer()->GetDrawCommands(), 0.0f);
	if (targets & CT_Stencil)
		PolyTriangleDrawer::ClearStencil(GetPolyFrameBuffer()->GetDrawCommands(), 0);
}

void PolyRenderState::EnableStencil(bool on)
{
}

void PolyRenderState::SetScissor(int x, int y, int w, int h)
{
}

void PolyRenderState::SetViewport(int x, int y, int w, int h)
{
	auto fb = GetPolyFrameBuffer();
	PolyTriangleDrawer::SetViewport(fb->GetDrawCommands(), x, y, w, h, fb->GetCanvas());
}

void PolyRenderState::EnableDepthTest(bool on)
{
	args.SetDepthTest(on);
}

void PolyRenderState::EnableMultisampling(bool on)
{
}

void PolyRenderState::EnableLineSmooth(bool on)
{
}

void PolyRenderState::EnableDrawBuffers(int count)
{
}

void PolyRenderState::Apply()
{
	drawcalls.Clock();

	args.SetStencilTest(false);
	args.SetWriteStencil(false);

	FColormap cm;
	cm.Clear();
	args.SetLight(GetColorTable(cm), (int)(mLightParms[3] * 255.0f), mViewpointUniforms->mGlobVis, true);

	args.SetColor(MAKEARGB(
		static_cast<uint32_t>(mStreamData.uVertexColor.W * 255.0f + 0.5f),
		static_cast<uint32_t>(mStreamData.uVertexColor.X * 255.0f + 0.5f),
		static_cast<uint32_t>(mStreamData.uVertexColor.Y * 255.0f + 0.5f),
		static_cast<uint32_t>(mStreamData.uVertexColor.Z * 255.0f + 0.5f)), 0);

	if (mMaterial.mChanged && mMaterial.mMaterial)
	{
		auto base = static_cast<PolyHardwareTexture*>(mMaterial.mMaterial->GetLayer(0, mMaterial.mTranslation));
		if (base)
		{
			DCanvas *texcanvas = base->GetImage(mMaterial);
			args.SetTexture(texcanvas->GetPixels(), texcanvas->GetHeight(), texcanvas->GetWidth());

			if (mRenderStyle == LegacyRenderStyles[STYLE_Normal])
				args.SetStyle(TriBlendMode::Normal);
			else if (mRenderStyle == LegacyRenderStyles[STYLE_Add])
				args.SetStyle(TriBlendMode::Add);
			else if (mRenderStyle == LegacyRenderStyles[STYLE_Translucent])
				args.SetStyle(TriBlendMode::Translucent);
			else
				args.SetStyle(TriBlendMode::Opaque);
		}
		else
		{
			args.SetStyle(TriBlendMode::Fill);
		}

		mMaterial.mChanged = false;
	}

	auto fb = GetPolyFrameBuffer();
	PolyTriangleDrawer::SetTwoSided(fb->GetDrawCommands(), true);

	drawcalls.Unclock();
}

void PolyRenderState::Bind(PolyDataBuffer *buffer, uint32_t offset, uint32_t length)
{
	if (buffer->bindingpoint == VIEWPOINT_BINDINGPOINT)
	{
		mViewpointUniforms = reinterpret_cast<HWViewpointUniforms*>(static_cast<uint8_t*>(buffer->Memory()) + offset);

		Mat4f viewToProj = Mat4f::FromValues(mViewpointUniforms->mProjectionMatrix.get());
		Mat4f worldToView = Mat4f::FromValues(mViewpointUniforms->mViewMatrix.get());

		auto fb = GetPolyFrameBuffer();
		PolyTriangleDrawer::SetTransform(fb->GetDrawCommands(), fb->GetFrameMemory()->NewObject<Mat4f>(viewToProj * worldToView), nullptr);
	}
}

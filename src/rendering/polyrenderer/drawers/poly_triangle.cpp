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

#include <stddef.h>
#include "templates.h"
#include "doomdef.h"

#include "w_wad.h"
#include "v_video.h"
#include "doomstat.h"
#include "st_stuff.h"
#include "g_game.h"
#include "g_level.h"
#include "r_data/r_translate.h"
#include "r_data/models/models.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "poly_triangle.h"
#include "swrenderer/drawers/r_draw_rgba.h"
#include "screen_triangle.h"
#include "x86.h"

static bool isBgraRenderTarget = false;

bool PolyTriangleDrawer::IsBgra()
{
	return isBgraRenderTarget;
}

void PolyTriangleDrawer::ClearDepth(const DrawerCommandQueuePtr &queue, float value)
{
	queue->Push<PolyClearDepthCommand>(value);
}

void PolyTriangleDrawer::ClearStencil(const DrawerCommandQueuePtr &queue, uint8_t value)
{
	queue->Push<PolyClearStencilCommand>(value);
}

void PolyTriangleDrawer::SetViewport(const DrawerCommandQueuePtr &queue, int x, int y, int width, int height, DCanvas *canvas, PolyDepthStencil *depthstencil)
{
	uint8_t *dest = (uint8_t*)canvas->GetPixels();
	int dest_width = canvas->GetWidth();
	int dest_height = canvas->GetHeight();
	int dest_pitch = canvas->GetPitch();
	bool dest_bgra = canvas->IsBgra();
	isBgraRenderTarget = dest_bgra;

	queue->Push<PolySetViewportCommand>(x, y, width, height, dest, dest_width, dest_height, dest_pitch, dest_bgra, depthstencil);
}

void PolyTriangleDrawer::SetInputAssembly(const DrawerCommandQueuePtr &queue, PolyInputAssembly *input)
{
	queue->Push<PolySetInputAssemblyCommand>(input);
}

void PolyTriangleDrawer::SetTransform(const DrawerCommandQueuePtr &queue, const Mat4f *objectToClip, const Mat4f *objectToWorld)
{
	queue->Push<PolySetTransformCommand>(objectToClip, objectToWorld);
}

void PolyTriangleDrawer::SetCullCCW(const DrawerCommandQueuePtr &queue, bool ccw)
{
	queue->Push<PolySetCullCCWCommand>(ccw);
}

void PolyTriangleDrawer::SetTwoSided(const DrawerCommandQueuePtr &queue, bool twosided)
{
	queue->Push<PolySetTwoSidedCommand>(twosided);
}

void PolyTriangleDrawer::SetWeaponScene(const DrawerCommandQueuePtr &queue, bool enable)
{
	queue->Push<PolySetWeaponSceneCommand>(enable);
}

void PolyTriangleDrawer::SetModelVertexShader(const DrawerCommandQueuePtr &queue, int frame1, int frame2, float interpolationFactor)
{
	queue->Push<PolySetModelVertexShaderCommand>(frame1, frame2, interpolationFactor);
}

void PolyTriangleDrawer::SetVertexBuffer(const DrawerCommandQueuePtr &queue, const void *vertices)
{
	queue->Push<PolySetVertexBufferCommand>(vertices);
}

void PolyTriangleDrawer::SetIndexBuffer(const DrawerCommandQueuePtr &queue, const void *elements)
{
	queue->Push<PolySetIndexBufferCommand>(elements);
}

void PolyTriangleDrawer::PushDrawArgs(const DrawerCommandQueuePtr &queue, const PolyDrawArgs &args)
{
	queue->Push<PolyPushDrawArgsCommand>(args);
}

void PolyTriangleDrawer::SetDepthClamp(const DrawerCommandQueuePtr &queue, bool on)
{
	queue->Push<PolySetDepthClampCommand>(on);
}

void PolyTriangleDrawer::SetDepthMask(const DrawerCommandQueuePtr &queue, bool on)
{
	queue->Push<PolySetDepthMaskCommand>(on);
}

void PolyTriangleDrawer::SetDepthFunc(const DrawerCommandQueuePtr &queue, int func)
{
	queue->Push<PolySetDepthFuncCommand>(func);
}

void PolyTriangleDrawer::SetDepthRange(const DrawerCommandQueuePtr &queue, float min, float max)
{
	queue->Push<PolySetDepthRangeCommand>(min, max);
}

void PolyTriangleDrawer::SetDepthBias(const DrawerCommandQueuePtr &queue, float depthBiasConstantFactor, float depthBiasSlopeFactor)
{
	queue->Push<PolySetDepthBiasCommand>(depthBiasConstantFactor, depthBiasSlopeFactor);
}

void PolyTriangleDrawer::SetColorMask(const DrawerCommandQueuePtr &queue, bool r, bool g, bool b, bool a)
{
	queue->Push<PolySetColorMaskCommand>(r, g, b, a);
}

void PolyTriangleDrawer::SetStencil(const DrawerCommandQueuePtr &queue, int stencilRef, int op)
{
	queue->Push<PolySetStencilCommand>(stencilRef, op);
}

void PolyTriangleDrawer::SetCulling(const DrawerCommandQueuePtr &queue, int mode)
{
	queue->Push<PolySetCullingCommand>(mode);
}

void PolyTriangleDrawer::EnableClipDistance(const DrawerCommandQueuePtr &queue, int num, bool state)
{
	queue->Push<PolyEnableClipDistanceCommand>(num, state);
}

void PolyTriangleDrawer::EnableStencil(const DrawerCommandQueuePtr &queue, bool on)
{
	queue->Push<PolyEnableStencilCommand>(on);
}

void PolyTriangleDrawer::SetScissor(const DrawerCommandQueuePtr &queue, int x, int y, int w, int h)
{
	queue->Push<PolySetScissorCommand>(x, y, w, h);
}

void PolyTriangleDrawer::EnableDepthTest(const DrawerCommandQueuePtr &queue, bool on)
{
	queue->Push<PolyEnableDepthTestCommand>(on);
}

void PolyTriangleDrawer::SetRenderStyle(const DrawerCommandQueuePtr &queue, FRenderStyle style)
{
	queue->Push<PolySetRenderStyleCommand>(style);
}

void PolyTriangleDrawer::SetTexture(const DrawerCommandQueuePtr &queue, void *pixels, int width, int height)
{
	queue->Push<PolySetTextureCommand>(pixels, width, height);
}

void PolyTriangleDrawer::SetShader(const DrawerCommandQueuePtr &queue, int specialEffect, int effectState, bool alphaTest)
{
	queue->Push<PolySetShaderCommand>(specialEffect, effectState, alphaTest);
}

void PolyTriangleDrawer::PushStreamData(const DrawerCommandQueuePtr &queue, const StreamData &data, const PolyPushConstants &constants)
{
	queue->Push<PolyPushStreamDataCommand>(data, constants);
}

void PolyTriangleDrawer::PushMatrices(const DrawerCommandQueuePtr &queue, const VSMatrix &modelMatrix, const VSMatrix &normalModelMatrix, const VSMatrix &textureMatrix)
{
	queue->Push<PolyPushMatricesCommand>(modelMatrix, normalModelMatrix, textureMatrix);
}

void PolyTriangleDrawer::SetViewpointUniforms(const DrawerCommandQueuePtr &queue, const HWViewpointUniforms *uniforms)
{
	queue->Push<PolySetViewpointUniformsCommand>(uniforms);
}

void PolyTriangleDrawer::Draw(const DrawerCommandQueuePtr &queue, int index, int vcount, PolyDrawMode mode)
{
	queue->Push<PolyDrawCommand>(index, vcount, mode);
}

void PolyTriangleDrawer::DrawIndexed(const DrawerCommandQueuePtr &queue, int index, int count, PolyDrawMode mode)
{
	queue->Push<PolyDrawIndexedCommand>(index, count, mode);
}

/////////////////////////////////////////////////////////////////////////////

void PolyTriangleThreadData::ClearDepth(float value)
{
	int width = depthstencil->Width();
	int height = depthstencil->Height();
	float *data = depthstencil->DepthValues();

	int skip = skipped_by_thread(0);
	int count = count_for_thread(0, height);

	data += skip * width;
	for (int i = 0; i < count; i++)
	{
		for (int x = 0; x < width; x++)
			data[x] = value;
		data += num_cores * width;
	}
}

void PolyTriangleThreadData::ClearStencil(uint8_t value)
{
	int width = depthstencil->Width();
	int height = depthstencil->Height();
	uint8_t *data = depthstencil->StencilValues();

	int skip = skipped_by_thread(0);
	int count = count_for_thread(0, height);

	data += skip * width;
	for (int i = 0; i < count; i++)
	{
		memset(data, value, width);
		data += num_cores * width;
	}
}

void PolyTriangleThreadData::SetViewport(int x, int y, int width, int height, uint8_t *new_dest, int new_dest_width, int new_dest_height, int new_dest_pitch, bool new_dest_bgra, PolyDepthStencil *new_depthstencil)
{
	viewport_x = x;
	viewport_y = y;
	viewport_width = width;
	viewport_height = height;
	dest = new_dest;
	dest_width = new_dest_width;
	dest_height = new_dest_height;
	dest_pitch = new_dest_pitch;
	dest_bgra = new_dest_bgra;
	depthstencil = new_depthstencil;
	UpdateClip();
}

void PolyTriangleThreadData::SetScissor(int x, int y, int w, int h)
{
	scissor.left = x;
	scissor.right = x + w;
	scissor.top = y;
	scissor.bottom = y + h;
	UpdateClip();
}

void PolyTriangleThreadData::UpdateClip()
{
	clip.left = MAX(MAX(viewport_x, scissor.left), 0);
	clip.top = MAX(MAX(viewport_y, scissor.top), 0);
	clip.right = MIN(MIN(viewport_x + viewport_width, scissor.right), dest_width);
	clip.bottom = MIN(MIN(viewport_y + viewport_height, scissor.bottom), dest_height);
}

void PolyTriangleThreadData::SetTransform(const Mat4f *newObjectToClip, const Mat4f *newObjectToWorld)
{
	swVertexShader.objectToClip = newObjectToClip;
	swVertexShader.objectToWorld = newObjectToWorld;
}

void PolyTriangleThreadData::PushDrawArgs(const PolyDrawArgs &args)
{
	drawargs = args;
}

void PolyTriangleThreadData::PushStreamData(const StreamData &data, const PolyPushConstants &constants)
{
	mainVertexShader.Data = data;
	mainVertexShader.uClipSplit = constants.uClipSplit;

	FColormap cm;
	cm.Clear();
	drawargs.SetLight(GetColorTable(cm), (int)(constants.uLightLevel * 255.0f), mainVertexShader.Viewpoint->mGlobVis * 32.0f, false);

	if (SpecialEffect != EFF_NONE)
	{
		// To do: need new drawers for these
		switch (SpecialEffect)
		{
		default: break;
		case EFF_FOGBOUNDARY: drawargs.SetStyle(TriBlendMode::FogBoundary); break;
		case EFF_SPHEREMAP: drawargs.SetStyle(TriBlendMode::Fill); break;
		case EFF_BURN: drawargs.SetStyle(TriBlendMode::Fill); break;
		case EFF_STENCIL: drawargs.SetStyle(TriBlendMode::Fill); break;
		}
	}
	else
	{
		switch (EffectState)
		{
		default:
			break;
		case SHADER_Paletted:
			break;
		case SHADER_NoTexture:
			drawargs.SetStyle(TriBlendMode::FillTranslucent);
			return;
		case SHADER_BasicFuzz:
		case SHADER_SmoothFuzz:
		case SHADER_SwirlyFuzz:
		case SHADER_TranslucentFuzz:
		case SHADER_JaggedFuzz:
		case SHADER_NoiseFuzz:
		case SHADER_SmoothNoiseFuzz:
		case SHADER_SoftwareFuzz:
			drawargs.SetStyle(TriBlendMode::Fuzzy);
			return;
		}

		auto style = RenderStyle;
		if (style.BlendOp == STYLEOP_Add && style.SrcAlpha == STYLEALPHA_One && style.DestAlpha == STYLEALPHA_Zero)
		{
			drawargs.SetStyle(AlphaTest ? TriBlendMode::Normal : TriBlendMode::Opaque);
		}
		else if (style.BlendOp == STYLEOP_Add && style.SrcAlpha == STYLEALPHA_Src && style.DestAlpha == STYLEALPHA_InvSrc)
		{
			drawargs.SetStyle(TriBlendMode::Normal);
		}
		else if (style.BlendOp == STYLEOP_Add && style.SrcAlpha == STYLEALPHA_SrcCol && style.DestAlpha == STYLEALPHA_One)
		{
			drawargs.SetStyle(TriBlendMode::SrcColor);
		}
		else
		{
			if (style == LegacyRenderStyles[STYLE_Normal]) drawargs.SetStyle(TriBlendMode::Normal);
			else if (style == LegacyRenderStyles[STYLE_Stencil]) drawargs.SetStyle(TriBlendMode::Stencil);
			else if (style == LegacyRenderStyles[STYLE_Translucent]) drawargs.SetStyle(TriBlendMode::Translucent);
			else if (style == LegacyRenderStyles[STYLE_Add]) drawargs.SetStyle(TriBlendMode::Add);
			//else if (style == LegacyRenderStyles[STYLE_Shaded]) drawargs.SetStyle(TriBlendMode::Shaded);
			else if (style == LegacyRenderStyles[STYLE_TranslucentStencil]) drawargs.SetStyle(TriBlendMode::TranslucentStencil);
			else if (style == LegacyRenderStyles[STYLE_Shadow]) drawargs.SetStyle(TriBlendMode::Shadow);
			else if (style == LegacyRenderStyles[STYLE_Subtract]) drawargs.SetStyle(TriBlendMode::Subtract);
			else if (style == LegacyRenderStyles[STYLE_AddStencil]) drawargs.SetStyle(TriBlendMode::AddStencil);
			else if (style == LegacyRenderStyles[STYLE_AddShaded]) drawargs.SetStyle(TriBlendMode::AddShaded);
			//else if (style == LegacyRenderStyles[STYLE_Multiply]) drawargs.SetStyle(TriBlendMode::Multiply);
			//else if (style == LegacyRenderStyles[STYLE_InverseMultiply]) drawargs.SetStyle(TriBlendMode::InverseMultiply);
			//else if (style == LegacyRenderStyles[STYLE_ColorBlend]) drawargs.SetStyle(TriBlendMode::ColorBlend);
			else if (style == LegacyRenderStyles[STYLE_Source]) drawargs.SetStyle(TriBlendMode::Opaque);
			//else if (style == LegacyRenderStyles[STYLE_ColorAdd]) drawargs.SetStyle(TriBlendMode::ColorAdd);
			else drawargs.SetStyle(TriBlendMode::Opaque);
		}
	}
}

void PolyTriangleThreadData::PushMatrices(const VSMatrix &modelMatrix, const VSMatrix &normalModelMatrix, const VSMatrix &textureMatrix)
{
	mainVertexShader.ModelMatrix = modelMatrix;
	mainVertexShader.NormalModelMatrix = normalModelMatrix;
	mainVertexShader.TextureMatrix = textureMatrix;
}

void PolyTriangleThreadData::SetViewpointUniforms(const HWViewpointUniforms *uniforms)
{
	mainVertexShader.Viewpoint = uniforms;
}

void PolyTriangleThreadData::SetDepthClamp(bool on)
{
}

void PolyTriangleThreadData::SetDepthMask(bool on)
{
	drawargs.SetWriteDepth(on);
}

void PolyTriangleThreadData::SetDepthFunc(int func)
{
	if (func == DF_LEqual || func == DF_Less)
	{
		drawargs.SetDepthTest(true);
	}
	else if (func == DF_Always)
	{
		drawargs.SetDepthTest(false);
	}
}

void PolyTriangleThreadData::SetDepthRange(float min, float max)
{
	// The only two variants used by hwrenderer layer
	if (min == 0.0f && max == 1.0f)
	{
	}
	else if (min == 1.0f && max == 1.0f)
	{
	}
}

void PolyTriangleThreadData::SetDepthBias(float depthBiasConstantFactor, float depthBiasSlopeFactor)
{
	depthbias = (float)(depthBiasConstantFactor / 2500.0);
}

void PolyTriangleThreadData::SetColorMask(bool r, bool g, bool b, bool a)
{
	drawargs.SetWriteColor(r);
}

void PolyTriangleThreadData::SetStencil(int stencilRef, int op)
{
	drawargs.SetStencilTestValue(stencilRef);
	if (op == SOP_Increment)
	{
		drawargs.SetWriteStencil(drawargs.StencilTest(), MIN(stencilRef + 1, (int)255));
	}
	else if (op == SOP_Decrement)
	{
		drawargs.SetWriteStencil(drawargs.StencilTest(), MAX(stencilRef - 1, (int)0));
	}
	else // SOP_Keep
	{
		drawargs.SetWriteStencil(false, stencilRef);
	}
}

void PolyTriangleThreadData::SetCulling(int mode)
{
	SetTwoSided(mode == Cull_None);
	SetCullCCW(mode == Cull_CCW);
}

void PolyTriangleThreadData::EnableClipDistance(int num, bool state)
{
}

void PolyTriangleThreadData::EnableStencil(bool on)
{
	drawargs.SetStencilTest(on);
	drawargs.SetWriteStencil(on && drawargs.StencilTestValue() != drawargs.StencilWriteValue(), drawargs.StencilWriteValue());
}

void PolyTriangleThreadData::EnableDepthTest(bool on)
{
	drawargs.SetDepthTest(on);
}

void PolyTriangleThreadData::SetRenderStyle(FRenderStyle style)
{
	RenderStyle = style;
}

void PolyTriangleThreadData::SetShader(int specialEffect, int effectState, bool alphaTest)
{
	SpecialEffect = specialEffect;
	EffectState = effectState;
	AlphaTest = alphaTest;
}

void PolyTriangleThreadData::SetTexture(void *pixels, int width, int height)
{
	drawargs.SetTexture((uint8_t*)pixels, width, height);
}

void PolyTriangleThreadData::DrawIndexed(int index, int vcount, PolyDrawMode drawmode)
{
	if (vcount < 3)
		return;

	elements += index;

	TriDrawTriangleArgs args;
	args.uniforms = &drawargs;

	ShadedTriVertex vertbuffer[3];
	ShadedTriVertex *vert[3] = { &vertbuffer[0], &vertbuffer[1], &vertbuffer[2] };
	if (drawmode == PolyDrawMode::Triangles)
	{
		for (int i = 0; i < vcount / 3; i++)
		{
			for (int j = 0; j < 3; j++)
				*vert[j] = ShadeVertex(*(elements++));
			DrawShadedTriangle(vert, ccw, &args);
		}
	}
	else if (drawmode == PolyDrawMode::TriangleFan)
	{
		*vert[0] = ShadeVertex(*(elements++));
		*vert[1] = ShadeVertex(*(elements++));
		for (int i = 2; i < vcount; i++)
		{
			*vert[2] = ShadeVertex(*(elements++));
			DrawShadedTriangle(vert, ccw, &args);
			std::swap(vert[1], vert[2]);
		}
	}
	else // TriangleDrawMode::TriangleStrip
	{
		bool toggleccw = ccw;
		*vert[0] = ShadeVertex(*(elements++));
		*vert[1] = ShadeVertex(*(elements++));
		for (int i = 2; i < vcount; i++)
		{
			*vert[2] = ShadeVertex(*(elements++));
			DrawShadedTriangle(vert, toggleccw, &args);
			ShadedTriVertex *vtmp = vert[0];
			vert[0] = vert[1];
			vert[1] = vert[2];
			vert[2] = vtmp;
			toggleccw = !toggleccw;
		}
	}
}

void PolyTriangleThreadData::Draw(int index, int vcount, PolyDrawMode drawmode)
{
	if (vcount < 3)
		return;

	TriDrawTriangleArgs args;
	args.uniforms = &drawargs;

	int vinput = index;

	ShadedTriVertex vertbuffer[3];
	ShadedTriVertex *vert[3] = { &vertbuffer[0], &vertbuffer[1], &vertbuffer[2] };
	if (drawmode == PolyDrawMode::Triangles)
	{
		for (int i = 0; i < vcount / 3; i++)
		{
			for (int j = 0; j < 3; j++)
				*vert[j] = ShadeVertex(vinput++);
			DrawShadedTriangle(vert, ccw, &args);
		}
	}
	else if (drawmode == PolyDrawMode::TriangleFan)
	{
		*vert[0] = ShadeVertex(vinput++);
		*vert[1] = ShadeVertex(vinput++);
		for (int i = 2; i < vcount; i++)
		{
			*vert[2] = ShadeVertex(vinput++);
			DrawShadedTriangle(vert, ccw, &args);
			std::swap(vert[1], vert[2]);
		}
	}
	else // TriangleDrawMode::TriangleStrip
	{
		bool toggleccw = ccw;
		*vert[0] = ShadeVertex(vinput++);
		*vert[1] = ShadeVertex(vinput++);
		for (int i = 2; i < vcount; i++)
		{
			*vert[2] = ShadeVertex(vinput++);
			DrawShadedTriangle(vert, toggleccw, &args);
			ShadedTriVertex *vtmp = vert[0];
			vert[0] = vert[1];
			vert[1] = vert[2];
			vert[2] = vtmp;
			toggleccw = !toggleccw;
		}
	}
}

ShadedTriVertex PolyTriangleThreadData::ShadeVertex(int index)
{
	inputAssembly->Load(this, vertices, index);
	mainVertexShader.main();
	return mainVertexShader;
}

void PolySWInputAssembly::Load(PolyTriangleThreadData *thread, const void *vertices, int index)
{
	if (thread->modelFrame1 == -1)
	{
		thread->swVertexShader.v1 = static_cast<const TriVertex*>(vertices)[index];
	}
	else
	{
		const FModelVertex &v1 = static_cast<const FModelVertex*>(vertices)[thread->modelFrame1 + index];
		const FModelVertex &v2 = static_cast<const FModelVertex*>(vertices)[thread->modelFrame2 + index];

		thread->swVertexShader.v1.x = v1.x;
		thread->swVertexShader.v1.y = v1.y;
		thread->swVertexShader.v1.z = v1.z;
		thread->swVertexShader.v1.w = 1.0f;
		thread->swVertexShader.v1.u = v1.u;
		thread->swVertexShader.v1.v = v1.v;

		thread->swVertexShader.v2.x = v2.x;
		thread->swVertexShader.v2.y = v2.y;
		thread->swVertexShader.v2.z = v2.z;
		thread->swVertexShader.v2.w = 1.0f;
		thread->swVertexShader.v2.u = v2.u;
		thread->swVertexShader.v2.v = v2.v;
	}
}

bool PolyTriangleThreadData::IsDegenerate(const ShadedTriVertex *const* vert)
{
	// A degenerate triangle has a zero cross product for two of its sides.
	float ax = vert[1]->gl_Position.X - vert[0]->gl_Position.X;
	float ay = vert[1]->gl_Position.Y - vert[0]->gl_Position.Y;
	float az = vert[1]->gl_Position.W - vert[0]->gl_Position.W;
	float bx = vert[2]->gl_Position.X - vert[0]->gl_Position.X;
	float by = vert[2]->gl_Position.Y - vert[0]->gl_Position.Y;
	float bz = vert[2]->gl_Position.W - vert[0]->gl_Position.W;
	float crossx = ay * bz - az * by;
	float crossy = az * bx - ax * bz;
	float crossz = ax * by - ay * bx;
	float crosslengthsqr = crossx * crossx + crossy * crossy + crossz * crossz;
	return crosslengthsqr <= 1.e-6f;
}

bool PolyTriangleThreadData::IsFrontfacing(TriDrawTriangleArgs *args)
{
	float a =
		args->v1->x * args->v2->y - args->v2->x * args->v1->y +
		args->v2->x * args->v3->y - args->v3->x * args->v2->y +
		args->v3->x * args->v1->y - args->v1->x * args->v3->y;
	return a <= 0.0f;
}

void PolyTriangleThreadData::DrawShadedTriangle(const ShadedTriVertex *const* vert, bool ccw, TriDrawTriangleArgs *args)
{
	// Reject triangle if degenerate
	if (IsDegenerate(vert))
		return;

	drawargs.SetColor(vert[0]->vColor, 0);

	// Cull, clip and generate additional vertices as needed
	ScreenTriVertex clippedvert[max_additional_vertices];
	int numclipvert = ClipEdge(vert);

	// Convert barycentric weights to actual vertices
	for (int i = 0; i < numclipvert; i++)
	{
		auto &v = clippedvert[i];
		memset(&v, 0, sizeof(ScreenTriVertex));
		for (int w = 0; w < 3; w++)
		{
			float weight = weights[i * 3 + w];
			v.x += vert[w]->gl_Position.X * weight;
			v.y += vert[w]->gl_Position.Y * weight;
			v.z += vert[w]->gl_Position.Z * weight;
			v.w += vert[w]->gl_Position.W * weight;
			v.u += vert[w]->vTexCoord.X * weight;
			v.v += vert[w]->vTexCoord.Y * weight;
			v.worldX += vert[w]->pixelpos.X * weight;
			v.worldY += vert[w]->pixelpos.Y * weight;
			v.worldZ += vert[w]->pixelpos.Z * weight;
		}
	}

#ifdef NO_SSE
	// Map to 2D viewport:
	for (int j = 0; j < numclipvert; j++)
	{
		auto &v = clippedvert[j];

		// Calculate normalized device coordinates:
		v.w = 1.0f / v.w;
		v.x *= v.w;
		v.y *= v.w;
		v.z *= v.w;

		// Apply viewport scale to get screen coordinates:
		v.x = viewport_x + viewport_width * (1.0f + v.x) * 0.5f;
		v.y = viewport_y + viewport_height * (1.0f - v.y) * 0.5f;
	}
#else
	// Map to 2D viewport:
	__m128 mviewport_x = _mm_set1_ps((float)viewport_x);
	__m128 mviewport_y = _mm_set1_ps((float)viewport_y);
	__m128 mviewport_halfwidth = _mm_set1_ps(viewport_width * 0.5f);
	__m128 mviewport_halfheight = _mm_set1_ps(viewport_height * 0.5f);
	__m128 mone = _mm_set1_ps(1.0f);
	int sse_length = (numclipvert + 3) / 4 * 4;
	for (int j = 0; j < sse_length; j += 4)
	{
		__m128 vx = _mm_loadu_ps(&clippedvert[j].x);
		__m128 vy = _mm_loadu_ps(&clippedvert[j + 1].x);
		__m128 vz = _mm_loadu_ps(&clippedvert[j + 2].x);
		__m128 vw = _mm_loadu_ps(&clippedvert[j + 3].x);
		_MM_TRANSPOSE4_PS(vx, vy, vz, vw);

		// Calculate normalized device coordinates:
		vw = _mm_div_ps(mone, vw);
		vx = _mm_mul_ps(vx, vw);
		vy = _mm_mul_ps(vy, vw);
		vz = _mm_mul_ps(vz, vw);

		// Apply viewport scale to get screen coordinates:
		vx = _mm_add_ps(mviewport_x, _mm_mul_ps(mviewport_halfwidth, _mm_add_ps(mone, vx)));
		vy = _mm_add_ps(mviewport_y, _mm_mul_ps(mviewport_halfheight, _mm_sub_ps(mone, vy)));

		_MM_TRANSPOSE4_PS(vx, vy, vz, vw);
		_mm_storeu_ps(&clippedvert[j].x, vx);
		_mm_storeu_ps(&clippedvert[j + 1].x, vy);
		_mm_storeu_ps(&clippedvert[j + 2].x, vz);
		_mm_storeu_ps(&clippedvert[j + 3].x, vw);
	}
#endif

	// Keep varyings in -128 to 128 range if possible
	// But don't do this for the skycap mode since the V texture coordinate is used for blending
	if (numclipvert > 0 && args->uniforms->BlendMode() != TriBlendMode::Skycap)
	{
		float newOriginU = floorf(clippedvert[0].u * 0.1f) * 10.0f;
		float newOriginV = floorf(clippedvert[0].v * 0.1f) * 10.0f;
		for (int i = 0; i < numclipvert; i++)
		{
			clippedvert[i].u -= newOriginU;
			clippedvert[i].v -= newOriginV;
		}
	}

	if (twosided && numclipvert > 2)
	{
		args->v1 = &clippedvert[0];
		args->v2 = &clippedvert[1];
		args->v3 = &clippedvert[2];
		ccw = !IsFrontfacing(args);
	}

	// Draw screen triangles
	if (ccw)
	{
		for (int i = numclipvert - 1; i > 1; i--)
		{
			args->v1 = &clippedvert[numclipvert - 1];
			args->v2 = &clippedvert[i - 1];
			args->v3 = &clippedvert[i - 2];
			if (IsFrontfacing(args) == ccw && args->CalculateGradients())
			{
				ScreenTriangle::Draw(args, this);
			}
		}
	}
	else
	{
		for (int i = 2; i < numclipvert; i++)
		{
			args->v1 = &clippedvert[0];
			args->v2 = &clippedvert[i - 1];
			args->v3 = &clippedvert[i];
			if (IsFrontfacing(args) != ccw && args->CalculateGradients())
			{
				ScreenTriangle::Draw(args, this);
			}
		}
	}
}

int PolyTriangleThreadData::ClipEdge(const ShadedTriVertex *const* verts)
{
	// use barycentric weights for clipped vertices
	weights = weightsbuffer;
	for (int i = 0; i < 3; i++)
	{
		weights[i * 3 + 0] = 0.0f;
		weights[i * 3 + 1] = 0.0f;
		weights[i * 3 + 2] = 0.0f;
		weights[i * 3 + i] = 1.0f;
	}

	// Clip and cull so that the following is true for all vertices:
	// -v.w <= v.x <= v.w
	// -v.w <= v.y <= v.w
	// -v.w <= v.z <= v.w
	
	// halfspace clip distances
	static const int numclipdistances = 9;
#ifdef NO_SSE
	float clipdistance[numclipdistances * 3];
	bool needsclipping = false;
	float *clipd = clipdistance;
	for (int i = 0; i < 3; i++)
	{
		const auto &v = *verts[i];
		clipd[0] = v.gl_Position.X + v.gl_Position.W;
		clipd[1] = v.gl_Position.W - v.gl_Position.X;
		clipd[2] = v.gl_Position.Y + v.gl_Position.W;
		clipd[3] = v.gl_Position.W - v.gl_Position.Y;
		clipd[4] = v.gl_Position.Z + v.gl_Position.W;
		clipd[5] = v.gl_Position.W - v.gl_Position.Z;
		clipd[6] = v.gl_ClipDistance[0];
		clipd[7] = v.gl_ClipDistance[1];
		clipd[8] = v.gl_ClipDistance[2];
		for (int j = 0; j < 9; j++)
			needsclipping = needsclipping || clipd[i];
		clipd += numclipdistances;
	}

	// If all halfspace clip distances are positive then the entire triangle is visible. Skip the expensive clipping step.
	if (!needsclipping)
	{
		return 3;
	}
#else
	__m128 mx = _mm_loadu_ps(&verts[0]->gl_Position.X);
	__m128 my = _mm_loadu_ps(&verts[1]->gl_Position.X);
	__m128 mz = _mm_loadu_ps(&verts[2]->gl_Position.X);
	__m128 mw = _mm_setzero_ps();
	_MM_TRANSPOSE4_PS(mx, my, mz, mw);
	__m128 clipd0 = _mm_add_ps(mx, mw);
	__m128 clipd1 = _mm_sub_ps(mw, mx);
	__m128 clipd2 = _mm_add_ps(my, mw);
	__m128 clipd3 = _mm_sub_ps(mw, my);
	__m128 clipd4 = _mm_add_ps(mz, mw);
	__m128 clipd5 = _mm_sub_ps(mw, mz);
	__m128 clipd6 = _mm_setr_ps(verts[0]->gl_ClipDistance[0], verts[1]->gl_ClipDistance[0], verts[2]->gl_ClipDistance[0], 0.0f);
	__m128 clipd7 = _mm_setr_ps(verts[0]->gl_ClipDistance[1], verts[1]->gl_ClipDistance[1], verts[2]->gl_ClipDistance[1], 0.0f);
	__m128 clipd8 = _mm_setr_ps(verts[0]->gl_ClipDistance[2], verts[1]->gl_ClipDistance[2], verts[2]->gl_ClipDistance[2], 0.0f);
	__m128 mneedsclipping = _mm_cmplt_ps(clipd0, _mm_setzero_ps());
	mneedsclipping = _mm_or_ps(mneedsclipping, _mm_cmplt_ps(clipd1, _mm_setzero_ps()));
	mneedsclipping = _mm_or_ps(mneedsclipping, _mm_cmplt_ps(clipd2, _mm_setzero_ps()));
	mneedsclipping = _mm_or_ps(mneedsclipping, _mm_cmplt_ps(clipd3, _mm_setzero_ps()));
	mneedsclipping = _mm_or_ps(mneedsclipping, _mm_cmplt_ps(clipd4, _mm_setzero_ps()));
	mneedsclipping = _mm_or_ps(mneedsclipping, _mm_cmplt_ps(clipd5, _mm_setzero_ps()));
	mneedsclipping = _mm_or_ps(mneedsclipping, _mm_cmplt_ps(clipd6, _mm_setzero_ps()));
	mneedsclipping = _mm_or_ps(mneedsclipping, _mm_cmplt_ps(clipd7, _mm_setzero_ps()));
	mneedsclipping = _mm_or_ps(mneedsclipping, _mm_cmplt_ps(clipd8, _mm_setzero_ps()));
	if (_mm_movemask_ps(mneedsclipping) == 0)
	{
		return 3;
	}
	float clipdistance[numclipdistances * 4];
	_mm_storeu_ps(clipdistance, clipd0);
	_mm_storeu_ps(clipdistance + 4, clipd1);
	_mm_storeu_ps(clipdistance + 8, clipd2);
	_mm_storeu_ps(clipdistance + 12, clipd3);
	_mm_storeu_ps(clipdistance + 16, clipd4);
	_mm_storeu_ps(clipdistance + 20, clipd5);
	_mm_storeu_ps(clipdistance + 24, clipd6);
	_mm_storeu_ps(clipdistance + 28, clipd7);
	_mm_storeu_ps(clipdistance + 32, clipd8);
#endif

	// Clip against each halfspace
	float *input = weights;
	float *output = weights + max_additional_vertices * 3;
	int inputverts = 3;
	for (int p = 0; p < numclipdistances; p++)
	{
		// Clip each edge
		int outputverts = 0;
		for (int i = 0; i < inputverts; i++)
		{
			int j = (i + 1) % inputverts;
#ifdef NO_SSE
			float clipdistance1 =
				clipdistance[0 * numclipdistances + p] * input[i * 3 + 0] +
				clipdistance[1 * numclipdistances + p] * input[i * 3 + 1] +
				clipdistance[2 * numclipdistances + p] * input[i * 3 + 2];

			float clipdistance2 =
				clipdistance[0 * numclipdistances + p] * input[j * 3 + 0] +
				clipdistance[1 * numclipdistances + p] * input[j * 3 + 1] +
				clipdistance[2 * numclipdistances + p] * input[j * 3 + 2];
#else
			float clipdistance1 =
				clipdistance[0 + p * 4] * input[i * 3 + 0] +
				clipdistance[1 + p * 4] * input[i * 3 + 1] +
				clipdistance[2 + p * 4] * input[i * 3 + 2];

			float clipdistance2 =
				clipdistance[0 + p * 4] * input[j * 3 + 0] +
				clipdistance[1 + p * 4] * input[j * 3 + 1] +
				clipdistance[2 + p * 4] * input[j * 3 + 2];
#endif

			// Clip halfspace
			if ((clipdistance1 >= 0.0f || clipdistance2 >= 0.0f) && outputverts + 1 < max_additional_vertices)
			{
				float t1 = (clipdistance1 < 0.0f) ? MAX(-clipdistance1 / (clipdistance2 - clipdistance1), 0.0f) : 0.0f;
				float t2 = (clipdistance2 < 0.0f) ? MIN(1.0f + clipdistance2 / (clipdistance1 - clipdistance2), 1.0f) : 1.0f;

				// add t1 vertex
				for (int k = 0; k < 3; k++)
					output[outputverts * 3 + k] = input[i * 3 + k] * (1.0f - t1) + input[j * 3 + k] * t1;
				outputverts++;
			
				if (t2 != 1.0f && t2 > t1)
				{
					// add t2 vertex
					for (int k = 0; k < 3; k++)
						output[outputverts * 3 + k] = input[i * 3 + k] * (1.0f - t2) + input[j * 3 + k] * t2;
					outputverts++;
				}
			}
		}
		std::swap(input, output);
		inputverts = outputverts;
		if (inputverts == 0)
			break;
	}

	weights = input;
	return inputverts;
}

PolyTriangleThreadData *PolyTriangleThreadData::Get(DrawerThread *thread)
{
	if (!thread->poly)
		thread->poly = std::make_shared<PolyTriangleThreadData>(thread->core, thread->num_cores, thread->numa_node, thread->num_numa_nodes, thread->numa_start_y, thread->numa_end_y);
	return thread->poly.get();
}

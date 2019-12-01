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
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "poly_triangle.h"
#include "swrenderer/drawers/r_draw_rgba.h"
#include "screen_triangle.h"
#include "x86.h"
#include <cmath>

#ifdef NO_SSE
static void WriteW(int y, int x0, int x1, const TriDrawTriangleArgs* args, PolyTriangleThreadData* thread)
{
	float startX = x0 + (0.5f - args->v1->x);
	float startY = y + (0.5f - args->v1->y);

	float posW = args->v1->w + args->gradientX.W * startX + args->gradientY.W * startY;
	float stepW = args->gradientX.W;
	float* w = thread->scanline.W;
	for (int x = x0; x < x1; x++)
	{
		w[x] = 1.0f / posW;
		posW += stepW;
	}
}
#else
static void WriteW(int y, int x0, int x1, const TriDrawTriangleArgs* args, PolyTriangleThreadData* thread)
{
	float startX = x0 + (0.5f - args->v1->x);
	float startY = y + (0.5f - args->v1->y);

	float posW = args->v1->w + args->gradientX.W * startX + args->gradientY.W * startY;
	float stepW = args->gradientX.W;
	float* w = thread->scanline.W;

	int ssecount = ((x1 - x0) & 3);
	int sseend = x0 + ssecount;

	__m128 mstepW = _mm_set1_ps(stepW * 4.0f);
	__m128 mposW = _mm_setr_ps(posW, posW + stepW, posW + stepW + stepW, posW + stepW + stepW + stepW);

	for (int x = x0; x < sseend; x += 4)
	{
		_mm_storeu_ps(w + x, _mm_rcp_ps(mposW));
		mposW = _mm_add_ps(mposW, mstepW);
	}

	posW += ssecount * stepW;
	for (int x = sseend; x < x1; x++)
	{
		w[x] = 1.0f / posW;
		posW += stepW;
	}
}
#endif

static void WriteLightArray(int y, int x0, int x1, const TriDrawTriangleArgs* args, PolyTriangleThreadData* thread)
{
	float startX = x0 + (0.5f - args->v1->x);
	float startY = y + (0.5f - args->v1->y);
	float posW = args->v1->w + args->gradientX.W * startX + args->gradientY.W * startY;
	float stepW = args->gradientX.W;

	float globVis = thread->mainVertexShader.Viewpoint->mGlobVis;

	uint32_t light = (int)(thread->PushConstants->uLightLevel * 255.0f);
	fixed_t shade = (fixed_t)((2.0f - (light + 12.0f) / 128.0f) * (float)FRACUNIT);
	fixed_t lightpos = (fixed_t)(globVis * posW * (float)FRACUNIT);
	fixed_t lightstep = (fixed_t)(globVis * stepW * (float)FRACUNIT);

	fixed_t maxvis = 24 * FRACUNIT / 32;
	fixed_t maxlight = 31 * FRACUNIT / 32;

	uint16_t *lightarray = thread->scanline.lightarray;

	fixed_t lightend = lightpos + lightstep * (x1 - x0);
	if (lightpos < maxvis && shade >= lightpos && shade - lightpos <= maxlight &&
		lightend < maxvis && shade >= lightend && shade - lightend <= maxlight)
	{
		//if (BitsPerPixel == 32)
		{
			lightpos += FRACUNIT - shade;
			for (int x = x0; x < x1; x++)
			{
				lightarray[x] = lightpos >> 8;
				lightpos += lightstep;
			}
		}
		/*else
		{
			lightpos = shade - lightpos;
			for (int x = x0; x < x1; x++)
			{
				lightarray[x] = (lightpos >> 3) & 0xffffff00;
				lightpos -= lightstep;
			}
		}*/
	}
	else
	{
		//if (BitsPerPixel == 32)
		{
			for (int x = x0; x < x1; x++)
			{
				lightarray[x] = (FRACUNIT - clamp<fixed_t>(shade - MIN(maxvis, lightpos), 0, maxlight)) >> 8;
				lightpos += lightstep;
			}
		}
		/*else
		{
			for (int x = x0; x < x1; x++)
			{
				lightarray[x] = (clamp<fixed_t>(shade - MIN(maxvis, lightpos), 0, maxlight) >> 3) & 0xffffff00;
				lightpos += lightstep;
			}
		}*/
	}
}

#ifdef NO_SSE
static void WriteVarying(float pos, float step, int x0, int x1, const float* w, float* varying)
{
	for (int x = x0; x < x1; x++)
	{
		varying[x] = pos * w[x];
		pos += step;
	}
}
#else
static void WriteVarying(float pos, float step, int x0, int x1, const float* w, float* varying)
{
	int ssecount = ((x1 - x0) & 3);
	int sseend = x0 + ssecount;

	__m128 mstep = _mm_set1_ps(step * 4.0f);
	__m128 mpos = _mm_setr_ps(pos, pos + step, pos + step + step, pos + step + step + step);

	for (int x = x0; x < sseend; x += 4)
	{
		_mm_storeu_ps(varying + x, _mm_mul_ps(mpos, _mm_loadu_ps(w + x)));
		mpos = _mm_add_ps(mpos, mstep);
	}

	pos += ssecount * step;
	for (int x = sseend; x < x1; x++)
	{
		varying[x] = pos * w[x];
		pos += step;
	}
}
#endif

static void WriteVaryings(int y, int x0, int x1, const TriDrawTriangleArgs* args, PolyTriangleThreadData* thread)
{
	float startX = x0 + (0.5f - args->v1->x);
	float startY = y + (0.5f - args->v1->y);

	WriteVarying(args->v1->u * args->v1->w + args->gradientX.U * startX + args->gradientY.U * startY, args->gradientX.U, x0, x1, thread->scanline.W, thread->scanline.U);
	WriteVarying(args->v1->v * args->v1->w + args->gradientX.V * startX + args->gradientY.V * startY, args->gradientX.V, x0, x1, thread->scanline.W, thread->scanline.V);
	WriteVarying(args->v1->worldX * args->v1->w + args->gradientX.WorldX * startX + args->gradientY.WorldX * startY, args->gradientX.WorldX, x0, x1, thread->scanline.W, thread->scanline.WorldX);
	WriteVarying(args->v1->worldY * args->v1->w + args->gradientX.WorldY * startX + args->gradientY.WorldY * startY, args->gradientX.WorldY, x0, x1, thread->scanline.W, thread->scanline.WorldY);
	WriteVarying(args->v1->worldZ * args->v1->w + args->gradientX.WorldZ * startX + args->gradientY.WorldZ * startY, args->gradientX.WorldZ, x0, x1, thread->scanline.W, thread->scanline.WorldZ);
}

static uint32_t BlendColor(FRenderStyle style, uint32_t fg, uint32_t bg)
{
	static const int shiftTable[] = {
		0, 0, 0, 0, // STYLEALPHA_Zero
		0, 0, 0, 0, // STYLEALPHA_One
		24, 24, 24, 24, // STYLEALPHA_Src
		24, 24, 24, 24, // STYLEALPHA_InvSrc
		24, 16, 8, 0, // STYLEALPHA_SrcCol
		24, 16, 8, 0, // STYLEALPHA_InvSrcCol
		24, 16, 8, 0, // STYLEALPHA_DstCol
		24, 16, 8, 0 // STYLEALPHA_InvDstCol
	};

	bool invsrc = style.SrcAlpha & 1;
	bool invdst = style.DestAlpha & 1;

	int srcinput = style.SrcAlpha <= STYLEALPHA_One ? 0 : (style.SrcAlpha >= STYLEALPHA_DstCol ? bg : fg);
	int dstinput = style.DestAlpha <= STYLEALPHA_One ? 0 : (style.DestAlpha >= STYLEALPHA_DstCol ? bg : fg);

	const int* shiftsrc = shiftTable + (style.SrcAlpha << 2);
	const int* shiftdst = shiftTable + (style.DestAlpha << 2);

	int32_t src[4], dst[4];
	for (int i = 0; i < 4; i++)
	{
		// Grab component for scale factors
		src[i] = (srcinput >> shiftsrc[i]) & 0xff;
		dst[i] = (dstinput >> shiftdst[i]) & 0xff;

		// Inverse if needed
		src[i] = invsrc ? 0xff - src[i] : src[i];
		dst[i] = invdst ? 0xff - dst[i] : dst[i];

		// Rescale 0-255 to 0-256
		src[i] = src[i] + (src[i] >> 7);
		dst[i] = dst[i] + (dst[i] >> 7);

		// Multiply with input
		src[i] = src[i] * ((fg >> (24 - (i << 3))) & 0xff);
		dst[i] = dst[i] * ((bg >> (24 - (i << 3))) & 0xff);
	}

	uint32_t out[4];
	switch (style.BlendOp)
	{
	default:
	case STYLEOP_Add: for (int i = 0; i < 4; i++) out[i] = clamp((src[i] + dst[i] + 127) >> 8, 0, 255); break;
	case STYLEOP_Sub: for (int i = 0; i < 4; i++) out[i] = clamp((src[i] - dst[i] + 127) >> 8, 0, 255); break;
	case STYLEOP_RevSub: for (int i = 0; i < 4; i++) out[i] = clamp((dst[i] - src[i] + 127) >> 8, 0, 255); break;
	}
	return MAKEARGB(out[0], out[1], out[2], out[3]);
}

static void WriteColor(int y, int x0, int x1, PolyTriangleThreadData* thread)
{
	uint32_t* dest = (uint32_t*)thread->dest;
	uint32_t* line = dest + y * (ptrdiff_t)thread->dest_pitch;
	FRenderStyle style = thread->RenderStyle;
	uint32_t* fragcolor = thread->scanline.FragColor;

	if (style.BlendOp == STYLEOP_Add && style.SrcAlpha == STYLEALPHA_One && style.DestAlpha == STYLEALPHA_Zero)
	{
		if (!thread->AlphaTest)
		{
			for (int x = x0; x < x1; x++)
			{
				line[x] = fragcolor[x];
			}
		}
		else
		{
			for (int x = x0; x < x1; x++)
			{
				if (fragcolor[x] > 0x7f000000)
					line[x] = fragcolor[x];
			}
		}
	}
	else
	{
		if (!thread->AlphaTest)
		{
			for (int x = x0; x < x1; x++)
			{
				line[x] = BlendColor(style, fragcolor[x], line[x]);
			}
		}
		else
		{
			for (int x = x0; x < x1; x++)
			{
				if (fragcolor[x] > 0x7f000000)
					line[x] = BlendColor(style, fragcolor[x], line[x]);
			}
		}
	}
}

static void WriteDepth(int y, int x0, int x1, PolyTriangleThreadData* thread)
{
	size_t pitch = thread->depthstencil->Width();
	float* line = thread->depthstencil->DepthValues() + pitch * y;
	float* w = thread->scanline.W;

	if (!thread->AlphaTest)
	{
		for (int x = x0; x < x1; x++)
		{
			line[x] = w[x];
		}
	}
	else
	{
		uint32_t* fragcolor = thread->scanline.FragColor;
		for (int x = x0; x < x1; x++)
		{
			if (fragcolor[x] > 0x7f000000)
				line[x] = w[x];
		}
	}
}

static void WriteStencil(int y, int x0, int x1, PolyTriangleThreadData* thread)
{
	size_t pitch = thread->depthstencil->Width();
	uint8_t* line = thread->depthstencil->StencilValues() + pitch * y;
	uint8_t value = thread->drawargs.StencilWriteValue();
	if (!thread->AlphaTest)
	{
		for (int x = x0; x < x1; x++)
		{
			line[x] = value;
		}
	}
	else
	{
		uint32_t* fragcolor = thread->scanline.FragColor;
		for (int x = x0; x < x1; x++)
		{
			if (fragcolor[x] > 0x7f000000)
				line[x] = value;
		}
	}
}

#ifdef NO_SSE
static float wrap(float value)
{
	return value - std::floor(value);
}
#else
static float wrap(float value)
{
	__m128 mvalue = _mm_set_ss(value);
	return _mm_cvtss_f32(_mm_sub_ss(mvalue, _mm_floor_ss(_mm_setzero_ps(), mvalue)));
}
#endif

static uint32_t sampleTexture(float u, float v, const uint32_t* texPixels, int texWidth, int texHeight)
{
	int texelX = MIN(static_cast<int>(wrap(u) * texWidth), texWidth - 1);
	int texelY = MIN(static_cast<int>(wrap(v) * texHeight), texHeight - 1);
	return texPixels[texelX * texHeight + texelY];
}

static void RunShader(int x0, int x1, PolyTriangleThreadData* thread)
{
	auto constants = thread->PushConstants;
	auto& streamdata = thread->mainVertexShader.Data;
	uint32_t* fragcolor = thread->scanline.FragColor;
	float* u = thread->scanline.U;
	float* v = thread->scanline.V;
	
	if (thread->SpecialEffect == EFF_FOGBOUNDARY) // fogboundary.fp
	{
		/*float fogdist = pixelpos.w;
		float fogfactor = exp2(uFogDensity * fogdist);
		FragColor = vec4(uFogColor.rgb, 1.0 - fogfactor);*/
		return;
	}
	else if (thread->SpecialEffect == EFF_BURN) // burn.fp
	{
		int texWidth = thread->drawargs.TextureWidth();
		int texHeight = thread->drawargs.TextureHeight();
		const uint32_t* texPixels = (const uint32_t*)thread->drawargs.TexturePixels();

		int tex2Width = thread->drawargs.Texture2Width();
		int tex2Height = thread->drawargs.Texture2Height();
		const uint32_t* tex2Pixels = (const uint32_t*)thread->drawargs.Texture2Pixels();

		uint32_t frag = thread->mainVertexShader.vColor;
		uint32_t frag_r = RPART(frag);
		uint32_t frag_g = GPART(frag);
		uint32_t frag_b = BPART(frag);
		uint32_t frag_a = APART(frag);
		frag_r += frag_r >> 7; // 255 -> 256
		frag_g += frag_g >> 7; // 255 -> 256
		frag_b += frag_b >> 7; // 255 -> 256
		frag_a += frag_a >> 7; // 255 -> 256
		for (int x = x0; x < x1; x++)
		{
			uint32_t t1 = sampleTexture(u[x], v[x], texPixels, texWidth, texHeight);
			uint32_t t2 = sampleTexture(u[x], 1.0f - v[x], tex2Pixels, tex2Width, tex2Height);

			uint32_t r = (frag_r * RPART(t1)) >> 8;
			uint32_t g = (frag_g * GPART(t1)) >> 8;
			uint32_t b = (frag_b * BPART(t1)) >> 8;
			uint32_t a = (frag_a * APART(t2)) >> 8;

			fragcolor[x] = MAKEARGB(a, r, g, b);
		}
		return;
	}
	else if (thread->SpecialEffect == EFF_STENCIL) // stencil.fp
	{
		/*for (int x = x0; x < x1; x++)
		{
			fragcolor[x] = 0x00ffffff;
		}*/
		return;
	}
	else if (thread->EffectState == SHADER_NoTexture) // func_notexture
	{
		uint32_t a = (int)(streamdata.uObjectColor.a * 255.0f);
		uint32_t r = (int)(streamdata.uObjectColor.r * 255.0f);
		uint32_t g = (int)(streamdata.uObjectColor.g * 255.0f);
		uint32_t b = (int)(streamdata.uObjectColor.b * 255.0f);
		uint32_t texel = MAKEARGB(a, r, g, b);
		
		if (streamdata.uDesaturationFactor > 0.0f)
		{
			uint32_t t = (int)(streamdata.uDesaturationFactor * 256.0f);
			uint32_t inv_t = 256 - t;
			uint32_t gray = (RPART(texel) * 77 + GPART(texel) * 143 + BPART(texel) * 37) >> 8;
			texel = MAKEARGB(
				APART(texel),
				(RPART(texel) * inv_t + gray * t + 127) >> 8,
				(GPART(texel) * inv_t + gray * t + 127) >> 8, 
				(BPART(texel) * inv_t + gray * t + 127) >> 8);
		}
		
		for (int x = x0; x < x1; x++)
		{
			fragcolor[x] = texel;
		}
	}
	else // func_normal
	{
		int texWidth = thread->drawargs.TextureWidth();
		int texHeight = thread->drawargs.TextureHeight();
		const uint32_t* texPixels = (const uint32_t*)thread->drawargs.TexturePixels();
		
		switch (constants->uTextureMode)
		{
		default:
		case TM_NORMAL:
		case TM_FOGLAYER:
			for (int x = x0; x < x1; x++)
			{
				uint32_t texel = sampleTexture(u[x], v[x], texPixels, texWidth, texHeight);
				fragcolor[x] = texel;
			}
			break;
		case TM_STENCIL:	// TM_STENCIL
			for (int x = x0; x < x1; x++)
			{
				uint32_t texel = sampleTexture(u[x], v[x], texPixels, texWidth, texHeight);
				fragcolor[x] = texel | 0x00ffffff;
			}
			break;
		case TM_OPAQUE:
			for (int x = x0; x < x1; x++)
			{
				uint32_t texel = sampleTexture(u[x], v[x], texPixels, texWidth, texHeight);
				fragcolor[x] = texel | 0xff000000;
			}
			break;
		case TM_INVERSE:
			for (int x = x0; x < x1; x++)
			{
				uint32_t texel = sampleTexture(u[x], v[x], texPixels, texWidth, texHeight);
				fragcolor[x] = MAKEARGB(APART(texel), 0xff - RPART(texel), 0xff - BPART(texel), 0xff - GPART(texel));
			}
			break;
		case TM_ALPHATEXTURE:
			for (int x = x0; x < x1; x++)
			{
				uint32_t texel = sampleTexture(u[x], v[x], texPixels, texWidth, texHeight);
				uint32_t gray = (RPART(texel) * 77 + GPART(texel) * 143 + BPART(texel) * 37) >> 8;
				uint32_t alpha = APART(texel);
				alpha += alpha >> 7;
				alpha = (alpha * gray + 127) >> 8;
				texel = (alpha << 24) | 0x00ffffff;
				fragcolor[x] = texel;
			}
			break;
		case TM_CLAMPY:
			for (int x = x0; x < x1; x++)
			{
				if (v[x] >= 0.0 && v[x] <= 1.0)
					fragcolor[x] = sampleTexture(u[x], v[x], texPixels, texWidth, texHeight);
				else
					fragcolor[x] = 0;
			}
			break;
		case TM_INVERTOPAQUE:
			for (int x = x0; x < x1; x++)
			{
				uint32_t texel = sampleTexture(u[x], v[x], texPixels, texWidth, texHeight);
				fragcolor[x] = MAKEARGB(0xff, 0xff - RPART(texel), 0xff - BPART(texel), 0xff - GPART(texel));
			}
		}

		if (constants->uTextureMode != TM_FOGLAYER)
		{
			if (streamdata.uAddColor.r != 0.0f || streamdata.uAddColor.g != 0.0f || streamdata.uAddColor.b != 0.0f)
			{
				uint32_t r = (int)(streamdata.uAddColor.r * 255.0f);
				uint32_t g = (int)(streamdata.uAddColor.g * 255.0f);
				uint32_t b = (int)(streamdata.uAddColor.b * 255.0f);
				for (int x = x0; x < x1; x++)
				{
					uint32_t texel = fragcolor[x];
					fragcolor[x] = MAKEARGB(
						APART(texel),
						MIN(r + RPART(texel), (uint32_t)255),
						MIN(g + GPART(texel), (uint32_t)255),
						MIN(b + BPART(texel), (uint32_t)255));
				}
			}

			if (streamdata.uObjectColor2.a == 0.0f)
			{
				if (streamdata.uObjectColor.r != 0.0f || streamdata.uObjectColor.g != 0.0f || streamdata.uObjectColor.b != 0.0f)
				{
					uint32_t r = (int)(streamdata.uObjectColor.r * 256.0f);
					uint32_t g = (int)(streamdata.uObjectColor.g * 256.0f);
					uint32_t b = (int)(streamdata.uObjectColor.b * 256.0f);
					for (int x = x0; x < x1; x++)
					{
						uint32_t texel = fragcolor[x];
						fragcolor[x] = MAKEARGB(
							APART(texel),
							MIN((r * RPART(texel)) >> 8, (uint32_t)255),
							MIN((g * GPART(texel)) >> 8, (uint32_t)255),
							MIN((b * BPART(texel)) >> 8, (uint32_t)255));
					}
				}
			}
			else
			{
				float t = thread->mainVertexShader.gradientdist.Z;
				float inv_t = 1.0f - t;
				uint32_t r = (int)((streamdata.uObjectColor.r * inv_t + streamdata.uObjectColor2.r * t) * 256.0f);
				uint32_t g = (int)((streamdata.uObjectColor.g * inv_t + streamdata.uObjectColor2.r * t) * 256.0f);
				uint32_t b = (int)((streamdata.uObjectColor.b * inv_t + streamdata.uObjectColor2.r * t) * 256.0f);
				for (int x = x0; x < x1; x++)
				{
					uint32_t texel = fragcolor[x];
					fragcolor[x] = MAKEARGB(
						APART(texel),
						MIN((r * RPART(texel)) >> 8, (uint32_t)255),
						MIN((g * GPART(texel)) >> 8, (uint32_t)255),
						MIN((b * BPART(texel)) >> 8, (uint32_t)255));
				}
			}

			if (streamdata.uDesaturationFactor > 0.0f)
			{
				uint32_t t = (int)(streamdata.uDesaturationFactor * 256.0f);
				uint32_t inv_t = 256 - t;
				for (int x = x0; x < x1; x++)
				{
					uint32_t texel = fragcolor[x];
					uint32_t gray = (RPART(texel) * 77 + GPART(texel) * 143 + BPART(texel) * 37) >> 8;
					fragcolor[x] = MAKEARGB(
						APART(texel),
						(RPART(texel) * inv_t + gray * t + 127) >> 8,
						(GPART(texel) * inv_t + gray * t + 127) >> 8, 
						(BPART(texel) * inv_t + gray * t + 127) >> 8);
				}
			}
		}
	}

	if (thread->mainVertexShader.vColor != 0xffffffff)
	{
		uint32_t a = APART(thread->mainVertexShader.vColor);
		uint32_t r = RPART(thread->mainVertexShader.vColor);
		uint32_t g = GPART(thread->mainVertexShader.vColor);
		uint32_t b = BPART(thread->mainVertexShader.vColor);
		a += a >> 7;
		r += r >> 7;
		g += g >> 7;
		b += b >> 7;
		for (int x = x0; x < x1; x++)
		{
			uint32_t texel = fragcolor[x];
			fragcolor[x] = MAKEARGB(
				(APART(texel) * a + 127) >> 8,
				(RPART(texel) * r + 127) >> 8,
				(GPART(texel) * g + 127) >> 8,
				(BPART(texel) * b + 127) >> 8);
		}
	}

	if (constants->uLightLevel >= 0.0f)
	{
		uint16_t* lightarray = thread->scanline.lightarray;
		for (int x = x0; x < x1; x++)
		{
			uint32_t fg = fragcolor[x];
			int lightshade = lightarray[x];
			fragcolor[x] = MAKEARGB(
				APART(fg),
				(RPART(fg) * lightshade) >> 8,
				(GPART(fg) * lightshade) >> 8,
				(BPART(fg) * lightshade) >> 8);
		}

		// To do: apply fog
	}
}

static void DrawSpan(int y, int x0, int x1, const TriDrawTriangleArgs* args, PolyTriangleThreadData* thread)
{
	WriteVaryings(y, x0, x1, args, thread);

	if (thread->PushConstants->uLightLevel >= 0.0f)
		WriteLightArray(y, x0, x1, args, thread);

	RunShader(x0, x1, thread);

	if (thread->drawargs.WriteColor())
		WriteColor(y, x0, x1, thread);
	if (thread->drawargs.WriteDepth())
		WriteDepth(y, x0, x1, thread);
	if (thread->drawargs.WriteStencil())
		WriteStencil(y, x0, x1, thread);
}

template<typename OptT>
static void TestSpan(int y, int x0, int x1, const TriDrawTriangleArgs* args, PolyTriangleThreadData* thread)
{
	using namespace TriScreenDrawerModes;

	WriteW(y, x0, x1, args, thread);

	if ((OptT::Flags & SWTRI_DepthTest) || (OptT::Flags & SWTRI_StencilTest))
	{
		size_t pitch = thread->depthstencil->Width();

		uint8_t* stencilbuffer;
		uint8_t* stencilLine;
		uint8_t stencilTestValue;
		if (OptT::Flags & SWTRI_StencilTest)
		{
			stencilbuffer = thread->depthstencil->StencilValues();
			stencilLine = stencilbuffer + pitch * y;
			stencilTestValue = thread->drawargs.StencilTestValue();
		}

		float* zbuffer;
		float* zbufferLine;
		float* w;
		float depthbias;
		if (OptT::Flags & SWTRI_DepthTest)
		{
			zbuffer = thread->depthstencil->DepthValues();
			zbufferLine = zbuffer + pitch * y;
			w = thread->scanline.W;
			depthbias = thread->depthbias;
		}

		int x = x0;
		int xend = x1;
		while (x < xend)
		{
			int xstart = x;

			if ((OptT::Flags & SWTRI_DepthTest) && (OptT::Flags & SWTRI_StencilTest))
			{
				while (zbufferLine[x] >= w[x] + depthbias && stencilLine[x] == stencilTestValue && x < xend)
					x++;
			}
			else if (OptT::Flags & SWTRI_DepthTest)
			{
				while (zbufferLine[x] >= w[x] + depthbias && x < xend)
					x++;
			}
			else if (OptT::Flags & SWTRI_StencilTest)
			{
				while (stencilLine[x] == stencilTestValue && x < xend)
					x++;
			}
			else
			{
				x = xend;
			}

			if (x > xstart)
			{
				DrawSpan(y, xstart, x, args, thread);
			}

			if ((OptT::Flags & SWTRI_DepthTest) && (OptT::Flags & SWTRI_StencilTest))
			{
				while ((zbufferLine[x] < w[x] + depthbias || stencilLine[x] != stencilTestValue) && x < xend)
					x++;
			}
			else if (OptT::Flags & SWTRI_DepthTest)
			{
				while (zbufferLine[x] < w[x] + depthbias && x < xend)
					x++;
			}
			else if (OptT::Flags & SWTRI_StencilTest)
			{
				while (stencilLine[x] != stencilTestValue && x < xend)
					x++;
			}
		}
	}
	else
	{
		DrawSpan(y, x0, x1, args, thread);
	}
}

static void SortVertices(const TriDrawTriangleArgs* args, ScreenTriVertex** sortedVertices)
{
	sortedVertices[0] = args->v1;
	sortedVertices[1] = args->v2;
	sortedVertices[2] = args->v3;

	if (sortedVertices[1]->y < sortedVertices[0]->y)
		std::swap(sortedVertices[0], sortedVertices[1]);
	if (sortedVertices[2]->y < sortedVertices[0]->y)
		std::swap(sortedVertices[0], sortedVertices[2]);
	if (sortedVertices[2]->y < sortedVertices[1]->y)
		std::swap(sortedVertices[1], sortedVertices[2]);
}

void ScreenTriangle::Draw(const TriDrawTriangleArgs* args, PolyTriangleThreadData* thread)
{
	// Sort vertices by Y position
	ScreenTriVertex* sortedVertices[3];
	SortVertices(args, sortedVertices);

	int clipleft = thread->clip.left;
	int cliptop = MAX(thread->clip.top, thread->numa_start_y);
	int clipright = thread->clip.right;
	int clipbottom = MIN(thread->clip.bottom, thread->numa_end_y);

	int topY = (int)(sortedVertices[0]->y + 0.5f);
	int midY = (int)(sortedVertices[1]->y + 0.5f);
	int bottomY = (int)(sortedVertices[2]->y + 0.5f);

	topY = MAX(topY, cliptop);
	midY = MIN(midY, clipbottom);
	bottomY = MIN(bottomY, clipbottom);

	if (topY >= bottomY)
		return;

	void(*testfunc)(int y, int x0, int x1, const TriDrawTriangleArgs * args, PolyTriangleThreadData * thread);

	int opt = 0;
	if (thread->drawargs.DepthTest()) opt |= TriScreenDrawerModes::SWTRI_DepthTest;
	if (thread->drawargs.StencilTest()) opt |= TriScreenDrawerModes::SWTRI_StencilTest;
	testfunc = ScreenTriangle::TestSpanOpts[opt];

	topY += thread->skipped_by_thread(topY);
	int num_cores = thread->num_cores;

	// Find start/end X positions for each line covered by the triangle:

	int y = topY;

	float longDX = sortedVertices[2]->x - sortedVertices[0]->x;
	float longDY = sortedVertices[2]->y - sortedVertices[0]->y;
	float longStep = longDX / longDY;
	float longPos = sortedVertices[0]->x + longStep * (y + 0.5f - sortedVertices[0]->y) + 0.5f;
	longStep *= num_cores;

	if (y < midY)
	{
		float shortDX = sortedVertices[1]->x - sortedVertices[0]->x;
		float shortDY = sortedVertices[1]->y - sortedVertices[0]->y;
		float shortStep = shortDX / shortDY;
		float shortPos = sortedVertices[0]->x + shortStep * (y + 0.5f - sortedVertices[0]->y) + 0.5f;
		shortStep *= num_cores;

		while (y < midY)
		{
			int x0 = (int)shortPos;
			int x1 = (int)longPos;
			if (x1 < x0) std::swap(x0, x1);
			x0 = clamp(x0, clipleft, clipright);
			x1 = clamp(x1, clipleft, clipright);

			testfunc(y, x0, x1, args, thread);

			shortPos += shortStep;
			longPos += longStep;
			y += num_cores;
		}
	}

	if (y < bottomY)
	{
		float shortDX = sortedVertices[2]->x - sortedVertices[1]->x;
		float shortDY = sortedVertices[2]->y - sortedVertices[1]->y;
		float shortStep = shortDX / shortDY;
		float shortPos = sortedVertices[1]->x + shortStep * (y + 0.5f - sortedVertices[1]->y) + 0.5f;
		shortStep *= num_cores;

		while (y < bottomY)
		{
			int x0 = (int)shortPos;
			int x1 = (int)longPos;
			if (x1 < x0) std::swap(x0, x1);
			x0 = clamp(x0, clipleft, clipright);
			x1 = clamp(x1, clipleft, clipright);

			testfunc(y, x0, x1, args, thread);

			shortPos += shortStep;
			longPos += longStep;
			y += num_cores;
		}
	}
}

void(*ScreenTriangle::TestSpanOpts[])(int y, int x0, int x1, const TriDrawTriangleArgs* args, PolyTriangleThreadData* thread) =
{
	&TestSpan<TriScreenDrawerModes::TestSpanOpt0>,
	&TestSpan<TriScreenDrawerModes::TestSpanOpt1>,
	&TestSpan<TriScreenDrawerModes::TestSpanOpt2>,
	&TestSpan<TriScreenDrawerModes::TestSpanOpt3>
};

#if 0

static void SortVertices(const TriDrawTriangleArgs *args, ScreenTriVertex **sortedVertices)
{
	sortedVertices[0] = args->v1;
	sortedVertices[1] = args->v2;
	sortedVertices[2] = args->v3;

	if (sortedVertices[1]->y < sortedVertices[0]->y)
		std::swap(sortedVertices[0], sortedVertices[1]);
	if (sortedVertices[2]->y < sortedVertices[0]->y)
		std::swap(sortedVertices[0], sortedVertices[2]);
	if (sortedVertices[2]->y < sortedVertices[1]->y)
		std::swap(sortedVertices[1], sortedVertices[2]);
}

void ScreenTriangle::Draw(const TriDrawTriangleArgs *args, PolyTriangleThreadData *thread)
{
	using namespace TriScreenDrawerModes;

	// Sort vertices by Y position
	ScreenTriVertex *sortedVertices[3];
	SortVertices(args, sortedVertices);

	int clipleft = thread->clip.left;
	int cliptop = MAX(thread->clip.top, thread->numa_start_y);
	int clipright = thread->clip.right;
	int clipbottom = MIN(thread->clip.bottom, thread->numa_end_y);

	int topY = (int)(sortedVertices[0]->y + 0.5f);
	int midY = (int)(sortedVertices[1]->y + 0.5f);
	int bottomY = (int)(sortedVertices[2]->y + 0.5f);

	topY = MAX(topY, cliptop);
	midY = MIN(midY, clipbottom);
	bottomY = MIN(bottomY, clipbottom);

	if (topY >= bottomY)
		return;

	topY += thread->skipped_by_thread(topY);
	int num_cores = thread->num_cores;

	// Find start/end X positions for each line covered by the triangle:

	int16_t edges[MAXHEIGHT * 2];

	int y = topY;

	float longDX = sortedVertices[2]->x - sortedVertices[0]->x;
	float longDY = sortedVertices[2]->y - sortedVertices[0]->y;
	float longStep = longDX / longDY;
	float longPos = sortedVertices[0]->x + longStep * (y + 0.5f - sortedVertices[0]->y) + 0.5f;
	longStep *= num_cores;

	if (y < midY)
	{
		float shortDX = sortedVertices[1]->x - sortedVertices[0]->x;
		float shortDY = sortedVertices[1]->y - sortedVertices[0]->y;
		float shortStep = shortDX / shortDY;
		float shortPos = sortedVertices[0]->x + shortStep * (y + 0.5f - sortedVertices[0]->y) + 0.5f;
		shortStep *= num_cores;

		while (y < midY)
		{
			int x0 = (int)shortPos;
			int x1 = (int)longPos;
			if (x1 < x0) std::swap(x0, x1);
			x0 = clamp(x0, clipleft, clipright);
			x1 = clamp(x1, clipleft, clipright);

			edges[y << 1] = x0;
			edges[(y << 1) + 1] = x1;

			shortPos += shortStep;
			longPos += longStep;
			y += num_cores;
		}
	}

	if (y < bottomY)
	{
		float shortDX = sortedVertices[2]->x - sortedVertices[1]->x;
		float shortDY = sortedVertices[2]->y - sortedVertices[1]->y;
		float shortStep = shortDX / shortDY;
		float shortPos = sortedVertices[1]->x + shortStep * (y + 0.5f - sortedVertices[1]->y) + 0.5f;
		shortStep *= num_cores;

		while (y < bottomY)
		{
			int x0 = (int)shortPos;
			int x1 = (int)longPos;
			if (x1 < x0) std::swap(x0, x1);
			x0 = clamp(x0, clipleft, clipright);
			x1 = clamp(x1, clipleft, clipright);

			edges[y << 1] = x0;
			edges[(y << 1) + 1] = x1;

			shortPos += shortStep;
			longPos += longStep;
			y += num_cores;
		}
	}

	int opt = 0;
	if (thread->drawargs.DepthTest()) opt |= SWTRI_DepthTest;
	if (thread->drawargs.StencilTest()) opt |= SWTRI_StencilTest;
	if (thread->drawargs.WriteColor()) opt |= SWTRI_WriteColor;
	if (thread->drawargs.WriteDepth()) opt |= SWTRI_WriteDepth;
	if (thread->drawargs.WriteStencil()) opt |= SWTRI_WriteStencil;
	
	TriangleDrawers[opt](args, thread, edges, topY, bottomY);
}

static void FillDepthValues(float *depthvalues, float posXW, float stepXW, int count, float depthbias)
{
#ifndef NO_SSE
	__m128 mstepXW = _mm_set1_ps(stepXW * 4.0f);
	__m128 mfirstStepXW = _mm_setr_ps(0.0f, stepXW, stepXW + stepXW, stepXW + stepXW + stepXW);
	__m128 mposXW = _mm_add_ps(_mm_set1_ps(posXW), mfirstStepXW);
	__m128 mdepthbias = _mm_set1_ps(depthbias);
	while (count > 0)
	{
		_mm_storeu_ps(depthvalues, _mm_add_ps(_mm_rcp_ps(mposXW), mdepthbias));
		mposXW = _mm_add_ps(mposXW, mstepXW);
		depthvalues += 4;
		count -= 4;
	}
#else
	while (count > 0)
	{
		*(depthvalues++) = 1.0f / posXW + depthbias;
		posXW += stepXW;
		count--;
	}
#endif
}

template<typename OptT>
void DrawTriangle(const TriDrawTriangleArgs *args, PolyTriangleThreadData *thread, int16_t *edges, int topY, int bottomY)
{
	using namespace TriScreenDrawerModes;

	void(*drawfunc)(int y, int x0, int x1, const TriDrawTriangleArgs *args, PolyTriangleThreadData *thread);
	float stepXW, v1X, v1Y, v1W;
	uint8_t stencilTestValue, stencilWriteValue;
	float *zbuffer;
	float *zbufferLine;
	uint8_t *stencilbuffer;
	uint8_t *stencilLine;
	int pitch;

	bool alphatest = thread->AlphaTest;
	uint8_t *alphatestbuf = thread->alphatestbuffer;

	if (OptT::Flags & SWTRI_WriteColor)
	{
		int bmode = (int)thread->drawargs.BlendMode();
		drawfunc = thread->dest_bgra ? ScreenTriangle::SpanDrawers32[bmode] : ScreenTriangle::SpanDrawers8[bmode];
	}

	if ((OptT::Flags & SWTRI_DepthTest) || (OptT::Flags & SWTRI_WriteDepth))
	{
		stepXW = args->gradientX.W;
		v1X = args->v1->x;
		v1Y = args->v1->y;
		v1W = args->v1->w;
		zbuffer = thread->depthstencil->DepthValues();
	}

	if ((OptT::Flags & SWTRI_StencilTest) || (OptT::Flags & SWTRI_WriteStencil))
	{
		stencilbuffer = thread->depthstencil->StencilValues();
	}

	if ((OptT::Flags & SWTRI_StencilTest) || (OptT::Flags & SWTRI_WriteStencil) || (OptT::Flags & SWTRI_DepthTest) || (OptT::Flags & SWTRI_WriteDepth))
		pitch = thread->depthstencil->Width();

	if (OptT::Flags & SWTRI_StencilTest)
		stencilTestValue = thread->drawargs.StencilTestValue();

	if (OptT::Flags & SWTRI_WriteStencil)
		stencilWriteValue = thread->drawargs.StencilWriteValue();

	float depthbias;
	if ((OptT::Flags & SWTRI_DepthTest) || (OptT::Flags & SWTRI_WriteDepth))
	{
		depthbias = thread->depthbias;
	}

	float *depthvalues = thread->depthvalues;

	int num_cores = thread->num_cores;
	for (int y = topY; y < bottomY; y += num_cores)
	{
		int x = edges[y << 1];
		int xend = edges[(y << 1) + 1];

		if ((OptT::Flags & SWTRI_StencilTest) || (OptT::Flags & SWTRI_WriteStencil))
			stencilLine = stencilbuffer + pitch * y;

		if ((OptT::Flags & SWTRI_DepthTest) || (OptT::Flags & SWTRI_WriteDepth))
		{
			zbufferLine = zbuffer + pitch * y;

			float startX = x + (0.5f - v1X);
			float startY = y + (0.5f - v1Y);
			float posXW = v1W + stepXW * startX + args->gradientY.W * startY;
			FillDepthValues(depthvalues + x, posXW, stepXW, xend - x, depthbias);
		}

#ifndef NO_SSE
		while (x < xend)
		{
			int xstart = x;

			if ((OptT::Flags & SWTRI_DepthTest) && (OptT::Flags & SWTRI_StencilTest))
			{
				int xendsse = x + ((xend - x) / 4);
				__m128 mposXW;
				while (x < xendsse &&
					_mm_movemask_ps(_mm_cmpge_ps(_mm_loadu_ps(zbufferLine + x), mposXW = _mm_loadu_ps(depthvalues + x))) == 15 &&
					stencilLine[x] == stencilTestValue &&
					stencilLine[x + 1] == stencilTestValue &&
					stencilLine[x + 2] == stencilTestValue &&
					stencilLine[x + 3] == stencilTestValue)
				{
					if (!alphatest && (OptT::Flags & SWTRI_WriteDepth))
						_mm_storeu_ps(zbufferLine + x, mposXW);
					x += 4;
				}

				while (zbufferLine[x] >= depthvalues[x] && stencilLine[x] == stencilTestValue && x < xend)
				{
					if (!alphatest && (OptT::Flags & SWTRI_WriteDepth))
						zbufferLine[x] = depthvalues[x];
					x++;
				}
			}
			else if (OptT::Flags & SWTRI_DepthTest)
			{
				int xendsse = x + ((xend - x) / 4);
				__m128 mposXW;
				while (x < xendsse && _mm_movemask_ps(_mm_cmpge_ps(_mm_loadu_ps(zbufferLine + x), mposXW = _mm_loadu_ps(depthvalues + x))) == 15)
				{
					if (!alphatest && (OptT::Flags & SWTRI_WriteDepth))
						_mm_storeu_ps(zbufferLine + x, mposXW);
					x += 4;
				}

				while (zbufferLine[x] >= depthvalues[x] && x < xend)
				{
					if (!alphatest && (OptT::Flags & SWTRI_WriteDepth))
						zbufferLine[x] = depthvalues[x];
					x++;
				}
			}
			else if (OptT::Flags & SWTRI_StencilTest)
			{
				int xendsse = x + ((xend - x) / 16);
				while (x < xendsse && _mm_movemask_epi8(_mm_cmpeq_epi8(_mm_loadu_si128((const __m128i*)&stencilLine[x]), _mm_set1_epi8(stencilTestValue))) == 0xffff)
				{
					x += 16;
				}

				while (stencilLine[x] == stencilTestValue && x < xend)
					x++;
			}
			else
			{
				x = xend;
			}

			if (x > xstart)
			{
				if (OptT::Flags & SWTRI_WriteColor)
					drawfunc(y, xstart, x, args, thread);

				if (alphatest)
				{
					if (OptT::Flags & SWTRI_WriteStencil)
					{
						for (int i = xstart; i < x; i++)
						{
							if (alphatestbuf[i] > 127)
								stencilLine[i++] = stencilWriteValue;
						}
					}

					if (OptT::Flags & SWTRI_WriteDepth)
					{
						for (int i = xstart; i < x; i++)
						{
							if (alphatestbuf[i] > 127)
								zbufferLine[i] = depthvalues[i];
						}
					}
				}
				else
				{
					if (OptT::Flags & SWTRI_WriteStencil)
					{
						int i = xstart;
						int xendsse = xstart + ((x - xstart) / 16);
						while (i < xendsse)
						{
							_mm_storeu_si128((__m128i*)&stencilLine[i], _mm_set1_epi8(stencilWriteValue));
							i += 16;
						}

						while (i < x)
							stencilLine[i++] = stencilWriteValue;
					}

					if (!(OptT::Flags & SWTRI_DepthTest) && (OptT::Flags & SWTRI_WriteDepth))
					{
						for (int i = xstart; i < x; i++)
						{
							zbufferLine[i] = depthvalues[i];
						}
					}
				}
			}

			if ((OptT::Flags & SWTRI_DepthTest) && (OptT::Flags & SWTRI_StencilTest))
			{
				int xendsse = x + ((xend - x) / 4);
				__m128 mposXW;
				while (x < xendsse &&
					(_mm_movemask_ps(_mm_cmpge_ps(_mm_loadu_ps(zbufferLine + x), mposXW = _mm_loadu_ps(depthvalues + x))) == 0 ||
					stencilLine[x] != stencilTestValue ||
					stencilLine[x + 1] != stencilTestValue ||
					stencilLine[x + 2] != stencilTestValue ||
					stencilLine[x + 3] != stencilTestValue))
				{
					x += 4;
				}

				while ((zbufferLine[x] < depthvalues[x] || stencilLine[x] != stencilTestValue) && x < xend)
				{
					x++;
				}
			}
			else if (OptT::Flags & SWTRI_DepthTest)
			{
				int xendsse = x + ((xend - x) / 4);
				__m128 mposXW;
				while (x < xendsse && _mm_movemask_ps(_mm_cmpge_ps(_mm_loadu_ps(zbufferLine + x), mposXW = _mm_loadu_ps(depthvalues + x))) == 0)
				{
					x += 4;
				}

				while (zbufferLine[x] < depthvalues[x] && x < xend)
				{
					x++;
				}
			}
			else if (OptT::Flags & SWTRI_StencilTest)
			{
				int xendsse = x + ((xend - x) / 16);
				while (x < xendsse && _mm_movemask_epi8(_mm_cmpeq_epi8(_mm_loadu_si128((const __m128i*)&stencilLine[x]), _mm_set1_epi8(stencilTestValue))) == 0)
				{
					x += 16;
				}

				while (stencilLine[x] != stencilTestValue && x < xend)
				{
					x++;
				}
			}
		}
#else
		while (x < xend)
		{
			int xstart = x;

			if ((OptT::Flags & SWTRI_DepthTest) && (OptT::Flags & SWTRI_StencilTest))
			{
				while (zbufferLine[x] >= depthvalues[x] && stencilLine[x] == stencilTestValue && x < xend)
				{
					if (!alphatest && (OptT::Flags & SWTRI_WriteDepth))
						zbufferLine[x] = depthvalues[x];
					x++;
				}
			}
			else if (OptT::Flags & SWTRI_DepthTest)
			{
				while (zbufferLine[x] >= depthvalues[x] && x < xend)
				{
					if (!alphatest && (OptT::Flags & SWTRI_WriteDepth))
						zbufferLine[x] = depthvalues[x];
					x++;
				}
			}
			else if (OptT::Flags & SWTRI_StencilTest)
			{
				while (stencilLine[x] == stencilTestValue && x < xend)
					x++;
			}
			else
			{
				x = xend;
			}

			if (x > xstart)
			{
				if (OptT::Flags & SWTRI_WriteColor)
					drawfunc(y, xstart, x, args, thread);

				if (alphatest)
				{
					if (OptT::Flags & SWTRI_WriteStencil)
					{
						for (int i = xstart; i < x; i++)
						{
							if (alphatestbuf[i] > 127)
								stencilLine[i++] = stencilWriteValue;
						}
					}

					if (OptT::Flags & SWTRI_WriteDepth)
					{
						for (int i = xstart; i < x; i++)
						{
							if (alphatestbuf[i] > 127)
								zbufferLine[i] = depthvalues[i];
						}
					}
				}
				else
				{
					if (OptT::Flags & SWTRI_WriteStencil)
					{
						for (int i = xstart; i < x; i++)
							stencilLine[i] = stencilWriteValue;
					}

					if (!(OptT::Flags & SWTRI_DepthTest) && (OptT::Flags & SWTRI_WriteDepth))
					{
						for (int i = xstart; i < x; i++)
						{
							zbufferLine[i] = depthvalues[i];
						}
					}
				}
			}

			if ((OptT::Flags & SWTRI_DepthTest) && (OptT::Flags & SWTRI_StencilTest))
			{
				while ((zbufferLine[x] < depthvalues[x] || stencilLine[x] != stencilTestValue) && x < xend)
				{
					x++;
				}
			}
			else if (OptT::Flags & SWTRI_DepthTest)
			{
				while (zbufferLine[x] < depthvalues[x] && x < xend)
				{
					x++;
				}
			}
			else if (OptT::Flags & SWTRI_StencilTest)
			{
				while (stencilLine[x] != stencilTestValue && x < xend)
				{
					x++;
				}
			}
		}
#endif
	}
}

template<typename ModeT, typename OptT, int BitsPerPixel>
void StepSpan(int y, int x0, int x1, const TriDrawTriangleArgs *args, PolyTriangleThreadData *thread)
{
	using namespace TriScreenDrawerModes;

	float v1X, v1Y, v1W, v1U, v1V, v1WorldX, v1WorldY, v1WorldZ;
	float startX, startY;
	float stepW, stepU, stepV, stepWorldX, stepWorldY, stepWorldZ;
	float posW, posU, posV, posWorldX, posWorldY, posWorldZ;
	int texWidth, texHeight;
	uint32_t light;
	fixed_t shade, lightpos, lightstep;

	float *worldposX = thread->worldposX;
	float *worldposY = thread->worldposY;
	float *worldposZ = thread->worldposZ;
	uint32_t *texel = thread->texel;
	int32_t *texelV = thread->texelV;
	uint16_t *lightarray = thread->lightarray;
	uint32_t *dynlights = thread->dynlights;

	v1X = args->v1->x;
	v1Y = args->v1->y;
	v1W = args->v1->w;
	v1U = args->v1->u * v1W;
	v1V = args->v1->v * v1W;
	startX = x0 + (0.5f - v1X);
	startY = y + (0.5f - v1Y);
	stepW = args->gradientX.W;
	stepU = args->gradientX.U;
	stepV = args->gradientX.V;
	posW = v1W + stepW * startX + args->gradientY.W * startY;
	posU = v1U + stepU * startX + args->gradientY.U * startY;
	posV = v1V + stepV * startX + args->gradientY.V * startY;

	if (!(ModeT::SWFlags & SWSTYLEF_Fill) && !(ModeT::SWFlags & SWSTYLEF_FogBoundary))
	{
		texWidth = thread->drawargs.TextureWidth();
		texHeight = thread->drawargs.TextureHeight();
	}

	if (OptT::Flags & SWOPT_DynLights)
	{
		v1WorldX = args->v1->worldX * v1W;
		v1WorldY = args->v1->worldY * v1W;
		v1WorldZ = args->v1->worldZ * v1W;
		stepWorldX = args->gradientX.WorldX;
		stepWorldY = args->gradientX.WorldY;
		stepWorldZ = args->gradientX.WorldZ;
		posWorldX = v1WorldX + stepWorldX * startX + args->gradientY.WorldX * startY;
		posWorldY = v1WorldY + stepWorldY * startX + args->gradientY.WorldY * startY;
		posWorldZ = v1WorldZ + stepWorldZ * startX + args->gradientY.WorldZ * startY;
	}

	if (!(OptT::Flags & SWOPT_FixedLight))
	{
		float globVis = thread->drawargs.GlobVis() * (1.0f / 32.0f);

		light = thread->drawargs.Light();
		shade = (fixed_t)((2.0f - (light + 12.0f) / 128.0f) * (float)FRACUNIT);
		lightpos = (fixed_t)(globVis * posW * (float)FRACUNIT);
		lightstep = (fixed_t)(globVis * stepW * (float)FRACUNIT);

		fixed_t maxvis = 24 * FRACUNIT / 32;
		fixed_t maxlight = 31 * FRACUNIT / 32;

		fixed_t lightend = lightpos + lightstep * (x1 - x0);
		if (lightpos < maxvis && shade >= lightpos && shade - lightpos <= maxlight &&
			lightend < maxvis && shade >= lightend && shade - lightend <= maxlight)
		{
			if (BitsPerPixel == 32)
			{
				lightpos += FRACUNIT - shade;
				for (int x = x0; x < x1; x++)
				{
					lightarray[x] = lightpos >> 8;
					lightpos += lightstep;
				}
			}
			else
			{
				lightpos = shade - lightpos;
				for (int x = x0; x < x1; x++)
				{
					lightarray[x] = (lightpos >> 3) & 0xffffff00;
					lightpos -= lightstep;
				}
			}
		}
		else
		{
			if (BitsPerPixel == 32)
			{
				for (int x = x0; x < x1; x++)
				{
					lightarray[x] = (FRACUNIT - clamp<fixed_t>(shade - MIN(maxvis, lightpos), 0, maxlight)) >> 8;
					lightpos += lightstep;
				}
			}
			else
			{
				for (int x = x0; x < x1; x++)
				{
					lightarray[x] = (clamp<fixed_t>(shade - MIN(maxvis, lightpos), 0, maxlight) >> 3) & 0xffffff00;
					lightpos += lightstep;
				}
			}
		}
	}

#ifndef NO_SSE
	__m128 mposW, mposU, mposV, mstepW, mstepU, mstepV;
	__m128 mposWorldX, mposWorldY, mposWorldZ, mstepWorldX, mstepWorldY, mstepWorldZ;
	__m128i mtexMul1, mtexMul2;

	#define SETUP_STEP_SSE(mpos,mstep,pos,step) \
		mstep = _mm_load_ss(&step); \
		mpos = _mm_load_ss(&pos); \
		mpos = _mm_shuffle_ps(mpos, mpos, _MM_SHUFFLE(2, 1, 0, 0)); \
		mpos = _mm_add_ss(mpos, mstep); \
		mpos = _mm_shuffle_ps(mpos, mpos, _MM_SHUFFLE(2, 1, 0, 0)); \
		mpos = _mm_add_ss(mpos, mstep); \
		mpos = _mm_shuffle_ps(mpos, mpos, _MM_SHUFFLE(2, 1, 0, 0)); \
		mpos = _mm_add_ss(mpos, mstep); \
		mpos = _mm_shuffle_ps(mpos, mpos, _MM_SHUFFLE(0, 1, 2, 3)); \
		mstep = _mm_mul_ss(mstep, _mm_set1_ps(4.0f)); \
		mstep = _mm_shuffle_ps(mstep, mstep, _MM_SHUFFLE(0, 0, 0, 0));

	SETUP_STEP_SSE(mposW, mstepW, posW, stepW);

	if (OptT::Flags & SWOPT_DynLights)
	{
		SETUP_STEP_SSE(mposWorldX, mstepWorldX, posWorldX, stepWorldX);
		SETUP_STEP_SSE(mposWorldY, mstepWorldY, posWorldY, stepWorldY);
		SETUP_STEP_SSE(mposWorldZ, mstepWorldZ, posWorldZ, stepWorldZ);
	}

	if (!(ModeT::SWFlags & SWSTYLEF_Fill) && !(ModeT::SWFlags & SWSTYLEF_FogBoundary))
	{
		SETUP_STEP_SSE(mposU, mstepU, posU, stepU);
		SETUP_STEP_SSE(mposV, mstepV, posV, stepV);

		mtexMul1 = _mm_setr_epi16(texWidth, texWidth, texWidth, texWidth, texHeight, texHeight, texHeight, texHeight);
		mtexMul2 = _mm_setr_epi16(texHeight, texHeight, texHeight, texHeight, 1, 1, 1, 1);
	}

	#undef SETUP_STEP_SSE

	for (int x = x0; x < x1; x += 4)
	{
		__m128 rcp_posW = _mm_div_ps(_mm_set1_ps(1.0f), mposW); // precision of _mm_rcp_ps(mposW) is terrible!

		if (OptT::Flags & SWOPT_DynLights)
		{
			_mm_storeu_ps(&worldposX[x], _mm_mul_ps(mposWorldX, rcp_posW));
			_mm_storeu_ps(&worldposY[x], _mm_mul_ps(mposWorldY, rcp_posW));
			_mm_storeu_ps(&worldposZ[x], _mm_mul_ps(mposWorldZ, rcp_posW));
			mposWorldX = _mm_add_ps(mposWorldX, mstepWorldX);
			mposWorldY = _mm_add_ps(mposWorldY, mstepWorldY);
			mposWorldZ = _mm_add_ps(mposWorldZ, mstepWorldZ);
		}
		if (!(ModeT::SWFlags & SWSTYLEF_Fill) && !(ModeT::SWFlags & SWSTYLEF_FogBoundary))
		{
			__m128 rcpW = _mm_mul_ps(_mm_set1_ps(0x01000000), rcp_posW);
			__m128i u = _mm_cvtps_epi32(_mm_mul_ps(mposU, rcpW));
			__m128i v = _mm_cvtps_epi32(_mm_mul_ps(mposV, rcpW));
			_mm_storeu_si128((__m128i*)&texelV[x], v);

			__m128i texelX = _mm_srli_epi32(_mm_slli_epi32(u, 8), 17);
			__m128i texelY = _mm_srli_epi32(_mm_slli_epi32(v, 8), 17);
			__m128i texelXY = _mm_mulhi_epu16(_mm_slli_epi16(_mm_packs_epi32(texelX, texelY), 1), mtexMul1);
			__m128i texlo = _mm_mullo_epi16(texelXY, mtexMul2);
			__m128i texhi = _mm_mulhi_epi16(texelXY, mtexMul2);
			texelX = _mm_unpacklo_epi16(texlo, texhi);
			texelY = _mm_unpackhi_epi16(texlo, texhi);
			_mm_storeu_si128((__m128i*)&texel[x], _mm_add_epi32(texelX, texelY));

			mposU = _mm_add_ps(mposU, mstepU);
			mposV = _mm_add_ps(mposV, mstepV);
		}

		mposW = _mm_add_ps(mposW, mstepW);
	}
#else
	for (int x = x0; x < x1; x++)
	{
		if (OptT::Flags & SWOPT_DynLights)
		{
			float rcp_posW = 1.0f / posW;
			worldposX[x] = posWorldX * rcp_posW;
			worldposY[x] = posWorldY * rcp_posW;
			worldposZ[x] = posWorldZ * rcp_posW;
			posWorldX += stepWorldX;
			posWorldY += stepWorldY;
			posWorldZ += stepWorldZ;
		}
		if (!(ModeT::SWFlags & SWSTYLEF_Fill) && !(ModeT::SWFlags & SWSTYLEF_FogBoundary))
		{
			float rcpW = 0x01000000 / posW;
			int32_t u = (int32_t)(posU * rcpW);
			int32_t v = (int32_t)(posV * rcpW);
			uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
			uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
			texel[x] = texelX * texHeight + texelY;
			texelV[x] = v;
			posU += stepU;
			posV += stepV;
		}

		posW += stepW;
	}
#endif

	if (OptT::Flags & SWOPT_DynLights)
	{
		PolyLight *lights;
		int num_lights;
		float worldnormalX, worldnormalY, worldnormalZ;
		uint32_t dynlightcolor;

		lights = thread->drawargs.Lights();
		num_lights = thread->drawargs.NumLights();
		worldnormalX = thread->drawargs.Normal().X;
		worldnormalY = thread->drawargs.Normal().Y;
		worldnormalZ = thread->drawargs.Normal().Z;
		dynlightcolor = thread->drawargs.DynLightColor();

		// The normal vector cannot be uniform when drawing models. Calculate and use the face normal:
		if (worldnormalX == 0.0f && worldnormalY == 0.0f && worldnormalZ == 0.0f)
		{
			float dx1 = args->v2->worldX - args->v1->worldX;
			float dy1 = args->v2->worldY - args->v1->worldY;
			float dz1 = args->v2->worldZ - args->v1->worldZ;
			float dx2 = args->v3->worldX - args->v1->worldX;
			float dy2 = args->v3->worldY - args->v1->worldY;
			float dz2 = args->v3->worldZ - args->v1->worldZ;
			worldnormalX = dy1 * dz2 - dz1 * dy2;
			worldnormalY = dz1 * dx2 - dx1 * dz2;
			worldnormalZ = dx1 * dy2 - dy1 * dx2;
			float lensqr = worldnormalX * worldnormalX + worldnormalY * worldnormalY + worldnormalZ * worldnormalZ;
#ifndef NO_SSE
			float rcplen = _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(lensqr)));
#else
			float rcplen = 1.0f / sqrt(lensqr);
#endif
			worldnormalX *= rcplen;
			worldnormalY *= rcplen;
			worldnormalZ *= rcplen;
		}

#ifndef NO_SSE
		__m128 mworldnormalX = _mm_set1_ps(worldnormalX);
		__m128 mworldnormalY = _mm_set1_ps(worldnormalY);
		__m128 mworldnormalZ = _mm_set1_ps(worldnormalZ);
		for (int x = x0; x < x1; x += 4)
		{
			__m128i litlo = _mm_shuffle_epi32(_mm_unpacklo_epi8(_mm_cvtsi32_si128(dynlightcolor), _mm_setzero_si128()), _MM_SHUFFLE(1, 0, 1, 0));
			__m128i lithi = litlo;

			for (int i = 0; i < num_lights; i++)
			{
				__m128 lightposX = _mm_set1_ps(lights[i].x);
				__m128 lightposY = _mm_set1_ps(lights[i].y);
				__m128 lightposZ = _mm_set1_ps(lights[i].z);
				__m128 light_radius = _mm_set1_ps(lights[i].radius);
				__m128i light_color = _mm_shuffle_epi32(_mm_unpacklo_epi8(_mm_cvtsi32_si128(lights[i].color), _mm_setzero_si128()), _MM_SHUFFLE(1, 0, 1, 0));

				__m128 is_attenuated = _mm_cmplt_ps(light_radius, _mm_setzero_ps());
				light_radius = _mm_andnot_ps(_mm_set1_ps(-0.0f), light_radius); // clear sign bit

				// L = light-pos
				// dist = sqrt(dot(L, L))
				// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
				__m128 Lx = _mm_sub_ps(lightposX, _mm_loadu_ps(&worldposX[x]));
				__m128 Ly = _mm_sub_ps(lightposY, _mm_loadu_ps(&worldposY[x]));
				__m128 Lz = _mm_sub_ps(lightposZ, _mm_loadu_ps(&worldposZ[x]));
				__m128 dist2 = _mm_add_ps(_mm_mul_ps(Lx, Lx), _mm_add_ps(_mm_mul_ps(Ly, Ly), _mm_mul_ps(Lz, Lz)));
				__m128 rcp_dist = _mm_rsqrt_ps(dist2);
				__m128 dist = _mm_mul_ps(dist2, rcp_dist);
				__m128 distance_attenuation = _mm_sub_ps(_mm_set1_ps(256.0f), _mm_min_ps(_mm_mul_ps(dist, light_radius), _mm_set1_ps(256.0f)));

				// The simple light type
				__m128 simple_attenuation = distance_attenuation;

				// The point light type
				// diffuse = max(dot(N,normalize(L)),0) * attenuation
				Lx = _mm_mul_ps(Lx, rcp_dist);
				Ly = _mm_mul_ps(Ly, rcp_dist);
				Lz = _mm_mul_ps(Lz, rcp_dist);
				__m128 dotNL = _mm_add_ps(_mm_add_ps(_mm_mul_ps(mworldnormalX, Lx), _mm_mul_ps(mworldnormalY, Ly)), _mm_mul_ps(mworldnormalZ, Lz));
				__m128 point_attenuation = _mm_mul_ps(_mm_max_ps(dotNL, _mm_setzero_ps()), distance_attenuation);

				__m128i attenuation = _mm_cvtps_epi32(_mm_or_ps(_mm_and_ps(is_attenuated, point_attenuation), _mm_andnot_ps(is_attenuated, simple_attenuation)));

				attenuation = _mm_shufflehi_epi16(_mm_shufflelo_epi16(attenuation, _MM_SHUFFLE(2, 2, 0, 0)), _MM_SHUFFLE(2, 2, 0, 0));
				__m128i attenlo = _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(1, 1, 0, 0));
				__m128i attenhi = _mm_shuffle_epi32(attenuation, _MM_SHUFFLE(3, 3, 2, 2));

				litlo = _mm_add_epi16(litlo, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenlo), 8));
				lithi = _mm_add_epi16(lithi, _mm_srli_epi16(_mm_mullo_epi16(light_color, attenhi), 8));
			}

			_mm_storeu_si128((__m128i*)&dynlights[x], _mm_packus_epi16(litlo, lithi));
		}
#else
		for (int x = x0; x < x1; x++)
		{
			uint32_t lit_r = RPART(dynlightcolor);
			uint32_t lit_g = GPART(dynlightcolor);
			uint32_t lit_b = BPART(dynlightcolor);

			for (int i = 0; i < num_lights; i++)
			{
				float lightposX = lights[i].x;
				float lightposY = lights[i].y;
				float lightposZ = lights[i].z;
				float light_radius = lights[i].radius;
				uint32_t light_color = lights[i].color;

				bool is_attenuated = light_radius < 0.0f;
				if (is_attenuated)
					light_radius = -light_radius;

				// L = light-pos
				// dist = sqrt(dot(L, L))
				// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
				float Lx = lightposX - worldposX[x];
				float Ly = lightposY - worldposY[x];
				float Lz = lightposZ - worldposZ[x];
				float dist2 = Lx * Lx + Ly * Ly + Lz * Lz;
#ifdef NO_SSE
				//float rcp_dist = 1.0f / sqrt(dist2);
				float rcp_dist = 1.0f / (dist2 * 0.01f);
#else
				float rcp_dist = _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(dist2)));
#endif
				float dist = dist2 * rcp_dist;
				float distance_attenuation = 256.0f - MIN(dist * light_radius, 256.0f);

				// The simple light type
				float simple_attenuation = distance_attenuation;

				// The point light type
				// diffuse = max(dot(N,normalize(L)),0) * attenuation
				Lx *= rcp_dist;
				Ly *= rcp_dist;
				Lz *= rcp_dist;
				float dotNL = worldnormalX * Lx + worldnormalY * Ly + worldnormalZ * Lz;
				float point_attenuation = MAX(dotNL, 0.0f) * distance_attenuation;

				uint32_t attenuation = (uint32_t)(is_attenuated ? (int32_t)point_attenuation : (int32_t)simple_attenuation);

				lit_r += (RPART(light_color) * attenuation) >> 8;
				lit_g += (GPART(light_color) * attenuation) >> 8;
				lit_b += (BPART(light_color) * attenuation) >> 8;
			}

			lit_r = MIN<uint32_t>(lit_r, 255);
			lit_g = MIN<uint32_t>(lit_g, 255);
			lit_b = MIN<uint32_t>(lit_b, 255);
			dynlights[x] = MAKEARGB(255, lit_r, lit_g, lit_b);
		}
#endif
	}
}

template<typename ModeT, typename OptT>
void DrawSpanOpt32(int y, int x0, int x1, const TriDrawTriangleArgs *args, PolyTriangleThreadData *thread)
{
	using namespace TriScreenDrawerModes;

	StepSpan<ModeT, OptT, 32>(y, x0, x1, args, thread);

	uint32_t fixedlight;
	uint32_t shade_fade_r, shade_fade_g, shade_fade_b, shade_light_r, shade_light_g, shade_light_b, desaturate, inv_desaturate;
	fixed_t fuzzscale;
	int _fuzzpos;
	const uint32_t *texPixels, *translation;
	uint32_t fillcolor;
	int actoralpha;
	uint8_t *alphatestbuf;

	uint32_t *texel = thread->texel;
	int32_t *texelV = thread->texelV;
	uint16_t *lightarray = thread->lightarray;
	uint32_t *dynlights = thread->dynlights;

	if (!(ModeT::SWFlags & SWSTYLEF_Fill) && !(ModeT::SWFlags & SWSTYLEF_FogBoundary))
	{
		texPixels = (const uint32_t*)thread->drawargs.TexturePixels();
	}

	if (ModeT::SWFlags & SWSTYLEF_Translated)
	{
		translation = (const uint32_t*)thread->drawargs.Translation();
	}

	if ((ModeT::SWFlags & SWSTYLEF_Fill) || (ModeT::SWFlags & SWSTYLEF_Skycap) || (ModeT::Flags & STYLEF_ColorIsFixed))
	{
		fillcolor = thread->drawargs.Color();
	}

	if (!(ModeT::Flags & STYLEF_Alpha1))
	{
		actoralpha = thread->drawargs.Alpha();
	}

	if (OptT::Flags & SWOPT_FixedLight)
	{
		fixedlight = thread->drawargs.Light();
		fixedlight += fixedlight >> 7; // 255 -> 256
	}

	if (OptT::Flags & SWOPT_ColoredFog)
	{
		shade_fade_r = thread->drawargs.ShadeFadeRed();
		shade_fade_g = thread->drawargs.ShadeFadeGreen();
		shade_fade_b = thread->drawargs.ShadeFadeBlue();
		shade_light_r = thread->drawargs.ShadeLightRed();
		shade_light_g = thread->drawargs.ShadeLightGreen();
		shade_light_b = thread->drawargs.ShadeLightBlue();
		desaturate = thread->drawargs.ShadeDesaturate();
		inv_desaturate = 256 - desaturate;
	}

	if (ModeT::BlendOp == STYLEOP_Fuzz)
	{
		fuzzscale = (200 << FRACBITS) / thread->dest_height;
		_fuzzpos = swrenderer::fuzzpos;
	}

	if (ModeT::SWFlags & SWSTYLEF_AlphaTest)
	{
		alphatestbuf = thread->alphatestbuffer;
	}

	uint32_t *dest = (uint32_t*)thread->dest;
	uint32_t *destLine = dest + thread->dest_pitch * y;

	int sseend = x0;
#ifndef NO_SSE
	if (ModeT::BlendOp == STYLEOP_Add &&
		ModeT::BlendSrc == STYLEALPHA_One &&
		ModeT::BlendDest == STYLEALPHA_Zero &&
		(ModeT::Flags & STYLEF_Alpha1) &&
		!(OptT::Flags & SWOPT_ColoredFog) &&
		!(ModeT::Flags & STYLEF_RedIsAlpha) &&
		!(ModeT::SWFlags & SWSTYLEF_Skycap) &&
		!(ModeT::SWFlags & SWSTYLEF_FogBoundary) &&
		!(ModeT::SWFlags & SWSTYLEF_Fill) &&
		!(ModeT::SWFlags & SWSTYLEF_AlphaTest))
	{
		sseend += (x1 - x0) / 2 * 2;

		__m128i mlightshade;
		if (OptT::Flags & SWOPT_FixedLight)
			mlightshade = _mm_set1_epi16(fixedlight);

		__m128i alphamask = _mm_set1_epi32(0xff000000);

		for (int x = x0; x < sseend; x += 2)
		{
			__m128i mfg = _mm_unpacklo_epi8(_mm_setr_epi32(texPixels[texel[x]], texPixels[texel[x + 1]], 0, 0), _mm_setzero_si128());

			if (!(OptT::Flags & SWOPT_FixedLight))
				mlightshade = _mm_shuffle_epi32(_mm_shufflelo_epi16(_mm_cvtsi32_si128(*(int*)&lightarray[x]), _MM_SHUFFLE(1, 1, 0, 0)), _MM_SHUFFLE(1, 1, 0, 0));

			if (OptT::Flags & SWOPT_DynLights)
			{
				__m128i mdynlight = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i*)&dynlights[x]), _mm_setzero_si128());
				mfg = _mm_srli_epi16(_mm_mullo_epi16(_mm_min_epi16(_mm_add_epi16(mdynlight, mlightshade), _mm_set1_epi16(256)), mfg), 8);
			}
			else
			{
				mfg = _mm_srli_epi16(_mm_mullo_epi16(mlightshade, mfg), 8);
			}

			_mm_storel_epi64((__m128i*)&destLine[x], _mm_or_si128(_mm_packus_epi16(mfg, _mm_setzero_si128()), alphamask));
		}
	}
#endif

	for (int x = sseend; x < x1; x++)
	{
		if (ModeT::BlendOp == STYLEOP_Fuzz)
		{
			using namespace swrenderer;

			unsigned int sampleshadeout = APART(texPixels[texel[x]]);
			sampleshadeout += sampleshadeout >> 7; // 255 -> 256

			int scaled_x = (x * fuzzscale) >> FRACBITS;
			int fuzz_x = fuzz_random_x_offset[scaled_x % FUZZ_RANDOM_X_SIZE] + _fuzzpos;

			fixed_t fuzzcount = FUZZTABLE << FRACBITS;
			fixed_t fuzz = ((fuzz_x << FRACBITS) + y * fuzzscale) % fuzzcount;
			unsigned int alpha = fuzzoffset[fuzz >> FRACBITS];

			sampleshadeout = (sampleshadeout * alpha) >> 5;

			uint32_t a = 256 - sampleshadeout;

			uint32_t dest = destLine[x];
			uint32_t out_r = (RPART(dest) * a) >> 8;
			uint32_t out_g = (GPART(dest) * a) >> 8;
			uint32_t out_b = (BPART(dest) * a) >> 8;
			destLine[x] = MAKEARGB(255, out_r, out_g, out_b);
		}
		else if (ModeT::SWFlags & SWSTYLEF_Skycap)
		{
			uint32_t fg = texPixels[texel[x]];

			int v = texelV[x];
			int start_fade = 2; // How fast it should fade out
			int alpha_top = clamp(v >> (16 - start_fade), 0, 256);
			int alpha_bottom = clamp(((2 << 24) - v) >> (16 - start_fade), 0, 256);
			int a = MIN(alpha_top, alpha_bottom);
			int inv_a = 256 - a;

			if (a == 256)
			{
				destLine[x] = fg;
			}
			else
			{
				uint32_t r = RPART(fg);
				uint32_t g = GPART(fg);
				uint32_t b = BPART(fg);
				uint32_t fg_a = APART(fg);
				uint32_t bg_red = RPART(fillcolor);
				uint32_t bg_green = GPART(fillcolor);
				uint32_t bg_blue = BPART(fillcolor);
				r = (r * a + bg_red * inv_a + 127) >> 8;
				g = (g * a + bg_green * inv_a + 127) >> 8;
				b = (b * a + bg_blue * inv_a + 127) >> 8;

				destLine[x] = MAKEARGB(255, r, g, b);
			}
		}
		else if (ModeT::SWFlags & SWSTYLEF_FogBoundary)
		{
			uint32_t fg = destLine[x];

			int lightshade;
			if (OptT::Flags & SWOPT_FixedLight)
			{
				lightshade = fixedlight;
			}
			else
			{
				lightshade = lightarray[x];
			}

			uint32_t shadedfg_r, shadedfg_g, shadedfg_b;
			if (OptT::Flags & SWOPT_ColoredFog)
			{
				uint32_t fg_r = RPART(fg);
				uint32_t fg_g = GPART(fg);
				uint32_t fg_b = BPART(fg);
				uint32_t intensity = ((fg_r * 77 + fg_g * 143 + fg_b * 37) >> 8) * desaturate;
				int inv_light = 256 - lightshade;
				shadedfg_r = (((shade_fade_r * inv_light + ((fg_r * inv_desaturate + intensity) >> 8) * lightshade) >> 8) * shade_light_r) >> 8;
				shadedfg_g = (((shade_fade_g * inv_light + ((fg_g * inv_desaturate + intensity) >> 8) * lightshade) >> 8) * shade_light_g) >> 8;
				shadedfg_b = (((shade_fade_b * inv_light + ((fg_b * inv_desaturate + intensity) >> 8) * lightshade) >> 8) * shade_light_b) >> 8;
			}
			else
			{
				shadedfg_r = (RPART(fg) * lightshade) >> 8;
				shadedfg_g = (GPART(fg) * lightshade) >> 8;
				shadedfg_b = (BPART(fg) * lightshade) >> 8;
			}

			destLine[x] = MAKEARGB(255, shadedfg_r, shadedfg_g, shadedfg_b);
		}
		else
		{
			uint32_t fg = 0;

			if (ModeT::SWFlags & SWSTYLEF_Fill)
			{
				fg = fillcolor;
			}
			else if (ModeT::SWFlags & SWSTYLEF_Translated)
			{
				fg = translation[((const uint8_t*)texPixels)[texel[x]]];
			}
			else if (ModeT::Flags & STYLEF_RedIsAlpha)
			{
				fg = ((const uint8_t*)texPixels)[texel[x]];
			}
			else
			{
				fg = texPixels[texel[x]];
			}

			if ((ModeT::Flags & STYLEF_ColorIsFixed) && !(ModeT::SWFlags & SWSTYLEF_Fill))
			{
				if (ModeT::Flags & STYLEF_RedIsAlpha)
					fg = (fg << 24) | (fillcolor & 0x00ffffff);
				else
					fg = (fg & 0xff000000) | (fillcolor & 0x00ffffff);
			}

			uint32_t fgalpha = fg >> 24;

			if (!(ModeT::Flags & STYLEF_Alpha1))
			{
				fgalpha = (fgalpha * actoralpha) >> 8;
			}

			if (ModeT::SWFlags & SWSTYLEF_AlphaTest)
			{
				alphatestbuf[x] = fgalpha;
			}

			uint32_t lightshade;
			if (OptT::Flags & SWOPT_FixedLight)
			{
				lightshade = fixedlight;
			}
			else
			{
				lightshade = lightarray[x];
			}

			uint32_t shadedfg_r, shadedfg_g, shadedfg_b;
			if (OptT::Flags & SWOPT_ColoredFog)
			{
				uint32_t fg_r = RPART(fg);
				uint32_t fg_g = GPART(fg);
				uint32_t fg_b = BPART(fg);
				uint32_t intensity = ((fg_r * 77 + fg_g * 143 + fg_b * 37) >> 8) * desaturate;
				int inv_light = 256 - lightshade;
				shadedfg_r = (((shade_fade_r * inv_light + ((fg_r * inv_desaturate + intensity) >> 8) * lightshade) >> 8) * shade_light_r) >> 8;
				shadedfg_g = (((shade_fade_g * inv_light + ((fg_g * inv_desaturate + intensity) >> 8) * lightshade) >> 8) * shade_light_g) >> 8;
				shadedfg_b = (((shade_fade_b * inv_light + ((fg_b * inv_desaturate + intensity) >> 8) * lightshade) >> 8) * shade_light_b) >> 8;

				if (OptT::Flags & SWOPT_DynLights)
				{
					shadedfg_r = MIN(shadedfg_r + ((fg_r * RPART(dynlights[x])) >> 8), (uint32_t)255);
					shadedfg_g = MIN(shadedfg_g + ((fg_g * GPART(dynlights[x])) >> 8), (uint32_t)255);
					shadedfg_b = MIN(shadedfg_b + ((fg_b * BPART(dynlights[x])) >> 8), (uint32_t)255);
				}
			}
			else
			{
				if (OptT::Flags & SWOPT_DynLights)
				{
					shadedfg_r = (RPART(fg) * MIN(lightshade + RPART(dynlights[x]), (uint32_t)256)) >> 8;
					shadedfg_g = (GPART(fg) * MIN(lightshade + GPART(dynlights[x]), (uint32_t)256)) >> 8;
					shadedfg_b = (BPART(fg) * MIN(lightshade + BPART(dynlights[x]), (uint32_t)256)) >> 8;
				}
				else
				{
					shadedfg_r = (RPART(fg) * lightshade) >> 8;
					shadedfg_g = (GPART(fg) * lightshade) >> 8;
					shadedfg_b = (BPART(fg) * lightshade) >> 8;
				}
			}

			if (ModeT::BlendSrc == STYLEALPHA_One && ModeT::BlendDest == STYLEALPHA_Zero)
			{
				destLine[x] = MAKEARGB(255, shadedfg_r, shadedfg_g, shadedfg_b);
			}
			else if (ModeT::BlendSrc == STYLEALPHA_One && ModeT::BlendDest == STYLEALPHA_One)
			{
				uint32_t dest = destLine[x];

				if (ModeT::BlendOp == STYLEOP_Add)
				{
					uint32_t out_r = MIN<uint32_t>(RPART(dest) + shadedfg_r, 255);
					uint32_t out_g = MIN<uint32_t>(GPART(dest) + shadedfg_g, 255);
					uint32_t out_b = MIN<uint32_t>(BPART(dest) + shadedfg_b, 255);
					destLine[x] = MAKEARGB(255, out_r, out_g, out_b);
				}
				else if (ModeT::BlendOp == STYLEOP_RevSub)
				{
					uint32_t out_r = MAX<uint32_t>(RPART(dest) - shadedfg_r, 0);
					uint32_t out_g = MAX<uint32_t>(GPART(dest) - shadedfg_g, 0);
					uint32_t out_b = MAX<uint32_t>(BPART(dest) - shadedfg_b, 0);
					destLine[x] = MAKEARGB(255, out_r, out_g, out_b);
				}
				else //if (ModeT::BlendOp == STYLEOP_Sub)
				{
					uint32_t out_r = MAX<uint32_t>(shadedfg_r - RPART(dest), 0);
					uint32_t out_g = MAX<uint32_t>(shadedfg_g - GPART(dest), 0);
					uint32_t out_b = MAX<uint32_t>(shadedfg_b - BPART(dest), 0);
					destLine[x] = MAKEARGB(255, out_r, out_g, out_b);
				}
			}
			else if (ModeT::SWFlags & SWSTYLEF_SrcColorOneMinusSrcColor)
			{
				uint32_t dest = destLine[x];

				uint32_t sfactor_r = shadedfg_r; sfactor_r += sfactor_r >> 7; // 255 -> 256
				uint32_t sfactor_g = shadedfg_g; sfactor_g += sfactor_g >> 7; // 255 -> 256
				uint32_t sfactor_b = shadedfg_b; sfactor_b += sfactor_b >> 7; // 255 -> 256
				uint32_t sfactor_a = fgalpha; sfactor_a += sfactor_a >> 7; // 255 -> 256
				uint32_t dfactor_r = 256 - sfactor_r;
				uint32_t dfactor_g = 256 - sfactor_g;
				uint32_t dfactor_b = 256 - sfactor_b;
				uint32_t out_r = (RPART(dest) * dfactor_r + shadedfg_r * sfactor_r + 128) >> 8;
				uint32_t out_g = (GPART(dest) * dfactor_g + shadedfg_g * sfactor_g + 128) >> 8;
				uint32_t out_b = (BPART(dest) * dfactor_b + shadedfg_b * sfactor_b + 128) >> 8;

				destLine[x] = MAKEARGB(255, out_r, out_g, out_b);
			}
			else if (ModeT::BlendSrc == STYLEALPHA_Src && ModeT::BlendDest == STYLEALPHA_InvSrc && fgalpha == 255)
			{
				destLine[x] = MAKEARGB(255, shadedfg_r, shadedfg_g, shadedfg_b);
			}
			else if (ModeT::BlendSrc != STYLEALPHA_Src || ModeT::BlendDest != STYLEALPHA_InvSrc || fgalpha != 0)
			{
				uint32_t dest = destLine[x];

				uint32_t sfactor = fgalpha; sfactor += sfactor >> 7; // 255 -> 256
				uint32_t src_r = shadedfg_r * sfactor;
				uint32_t src_g = shadedfg_g * sfactor;
				uint32_t src_b = shadedfg_b * sfactor;
				uint32_t dest_r = RPART(dest);
				uint32_t dest_g = GPART(dest);
				uint32_t dest_b = BPART(dest);
				if (ModeT::BlendDest == STYLEALPHA_One)
				{
					dest_r <<= 8;
					dest_g <<= 8;
					dest_b <<= 8;
				}
				else
				{
					uint32_t dfactor = 256 - sfactor;
					dest_r *= dfactor;
					dest_g *= dfactor;
					dest_b *= dfactor;
				}

				uint32_t out_r, out_g, out_b;
				if (ModeT::BlendOp == STYLEOP_Add)
				{
					if (ModeT::BlendDest == STYLEALPHA_One)
					{
						out_r = MIN<int32_t>((dest_r + src_r + 128) >> 8, 255);
						out_g = MIN<int32_t>((dest_g + src_g + 128) >> 8, 255);
						out_b = MIN<int32_t>((dest_b + src_b + 128) >> 8, 255);
					}
					else
					{
						out_r = (dest_r + src_r + 128) >> 8;
						out_g = (dest_g + src_g + 128) >> 8;
						out_b = (dest_b + src_b + 128) >> 8;
					}
				}
				else if (ModeT::BlendOp == STYLEOP_RevSub)
				{
					out_r = MAX<int32_t>(static_cast<int32_t>(dest_r - src_r + 128) >> 8, 0);
					out_g = MAX<int32_t>(static_cast<int32_t>(dest_g - src_g + 128) >> 8, 0);
					out_b = MAX<int32_t>(static_cast<int32_t>(dest_b - src_b + 128) >> 8, 0);
				}
				else //if (ModeT::BlendOp == STYLEOP_Sub)
				{
					out_r = MAX<int32_t>(static_cast<int32_t>(src_r - dest_r + 128) >> 8, 0);
					out_g = MAX<int32_t>(static_cast<int32_t>(src_g - dest_g + 128) >> 8, 0);
					out_b = MAX<int32_t>(static_cast<int32_t>(src_b - dest_b + 128) >> 8, 0);
				}

				destLine[x] = MAKEARGB(255, out_r, out_g, out_b);
			}
		}
	}
}

template<typename ModeT>
void DrawSpan32(int y, int x0, int x1, const TriDrawTriangleArgs *args, PolyTriangleThreadData *thread)
{
	using namespace TriScreenDrawerModes;

	if (thread->drawargs.NumLights() == 0 && thread->drawargs.DynLightColor() == 0)
	{
		if (!thread->drawargs.FixedLight())
		{
			if (thread->drawargs.SimpleShade())
				DrawSpanOpt32<ModeT, DrawerOpt>(y, x0, x1, args, thread);
			else
				DrawSpanOpt32<ModeT, DrawerOptC>(y, x0, x1, args, thread);
		}
		else
		{
			if (thread->drawargs.SimpleShade())
				DrawSpanOpt32<ModeT, DrawerOptF>(y, x0, x1, args, thread);
			else
				DrawSpanOpt32<ModeT, DrawerOptCF>(y, x0, x1, args, thread);
		}
	}
	else
	{
		if (!thread->drawargs.FixedLight())
		{
			if (thread->drawargs.SimpleShade())
				DrawSpanOpt32<ModeT, DrawerOptL>(y, x0, x1, args, thread);
			else
				DrawSpanOpt32<ModeT, DrawerOptLC>(y, x0, x1, args, thread);
		}
		else
		{
			if (thread->drawargs.SimpleShade())
				DrawSpanOpt32<ModeT, DrawerOptLF>(y, x0, x1, args, thread);
			else
				DrawSpanOpt32<ModeT, DrawerOptLCF>(y, x0, x1, args, thread);
		}
	}
}

template<typename ModeT, typename OptT>
void DrawSpanOpt8(int y, int x0, int x1, const TriDrawTriangleArgs *args, PolyTriangleThreadData *thread)
{
	using namespace TriScreenDrawerModes;

	StepSpan<ModeT, OptT, 8>(y, x0, x1, args, thread);

	uint32_t fixedlight, capcolor;
	fixed_t fuzzscale;
	int _fuzzpos;
	const uint8_t *colormaps, *texPixels, *translation;
	uint32_t fillcolor;
	int actoralpha;

	uint32_t *texel = thread->texel;
	int32_t *texelV = thread->texelV;
	uint16_t *lightarray = thread->lightarray;
	uint32_t *dynlights = thread->dynlights;

	colormaps = thread->drawargs.BaseColormap();

	if (!(ModeT::SWFlags & SWSTYLEF_Fill) && !(ModeT::SWFlags & SWSTYLEF_FogBoundary))
	{
		texPixels = thread->drawargs.TexturePixels();
	}

	if (ModeT::SWFlags & SWSTYLEF_Translated)
	{
		translation = thread->drawargs.Translation();
	}

	if ((ModeT::SWFlags & SWSTYLEF_Fill) || (ModeT::SWFlags & SWSTYLEF_Skycap) || (ModeT::Flags & STYLEF_ColorIsFixed))
	{
		fillcolor = thread->drawargs.Color();
	}

	if (!(ModeT::Flags & STYLEF_Alpha1))
	{
		actoralpha = thread->drawargs.Alpha();
	}

	if (ModeT::SWFlags & SWSTYLEF_Skycap)
		capcolor = GPalette.BaseColors[fillcolor].d;

	if (OptT::Flags & SWOPT_FixedLight)
	{
		fixedlight = thread->drawargs.Light();
		fixedlight += fixedlight >> 7; // 255 -> 256
		fixedlight = ((256 - fixedlight) * NUMCOLORMAPS) & 0xffffff00;
	}

	if (ModeT::BlendOp == STYLEOP_Fuzz)
	{
		fuzzscale = (200 << FRACBITS) / thread->dest_height;
		_fuzzpos = swrenderer::fuzzpos;
	}

	uint8_t *dest = (uint8_t*)thread->dest;
	uint8_t *destLine = dest + thread->dest_pitch * y;

	for (int x = x0; x < x1; x++)
	{
		if (ModeT::BlendOp == STYLEOP_Fuzz)
		{
			using namespace swrenderer;

			unsigned int sampleshadeout = (texPixels[texel[x]] != 0) ? 256 : 0;

			int scaled_x = (x * fuzzscale) >> FRACBITS;
			int fuzz_x = fuzz_random_x_offset[scaled_x % FUZZ_RANDOM_X_SIZE] + _fuzzpos;

			fixed_t fuzzcount = FUZZTABLE << FRACBITS;
			fixed_t fuzz = ((fuzz_x << FRACBITS) + y * fuzzscale) % fuzzcount;
			unsigned int alpha = fuzzoffset[fuzz >> FRACBITS];

			sampleshadeout = (sampleshadeout * alpha) >> 5;

			uint32_t a = 256 - sampleshadeout;

			uint32_t dest = GPalette.BaseColors[destLine[x]].d;
			uint32_t r = (RPART(dest) * a) >> 8;
			uint32_t g = (GPART(dest) * a) >> 8;
			uint32_t b = (BPART(dest) * a) >> 8;
			destLine[x] = RGB256k.All[((r >> 2) << 12) | ((g >> 2) << 6) | (b >> 2)];
		}
		else if (ModeT::SWFlags & SWSTYLEF_Skycap)
		{
			int32_t v = texelV[x];
			int fg = texPixels[texel[x]];

			int start_fade = 2; // How fast it should fade out
			int alpha_top = clamp(v >> (16 - start_fade), 0, 256);
			int alpha_bottom = clamp(((2 << 24) - v) >> (16 - start_fade), 0, 256);
			int a = MIN(alpha_top, alpha_bottom);
			int inv_a = 256 - a;

			if (a == 256)
			{
				destLine[x] = fg;
			}
			else
			{
				uint32_t texelrgb = GPalette.BaseColors[fg].d;

				uint32_t r = RPART(texelrgb);
				uint32_t g = GPART(texelrgb);
				uint32_t b = BPART(texelrgb);
				uint32_t fg_a = APART(texelrgb);
				uint32_t bg_red = RPART(capcolor);
				uint32_t bg_green = GPART(capcolor);
				uint32_t bg_blue = BPART(capcolor);
				r = (r * a + bg_red * inv_a + 127) >> 8;
				g = (g * a + bg_green * inv_a + 127) >> 8;
				b = (b * a + bg_blue * inv_a + 127) >> 8;

				destLine[x] = RGB256k.All[((r >> 2) << 12) | ((g >> 2) << 6) | (b >> 2)];
			}
		}
		else if (ModeT::SWFlags & SWSTYLEF_FogBoundary)
		{
			int fg = destLine[x];

			uint8_t shadedfg;
			if (OptT::Flags & SWOPT_FixedLight)
			{
				shadedfg = colormaps[fixedlight + fg];
			}
			else
			{
				shadedfg = colormaps[lightarray[x] + fg];
			}

			destLine[x] = shadedfg;
		}
		else
		{
			int fg;
			if (ModeT::SWFlags & SWSTYLEF_Fill)
			{
				fg = fillcolor;
			}
			else
			{
				fg = texPixels[texel[x]];
			}

			int fgalpha = 255;

			if (ModeT::BlendDest == STYLEALPHA_InvSrc)
			{
				if (fg == 0)
					fgalpha = 0;
			}

			if ((ModeT::Flags & STYLEF_ColorIsFixed) && !(ModeT::SWFlags & SWSTYLEF_Fill))
			{
				if (ModeT::Flags & STYLEF_RedIsAlpha)
					fgalpha = fg;
				fg = fillcolor;
			}

			if (!(ModeT::Flags & STYLEF_Alpha1))
			{
				fgalpha = (fgalpha * actoralpha) >> 8;
			}

			if (ModeT::SWFlags & SWSTYLEF_Translated)
				fg = translation[fg];

			uint8_t shadedfg;
			if (OptT::Flags & SWOPT_FixedLight)
			{
				shadedfg = colormaps[fixedlight + fg];
			}
			else
			{
				shadedfg = colormaps[lightarray[x] + fg];
			}

			if (OptT::Flags & SWOPT_DynLights)
			{
				uint32_t lit = dynlights[x];
				if (lit & 0x00ffffff)
				{
					uint32_t fgrgb = GPalette.BaseColors[fg];
					uint32_t shadedfgrgb = GPalette.BaseColors[shadedfg];

					uint32_t out_r = MIN(((RPART(fgrgb) * RPART(lit)) >> 8) + RPART(shadedfgrgb), (uint32_t)255);
					uint32_t out_g = MIN(((GPART(fgrgb) * GPART(lit)) >> 8) + GPART(shadedfgrgb), (uint32_t)255);
					uint32_t out_b = MIN(((BPART(fgrgb) * BPART(lit)) >> 8) + BPART(shadedfgrgb), (uint32_t)255);
					shadedfg = RGB256k.All[((out_r >> 2) << 12) | ((out_g >> 2) << 6) | (out_b >> 2)];
				}
			}

			if (ModeT::BlendSrc == STYLEALPHA_One && ModeT::BlendDest == STYLEALPHA_Zero)
			{
				destLine[x] = shadedfg;
			}
			else if (ModeT::BlendSrc == STYLEALPHA_One && ModeT::BlendDest == STYLEALPHA_One)
			{
				uint32_t src = GPalette.BaseColors[shadedfg];
				uint32_t dest = GPalette.BaseColors[destLine[x]];

				if (ModeT::BlendOp == STYLEOP_Add)
				{
					uint32_t out_r = MIN<uint32_t>(RPART(dest) + RPART(src), 255);
					uint32_t out_g = MIN<uint32_t>(GPART(dest) + GPART(src), 255);
					uint32_t out_b = MIN<uint32_t>(BPART(dest) + BPART(src), 255);
					destLine[x] = RGB256k.All[((out_r >> 2) << 12) | ((out_g >> 2) << 6) | (out_b >> 2)];
				}
				else if (ModeT::BlendOp == STYLEOP_RevSub)
				{
					uint32_t out_r = MAX<uint32_t>(RPART(dest) - RPART(src), 0);
					uint32_t out_g = MAX<uint32_t>(GPART(dest) - GPART(src), 0);
					uint32_t out_b = MAX<uint32_t>(BPART(dest) - BPART(src), 0);
					destLine[x] = RGB256k.All[((out_r >> 2) << 12) | ((out_g >> 2) << 6) | (out_b >> 2)];
				}
				else //if (ModeT::BlendOp == STYLEOP_Sub)
				{
					uint32_t out_r = MAX<uint32_t>(RPART(src) - RPART(dest), 0);
					uint32_t out_g = MAX<uint32_t>(GPART(src) - GPART(dest), 0);
					uint32_t out_b = MAX<uint32_t>(BPART(src) - BPART(dest), 0);
					destLine[x] = RGB256k.All[((out_r >> 2) << 12) | ((out_g >> 2) << 6) | (out_b >> 2)];
				}
			}
			else if (ModeT::SWFlags & SWSTYLEF_SrcColorOneMinusSrcColor)
			{
				uint32_t src = GPalette.BaseColors[shadedfg];
				uint32_t dest = GPalette.BaseColors[destLine[x]];

				uint32_t sfactor_r = RPART(src); sfactor_r += sfactor_r >> 7; // 255 -> 256
				uint32_t sfactor_g = GPART(src); sfactor_g += sfactor_g >> 7; // 255 -> 256
				uint32_t sfactor_b = BPART(src); sfactor_b += sfactor_b >> 7; // 255 -> 256
				uint32_t sfactor_a = fgalpha; sfactor_a += sfactor_a >> 7; // 255 -> 256
				uint32_t dfactor_r = 256 - sfactor_r;
				uint32_t dfactor_g = 256 - sfactor_g;
				uint32_t dfactor_b = 256 - sfactor_b;
				uint32_t out_r = (RPART(dest) * dfactor_r + RPART(src) * sfactor_r + 128) >> 8;
				uint32_t out_g = (GPART(dest) * dfactor_g + GPART(src) * sfactor_g + 128) >> 8;
				uint32_t out_b = (BPART(dest) * dfactor_b + BPART(src) * sfactor_b + 128) >> 8;

				destLine[x] = RGB256k.All[((out_r >> 2) << 12) | ((out_g >> 2) << 6) | (out_b >> 2)];
			}
			else if (ModeT::BlendSrc == STYLEALPHA_Src && ModeT::BlendDest == STYLEALPHA_InvSrc && fgalpha == 255)
			{
				destLine[x] = shadedfg;
			}
			else if (ModeT::BlendSrc != STYLEALPHA_Src || ModeT::BlendDest != STYLEALPHA_InvSrc || fgalpha != 0)
			{
				uint32_t src = GPalette.BaseColors[shadedfg];
				uint32_t dest = GPalette.BaseColors[destLine[x]];

				uint32_t sfactor = fgalpha; sfactor += sfactor >> 7; // 255 -> 256
				uint32_t dfactor = 256 - sfactor;
				uint32_t src_r = RPART(src) * sfactor;
				uint32_t src_g = GPART(src) * sfactor;
				uint32_t src_b = BPART(src) * sfactor;
				uint32_t dest_r = RPART(dest);
				uint32_t dest_g = GPART(dest);
				uint32_t dest_b = BPART(dest);
				if (ModeT::BlendDest == STYLEALPHA_One)
				{
					dest_r <<= 8;
					dest_g <<= 8;
					dest_b <<= 8;
				}
				else
				{
					uint32_t dfactor = 256 - sfactor;
					dest_r *= dfactor;
					dest_g *= dfactor;
					dest_b *= dfactor;
				}

				uint32_t out_r, out_g, out_b;
				if (ModeT::BlendOp == STYLEOP_Add)
				{
					if (ModeT::BlendDest == STYLEALPHA_One)
					{
						out_r = MIN<int32_t>((dest_r + src_r + 128) >> 8, 255);
						out_g = MIN<int32_t>((dest_g + src_g + 128) >> 8, 255);
						out_b = MIN<int32_t>((dest_b + src_b + 128) >> 8, 255);
					}
					else
					{
						out_r = (dest_r + src_r + 128) >> 8;
						out_g = (dest_g + src_g + 128) >> 8;
						out_b = (dest_b + src_b + 128) >> 8;
					}
				}
				else if (ModeT::BlendOp == STYLEOP_RevSub)
				{
					out_r = MAX<int32_t>(static_cast<int32_t>(dest_r - src_r + 128) >> 8, 0);
					out_g = MAX<int32_t>(static_cast<int32_t>(dest_g - src_g + 128) >> 8, 0);
					out_b = MAX<int32_t>(static_cast<int32_t>(dest_b - src_b + 128) >> 8, 0);
				}
				else //if (ModeT::BlendOp == STYLEOP_Sub)
				{
					out_r = MAX<int32_t>(static_cast<int32_t>(src_r - dest_r + 128) >> 8, 0);
					out_g = MAX<int32_t>(static_cast<int32_t>(src_g - dest_g + 128) >> 8, 0);
					out_b = MAX<int32_t>(static_cast<int32_t>(src_b - dest_b + 128) >> 8, 0);
				}

				destLine[x] = RGB256k.All[((out_r >> 2) << 12) | ((out_g >> 2) << 6) | (out_b >> 2)];
			}
		}
	}
}

template<typename ModeT>
void DrawSpan8(int y, int x0, int x1, const TriDrawTriangleArgs *args, PolyTriangleThreadData *thread)
{
	using namespace TriScreenDrawerModes;

	if (thread->drawargs.NumLights() == 0 && thread->drawargs.DynLightColor() == 0)
	{
		if (!thread->drawargs.FixedLight())
			DrawSpanOpt8<ModeT, DrawerOptC>(y, x0, x1, args, thread);
		else
			DrawSpanOpt8<ModeT, DrawerOptCF>(y, x0, x1, args, thread);
	}
	else
	{
		if (!thread->drawargs.FixedLight())
			DrawSpanOpt8<ModeT, DrawerOptLC>(y, x0, x1, args, thread);
		else
			DrawSpanOpt8<ModeT, DrawerOptLCF>(y, x0, x1, args, thread);
	}
}

void(*ScreenTriangle::SpanDrawers8[])(int, int, int, const TriDrawTriangleArgs *, PolyTriangleThreadData *) =
{
	&DrawSpan8<TriScreenDrawerModes::StyleOpaque>,
	&DrawSpan8<TriScreenDrawerModes::StyleSkycap>,
	&DrawSpan8<TriScreenDrawerModes::StyleFogBoundary>,
	&DrawSpan8<TriScreenDrawerModes::StyleSrcColor>,
	&DrawSpan8<TriScreenDrawerModes::StyleFill>,
	&DrawSpan8<TriScreenDrawerModes::StyleFillTranslucent>,
	&DrawSpan8<TriScreenDrawerModes::StyleNormal>,
	&DrawSpan8<TriScreenDrawerModes::StyleAlphaTest>,
	&DrawSpan8<TriScreenDrawerModes::StyleFuzzy>,
	&DrawSpan8<TriScreenDrawerModes::StyleStencil>,
	&DrawSpan8<TriScreenDrawerModes::StyleTranslucent>,
	&DrawSpan8<TriScreenDrawerModes::StyleAdd>,
	&DrawSpan8<TriScreenDrawerModes::StyleShaded>,
	&DrawSpan8<TriScreenDrawerModes::StyleTranslucentStencil>,
	&DrawSpan8<TriScreenDrawerModes::StyleShadow>,
	&DrawSpan8<TriScreenDrawerModes::StyleSubtract>,
	&DrawSpan8<TriScreenDrawerModes::StyleAddStencil>,
	&DrawSpan8<TriScreenDrawerModes::StyleAddShaded>,
	&DrawSpan8<TriScreenDrawerModes::StyleOpaqueTranslated>,
	&DrawSpan8<TriScreenDrawerModes::StyleSrcColorTranslated>,
	&DrawSpan8<TriScreenDrawerModes::StyleNormalTranslated>,
	&DrawSpan8<TriScreenDrawerModes::StyleStencilTranslated>,
	&DrawSpan8<TriScreenDrawerModes::StyleTranslucentTranslated>,
	&DrawSpan8<TriScreenDrawerModes::StyleAddTranslated>,
	&DrawSpan8<TriScreenDrawerModes::StyleShadedTranslated>,
	&DrawSpan8<TriScreenDrawerModes::StyleTranslucentStencilTranslated>,
	&DrawSpan8<TriScreenDrawerModes::StyleShadowTranslated>,
	&DrawSpan8<TriScreenDrawerModes::StyleSubtractTranslated>,
	&DrawSpan8<TriScreenDrawerModes::StyleAddStencilTranslated>,
	&DrawSpan8<TriScreenDrawerModes::StyleAddShadedTranslated>
};

void(*ScreenTriangle::SpanDrawers32[])(int, int, int, const TriDrawTriangleArgs *, PolyTriangleThreadData *) =
{
	&DrawSpan32<TriScreenDrawerModes::StyleOpaque>,
	&DrawSpan32<TriScreenDrawerModes::StyleSkycap>,
	&DrawSpan32<TriScreenDrawerModes::StyleFogBoundary>,
	&DrawSpan32<TriScreenDrawerModes::StyleSrcColor>,
	&DrawSpan32<TriScreenDrawerModes::StyleFill>,
	&DrawSpan32<TriScreenDrawerModes::StyleFillTranslucent>,
	&DrawSpan32<TriScreenDrawerModes::StyleNormal>,
	&DrawSpan32<TriScreenDrawerModes::StyleAlphaTest>,
	&DrawSpan32<TriScreenDrawerModes::StyleFuzzy>,
	&DrawSpan32<TriScreenDrawerModes::StyleStencil>,
	&DrawSpan32<TriScreenDrawerModes::StyleTranslucent>,
	&DrawSpan32<TriScreenDrawerModes::StyleAdd>,
	&DrawSpan32<TriScreenDrawerModes::StyleShaded>,
	&DrawSpan32<TriScreenDrawerModes::StyleTranslucentStencil>,
	&DrawSpan32<TriScreenDrawerModes::StyleShadow>,
	&DrawSpan32<TriScreenDrawerModes::StyleSubtract>,
	&DrawSpan32<TriScreenDrawerModes::StyleAddStencil>,
	&DrawSpan32<TriScreenDrawerModes::StyleAddShaded>,
	&DrawSpan32<TriScreenDrawerModes::StyleOpaqueTranslated>,
	&DrawSpan32<TriScreenDrawerModes::StyleSrcColorTranslated>,
	&DrawSpan32<TriScreenDrawerModes::StyleNormalTranslated>,
	&DrawSpan32<TriScreenDrawerModes::StyleStencilTranslated>,
	&DrawSpan32<TriScreenDrawerModes::StyleTranslucentTranslated>,
	&DrawSpan32<TriScreenDrawerModes::StyleAddTranslated>,
	&DrawSpan32<TriScreenDrawerModes::StyleShadedTranslated>,
	&DrawSpan32<TriScreenDrawerModes::StyleTranslucentStencilTranslated>,
	&DrawSpan32<TriScreenDrawerModes::StyleShadowTranslated>,
	&DrawSpan32<TriScreenDrawerModes::StyleSubtractTranslated>,
	&DrawSpan32<TriScreenDrawerModes::StyleAddStencilTranslated>,
	&DrawSpan32<TriScreenDrawerModes::StyleAddShadedTranslated>
};

void(*ScreenTriangle::TriangleDrawers[])(const TriDrawTriangleArgs *args, PolyTriangleThreadData *thread, int16_t *edges, int topY, int bottomY) =
{
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt0>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt1>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt2>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt3>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt4>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt5>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt6>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt7>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt8>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt9>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt10>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt11>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt12>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt13>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt14>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt15>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt16>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt17>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt18>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt19>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt20>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt21>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt22>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt23>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt24>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt25>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt26>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt27>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt28>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt29>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt30>,
	&DrawTriangle<TriScreenDrawerModes::TriangleOpt31>
};

int ScreenTriangle::FuzzStart = 0;

#endif

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
	uint8_t value = thread->StencilWriteValue;
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
	__m128 f = _mm_set_ss(value);
	__m128 t = _mm_cvtepi32_ps(_mm_cvttps_epi32(f));
	__m128 r = _mm_sub_ps(t, _mm_and_ps(_mm_cmplt_ps(f, t), _mm_set_ss(1.0f)));
	return _mm_cvtss_f32(_mm_sub_ss(f, r));
}
#endif

static uint32_t sampleTexture(float u, float v, const void* texPixels, int texWidth, int texHeight, bool texBgra)
{
	int texelX = MIN(static_cast<int>(wrap(u) * texWidth), texWidth - 1);
	int texelY = MIN(static_cast<int>(wrap(v) * texHeight), texHeight - 1);
	int texelOffset = texelX * texHeight + texelY;
	if (texBgra)
	{
		return static_cast<const uint32_t*>(texPixels)[texelOffset];
	}
	else
	{
		uint32_t c = static_cast<const uint8_t*>(texPixels)[texelOffset];
		return (c << 16) | 0xff000000;
	}
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
		int texWidth = thread->textures[0].width;
		int texHeight = thread->textures[0].height;
		const void* texPixels = thread->textures[0].pixels;
		bool texBgra = thread->textures[0].bgra;

		int tex2Width = thread->textures[1].width;
		int tex2Height = thread->textures[1].height;
		const void* tex2Pixels = thread->textures[1].pixels;
		bool tex2Bgra = thread->textures[1].bgra;

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
			uint32_t t1 = sampleTexture(u[x], v[x], texPixels, texWidth, texHeight, texBgra);
			uint32_t t2 = sampleTexture(u[x], 1.0f - v[x], tex2Pixels, tex2Width, tex2Height, tex2Bgra);

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
	else if (thread->EffectState == SHADER_Paletted) // func_paletted
	{
		int texWidth = thread->textures[0].width;
		int texHeight = thread->textures[0].height;
		const void* texPixels = thread->textures[0].pixels;
		bool texBgra = thread->textures[0].bgra;

		const uint32_t* lut = (const uint32_t*)thread->textures[1].pixels;

		for (int x = x0; x < x1; x++)
		{
			fragcolor[x] = lut[RPART(sampleTexture(u[x], v[x], texPixels, texWidth, texHeight, texBgra))] | 0xff000000;
		}
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
		int texWidth = thread->textures[0].width;
		int texHeight = thread->textures[0].height;
		const void* texPixels = thread->textures[0].pixels;
		bool texBgra = thread->textures[0].bgra;
		
		switch (constants->uTextureMode)
		{
		default:
		case TM_NORMAL:
		case TM_FOGLAYER:
			for (int x = x0; x < x1; x++)
			{
				uint32_t texel = sampleTexture(u[x], v[x], texPixels, texWidth, texHeight, texBgra);
				fragcolor[x] = texel;
			}
			break;
		case TM_STENCIL:	// TM_STENCIL
			for (int x = x0; x < x1; x++)
			{
				uint32_t texel = sampleTexture(u[x], v[x], texPixels, texWidth, texHeight, texBgra);
				fragcolor[x] = texel | 0x00ffffff;
			}
			break;
		case TM_OPAQUE:
			for (int x = x0; x < x1; x++)
			{
				uint32_t texel = sampleTexture(u[x], v[x], texPixels, texWidth, texHeight, texBgra);
				fragcolor[x] = texel | 0xff000000;
			}
			break;
		case TM_INVERSE:
			for (int x = x0; x < x1; x++)
			{
				uint32_t texel = sampleTexture(u[x], v[x], texPixels, texWidth, texHeight, texBgra);
				fragcolor[x] = MAKEARGB(APART(texel), 0xff - RPART(texel), 0xff - BPART(texel), 0xff - GPART(texel));
			}
			break;
		case TM_ALPHATEXTURE:
			for (int x = x0; x < x1; x++)
			{
				uint32_t texel = sampleTexture(u[x], v[x], texPixels, texWidth, texHeight, texBgra);
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
					fragcolor[x] = sampleTexture(u[x], v[x], texPixels, texWidth, texHeight, texBgra);
				else
					fragcolor[x] = 0;
			}
			break;
		case TM_INVERTOPAQUE:
			for (int x = x0; x < x1; x++)
			{
				uint32_t texel = sampleTexture(u[x], v[x], texPixels, texWidth, texHeight, texBgra);
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

	if (thread->WriteColor)
		WriteColor(y, x0, x1, thread);
	if (thread->WriteDepth)
		WriteDepth(y, x0, x1, thread);
	if (thread->WriteStencil)
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
			stencilTestValue = thread->StencilTestValue;
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
	if (thread->DepthTest) opt |= TriScreenDrawerModes::SWTRI_DepthTest;
	if (thread->StencilTest) opt |= TriScreenDrawerModes::SWTRI_StencilTest;
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

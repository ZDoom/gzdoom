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
#include "poly_thread.h"
#include "screen_scanline_setup.h"
#include "x86.h"
#include <cmath>

static uint32_t SampleTexture(uint32_t u, uint32_t v, const void* texPixels, int texWidth, int texHeight, bool texBgra)
{
	int texelX = (u * texWidth) >> 16;
	int texelY = (v * texHeight) >> 16;
	int texelOffset = texelX + texelY * texWidth;
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

static void EffectFogBoundary(int x0, int x1, PolyTriangleThreadData* thread)
{
	uint32_t* fragcolor = thread->scanline.FragColor;
	for (int x = x0; x < x1; x++)
	{
		/*float fogdist = pixelpos.w;
		float fogfactor = exp2(uFogDensity * fogdist);
		FragColor = vec4(uFogColor.rgb, 1.0 - fogfactor);*/
		fragcolor[x] = 0;
	}
}

static void EffectBurn(int x0, int x1, PolyTriangleThreadData* thread)
{
	int texWidth = thread->textures[0].width;
	int texHeight = thread->textures[0].height;
	const void* texPixels = thread->textures[0].pixels;
	bool texBgra = thread->textures[0].bgra;

	int tex2Width = thread->textures[1].width;
	int tex2Height = thread->textures[1].height;
	const void* tex2Pixels = thread->textures[1].pixels;
	bool tex2Bgra = thread->textures[1].bgra;

	uint32_t* fragcolor = thread->scanline.FragColor;
	uint16_t* u = thread->scanline.U;
	uint16_t* v = thread->scanline.V;
	for (int x = x0; x < x1; x++)
	{
		uint32_t frag_r = thread->scanline.vColorR[x];
		uint32_t frag_g = thread->scanline.vColorG[x];
		uint32_t frag_b = thread->scanline.vColorB[x];
		uint32_t frag_a = thread->scanline.vColorA[x];
		frag_r += frag_r >> 7; // 255 -> 256
		frag_g += frag_g >> 7; // 255 -> 256
		frag_b += frag_b >> 7; // 255 -> 256
		frag_a += frag_a >> 7; // 255 -> 256

		uint32_t t1 = SampleTexture(u[x], v[x], texPixels, texWidth, texHeight, texBgra);
		uint32_t t2 = SampleTexture(u[x], 0xffff - v[x], tex2Pixels, tex2Width, tex2Height, tex2Bgra);

		uint32_t r = (frag_r * RPART(t1)) >> 8;
		uint32_t g = (frag_g * GPART(t1)) >> 8;
		uint32_t b = (frag_b * BPART(t1)) >> 8;
		uint32_t a = (frag_a * APART(t2)) >> 8;

		fragcolor[x] = MAKEARGB(a, r, g, b);
	}
}

static void EffectStencil(int x0, int x1, PolyTriangleThreadData* thread)
{
	/*for (int x = x0; x < x1; x++)
	{
		fragcolor[x] = 0x00ffffff;
	}*/
}

static void FuncPaletted(int x0, int x1, PolyTriangleThreadData* thread)
{
	int texWidth = thread->textures[0].width;
	int texHeight = thread->textures[0].height;
	const void* texPixels = thread->textures[0].pixels;
	bool texBgra = thread->textures[0].bgra;
	const uint32_t* lut = (const uint32_t*)thread->textures[1].pixels;
	uint32_t* fragcolor = thread->scanline.FragColor;
	uint16_t* u = thread->scanline.U;
	uint16_t* v = thread->scanline.V;

	for (int x = x0; x < x1; x++)
	{
		fragcolor[x] = lut[RPART(SampleTexture(u[x], v[x], texPixels, texWidth, texHeight, texBgra))] | 0xff000000;
	}
}

static void FuncNoTexture(int x0, int x1, PolyTriangleThreadData* thread)
{
	auto& streamdata = thread->mainVertexShader.Data;
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

	uint32_t* fragcolor = thread->scanline.FragColor;
	for (int x = x0; x < x1; x++)
	{
		fragcolor[x] = texel;
	}
}

static void FuncNormal(int x0, int x1, PolyTriangleThreadData* thread)
{
	int texWidth = thread->textures[0].width;
	int texHeight = thread->textures[0].height;
	const void* texPixels = thread->textures[0].pixels;
	bool texBgra = thread->textures[0].bgra;
	uint32_t* fragcolor = thread->scanline.FragColor;
	uint16_t* u = thread->scanline.U;
	uint16_t* v = thread->scanline.V;

	for (int x = x0; x < x1; x++)
	{
		uint32_t texel = SampleTexture(u[x], v[x], texPixels, texWidth, texHeight, texBgra);
		fragcolor[x] = texel;
	}
}

static void FuncNormal_Stencil(int x0, int x1, PolyTriangleThreadData* thread)
{
	int texWidth = thread->textures[0].width;
	int texHeight = thread->textures[0].height;
	const void* texPixels = thread->textures[0].pixels;
	bool texBgra = thread->textures[0].bgra;
	uint32_t* fragcolor = thread->scanline.FragColor;
	uint16_t* u = thread->scanline.U;
	uint16_t* v = thread->scanline.V;

	for (int x = x0; x < x1; x++)
	{
		uint32_t texel = SampleTexture(u[x], v[x], texPixels, texWidth, texHeight, texBgra);
		fragcolor[x] = texel | 0x00ffffff;
	}
}

static void FuncNormal_Opaque(int x0, int x1, PolyTriangleThreadData* thread)
{
	int texWidth = thread->textures[0].width;
	int texHeight = thread->textures[0].height;
	const void* texPixels = thread->textures[0].pixels;
	bool texBgra = thread->textures[0].bgra;
	uint32_t* fragcolor = thread->scanline.FragColor;
	uint16_t* u = thread->scanline.U;
	uint16_t* v = thread->scanline.V;

	for (int x = x0; x < x1; x++)
	{
		uint32_t texel = SampleTexture(u[x], v[x], texPixels, texWidth, texHeight, texBgra);
		fragcolor[x] = texel | 0xff000000;
	}
}

static void FuncNormal_Inverse(int x0, int x1, PolyTriangleThreadData* thread)
{
	int texWidth = thread->textures[0].width;
	int texHeight = thread->textures[0].height;
	const void* texPixels = thread->textures[0].pixels;
	bool texBgra = thread->textures[0].bgra;
	uint32_t* fragcolor = thread->scanline.FragColor;
	uint16_t* u = thread->scanline.U;
	uint16_t* v = thread->scanline.V;

	for (int x = x0; x < x1; x++)
	{
		uint32_t texel = SampleTexture(u[x], v[x], texPixels, texWidth, texHeight, texBgra);
		fragcolor[x] = MAKEARGB(APART(texel), 0xff - RPART(texel), 0xff - BPART(texel), 0xff - GPART(texel));
	}
}

static void FuncNormal_AlphaTexture(int x0, int x1, PolyTriangleThreadData* thread)
{
	int texWidth = thread->textures[0].width;
	int texHeight = thread->textures[0].height;
	const void* texPixels = thread->textures[0].pixels;
	bool texBgra = thread->textures[0].bgra;
	uint32_t* fragcolor = thread->scanline.FragColor;
	uint16_t* u = thread->scanline.U;
	uint16_t* v = thread->scanline.V;

	for (int x = x0; x < x1; x++)
	{
		uint32_t texel = SampleTexture(u[x], v[x], texPixels, texWidth, texHeight, texBgra);
		uint32_t gray = (RPART(texel) * 77 + GPART(texel) * 143 + BPART(texel) * 37) >> 8;
		uint32_t alpha = APART(texel);
		alpha += alpha >> 7;
		alpha = (alpha * gray + 127) >> 8;
		texel = (alpha << 24) | 0x00ffffff;
		fragcolor[x] = texel;
	}
}

static void FuncNormal_ClampY(int x0, int x1, PolyTriangleThreadData* thread)
{
	int texWidth = thread->textures[0].width;
	int texHeight = thread->textures[0].height;
	const void* texPixels = thread->textures[0].pixels;
	bool texBgra = thread->textures[0].bgra;
	uint32_t* fragcolor = thread->scanline.FragColor;
	uint16_t* u = thread->scanline.U;
	uint16_t* v = thread->scanline.V;

	for (int x = x0; x < x1; x++)
	{
		fragcolor[x] = SampleTexture(u[x], v[x], texPixels, texWidth, texHeight, texBgra);
		if (v[x] < 0.0 || v[x] > 1.0)
			fragcolor[x] &= 0x00ffffff;
	}
}

static void FuncNormal_InvertOpaque(int x0, int x1, PolyTriangleThreadData* thread)
{
	int texWidth = thread->textures[0].width;
	int texHeight = thread->textures[0].height;
	const void* texPixels = thread->textures[0].pixels;
	bool texBgra = thread->textures[0].bgra;
	uint32_t* fragcolor = thread->scanline.FragColor;
	uint16_t* u = thread->scanline.U;
	uint16_t* v = thread->scanline.V;

	for (int x = x0; x < x1; x++)
	{
		uint32_t texel = SampleTexture(u[x], v[x], texPixels, texWidth, texHeight, texBgra);
		fragcolor[x] = MAKEARGB(0xff, 0xff - RPART(texel), 0xff - BPART(texel), 0xff - GPART(texel));
	}
}

static void FuncNormal_AddColor(int x0, int x1, PolyTriangleThreadData* thread)
{
	auto& streamdata = thread->mainVertexShader.Data;
	uint32_t r = (int)(streamdata.uAddColor.r * 255.0f);
	uint32_t g = (int)(streamdata.uAddColor.g * 255.0f);
	uint32_t b = (int)(streamdata.uAddColor.b * 255.0f);
	uint32_t* fragcolor = thread->scanline.FragColor;
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

static void FuncNormal_AddObjectColor(int x0, int x1, PolyTriangleThreadData* thread)
{
	auto& streamdata = thread->mainVertexShader.Data;
	uint32_t r = (int)(streamdata.uObjectColor.r * 256.0f);
	uint32_t g = (int)(streamdata.uObjectColor.g * 256.0f);
	uint32_t b = (int)(streamdata.uObjectColor.b * 256.0f);
	uint32_t* fragcolor = thread->scanline.FragColor;
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

static void FuncNormal_AddObjectColor2(int x0, int x1, PolyTriangleThreadData* thread)
{
	auto& streamdata = thread->mainVertexShader.Data;
	float* gradientdistZ = thread->scanline.GradientdistZ;
	uint32_t* fragcolor = thread->scanline.FragColor;
	for (int x = x0; x < x1; x++)
	{
		float t = gradientdistZ[x];
		float inv_t = 1.0f - t;
		uint32_t r = (int)((streamdata.uObjectColor.r * inv_t + streamdata.uObjectColor2.r * t) * 256.0f);
		uint32_t g = (int)((streamdata.uObjectColor.g * inv_t + streamdata.uObjectColor2.g * t) * 256.0f);
		uint32_t b = (int)((streamdata.uObjectColor.b * inv_t + streamdata.uObjectColor2.b * t) * 256.0f);

		uint32_t texel = fragcolor[x];
		fragcolor[x] = MAKEARGB(
			APART(texel),
			MIN((r * RPART(texel)) >> 8, (uint32_t)255),
			MIN((g * GPART(texel)) >> 8, (uint32_t)255),
			MIN((b * BPART(texel)) >> 8, (uint32_t)255));
	}
}

static void FuncNormal_DesaturationFactor(int x0, int x1, PolyTriangleThreadData* thread)
{
	auto& streamdata = thread->mainVertexShader.Data;
	uint32_t* fragcolor = thread->scanline.FragColor;
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

static void RunAlphaTest(int x0, int x1, PolyTriangleThreadData* thread)
{
	uint32_t alphaThreshold = thread->AlphaThreshold;
	uint32_t* fragcolor = thread->scanline.FragColor;
	uint8_t* discard = thread->scanline.discard;
	for (int x = x0; x < x1; x++)
	{
		discard[x] = fragcolor[x] <= alphaThreshold;
	}
}

static void ApplyVertexColor(int x0, int x1, PolyTriangleThreadData* thread)
{
	uint32_t* fragcolor = thread->scanline.FragColor;
	for (int x = x0; x < x1; x++)
	{
		uint32_t r = thread->scanline.vColorR[x];
		uint32_t g = thread->scanline.vColorG[x];
		uint32_t b = thread->scanline.vColorB[x];
		uint32_t a = thread->scanline.vColorA[x];

		a += a >> 7;
		r += r >> 7;
		g += g >> 7;
		b += b >> 7;

		uint32_t texel = fragcolor[x];
		fragcolor[x] = MAKEARGB(
			(APART(texel) * a + 127) >> 8,
			(RPART(texel) * r + 127) >> 8,
			(GPART(texel) * g + 127) >> 8,
			(BPART(texel) * b + 127) >> 8);
	}
}

static void MainFP(int x0, int x1, PolyTriangleThreadData* thread)
{
	if (thread->EffectState == SHADER_Paletted) // func_paletted
	{
		FuncPaletted(x0, x1, thread);
	}
	else if (thread->EffectState == SHADER_NoTexture) // func_notexture
	{
		FuncNoTexture(x0, x1, thread);
	}
	else // func_normal
	{
		auto constants = thread->PushConstants;

		switch (constants->uTextureMode)
		{
		default:
		case TM_NORMAL:
		case TM_FOGLAYER:     FuncNormal(x0, x1, thread); break;
		case TM_STENCIL:      FuncNormal_Stencil(x0, x1, thread); break;
		case TM_OPAQUE:       FuncNormal_Opaque(x0, x1, thread); break;
		case TM_INVERSE:      FuncNormal_Inverse(x0, x1, thread); break;
		case TM_ALPHATEXTURE: FuncNormal_AlphaTexture(x0, x1, thread); break;
		case TM_CLAMPY:       FuncNormal_ClampY(x0, x1, thread); break;
		case TM_INVERTOPAQUE: FuncNormal_InvertOpaque(x0, x1, thread); break;
		}

		if (constants->uTextureMode != TM_FOGLAYER)
		{
			auto& streamdata = thread->mainVertexShader.Data;

			if (streamdata.uAddColor.r != 0.0f || streamdata.uAddColor.g != 0.0f || streamdata.uAddColor.b != 0.0f)
			{
				FuncNormal_AddColor(x0, x1, thread);
			}

			if (streamdata.uObjectColor2.a == 0.0f)
			{
				if (streamdata.uObjectColor.r != 1.0f || streamdata.uObjectColor.g != 1.0f || streamdata.uObjectColor.b != 1.0f)
				{
					FuncNormal_AddObjectColor(x0, x1, thread);
				}
			}
			else
			{
				FuncNormal_AddObjectColor2(x0, x1, thread);
			}

			if (streamdata.uDesaturationFactor > 0.0f)
			{
				FuncNormal_DesaturationFactor(x0, x1, thread);
			}
		}
	}

	if (thread->AlphaTest)
		RunAlphaTest(x0, x1, thread);

	ApplyVertexColor(x0, x1, thread);

	auto constants = thread->PushConstants;
	uint32_t* fragcolor = thread->scanline.FragColor;
	if (constants->uLightLevel >= 0.0f && thread->numPolyLights > 0)
	{
		uint16_t* lightarray = thread->scanline.lightarray;
		uint32_t* dynlights = thread->scanline.dynlights;
		for (int x = x0; x < x1; x++)
		{
			uint32_t fg = fragcolor[x];
			int lightshade = lightarray[x];
			uint32_t dynlight = dynlights[x];

			uint32_t a = APART(fg);
			uint32_t r = MIN((RPART(fg) * (lightshade + RPART(dynlight))) >> 8, (uint32_t)255);
			uint32_t g = MIN((GPART(fg) * (lightshade + GPART(dynlight))) >> 8, (uint32_t)255);
			uint32_t b = MIN((BPART(fg) * (lightshade + BPART(dynlight))) >> 8, (uint32_t)255);

			fragcolor[x] = MAKEARGB(a, r, g, b);
		}
	}
	else if (constants->uLightLevel >= 0.0f)
	{
		uint16_t* lightarray = thread->scanline.lightarray;
		for (int x = x0; x < x1; x++)
		{
			uint32_t fg = fragcolor[x];
			int lightshade = lightarray[x];

			uint32_t a = APART(fg);
			uint32_t r = (RPART(fg) * lightshade) >> 8;
			uint32_t g = (GPART(fg) * lightshade) >> 8;
			uint32_t b = (BPART(fg) * lightshade) >> 8;

			fragcolor[x] = MAKEARGB(a, r, g, b);
		}

		// To do: apply fog
	}
	else if (thread->numPolyLights > 0)
	{
		uint32_t* dynlights = thread->scanline.dynlights;
		for (int x = x0; x < x1; x++)
		{
			uint32_t fg = fragcolor[x];
			uint32_t dynlight = dynlights[x];

			uint32_t a = APART(fg);
			uint32_t r = MIN((RPART(fg) * RPART(dynlight)) >> 8, (uint32_t)255);
			uint32_t g = MIN((GPART(fg) * GPART(dynlight)) >> 8, (uint32_t)255);
			uint32_t b = MIN((BPART(fg) * BPART(dynlight)) >> 8, (uint32_t)255);

			fragcolor[x] = MAKEARGB(a, r, g, b);
		}
	}
}

void SelectFragmentShader(PolyTriangleThreadData* thread)
{
	void (*fragshader)(int x0, int x1, PolyTriangleThreadData * thread);

	if (thread->SpecialEffect == EFF_FOGBOUNDARY) // fogboundary.fp
	{
		fragshader = &EffectFogBoundary;
	}
	else if (thread->SpecialEffect == EFF_BURN) // burn.fp
	{
		fragshader = &EffectBurn;
	}
	else if (thread->SpecialEffect == EFF_STENCIL) // stencil.fp
	{
		fragshader = &EffectStencil;
	}
	else
	{
		fragshader = &MainFP;
	}

	thread->FragmentShader = fragshader;
}

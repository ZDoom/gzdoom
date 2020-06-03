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
	float uFogDensity = thread->PushConstants->uFogDensity;
	uint32_t fogcolor = MAKEARGB(
		0,
		static_cast<int>(clamp(thread->mainVertexShader.Data.uFogColor.r, 0.0f, 1.0f) * 255.0f),
		static_cast<int>(clamp(thread->mainVertexShader.Data.uFogColor.g, 0.0f, 1.0f) * 255.0f),
		static_cast<int>(clamp(thread->mainVertexShader.Data.uFogColor.b, 0.0f, 1.0f) * 255.0f));

	uint32_t* fragcolor = thread->scanline.FragColor;
	for (int x = x0; x < x1; x++)
	{
		float fogdist = thread->scanline.W[x];
		float fogfactor = std::exp2(uFogDensity * fogdist);

		// FragColor = vec4(uFogColor.rgb, 1.0 - fogfactor):
		uint32_t alpha = static_cast<int>(clamp(1.0f - (1.0f / 255.0f), 0.0f, 1.0f) * 255.0f);
		fragcolor[x] = fogcolor | alpha;
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

static void ProcessMaterial(int x0, int x1, PolyTriangleThreadData* thread)
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
}

static void GetLightColor(int x0, int x1, PolyTriangleThreadData* thread)
{
	uint32_t* fragcolor = thread->scanline.FragColor;
	uint32_t* lightarray = thread->scanline.lightarray;

	if (thread->PushConstants->uFogEnabled >= 0)
	{
		for (int x = x0; x < x1; x++)
		{
			uint32_t fg = fragcolor[x];
			uint32_t lightshade = lightarray[x];

			uint32_t mulA = APART(lightshade);
			uint32_t mulR = RPART(lightshade);
			uint32_t mulG = GPART(lightshade);
			uint32_t mulB = BPART(lightshade);
			mulA += mulA >> 7;
			mulR += mulR >> 7;
			mulG += mulG >> 7;
			mulB += mulB >> 7;

			uint32_t a = (APART(fg) * mulA + 127) >> 8;
			uint32_t r = (RPART(fg) * mulR + 127) >> 8;
			uint32_t g = (GPART(fg) * mulG + 127) >> 8;
			uint32_t b = (BPART(fg) * mulB + 127) >> 8;

			fragcolor[x] = MAKEARGB(a, r, g, b);
		}
	}
	else
	{
		uint32_t fogR = (int)((thread->mainVertexShader.Data.uFogColor.r) * 255.0f);
		uint32_t fogG = (int)((thread->mainVertexShader.Data.uFogColor.g) * 255.0f);
		uint32_t fogB = (int)((thread->mainVertexShader.Data.uFogColor.b) * 255.0f);
		float uFogDensity = thread->PushConstants->uFogDensity;
		
		float* w = thread->scanline.W;
		for (int x = x0; x < x1; x++)
		{
			uint32_t fg = fragcolor[x];
			uint32_t lightshade = lightarray[x];

			uint32_t mulA = APART(lightshade);
			uint32_t mulR = RPART(lightshade);
			uint32_t mulG = GPART(lightshade);
			uint32_t mulB = BPART(lightshade);
			mulA += mulA >> 7;
			mulR += mulR >> 7;
			mulG += mulG >> 7;
			mulB += mulB >> 7;

			float fogdist = MAX(16.0f, w[x]);
			float fogfactor = std::exp2(uFogDensity * fogdist);

			uint32_t a = (APART(fg) * mulA + 127) >> 8;
			uint32_t r = (RPART(fg) * mulR + 127) >> 8;
			uint32_t g = (GPART(fg) * mulG + 127) >> 8;
			uint32_t b = (BPART(fg) * mulB + 127) >> 8;

			uint32_t t = (int)(fogfactor * 256.0f);
			uint32_t inv_t = 256 - t;
			r = (fogR * inv_t + r * t + 127) >> 8;
			g = (fogG * inv_t + g * t + 127) >> 8;
			b = (fogB * inv_t + b * t + 127) >> 8;

			fragcolor[x] = MAKEARGB(a, r, g, b);
		}
	}
}

static void MainFP(int x0, int x1, PolyTriangleThreadData* thread)
{
	ProcessMaterial(x0, x1, thread);

	if (thread->AlphaTest)
		RunAlphaTest(x0, x1, thread);

	auto constants = thread->PushConstants;
	if (constants->uFogEnabled != -3)
	{
		if (constants->uTextureMode != TM_FOGLAYER)
		{
			GetLightColor(x0, x1, thread);
		}
		else
		{
			/*float fogdist = 0.0f;
			float fogfactor = 0.0f;
			if (constants->uFogEnabled != 0)
			{
				fogdist = MAX(16.0f, w[x]);
				fogfactor = std::exp2(constants->uFogDensity * fogdist);
			}
			frag = vec4(uFogColor.rgb, (1.0 - fogfactor) * frag.a * 0.75 * vColor.a);*/
		}
	}
	else // simple 2D (uses the fog color to add a color overlay)
	{
		uint32_t fogR = (int)((thread->mainVertexShader.Data.uFogColor.r) * 255.0f);
		uint32_t fogG = (int)((thread->mainVertexShader.Data.uFogColor.g) * 255.0f);
		uint32_t fogB = (int)((thread->mainVertexShader.Data.uFogColor.b) * 255.0f);

		auto vColorR = thread->scanline.vColorR;
		auto vColorG = thread->scanline.vColorG;
		auto vColorB = thread->scanline.vColorB;
		auto vColorA = thread->scanline.vColorA;
		uint32_t* fragcolor = thread->scanline.FragColor;

		if (constants->uTextureMode == TM_FIXEDCOLORMAP)
		{
			// float gray = grayscale(frag);
			// vec4 cm = (uObjectColor + gray * (uAddColor - uObjectColor)) * 2;
			// frag = vec4(clamp(cm.rgb, 0.0, 1.0), frag.a);
			// frag = frag * vColor;
			// frag.rgb = frag.rgb + uFogColor.rgb;

			uint32_t startR = (int)((thread->mainVertexShader.Data.uObjectColor.r) * 255.0f);
			uint32_t startG = (int)((thread->mainVertexShader.Data.uObjectColor.g) * 255.0f);
			uint32_t startB = (int)((thread->mainVertexShader.Data.uObjectColor.b) * 255.0f);
			uint32_t rangeR = (int)((thread->mainVertexShader.Data.uAddColor.r) * 255.0f) - startR;
			uint32_t rangeG = (int)((thread->mainVertexShader.Data.uAddColor.g) * 255.0f) - startG;
			uint32_t rangeB = (int)((thread->mainVertexShader.Data.uAddColor.b) * 255.0f) - startB;

			for (int x = x0; x < x1; x++)
			{
				uint32_t a = APART(fragcolor[x]);
				uint32_t r = RPART(fragcolor[x]);
				uint32_t g = GPART(fragcolor[x]);
				uint32_t b = BPART(fragcolor[x]);

				uint32_t gray = (r * 77 + g * 143 + b * 37) >> 8;
				gray += (gray >> 7); // gray*=256/255

				r = (startR + ((gray * rangeR) >> 8)) << 1;
				g = (startG + ((gray * rangeG) >> 8)) << 1;
				b = (startB + ((gray * rangeB) >> 8)) << 1;

				r = MIN(r, (uint32_t)255);
				g = MIN(g, (uint32_t)255);
				b = MIN(b, (uint32_t)255);

				fragcolor[x] = MAKEARGB(a, r, g, b);
			}
		}
		else
		{
			for (int x = x0; x < x1; x++)
			{
				uint32_t a = vColorA[x];
				uint32_t r = vColorR[x];
				uint32_t g = vColorG[x];
				uint32_t b = vColorB[x];
				a += a >> 7;
				r += r >> 7;
				g += g >> 7;
				b += b >> 7;

				// frag = frag * vColor;
				a = (APART(fragcolor[x]) * a + 127) >> 8;
				r = (RPART(fragcolor[x]) * r + 127) >> 8;
				g = (GPART(fragcolor[x]) * g + 127) >> 8;
				b = (BPART(fragcolor[x]) * b + 127) >> 8;

				// frag.rgb = frag.rgb + uFogColor.rgb;
				r = MIN(r + fogR, (uint32_t)255);
				g = MIN(g + fogG, (uint32_t)255);
				b = MIN(b + fogB, (uint32_t)255);

				fragcolor[x] = MAKEARGB(a, r, g, b);
			}
		}
	}
}

void ColormapFP(int x0, int x1, PolyTriangleThreadData* thread)
{
	// This is implemented in BlendColorColormap.
}

void SelectFragmentShader(PolyTriangleThreadData* thread)
{
	void (*fragshader)(int x0, int x1, PolyTriangleThreadData * thread);

	if (thread->ColormapShader)
	{
		fragshader = &ColormapFP;
	}
	else if (thread->SpecialEffect == EFF_FOGBOUNDARY) // fogboundary.fp
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

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

#pragma once

#include "screen_triangle.h"

namespace TriScreenDrawerModes
{
	namespace
	{
		struct BgraColor
		{
			uint32_t b, g, r, a;
			BgraColor() { }
			BgraColor(uint32_t c) : b(BPART(c)), g(GPART(c)), r(RPART(c)), a(APART(c)) { }
			BgraColor &operator=(uint32_t c) { b = BPART(c); g = GPART(c); r = RPART(c); a = APART(c); return *this; }
			operator uint32_t() const { return MAKEARGB(a, r, g, b); }
		};
	}

	template<typename SamplerT, typename FilterModeT>
	FORCEINLINE unsigned int Sample32(int32_t u, int32_t v, const uint32_t *texPixels, int texWidth, int texHeight, uint32_t oneU, uint32_t oneV, uint32_t color, const uint32_t *translation)
	{
		uint32_t texel;
		if (SamplerT::Mode == (int)Samplers::Shaded || SamplerT::Mode == (int)Samplers::Stencil || SamplerT::Mode == (int)Samplers::Fill || SamplerT::Mode == (int)Samplers::Fuzz || SamplerT::Mode == (int)Samplers::FogBoundary)
		{
			return color;
		}
		else if (SamplerT::Mode == (int)Samplers::Translated)
		{
			const uint8_t *texpal = (const uint8_t *)texPixels;
			uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
			uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
			return translation[texpal[texelX * texHeight + texelY]];
		}
		else if (FilterModeT::Mode == (int)FilterModes::Nearest)
		{
			uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
			uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
			texel = texPixels[texelX * texHeight + texelY];
		}
		else
		{
			u -= oneU >> 1;
			v -= oneV >> 1;

			unsigned int frac_x0 = (((uint32_t)u << 8) >> FRACBITS) * texWidth;
			unsigned int frac_x1 = ((((uint32_t)u << 8) + oneU) >> FRACBITS) * texWidth;
			unsigned int frac_y0 = (((uint32_t)v << 8) >> FRACBITS) * texHeight;
			unsigned int frac_y1 = ((((uint32_t)v << 8) + oneV) >> FRACBITS) * texHeight;
			unsigned int x0 = frac_x0 >> FRACBITS;
			unsigned int x1 = frac_x1 >> FRACBITS;
			unsigned int y0 = frac_y0 >> FRACBITS;
			unsigned int y1 = frac_y1 >> FRACBITS;

			unsigned int p00 = texPixels[x0 * texHeight + y0];
			unsigned int p01 = texPixels[x0 * texHeight + y1];
			unsigned int p10 = texPixels[x1 * texHeight + y0];
			unsigned int p11 = texPixels[x1 * texHeight + y1];

			unsigned int inv_a = (frac_x1 >> (FRACBITS - 4)) & 15;
			unsigned int inv_b = (frac_y1 >> (FRACBITS - 4)) & 15;
			unsigned int a = 16 - inv_a;
			unsigned int b = 16 - inv_b;

			unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
			unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
			unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
			unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

			texel = (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
		}

		if (SamplerT::Mode == (int)Samplers::Skycap)
		{
			int start_fade = 2; // How fast it should fade out

			int alpha_top = clamp(v >> (16 - start_fade), 0, 256);
			int alpha_bottom = clamp(((2 << 24) - v) >> (16 - start_fade), 0, 256);
			int a = MIN(alpha_top, alpha_bottom);
			int inv_a = 256 - a;

			uint32_t r = RPART(texel);
			uint32_t g = GPART(texel);
			uint32_t b = BPART(texel);
			uint32_t fg_a = APART(texel);
			uint32_t bg_red = RPART(color);
			uint32_t bg_green = GPART(color);
			uint32_t bg_blue = BPART(color);
			r = (r * a + bg_red * inv_a + 127) >> 8;
			g = (g * a + bg_green * inv_a + 127) >> 8;
			b = (b * a + bg_blue * inv_a + 127) >> 8;
			return MAKEARGB(fg_a, r, g, b);
		}
		else
		{
			return texel;
		}
	}

	template<typename SamplerT>
	FORCEINLINE unsigned int SampleShade32(int32_t u, int32_t v, const uint32_t *texPixels, int texWidth, int texHeight, int &fuzzpos)
	{
		if (SamplerT::Mode == (int)Samplers::Shaded)
		{
			const uint8_t *texpal = (const uint8_t *)texPixels;
			uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
			uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
			unsigned int sampleshadeout = texpal[texelX * texHeight + texelY];
			sampleshadeout += sampleshadeout >> 7; // 255 -> 256
			return sampleshadeout;
		}
		else if (SamplerT::Mode == (int)Samplers::Stencil)
		{
			uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
			uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
			unsigned int sampleshadeout = APART(texPixels[texelX * texHeight + texelY]);
			sampleshadeout += sampleshadeout >> 7; // 255 -> 256
			return sampleshadeout;
		}
		else if (SamplerT::Mode == (int)Samplers::Fuzz)
		{
			uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
			uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
			unsigned int sampleshadeout = APART(texPixels[texelX * texHeight + texelY]);
			sampleshadeout += sampleshadeout >> 7; // 255 -> 256
			sampleshadeout = (sampleshadeout * fuzzcolormap[fuzzpos++]) >> 5;
			if (fuzzpos >= FUZZTABLE) fuzzpos = 0;
			return sampleshadeout;
		}
		else
		{
			return 0;
		}
	}

	FORCEINLINE BgraColor VECTORCALL AddLights(BgraColor material, BgraColor fgcolor, BgraColor dynlight)
	{
		fgcolor.r = MIN(fgcolor.r + ((material.r * dynlight.r) >> 8), (uint32_t)255);
		fgcolor.g = MIN(fgcolor.g + ((material.g * dynlight.g) >> 8), (uint32_t)255);
		fgcolor.b = MIN(fgcolor.b + ((material.b * dynlight.b) >> 8), (uint32_t)255);
		return fgcolor;
	}

	FORCEINLINE BgraColor VECTORCALL CalcDynamicLight(const PolyLight *lights, int num_lights, FVector3 worldpos, FVector3 worldnormal, uint32_t dynlightcolor)
	{
		BgraColor lit = dynlightcolor;

		for (int i = 0; i != num_lights; i++)
		{
			FVector3 lightpos = { lights[i].x, lights[i].y, lights[i].z };
			float light_radius = lights[i].radius;

			bool is_attenuated = light_radius < 0.0f;
			if (is_attenuated)
				light_radius = -light_radius;

			// L = light-pos
			// dist = sqrt(dot(L, L))
			// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
			FVector3 L = lightpos - worldpos;
			float dist2 = L | L;
			float rcp_dist = 1.0f / sqrt(dist2);
			float dist = dist2 * rcp_dist;
			float distance_attenuation = 256.0f - MIN(dist * light_radius, 256.0f);

			// The simple light type
			float simple_attenuation = distance_attenuation;

			// The point light type
			// diffuse = max(dot(N,normalize(L)),0) * attenuation
			float dotNL = worldnormal | (L * rcp_dist);
			float point_attenuation = MAX(dotNL, 0.0f) * distance_attenuation;

			uint32_t attenuation = (uint32_t)(is_attenuated ? (int32_t)point_attenuation : (int32_t)simple_attenuation);

			BgraColor light_color = lights[i].color;
			lit.r += (light_color.r * attenuation) >> 8;
			lit.g += (light_color.g * attenuation) >> 8;
			lit.b += (light_color.b * attenuation) >> 8;
		}

		lit.r = MIN(lit.r, (uint32_t)256);
		lit.g = MIN(lit.g, (uint32_t)256);
		lit.b = MIN(lit.b, (uint32_t)256);
		return lit;
	}

	template<typename ShadeModeT>
	FORCEINLINE BgraColor Shade32(BgraColor fgcolor, BgraColor mlight, uint32_t desaturate, uint32_t inv_desaturate, BgraColor shade_fade, BgraColor shade_light, BgraColor dynlight)
	{
		BgraColor material = fgcolor;
		if (ShadeModeT::Mode == (int)ShadeMode::Simple)
		{
			fgcolor.r = (fgcolor.r * mlight.r) >> 8;
			fgcolor.g = (fgcolor.g * mlight.g) >> 8;
			fgcolor.b = (fgcolor.b * mlight.b) >> 8;
		}
		else if (ShadeModeT::Mode == (int)ShadeMode::Advanced)
		{
			uint32_t intensity = ((fgcolor.r * 77 + fgcolor.g * 143 + fgcolor.b * 37) >> 8) * desaturate;
			fgcolor.r = (((shade_fade.r + ((fgcolor.r * inv_desaturate + intensity) >> 8) * mlight.r) >> 8) * shade_light.r) >> 8;
			fgcolor.g = (((shade_fade.g + ((fgcolor.g * inv_desaturate + intensity) >> 8) * mlight.g) >> 8) * shade_light.g) >> 8;
			fgcolor.b = (((shade_fade.b + ((fgcolor.b * inv_desaturate + intensity) >> 8) * mlight.b) >> 8) * shade_light.b) >> 8;
		}
		return AddLights(material, fgcolor, dynlight);
	}

	template<typename BlendT>
	FORCEINLINE BgraColor Blend32(BgraColor fgcolor, BgraColor bgcolor, uint32_t ifgcolor, uint32_t ifgshade, uint32_t srcalpha, uint32_t destalpha)
	{
		if (BlendT::Mode == (int)BlendModes::Opaque)
		{
			return fgcolor;
		}
		else if (BlendT::Mode == (int)BlendModes::Masked)
		{
			return (ifgcolor == 0) ? bgcolor : fgcolor;
		}
		else if (BlendT::Mode == (int)BlendModes::AddSrcColorOneMinusSrcColor)
		{
			uint32_t srcred = fgcolor.r + (fgcolor.r >> 7);
			uint32_t srcgreen = fgcolor.g + (fgcolor.g >> 7);
			uint32_t srcblue = fgcolor.b + (fgcolor.b >> 7);
			uint32_t inv_srcred = 256 - srcred;
			uint32_t inv_srcgreen = 256 - srcgreen;
			uint32_t inv_srcblue = 256 - srcblue;

			BgraColor outcolor;
			outcolor.r = (fgcolor.r * srcred + bgcolor.r * inv_srcred) >> 8;
			outcolor.g = (fgcolor.g * srcgreen + bgcolor.g * inv_srcgreen) >> 8;
			outcolor.b = (fgcolor.b * srcblue + bgcolor.b * inv_srcblue) >> 8;
			outcolor.a = 255;
			return outcolor;
		}
		else if (BlendT::Mode == (int)BlendModes::Shaded)
		{
			uint32_t alpha = ifgshade;
			uint32_t inv_alpha = 256 - alpha;

			BgraColor outcolor;
			outcolor.r = (fgcolor.r * alpha + bgcolor.r * inv_alpha) >> 8;
			outcolor.g = (fgcolor.g * alpha + bgcolor.g * inv_alpha) >> 8;
			outcolor.b = (fgcolor.b * alpha + bgcolor.b * inv_alpha) >> 8;
			outcolor.a = 255;
			return outcolor;
		}
		else if (BlendT::Mode == (int)BlendModes::AddClampShaded)
		{
			uint32_t alpha = ifgshade;
			BgraColor outcolor;
			outcolor.r = ((fgcolor.r * alpha) >> 8) + bgcolor.r;
			outcolor.g = ((fgcolor.g * alpha) >> 8) + bgcolor.g;
			outcolor.b = ((fgcolor.b * alpha) >> 8) + bgcolor.b;
			outcolor.a = 255;
			return outcolor;
		}
		else
		{
			uint32_t alpha = APART(ifgcolor);
			alpha += alpha >> 7; // 255->256
			uint32_t inv_alpha = 256 - alpha;

			uint32_t bgalpha = (destalpha * alpha + (inv_alpha << 8) + 128) >> 8;
			uint32_t fgalpha = (srcalpha * alpha + 128) >> 8;

			fgcolor.r *= fgalpha;
			fgcolor.g *= fgalpha;
			fgcolor.b *= fgalpha;
			bgcolor.r *= bgalpha;
			bgcolor.g *= bgalpha;
			bgcolor.b *= bgalpha;

			BgraColor outcolor;
			if (BlendT::Mode == (int)BlendModes::AddClamp)
			{
				outcolor.r = MIN<uint32_t>((fgcolor.r + bgcolor.r) >> 8, 255);
				outcolor.g = MIN<uint32_t>((fgcolor.g + bgcolor.g) >> 8, 255);
				outcolor.b = MIN<uint32_t>((fgcolor.b + bgcolor.b) >> 8, 255);
			}
			else if (BlendT::Mode == (int)BlendModes::SubClamp)
			{
				outcolor.r = MAX(int32_t(fgcolor.r - bgcolor.r) >> 8, 0);
				outcolor.g = MAX(int32_t(fgcolor.g - bgcolor.g) >> 8, 0);
				outcolor.b = MAX(int32_t(fgcolor.b - bgcolor.b) >> 8, 0);
			}
			else if (BlendT::Mode == (int)BlendModes::RevSubClamp)
			{
				outcolor.r = MAX(int32_t(bgcolor.r - fgcolor.r) >> 8, 0);
				outcolor.g = MAX(int32_t(bgcolor.g - fgcolor.g) >> 8, 0);
				outcolor.b = MAX(int32_t(bgcolor.b - fgcolor.b) >> 8, 0);
			}
			outcolor.a = 255;
			return outcolor;
		}
	}
}

template<typename BlendT, typename SamplerT>
class TriScreenDrawer32
{
public:
	static void Execute(int x, int y, uint32_t mask0, uint32_t mask1, const TriDrawTriangleArgs *args)
	{
		using namespace TriScreenDrawerModes;

		bool is_simple_shade = args->uniforms->SimpleShade();

		if (SamplerT::Mode == (int)Samplers::Texture)
		{
			bool is_nearest_filter = args->uniforms->NearestFilter();

			if (is_simple_shade)
			{
				if (is_nearest_filter)
					DrawBlock<SimpleShade, NearestFilter>(x, y, mask0, mask1, args);
				else
					DrawBlock<SimpleShade, LinearFilter>(x, y, mask0, mask1, args);
			}
			else
			{
				if (is_nearest_filter)
					DrawBlock<AdvancedShade, NearestFilter>(x, y, mask0, mask1, args);
				else
					DrawBlock<AdvancedShade, LinearFilter>(x, y, mask0, mask1, args);
			}
		}
		else if (SamplerT::Mode == (int)Samplers::Fuzz)
		{
			DrawBlock<NoShade, NearestFilter>(x, y, mask0, mask1, args);
		}
		else // no linear filtering for translated, shaded, stencil, fill or skycap
		{
			if (is_simple_shade)
			{
				DrawBlock<SimpleShade, NearestFilter>(x, y, mask0, mask1, args);
			}
			else
			{
				DrawBlock<AdvancedShade, NearestFilter>(x, y, mask0, mask1, args);
			}
		}
	}

private:
	template<typename ShadeModeT, typename FilterModeT>
	FORCEINLINE static void DrawBlock(int destX, int destY, uint32_t mask0, uint32_t mask1, const TriDrawTriangleArgs *args)
	{
		using namespace TriScreenDrawerModes;

		bool is_fixed_light = args->uniforms->FixedLight();
		uint32_t lightmask = is_fixed_light ? 0 : 0xffffffff;
		uint32_t srcalpha = args->uniforms->SrcAlpha();
		uint32_t destalpha = args->uniforms->DestAlpha();

		int fuzzpos = (ScreenTriangle::FuzzStart + destX * 123 + destY) % FUZZTABLE;

		auto lights = args->uniforms->Lights();
		auto num_lights = args->uniforms->NumLights();
		FVector3 worldnormal = args->uniforms->Normal();
		uint32_t dynlightcolor = args->uniforms->DynLightColor();

		// Calculate gradients
		const ShadedTriVertex &v1 = *args->v1;
		ScreenTriangleStepVariables gradientX = args->gradientX;
		ScreenTriangleStepVariables gradientY = args->gradientY;
		ScreenTriangleStepVariables blockPosY;
		blockPosY.W = v1.w + gradientX.W * (destX - v1.x) + gradientY.W * (destY - v1.y);
		blockPosY.U = v1.u * v1.w + gradientX.U * (destX - v1.x) + gradientY.U * (destY - v1.y);
		blockPosY.V = v1.v * v1.w + gradientX.V * (destX - v1.x) + gradientY.V * (destY - v1.y);
		blockPosY.WorldX = v1.worldX * v1.w + gradientX.WorldX * (destX - v1.x) + gradientY.WorldX * (destY - v1.y);
		blockPosY.WorldY = v1.worldY * v1.w + gradientX.WorldY * (destX - v1.x) + gradientY.WorldY * (destY - v1.y);
		blockPosY.WorldZ = v1.worldZ * v1.w + gradientX.WorldZ * (destX - v1.x) + gradientY.WorldZ * (destY - v1.y);
		gradientX.W *= 8.0f;
		gradientX.U *= 8.0f;
		gradientX.V *= 8.0f;
		gradientX.WorldX *= 8.0f;
		gradientX.WorldY *= 8.0f;
		gradientX.WorldZ *= 8.0f;

		// Output
		uint32_t * RESTRICT destOrg = (uint32_t*)args->dest;
		int pitch = args->pitch;
		uint32_t *dest = destOrg + destX + destY * pitch;

		// Light
		uint32_t light = args->uniforms->Light();
		float shade = 2.0f - (light + 12.0f) / 128.0f;
		float globVis = args->uniforms->GlobVis() * (1.0f / 32.0f);
		light += (light >> 7); // 255 -> 256

		// Sampling stuff
		uint32_t color = args->uniforms->Color();
		const uint32_t * RESTRICT translation = (const uint32_t *)args->uniforms->Translation();
		const uint32_t * RESTRICT texPixels = (const uint32_t *)args->uniforms->TexturePixels();
		uint32_t texWidth = args->uniforms->TextureWidth();
		uint32_t texHeight = args->uniforms->TextureHeight();
		uint32_t oneU, oneV;
		if (SamplerT::Mode != (int)Samplers::Fill)
		{
			oneU = ((0x800000 + texWidth - 1) / texWidth) * 2 + 1;
			oneV = ((0x800000 + texHeight - 1) / texHeight) * 2 + 1;
		}
		else
		{
			oneU = 0;
			oneV = 0;
		}

		// Shade constants
		int inv_desaturate;
		BgraColor shade_fade, shade_light;
		int desaturate;
		if (ShadeModeT::Mode == (int)ShadeMode::Advanced)
		{
			shade_fade.r = args->uniforms->ShadeFadeRed();
			shade_fade.g = args->uniforms->ShadeFadeGreen();
			shade_fade.b = args->uniforms->ShadeFadeBlue();
			shade_light.r = args->uniforms->ShadeLightRed();
			shade_light.g = args->uniforms->ShadeLightGreen();
			shade_light.b = args->uniforms->ShadeLightBlue();
			desaturate = args->uniforms->ShadeDesaturate();
			inv_desaturate = 256 - desaturate;
		}
		else
		{
			inv_desaturate = 0;
			shade_fade.r = 0;
			shade_fade.g = 0;
			shade_fade.b = 0;
			shade_light.r = 0;
			shade_light.g = 0;
			shade_light.b = 0;
			desaturate = 0;
		}

		if (mask0 == 0xffffffff && mask1 == 0xffffffff)
		{
			for (int y = 0; y < 8; y++)
			{
				float rcpW = 0x01000000 / blockPosY.W;
				int32_t posU = (int32_t)(blockPosY.U * rcpW);
				int32_t posV = (int32_t)(blockPosY.V * rcpW);

				fixed_t lightpos = FRACUNIT - (int)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				lightpos = (lightpos & lightmask) | ((light << 8) & ~lightmask);

				FVector3 worldpos = FVector3(blockPosY.WorldX, blockPosY.WorldY, blockPosY.WorldZ) / blockPosY.W;
				BgraColor dynlight = CalcDynamicLight(lights, num_lights, worldpos, worldnormal, dynlightcolor);

				ScreenTriangleStepVariables blockPosX = blockPosY;
				blockPosX.W += gradientX.W;
				blockPosX.U += gradientX.U;
				blockPosX.V += gradientX.V;
				blockPosX.WorldX += gradientX.WorldX;
				blockPosX.WorldY += gradientX.WorldY;
				blockPosX.WorldZ += gradientX.WorldZ;

				rcpW = 0x01000000 / blockPosX.W;
				int32_t nextU = (int32_t)(blockPosX.U * rcpW);
				int32_t nextV = (int32_t)(blockPosX.V * rcpW);
				int32_t stepU = (nextU - posU) / 8;
				int32_t stepV = (nextV - posV) / 8;

				fixed_t lightnext = FRACUNIT - (fixed_t)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				fixed_t lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				worldpos = FVector3(blockPosX.WorldX, blockPosX.WorldY, blockPosX.WorldZ) / blockPosX.W;
				BgraColor dynlightnext = CalcDynamicLight(lights, num_lights, worldpos, worldnormal, dynlightcolor);
				BgraColor dynlightstep;
				dynlightstep.r = int32_t(dynlightnext.r - dynlight.r) >> 3;
				dynlightstep.g = int32_t(dynlightnext.g - dynlight.g) >> 3;
				dynlightstep.b = int32_t(dynlightnext.b - dynlight.b) >> 3;

				for (int ix = 0; ix < 8; ix++)
				{
					// Load bgcolor
					BgraColor bgcolor;
					if (BlendT::Mode != (int)BlendModes::Opaque)
						bgcolor = dest[ix];
					else
						bgcolor = 0;

					// Sample fgcolor
					if (SamplerT::Mode == (int)Samplers::FogBoundary) color = dest[ix];
					unsigned int ifgcolor = Sample32<SamplerT, FilterModeT>(posU, posV, texPixels, texWidth, texHeight, oneU, oneV, color, translation);
					unsigned int ifgshade = SampleShade32<SamplerT>(posU, posV, texPixels, texWidth, texHeight, fuzzpos);
					posU += stepU;
					posV += stepV;

					// Setup light
					int lightpos0 = lightpos >> 8;
					lightpos += lightstep;
					BgraColor mlight;
					mlight.r = lightpos0;
					mlight.g = lightpos0;
					mlight.b = lightpos0;

					BgraColor shade_fade_lit;
					if (ShadeModeT::Mode == (int)ShadeMode::Advanced)
					{
						uint32_t inv_light = 256 - lightpos0;
						shade_fade_lit.r = shade_fade.r * inv_light;
						shade_fade_lit.g = shade_fade.g * inv_light;
						shade_fade_lit.b = shade_fade.b * inv_light;
					}
					else
					{
						shade_fade_lit.r = 0;
						shade_fade_lit.g = 0;
						shade_fade_lit.b = 0;
					}

					// Shade and blend
					BgraColor fgcolor = Shade32<ShadeModeT>(ifgcolor, mlight, desaturate, inv_desaturate, shade_fade_lit, shade_light, dynlight);
					BgraColor outcolor = Blend32<BlendT>(fgcolor, bgcolor, ifgcolor, ifgshade, srcalpha, destalpha);

					// Store result
					dest[ix] = outcolor;

					dynlight.r = MAX<int32_t>(dynlight.r + dynlightstep.r, 0);
					dynlight.g = MAX<int32_t>(dynlight.g + dynlightstep.g, 0);
					dynlight.b = MAX<int32_t>(dynlight.b + dynlightstep.b, 0);
				}

				blockPosY.W += gradientY.W;
				blockPosY.U += gradientY.U;
				blockPosY.V += gradientY.V;
				blockPosY.WorldX += gradientY.WorldX;
				blockPosY.WorldY += gradientY.WorldY;
				blockPosY.WorldZ += gradientY.WorldZ;

				dest += pitch;
			}
		}
		else
		{
			// mask0 loop:
			for (int y = 0; y < 4; y++)
			{
				float rcpW = 0x01000000 / blockPosY.W;
				int32_t posU = (int32_t)(blockPosY.U * rcpW);
				int32_t posV = (int32_t)(blockPosY.V * rcpW);

				fixed_t lightpos = FRACUNIT - (fixed_t)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				lightpos = (lightpos & lightmask) | ((light << 8) & ~lightmask);

				FVector3 worldpos = FVector3(blockPosY.WorldX, blockPosY.WorldY, blockPosY.WorldZ) / blockPosY.W;
				BgraColor dynlight = CalcDynamicLight(lights, num_lights, worldpos, worldnormal, dynlightcolor);

				ScreenTriangleStepVariables blockPosX = blockPosY;
				blockPosX.W += gradientX.W;
				blockPosX.U += gradientX.U;
				blockPosX.V += gradientX.V;
				blockPosX.WorldX += gradientX.WorldX;
				blockPosX.WorldY += gradientX.WorldY;
				blockPosX.WorldZ += gradientX.WorldZ;

				rcpW = 0x01000000 / blockPosX.W;
				int32_t nextU = (int32_t)(blockPosX.U * rcpW);
				int32_t nextV = (int32_t)(blockPosX.V * rcpW);
				int32_t stepU = (nextU - posU) / 8;
				int32_t stepV = (nextV - posV) / 8;

				fixed_t lightnext = FRACUNIT - (fixed_t)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				fixed_t lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				worldpos = FVector3(blockPosX.WorldX, blockPosX.WorldY, blockPosX.WorldZ) / blockPosX.W;
				BgraColor dynlightnext = CalcDynamicLight(lights, num_lights, worldpos, worldnormal, dynlightcolor);
				BgraColor dynlightstep;
				dynlightstep.r = int32_t(dynlightnext.r - dynlight.r) >> 3;
				dynlightstep.g = int32_t(dynlightnext.g - dynlight.g) >> 3;
				dynlightstep.b = int32_t(dynlightnext.b - dynlight.b) >> 3;

				for (int x = 0; x < 8; x++)
				{
					// Load bgcolor
					BgraColor bgcolor;
					if (BlendT::Mode != (int)BlendModes::Opaque)
					{
						if (mask0 & (1 << 31)) bgcolor = dest[x];
					}
					else
						bgcolor = 0;

					// Sample fgcolor
					if (SamplerT::Mode == (int)Samplers::FogBoundary && (mask0 & (1 << 31))) color = dest[x];
					unsigned int ifgcolor = Sample32<SamplerT, FilterModeT>(posU, posV, texPixels, texWidth, texHeight, oneU, oneV, color, translation);
					unsigned int ifgshade = SampleShade32<SamplerT>(posU, posV, texPixels, texWidth, texHeight, fuzzpos);
					posU += stepU;
					posV += stepV;

					// Setup light
					int lightpos0 = lightpos >> 8;
					lightpos += lightstep;
					BgraColor mlight;
					mlight.r = lightpos0;
					mlight.g = lightpos0;
					mlight.b = lightpos0;

					BgraColor shade_fade_lit;
					if (ShadeModeT::Mode == (int)ShadeMode::Advanced)
					{
						uint32_t inv_light = 256 - lightpos0;
						shade_fade_lit.r = shade_fade.r * inv_light;
						shade_fade_lit.g = shade_fade.g * inv_light;
						shade_fade_lit.b = shade_fade.b * inv_light;
					}
					else
					{
						shade_fade_lit.r = 0;
						shade_fade_lit.g = 0;
						shade_fade_lit.b = 0;
					}

					// Shade and blend
					BgraColor fgcolor = Shade32<ShadeModeT>(ifgcolor, mlight, desaturate, inv_desaturate, shade_fade_lit, shade_light, dynlight);
					BgraColor outcolor = Blend32<BlendT>(fgcolor, bgcolor, ifgcolor, ifgshade, srcalpha, destalpha);

					// Store result
					if (mask0 & (1 << 31)) dest[x] = outcolor;

					mask0 <<= 1;

					dynlight.r = MAX<int32_t>(dynlight.r + dynlightstep.r, 0);
					dynlight.g = MAX<int32_t>(dynlight.g + dynlightstep.g, 0);
					dynlight.b = MAX<int32_t>(dynlight.b + dynlightstep.b, 0);
				}

				blockPosY.W += gradientY.W;
				blockPosY.U += gradientY.U;
				blockPosY.V += gradientY.V;
				blockPosY.WorldX += gradientY.WorldX;
				blockPosY.WorldY += gradientY.WorldY;
				blockPosY.WorldZ += gradientY.WorldZ;

				dest += pitch;
			}

			// mask1 loop:
			for (int y = 0; y < 4; y++)
			{
				float rcpW = 0x01000000 / blockPosY.W;
				int32_t posU = (int32_t)(blockPosY.U * rcpW);
				int32_t posV = (int32_t)(blockPosY.V * rcpW);

				fixed_t lightpos = FRACUNIT - (fixed_t)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosY.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				lightpos = (lightpos & lightmask) | ((light << 8) & ~lightmask);

				FVector3 worldpos = FVector3(blockPosY.WorldX, blockPosY.WorldY, blockPosY.WorldZ) / blockPosY.W;
				BgraColor dynlight = CalcDynamicLight(lights, num_lights, worldpos, worldnormal, dynlightcolor);

				ScreenTriangleStepVariables blockPosX = blockPosY;
				blockPosX.W += gradientX.W;
				blockPosX.U += gradientX.U;
				blockPosX.V += gradientX.V;
				blockPosX.WorldX += gradientX.WorldX;
				blockPosX.WorldY += gradientX.WorldY;
				blockPosX.WorldZ += gradientX.WorldZ;

				rcpW = 0x01000000 / blockPosX.W;
				int32_t nextU = (int32_t)(blockPosX.U * rcpW);
				int32_t nextV = (int32_t)(blockPosX.V * rcpW);
				int32_t stepU = (nextU - posU) / 8;
				int32_t stepV = (nextV - posV) / 8;

				fixed_t lightnext = FRACUNIT - (fixed_t)(clamp(shade - MIN(24.0f / 32.0f, globVis * blockPosX.W), 0.0f, 31.0f / 32.0f) * (float)FRACUNIT);
				fixed_t lightstep = (lightnext - lightpos) / 8;
				lightstep = lightstep & lightmask;

				worldpos = FVector3(blockPosX.WorldX, blockPosX.WorldY, blockPosX.WorldZ) / blockPosX.W;
				BgraColor dynlightnext = CalcDynamicLight(lights, num_lights, worldpos, worldnormal, dynlightcolor);
				BgraColor dynlightstep;
				dynlightstep.r = int32_t(dynlightnext.r - dynlight.r) >> 3;
				dynlightstep.g = int32_t(dynlightnext.g - dynlight.g) >> 3;
				dynlightstep.b = int32_t(dynlightnext.b - dynlight.b) >> 3;

				for (int x = 0; x < 8; x++)
				{
					// Load bgcolor
					BgraColor bgcolor;
					if (BlendT::Mode != (int)BlendModes::Opaque)
					{
						if (mask1 & (1 << 31)) bgcolor = dest[x];
					}
					else
						bgcolor = 0;

					// Sample fgcolor
					if (SamplerT::Mode == (int)Samplers::FogBoundary && (mask1 & (1 << 31))) color = dest[x];
					unsigned int ifgcolor = Sample32<SamplerT, FilterModeT>(posU, posV, texPixels, texWidth, texHeight, oneU, oneV, color, translation);
					unsigned int ifgshade = SampleShade32<SamplerT>(posU, posV, texPixels, texWidth, texHeight, fuzzpos);
					posU += stepU;
					posV += stepV;

					// Setup light
					int lightpos0 = lightpos >> 8;
					lightpos += lightstep;
					BgraColor mlight;
					mlight.r = lightpos0;
					mlight.g = lightpos0;
					mlight.b = lightpos0;

					BgraColor shade_fade_lit;
					if (ShadeModeT::Mode == (int)ShadeMode::Advanced)
					{
						uint32_t inv_light = 256 - lightpos0;
						shade_fade_lit.r = shade_fade.r * inv_light;
						shade_fade_lit.g = shade_fade.g * inv_light;
						shade_fade_lit.b = shade_fade.b * inv_light;
					}
					else
					{
						shade_fade_lit.r = 0;
						shade_fade_lit.g = 0;
						shade_fade_lit.b = 0;
					}

					// Shade and blend
					BgraColor fgcolor = Shade32<ShadeModeT>(ifgcolor, mlight, desaturate, inv_desaturate, shade_fade_lit, shade_light, dynlight);
					BgraColor outcolor = Blend32<BlendT>(fgcolor, bgcolor, ifgcolor, ifgshade, srcalpha, destalpha);

					// Store result
					if (mask1 & (1 << 31)) dest[x] = outcolor;

					mask1 <<= 1;

					dynlight.r = MAX<int32_t>(dynlight.r + dynlightstep.r, 0);
					dynlight.g = MAX<int32_t>(dynlight.g + dynlightstep.g, 0);
					dynlight.b = MAX<int32_t>(dynlight.b + dynlightstep.b, 0);
				}

				blockPosY.W += gradientY.W;
				blockPosY.U += gradientY.U;
				blockPosY.V += gradientY.V;
				blockPosY.WorldX += gradientY.WorldX;
				blockPosY.WorldY += gradientY.WorldY;
				blockPosY.WorldZ += gradientY.WorldZ;

				dest += pitch;
			}
		}
	}
};

template<typename BlendT, typename SamplerT>
class RectScreenDrawer32
{
public:
	static void Execute(const void *destOrg, int destWidth, int destHeight, int destPitch, const RectDrawArgs *args, WorkerThreadData *thread)
	{
		using namespace TriScreenDrawerModes;

		if (args->SimpleShade())
		{
			Loop<SimpleShade, NearestFilter>(destOrg, destWidth, destHeight, destPitch, args, thread);
		}
		else
		{
			Loop<AdvancedShade, NearestFilter>(destOrg, destWidth, destHeight, destPitch, args, thread);
		}
	}

private:
	template<typename ShadeModeT, typename FilterModeT>
	FORCEINLINE static void Loop(const void *destOrg, int destWidth, int destHeight, int destPitch, const RectDrawArgs *args, WorkerThreadData *thread)
	{
		using namespace TriScreenDrawerModes;

		int x0 = clamp((int)(args->X0() + 0.5f), 0, destWidth);
		int x1 = clamp((int)(args->X1() + 0.5f), 0, destWidth);
		int y0 = clamp((int)(args->Y0() + 0.5f), 0, destHeight);
		int y1 = clamp((int)(args->Y1() + 0.5f), 0, destHeight);

		if (x1 <= x0 || y1 <= y0)
			return;

		uint32_t srcalpha = args->SrcAlpha();
		uint32_t destalpha = args->DestAlpha();

		// Setup step variables
		float fstepU = (args->U1() - args->U0()) / (args->X1() - args->X0());
		float fstepV = (args->V1() - args->V0()) / (args->Y1() - args->Y0());
		uint32_t startU = (int32_t)((args->U0() + (x0 + 0.5f - args->X0()) * fstepU) * 0x1000000);
		uint32_t startV = (int32_t)((args->V0() + (y0 + 0.5f - args->Y0()) * fstepV) * 0x1000000);
		uint32_t stepU = (int32_t)(fstepU * 0x1000000);
		uint32_t stepV = (int32_t)(fstepV * 0x1000000);

		// Sampling stuff
		uint32_t color = args->Color();
		const uint32_t * RESTRICT translation = (const uint32_t *)args->Translation();
		const uint32_t * RESTRICT texPixels = (const uint32_t *)args->TexturePixels();
		uint32_t texWidth = args->TextureWidth();
		uint32_t texHeight = args->TextureHeight();
		uint32_t oneU, oneV;
		if (SamplerT::Mode != (int)Samplers::Fill)
		{
			oneU = ((0x800000 + texWidth - 1) / texWidth) * 2 + 1;
			oneV = ((0x800000 + texHeight - 1) / texHeight) * 2 + 1;
		}
		else
		{
			oneU = 0;
			oneV = 0;
		}

		// Setup light
		uint32_t lightpos = args->Light();
		lightpos += lightpos >> 7; // 255 -> 256
		BgraColor mlight;

		BgraColor dynlight = 0;

		// Shade constants
		int inv_desaturate;
		BgraColor shade_fade_lit, shade_light;
		int desaturate;
		if (ShadeModeT::Mode == (int)ShadeMode::Advanced)
		{
			uint32_t inv_light = 256 - lightpos;
			shade_fade_lit.r = args->ShadeFadeRed() * inv_light;
			shade_fade_lit.g = args->ShadeFadeGreen() * inv_light;
			shade_fade_lit.b = args->ShadeFadeBlue() * inv_light;
			shade_light.r = args->ShadeLightRed();
			shade_light.g = args->ShadeLightGreen();
			shade_light.b = args->ShadeLightBlue();
			desaturate = args->ShadeDesaturate();
			inv_desaturate = 256 - desaturate;
			mlight.r = lightpos;
			mlight.g = lightpos;
			mlight.b = lightpos;
		}
		else
		{
			inv_desaturate = 0;
			shade_fade_lit.r = 0;
			shade_fade_lit.g = 0;
			shade_fade_lit.b = 0;
			shade_light.r = 0;
			shade_light.g = 0;
			shade_light.b = 0;
			desaturate = 0;
			mlight.r = lightpos;
			mlight.g = lightpos;
			mlight.b = lightpos;
		}

		int count = x1 - x0;

		int fuzzpos = (ScreenTriangle::FuzzStart + x0 * 123 + y0) % FUZZTABLE;

		uint32_t posV = startV;
		for (int y = y0; y < y1; y++, posV += stepV)
		{
			int coreBlock = y / 8;
			if (coreBlock % thread->num_cores != thread->core)
			{
				fuzzpos = (fuzzpos + count) % FUZZTABLE;
				continue;
			}

			uint32_t *dest = ((uint32_t*)destOrg) + y * destPitch + x0;

			uint32_t posU = startU;
			for (int i = 0; i < count; i++)
			{
				// Load bgcolor
				BgraColor bgcolor;
				if (BlendT::Mode != (int)BlendModes::Opaque)
					bgcolor = *dest;
				else
					bgcolor = 0;

				// Sample fgcolor
				if (SamplerT::Mode == (int)Samplers::FogBoundary) color = *dest;
				unsigned int ifgcolor = Sample32<SamplerT, FilterModeT>(posU, posV, texPixels, texWidth, texHeight, oneU, oneV, color, translation);
				unsigned int ifgshade = SampleShade32<SamplerT>(posU, posV, texPixels, texWidth, texHeight, fuzzpos);
				posU += stepU;

				// Shade and blend
				BgraColor fgcolor = Shade32<ShadeModeT>(ifgcolor, mlight, desaturate, inv_desaturate, shade_fade_lit, shade_light, dynlight);
				BgraColor outcolor = Blend32<BlendT>(fgcolor, bgcolor, ifgcolor, ifgshade, srcalpha, destalpha);

				// Store result
				*dest = outcolor;
				dest++;
			}
		}
	}
};

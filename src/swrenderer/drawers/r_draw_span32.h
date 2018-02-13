/*
**  Drawer commands for spans
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

#include "swrenderer/drawers/r_draw_rgba.h"
#include "swrenderer/viewport/r_spandrawer.h"

namespace swrenderer
{
	namespace DrawSpan32TModes
	{
		enum class SpanBlendModes { Opaque, Masked, Translucent, AddClamp, SubClamp, RevSubClamp };
		struct OpaqueSpan { static const int Mode = (int)SpanBlendModes::Opaque; };
		struct MaskedSpan { static const int Mode = (int)SpanBlendModes::Masked; };
		struct TranslucentSpan { static const int Mode = (int)SpanBlendModes::Translucent; };
		struct AddClampSpan { static const int Mode = (int)SpanBlendModes::AddClamp; };
		struct SubClampSpan { static const int Mode = (int)SpanBlendModes::SubClamp; };
		struct RevSubClampSpan { static const int Mode = (int)SpanBlendModes::RevSubClamp; };

		enum class FilterModes { Nearest, Linear };
		struct NearestFilter { static const int Mode = (int)FilterModes::Nearest; };
		struct LinearFilter { static const int Mode = (int)FilterModes::Linear; };

		enum class ShadeMode { Simple, Advanced };
		struct SimpleShade { static const int Mode = (int)ShadeMode::Simple; };
		struct AdvancedShade { static const int Mode = (int)ShadeMode::Advanced; };

		enum class SpanTextureSize { SizeAny, Size64x64 };
		struct TextureSizeAny { static const int Mode = (int)SpanTextureSize::SizeAny; };
		struct TextureSize64x64 { static const int Mode = (int)SpanTextureSize::Size64x64; };
	}

	template<typename BlendT>
	class DrawSpan32T : public DrawerCommand
	{
	protected:
		SpanDrawerArgs args;

	public:
		DrawSpan32T(const SpanDrawerArgs &drawerargs) : args(drawerargs) { }

		struct TextureData
		{
			uint32_t width;
			uint32_t height;
			uint32_t xone;
			uint32_t yone;
			uint32_t xstep;
			uint32_t ystep;
			uint32_t xfrac;
			uint32_t yfrac;
			const uint32_t *source;
		};

		void Execute(DrawerThread *thread) override
		{
			using namespace DrawSpan32TModes;

			if (thread->line_skipped_by_thread(args.DestY())) return;
			
			TextureData texdata;
			texdata.width = args.TextureWidth();
			texdata.height = args.TextureHeight();
			texdata.xstep = args.TextureUStep();
			texdata.ystep = args.TextureVStep();
			texdata.xfrac = args.TextureUPos();
			texdata.yfrac = args.TextureVPos();
			
			texdata.source = (const uint32_t*)args.TexturePixels();
			
			double lod = args.TextureLOD();
			bool mipmapped = args.MipmappedTexture();
			
			bool magnifying = lod < 0.0;
			if (r_mipmap && mipmapped)
			{
				int level = (int)lod;
				while (level > 0)
				{
					if (texdata.width <= 2 || texdata.height <= 2)
						break;

					texdata.source += texdata.width * texdata.height;
					texdata.width = MAX<uint32_t>(texdata.width / 2, 1);
					texdata.height = MAX<uint32_t>(texdata.height / 2, 1);
					level--;
				}
			}

			texdata.xone = (0x80000000u / texdata.width) << 1;
			texdata.yone = (0x80000000u / texdata.height) << 1;

			bool is_nearest_filter = (magnifying && !r_magfilter) || (!magnifying && !r_minfilter);
			bool is_64x64 = texdata.width == 64 && texdata.height == 64;
			
			auto shade_constants = args.ColormapConstants();
			if (shade_constants.simple_shade)
			{
				if (is_nearest_filter)
				{
					if (is_64x64)
						Loop<SimpleShade, NearestFilter, TextureSize64x64>(thread, texdata, shade_constants);
					else
						Loop<SimpleShade, NearestFilter, TextureSizeAny>(thread, texdata, shade_constants);
				}
				else
				{
					if (is_64x64)
						Loop<SimpleShade, LinearFilter, TextureSize64x64>(thread, texdata, shade_constants);
					else
						Loop<SimpleShade, LinearFilter, TextureSizeAny>(thread, texdata, shade_constants);
				}
			}
			else
			{
				if (is_nearest_filter)
				{
					if (is_64x64)
						Loop<AdvancedShade, NearestFilter, TextureSize64x64>(thread, texdata, shade_constants);
					else
						Loop<AdvancedShade, NearestFilter, TextureSizeAny>(thread, texdata, shade_constants);
				}
				else
				{
					if (is_64x64)
						Loop<AdvancedShade, LinearFilter, TextureSize64x64>(thread, texdata, shade_constants);
					else
						Loop<AdvancedShade, LinearFilter, TextureSizeAny>(thread, texdata, shade_constants);
				}
			}
		}

		template<typename ShadeModeT, typename FilterModeT, typename TextureSizeT>
		FORCEINLINE void Loop(DrawerThread *thread, TextureData texdata, ShadeConstants shade_constants)
		{
			using namespace DrawSpan32TModes;

			// Shade constants
			uint32_t light = 256 - (args.Light() >> (FRACBITS - 8));
			uint32_t inv_light = 256 - light;

			int inv_desaturate;
			BgraColor shade_fade, shade_light;
			int desaturate;
			if (ShadeModeT::Mode == (int)ShadeMode::Advanced)
			{
				inv_desaturate = 256 - shade_constants.desaturate;
				shade_fade.r = shade_constants.fade_red * inv_light;
				shade_fade.g = shade_constants.fade_green * inv_light;
				shade_fade.b = shade_constants.fade_blue * inv_light;
				shade_light.r = shade_constants.light_red;
				shade_light.g = shade_constants.light_green;
				shade_light.b = shade_constants.light_blue;
				desaturate = shade_constants.desaturate;
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

			auto lights = args.dc_lights;
			auto num_lights = args.dc_num_lights;
			float viewpos_x = args.dc_viewpos.X;
			float step_viewpos_x = args.dc_viewpos_step.X;

			int count = args.DestX2() - args.DestX1() + 1;
			int pitch = args.Viewport()->RenderTarget->GetPitch();
			uint32_t *dest = (uint32_t*)args.Viewport()->GetDest(args.DestX1(), args.DestY());

			if (FilterModeT::Mode == (int)FilterModes::Linear)
			{
				texdata.xfrac -= texdata.xone / 2;
				texdata.yfrac -= texdata.yone / 2;
			}

			uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
			uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);

			for (int index = 0; index < count; index++)
			{
				BgraColor bgcolor;
				if (BlendT::Mode != (int)SpanBlendModes::Opaque)
				{
					bgcolor = *dest;
				}
				else
				{
					bgcolor = 0;
				}

				uint32_t ifgcolor = Sample<FilterModeT, TextureSizeT>(texdata.width, texdata.height, texdata.xone, texdata.yone, texdata.xstep, texdata.ystep, texdata.xfrac, texdata.yfrac, texdata.source);
				BgraColor fgcolor = Shade<ShadeModeT>(ifgcolor, light, desaturate, inv_desaturate, shade_fade, shade_light, lights, num_lights, viewpos_x);
				BgraColor outcolor = Blend(fgcolor, bgcolor, srcalpha, destalpha, ifgcolor);

				*dest = outcolor;
				dest++;
				texdata.xfrac += texdata.xstep;
				texdata.yfrac += texdata.ystep;
				viewpos_x += step_viewpos_x;
			}

		}

		template<typename FilterModeT, typename TextureSizeT>
		FORCEINLINE uint32_t Sample(uint32_t width, uint32_t height, uint32_t xone, uint32_t yone, uint32_t xstep, uint32_t ystep, uint32_t xfrac, uint32_t yfrac, const uint32_t *source)
		{
			using namespace DrawSpan32TModes;

			if (FilterModeT::Mode == (int)FilterModes::Nearest && TextureSizeT::Mode == (int)SpanTextureSize::Size64x64)
			{
				int sample_index = ((xfrac >> (32 - 6 - 6)) & (63 * 64)) + (yfrac >> (32 - 6));
				return source[sample_index];
			}
			else if (FilterModeT::Mode == (int)FilterModes::Nearest)
			{
				uint32_t x = ((xfrac >> 16) * width) >> 16;
				uint32_t y = ((yfrac >> 16) * height) >> 16;
				int sample_index = x * height + y;
				return source[sample_index];
			}
			else
			{
				uint32_t p00, p01, p10, p11;
				uint32_t frac_x, frac_y;
				if (TextureSizeT::Mode == (int)SpanTextureSize::Size64x64)
				{
					frac_x = xfrac >> 16 << 6;
					frac_y = yfrac >> 16 << 6;
					uint32_t x0 = frac_x >> 16;
					uint32_t y0 = frac_y >> 16;
					uint32_t x1 = (x0 + 1) & 0x3f;
					uint32_t y1 = (y0 + 1) & 0x3f;
					p00 = source[(y0 + (x0 << 6))];
					p01 = source[(y1 + (x0 << 6))];
					p10 = source[(y0 + (x1 << 6))];
					p11 = source[(y1 + (x1 << 6))];
				}
				else
				{
					frac_x = (xfrac >> 16) * width;
					frac_y = (yfrac >> 16) * height;
					uint32_t x0 = frac_x >> 16;
					uint32_t y0 = frac_y >> 16;
					uint32_t x1 = (((xfrac + xone) >> 16) * width) >> 16;
					uint32_t y1 = (((yfrac + yone) >> 16) * height) >> 16;
					p00 = source[y0 + x0 * height];
					p01 = source[y1 + x0 * height];
					p10 = source[y0 + x1 * height];
					p11 = source[y1 + x1 * height];
				}

				uint32_t inv_b = (frac_x >> 12) & 15;
				uint32_t inv_a = (frac_y >> 12) & 15;
				uint32_t a = 16 - inv_a;
				uint32_t b = 16 - inv_b;

				uint32_t sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
				uint32_t sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
				uint32_t sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
				uint32_t salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

				return (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
			}
		}

		template<typename ShadeModeT>
		FORCEINLINE BgraColor Shade(BgraColor fgcolor, uint32_t light, uint32_t desaturate, uint32_t inv_desaturate, BgraColor shade_fade, BgraColor shade_light, const DrawerLight *lights, int num_lights, float viewpos_x)
		{
			using namespace DrawSpan32TModes;

			BgraColor material = fgcolor;
			if (ShadeModeT::Mode == (int)ShadeMode::Simple)
			{
				fgcolor.r = (fgcolor.r * light) >> 8;
				fgcolor.g = (fgcolor.g * light) >> 8;
				fgcolor.b = (fgcolor.b * light) >> 8;
			}
			else
			{
				uint32_t intensity = ((fgcolor.r * 77 + fgcolor.g * 143 + fgcolor.b * 37) >> 8) * desaturate;
				fgcolor.r = (((shade_fade.r + ((fgcolor.r * inv_desaturate + intensity) >> 8) * light) >> 8) * shade_light.r) >> 8;
				fgcolor.g = (((shade_fade.g + ((fgcolor.g * inv_desaturate + intensity) >> 8) * light) >> 8) * shade_light.g) >> 8;
				fgcolor.b = (((shade_fade.b + ((fgcolor.b * inv_desaturate + intensity) >> 8) * light) >> 8) * shade_light.b) >> 8;
			}

			return AddLights(material, fgcolor, lights, num_lights, viewpos_x);
		}

		FORCEINLINE BgraColor AddLights(BgraColor material, BgraColor fgcolor, const DrawerLight *lights, int num_lights, float viewpos_x)
		{
			using namespace DrawSpan32TModes;

			BgraColor lit;
			lit.r = 0;
			lit.g = 0;
			lit.b = 0;

			for (int i = 0; i != num_lights; i++)
			{
				float light_x = lights[i].x;
				float light_y = lights[i].y;
				float light_z = lights[i].z;
				float light_radius = lights[i].radius;

				// L = light-pos
				// dist = sqrt(dot(L, L))
				// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
				float Lyz2 = light_y; // L.y*L.y + L.z*L.z
				float Lx = light_x - viewpos_x;
				float dist2 = Lyz2 + Lx * Lx;
				float rcp_dist = 1.f/sqrt(dist2);
				float dist = dist2 * rcp_dist;
				float distance_attenuation = 256.0f - MIN(dist * light_radius, 256.0f);

				// The simple light type
				float simple_attenuation = distance_attenuation;

				// The point light type
				// diffuse = dot(N,L) * attenuation
				float point_attenuation = light_z * rcp_dist * distance_attenuation;

				uint32_t attenuation = (int32_t)((light_z == 0.0f) ? simple_attenuation : point_attenuation);

				BgraColor light_color = lights[i].color;

				lit.r += (light_color.r * attenuation) >> 8;
				lit.g += (light_color.g * attenuation) >> 8;
				lit.b += (light_color.b * attenuation) >> 8;
			}

			lit.r = MIN<uint32_t>(lit.r, 256);
			lit.g = MIN<uint32_t>(lit.g, 256);
			lit.b = MIN<uint32_t>(lit.b, 256);

			fgcolor.r = MIN<uint32_t>(fgcolor.r + ((material.r * lit.r) >> 8), 255);
			fgcolor.g = MIN<uint32_t>(fgcolor.g + ((material.g * lit.g) >> 8), 255);
			fgcolor.b = MIN<uint32_t>(fgcolor.b + ((material.b * lit.b) >> 8), 255);
			return fgcolor;
		}

		FORCEINLINE BgraColor Blend(BgraColor fgcolor, BgraColor bgcolor, uint32_t srcalpha, uint32_t destalpha, unsigned int ifgcolor)
		{
			using namespace DrawSpan32TModes;

			if (BlendT::Mode == (int)SpanBlendModes::Opaque)
			{
				return fgcolor;
			}
			else if (BlendT::Mode == (int)SpanBlendModes::Masked)
			{
				return (ifgcolor == 0) ? bgcolor : fgcolor;
			}
			else if (BlendT::Mode == (int)SpanBlendModes::Translucent)
			{
				fgcolor.r = fgcolor.r * srcalpha;
				fgcolor.g = fgcolor.g * srcalpha;
				fgcolor.b = fgcolor.b * srcalpha;
				bgcolor.r = bgcolor.r * destalpha;
				bgcolor.g = bgcolor.g * destalpha;
				bgcolor.b = bgcolor.b * destalpha;

				BgraColor outcolor;
				outcolor.r = MIN<uint32_t>((fgcolor.r + bgcolor.r) >> 8, 255);
				outcolor.g = MIN<uint32_t>((fgcolor.g + bgcolor.g) >> 8, 255);
				outcolor.b = MIN<uint32_t>((fgcolor.b + bgcolor.b) >> 8, 255);
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
				if (BlendT::Mode == (int)SpanBlendModes::AddClamp)
				{
					outcolor.r = MIN<uint32_t>((fgcolor.r + bgcolor.r) >> 8, 255);
					outcolor.g = MIN<uint32_t>((fgcolor.g + bgcolor.g) >> 8, 255);
					outcolor.b = MIN<uint32_t>((fgcolor.b + bgcolor.b) >> 8, 255);
				}
				else if (BlendT::Mode == (int)SpanBlendModes::SubClamp)
				{
					outcolor.r = MAX(int32_t(fgcolor.r - bgcolor.r) >> 8, 0);
					outcolor.g = MAX(int32_t(fgcolor.g - bgcolor.g) >> 8, 0);
					outcolor.b = MAX(int32_t(fgcolor.b - bgcolor.b) >> 8, 0);
				}
				else if (BlendT::Mode == (int)SpanBlendModes::RevSubClamp)
				{
					outcolor.r = MAX(int32_t(bgcolor.r - fgcolor.r) >> 8, 0);
					outcolor.g = MAX(int32_t(bgcolor.g - fgcolor.g) >> 8, 0);
					outcolor.b = MAX(int32_t(bgcolor.b - fgcolor.b) >> 8, 0);
				}
				outcolor.a = 255;
				return outcolor;
			}
		}

		FString DebugInfo() override { return "DrawSpan32T"; }
	};

	typedef DrawSpan32T<DrawSpan32TModes::OpaqueSpan> DrawSpan32Command;
	typedef DrawSpan32T<DrawSpan32TModes::MaskedSpan> DrawSpanMasked32Command;
	typedef DrawSpan32T<DrawSpan32TModes::TranslucentSpan> DrawSpanTranslucent32Command;
	typedef DrawSpan32T<DrawSpan32TModes::AddClampSpan> DrawSpanAddClamp32Command;
	typedef DrawSpan32T<DrawSpan32TModes::SubClampSpan> DrawSpanSubClamp32Command;
	typedef DrawSpan32T<DrawSpan32TModes::RevSubClampSpan> DrawSpanRevSubClamp32Command;
}

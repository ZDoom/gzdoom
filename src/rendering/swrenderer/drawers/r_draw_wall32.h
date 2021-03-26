/*
**  Drawer commands for walls
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

#include "swrenderer/drawers/r_draw_pal.h"
#include "swrenderer/drawers/r_draw_rgba.h"
#include "swrenderer/viewport/r_walldrawer.h"

namespace swrenderer
{
	namespace DrawWall32TModes
	{
		enum class WallBlendModes { Opaque, Masked, AddClamp, SubClamp, RevSubClamp };
		struct OpaqueWall { static const int Mode = (int)WallBlendModes::Opaque; };
		struct MaskedWall { static const int Mode = (int)WallBlendModes::Masked; };
		struct AddClampWall { static const int Mode = (int)WallBlendModes::AddClamp; };
		struct SubClampWall { static const int Mode = (int)WallBlendModes::SubClamp; };
		struct RevSubClampWall { static const int Mode = (int)WallBlendModes::RevSubClamp; };

		enum class FilterModes { Nearest, Linear };
		struct NearestFilter { static const int Mode = (int)FilterModes::Nearest; };
		struct LinearFilter { static const int Mode = (int)FilterModes::Linear; };

		enum class ShadeMode { Simple, Advanced };
		struct SimpleShade { static const int Mode = (int)ShadeMode::Simple; };
		struct AdvancedShade { static const int Mode = (int)ShadeMode::Advanced; };
	}

	template<typename BlendT>
	class DrawWall32T : public DrawWallCommand
	{
	public:
		DrawWall32T(const WallDrawerArgs &drawerargs) : DrawWallCommand(drawerargs) { }

		void DrawColumn(DrawerThread *thread, const WallColumnDrawerArgs& args) override
		{
			using namespace DrawWall32TModes;

			const uint32_t *source2 = (const uint32_t*)args.TexturePixels2();
			bool is_nearest_filter = (source2 == nullptr);
			auto shade_constants = args.ColormapConstants();
			if (shade_constants.simple_shade)
			{
				if (is_nearest_filter)
					Loop<SimpleShade, NearestFilter>(thread, args, shade_constants);
				else
					Loop<SimpleShade, LinearFilter>(thread, args, shade_constants);
			}
			else
			{
				if (is_nearest_filter)
					Loop<AdvancedShade, NearestFilter>(thread, args, shade_constants);
				else
					Loop<AdvancedShade, LinearFilter>(thread, args, shade_constants);
			}
		}

		template<typename ShadeModeT, typename FilterModeT>
		FORCEINLINE void Loop(DrawerThread *thread, const WallColumnDrawerArgs& args, ShadeConstants shade_constants)
		{
			using namespace DrawWall32TModes;

			const uint32_t *source = (const uint32_t*)args.TexturePixels();
			const uint32_t *source2 = (const uint32_t*)args.TexturePixels2();
			int textureheight = args.TextureHeight();
			uint32_t one = ((0x80000000 + textureheight - 1) / textureheight) * 2 + 1;

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

			int count = args.Count();
			int pitch = args.Viewport()->RenderTarget->GetPitch();
			uint32_t fracstep = args.TextureVStep();
			uint32_t frac = args.TextureVPos();
			uint32_t texturefracx = args.TextureUPos();
			uint32_t *dest = (uint32_t*)args.Dest();
			int dest_y = args.DestY();

			auto lights = args.dc_lights;
			auto num_lights = args.dc_num_lights;
			float viewpos_z = args.dc_viewpos.Z + args.dc_viewpos_step.Z * thread->skipped_by_thread(dest_y);
			float step_viewpos_z = args.dc_viewpos_step.Z * thread->num_cores;

			count = thread->count_for_thread(dest_y, count);
			if (count <= 0) return;
			frac += thread->skipped_by_thread(dest_y) * fracstep;
			dest = thread->dest_for_thread(dest_y, pitch, dest);
			fracstep *= thread->num_cores;
			pitch *= thread->num_cores;

			if (FilterModeT::Mode == (int)FilterModes::Linear)
			{
				frac -= one / 2;
			}

			uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
			uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);

			for (int index = 0; index < count; index++)
			{
				BgraColor bgcolor;
				if (BlendT::Mode != (int)WallBlendModes::Opaque)
				{
					bgcolor = *dest;
				}
				else
				{
					bgcolor = 0;
				}

				uint32_t ifgcolor = Sample<FilterModeT>(frac, source, source2, textureheight, one, texturefracx);
				BgraColor fgcolor = Shade<ShadeModeT>(ifgcolor, light, desaturate, inv_desaturate, shade_fade, shade_light, lights, num_lights, viewpos_z);
				BgraColor outcolor = Blend(fgcolor, bgcolor, ifgcolor, srcalpha, destalpha);

				*dest = outcolor;
				dest += pitch;
				frac += fracstep;
				viewpos_z += step_viewpos_z;
			}
		}

		template<typename FilterModeT>
		FORCEINLINE BgraColor Sample(uint32_t frac, const uint32_t *source, const uint32_t *source2, int textureheight, uint32_t one, uint32_t texturefracx)
		{
			using namespace DrawWall32TModes;

			if (FilterModeT::Mode == (int)FilterModes::Nearest)
			{
				int sample_index = ((frac >> FRACBITS) * textureheight) >> FRACBITS;
				return source[sample_index];
			}
			else
			{
				unsigned int frac_y0 = (frac >> FRACBITS) * textureheight;
				unsigned int frac_y1 = ((frac + one) >> FRACBITS) * textureheight;
				unsigned int y0 = frac_y0 >> FRACBITS;
				unsigned int y1 = frac_y1 >> FRACBITS;

				unsigned int p00 = source[y0];
				unsigned int p01 = source[y1];
				unsigned int p10 = source2[y0];
				unsigned int p11 = source2[y1];

				unsigned int inv_b = texturefracx;
				unsigned int inv_a = (frac_y1 >> (FRACBITS - 4)) & 15;
				unsigned int a = 16 - inv_a;
				unsigned int b = 16 - inv_b;

				BgraColor result;
				result.r = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
				result.g = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
				result.b = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
				result.a = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;
				return result;
			}
		}

		template<typename ShadeModeT>
		FORCEINLINE BgraColor Shade(BgraColor fgcolor, uint32_t light, uint32_t desaturate, uint32_t inv_desaturate, BgraColor shade_fade, BgraColor shade_light, const DrawerLight *lights, int num_lights, float viewpos_z)
		{
			using namespace DrawWall32TModes;

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

			return AddLights(material, fgcolor, lights, num_lights, viewpos_z);
		}

		FORCEINLINE BgraColor AddLights(BgraColor material, BgraColor fgcolor, const DrawerLight *lights, int num_lights, float viewpos_z)
		{
			using namespace DrawWall32TModes;

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
				float Lxy2 = light_x; // L.x*L.x + L.y*L.y
				float Lz = light_z - viewpos_z;
				float dist2 = Lxy2 + Lz * Lz;
				float rcp_dist = 1.f/sqrt(dist2);
				float dist = dist2 * rcp_dist;
				float distance_attenuation = 256.0f - MIN(dist * light_radius, 256.0f);

				// The simple light type
				float simple_attenuation = distance_attenuation;

				// The point light type
				// diffuse = dot(N,L) * attenuation
				float point_attenuation = light_y * rcp_dist * distance_attenuation;

				uint32_t attenuation = (int32_t)((light_y == 0.0f) ? simple_attenuation : point_attenuation);

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

		FORCEINLINE BgraColor Blend(BgraColor fgcolor, BgraColor bgcolor, unsigned int ifgcolor, uint32_t srcalpha, uint32_t destalpha)
		{
			using namespace DrawWall32TModes;

			if (BlendT::Mode == (int)WallBlendModes::Opaque)
			{
				fgcolor.a = 255;
				return fgcolor;
			}
			else if (BlendT::Mode == (int)WallBlendModes::Masked)
			{
				return (ifgcolor == 0) ? bgcolor : fgcolor;
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
				if (BlendT::Mode == (int)WallBlendModes::AddClamp)
				{
					outcolor.r = MIN<uint32_t>((fgcolor.r + bgcolor.r) >> 8, 255);
					outcolor.g = MIN<uint32_t>((fgcolor.g + bgcolor.g) >> 8, 255);
					outcolor.b = MIN<uint32_t>((fgcolor.b + bgcolor.b) >> 8, 255);
				}
				else if (BlendT::Mode == (int)WallBlendModes::SubClamp)
				{
					outcolor.r = MAX(int32_t(fgcolor.r - bgcolor.r) >> 8, 0);
					outcolor.g = MAX(int32_t(fgcolor.g - bgcolor.g) >> 8, 0);
					outcolor.b = MAX(int32_t(fgcolor.b - bgcolor.b) >> 8, 0);
				}
				else if (BlendT::Mode == (int)WallBlendModes::RevSubClamp)
				{
					outcolor.r = MAX(int32_t(bgcolor.r - fgcolor.r) >> 8, 0);
					outcolor.g = MAX(int32_t(bgcolor.g - fgcolor.g) >> 8, 0);
					outcolor.b = MAX(int32_t(bgcolor.b - fgcolor.b) >> 8, 0);
				}
				outcolor.a = 255;
				return outcolor;
			}
		}
	};

	typedef DrawWall32T<DrawWall32TModes::OpaqueWall> DrawWall32Command;
	typedef DrawWall32T<DrawWall32TModes::MaskedWall> DrawWallMasked32Command;
	typedef DrawWall32T<DrawWall32TModes::AddClampWall> DrawWallAddClamp32Command;
	typedef DrawWall32T<DrawWall32TModes::SubClampWall> DrawWallSubClamp32Command;
	typedef DrawWall32T<DrawWall32TModes::RevSubClampWall> DrawWallRevSubClamp32Command;
}

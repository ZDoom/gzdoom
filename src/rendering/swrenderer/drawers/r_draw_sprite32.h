/*
**  Drawer commands for sprites
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
#include "swrenderer/viewport/r_walldrawer.h"

namespace swrenderer
{
	namespace DrawSprite32TModes
	{
		enum class SpriteBlendModes { Copy, Opaque, Shaded, AddClampShaded, AddClamp, SubClamp, RevSubClamp };
		struct CopySprite { static const int Mode = (int)SpriteBlendModes::Copy; };
		struct OpaqueSprite { static const int Mode = (int)SpriteBlendModes::Opaque; };
		struct ShadedSprite { static const int Mode = (int)SpriteBlendModes::Shaded; };
		struct AddClampShadedSprite { static const int Mode = (int)SpriteBlendModes::AddClampShaded; };
		struct AddClampSprite { static const int Mode = (int)SpriteBlendModes::AddClamp; };
		struct SubClampSprite { static const int Mode = (int)SpriteBlendModes::SubClamp; };
		struct RevSubClampSprite { static const int Mode = (int)SpriteBlendModes::RevSubClamp; };

		enum class FilterModes { Nearest, Linear };
		struct NearestFilter { static const int Mode = (int)FilterModes::Nearest; };
		struct LinearFilter { static const int Mode = (int)FilterModes::Linear; };

		enum class ShadeMode { Simple, Advanced };
		struct SimpleShade { static const int Mode = (int)ShadeMode::Simple; };
		struct AdvancedShade { static const int Mode = (int)ShadeMode::Advanced; };

		enum class SpriteSamplers { Texture, Fill, Shaded, Translated };
		struct TextureSampler { static const int Mode = (int)SpriteSamplers::Texture; };
		struct FillSampler { static const int Mode = (int)SpriteSamplers::Fill; };
		struct ShadedSampler { static const int Mode = (int)SpriteSamplers::Shaded; };
		struct TranslatedSampler { static const int Mode = (int)SpriteSamplers::Translated; };
	}

	template<typename BlendT, typename SamplerT>
	class DrawSprite32T
	{
	public:
		static void DrawColumn(const SpriteDrawerArgs& args)
		{
			using namespace DrawSprite32TModes;

			auto shade_constants = args.ColormapConstants();
			if (SamplerT::Mode == (int)SpriteSamplers::Texture)
			{
				const uint32_t *source2 = (const uint32_t*)args.TexturePixels2();
				bool is_nearest_filter = (source2 == nullptr);

				if (shade_constants.simple_shade)
				{
					if (is_nearest_filter)
						Loop<SimpleShade, NearestFilter>(args, shade_constants);
					else
						Loop<SimpleShade, LinearFilter>(args, shade_constants);
				}
				else
				{
					if (is_nearest_filter)
						Loop<AdvancedShade, NearestFilter>(args, shade_constants);
					else
						Loop<AdvancedShade, LinearFilter>(args, shade_constants);
				}
			}
			else // no linear filtering for translated, shaded or fill
			{
				if (shade_constants.simple_shade)
				{
					Loop<SimpleShade, NearestFilter>(args, shade_constants);
				}
				else
				{
					Loop<AdvancedShade, NearestFilter>(args, shade_constants);
				}
			}
		}

		template<typename ShadeModeT, typename FilterModeT>
		FORCEINLINE static void Loop(const SpriteDrawerArgs& args, ShadeConstants shade_constants)
		{
			using namespace DrawSprite32TModes;

			const uint32_t *source;
			const uint32_t *source2;
			const uint8_t *colormap;
			const uint32_t *translation;

			if (SamplerT::Mode == (int)SpriteSamplers::Shaded || SamplerT::Mode == (int)SpriteSamplers::Translated)
			{
				source = (const uint32_t*)args.TexturePixels();
				source2 = nullptr;
				colormap = args.Colormap(args.Viewport());
				translation = (const uint32_t*)args.TranslationMap();
			}
			else
			{
				source = (const uint32_t*)args.TexturePixels();
				source2 = (const uint32_t*)args.TexturePixels2();
				colormap = nullptr;
				translation = nullptr;
			}

			int textureheight = args.TextureHeight();
			uint32_t one = ((0x20000000 + textureheight - 1) / textureheight) * 2 + 1;

			// Shade constants
			BgraColor dynlight = args.DynamicLight();
			uint32_t light = 256 - (args.Light() >> (FRACBITS - 8));
			BgraColor mlight;

			int inv_desaturate;
			BgraColor shade_fade, shade_light;
			int desaturate;
			BgraColor lightcontrib;
			if (ShadeModeT::Mode == (int)ShadeMode::Advanced)
			{
				uint32_t inv_light = 256 - light;
				inv_desaturate = 256 - shade_constants.desaturate;
				shade_fade.r = shade_constants.fade_red * inv_light;
				shade_fade.g = shade_constants.fade_green * inv_light;
				shade_fade.b = shade_constants.fade_blue * inv_light;
				shade_light.r = shade_constants.light_red;
				shade_light.g = shade_constants.light_green;
				shade_light.b = shade_constants.light_blue;
				desaturate = shade_constants.desaturate;
				lightcontrib.r = MIN<uint32_t>(light + dynlight.r, 256) - light;
				lightcontrib.g = MIN<uint32_t>(light + dynlight.g, 256) - light;
				lightcontrib.b = MIN<uint32_t>(light + dynlight.b, 256) - light;
				mlight.r = light;
				mlight.g = light;
				mlight.b = light;
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
				lightcontrib = 0;
				mlight.r = MIN<uint32_t>(light + dynlight.r, 256);
				mlight.g = MIN<uint32_t>(light + dynlight.g, 256);
				mlight.b = MIN<uint32_t>(light + dynlight.b, 256);
			}

			int count = args.Count();
			if (count <= 0) return;

			int pitch = args.Viewport()->RenderTarget->GetPitch();
			uint32_t fracstep = args.TextureVStep();
			uint32_t frac = args.TextureVPos();
			uint32_t texturefracx = args.TextureUPos();
			uint32_t *dest = (uint32_t*)args.Dest();
			int dest_y = args.DestY();

			if (FilterModeT::Mode == (int)FilterModes::Linear)
			{
				frac -= one / 2;
			}

			uint32_t srcalpha = args.SrcAlpha() >> (FRACBITS - 8);
			uint32_t destalpha = args.DestAlpha() >> (FRACBITS - 8);
			uint32_t srccolor = args.SrcColorBgra();
			uint32_t color = LightBgra::shade_bgra_simple(args.SolidColorBgra(),
				LightBgra::calc_light_multiplier(light));

			for (int index = 0; index < count; index++)
			{
				BgraColor bgcolor;
				if (BlendT::Mode != (int)SpriteBlendModes::Opaque && BlendT::Mode != (int)SpriteBlendModes::Copy)
				{
					bgcolor = *dest;
				}
				else
				{
					bgcolor = 0;
				}

				uint32_t ifgcolor = Sample<FilterModeT>(frac, source, source2, translation, textureheight, one, texturefracx, color, srccolor);
				uint32_t ifgshade = SampleShade(frac, source, colormap);
				BgraColor fgcolor = Shade<ShadeModeT>(ifgcolor, mlight, desaturate, inv_desaturate, shade_fade, shade_light, lightcontrib);
				BgraColor outcolor = Blend(fgcolor, bgcolor, ifgcolor, ifgshade, srcalpha, destalpha);

				*dest = outcolor;
				dest += pitch;
				frac += fracstep;
			}
		}

		template<typename FilterModeT>
		FORCEINLINE static BgraColor Sample(uint32_t frac, const uint32_t *source, const uint32_t *source2, const uint32_t *translation, int textureheight, uint32_t one, uint32_t texturefracx, uint32_t color, uint32_t srccolor)
		{
			using namespace DrawSprite32TModes;

			if (SamplerT::Mode == (int)SpriteSamplers::Shaded)
			{
				return color;
			}
			else if (SamplerT::Mode == (int)SpriteSamplers::Translated)
			{
				const uint8_t *sourcepal = (const uint8_t *)source;
				return translation[sourcepal[frac >> FRACBITS]];
			}
			else if (SamplerT::Mode == (int)SpriteSamplers::Fill)
			{
				return srccolor;
			}
			else if (FilterModeT::Mode == (int)FilterModes::Nearest)
			{
				int sample_index = (((frac << 2) >> FRACBITS) * textureheight) >> FRACBITS;
				return source[sample_index];
			}
			else
			{
				// Clamp to edge
				unsigned int frac_y0 = (clamp<unsigned int>(frac, 0, 1 << 30) >> (FRACBITS - 2)) * textureheight;
				unsigned int frac_y1 = (clamp<unsigned int>(frac + one, 0, 1 << 30) >> (FRACBITS - 2)) * textureheight;
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

		FORCEINLINE static uint32_t SampleShade(uint32_t frac, const uint32_t *source, const uint8_t *colormap)
		{
			using namespace DrawSprite32TModes;

			if (SamplerT::Mode == (int)SpriteSamplers::Shaded)
			{
				const uint8_t *sourcepal = (const uint8_t *)source;
				unsigned int sampleshadeout = colormap[sourcepal[frac >> FRACBITS]];
				return clamp<unsigned int>(sampleshadeout, 0, 64) * 4;
			}
			else
			{
				return 0;
			}
		}

		template<typename ShadeModeT>
		FORCEINLINE static BgraColor Shade(BgraColor fgcolor, BgraColor mlight, uint32_t desaturate, uint32_t inv_desaturate, BgraColor shade_fade, BgraColor shade_light, BgraColor lightcontrib)
		{
			using namespace DrawSprite32TModes;

			if (BlendT::Mode == (int)SpriteBlendModes::Copy)
				return fgcolor;

			if (ShadeModeT::Mode == (int)ShadeMode::Simple)
			{
				fgcolor.r = (fgcolor.r * mlight.r) >> 8;
				fgcolor.g = (fgcolor.g * mlight.g) >> 8;
				fgcolor.b = (fgcolor.b * mlight.b) >> 8;
				return fgcolor;
			}
			else
			{
				BgraColor lit_dynlight = ((fgcolor.r * lightcontrib.r) >> 8);

				uint32_t intensity = ((fgcolor.r * 77 + fgcolor.g * 143 + fgcolor.b * 37) >> 8) * desaturate;
				fgcolor.r = (((shade_fade.r + ((fgcolor.r * inv_desaturate + intensity) >> 8) * mlight.r) >> 8) * shade_light.r) >> 8;
				fgcolor.g = (((shade_fade.g + ((fgcolor.g * inv_desaturate + intensity) >> 8) * mlight.g) >> 8) * shade_light.g) >> 8;
				fgcolor.b = (((shade_fade.b + ((fgcolor.b * inv_desaturate + intensity) >> 8) * mlight.b) >> 8) * shade_light.b) >> 8;

				fgcolor.r = MIN<uint32_t>(fgcolor.r + lit_dynlight.r, 255);
				fgcolor.g = MIN<uint32_t>(fgcolor.g + lit_dynlight.g, 255);
				fgcolor.b = MIN<uint32_t>(fgcolor.b + lit_dynlight.b, 255);
				return fgcolor;
			}
		}

		FORCEINLINE static BgraColor Blend(BgraColor fgcolor, BgraColor bgcolor, uint32_t ifgcolor, uint32_t ifgshade, uint32_t srcalpha, uint32_t destalpha)
		{
			using namespace DrawSprite32TModes;

			if (BlendT::Mode == (int)SpriteBlendModes::Opaque)
			{
				fgcolor.a = 255;
				return fgcolor;
			}
			else if (BlendT::Mode == (int)SpriteBlendModes::Shaded)
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
			else if (BlendT::Mode == (int)SpriteBlendModes::AddClampShaded)
			{
				uint32_t alpha = ifgshade;
				BgraColor outcolor;
				outcolor.r = MIN<uint32_t>(((fgcolor.r * alpha) >> 8) + bgcolor.r, 255);
				outcolor.g = MIN<uint32_t>(((fgcolor.g * alpha) >> 8) + bgcolor.g, 255);
				outcolor.b = MIN<uint32_t>(((fgcolor.b * alpha) >> 8) + bgcolor.b, 255);
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
				if (BlendT::Mode == (int)SpriteBlendModes::AddClamp)
				{
					outcolor.r = MIN<uint32_t>((fgcolor.r + bgcolor.r) >> 8, 255);
					outcolor.g = MIN<uint32_t>((fgcolor.g + bgcolor.g) >> 8, 255);
					outcolor.b = MIN<uint32_t>((fgcolor.b + bgcolor.b) >> 8, 255);
				}
				else if (BlendT::Mode == (int)SpriteBlendModes::SubClamp)
				{
					outcolor.r = MAX(int32_t(fgcolor.r - bgcolor.r) >> 8, 0);
					outcolor.g = MAX(int32_t(fgcolor.g - bgcolor.g) >> 8, 0);
					outcolor.b = MAX(int32_t(fgcolor.b - bgcolor.b) >> 8, 0);
				}
				else if (BlendT::Mode == (int)SpriteBlendModes::RevSubClamp)
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

	typedef DrawSprite32T<DrawSprite32TModes::CopySprite, DrawSprite32TModes::TextureSampler> DrawSpriteCopy32Command;

	typedef DrawSprite32T<DrawSprite32TModes::OpaqueSprite, DrawSprite32TModes::TextureSampler> DrawSprite32Command;
	typedef DrawSprite32T<DrawSprite32TModes::AddClampSprite, DrawSprite32TModes::TextureSampler> DrawSpriteAddClamp32Command;
	typedef DrawSprite32T<DrawSprite32TModes::SubClampSprite, DrawSprite32TModes::TextureSampler> DrawSpriteSubClamp32Command;
	typedef DrawSprite32T<DrawSprite32TModes::RevSubClampSprite, DrawSprite32TModes::TextureSampler> DrawSpriteRevSubClamp32Command;

	typedef DrawSprite32T<DrawSprite32TModes::OpaqueSprite, DrawSprite32TModes::FillSampler> FillSprite32Command;
	typedef DrawSprite32T<DrawSprite32TModes::AddClampSprite, DrawSprite32TModes::FillSampler> FillSpriteAddClamp32Command;
	typedef DrawSprite32T<DrawSprite32TModes::SubClampSprite, DrawSprite32TModes::FillSampler> FillSpriteSubClamp32Command;
	typedef DrawSprite32T<DrawSprite32TModes::RevSubClampSprite, DrawSprite32TModes::FillSampler> FillSpriteRevSubClamp32Command;

	typedef DrawSprite32T<DrawSprite32TModes::ShadedSprite, DrawSprite32TModes::ShadedSampler> DrawSpriteShaded32Command;
	typedef DrawSprite32T<DrawSprite32TModes::AddClampShadedSprite, DrawSprite32TModes::ShadedSampler> DrawSpriteAddClampShaded32Command;

	typedef DrawSprite32T<DrawSprite32TModes::OpaqueSprite, DrawSprite32TModes::TranslatedSampler> DrawSpriteTranslated32Command;
	typedef DrawSprite32T<DrawSprite32TModes::AddClampSprite, DrawSprite32TModes::TranslatedSampler> DrawSpriteTranslatedAddClamp32Command;
	typedef DrawSprite32T<DrawSprite32TModes::SubClampSprite, DrawSprite32TModes::TranslatedSampler> DrawSpriteTranslatedSubClamp32Command;
	typedef DrawSprite32T<DrawSprite32TModes::RevSubClampSprite, DrawSprite32TModes::TranslatedSampler> DrawSpriteTranslatedRevSubClamp32Command;
}

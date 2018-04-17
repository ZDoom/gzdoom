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
	class DrawSprite32T : public DrawerCommand
	{
	public:
		SpriteDrawerArgs args;

		DrawSprite32T(const SpriteDrawerArgs &drawerargs) : args(drawerargs) { }

		void Execute(DrawerThread *thread) override
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
						Loop<SimpleShade, NearestFilter>(thread, shade_constants);
					else
						Loop<SimpleShade, LinearFilter>(thread, shade_constants);
				}
				else
				{
					if (is_nearest_filter)
						Loop<AdvancedShade, NearestFilter>(thread, shade_constants);
					else
						Loop<AdvancedShade, LinearFilter>(thread, shade_constants);
				}
			}
			else // no linear filtering for translated, shaded or fill
			{
				if (shade_constants.simple_shade)
				{
					Loop<SimpleShade, NearestFilter>(thread, shade_constants);
				}
				else
				{
					Loop<AdvancedShade, NearestFilter>(thread, shade_constants);
				}
			}
		}

		template<typename ShadeModeT, typename FilterModeT>
		FORCEINLINE void VECTORCALL Loop(DrawerThread *thread, ShadeConstants shade_constants)
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
			__m128i dynlight = _mm_cvtsi32_si128(args.DynamicLight());
			dynlight = _mm_unpacklo_epi8(dynlight, _mm_setzero_si128());
			dynlight = _mm_shuffle_epi32(dynlight, _MM_SHUFFLE(1, 0, 1, 0));
			int light = 256 - (args.Light() >> (FRACBITS - 8));
			__m128i mlight = _mm_set_epi16(256, light, light, light, 256, light, light, light);

			__m128i inv_desaturate, shade_fade, shade_light;
			int desaturate;
			__m128i lightcontrib;
			if (ShadeModeT::Mode == (int)ShadeMode::Advanced)
			{
				__m128i inv_light = _mm_set_epi16(0, 256 - light, 256 - light, 256 - light, 0, 256 - light, 256 - light, 256 - light);
				inv_desaturate = _mm_setr_epi16(256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate, 256 - shade_constants.desaturate);
				shade_fade = _mm_set_epi16(shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue, shade_constants.fade_alpha, shade_constants.fade_red, shade_constants.fade_green, shade_constants.fade_blue);
				shade_fade = _mm_mullo_epi16(shade_fade, inv_light);
				shade_light = _mm_set_epi16(shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue, shade_constants.light_alpha, shade_constants.light_red, shade_constants.light_green, shade_constants.light_blue);
				desaturate = shade_constants.desaturate;

				lightcontrib = _mm_min_epi16(_mm_add_epi16(mlight, dynlight), _mm_set1_epi16(256));
				lightcontrib = _mm_sub_epi16(lightcontrib, mlight);
			}
			else
			{
				inv_desaturate = _mm_setzero_si128();
				shade_fade = _mm_setzero_si128();
				shade_fade = _mm_setzero_si128();
				shade_light = _mm_setzero_si128();
				desaturate = 0;
				lightcontrib = _mm_setzero_si128();

				mlight = _mm_min_epi16(_mm_add_epi16(mlight, dynlight), _mm_set1_epi16(256));
			}

			int count = args.Count();
			int pitch = args.Viewport()->RenderTarget->GetPitch();
			uint32_t fracstep = args.TextureVStep();
			uint32_t frac = args.TextureVPos();
			uint32_t texturefracx = args.TextureUPos();
			uint32_t *dest = (uint32_t*)args.Dest();
			int dest_y = args.DestY();

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
			uint32_t srccolor = args.SrcColorBgra();
			uint32_t color = LightBgra::shade_bgra_simple(args.SolidColorBgra(),
				LightBgra::calc_light_multiplier(light));

			int ssecount = count / 2;
			for (int index = 0; index < ssecount; index++)
			{
				int offset = index * pitch * 2;
				uint32_t desttmp[2];
				desttmp[0] = dest[offset];
				desttmp[1] = dest[offset + pitch];

				__m128i bgcolor;
				if (BlendT::Mode != (int)SpriteBlendModes::Opaque && BlendT::Mode != (int)SpriteBlendModes::Copy)
				{
					bgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)desttmp), _mm_setzero_si128());
				}
				else
				{
					bgcolor = _mm_setzero_si128();
				}

				unsigned int ifgcolor[2], ifgshade[2];
				ifgcolor[0] = Sample<FilterModeT>(frac, source, source2, translation, textureheight, one, texturefracx, color, srccolor);
				ifgshade[0] = SampleShade(frac, source, colormap);
				frac += fracstep;

				ifgcolor[1] = Sample<FilterModeT>(frac, source, source2, translation, textureheight, one, texturefracx, color, srccolor);
				ifgshade[1] = SampleShade(frac, source, colormap);
				frac += fracstep;

				__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

				fgcolor = Shade<ShadeModeT>(fgcolor, mlight, ifgcolor[0], ifgcolor[1], desaturate, inv_desaturate, shade_fade, shade_light, lightcontrib);
				__m128i outcolor = Blend(fgcolor, bgcolor, ifgcolor[0], ifgcolor[1], ifgshade[0], ifgshade[1], srcalpha, destalpha);

				_mm_storel_epi64((__m128i*)desttmp, outcolor);
				dest[offset] = desttmp[0];
				dest[offset + pitch] = desttmp[1];
			}

			if (ssecount * 2 != count)
			{
				int index = ssecount * 2;
				int offset = index * pitch;

				__m128i bgcolor;
				if (BlendT::Mode != (int)SpriteBlendModes::Opaque && BlendT::Mode != (int)SpriteBlendModes::Copy)
				{
					bgcolor = _mm_unpacklo_epi8(_mm_cvtsi32_si128(dest[offset]), _mm_setzero_si128());
				}
				else
				{
					bgcolor = _mm_setzero_si128();
				}

				// Sample
				unsigned int ifgcolor[2], ifgshade[2];
				ifgcolor[0] = Sample<FilterModeT>(frac, source, source2, translation, textureheight, one, texturefracx, color, srccolor);
				ifgcolor[1] = 0;
				ifgshade[0] = SampleShade(frac, source, colormap);
				ifgshade[1] = 0;
				__m128i fgcolor = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)ifgcolor), _mm_setzero_si128());

				fgcolor = Shade<ShadeModeT>(fgcolor, mlight, ifgcolor[0], ifgcolor[1], desaturate, inv_desaturate, shade_fade, shade_light, lightcontrib);
				__m128i outcolor = Blend(fgcolor, bgcolor, ifgcolor[0], ifgcolor[1], ifgshade[0], ifgshade[1], srcalpha, destalpha);

				dest[offset] = _mm_cvtsi128_si32(outcolor);
			}
		}

		template<typename FilterModeT>
		FORCEINLINE unsigned int VECTORCALL Sample(uint32_t frac, const uint32_t *source, const uint32_t *source2, const uint32_t *translation, int textureheight, uint32_t one, uint32_t texturefracx, uint32_t color, uint32_t srccolor)
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

				unsigned int sred = (RPART(p00) * (a * b) + RPART(p01) * (inv_a * b) + RPART(p10) * (a * inv_b) + RPART(p11) * (inv_a * inv_b) + 127) >> 8;
				unsigned int sgreen = (GPART(p00) * (a * b) + GPART(p01) * (inv_a * b) + GPART(p10) * (a * inv_b) + GPART(p11) * (inv_a * inv_b) + 127) >> 8;
				unsigned int sblue = (BPART(p00) * (a * b) + BPART(p01) * (inv_a * b) + BPART(p10) * (a * inv_b) + BPART(p11) * (inv_a * inv_b) + 127) >> 8;
				unsigned int salpha = (APART(p00) * (a * b) + APART(p01) * (inv_a * b) + APART(p10) * (a * inv_b) + APART(p11) * (inv_a * inv_b) + 127) >> 8;

				return (salpha << 24) | (sred << 16) | (sgreen << 8) | sblue;
			}
		}

		FORCEINLINE unsigned int VECTORCALL SampleShade(uint32_t frac, const uint32_t *source, const uint8_t *colormap)
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
		FORCEINLINE __m128i VECTORCALL Shade(__m128i fgcolor, __m128i mlight, unsigned int ifgcolor0, unsigned int ifgcolor1, int desaturate, __m128i inv_desaturate, __m128i shade_fade, __m128i shade_light, __m128i lightcontrib)
		{
			using namespace DrawSprite32TModes;

			if (BlendT::Mode == (int)SpriteBlendModes::Copy)
				return fgcolor;

			if (ShadeModeT::Mode == (int)ShadeMode::Simple)
			{
				fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, mlight), 8);
				return fgcolor;
			}
			else
			{
				__m128i lit_dynlight = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, lightcontrib), 8);

				int blue0 = BPART(ifgcolor0);
				int green0 = GPART(ifgcolor0);
				int red0 = RPART(ifgcolor0);
				int intensity0 = ((red0 * 77 + green0 * 143 + blue0 * 37) >> 8) * desaturate;

				int blue1 = BPART(ifgcolor1);
				int green1 = GPART(ifgcolor1);
				int red1 = RPART(ifgcolor1);
				int intensity1 = ((red1 * 77 + green1 * 143 + blue1 * 37) >> 8) * desaturate;

				__m128i intensity = _mm_set_epi16(0, intensity1, intensity1, intensity1, 0, intensity0, intensity0, intensity0);

				fgcolor = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(fgcolor, inv_desaturate), intensity), 8);
				fgcolor = _mm_mullo_epi16(fgcolor, mlight);
				fgcolor = _mm_srli_epi16(_mm_add_epi16(shade_fade, fgcolor), 8);
				fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, shade_light), 8);

				fgcolor = _mm_add_epi16(fgcolor, lit_dynlight);
				fgcolor = _mm_min_epi16(fgcolor, _mm_set1_epi16(255));
				return fgcolor;
			}
		}

		FORCEINLINE __m128i VECTORCALL Blend(__m128i fgcolor, __m128i bgcolor, unsigned int ifgcolor0, unsigned int ifgcolor1, unsigned int ifgshade0, unsigned int ifgshade1, uint32_t srcalpha, uint32_t destalpha)
		{
			using namespace DrawSprite32TModes;

			if (BlendT::Mode == (int)SpriteBlendModes::Opaque)
			{
				__m128i outcolor = fgcolor;
				outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
				outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));
				return outcolor;
			}
			else if (BlendT::Mode == (int)SpriteBlendModes::Shaded)
			{
				__m128i alpha = _mm_set_epi16(ifgshade1, ifgshade1, ifgshade1, ifgshade1, ifgshade0, ifgshade0, ifgshade0, ifgshade0);
				__m128i inv_alpha = _mm_sub_epi16(_mm_set1_epi16(256), alpha);

				fgcolor = _mm_mullo_epi16(fgcolor, alpha);
				bgcolor = _mm_mullo_epi16(bgcolor, inv_alpha);
				__m128i outcolor = _mm_srli_epi16(_mm_add_epi16(fgcolor, bgcolor), 8);
				outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
				outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));
				return outcolor;
			}
			else if (BlendT::Mode == (int)SpriteBlendModes::AddClampShaded)
			{
				__m128i alpha = _mm_set_epi16(ifgshade1, ifgshade1, ifgshade1, ifgshade1, ifgshade0, ifgshade0, ifgshade0, ifgshade0);

				fgcolor = _mm_srli_epi16(_mm_mullo_epi16(fgcolor, alpha), 8);
				__m128i outcolor = _mm_add_epi16(fgcolor, bgcolor);
				outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
				outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));
				return outcolor;
			}
			else
			{
				uint32_t alpha0 = APART(ifgcolor0);
				uint32_t alpha1 = APART(ifgcolor1);
				alpha0 += alpha0 >> 7; // 255->256
				alpha1 += alpha1 >> 7; // 255->256
				uint32_t inv_alpha0 = 256 - alpha0;
				uint32_t inv_alpha1 = 256 - alpha1;

				uint32_t bgalpha0 = (destalpha * alpha0 + (inv_alpha0 << 8) + 128) >> 8;
				uint32_t bgalpha1 = (destalpha * alpha1 + (inv_alpha1 << 8) + 128) >> 8;
				uint32_t fgalpha0 = (srcalpha * alpha0 + 128) >> 8;
				uint32_t fgalpha1 = (srcalpha * alpha1 + 128) >> 8;

				__m128i bgalpha = _mm_set_epi16(bgalpha1, bgalpha1, bgalpha1, bgalpha1, bgalpha0, bgalpha0, bgalpha0, bgalpha0);
				__m128i fgalpha = _mm_set_epi16(fgalpha1, fgalpha1, fgalpha1, fgalpha1, fgalpha0, fgalpha0, fgalpha0, fgalpha0);

				fgcolor = _mm_mullo_epi16(fgcolor, fgalpha);
				bgcolor = _mm_mullo_epi16(bgcolor, bgalpha);

				__m128i fg_lo = _mm_unpacklo_epi16(fgcolor, _mm_setzero_si128());
				__m128i bg_lo = _mm_unpacklo_epi16(bgcolor, _mm_setzero_si128());
				__m128i fg_hi = _mm_unpackhi_epi16(fgcolor, _mm_setzero_si128());
				__m128i bg_hi = _mm_unpackhi_epi16(bgcolor, _mm_setzero_si128());

				__m128i out_lo, out_hi;
				if (BlendT::Mode == (int)SpriteBlendModes::AddClamp)
				{
					out_lo = _mm_add_epi32(fg_lo, bg_lo);
					out_hi = _mm_add_epi32(fg_hi, bg_hi);
				}
				else if (BlendT::Mode == (int)SpriteBlendModes::SubClamp)
				{
					out_lo = _mm_sub_epi32(fg_lo, bg_lo);
					out_hi = _mm_sub_epi32(fg_hi, bg_hi);
				}
				else if (BlendT::Mode == (int)SpriteBlendModes::RevSubClamp)
				{
					out_lo = _mm_sub_epi32(bg_lo, fg_lo);
					out_hi = _mm_sub_epi32(bg_hi, fg_hi);
				}

				out_lo = _mm_srai_epi32(out_lo, 8);
				out_hi = _mm_srai_epi32(out_hi, 8);
				__m128i outcolor = _mm_packs_epi32(out_lo, out_hi);
				outcolor = _mm_packus_epi16(outcolor, _mm_setzero_si128());
				outcolor = _mm_or_si128(outcolor, _mm_set1_epi32(0xff000000));
				return outcolor;
			}
		}

		FString DebugInfo() override { return "DrawSprite32T"; }
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

/*
** r_drawerargs.cpp
**
**---------------------------------------------------------------------------
** Copyright 1998-2016 Randy Heit
** Copyright 2016 Magnus Norddahl
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include <stddef.h>
#include "r_drawerargs.h"
#include "r_draw_pal.h"
#include "r_draw_rgba.h"

namespace swrenderer
{
	namespace
	{
		SWPixelFormatDrawers *active_drawers;
		SWPalDrawers pal_drawers;
		SWTruecolorDrawers tc_drawers;
	}

	void R_InitColumnDrawers()
	{
		if (r_swtruecolor)
			active_drawers = &tc_drawers;
		else
			active_drawers = &pal_drawers;
	}

	SWPixelFormatDrawers *DrawerArgs::Drawers() const
	{
		return active_drawers;
	}

	ColumnDrawerArgs::ColumnDrawerArgs()
	{
		colfunc = &SWPixelFormatDrawers::DrawColumn;
		basecolfunc = &SWPixelFormatDrawers::DrawColumn;
		fuzzcolfunc = &SWPixelFormatDrawers::DrawFuzzColumn;
		transcolfunc = &SWPixelFormatDrawers::DrawTranslatedColumn;
	}

	SpanDrawerArgs::SpanDrawerArgs()
	{
		spanfunc = &SWPixelFormatDrawers::DrawSpan;
	}

	void DrawerArgs::SetColorMapLight(FSWColormap *base_colormap, float light, int shade)
	{
		mBaseColormap = base_colormap;
		mTranslation = nullptr;
		mLight = light;
		mShade = shade;
	}

	void DrawerArgs::SetTranslationMap(lighttable_t *translation)
	{
		mBaseColormap = nullptr;
		mTranslation = translation;
	}

	uint8_t *DrawerArgs::Colormap() const
	{
		if (mBaseColormap)
		{
			if (r_swtruecolor)
				return mBaseColormap->Maps;
			else
				return mBaseColormap->Maps + (GETPALOOKUP(mLight, mShade) << COLORMAPSHIFT);
		}
		else
		{
			return mTranslation;
		}
	}

	ShadeConstants DrawerArgs::ColormapConstants() const
	{
		ShadeConstants shadeConstants;
		if (mBaseColormap)
		{
			shadeConstants.light_red = mBaseColormap->Color.r * 256 / 255;
			shadeConstants.light_green = mBaseColormap->Color.g * 256 / 255;
			shadeConstants.light_blue = mBaseColormap->Color.b * 256 / 255;
			shadeConstants.light_alpha = mBaseColormap->Color.a * 256 / 255;
			shadeConstants.fade_red = mBaseColormap->Fade.r;
			shadeConstants.fade_green = mBaseColormap->Fade.g;
			shadeConstants.fade_blue = mBaseColormap->Fade.b;
			shadeConstants.fade_alpha = mBaseColormap->Fade.a;
			shadeConstants.desaturate = MIN(abs(mBaseColormap->Desaturate), 255) * 255 / 256;
			shadeConstants.simple_shade = (mBaseColormap->Color.d == 0x00ffffff && mBaseColormap->Fade.d == 0x00000000 && mBaseColormap->Desaturate == 0);
		}
		else
		{
			shadeConstants.light_red = 256;
			shadeConstants.light_green = 256;
			shadeConstants.light_blue = 256;
			shadeConstants.light_alpha = 256;
			shadeConstants.fade_red = 0;
			shadeConstants.fade_green = 0;
			shadeConstants.fade_blue = 0;
			shadeConstants.fade_alpha = 256;
			shadeConstants.desaturate = 0;
			shadeConstants.simple_shade = true;
		}
		return shadeConstants;
	}

	void SpanDrawerArgs::SetSpanTexture(FTexture *tex)
	{
		tex->GetWidth();
		ds_xbits = tex->WidthBits;
		ds_ybits = tex->HeightBits;
		if ((1 << ds_xbits) > tex->GetWidth())
		{
			ds_xbits--;
		}
		if ((1 << ds_ybits) > tex->GetHeight())
		{
			ds_ybits--;
		}

		ds_source = r_swtruecolor ? (const uint8_t*)tex->GetPixelsBgra() : tex->GetPixels();
		ds_source_mipmapped = tex->Mipmapped() && tex->GetWidth() > 1 && tex->GetHeight() > 1;
	}

	void ColumnDrawerArgs::DrawMaskedColumn(int x, fixed_t iscale, FTexture *tex, fixed_t col, double spryscale, double sprtopscreen, bool sprflipvert, const short *mfloorclip, const short *mceilingclip, bool unmasked)
	{
		// Handle the linear filtered version in a different function to reduce chances of merge conflicts from zdoom.
		if (r_swtruecolor && !drawer_needs_pal_input) // To do: add support to R_DrawColumnHoriz_rgba
		{
			DrawMaskedColumnBgra(x, iscale, tex, col, spryscale, sprtopscreen, sprflipvert, mfloorclip, mceilingclip, unmasked);
			return;
		}

		dc_x = x;
		dc_iscale = iscale;
		dc_textureheight = tex->GetHeight();

		const FTexture::Span *span;
		const BYTE *column;
		if (r_swtruecolor && !drawer_needs_pal_input)
			column = (const BYTE *)tex->GetColumnBgra(col >> FRACBITS, &span);
		else
			column = tex->GetColumn(col >> FRACBITS, &span);

		FTexture::Span unmaskedSpan[2];
		if (unmasked)
		{
			span = unmaskedSpan;
			unmaskedSpan[0].TopOffset = 0;
			unmaskedSpan[0].Length = tex->GetHeight();
			unmaskedSpan[1].TopOffset = 0;
			unmaskedSpan[1].Length = 0;
		}

		int pixelsize = r_swtruecolor ? 4 : 1;

		while (span->Length != 0)
		{
			const int length = span->Length;
			const int top = span->TopOffset;

			// calculate unclipped screen coordinates for post
			dc_yl = (int)(sprtopscreen + spryscale * top + 0.5);
			dc_yh = (int)(sprtopscreen + spryscale * (top + length) + 0.5) - 1;

			if (sprflipvert)
			{
				swapvalues(dc_yl, dc_yh);
			}

			if (dc_yh >= mfloorclip[dc_x])
			{
				dc_yh = mfloorclip[dc_x] - 1;
			}
			if (dc_yl < mceilingclip[dc_x])
			{
				dc_yl = mceilingclip[dc_x];
			}

			if (dc_yl <= dc_yh)
			{
				dc_texturefrac = FLOAT2FIXED((dc_yl + 0.5 - sprtopscreen) / spryscale);
				dc_source = column;
				dc_source2 = nullptr;
				SetDest(dc_x, dc_yl);
				dc_count = dc_yh - dc_yl + 1;

				fixed_t maxfrac = ((top + length) << FRACBITS) - 1;
				dc_texturefrac = MAX(dc_texturefrac, 0);
				dc_texturefrac = MIN(dc_texturefrac, maxfrac);
				if (dc_iscale > 0)
					dc_count = MIN(dc_count, (maxfrac - dc_texturefrac + dc_iscale - 1) / dc_iscale);
				else if (dc_iscale < 0)
					dc_count = MIN(dc_count, (dc_texturefrac - dc_iscale) / (-dc_iscale));

				(Drawers()->*colfunc)(*this);
			}
			span++;
		}
	}

	void ColumnDrawerArgs::DrawMaskedColumnBgra(int x, fixed_t iscale, FTexture *tex, fixed_t col, double spryscale, double sprtopscreen, bool sprflipvert, const short *mfloorclip, const short *mceilingclip, bool unmasked)
	{
		dc_x = x;
		dc_iscale = iscale;

		// Normalize to 0-1 range:
		double uv_stepd = FIXED2DBL(dc_iscale);
		double v_step = uv_stepd / tex->GetHeight();

		// Convert to uint32:
		dc_iscale = (uint32_t)(v_step * (1 << 30));

		// Texture mipmap and filter selection:
		fixed_t xoffset = col;

		double xmagnitude = 1.0; // To do: pass this into R_DrawMaskedColumn
		double ymagnitude = fabs(uv_stepd);
		double magnitude = MAX(ymagnitude, xmagnitude);
		double min_lod = -1000.0;
		double lod = MAX(log2(magnitude) + r_lod_bias, min_lod);
		bool magnifying = lod < 0.0f;

		int mipmap_offset = 0;
		int mip_width = tex->GetWidth();
		int mip_height = tex->GetHeight();
		uint32_t xpos = (uint32_t)((((uint64_t)xoffset) << FRACBITS) / mip_width);
		if (r_mipmap && tex->Mipmapped() && mip_width > 1 && mip_height > 1)
		{
			int level = (int)lod;
			while (level > 0 && mip_width > 1 && mip_height > 1)
			{
				mipmap_offset += mip_width * mip_height;
				level--;
				mip_width = MAX(mip_width >> 1, 1);
				mip_height = MAX(mip_height >> 1, 1);
			}
		}
		xoffset = (xpos >> FRACBITS) * mip_width;

		const uint32_t *pixels = tex->GetPixelsBgra() + mipmap_offset;

		bool filter_nearest = (magnifying && !r_magfilter) || (!magnifying && !r_minfilter);
		if (filter_nearest)
		{
			xoffset = MAX(MIN(xoffset, (mip_width << FRACBITS) - 1), 0);

			int tx = xoffset >> FRACBITS;
			dc_source = (BYTE*)(pixels + tx * mip_height);
			dc_source2 = nullptr;
			dc_textureheight = mip_height;
			dc_texturefracx = 0;
		}
		else
		{
			xoffset = MAX(MIN(xoffset - (FRACUNIT / 2), (mip_width << FRACBITS) - 1), 0);

			int tx0 = xoffset >> FRACBITS;
			int tx1 = MIN(tx0 + 1, mip_width - 1);
			dc_source = (BYTE*)(pixels + tx0 * mip_height);
			dc_source2 = (BYTE*)(pixels + tx1 * mip_height);
			dc_textureheight = mip_height;
			dc_texturefracx = (xoffset >> (FRACBITS - 4)) & 15;
		}

		// Grab the posts we need to draw
		const FTexture::Span *span;
		tex->GetColumnBgra(col >> FRACBITS, &span);
		FTexture::Span unmaskedSpan[2];
		if (unmasked)
		{
			span = unmaskedSpan;
			unmaskedSpan[0].TopOffset = 0;
			unmaskedSpan[0].Length = tex->GetHeight();
			unmaskedSpan[1].TopOffset = 0;
			unmaskedSpan[1].Length = 0;
		}

		// Draw each span post
		while (span->Length != 0)
		{
			const int length = span->Length;
			const int top = span->TopOffset;

			// calculate unclipped screen coordinates for post
			dc_yl = (int)(sprtopscreen + spryscale * top + 0.5);
			dc_yh = (int)(sprtopscreen + spryscale * (top + length) + 0.5) - 1;

			if (sprflipvert)
			{
				swapvalues(dc_yl, dc_yh);
			}

			if (dc_yh >= mfloorclip[dc_x])
			{
				dc_yh = mfloorclip[dc_x] - 1;
			}
			if (dc_yl < mceilingclip[dc_x])
			{
				dc_yl = mceilingclip[dc_x];
			}

			if (dc_yl <= dc_yh)
			{
				SetDest(dc_x, dc_yl);
				dc_count = dc_yh - dc_yl + 1;

				double v = ((dc_yl + 0.5 - sprtopscreen) / spryscale) / tex->GetHeight();
				dc_texturefrac = (uint32_t)(v * (1 << 30));

				(Drawers()->*colfunc)(*this);
			}
			span++;
		}
	}

	bool ColumnDrawerArgs::SetBlendFunc(int op, fixed_t fglevel, fixed_t bglevel, int flags)
	{
		// r_drawtrans is a seriously bad thing to turn off. I wonder if I should
		// just remove it completely.
		if (!r_drawtrans || (op == STYLEOP_Add && fglevel == FRACUNIT && bglevel == 0 && !(flags & STYLEF_InvertSource)))
		{
			if (flags & STYLEF_ColorIsFixed)
			{
				colfunc = &SWPixelFormatDrawers::FillColumn;
			}
			else if (TranslationMap() == nullptr)
			{
				colfunc = basecolfunc;
			}
			else
			{
				colfunc = transcolfunc;
				drawer_needs_pal_input = true;
			}
			return true;
		}
		if (flags & STYLEF_InvertSource)
		{
			dc_srcblend = Col2RGB8_Inverse[fglevel >> 10];
			dc_destblend = Col2RGB8_LessPrecision[bglevel >> 10];
			dc_srcalpha = fglevel;
			dc_destalpha = bglevel;
		}
		else if (op == STYLEOP_Add && fglevel + bglevel <= FRACUNIT)
		{
			dc_srcblend = Col2RGB8[fglevel >> 10];
			dc_destblend = Col2RGB8[bglevel >> 10];
			dc_srcalpha = fglevel;
			dc_destalpha = bglevel;
		}
		else
		{
			dc_srcblend = Col2RGB8_LessPrecision[fglevel >> 10];
			dc_destblend = Col2RGB8_LessPrecision[bglevel >> 10];
			dc_srcalpha = fglevel;
			dc_destalpha = bglevel;
		}
		switch (op)
		{
		case STYLEOP_Add:
			if (fglevel == 0 && bglevel == FRACUNIT)
			{
				return false;
			}
			if (fglevel + bglevel <= FRACUNIT)
			{ // Colors won't overflow when added
				if (flags & STYLEF_ColorIsFixed)
				{
					colfunc = &SWPixelFormatDrawers::FillAddColumn;
				}
				else if (TranslationMap() == nullptr)
				{
					colfunc = &SWPixelFormatDrawers::DrawAddColumn;
				}
				else
				{
					colfunc = &SWPixelFormatDrawers::DrawTranslatedAddColumn;
					drawer_needs_pal_input = true;
				}
			}
			else
			{ // Colors might overflow when added
				if (flags & STYLEF_ColorIsFixed)
				{
					colfunc = &SWPixelFormatDrawers::FillAddClampColumn;
				}
				else if (TranslationMap() == nullptr)
				{
					colfunc = &SWPixelFormatDrawers::DrawAddClampColumn;
				}
				else
				{
					colfunc = &SWPixelFormatDrawers::DrawAddClampTranslatedColumn;
					drawer_needs_pal_input = true;
				}
			}
			return true;

		case STYLEOP_Sub:
			if (flags & STYLEF_ColorIsFixed)
			{
				colfunc = &SWPixelFormatDrawers::FillSubClampColumn;
			}
			else if (TranslationMap() == nullptr)
			{
				colfunc = &SWPixelFormatDrawers::DrawSubClampColumn;
			}
			else
			{
				colfunc = &SWPixelFormatDrawers::DrawSubClampTranslatedColumn;
				drawer_needs_pal_input = true;
			}
			return true;

		case STYLEOP_RevSub:
			if (fglevel == 0 && bglevel == FRACUNIT)
			{
				return false;
			}
			if (flags & STYLEF_ColorIsFixed)
			{
				colfunc = &SWPixelFormatDrawers::FillRevSubClampColumn;
			}
			else if (TranslationMap() == nullptr)
			{
				colfunc = &SWPixelFormatDrawers::DrawRevSubClampColumn;
			}
			else
			{
				colfunc = &SWPixelFormatDrawers::DrawRevSubClampTranslatedColumn;
				drawer_needs_pal_input = true;
			}
			return true;

		default:
			return false;
		}
	}

	fixed_t ColumnDrawerArgs::GetAlpha(int type, fixed_t alpha)
	{
		switch (type)
		{
		case STYLEALPHA_Zero:		return 0;
		case STYLEALPHA_One:		return OPAQUE;
		case STYLEALPHA_Src:		return alpha;
		case STYLEALPHA_InvSrc:		return OPAQUE - alpha;
		default:					return 0;
		}
	}

	bool ColumnDrawerArgs::SetPatchStyle(FRenderStyle style, fixed_t alpha, int translation, uint32_t color, FDynamicColormap *&basecolormap, fixed_t shadedlightshade)
	{
		fixed_t fglevel, bglevel;

		drawer_needs_pal_input = false;

		style.CheckFuzz();

		if (style.BlendOp == STYLEOP_Shadow)
		{
			style = LegacyRenderStyles[STYLE_TranslucentStencil];
			alpha = TRANSLUC33;
			color = 0;
		}

		if (style.Flags & STYLEF_ForceAlpha)
		{
			alpha = clamp<fixed_t>(alpha, 0, OPAQUE);
		}
		else if (style.Flags & STYLEF_TransSoulsAlpha)
		{
			alpha = fixed_t(transsouls * OPAQUE);
		}
		else if (style.Flags & STYLEF_Alpha1)
		{
			alpha = FRACUNIT;
		}
		else
		{
			alpha = clamp<fixed_t>(alpha, 0, OPAQUE);
		}

		if (translation != -1)
		{
			SetTranslationMap(nullptr);
			if (translation != 0)
			{
				FRemapTable *table = TranslationToTable(translation);
				if (table != NULL && !table->Inactive)
				{
					if (r_swtruecolor)
						SetTranslationMap((uint8_t*)table->Palette);
					else
						SetTranslationMap(table->Remap);
				}
			}
		}

		// Check for special modes
		if (style.BlendOp == STYLEOP_Fuzz)
		{
			colfunc = fuzzcolfunc;
			return true;
		}
		else if (style == LegacyRenderStyles[STYLE_Shaded])
		{
			// Shaded drawer only gets 16 levels of alpha because it saves memory.
			if ((alpha >>= 12) == 0 || basecolormap == nullptr)
				return false;
			colfunc = &SWPixelFormatDrawers::DrawShadedColumn;
			drawer_needs_pal_input = true;
			CameraLight *cameraLight = CameraLight::Instance();
			dc_color = cameraLight->fixedcolormap ? cameraLight->fixedcolormap->Maps[APART(color)] : basecolormap->Maps[APART(color)];
			basecolormap = &ShadeFakeColormap[16 - alpha];
			if (cameraLight->fixedlightlev >= 0 && cameraLight->fixedcolormap == NULL)
			{
				fixed_t shade = shadedlightshade;
				if (shade == 0) FIXEDLIGHT2SHADE(cameraLight->fixedlightlev);
				SetColorMapLight(basecolormap, 0, shade);
			}
			else
			{
				SetColorMapLight(basecolormap, 0, shadedlightshade);
			}
			return true;
		}

		fglevel = GetAlpha(style.SrcAlpha, alpha);
		bglevel = GetAlpha(style.DestAlpha, alpha);

		if (style.Flags & STYLEF_ColorIsFixed)
		{
			uint32_t x = fglevel >> 10;
			uint32_t r = RPART(color);
			uint32_t g = GPART(color);
			uint32_t b = BPART(color);
			// dc_color is used by the rt_* routines. It is indexed into dc_srcblend.
			dc_color = RGB256k.RGB[r >> 2][g >> 2][b >> 2];
			if (style.Flags & STYLEF_InvertSource)
			{
				r = 255 - r;
				g = 255 - g;
				b = 255 - b;
			}
			uint32_t alpha = clamp(fglevel >> (FRACBITS - 8), 0, 255);
			dc_srccolor_bgra = (alpha << 24) | (r << 16) | (g << 8) | b;
			// dc_srccolor is used by the R_Fill* routines. It is premultiplied
			// with the alpha.
			dc_srccolor = ((((r*x) >> 4) << 20) | ((g*x) >> 4) | ((((b)*x) >> 4) << 10)) & 0x3feffbff;
			SetColorMapLight(&identitycolormap, 0, 0);
		}

		if (!ColumnDrawerArgs::SetBlendFunc(style.BlendOp, fglevel, bglevel, style.Flags))
		{
			return false;
		}
		return true;
	}

	bool ColumnDrawerArgs::SetPatchStyle(FRenderStyle style, float alpha, int translation, uint32_t color, FDynamicColormap *&basecolormap, fixed_t shadedlightshade)
	{
		return SetPatchStyle(style, FLOAT2FIXED(alpha), translation, color, basecolormap, shadedlightshade);
	}

	void WallDrawerArgs::SetDest(int x, int y)
	{
		int pixelsize = r_swtruecolor ? 4 : 1;
		dc_dest = dc_destorg + (ylookup[y] + x) * pixelsize;
		dc_dest_y = y;
	}

	void SpanDrawerArgs::SetSpanStyle(bool masked, bool additive, fixed_t alpha)
	{
		if (masked)
		{
			if (alpha < OPAQUE || additive)
			{
				if (!additive)
				{
					spanfunc = &SWPixelFormatDrawers::DrawSpanMaskedTranslucent;
					dc_srcblend = Col2RGB8[alpha >> 10];
					dc_destblend = Col2RGB8[(OPAQUE - alpha) >> 10];
					dc_srcalpha = alpha;
					dc_destalpha = OPAQUE - alpha;
				}
				else
				{
					spanfunc = &SWPixelFormatDrawers::DrawSpanMaskedAddClamp;
					dc_srcblend = Col2RGB8_LessPrecision[alpha >> 10];
					dc_destblend = Col2RGB8_LessPrecision[FRACUNIT >> 10];
					dc_srcalpha = alpha;
					dc_destalpha = FRACUNIT;
				}
			}
			else
			{
				spanfunc = &SWPixelFormatDrawers::DrawSpanMasked;
			}
		}
		else
		{
			if (alpha < OPAQUE || additive)
			{
				if (!additive)
				{
					spanfunc = &SWPixelFormatDrawers::DrawSpanTranslucent;
					dc_srcblend = Col2RGB8[alpha >> 10];
					dc_destblend = Col2RGB8[(OPAQUE - alpha) >> 10];
					dc_srcalpha = alpha;
					dc_destalpha = OPAQUE - alpha;
				}
				else
				{
					spanfunc = &SWPixelFormatDrawers::DrawSpanAddClamp;
					dc_srcblend = Col2RGB8_LessPrecision[alpha >> 10];
					dc_destblend = Col2RGB8_LessPrecision[FRACUNIT >> 10];
					dc_srcalpha = alpha;
					dc_destalpha = FRACUNIT;
				}
			}
			else
			{
				spanfunc = &SWPixelFormatDrawers::DrawSpan;
			}
		}
	}

	void WallDrawerArgs::SetStyle(bool masked, bool additive, fixed_t alpha)
	{
		if (alpha < OPAQUE || additive)
		{
			if (!additive)
			{
				wallfunc = &SWPixelFormatDrawers::DrawWallAddColumn;
				dc_srcblend = Col2RGB8[alpha >> 10];
				dc_destblend = Col2RGB8[(OPAQUE - alpha) >> 10];
				dc_srcalpha = alpha;
				dc_destalpha = OPAQUE - alpha;
			}
			else
			{
				wallfunc = &SWPixelFormatDrawers::DrawWallAddClampColumn;
				dc_srcblend = Col2RGB8_LessPrecision[alpha >> 10];
				dc_destblend = Col2RGB8_LessPrecision[FRACUNIT >> 10];
				dc_srcalpha = alpha;
				dc_destalpha = FRACUNIT;
			}
		}
		else if (masked)
		{
			wallfunc = &SWPixelFormatDrawers::DrawWallMaskedColumn;
		}
		else
		{
			wallfunc = &SWPixelFormatDrawers::DrawWallColumn;
		}
	}

	void SpanDrawerArgs::DrawSpan()
	{
		(Drawers()->*spanfunc)(*this);
	}

	void SpanDrawerArgs::DrawTiltedSpan(int y, int x1, int x2, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy, FDynamicColormap *basecolormap)
	{
		Drawers()->DrawTiltedSpan(*this, y, x1, x2, plane_sz, plane_su, plane_sv, plane_shade, planeshade, planelightfloat, pviewx, pviewy, basecolormap);
	}

	void SpanDrawerArgs::DrawFogBoundaryLine(int y, int x1, int x2)
	{
		Drawers()->DrawFogBoundaryLine(*this, y, x1, x2);
	}

	void SpanDrawerArgs::DrawColoredSpan(int y, int x1, int x2)
	{
		Drawers()->DrawColoredSpan(*this, y, x1, x2);
	}

	void SkyDrawerArgs::DrawSingleSkyColumn(uint32_t solid_top, uint32_t solid_bottom, bool fadeSky)
	{
		Drawers()->DrawSingleSkyColumn(*this, solid_top, solid_bottom, fadeSky);
	}

	void SkyDrawerArgs::DrawDoubleSkyColumn(uint32_t solid_top, uint32_t solid_bottom, bool fadeSky)
	{
		Drawers()->DrawDoubleSkyColumn(*this, solid_top, solid_bottom, fadeSky);
	}

	void SkyDrawerArgs::SetDest(int x, int y)
	{
		int pixelsize = r_swtruecolor ? 4 : 1;
		dc_dest = dc_destorg + (ylookup[y] + x) * pixelsize;
		dc_dest_y = y;
	}

	void ColumnDrawerArgs::FillColumn()
	{
		Drawers()->FillColumn(*this);
	}

	void ColumnDrawerArgs::SetDest(int x, int y)
	{
		int pixelsize = r_swtruecolor ? 4 : 1;
		dc_dest = dc_destorg + (ylookup[y] + x) * pixelsize;
		dc_dest_y = y;
	}
}

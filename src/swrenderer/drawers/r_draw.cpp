/*
** r_draw.cpp
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

#include "templates.h"
#include "doomdef.h"
#include "i_system.h"
#include "w_wad.h"
#include "v_video.h"
#include "doomstat.h"
#include "st_stuff.h"
#include "g_game.h"
#include "g_level.h"
#include "r_data/r_translate.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "r_draw.h"
#include "r_draw_rgba.h"
#include "r_draw_pal.h"
#include "r_thread.h"
#include "swrenderer/r_main.h"

CVAR(Bool, r_dynlights, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);

namespace swrenderer
{
	double dc_texturemid;
	FLightNode *dc_light_list;

	int ylookup[MAXHEIGHT];
	uint8_t shadetables[NUMCOLORMAPS * 16 * 256];
	FDynamicColormap ShadeFakeColormap[16];
	uint8_t identitymap[256];
	FDynamicColormap identitycolormap;
	int fuzzoffset[FUZZTABLE + 1];
	int fuzzpos;
	int fuzzviewheight;

	short zeroarray[MAXWIDTH];
	short screenheightarray[MAXWIDTH];

	DrawerFunc colfunc;
	DrawerFunc basecolfunc;
	DrawerFunc fuzzcolfunc;
	DrawerFunc transcolfunc;
	DrawerFunc spanfunc;

	namespace drawerargs
	{
		int dc_pitch;
		lighttable_t *dc_colormap;
		FSWColormap *dc_fcolormap;
		ShadeConstants dc_shade_constants;
		fixed_t dc_light;
		int dc_x;
		int dc_yl;
		int dc_yh;
		fixed_t dc_iscale;
		fixed_t dc_texturefrac;
		uint32_t dc_textureheight;
		int dc_color;
		uint32_t dc_srccolor;
		uint32_t dc_srccolor_bgra;
		uint32_t *dc_srcblend;
		uint32_t *dc_destblend;
		fixed_t dc_srcalpha;
		fixed_t dc_destalpha;
		const uint8_t *dc_source;
		const uint8_t *dc_source2;
		uint32_t dc_texturefracx;
		uint8_t *dc_translation;
		uint8_t *dc_dest;
		uint8_t *dc_destorg;
		int dc_destheight;
		int dc_count;
		FVector3 dc_normal;
		FVector3 dc_viewpos;
		FVector3 dc_viewpos_step;
		TriLight *dc_lights;
		int dc_num_lights;
		uint32_t dc_wall_texturefrac[4];
		uint32_t dc_wall_iscale[4];
		uint8_t *dc_wall_colormap[4];
		fixed_t dc_wall_light[4];
		const uint8_t *dc_wall_source[4];
		const uint8_t *dc_wall_source2[4];
		uint32_t dc_wall_texturefracx[4];
		uint32_t dc_wall_sourceheight[4];
		int dc_wall_fracbits;
		int ds_y;
		int ds_x1;
		int ds_x2;
		lighttable_t * ds_colormap;
		FSWColormap *ds_fcolormap;
		ShadeConstants ds_shade_constants;
		dsfixed_t ds_light;
		dsfixed_t ds_xfrac;
		dsfixed_t ds_yfrac;
		dsfixed_t ds_xstep;
		dsfixed_t ds_ystep;
		int ds_xbits;
		int ds_ybits;
		fixed_t ds_alpha;
		double ds_lod;
		const uint8_t *ds_source;
		bool ds_source_mipmapped;
		int ds_color;
		bool drawer_needs_pal_input;
		unsigned int dc_tspans[4][MAXHEIGHT];
		unsigned int *dc_ctspan[4];
		unsigned int *horizspan[4];
	}

	namespace
	{
		SWPixelFormatDrawers *active_drawers;
		SWPalDrawers pal_drawers;
		SWTruecolorDrawers tc_drawers;
	}

	SWPixelFormatDrawers *R_Drawers()
	{
		return active_drawers;
	}

	void R_InitColumnDrawers()
	{
		if (r_swtruecolor)
			active_drawers = &tc_drawers;
		else
			active_drawers = &pal_drawers;

		colfunc = basecolfunc = &SWPixelFormatDrawers::DrawColumn;
		fuzzcolfunc = &SWPixelFormatDrawers::DrawFuzzColumn;
		transcolfunc = &SWPixelFormatDrawers::DrawTranslatedColumn;
		spanfunc = &SWPixelFormatDrawers::DrawSpan;
	}

	void R_InitShadeMaps()
	{
		int i, j;
		// set up shading tables for shaded columns
		// 16 colormap sets, progressing from full alpha to minimum visible alpha

		uint8_t *table = shadetables;

		// Full alpha
		for (i = 0; i < 16; ++i)
		{
			ShadeFakeColormap[i].Color = ~0u;
			ShadeFakeColormap[i].Desaturate = ~0u;
			ShadeFakeColormap[i].Next = NULL;
			ShadeFakeColormap[i].Maps = table;

			for (j = 0; j < NUMCOLORMAPS; ++j)
			{
				int a = (NUMCOLORMAPS - j) * 256 / NUMCOLORMAPS * (16 - i);
				for (int k = 0; k < 256; ++k)
				{
					uint8_t v = (((k + 2) * a) + 256) >> 14;
					table[k] = MIN<uint8_t>(v, 64);
				}
				table += 256;
			}
		}
		for (i = 0; i < NUMCOLORMAPS * 16 * 256; ++i)
		{
			assert(shadetables[i] <= 64);
		}

		// Set up a guaranteed identity map
		for (i = 0; i < 256; ++i)
		{
			identitymap[i] = i;
		}
		identitycolormap.Maps = identitymap;
	}

	void R_InitFuzzTable(int fuzzoff)
	{
		/*
			FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
			FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
			FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,
			FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,
			FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,
			FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,
			FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF 
		*/

		static const int8_t fuzzinit[FUZZTABLE] = {
			1,-1, 1,-1, 1, 1,-1,
			1, 1,-1, 1, 1, 1,-1,
			1, 1, 1,-1,-1,-1,-1,
			1,-1,-1, 1, 1, 1, 1,-1,
			1,-1, 1, 1,-1,-1, 1,
			1,-1,-1,-1,-1, 1, 1,
			1, 1,-1, 1, 1,-1, 1 
		};

		for (int i = 0; i < FUZZTABLE; i++)
		{
			fuzzoffset[i] = fuzzinit[i] * fuzzoff;
		}
	}

	namespace
	{
		bool R_SetBlendFunc(int op, fixed_t fglevel, fixed_t bglevel, int flags)
		{
			using namespace drawerargs;

			// r_drawtrans is a seriously bad thing to turn off. I wonder if I should
			// just remove it completely.
			if (!r_drawtrans || (op == STYLEOP_Add && fglevel == FRACUNIT && bglevel == 0 && !(flags & STYLEF_InvertSource)))
			{
				if (flags & STYLEF_ColorIsFixed)
				{
					colfunc = &SWPixelFormatDrawers::FillColumn;
				}
				else if (dc_translation == NULL)
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
					else if (dc_translation == NULL)
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
					else if (dc_translation == NULL)
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
				else if (dc_translation == NULL)
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
				else if (dc_translation == NULL)
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

		fixed_t GetAlpha(int type, fixed_t alpha)
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

		FDynamicColormap *basecolormapsave;
	}

	bool R_SetPatchStyle(FRenderStyle style, fixed_t alpha, int translation, uint32_t color)
	{
		using namespace drawerargs;

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
			dc_translation = NULL;
			if (translation != 0)
			{
				FRemapTable *table = TranslationToTable(translation);
				if (table != NULL && !table->Inactive)
				{
					if (r_swtruecolor)
						dc_translation = (uint8_t*)table->Palette;
					else
						dc_translation = table->Remap;
				}
			}
		}
		basecolormapsave = basecolormap;

		// Check for special modes
		if (style.BlendOp == STYLEOP_Fuzz)
		{
			colfunc = fuzzcolfunc;
			return true;
		}
		else if (style == LegacyRenderStyles[STYLE_Shaded])
		{
			// Shaded drawer only gets 16 levels of alpha because it saves memory.
			if ((alpha >>= 12) == 0)
				return false;
			colfunc = &SWPixelFormatDrawers::DrawShadedColumn;
			drawer_needs_pal_input = true;
			dc_color = fixedcolormap ? fixedcolormap->Maps[APART(color)] : basecolormap->Maps[APART(color)];
			basecolormap = &ShadeFakeColormap[16 - alpha];
			if (fixedlightlev >= 0 && fixedcolormap == NULL)
			{
				R_SetColorMapLight(basecolormap, 0, FIXEDLIGHT2SHADE(fixedlightlev));
			}
			else
			{
				R_SetColorMapLight(basecolormap, 0, 0);
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
			R_SetColorMapLight(&identitycolormap, 0, 0);
		}

		if (!R_SetBlendFunc(style.BlendOp, fglevel, bglevel, style.Flags))
		{
			return false;
		}
		return true;
	}

	bool R_SetPatchStyle(FRenderStyle style, float alpha, int translation, uint32_t color)
	{
		return R_SetPatchStyle(style, FLOAT2FIXED(alpha), translation, color);
	}

	void R_FinishSetPatchStyle()
	{
		basecolormap = basecolormapsave;
	}

	DrawerFunc R_GetTransMaskDrawer()
	{
		if (colfunc == &SWPixelFormatDrawers::DrawAddColumn)
		{
			return &SWPixelFormatDrawers::DrawWallAddColumn;
		}
		if (colfunc == &SWPixelFormatDrawers::DrawAddClampColumn)
		{
			return &SWPixelFormatDrawers::DrawWallAddClampColumn;
		}
		if (colfunc == &SWPixelFormatDrawers::DrawSubClampColumn)
		{
			return &SWPixelFormatDrawers::DrawWallSubClampColumn;
		}
		if (colfunc == &SWPixelFormatDrawers::DrawRevSubClampColumn)
		{
			return &SWPixelFormatDrawers::DrawWallRevSubClampColumn;
		}
		return nullptr;
	}

	void R_SetColorMapLight(FSWColormap *base_colormap, float light, int shade)
	{
		using namespace drawerargs;

		dc_fcolormap = base_colormap;
		if (r_swtruecolor)
		{
			dc_shade_constants.light_red = dc_fcolormap->Color.r * 256 / 255;
			dc_shade_constants.light_green = dc_fcolormap->Color.g * 256 / 255;
			dc_shade_constants.light_blue = dc_fcolormap->Color.b * 256 / 255;
			dc_shade_constants.light_alpha = dc_fcolormap->Color.a * 256 / 255;
			dc_shade_constants.fade_red = dc_fcolormap->Fade.r;
			dc_shade_constants.fade_green = dc_fcolormap->Fade.g;
			dc_shade_constants.fade_blue = dc_fcolormap->Fade.b;
			dc_shade_constants.fade_alpha = dc_fcolormap->Fade.a;
			dc_shade_constants.desaturate = MIN(abs(dc_fcolormap->Desaturate), 255) * 255 / 256;
			dc_shade_constants.simple_shade = (dc_fcolormap->Color.d == 0x00ffffff && dc_fcolormap->Fade.d == 0x00000000 && dc_fcolormap->Desaturate == 0);
			dc_colormap = base_colormap->Maps;
			dc_light = LIGHTSCALE(light, shade);
		}
		else
		{
			dc_colormap = base_colormap->Maps + (GETPALOOKUP(light, shade) << COLORMAPSHIFT);
		}
	}

	void R_SetDSColorMapLight(FSWColormap *base_colormap, float light, int shade)
	{
		using namespace drawerargs;
	
		ds_fcolormap = base_colormap;
		if (r_swtruecolor)
		{
			ds_shade_constants.light_red = ds_fcolormap->Color.r * 256 / 255;
			ds_shade_constants.light_green = ds_fcolormap->Color.g * 256 / 255;
			ds_shade_constants.light_blue = ds_fcolormap->Color.b * 256 / 255;
			ds_shade_constants.light_alpha = ds_fcolormap->Color.a * 256 / 255;
			ds_shade_constants.fade_red = ds_fcolormap->Fade.r;
			ds_shade_constants.fade_green = ds_fcolormap->Fade.g;
			ds_shade_constants.fade_blue = ds_fcolormap->Fade.b;
			ds_shade_constants.fade_alpha = ds_fcolormap->Fade.a;
			ds_shade_constants.desaturate = MIN(abs(ds_fcolormap->Desaturate), 255) * 255 / 256;
			ds_shade_constants.simple_shade = (ds_fcolormap->Color.d == 0x00ffffff && ds_fcolormap->Fade.d == 0x00000000 && ds_fcolormap->Desaturate == 0);
			ds_colormap = base_colormap->Maps;
			ds_light = LIGHTSCALE(light, shade);
		}
		else
		{
			ds_colormap = base_colormap->Maps + (GETPALOOKUP(light, shade) << COLORMAPSHIFT);
		}
	}

	void R_SetTranslationMap(lighttable_t *translation)
	{
		using namespace drawerargs;

		if (r_swtruecolor)
		{
			dc_fcolormap = nullptr;
			dc_colormap = nullptr;
			dc_translation = translation;
			dc_shade_constants.light_red = 256;
			dc_shade_constants.light_green = 256;
			dc_shade_constants.light_blue = 256;
			dc_shade_constants.light_alpha = 256;
			dc_shade_constants.fade_red = 0;
			dc_shade_constants.fade_green = 0;
			dc_shade_constants.fade_blue = 0;
			dc_shade_constants.fade_alpha = 256;
			dc_shade_constants.desaturate = 0;
			dc_shade_constants.simple_shade = true;
			dc_light = 0;
		}
		else
		{
			dc_fcolormap = nullptr;
			dc_colormap = translation;
		}
	}

	void R_SetSpanTexture(FTexture *tex)
	{
		using namespace drawerargs;

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

	void R_SetSpanColormap(FDynamicColormap *colormap, int shade)
	{
		R_SetDSColorMapLight(colormap, 0, shade);
	}

	void R_UpdateFuzzPos()
	{
		using namespace drawerargs;

		dc_yl = MAX(dc_yl, 1);
		dc_yh = MIN(dc_yh, fuzzviewheight);
		if (dc_yl <= dc_yh)
			fuzzpos = (fuzzpos + dc_yh - dc_yl + 1) % FUZZTABLE;
	}

	void R_DrawMaskedColumn(int x, fixed_t iscale, FTexture *tex, fixed_t col, double spryscale, double sprtopscreen, bool sprflipvert, const short *mfloorclip, const short *mceilingclip, bool unmasked)
	{
		using namespace drawerargs;

		// Handle the linear filtered version in a different function to reduce chances of merge conflicts from zdoom.
		if (r_swtruecolor && !drawer_needs_pal_input) // To do: add support to R_DrawColumnHoriz_rgba
		{
			R_DrawMaskedColumnBgra(x, iscale, tex, col, spryscale, sprtopscreen, sprflipvert, mfloorclip, mceilingclip, unmasked);
			return;
		}

		dc_x = x;
		dc_iscale = iscale;

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
				dc_dest = (ylookup[dc_yl] + dc_x) * pixelsize + dc_destorg;
				dc_count = dc_yh - dc_yl + 1;

				fixed_t maxfrac = ((top + length) << FRACBITS) - 1;
				dc_texturefrac = MAX(dc_texturefrac, 0);
				dc_texturefrac = MIN(dc_texturefrac, maxfrac);
				if (dc_iscale > 0)
					dc_count = MIN(dc_count, (maxfrac - dc_texturefrac + dc_iscale - 1) / dc_iscale);
				else if (dc_iscale < 0)
					dc_count = MIN(dc_count, (dc_texturefrac - dc_iscale) / (-dc_iscale));

				(R_Drawers()->*colfunc)();
			}
			span++;
		}
	}

	void R_DrawMaskedColumnBgra(int x, fixed_t iscale, FTexture *tex, fixed_t col, double spryscale, double sprtopscreen, bool sprflipvert, const short *mfloorclip, const short *mceilingclip, bool unmasked)
	{
		using namespace drawerargs;

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
				dc_dest = (ylookup[dc_yl] + dc_x) * 4 + dc_destorg;
				dc_count = dc_yh - dc_yl + 1;

				double v = ((dc_yl + 0.5 - sprtopscreen) / spryscale) / tex->GetHeight();
				dc_texturefrac = (uint32_t)(v * (1 << 30));

				(R_Drawers()->*colfunc)();
			}
			span++;
		}
	}
}

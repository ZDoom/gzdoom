
#include <stddef.h>

#include "templates.h"
#include "doomdef.h"
#include "i_system.h"
#include "w_wad.h"
#include "r_local.h"
#include "v_video.h"
#include "doomstat.h"
#include "st_stuff.h"
#include "g_game.h"
#include "g_level.h"
#include "r_data/r_translate.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "r_plane.h"
#include "r_draw_tc.h"
#include "r_draw_rgba.h"
#include "r_draw_pal.h"
#include "r_thread.h"

namespace swrenderer
{
	// Needed by R_DrawFogBoundary (which probably shouldn't be part of this file)
	extern "C" short spanend[MAXHEIGHT];
	extern float rw_light;
	extern float rw_lightstep;
	extern int wallshade;

	double dc_texturemid;

	int ylookup[MAXHEIGHT];
	uint8_t shadetables[NUMCOLORMAPS * 16 * 256];
	FDynamicColormap ShadeFakeColormap[16];
	uint8_t identitymap[256];
	FDynamicColormap identitycolormap;
	int fuzzoffset[FUZZTABLE + 1];
	int fuzzpos;
	int fuzzviewheight;

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
		uint32_t vplce[4];
		uint32_t vince[4];
		uint8_t *palookupoffse[4];
		fixed_t palookuplight[4];
		const uint8_t *bufplce[4];
		const uint8_t *bufplce2[4];
		uint32_t buftexturefracx[4];
		uint32_t bufheight[4];
		int vlinebits;
		int mvlinebits;
		int tmvlinebits;
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

	void R_InitColumnDrawers()
	{
		colfunc = basecolfunc = R_DrawColumn;
		fuzzcolfunc = R_DrawFuzzColumn;
		transcolfunc = R_DrawTranslatedColumn;
		spanfunc = R_DrawSpan;
		hcolfunc_pre = R_DrawColumnHoriz;
		hcolfunc_post1 = rt_map1col;
		hcolfunc_post4 = rt_map4cols;
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
					colfunc = R_FillColumn;
					hcolfunc_post1 = rt_copy1col;
					hcolfunc_post4 = rt_copy4cols;
				}
				else if (dc_translation == NULL)
				{
					colfunc = basecolfunc;
					hcolfunc_post1 = rt_map1col;
					hcolfunc_post4 = rt_map4cols;
				}
				else
				{
					colfunc = transcolfunc;
					hcolfunc_post1 = rt_tlate1col;
					hcolfunc_post4 = rt_tlate4cols;
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
						colfunc = R_FillAddColumn;
						hcolfunc_post1 = rt_add1col;
						hcolfunc_post4 = rt_add4cols;
					}
					else if (dc_translation == NULL)
					{
						colfunc = R_DrawAddColumn;
						hcolfunc_post1 = rt_add1col;
						hcolfunc_post4 = rt_add4cols;
					}
					else
					{
						colfunc = R_DrawTlatedAddColumn;
						hcolfunc_post1 = rt_tlateadd1col;
						hcolfunc_post4 = rt_tlateadd4cols;
						drawer_needs_pal_input = true;
					}
				}
				else
				{ // Colors might overflow when added
					if (flags & STYLEF_ColorIsFixed)
					{
						colfunc = R_FillAddClampColumn;
						hcolfunc_post1 = rt_addclamp1col;
						hcolfunc_post4 = rt_addclamp4cols;
					}
					else if (dc_translation == NULL)
					{
						colfunc = R_DrawAddClampColumn;
						hcolfunc_post1 = rt_addclamp1col;
						hcolfunc_post4 = rt_addclamp4cols;
					}
					else
					{
						colfunc = R_DrawAddClampTranslatedColumn;
						hcolfunc_post1 = rt_tlateaddclamp1col;
						hcolfunc_post4 = rt_tlateaddclamp4cols;
						drawer_needs_pal_input = true;
					}
				}
				return true;

			case STYLEOP_Sub:
				if (flags & STYLEF_ColorIsFixed)
				{
					colfunc = R_FillSubClampColumn;
					hcolfunc_post1 = rt_subclamp1col;
					hcolfunc_post4 = rt_subclamp4cols;
				}
				else if (dc_translation == NULL)
				{
					colfunc = R_DrawSubClampColumn;
					hcolfunc_post1 = rt_subclamp1col;
					hcolfunc_post4 = rt_subclamp4cols;
				}
				else
				{
					colfunc = R_DrawSubClampTranslatedColumn;
					hcolfunc_post1 = rt_tlatesubclamp1col;
					hcolfunc_post4 = rt_tlatesubclamp4cols;
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
					colfunc = R_FillRevSubClampColumn;
					hcolfunc_post1 = rt_subclamp1col;
					hcolfunc_post4 = rt_subclamp4cols;
				}
				else if (dc_translation == NULL)
				{
					colfunc = R_DrawRevSubClampColumn;
					hcolfunc_post1 = rt_revsubclamp1col;
					hcolfunc_post4 = rt_revsubclamp4cols;
				}
				else
				{
					colfunc = R_DrawRevSubClampTranslatedColumn;
					hcolfunc_post1 = rt_tlaterevsubclamp1col;
					hcolfunc_post4 = rt_tlaterevsubclamp4cols;
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

	ESPSResult R_SetPatchStyle(FRenderStyle style, fixed_t alpha, int translation, uint32_t color)
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

		if (style.Flags & STYLEF_TransSoulsAlpha)
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
		hcolfunc_pre = R_DrawColumnHoriz;

		// Check for special modes
		if (style.BlendOp == STYLEOP_Fuzz)
		{
			colfunc = fuzzcolfunc;
			return DoDraw0;
		}
		else if (style == LegacyRenderStyles[STYLE_Shaded])
		{
			// Shaded drawer only gets 16 levels of alpha because it saves memory.
			if ((alpha >>= 12) == 0)
				return DontDraw;
			colfunc = R_DrawShadedColumn;
			hcolfunc_post1 = rt_shaded1col;
			hcolfunc_post4 = rt_shaded4cols;
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
			bool active_columnmethod = r_columnmethod && !r_swtruecolor;
			return active_columnmethod ? DoDraw1 : DoDraw0;
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
			dc_color = RGB32k.RGB[r >> 3][g >> 3][b >> 3];
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
			hcolfunc_pre = R_FillColumnHoriz;
			R_SetColorMapLight(&identitycolormap, 0, 0);
		}

		if (!R_SetBlendFunc(style.BlendOp, fglevel, bglevel, style.Flags))
		{
			return DontDraw;
		}
		bool active_columnmethod = r_columnmethod && !r_swtruecolor;
		return active_columnmethod ? DoDraw1 : DoDraw0;
	}

	ESPSResult R_SetPatchStyle(FRenderStyle style, float alpha, int translation, uint32_t color)
	{
		return R_SetPatchStyle(style, FLOAT2FIXED(alpha), translation, color);
	}

	void R_FinishSetPatchStyle()
	{
		basecolormap = basecolormapsave;
	}

	const uint8_t *R_GetColumn(FTexture *tex, int col)
	{
		int width;

		// If the texture's width isn't a power of 2, then we need to make it a
		// positive offset for proper clamping.
		if (col < 0 && (width = tex->GetWidth()) != (1 << tex->WidthBits))
		{
			col = width + (col % width);
		}

		if (r_swtruecolor)
			return (const uint8_t *)tex->GetColumnBgra(col, nullptr);
		else
			return tex->GetColumn(col, nullptr);
	}

	bool R_GetTransMaskDrawers(fixed_t(**tmvline1)(), void(**tmvline4)())
	{
		if (colfunc == R_DrawAddColumn)
		{
			*tmvline1 = tmvline1_add;
			*tmvline4 = tmvline4_add;
			return true;
		}
		if (colfunc == R_DrawAddClampColumn)
		{
			*tmvline1 = tmvline1_addclamp;
			*tmvline4 = tmvline4_addclamp;
			return true;
		}
		if (colfunc == R_DrawSubClampColumn)
		{
			*tmvline1 = tmvline1_subclamp;
			*tmvline4 = tmvline4_subclamp;
			return true;
		}
		if (colfunc == R_DrawRevSubClampColumn)
		{
			*tmvline1 = tmvline1_revsubclamp;
			*tmvline4 = tmvline4_revsubclamp;
			return true;
		}
		return false;
	}

	void setupvline(int fracbits)
	{
		drawerargs::vlinebits = fracbits;
	}

	void setupmvline(int fracbits)
	{
		drawerargs::mvlinebits = fracbits;
	}

	void setuptmvline(int fracbits)
	{
		drawerargs::tmvlinebits = fracbits;
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

	void rt_initcols(uint8_t *buffer)
	{
		using namespace drawerargs;

		for (int y = 3; y >= 0; y--)
			horizspan[y] = dc_ctspan[y] = &dc_tspans[y][0];

		DrawerCommandQueue::QueueCommand<RtInitColsRGBACommand>(buffer);
	}

	void rt_span_coverage(int x, int start, int stop)
	{
		using namespace drawerargs;

		unsigned int **tspan = &dc_ctspan[x & 3];
		(*tspan)[0] = start;
		(*tspan)[1] = stop;
		*tspan += 2;
	}

	void rt_flip_posts()
	{
		using namespace drawerargs;

		unsigned int *front = horizspan[dc_x & 3];
		unsigned int *back = dc_ctspan[dc_x & 3] - 2;

		while (front < back)
		{
			swapvalues(front[0], back[0]);
			swapvalues(front[1], back[1]);
			front += 2;
			back -= 2;
		}
	}

	void rt_draw4cols(int sx)
	{
		using namespace drawerargs;

		int x, bad;
		unsigned int maxtop, minbot, minnexttop;

		// Place a dummy "span" in each column. These don't get
		// drawn. They're just here to avoid special cases in the
		// max/min calculations below.
		for (x = 0; x < 4; ++x)
		{
			dc_ctspan[x][0] = screen->GetHeight()+1;
			dc_ctspan[x][1] = screen->GetHeight();
		}

		for (;;)
		{
			// If a column is out of spans, mark it as such
			bad = 0;
			minnexttop = 0xffffffff;
			for (x = 0; x < 4; ++x)
			{
				if (horizspan[x] >= dc_ctspan[x])
				{
					bad |= 1 << x;
				}
				else if ((horizspan[x]+2)[0] < minnexttop)
				{
					minnexttop = (horizspan[x]+2)[0];
				}
			}
			// Once all columns are out of spans, we're done
			if (bad == 15)
			{
				return;
			}

			// Find the largest shared area for the spans in each column
			maxtop = MAX (MAX (horizspan[0][0], horizspan[1][0]),
						  MAX (horizspan[2][0], horizspan[3][0]));
			minbot = MIN (MIN (horizspan[0][1], horizspan[1][1]),
						  MIN (horizspan[2][1], horizspan[3][1]));

			// If there is no shared area with these spans, draw each span
			// individually and advance to the next spans until we reach a shared area.
			// However, only draw spans down to the highest span in the next set of
			// spans. If we allow the entire height of a span to be drawn, it could
			// prevent any more shared areas from being drawn in these four columns.
			//
			// Example: Suppose we have the following arrangement:
			//			A CD
			//			A CD
			//			 B D
			//			 B D
			//			aB D
			//			aBcD
			//			aBcD
			//			aBc
			//
			// If we draw the entire height of the spans, we end up drawing this first:
			//			A CD
			//			A CD
			//			 B D
			//			 B D
			//			 B D
			//			 B D
			//			 B D
			//			 B D
			//			 B
			//
			// This leaves only the "a" and "c" columns to be drawn, and they are not
			// part of a shared area, but if we can include B and D with them, we can
			// get a shared area. So we cut off everything in the first set just
			// above the "a" column and end up drawing this first:
			//			A CD
			//			A CD
			//			 B D
			//			 B D
			//
			// Then the next time through, we have the following arrangement with an
			// easily shared area to draw:
			//			aB D
			//			aBcD
			//			aBcD
			//			aBc
			if (bad != 0 || maxtop > minbot)
			{
				int drawcount = 0;
				for (x = 0; x < 4; ++x)
				{
					if (!(bad & 1))
					{
						if (horizspan[x][1] < minnexttop)
						{
							hcolfunc_post1 (x, sx+x, horizspan[x][0], horizspan[x][1]);
							horizspan[x] += 2;
							drawcount++;
						}
						else if (minnexttop > horizspan[x][0])
						{
							hcolfunc_post1 (x, sx+x, horizspan[x][0], minnexttop-1);
							horizspan[x][0] = minnexttop;
							drawcount++;
						}
					}
					bad >>= 1;
				}
				// Drawcount *should* always be non-zero. The reality is that some situations
				// can make this not true. Unfortunately, I'm not sure what those situations are.
				if (drawcount == 0)
				{
					return;
				}
				continue;
			}

			// Draw any span fragments above the shared area.
			for (x = 0; x < 4; ++x)
			{
				if (maxtop > horizspan[x][0])
				{
					hcolfunc_post1 (x, sx+x, horizspan[x][0], maxtop-1);
				}
			}

			// Draw the shared area.
			hcolfunc_post4 (sx, maxtop, minbot);

			// For each column, if part of the span is past the shared area,
			// set its top to just below the shared area. Otherwise, advance
			// to the next span in that column.
			for (x = 0; x < 4; ++x)
			{
				if (minbot < horizspan[x][1])
				{
					horizspan[x][0] = minbot+1;
				}
				else
				{
					horizspan[x] += 2;
				}
			}
		}
	}

	void R_SetupSpanBits(FTexture *tex)
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
	}

	void R_SetSpanColormap(FDynamicColormap *colormap, int shade)
	{
		R_SetDSColorMapLight(colormap, 0, shade);
	}

	void R_SetSpanSource(FTexture *tex)
	{
		using namespace drawerargs;

		ds_source = r_swtruecolor ? (const uint8_t*)tex->GetPixelsBgra() : tex->GetPixels();
		ds_source_mipmapped = tex->Mipmapped() && tex->GetWidth() > 1 && tex->GetHeight() > 1;
	}

	/////////////////////////////////////////////////////////////////////////

	void R_FillColumnHoriz()
	{
		using namespace drawerargs;

		if (dc_count <= 0)
			return;

		int x = dc_x & 3;
		unsigned int **span = &dc_ctspan[x];
		(*span)[0] = dc_yl;
		(*span)[1] = dc_yh;
		*span += 2;

		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<FillColumnHorizRGBACommand>();
		else
			DrawerCommandQueue::QueueCommand<FillColumnHorizPalCommand>();
	}

	void R_DrawColumnHoriz()
	{
		using namespace drawerargs;

		if (dc_count <= 0)
			return;

		int x = dc_x & 3;
		unsigned int **span = &dc_ctspan[x];
		(*span)[0] = dc_yl;
		(*span)[1] = dc_yh;
		*span += 2;

		if (drawer_needs_pal_input || !r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnHorizRGBACommand<uint8_t>>();
		else
			DrawerCommandQueue::QueueCommand<DrawColumnHorizRGBACommand<uint32_t>>();
	}

	// Copies one span at hx to the screen at sx.
	void rt_copy1col(int hx, int sx, int yl, int yh)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnRt1CopyLLVMCommand>(hx, sx, yl, yh);
		else
			DrawerCommandQueue::QueueCommand<DrawColumnRt1CopyPalCommand>(hx, sx, yl, yh);
	}

	// Copies all four spans to the screen starting at sx.
	void rt_copy4cols(int sx, int yl, int yh)
	{
		// To do: we could do this with SSE using __m128i
		rt_copy1col(0, sx, yl, yh);
		rt_copy1col(1, sx + 1, yl, yh);
		rt_copy1col(2, sx + 2, yl, yh);
		rt_copy1col(3, sx + 3, yl, yh);
	}

	// Maps one span at hx to the screen at sx.
	void rt_map1col(int hx, int sx, int yl, int yh)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnRt1LLVMCommand>(hx, sx, yl, yh);
		else
			DrawerCommandQueue::QueueCommand<DrawColumnRt1PalCommand>(hx, sx, yl, yh);
	}

	// Maps all four spans to the screen starting at sx.
	void rt_map4cols(int sx, int yl, int yh)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnRt4LLVMCommand>(0, sx, yl, yh);
		else
			DrawerCommandQueue::QueueCommand<DrawColumnRt4PalCommand>(0, sx, yl, yh);
	}

	// Translates one span at hx to the screen at sx.
	void rt_tlate1col(int hx, int sx, int yl, int yh)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnRt1TranslatedLLVMCommand>(hx, sx, yl, yh);
		else
			DrawerCommandQueue::QueueCommand<DrawColumnRt1TranslatedPalCommand>(hx, sx, yl, yh);
	}

	// Translates all four spans to the screen starting at sx.
	void rt_tlate4cols(int sx, int yl, int yh)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnRt4TranslatedLLVMCommand>(0, sx, yl, yh);
		else
			DrawerCommandQueue::QueueCommand<DrawColumnRt4TranslatedPalCommand>(0, sx, yl, yh);
	}

	// Adds one span at hx to the screen at sx without clamping.
	void rt_add1col(int hx, int sx, int yl, int yh)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnRt1AddLLVMCommand>(hx, sx, yl, yh);
		else
			DrawerCommandQueue::QueueCommand<DrawColumnRt1AddPalCommand>(hx, sx, yl, yh);
	}

	// Adds all four spans to the screen starting at sx without clamping.
	void rt_add4cols(int sx, int yl, int yh)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnRt4AddLLVMCommand>(0, sx, yl, yh);
		else
			DrawerCommandQueue::QueueCommand<DrawColumnRt4AddPalCommand>(0, sx, yl, yh);
	}

	// Translates and adds one span at hx to the screen at sx without clamping.
	void rt_tlateadd1col(int hx, int sx, int yl, int yh)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnRt1AddClampTranslatedLLVMCommand>(hx, sx, yl, yh);
		else
			DrawerCommandQueue::QueueCommand<DrawColumnRt1AddTranslatedPalCommand>(hx, sx, yl, yh);
	}

	// Translates and adds all four spans to the screen starting at sx without clamping.
	void rt_tlateadd4cols(int sx, int yl, int yh)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnRt4AddClampTranslatedLLVMCommand>(0, sx, yl, yh);
		else
			DrawerCommandQueue::QueueCommand<DrawColumnRt4AddTranslatedPalCommand>(0, sx, yl, yh);
	}

	// Shades one span at hx to the screen at sx.
	void rt_shaded1col(int hx, int sx, int yl, int yh)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnRt1ShadedLLVMCommand>(hx, sx, yl, yh);
		else
			DrawerCommandQueue::QueueCommand<DrawColumnRt1ShadedPalCommand>(hx, sx, yl, yh);
	}

	// Shades all four spans to the screen starting at sx.
	void rt_shaded4cols(int sx, int yl, int yh)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnRt4ShadedLLVMCommand>(0, sx, yl, yh);
		else
			DrawerCommandQueue::QueueCommand<DrawColumnRt4ShadedPalCommand>(0, sx, yl, yh);
	}

	// Adds one span at hx to the screen at sx with clamping.
	void rt_addclamp1col(int hx, int sx, int yl, int yh)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnRt1AddClampLLVMCommand>(hx, sx, yl, yh);
		else
			DrawerCommandQueue::QueueCommand<DrawColumnRt1AddClampPalCommand>(hx, sx, yl, yh);
	}

	// Adds all four spans to the screen starting at sx with clamping.
	void rt_addclamp4cols(int sx, int yl, int yh)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnRt4AddClampLLVMCommand>(0, sx, yl, yh);
		else
			DrawerCommandQueue::QueueCommand<DrawColumnRt4AddClampPalCommand>(0, sx, yl, yh);
	}

	// Translates and adds one span at hx to the screen at sx with clamping.
	void rt_tlateaddclamp1col(int hx, int sx, int yl, int yh)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnRt1AddClampTranslatedLLVMCommand>(hx, sx, yl, yh);
		else
			DrawerCommandQueue::QueueCommand<DrawColumnRt1AddClampTranslatedPalCommand>(hx, sx, yl, yh);
	}

	// Translates and adds all four spans to the screen starting at sx with clamping.
	void rt_tlateaddclamp4cols(int sx, int yl, int yh)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnRt4AddClampTranslatedLLVMCommand>(0, sx, yl, yh);
		else
			DrawerCommandQueue::QueueCommand<DrawColumnRt4AddClampTranslatedPalCommand>(0, sx, yl, yh);
	}

	// Subtracts one span at hx to the screen at sx with clamping.
	void rt_subclamp1col(int hx, int sx, int yl, int yh)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnRt1SubClampLLVMCommand>(hx, sx, yl, yh);
		else
			DrawerCommandQueue::QueueCommand<DrawColumnRt1SubClampPalCommand>(hx, sx, yl, yh);
	}

	// Subtracts all four spans to the screen starting at sx with clamping.
	void rt_subclamp4cols(int sx, int yl, int yh)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnRt4SubClampLLVMCommand>(0, sx, yl, yh);
		else
			DrawerCommandQueue::QueueCommand<DrawColumnRt4SubClampPalCommand>(0, sx, yl, yh);
	}

	// Translates and subtracts one span at hx to the screen at sx with clamping.
	void rt_tlatesubclamp1col(int hx, int sx, int yl, int yh)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnRt1SubClampTranslatedLLVMCommand>(hx, sx, yl, yh);
		else
			DrawerCommandQueue::QueueCommand<DrawColumnRt1SubClampTranslatedPalCommand>(hx, sx, yl, yh);
	}

	// Translates and subtracts all four spans to the screen starting at sx with clamping.
	void rt_tlatesubclamp4cols(int sx, int yl, int yh)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnRt4SubClampTranslatedLLVMCommand>(0, sx, yl, yh);
		else
			DrawerCommandQueue::QueueCommand<DrawColumnRt4SubClampTranslatedPalCommand>(0, sx, yl, yh);
	}

	// Subtracts one span at hx from the screen at sx with clamping.
	void rt_revsubclamp1col(int hx, int sx, int yl, int yh)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnRt1RevSubClampLLVMCommand>(hx, sx, yl, yh);
		else
			DrawerCommandQueue::QueueCommand<DrawColumnRt1RevSubClampPalCommand>(hx, sx, yl, yh);
	}

	// Subtracts all four spans from the screen starting at sx with clamping.
	void rt_revsubclamp4cols(int sx, int yl, int yh)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnRt4RevSubClampLLVMCommand>(0, sx, yl, yh);
		else
			DrawerCommandQueue::QueueCommand<DrawColumnRt4RevSubClampPalCommand>(0, sx, yl, yh);
	}

	// Translates and subtracts one span at hx from the screen at sx with clamping.
	void rt_tlaterevsubclamp1col(int hx, int sx, int yl, int yh)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnRt1RevSubClampTranslatedLLVMCommand>(hx, sx, yl, yh);
		else
			DrawerCommandQueue::QueueCommand<DrawColumnRt1RevSubClampTranslatedPalCommand>(hx, sx, yl, yh);
	}

	// Translates and subtracts all four spans from the screen starting at sx with clamping.
	void rt_tlaterevsubclamp4cols(int sx, int yl, int yh)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnRt4RevSubClampTranslatedLLVMCommand>(0, sx, yl, yh);
		else
			DrawerCommandQueue::QueueCommand<DrawColumnRt4RevSubClampTranslatedPalCommand>(0, sx, yl, yh);
	}

	uint32_t vlinec1()
	{
		using namespace drawerargs;

		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawWall1LLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawWall1PalCommand>();

		return dc_texturefrac + dc_count * dc_iscale;
	}

	void vlinec4()
	{
		using namespace drawerargs;

		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawWall4LLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawWall4PalCommand>();

		for (int i = 0; i < 4; i++)
			vplce[i] += vince[i] * dc_count;
	}

	uint32_t mvlinec1()
	{
		using namespace drawerargs;

		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawWallMasked1LLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawWallMasked1PalCommand>();

		return dc_texturefrac + dc_count * dc_iscale;
	}

	void mvlinec4()
	{
		using namespace drawerargs;

		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawWallMasked4LLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawWallMasked4PalCommand>();

		for (int i = 0; i < 4; i++)
			vplce[i] += vince[i] * dc_count;
	}

	fixed_t tmvline1_add()
	{
		using namespace drawerargs;

		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawWallAdd1LLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawWallAdd1PalCommand>();

		return dc_texturefrac + dc_count * dc_iscale;
	}

	void tmvline4_add()
	{
		using namespace drawerargs;

		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawWallAdd4LLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawWallAdd4PalCommand>();

		for (int i = 0; i < 4; i++)
			vplce[i] += vince[i] * dc_count;
	}

	fixed_t tmvline1_addclamp()
	{
		using namespace drawerargs;

		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawWallAddClamp1LLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawWallAddClamp1PalCommand>();

		return dc_texturefrac + dc_count * dc_iscale;
	}

	void tmvline4_addclamp()
	{
		using namespace drawerargs;

		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawWallAddClamp4LLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawWallAddClamp4PalCommand>();

		for (int i = 0; i < 4; i++)
			vplce[i] += vince[i] * dc_count;
	}

	fixed_t tmvline1_subclamp()
	{
		using namespace drawerargs;

		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawWallSubClamp1LLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawWallSubClamp1PalCommand>();

		return dc_texturefrac + dc_count * dc_iscale;
	}

	void tmvline4_subclamp()
	{
		using namespace drawerargs;

		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawWallSubClamp4LLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawWallSubClamp4PalCommand>();

		for (int i = 0; i < 4; i++)
			vplce[i] += vince[i] * dc_count;
	}

	fixed_t tmvline1_revsubclamp()
	{
		using namespace drawerargs;

		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawWallRevSubClamp1LLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawWallRevSubClamp1PalCommand>();

		return dc_texturefrac + dc_count * dc_iscale;
	}

	void tmvline4_revsubclamp()
	{
		using namespace drawerargs;

		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawWallRevSubClamp4LLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawWallRevSubClamp4PalCommand>();

		for (int i = 0; i < 4; i++)
			vplce[i] += vince[i] * dc_count;
	}

	void R_DrawSingleSkyCol1(uint32_t solid_top, uint32_t solid_bottom)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawSingleSky1LLVMCommand>(solid_top, solid_bottom);
		else
			DrawerCommandQueue::QueueCommand<DrawSingleSky1PalCommand>(solid_top, solid_bottom);
	}

	void R_DrawSingleSkyCol4(uint32_t solid_top, uint32_t solid_bottom)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawSingleSky4LLVMCommand>(solid_top, solid_bottom);
		else
			DrawerCommandQueue::QueueCommand<DrawSingleSky4PalCommand>(solid_top, solid_bottom);
	}

	void R_DrawDoubleSkyCol1(uint32_t solid_top, uint32_t solid_bottom)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawDoubleSky1LLVMCommand>(solid_top, solid_bottom);
		else
			DrawerCommandQueue::QueueCommand<DrawDoubleSky1PalCommand>(solid_top, solid_bottom);
	}

	void R_DrawDoubleSkyCol4(uint32_t solid_top, uint32_t solid_bottom)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawDoubleSky4LLVMCommand>(solid_top, solid_bottom);
		else
			DrawerCommandQueue::QueueCommand<DrawDoubleSky4PalCommand>(solid_top, solid_bottom);
	}

	void R_DrawColumn()
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnLLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawColumnPalCommand>();
	}

	void R_FillColumn()
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<FillColumnLLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<FillColumnPalCommand>();
	}

	void R_FillAddColumn()
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<FillColumnAddLLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<FillColumnAddPalCommand>();
	}

	void R_FillAddClampColumn()
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<FillColumnAddClampLLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<FillColumnAddClampPalCommand>();
	}

	void R_FillSubClampColumn()
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<FillColumnSubClampLLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<FillColumnSubClampPalCommand>();
	}

	void R_FillRevSubClampColumn()
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<FillColumnRevSubClampLLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<FillColumnRevSubClampPalCommand>();
	}

	void R_DrawFuzzColumn()
	{
		using namespace drawerargs;

		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawFuzzColumnRGBACommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawFuzzColumnPalCommand>();

		dc_yl = MAX(dc_yl, 1);
		dc_yh = MIN(dc_yh, fuzzviewheight);
		if (dc_yl <= dc_yh)
			fuzzpos = (fuzzpos + dc_yh - dc_yl + 1) % FUZZTABLE;
	}

	void R_DrawAddColumn()
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnAddLLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawColumnAddPalCommand>();
	}

	void R_DrawTranslatedColumn()
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnTranslatedLLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawColumnTranslatedPalCommand>();
	}

	void R_DrawTlatedAddColumn()
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnTlatedAddLLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawColumnTlatedAddPalCommand>();
	}

	void R_DrawShadedColumn()
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnShadedLLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawColumnShadedPalCommand>();
	}

	void R_DrawAddClampColumn()
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnAddClampLLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawColumnAddClampPalCommand>();
	}

	void R_DrawAddClampTranslatedColumn()
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnAddClampTranslatedLLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawColumnAddClampTranslatedPalCommand>();
	}

	void R_DrawSubClampColumn()
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnSubClampLLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawColumnSubClampPalCommand>();
	}

	void R_DrawSubClampTranslatedColumn()
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnSubClampTranslatedLLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawColumnSubClampTranslatedPalCommand>();
	}

	void R_DrawRevSubClampColumn()
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnRevSubClampLLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawColumnRevSubClampPalCommand>();
	}

	void R_DrawRevSubClampTranslatedColumn()
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColumnRevSubClampTranslatedLLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawColumnRevSubClampTranslatedPalCommand>();
	}

	void R_DrawSpan()
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawSpanLLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawSpanPalCommand>();
	}

	void R_DrawSpanMasked()
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawSpanMaskedLLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawSpanMaskedPalCommand>();
	}

	void R_DrawSpanTranslucent()
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawSpanTranslucentLLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawSpanTranslucentPalCommand>();
	}

	void R_DrawSpanMaskedTranslucent()
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawSpanMaskedTranslucentLLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawSpanMaskedTranslucentPalCommand>();
	}

	void R_DrawSpanAddClamp()
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawSpanAddClampLLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawSpanAddClampPalCommand>();
	}

	void R_DrawSpanMaskedAddClamp()
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawSpanMaskedAddClampLLVMCommand>();
		else
			DrawerCommandQueue::QueueCommand<DrawSpanMaskedAddClampPalCommand>();
	}

	void R_FillSpan()
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<FillSpanRGBACommand>();
		else
			DrawerCommandQueue::QueueCommand<FillSpanPalCommand>();
	}

	void R_DrawTiltedSpan(int y, int x1, int x2, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawTiltedSpanRGBACommand>(y, x1, x2, plane_sz, plane_su, plane_sv, plane_shade, planeshade, planelightfloat, pviewx, pviewy);
		else
			DrawerCommandQueue::QueueCommand<DrawTiltedSpanPalCommand>(y, x1, x2, plane_sz, plane_su, plane_sv, plane_shade, planeshade, planelightfloat, pviewx, pviewy);
	}

	void R_DrawColoredSpan(int y, int x1, int x2)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawColoredSpanRGBACommand>(y, x1, x2);
		else
			DrawerCommandQueue::QueueCommand<DrawColoredSpanPalCommand>(y, x1, x2);
	}

	namespace
	{
		ShadeConstants slab_rgba_shade_constants;
		const uint8_t *slab_rgba_colormap;
		fixed_t slab_rgba_light;
	}

	void R_SetupDrawSlab(FSWColormap *base_colormap, float light, int shade)
	{
		slab_rgba_shade_constants.light_red = base_colormap->Color.r * 256 / 255;
		slab_rgba_shade_constants.light_green = base_colormap->Color.g * 256 / 255;
		slab_rgba_shade_constants.light_blue = base_colormap->Color.b * 256 / 255;
		slab_rgba_shade_constants.light_alpha = base_colormap->Color.a * 256 / 255;
		slab_rgba_shade_constants.fade_red = base_colormap->Fade.r;
		slab_rgba_shade_constants.fade_green = base_colormap->Fade.g;
		slab_rgba_shade_constants.fade_blue = base_colormap->Fade.b;
		slab_rgba_shade_constants.fade_alpha = base_colormap->Fade.a;
		slab_rgba_shade_constants.desaturate = MIN(abs(base_colormap->Desaturate), 255) * 255 / 256;
		slab_rgba_shade_constants.simple_shade = (base_colormap->Color.d == 0x00ffffff && base_colormap->Fade.d == 0x00000000 && base_colormap->Desaturate == 0);
		slab_rgba_colormap = base_colormap->Maps;
		slab_rgba_light = LIGHTSCALE(light, shade);
	}

	void R_DrawSlab(int dx, fixed_t v, int dy, fixed_t vi, const uint8_t *vptr, uint8_t *p)
	{
		if (r_swtruecolor)
			DrawerCommandQueue::QueueCommand<DrawSlabRGBACommand>(dx, v, dy, vi, vptr, p, slab_rgba_shade_constants, slab_rgba_colormap, slab_rgba_light);
		else
			DrawerCommandQueue::QueueCommand<DrawSlabPalCommand>(dx, v, dy, vi, vptr, p, slab_rgba_colormap);
	}

	void R_DrawFogBoundarySection(int y, int y2, int x1)
	{
		for (; y < y2; ++y)
		{
			int x2 = spanend[y];
			if (r_swtruecolor)
				DrawerCommandQueue::QueueCommand<DrawFogBoundaryLineRGBACommand>(y, x1, x2);
			else
				DrawerCommandQueue::QueueCommand<DrawFogBoundaryLinePalCommand>(y, x1, x2);
		}
	}

	void R_DrawFogBoundary(int x1, int x2, short *uclip, short *dclip)
	{
		// This is essentially the same as R_MapVisPlane but with an extra step
		// to create new horizontal spans whenever the light changes enough that
		// we need to use a new colormap.

		double lightstep = rw_lightstep;
		double light = rw_light + rw_lightstep*(x2 - x1 - 1);
		int x = x2 - 1;
		int t2 = uclip[x];
		int b2 = dclip[x];
		int rcolormap = GETPALOOKUP(light, wallshade);
		int lcolormap;
		uint8_t *basecolormapdata = basecolormap->Maps;

		if (b2 > t2)
		{
			clearbufshort(spanend + t2, b2 - t2, x);
		}

		R_SetColorMapLight(basecolormap, (float)light, wallshade);

		uint8_t *fake_dc_colormap = basecolormap->Maps + (GETPALOOKUP(light, wallshade) << COLORMAPSHIFT);

		for (--x; x >= x1; --x)
		{
			int t1 = uclip[x];
			int b1 = dclip[x];
			const int xr = x + 1;
			int stop;

			light -= rw_lightstep;
			lcolormap = GETPALOOKUP(light, wallshade);
			if (lcolormap != rcolormap)
			{
				if (t2 < b2 && rcolormap != 0)
				{ // Colormap 0 is always the identity map, so rendering it is
				  // just a waste of time.
					R_DrawFogBoundarySection(t2, b2, xr);
				}
				if (t1 < t2) t2 = t1;
				if (b1 > b2) b2 = b1;
				if (t2 < b2)
				{
					clearbufshort(spanend + t2, b2 - t2, x);
				}
				rcolormap = lcolormap;
				R_SetColorMapLight(basecolormap, (float)light, wallshade);
				fake_dc_colormap = basecolormap->Maps + (GETPALOOKUP(light, wallshade) << COLORMAPSHIFT);
			}
			else
			{
				if (fake_dc_colormap != basecolormapdata)
				{
					stop = MIN(t1, b2);
					while (t2 < stop)
					{
						int y = t2++;
						if (r_swtruecolor)
							DrawerCommandQueue::QueueCommand<DrawFogBoundaryLineRGBACommand>(y, xr, spanend[y]);
						else
							DrawerCommandQueue::QueueCommand<DrawFogBoundaryLinePalCommand>(y, xr, spanend[y]);
					}
					stop = MAX(b1, t2);
					while (b2 > stop)
					{
						int y = --b2;
						if (r_swtruecolor)
							DrawerCommandQueue::QueueCommand<DrawFogBoundaryLineRGBACommand>(y, xr, spanend[y]);
						else
							DrawerCommandQueue::QueueCommand<DrawFogBoundaryLinePalCommand>(y, xr, spanend[y]);
					}
				}
				else
				{
					t2 = MAX(t2, MIN(t1, b2));
					b2 = MIN(b2, MAX(b1, t2));
				}

				stop = MIN(t2, b1);
				while (t1 < stop)
				{
					spanend[t1++] = x;
				}
				stop = MAX(b2, t2);
				while (b1 > stop)
				{
					spanend[--b1] = x;
				}
			}

			t2 = uclip[x];
			b2 = dclip[x];
		}
		if (t2 < b2 && rcolormap != 0)
		{
			R_DrawFogBoundarySection(t2, b2, x1);
		}
	}

	void R_DrawParticle(vissprite_t *sprite)
	{
		if (r_swtruecolor)
			R_DrawParticle_rgba(sprite);
		else
			R_DrawParticle_C(sprite);
	}
}

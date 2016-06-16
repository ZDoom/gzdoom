// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//		The actual span/column drawing functions.
//		Here find the main potential for optimization,
//		 e.g. inline assembly, different algorithms.
//
//-----------------------------------------------------------------------------

#define DRAWER_INTERNALS

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
#include "r_draw_rgba.h"
#include "d_net.h"
#include "r_drawer_context.h"

#include "gi.h"
#include "stats.h"
#include "x86.h"

#ifdef X86_ASM
extern "C" void R_SetSpanSource_ASM (const BYTE *flat);
extern "C" void R_SetSpanSize_ASM (int xbits, int ybits);
extern "C" void R_SetSpanColormap_ASM (BYTE *colormap);
extern "C" void R_SetTiltedSpanSource_ASM(const BYTE *flat);
extern "C" BYTE *ds_curcolormap, *ds_cursource, *ds_curtiltedsource;
#endif

DCanvas *DrawerContext::Canvas()
{
	return dc_canvas;
}

uint8_t DrawerContext::FlatColor()
{
	return dc_color;
}

FColormap *DrawerContext::LightColormap()
{
	return dc_fcolormap;
}

fixed_t DrawerContext::TextureFrac()
{
	return dc_texturefrac;
}

fixed_t DrawerContext::TextureStep()
{
	return dc_iscale;
}

double DrawerContext::TextureMid()
{
	return dc_texturemid;
}

int DrawerContext::SpanXBits()
{
	return ds_xbits;
}

int DrawerContext::SpanYBits()
{
	return ds_ybits;
}

lighttable_t *DrawerContext::SpanLitColormap()
{
	return ds_colormap;
}

bool DrawerContext::IsFuzzColumn()
{
	return colfunc == fuzzcolfunc;
}

bool DrawerContext::IsFillColumn()
{
	return colfunc == R_FillColumn;
}

bool DrawerContext::IsBaseColumn()
{
	return colfunc == basecolfunc;
}

void DrawerContext::SetDest(int x, int y)
{
	int pixelsize = r_swtruecolor ? 4 : 1;
	dc_dest = dc_destorg + (ylookup[y] + x) * pixelsize;
}

void DrawerContext::SetFlatColor(uint8_t color_index)
{
	dc_color = color_index;
}

void DrawerContext::SetLight(FColormap *base_colormap, float light, int shade)
{
	R_SetColorMapLight(base_colormap, light, shade);
}

void DrawerContext::SetX(int x)
{
	dc_x = x;
}

void DrawerContext::SetY1(int y)
{
	dc_yl = y;
}

void DrawerContext::SetY2(int y)
{
	dc_yh = y;
}

void DrawerContext::SetSource(const BYTE *source)
{
	dc_source = source;
}

void DrawerContext::SetTextureFrac(fixed_t pos)
{
	dc_texturefrac = pos;
}

void DrawerContext::SetTextureStep(fixed_t step)
{
	dc_iscale = step;
}

void DrawerContext::SetTextureMid(double value)
{
	dc_texturemid = value;
}

void DrawerContext::SetDrawCount(int count)
{
	dc_count = count;
}

void DrawerContext::SetSpanY(int y)
{
	ds_y = y;
}

void DrawerContext::SetSpanX1(int x)
{
	ds_x1 = x;
}

void DrawerContext::SetSpanX2(int x)
{
	ds_x2 = x;
}

void DrawerContext::SetSpanXStep(dsfixed_t step)
{
	ds_xstep = step;
}

void DrawerContext::SetSpanYStep(dsfixed_t step)
{
	ds_ystep = step;
}

void DrawerContext::SetSpanXBits(int bits)
{
	ds_xbits = bits;
}

void DrawerContext::SetSpanYBits(int bits)
{
	ds_ybits = bits;
}

void DrawerContext::SetSpanXFrac(dsfixed_t frac)
{
	ds_xfrac = frac;
}

void DrawerContext::SetSpanYFrac(dsfixed_t frac)
{
	ds_yfrac = frac;
}

void DrawerContext::SetSpanLight(FColormap *base_colormap, float light, int shade)
{
	R_SetDSColorMapLight(base_colormap ? base_colormap : &identitycolormap, light, shade);
	R_SetSpanColormap();
}

ESPSResult DrawerContext::SetPatchStyle(FRenderStyle style, fixed_t alpha, int translation, DWORD color)
{
	return R_SetPatchStyle(style, alpha, translation, color);
}

ESPSResult DrawerContext::SetPatchStyle(FRenderStyle style, float alpha, int translation, DWORD color)
{
	return R_SetPatchStyle(style, FLOAT2FIXED(alpha), translation, color);
}

void DrawerContext::FinishSetPatchStyle()
{
	R_FinishSetPatchStyle();
}

void DrawerContext::SetCanvas(DCanvas *canvas)
{
	dc_canvas = canvas;
	dc_destorg = canvas->GetBuffer();

	if (r_swtruecolor != canvas->IsBgra())
	{
		r_swtruecolor = canvas->IsBgra();
		R_InitColumnDrawers();
	}
}

void DrawerContext::SetTranslationMap(lighttable_t *translation)
{
	R_SetTranslationMap(translation ? translation : identitymap);
}

void DrawerContext::SetSpanSource(FTexture *tex)
{
	R_SetupSpanBits(tex);
	if (r_swtruecolor)
		ds_source = (const BYTE*)tex->GetPixelsBgra();
	else
		ds_source = tex->GetPixels();

#ifdef X86_ASM
	if (!r_swtruecolor && ds_source != ds_cursource)
	{
		R_SetSpanSource_ASM (ds_source);
	}
	if (!r_swtruecolor)
	{
		if (ds_source != ds_curtiltedsource)
			R_SetTiltedSpanSource_ASM(ds_source);
	}
#endif
}

void DrawerContext::SetTiltedSpanState(FVector3 plane_sz, FVector3 plane_su, FVector3 plane_sv, bool plane_shade, float planelightfloat, fixed_t pviewx, fixed_t pviewy)
{
	ds_plane_sz = plane_sz;
	ds_plane_su = plane_su;
	ds_plane_sv = plane_sv;
	ds_plane_shade = plane_shade;
	ds_planelightfloat = planelightfloat;
	ds_pviewx = pviewx;
	ds_pviewy = pviewy;

	if (!plane_shade)
	{
		for (int i = 0; i < viewwidth; ++i)
		{
			tiltlighting[i] = DrawerContext::SpanLitColormap();
		}
	}
}

void DrawerContext::SetSlabLight(const BYTE *colormap)
{
	R_SetupDrawSlab(colormap);
}

void DrawerContext::DrawSlab(int dx, fixed_t v, int dy, fixed_t vi, const BYTE *source, int dest_x, int dest_y)
{
	int pixelsize = r_swtruecolor ? 4 : 1;
	R_DrawSlab(dx, v, dy, vi, source, (ylookup[dest_y] + dest_x) * pixelsize + dc_destorg);
}

void DrawerContext::SetSpanStyle(fixed_t alpha, bool additive, bool masked)
{
	if (spanfunc != R_FillSpan)
	{
		if (masked)
		{
			if (alpha < OPAQUE || additive)
			{
				if (!additive)
				{
					spanfunc = R_DrawSpanMaskedTranslucent;
					dc_srcblend = Col2RGB8[alpha >> 10];
					dc_destblend = Col2RGB8[(OPAQUE - alpha) >> 10];
					dc_srcalpha = alpha;
					dc_destalpha = OPAQUE - alpha;
				}
				else
				{
					spanfunc = R_DrawSpanMaskedAddClamp;
					dc_srcblend = Col2RGB8_LessPrecision[alpha >> 10];
					dc_destblend = Col2RGB8_LessPrecision[FRACUNIT >> 10];
					dc_srcalpha = alpha;
					dc_destalpha = OPAQUE - alpha;
				}
			}
			else
			{
				spanfunc = R_DrawSpanMasked;
			}
		}
		else
		{
			if (alpha < OPAQUE || additive)
			{
				if (!additive)
				{
					spanfunc = R_DrawSpanTranslucent;
					dc_srcblend = Col2RGB8[alpha >> 10];
					dc_destblend = Col2RGB8[(OPAQUE - alpha) >> 10];
					dc_srcalpha = alpha;
					dc_destalpha = OPAQUE - alpha;
				}
				else
				{
					spanfunc = R_DrawSpanAddClamp;
					dc_srcblend = Col2RGB8_LessPrecision[alpha >> 10];
					dc_destblend = Col2RGB8_LessPrecision[FRACUNIT >> 10];
					dc_srcalpha = alpha;
					dc_destalpha = OPAQUE - alpha;
				}
			}
			else
			{
				spanfunc = R_DrawSpan;
			}
		}
	}
}

void DrawerContext::RtInitCols(BYTE *buffer)
{
	rt_initcols(buffer);
}

void DrawerContext::RtSpanCoverage(int x, int start, int stop)
{
	rt_span_coverage(x, start, stop);
}

void DrawerContext::SetMaskedColumnState(short *mfloorclip, short *mceilingclip, double spryscale, double sprtopscreen, bool sprflipvert)
{
	dc_mfloorclip = mfloorclip;
	dc_mceilingclip = mceilingclip;
	dc_spryscale = spryscale;
	dc_sprtopscreen = sprtopscreen;
	dc_sprflipvert = sprflipvert;
}

void DrawerContext::DrawMaskedColumn(int x, const BYTE *column, const FTexture::Span *spans)
{
	R_DrawMaskedColumn(x, column, spans);
}

void DrawerContext::DrawMaskedColumnHoriz(int x, const BYTE *column, const FTexture::Span *spans)
{
	dc_x = x;
	R_DrawMaskedColumnHoriz(column, spans);
}

void DrawerContext::DrawFogBoundary(int x1, int x2, short *uclip, short *dclip)
{
	R_DrawFogBoundary(x1, x2, uclip, dclip);
}

void DrawerContext::DrawRt4cols(int sx)
{
	rt_draw4cols(sx);
}

void DrawerContext::DrawColumn()
{
	colfunc();
}

void DrawerContext::DrawSpan()
{
	spanfunc();
}

void DrawerContext::DrawHColumnPre()
{
	hcolfunc_pre();
}

void DrawerContext::DrawSimplePolySpan()
{
	R_DrawSpan();
}

void DrawerContext::SetBaseStyle()
{
	colfunc = basecolfunc;
	hcolfunc_post1 = rt_map1col;
	hcolfunc_post4 = rt_map4cols;
}

void DrawerContext::DrawWall(int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat, FTexture *rw_pic, fixed_t rw_offset, const BYTE *(*getcol)(FTexture *tex, int col))
{
	wallscan(x1, x2, uwal, dwal, swal, lwal, yrepeat, rw_pic, rw_offset, getcol ? getcol : R_GetColumn);
}

void DrawerContext::DrawMaskedWall(int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat, FTexture *rw_pic, fixed_t rw_offset, const BYTE *(*getcol)(FTexture *tex, int col))
{
	maskwallscan(x1, x2, uwal, dwal, swal, lwal, yrepeat, rw_pic, rw_offset, getcol ? getcol : R_GetColumn);
}

void DrawerContext::DrawTransMaskedWall(int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat, FTexture *rw_pic, fixed_t rw_offset, const BYTE *(*getcol)(FTexture *tex, int col))
{
	transmaskwallscan(x1, x2, uwal, dwal, swal, lwal, yrepeat, rw_pic, rw_offset, getcol ? getcol : R_GetColumn);
}

void DrawerContext::DrawColoredSpan(int y, int x1, int x2)
{
	R_DrawColoredSpan(y, x1, x2);
}

void DrawerContext::DrawTiltedSpan(int y, int x1, int x2)
{
	R_DrawTiltedSpan(y, x1, x2);
}

void DrawerContext::FillTransColumn(int x, int y1, int y2, int color, int alpha)
{
	R_FillTransColumn(x, y1, y2, color, alpha);
}

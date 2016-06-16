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
// DESCRIPTION:
//		System specific interface stuff.
//
//-----------------------------------------------------------------------------


#ifndef __R_DRAWER_CONTEXT__
#define __R_DRAWER_CONTEXT__

#include "r_defs.h"

// [RH] Consolidate column drawer selection
enum ESPSResult
{
	DontDraw,	// not useful to draw this
	DoDraw0,	// draw this as if r_columnmethod is 0
	DoDraw1,	// draw this as if r_columnmethod is 1
};

struct TiltedPlaneData;

// Immediate graphics context for column/span based software rendering.
class DrawerContext
{
public:
	static DCanvas *Canvas();						// dc_destorg

	static uint8_t FlatColor();						// dc_color
	static FColormap *LightColormap();				// dc_fcolormap
	static fixed_t TextureFrac();					// dc_texturefrac
	static fixed_t TextureStep();					// dc_iscale
	static double TextureMid();						// dc_texturemid

	static int SpanXBits();							// ds_xbits
	static int SpanYBits();							// ds_ybits
	static lighttable_t *SpanLitColormap();			// ds_colormap

	static bool IsFuzzColumn();						// colfunc == fuzzcolfunc
	static bool IsFillColumn();						// colfunc == R_FillColumn
	static bool IsBaseColumn();						// colfunc == basecolfunc

	static void SetCanvas(DCanvas *canvas);			// dc_destorg

	static void SetFlatColor(uint8_t color_index);	// dc_color
	static void SetLight(FColormap *base_colormap, float light, int shade);
	static void SetTranslationMap(lighttable_t *translation);
	static void SetX(int x);						// dc_x
	static void SetY1(int y);						// dc_yl
	static void SetY2(int y);						// dc_yh
	static void SetSource(const BYTE *source);		// dc_source
	static void SetTextureFrac(fixed_t pos);		// dc_texturefrac
	static void SetTextureStep(fixed_t step);		// dc_iscale
	static void SetTextureMid(double value);		// dc_texturemid
	static void SetDest(int x, int y);				// dc_dest
	static void SetDrawCount(int count);			// dc_count

	static void SetSpanY(int y);					// ds_y
	static void SetSpanX1(int x);					// ds_x1
	static void SetSpanX2(int x);					// ds_x2
	static void SetSpanXStep(dsfixed_t step);		// ds_xstep
	static void SetSpanYStep(dsfixed_t step);		// ds_ystep
	static void SetSpanXBits(int bits);				// ds_xbits
	static void SetSpanYBits(int bits);				// ds_ybits
	static void SetSpanXFrac(dsfixed_t frac);		// ds_xfrac
	static void SetSpanYFrac(dsfixed_t frac);		// ds_yfrac
	static void SetSpanLight(FColormap *base_colormap, float light, int shade);
	static void SetSpanSource(FTexture *texture);
	static void SetSpanStyle(fixed_t alpha, bool additive, bool masked);

	static void SetSlabLight(const BYTE *colormap);

	static ESPSResult SetPatchStyle(FRenderStyle style, fixed_t alpha, int translation, DWORD color);
	static ESPSResult SetPatchStyle(FRenderStyle style, float alpha, int translation, DWORD color);
	// Call this after finished drawing the current thing, in case its style was STYLE_Shade
	static void SetBaseStyle();
	static void FinishSetPatchStyle();

	static void SetMaskedColumnState(short *mfloorclip, short *mceilingclip, double spryscale, double sprtopscreen, bool sprflipvert);
	static void SetTiltedSpanState(FVector3 plane_sz, FVector3 plane_su, FVector3 plane_sv, bool plane_shade, float planelightfloat, fixed_t pviewx, fixed_t pviewy);

	static void RtInitCols(BYTE *buffer);
	static void RtSpanCoverage(int x, int start, int stop);

	static void DrawMaskedColumn(int x, const BYTE *column, const FTexture::Span *spans);
	static void DrawMaskedColumnHoriz(int x, const BYTE *column, const FTexture::Span *spans);

	static void DrawRt4cols(int sx);
	static void DrawColumn();
	static void DrawHColumnPre();
	static void DrawSpan();
	static void DrawSimplePolySpan();

	static void DrawWall(int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat, FTexture *texture, fixed_t texturefrac, const BYTE *(*getcol)(FTexture *tex, int col) = nullptr);
	static void DrawMaskedWall(int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat, FTexture *texture, fixed_t texturefrac, const BYTE *(*getcol)(FTexture *tex, int col) = nullptr);
	static void DrawTransMaskedWall(int x1, int x2, short *uwal, short *dwal, float *swal, fixed_t *lwal, double yrepeat, FTexture *texture, fixed_t texturefrac, const BYTE *(*getcol)(FTexture *tex, int col) = nullptr);

	static void DrawColoredSpan(int y, int x1, int x2);
	static void DrawTiltedSpan(int y, int x1, int x2);

	static void DrawSlab(int dx, fixed_t v, int dy, fixed_t vi, const BYTE *source, int dest_x, int dest_y);

	static void FillTransColumn(int x, int y1, int y2, int color, int alpha);

	static void DrawFogBoundary(int x1, int x2, short *uclip, short *dclip);
};

#endif

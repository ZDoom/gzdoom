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
//		Functions to draw patches (by post) directly to screen.
//		Functions to blit a block to the screen.
//
//-----------------------------------------------------------------------------


#ifndef __V_VIDEO_H__
#define __V_VIDEO_H__

#include "doomtype.h"
#include "m_bbox.h"

#include "v_palette.h"
#include "v_font.h"
#include "colormatcher.h"

#include "doomdef.h"

// Needed because we are refering to patches.
#include "r_data.h"

extern int CleanWidth, CleanHeight, CleanXfac, CleanYfac;
extern int DisplayWidth, DisplayHeight, DisplayBits;

//
// VIDEO
//
// [RH] Made screens more implementation-independant:
//
class DCanvas : public DObject
{
	DECLARE_ABSTRACT_CLASS (DCanvas, DObject)
public:
	enum EWrapperCode
	{
		EWrapper_Normal = 0,		// Just draws the patch as is
		EWrapper_Lucent = 1,		// Mixes the patch with the background
		EWrapper_Translated = 2,	// Color translates the patch and draws it
		EWrapper_TlatedLucent = 3,	// Translates the patch and mixes it with the background
		EWrapper_Colored = 4,		// Fills the patch area with a solid color
		EWrapper_ColoredLucent = 5,	// Mixes a solid color in the patch area with the background

		ETWrapper_Normal = 0,		// Normal text
		ETWrapper_Translucent = 1,	// Translucent text
		ETWrapper_Shadow = 2,		// Shadowed text
	};

	FFont *Font;

	DCanvas (int width, int height);
	virtual ~DCanvas ();

	// Member variable access
	inline BYTE *GetBuffer () const { return Buffer; }
	inline int GetWidth () const { return Width; }
	inline int GetHeight () const { return Height; }
	inline int GetPitch () const { return Pitch; }

	virtual bool IsValid ();

	// Access control
	virtual bool Lock () = 0;		// Returns true if the surface was lost since last time
	virtual void Unlock () = 0;

	// Copy blocks from one canvas to another
	virtual void Blit (int srcx, int srcy, int srcwidth, int srcheight, DCanvas *dest, int destx, int desty, int destwidth, int destheight);

	// Draw a linear block of pixels into the canvas
	virtual void DrawBlock (int x, int y, int width, int height, const byte *src) const;
	virtual void DrawPageBlock (const byte *src) const;

	// Draw a masked, translated block of pixels (e.g. chars stored in an FFont)
	virtual void DrawMaskedBlock (int x, int y, int width, int height, const byte *src, const byte *colors) const;
	virtual void ScaleMaskedBlock (int x, int y, int width, int height, int dwidth, int dheight, const byte *src, const byte *colors) const;
	virtual void DrawTranslucentMaskedBlock (int x, int y, int width, int height, const byte *src, const byte *colors, fixed_t fade) const;
	virtual void ScaleTranslucentMaskedBlock (int x, int y, int width, int height, int dwidth, int dheight, const byte *src, const byte *colors, fixed_t fade) const;
	virtual void DrawShadowedMaskedBlock (int x, int y, int width, int height, const byte *src, const byte *colors, fixed_t shade) const;
	virtual void ScaleShadowedMaskedBlock (int x, int y, int width, int height, int dwidth, int dheight, const byte *src, const byte *colors, fixed_t shade) const;
	virtual void DrawAlphaMaskedBlock (int x, int y, int width, int height, const byte *src, int color) const;
	virtual void ScaleAlphaMaskedBlock (int x, int y, int width, int height, int dwidth, int dheight, const byte *src, int color) const;

	// Reads a linear block of pixels into the view buffer.
	virtual void GetBlock (int x, int y, int width, int height, byte *dest) const;

	// Darken the entire canvas
	virtual void Dim () const;

	// Fill an area with a 64x64 flat texture
	virtual void FlatFill (int left, int top, int right, int bottom, const byte *src) const;

	// Set an area to a specified color
	virtual void Clear (int left, int top, int right, int bottom, int color) const;

	// Calculate gamma table
	void CalcGamma (float gamma, BYTE gammalookup[256]);

	// Text drawing functions -----------------------------------------------

	virtual void SetFont (FFont *font);

	// Return width of string in pixels (unscaled)
	int StringWidth (const byte *str) const;
	inline int StringWidth (const char *str) const { return StringWidth ((const byte *)str); }

	// Output some text with the current font
	inline void DrawText (int normalcolor, int x, int y, const byte *string) const;
	inline void DrawTextLuc (int normalcolor, int x, int y, const byte *string, fixed_t trans=0x8000) const;
	inline void DrawTextClean (int normalcolor, int x, int y, const byte *string) const;		// Does not adjust x and y
	inline void DrawTextCleanLuc (int normalcolor, int x, int y, const byte *string, fixed_t trans=0x8000) const; // ditto
	inline void DrawTextCleanMove (int normalcolor, int x, int y, const byte *string) const;	// This one does
	inline void DrawTextShadow (int normalcolor, int x, int y, const byte *string) const;
	inline void DrawTextCleanShadow (int normalcolor, int x, int y, const byte *string) const;
	inline void DrawTextCleanShadowMove (int normalcolor, int x, int y, const byte *string) const;

	inline void DrawText (int normalcolor, int x, int y, const char *string) const;
	inline void DrawTextLuc (int normalcolor, int x, int y, const char *string, fixed_t trans=0x8000) const;
	inline void DrawTextClean (int normalcolor, int x, int y, const char *string) const;
	inline void DrawTextCleanLuc (int normalcolor, int x, int y, const char *string, fixed_t trans=0x8000) const;
	inline void DrawTextCleanMove (int normalcolor, int x, int y, const char *string) const;
	inline void DrawTextShadow (int normalcolor, int x, int y, const char *string) const;
	inline void DrawTextCleanShadow (int normalcolor, int x, int y, const char *string) const;
	inline void DrawTextCleanShadowMove (int normalcolor, int x, int y, const char *string) const;

	inline void DrawChar (int normalcolor, int x, int y, byte character) const;
	inline void DrawCharLuc (int normalcolor, int x, int y, byte character, fixed_t trans=0x8000) const;
	inline void DrawCharClean (int normalcolor, int x, int y, byte character) const;
	inline void DrawCharCleanLuc (int normalcolor, int x, int y, byte character, fixed_t trans=0x8000) const;
	inline void DrawCharCleanMove (int normalcolor, int x, int y, byte character) const;
	inline void DrawCharShadow (int normalcolor, int x, int y, byte character) const;
	inline void DrawCharCleanShadow (int normalcolor, int x, int y, byte character) const;
	inline void DrawCharCleanShadowMove (int normalcolor, int x, int y, byte character) const;


	// Patch drawing functions
	void DrawPatchFlipped (const patch_t *patch, int x, int y) const;

	inline void DrawPatch (const patch_t *patch, int x, int y) const;
	inline void DrawPatchStretched (const patch_t *patch, int x, int y, int dw, int dh) const;
	inline void DrawPatchDirect (const patch_t *patch, int x, int y) const;
	inline void DrawPatchIndirect (const patch_t *patch, int x, int y) const;
	inline void DrawPatchClean (const patch_t *patch, int x, int y) const;
	inline void DrawPatchCleanNoMove (const patch_t *patch, int x, int y) const;

	inline void DrawLucentPatch (const patch_t *patch, int x, int y) const;
	inline void DrawLucentPatchStretched (const patch_t *patch, int x, int y, int dw, int dh) const;
	inline void DrawLucentPatchDirect (const patch_t *patch, int x, int y) const;
	inline void DrawLucentPatchIndirect (const patch_t *patch, int x, int y) const;
	inline void DrawLucentPatchClean (const patch_t *patch, int x, int y) const;
	inline void DrawLucentPatchCleanNoMove (const patch_t *patch, int x, int y) const;

	inline void DrawTranslatedPatch (const patch_t *patch, int x, int y) const;
	inline void DrawTranslatedPatchStretched (const patch_t *patch, int x, int y, int dw, int dh) const;
	inline void DrawTranslatedPatchDirect (const patch_t *patch, int x, int y) const;
	inline void DrawTranslatedPatchIndirect (const patch_t *patch, int x, int y) const;
	inline void DrawTranslatedPatchClean (const patch_t *patch, int x, int y) const;
	inline void DrawTranslatedPatchCleanNoMove (const patch_t *patch, int x, int y) const;

	inline void DrawTranslatedLucentPatch (const patch_t *patch, int x, int y) const;
	inline void DrawTranslatedLucentPatchStretched (const patch_t *patch, int x, int y, int dw, int dh) const;
	inline void DrawTranslatedLucentPatchDirect (const patch_t *patch, int x, int y) const;
	inline void DrawTranslatedLucentPatchIndirect (const patch_t *patch, int x, int y) const;
	inline void DrawTranslatedLucentPatchClean (const patch_t *patch, int x, int y) const;
	inline void DrawTranslatedLucentPatchCleanNoMove (const patch_t *patch, int x, int y) const;

	inline void DrawColoredPatch (const patch_t *patch, int x, int y) const;
	inline void DrawColoredPatchStretched (const patch_t *patch, int x, int y, int dw, int dh) const;
	inline void DrawColoredPatchDirect (const patch_t *patch, int x, int y) const;
	inline void DrawColoredPatchIndirect (const patch_t *patch, int x, int y) const;
	inline void DrawColoredPatchClean (const patch_t *patch, int x, int y) const;
	inline void DrawColoredPatchCleanNoMove (const patch_t *patch, int x, int y) const;

	inline void DrawColoredLucentPatch (const patch_t *patch, int x, int y) const;
	inline void DrawColoredLucentPatchStretched (const patch_t *patch, int x, int y, int dw, int dh) const;
	inline void DrawColoredLucentPatchDirect (const patch_t *patch, int x, int y) const;
	inline void DrawColoredLucentPatchIndirect (const patch_t *patch, int x, int y) const;
	inline void DrawColoredLucentPatchClean (const patch_t *patch, int x, int y) const;
	inline void DrawColoredLucentPatchCleanNoMove (const patch_t *patch, int x, int y) const;

	inline void DrawShadowedPatch (const patch_t *patch, int x, int y) const;
	inline void DrawShadowedPatchStretched (const patch_t *patch, int x, int y, int dw, int dh) const;
	inline void DrawShadowedPatchDirect (const patch_t *patch, int x, int y) const;
	inline void DrawShadowedPatchIndirect (const patch_t *patch, int x, int y) const;
	inline void DrawShadowedPatchClean (const patch_t *patch, int x, int y) const;
	inline void DrawShadowedPatchCleanNoMove (const patch_t *patch, int x, int y) const;

protected:
	BYTE *Buffer;
	int Width;
	int Height;
	int Pitch;
	int LockCount;

	virtual void TextWrapper (EWrapperCode drawer, int normalcolor, int x, int y, const byte *string) const;
	virtual void TextSWrapper (EWrapperCode drawer, int normalcolor, int x, int y, const byte *string) const;

	virtual void CharWrapper (EWrapperCode drawer, int normalcolor, int x, int y, byte character) const;
	virtual void CharSWrapper (EWrapperCode drawer, int normalcolor, int x, int y, byte character) const;

	virtual void DrawWrapper (EWrapperCode drawer, const patch_t *patch, int x, int y) const;
	virtual void DrawSWrapper (EWrapperCode drawer, const patch_t *patch, int x, int y, int destwidth, int destheight) const;
	virtual void DrawIWrapper (EWrapperCode drawer, const patch_t *patch, int x, int y) const;
	virtual void DrawCWrapper (EWrapperCode drawer, const patch_t *patch, int x, int y) const;
	virtual void DrawCNMWrapper (EWrapperCode drawer, const patch_t *patch, int x, int y) const;

	bool ClipBox (int &left, int &top, int &width, int &height, const byte *&src, const int srcpitch) const;
	bool ClipScaleBox (int &left, int &top, int &width, int &height, int &dwidth, int &dheight, const byte *&src, const int srcpitch, fixed_t &xinc, fixed_t &yinc, fixed_t &xstart, fixed_t &yerr) const;

	static void DrawPatchP (const byte *source, byte *dest, int count, int pitch);
	static void DrawLucentPatchP (const byte *source, byte *dest, int count, int pitch);
	static void DrawTranslatedPatchP (const byte *source, byte *dest, int count, int pitch);
	static void DrawTlatedLucentPatchP (const byte *source, byte *dest, int count, int pitch);
	static void DrawColoredPatchP (const byte *source, byte *dest, int count, int pitch);
	static void DrawColorLucentPatchP (const byte *source, byte *dest, int count, int pitch);

	static void DrawPatchSP (const byte *source, byte *dest, int count, int pitch, int yinc);
	static void DrawLucentPatchSP (const byte *source, byte *dest, int count, int pitch, int yinc);
	static void DrawTranslatedPatchSP (const byte *source, byte *dest, int count, int pitch, int yinc);
	static void DrawTlatedLucentPatchSP (const byte *source, byte *dest, int count, int pitch, int yinc);
	static void DrawColoredPatchSP (const byte *source, byte *dest, int count, int pitch, int yinc);
	static void DrawColorLucentPatchSP (const byte *source, byte *dest, int count, int pitch, int yinc);

	static void DrawPatchD (const byte *source, byte *dest, int count, int pitch);
	static void DrawLucentPatchD (const byte *source, byte *dest, int count, int pitch);
	static void DrawTranslatedPatchD (const byte *source, byte *dest, int count, int pitch);
	static void DrawTlatedLucentPatchD (const byte *source, byte *dest, int count, int pitch);
	static void DrawColoredPatchD (const byte *source, byte *dest, int count, int pitch);
	static void DrawColorLucentPatchD (const byte *source, byte *dest, int count, int pitch);

	static void DrawPatchSD (const byte *source, byte *dest, int count, int pitch, int yinc);
	static void DrawLucentPatchSD (const byte *source, byte *dest, int count, int pitch, int yinc);
	static void DrawTranslatedPatchSD (const byte *source, byte *dest, int count, int pitch, int yinc);
	static void DrawTlatedLucentPatchSD (const byte *source, byte *dest, int count, int pitch, int yinc);

	typedef void (*vdrawfunc) (const byte *source, byte *dest, int count, int pitch);
	typedef void (*vdrawsfunc) (const byte *source, byte *dest, int count, int pitch, int yinc);

	// Palettized versions of the column drawers
	static vdrawfunc Pfuncs[6];
	static vdrawsfunc Psfuncs[6];

	// The current set of column drawers (set in V_SetResolution)
	static vdrawfunc *m_Drawfuncs;
	static vdrawsfunc *m_Drawsfuncs;

private:
	// Keep track of canvases, for automatic destruction at exit
	DCanvas *Next;
	static DCanvas *CanvasChain;

	friend void STACK_ARGS FreeCanvasChain ();
};

// The color to fill with for #4 and #5 above
extern int V_ColorFill;

// Fade amount used by the translucent patch drawers
extern fixed_t V_Fade;

// The color map for #1 and #2 above
extern byte *V_ColorMap;

extern fixed_t V_TextTrans;

inline void DCanvas::DrawText (int normalcolor, int x, int y, const byte *string) const
{
	TextWrapper (ETWrapper_Normal, normalcolor, x, y, string);
}
inline void DCanvas::DrawTextLuc (int normalcolor, int x, int y, const byte *string, fixed_t trans) const
{
	V_TextTrans = trans;
	TextWrapper (ETWrapper_Translucent, normalcolor, x, y, string);
}
inline void DCanvas::DrawTextClean (int normalcolor, int x, int y, const byte *string) const
{
	TextSWrapper (ETWrapper_Normal, normalcolor, x, y, string);
}
inline void DCanvas::DrawTextCleanLuc (int normalcolor, int x, int y, const byte *string, fixed_t trans) const
{
	V_TextTrans = trans;
	TextSWrapper (ETWrapper_Translucent, normalcolor, x, y, string);
}
inline void DCanvas::DrawTextCleanMove (int normalcolor, int x, int y, const byte *string) const
{
	TextSWrapper (ETWrapper_Normal, normalcolor,
		(x - 160) * CleanXfac + Width / 2,
		(y - 100) * CleanYfac + Height / 2,
		string);
}
inline void DCanvas::DrawTextShadow (int normalcolor, int x, int y, const byte *string) const
{
	TextWrapper (ETWrapper_Shadow, normalcolor, x, y, string);
}
inline void DCanvas::DrawTextCleanShadow (int normalcolor, int x, int y, const byte *string) const
{
	TextSWrapper (ETWrapper_Shadow, normalcolor, x, y, string);
}
inline void DCanvas::DrawTextCleanShadowMove (int normalcolor, int x, int y, const byte *string) const
{
	TextSWrapper (ETWrapper_Shadow, normalcolor,
		(x - 160) * CleanXfac + Width / 2,
		(y - 100) * CleanYfac + Height / 2,
		string);
}

inline void DCanvas::DrawText (int normalcolor, int x, int y, const char *string) const
{
	TextWrapper (ETWrapper_Normal, normalcolor, x, y, (const byte *)string);
}
inline void DCanvas::DrawTextLuc (int normalcolor, int x, int y, const char *string, fixed_t trans) const
{
	V_TextTrans = trans;
	TextWrapper (ETWrapper_Translucent, normalcolor, x, y, (const byte *)string);
}
inline void DCanvas::DrawTextClean (int normalcolor, int x, int y, const char *string) const
{
	TextSWrapper (ETWrapper_Normal, normalcolor, x, y, (const byte *)string);
}
inline void DCanvas::DrawTextCleanLuc (int normalcolor, int x, int y, const char *string, fixed_t trans) const
{
	V_TextTrans = trans;
	TextSWrapper (ETWrapper_Translucent, normalcolor, x, y, (const byte *)string);
}
inline void DCanvas::DrawTextCleanMove (int normalcolor, int x, int y, const char *string) const
{
	TextSWrapper (ETWrapper_Normal, normalcolor,
		(x - 160) * CleanXfac + Width / 2,
		(y - 100) * CleanYfac + Height / 2,
		(const byte *)string);
}
inline void DCanvas::DrawTextShadow (int normalcolor, int x, int y, const char *string) const
{
	TextWrapper (ETWrapper_Shadow, normalcolor, x, y, (const byte *)string);
}
inline void DCanvas::DrawTextCleanShadow (int normalcolor, int x, int y, const char *string) const
{
	TextSWrapper (ETWrapper_Shadow, normalcolor, x, y, (const byte *)string);
}
inline void DCanvas::DrawTextCleanShadowMove (int normalcolor, int x, int y, const char *string) const
{
	TextSWrapper (ETWrapper_Shadow, normalcolor,
		(x - 160) * CleanXfac + Width / 2,
		(y - 100) * CleanYfac + Height / 2,
		(const byte *)string);
}

inline void DCanvas::DrawChar (int normalcolor, int x, int y, byte character) const
{
	CharWrapper (ETWrapper_Normal, normalcolor, x, y, character);
}
inline void DCanvas::DrawCharLuc (int normalcolor, int x, int y, byte character, fixed_t trans) const
{
	V_TextTrans = trans;
	CharWrapper (ETWrapper_Translucent, normalcolor, x, y, character);
}
inline void DCanvas::DrawCharClean (int normalcolor, int x, int y, byte character) const
{
	CharSWrapper (ETWrapper_Normal, normalcolor, x, y, character);
}
inline void DCanvas::DrawCharCleanLuc (int normalcolor, int x, int y, byte character, fixed_t trans) const
{
	V_TextTrans = trans;
	CharSWrapper (ETWrapper_Translucent, normalcolor, x, y, character);
}
inline void DCanvas::DrawCharCleanMove (int normalcolor, int x, int y, byte character) const
{
	CharSWrapper (ETWrapper_Normal, normalcolor,
		(x - 160) * CleanXfac + Width / 2,
		(y - 100) * CleanYfac + Height / 2,
		character);
}
inline void DCanvas::DrawCharShadow (int normalcolor, int x, int y, byte character) const
{
	CharWrapper (ETWrapper_Shadow, normalcolor, x, y, character);
}
inline void DCanvas::DrawCharCleanShadow (int normalcolor, int x, int y, byte character) const
{
	CharSWrapper (ETWrapper_Shadow, normalcolor, x, y, character);
}
inline void DCanvas::DrawCharCleanShadowMove (int normalcolor, int x, int y, byte character) const
{
	CharSWrapper (ETWrapper_Shadow, normalcolor,
		(x - 160) * CleanXfac + Width / 2,
		(y - 100) * CleanYfac + Height / 2,
		character);
}

inline void DCanvas::DrawPatch (const patch_t *patch, int x, int y) const
{
	DrawWrapper (EWrapper_Normal, patch, x, y);
}
inline void DCanvas::DrawPatchStretched (const patch_t *patch, int x, int y, int dw, int dh) const
{
	DrawSWrapper (EWrapper_Normal, patch, x, y, dw, dh);
}
inline void DCanvas::DrawPatchDirect (const patch_t *patch, int x, int y) const
{
	DrawWrapper (EWrapper_Normal, patch, x, y);
}
inline void DCanvas::DrawPatchIndirect (const patch_t *patch, int x, int y) const
{
	DrawIWrapper (EWrapper_Normal, patch, x, y);
}
inline void DCanvas::DrawPatchClean (const patch_t *patch, int x, int y) const
{
	DrawCWrapper (EWrapper_Normal, patch, x, y);
}
inline void DCanvas::DrawPatchCleanNoMove (const patch_t *patch, int x, int y) const
{
	DrawCNMWrapper (EWrapper_Normal, patch, x, y);
}

inline void DCanvas::DrawLucentPatch (const patch_t *patch, int x, int y) const
{
	DrawWrapper (EWrapper_Lucent, patch, x, y);
}
inline void DCanvas::DrawLucentPatchStretched (const patch_t *patch, int x, int y, int dw, int dh) const
{
	DrawSWrapper (EWrapper_Lucent, patch, x, y, dw, dh);
}
inline void DCanvas::DrawLucentPatchDirect (const patch_t *patch, int x, int y) const
{
	DrawWrapper (EWrapper_Lucent, patch, x, y);
}
inline void DCanvas::DrawLucentPatchIndirect (const patch_t *patch, int x, int y) const
{
	DrawIWrapper (EWrapper_Lucent, patch, x, y);
}
inline void DCanvas::DrawLucentPatchClean (const patch_t *patch, int x, int y) const
{
	DrawCWrapper (EWrapper_Lucent, patch, x, y);
}
inline void DCanvas::DrawLucentPatchCleanNoMove (const patch_t *patch, int x, int y) const
{
	DrawCNMWrapper (EWrapper_Lucent, patch, x, y);
}

inline void DCanvas::DrawTranslatedPatch (const patch_t *patch, int x, int y) const
{
	DrawWrapper (EWrapper_Translated, patch, x, y);
}
inline void DCanvas::DrawTranslatedPatchStretched (const patch_t *patch, int x, int y, int dw, int dh) const
{
	DrawSWrapper (EWrapper_Translated, patch, x, y, dw, dh);
}
inline void DCanvas::DrawTranslatedPatchDirect (const patch_t *patch, int x, int y) const
{
	DrawWrapper (EWrapper_Translated, patch, x, y);
}
inline void DCanvas::DrawTranslatedPatchIndirect (const patch_t *patch, int x, int y) const
{
	DrawIWrapper (EWrapper_Translated, patch, x, y);
}
inline void DCanvas::DrawTranslatedPatchClean (const patch_t *patch, int x, int y) const
{
	DrawCWrapper (EWrapper_Translated, patch, x, y);
}
inline void DCanvas::DrawTranslatedPatchCleanNoMove (const patch_t *patch, int x, int y) const
{
	DrawCNMWrapper (EWrapper_Translated, patch, x, y);
}

inline void DCanvas::DrawTranslatedLucentPatch (const patch_t *patch, int x, int y) const
{
	DrawWrapper (EWrapper_TlatedLucent, patch, x, y);
}
inline void DCanvas::DrawTranslatedLucentPatchStretched (const patch_t *patch, int x, int y, int dw, int dh) const
{
	DrawSWrapper (EWrapper_TlatedLucent, patch, x, y, dw, dh);
}
inline void DCanvas::DrawTranslatedLucentPatchDirect (const patch_t *patch, int x, int y) const
{
	DrawWrapper (EWrapper_TlatedLucent, patch, x, y);
}
inline void DCanvas::DrawTranslatedLucentPatchIndirect (const patch_t *patch, int x, int y) const
{
	DrawIWrapper (EWrapper_TlatedLucent, patch, x, y);
}
inline void DCanvas::DrawTranslatedLucentPatchClean (const patch_t *patch, int x, int y) const
{
	DrawCWrapper (EWrapper_TlatedLucent, patch, x, y);
}
inline void DCanvas::DrawTranslatedLucentPatchCleanNoMove (const patch_t *patch, int x, int y) const
{
	DrawCNMWrapper (EWrapper_TlatedLucent, patch, x, y);
}

inline void DCanvas::DrawColoredPatch (const patch_t *patch, int x, int y) const
{
	DrawWrapper (EWrapper_Colored, patch, x, y);
}
inline void DCanvas::DrawColoredPatchStretched (const patch_t *patch, int x, int y, int dw, int dh) const
{
	DrawSWrapper (EWrapper_Colored, patch, x, y, dw, dh);
}
inline void DCanvas::DrawColoredPatchDirect (const patch_t *patch, int x, int y) const
{
	DrawWrapper (EWrapper_Colored, patch, x, y);
}
inline void DCanvas::DrawColoredPatchIndirect (const patch_t *patch, int x, int y) const
{
	DrawIWrapper (EWrapper_Colored, patch, x, y);
}
inline void DCanvas::DrawColoredPatchClean (const patch_t *patch, int x, int y) const
{
	DrawCWrapper (EWrapper_Colored, patch, x, y);
}
inline void DCanvas::DrawColoredPatchCleanNoMove (const patch_t *patch, int x, int y) const
{
	DrawCNMWrapper (EWrapper_Colored, patch, x, y);
}

inline void DCanvas::DrawColoredLucentPatch (const patch_t *patch, int x, int y) const
{
	DrawWrapper (EWrapper_ColoredLucent, patch, x, y);
}
inline void DCanvas::DrawColoredLucentPatchStretched (const patch_t *patch, int x, int y, int dw, int dh) const
{
	DrawSWrapper (EWrapper_ColoredLucent, patch, x, y, dw, dh);
}
inline void DCanvas::DrawColoredLucentPatchDirect (const patch_t *patch, int x, int y) const
{
	DrawWrapper (EWrapper_ColoredLucent, patch, x, y);
}
inline void DCanvas::DrawColoredLucentPatchIndirect (const patch_t *patch, int x, int y) const
{
	DrawIWrapper (EWrapper_ColoredLucent, patch, x, y);
}
inline void DCanvas::DrawColoredLucentPatchClean (const patch_t *patch, int x, int y) const
{
	DrawCWrapper (EWrapper_ColoredLucent, patch, x, y);
}
inline void DCanvas::DrawColoredLucentPatchCleanNoMove (const patch_t *patch, int x, int y) const
{
	DrawCNMWrapper (EWrapper_ColoredLucent, patch, x, y);
}

inline void DCanvas::DrawShadowedPatch (const patch_t *patch, int x, int y) const
{
	V_ColorFill = 0;
	DrawWrapper (EWrapper_ColoredLucent, patch, x+2, y+2);
	DrawWrapper (EWrapper_Normal, patch, x, y);
}
inline void DCanvas::DrawShadowedPatchStretched (const patch_t *patch, int x, int y, int dw, int dh) const
{
	V_ColorFill = 0;
	DrawSWrapper (EWrapper_ColoredLucent, patch, x+2, y+2, dw, dh);
	DrawSWrapper (EWrapper_Normal, patch, x, y, dw, dh);
}
inline void DCanvas::DrawShadowedPatchDirect (const patch_t *patch, int x, int y) const
{
	V_ColorFill = 0;
	DrawWrapper (EWrapper_ColoredLucent, patch, x+2, y+2);
	DrawWrapper (EWrapper_Normal, patch, x, y);
}
inline void DCanvas::DrawShadowedPatchIndirect (const patch_t *patch, int x, int y) const
{
	V_ColorFill = 0;
	DrawIWrapper (EWrapper_ColoredLucent, patch, x+2, y+2);
	DrawIWrapper (EWrapper_Normal, patch, x, y);
}
inline void DCanvas::DrawShadowedPatchClean (const patch_t *patch, int x, int y) const
{
	V_ColorFill = 0;
	DrawCWrapper (EWrapper_ColoredLucent, patch, x+2, y+2);
	DrawCWrapper (EWrapper_Normal, patch, x, y);
}
inline void DCanvas::DrawShadowedPatchCleanNoMove (const patch_t *patch, int x, int y) const
{
	V_ColorFill = 0;
	DrawCNMWrapper (EWrapper_ColoredLucent, patch, x+2, y+2);
	DrawCNMWrapper (EWrapper_Normal, patch, x, y);
}

// A canvas in system memory.

class DSimpleCanvas : public DCanvas
{
	DECLARE_CLASS (DSimpleCanvas, DCanvas)
public:
	DSimpleCanvas (int width, int height);
	~DSimpleCanvas ();

	bool IsValid ();
	bool Lock ();
	void Unlock ();

protected:
	BYTE *MemBuffer;
};

// A canvas that represents the actual display. The video code is responsible
// for actually implementing this. Built on top of SimpleCanvas, because it
// needs a system memory buffer when buffered output is enabled.

class DFrameBuffer : public DSimpleCanvas
{
	DECLARE_ABSTRACT_CLASS (DFrameBuffer, DSimpleCanvas)
public:
	DFrameBuffer (int width, int height);

	// Force the surface to use buffered output if true is passed.
	virtual bool Lock (bool buffered) = 0;

	// Locks the surface, using whatever the previous buffered status was.
	virtual bool Relock () = 0;

	// If the surface is buffered by choice, copy the specified region
	// to the screen and then go unbuffered. If the surface must be
	// buffered (e.g. if the display is not 8-bit), then does nothing.
	virtual void PartialUpdate (int x, int y, int width, int height) = 0;

	// Make the surface visible. Also implies Unlock().
	virtual void Update () = 0;

	// Return a pointer to 256 palette entries that can be written to.
	virtual PalEntry *GetPalette () = 0;

	// Mark the palette as changed. It will be updated on the next Update().
	virtual void UpdatePalette () = 0;

	// Sets the gamma level. Returns false if the hardware does not support
	// gamma changing. (Always true for now, since palettes can always be
	// gamma adjusted.)
	virtual bool SetGamma (float gamma) = 0;

	// Sets a color flash. RGB is the color, and amount is 0-256, with 256
	// being all flash and 0 being no flash. Returns false if the hardware
	// does not support this. (Always true for now, since palettes can always
	// be flashed.)
	virtual bool SetFlash (PalEntry rgb, int amount) = 0;

	// Returns the number of video pages the frame buffer is using.
	virtual int GetPageCount () = 0;

#ifdef _WIN32
	virtual int QueryNewPalette () = 0;
#endif

protected:
	void DrawRateStuff ();
	void CopyFromBuff (BYTE *src, int srcPitch, int width, int height, BYTE *dest);

private:
	DWORD LastMS, LastSec, FrameCount, LastCount, LastTic;
};

extern FColorMatcher ColorMatcher;

// This is the screen updated by I_FinishUpdate.
extern DFrameBuffer *screen;

#define SCREENWIDTH (screen->GetWidth ())
#define SCREENHEIGHT (screen->GetHeight ())
#define SCREENPITCH (screen->GetPitch ())

EXTERN_CVAR (Float, Gamma)

// Translucency tables
extern "C" DWORD Col2RGB8[65][256];
extern "C" byte RGB32k[32][32][32];
extern "C" DWORD *Col2RGB8_LessPrecision[65];

extern int TransArea, TotalArea;

// Allocates buffer screens, call before R_Init.
void V_Init ();

void V_MarkRect (int x, int y, int width, int height);

// Returns the closest color to the one desired. String
// should be of the form "rr gg bb".
int V_GetColorFromString (const DWORD *palette, const char *colorstring);
// Scans through the X11R6RGB lump for a matching color
// and returns a color string suitable for V_GetColorFromString.
char *V_GetColorStringByName (const char *name);

// Tries to get color by name, then by string
int V_GetColor (const DWORD *palette, const char *str);

bool V_SetResolution (int width, int height, int bpp);


#ifdef USEASM
extern "C" void ASM_PatchPitch (void);
#endif

#endif // __V_VIDEO_H__

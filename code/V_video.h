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

#include "doomdef.h"

// Needed because we are refering to patches.
#include "r_data.h"

extern int CleanWidth, CleanHeight, CleanXfac, CleanYfac;

//
// VIDEO
//
// [RH] Made screens more implementation-independant:
//
// V_LockScreen() must be called before drawing on a
// screen, and V_UnlockScreen() must be called when
// drawing is finished. As far as ZDoom is concerned,
// there are only two display depths: 8-bit indexed and
// 32-bit RGBA. The video driver is expected to be able
// to convert these to a format supported by the hardware.
// As such, the bits field is only used as an indicator
// of the output display depth and not as the screen's
// display depth (use is8bit for that).
class DCanvas : DObject
{
	DECLARE_CLASS (DCanvas, DObject)
public:
	enum EWrapperCode
	{
		EWrapper_Normal = 0,		// Just draws the patch as is
		EWrapper_Lucent = 1,		// Mixes the patch with the background
		EWrapper_Translated = 2,	// Color translates the patch and draws it
		EWrapper_TlatedLucent = 3,	// Translates the patch and mixes it with the background
		EWrapper_Colored = 4,		// Fills the patch area with a solid color
		EWrapper_ColoredLucent = 5	// Mixes a solid color in the patch area with the background
	};

	//DCanvas ();
	DCanvas (int width, int height, int bits);
	~DCanvas ();

	int bits;
	byte *buffer;
	int width;
	int height;
	int pitch;
	bool is8bit;

	// Copy blocks from one canvas to another
	void Blit (int srcx, int srcy, int srcwidth, int srcheight, DCanvas *dest, int destx, int desty, int destwidth, int destheight);
	void CopyRect (int srcx, int srcy, int width, int height, int destx, int desty, DCanvas *destscrn);

	// Draw a linear block of pixels into the view buffer.
	void DrawBlock (int x, int y, int width, int height, const byte *src) const;

	// Reads a linear block of pixels into the view buffer.
	void GetBlock (int x, int y, int width, int height, byte *dest) const;

	// Darken the entire canvas
	void Dim () const;

	// Fill an area with a 64x64 flat texture
	void FlatFill (int left, int top, int right, int bottom, const byte *src) const;

	// Set an area to a specified color
	void Clear (int left, int top, int right, int bottom, int color) const;

	// Access control
	void Lock ();
	void Unlock ();

	// Palette control (unused)
	void AttachPalette (palette_t *pal);
	void DetachPalette ();

	// Text drawing functions
	// Output a line of text using the console font
	void PrintStr (int x, int y, const char *s, int count) const;
	void PrintStr2 (int x, int y, const char *s, int count) const;

	// Output some text with wad heads-up font
	inline void DrawText (int normalcolor, int x, int y, const byte *string) const;
	inline void DrawTextLuc (int normalcolor, int x, int y, const byte *string) const;
	inline void DrawTextClean (int normalcolor, int x, int y, const byte *string) const;		// Does not adjust x and y
	inline void DrawTextCleanLuc (int normalcolor, int x, int y, const byte *string) const;		// ditto
	inline void DrawTextCleanMove (int normalcolor, int x, int y, const byte *string) const;	// This one does

	inline void DrawText (int normalcolor, int x, int y, const char *string) const;
	inline void DrawTextLuc (int normalcolor, int x, int y, const char *string) const;
	inline void DrawTextClean (int normalcolor, int x, int y, const char *string) const;
	inline void DrawTextCleanLuc (int normalcolor, int x, int y, const char *string) const;
	inline void DrawTextCleanMove (int normalcolor, int x, int y, const char *string) const;

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

protected:
	void TextWrapper (EWrapperCode drawer, int normalcolor, int x, int y, const byte *string) const;
	void TextSWrapper (EWrapperCode drawer, int normalcolor, int x, int y, const byte *string) const;

	void DrawWrapper (EWrapperCode drawer, const patch_t *patch, int x, int y) const;
	void DrawSWrapper (EWrapperCode drawer, const patch_t *patch, int x, int y, int destwidth, int destheight) const;
	void DrawIWrapper (EWrapperCode drawer, const patch_t *patch, int x, int y) const;
	void DrawCWrapper (EWrapperCode drawer, const patch_t *patch, int x, int y) const;
	void DrawCNMWrapper (EWrapperCode drawer, const patch_t *patch, int x, int y) const;

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

	// Direct (true-color) versions of the column drawers
	static vdrawfunc Dfuncs[6];
	static vdrawsfunc Dsfuncs[6];

	// The current set of column drawers (set in V_SetResolution)
	static vdrawfunc *m_Drawfuncs;
	static vdrawsfunc *m_Drawsfuncs;

private:
	int m_LockCount;
	palette_t *m_Palette;
	void *m_Private;

	friend bool I_AllocateScreen (DCanvas *canvas, int width, int height, int bits);
	friend void I_FreeScreen (DCanvas *canvas);

	friend void I_FinishUpdate ();

	friend void I_LockScreen (DCanvas *canvas);
	friend void I_UnlockScreen (DCanvas *canvas);
	friend void I_Blit (DCanvas *from, int srcx, int srcy, int srcwidth, int srcheight,
						DCanvas *to, int destx, int desty, int destwidth, int destheight);
};

inline void DCanvas::DrawText (int normalcolor, int x, int y, const byte *string) const
{
	TextWrapper (EWrapper_Translated, normalcolor, x, y, string);
}
inline void DCanvas::DrawTextLuc (int normalcolor, int x, int y, const byte *string) const
{
	TextWrapper (EWrapper_TlatedLucent, normalcolor, x, y, string);
}
inline void DCanvas::DrawTextClean (int normalcolor, int x, int y, const byte *string) const
{
	TextSWrapper (EWrapper_Translated, normalcolor, x, y, string);
}
inline void DCanvas::DrawTextCleanLuc (int normalcolor, int x, int y, const byte *string) const
{
	TextSWrapper (EWrapper_TlatedLucent, normalcolor, x, y, string);
}
inline void DCanvas::DrawTextCleanMove (int normalcolor, int x, int y, const byte *string) const
{
	TextSWrapper (EWrapper_Translated, normalcolor,
		(x - 160) * CleanXfac + width / 2,
		(y - 100) * CleanYfac + height / 2,
		string);
}

inline void DCanvas::DrawText (int normalcolor, int x, int y, const char *string) const
{
	TextWrapper (EWrapper_Translated, normalcolor, x, y, (const byte *)string);
}
inline void DCanvas::DrawTextLuc (int normalcolor, int x, int y, const char *string) const
{
	TextWrapper (EWrapper_TlatedLucent, normalcolor, x, y, (const byte *)string);
}
inline void DCanvas::DrawTextClean (int normalcolor, int x, int y, const char *string) const
{
	TextSWrapper (EWrapper_Translated, normalcolor, x, y, (const byte *)string);
}
inline void DCanvas::DrawTextCleanLuc (int normalcolor, int x, int y, const char *string) const
{
	TextSWrapper (EWrapper_TlatedLucent, normalcolor, x, y, (const byte *)string);
}
inline void DCanvas::DrawTextCleanMove (int normalcolor, int x, int y, const char *string) const
{
	TextSWrapper (EWrapper_Translated, normalcolor,
		(x - 160) * CleanXfac + width / 2,
		(y - 100) * CleanYfac + height / 2,
		(const byte *)string);
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

extern "C" palette_t *DefaultPalette;

// This is the screen updated by I_FinishUpdate.
extern	DCanvas *screen;

extern	DBoundingBox 	dirtybox;

extern	byte	newgamma[256];
EXTERN_CVAR (gamma)

// Translucency tables
extern unsigned int Col2RGB8[65][256];
extern byte RGB32k[32][32][32];

// Allocates buffer screens, call before R_Init.
void V_Init (void);

// The color to fill with for #4 and #5 above
extern int V_ColorFill;

// The color map for #1 and #2 above
extern byte *V_ColorMap;

// The palette lookup table to be used with for direct modes
extern unsigned int *V_Palette;

void V_MarkRect (int x, int y, int width, int height);

// BestColor
byte BestColor (const unsigned int *palette, const int r, const int g, const int b, const int numcolors);
// Returns the closest color to the one desired. String
// should be of the form "rrrr gggg bbbb".
int V_GetColorFromString (const unsigned int *palette, const char *colorstring);
// Scans through the X11R6RGB lump for a matching color
// and returns a color string suitable for V_GetColorFromString.
char *V_GetColorStringByName (const char *name);


BOOL V_SetResolution (int width, int height, int bpp);


#ifdef USEASM
extern "C" void ASM_PatchPitch (void);
extern "C" void ASM_PatchColSize (void);
#endif

#endif // __V_VIDEO_H__

/*
** v_video.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2004 Randy Heit
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

class FTexture;

// TagItem definitions for DrawTexture. As far as I know, tag lists
// originated on the Amiga.
//
// Think of TagItems as an array of the following structure:
//
// struct TagItem {
//     DWORD ti_Tag;
//     DWORD ti_Data;
// };

#define TAG_DONE	(0L) /* Used to indicate the end of the Tag list */
#define TAG_END		(0L) /* Ditto									*/
#define TAG_IGNORE	(1L) /* Ignore this Tag							*/
#define TAG_MORE	(2L) /* Ends this list and continues with the	*/
						 /* list pointed to in ti_Data 				*/
//#define TAG_SKIP	(3L) /* Skip this and the next ti_Data Tags		*/

#define TAG_USER	((DWORD)(1L<<31))

enum
{
	DTA_Base = TAG_USER + 5000,
	DTA_DestWidth,		// width of area to draw to
	DTA_DestHeight,		// height of area to draw to
	DTA_Alpha,			// alpha value for translucency
	DTA_FillColor,		// color to stencil onto the destination
	DTA_Translation,	// translation table to recolor the source
	DTA_AlphaChannel,	// bool: the source is an alpha channel; used with DTA_FillColor
	DTA_Clean,			// bool: scale texture size and position by CleanXfac and CleanYfac
	DTA_320x200,		// bool: scale texture size and position to fit on a virtual 320x200 screen
	DTA_CleanNoMove,	// bool: like DTA_Clean but does not reposition output position
	DTA_FlipX,			// bool: flip image horizontally
	DTA_ShadowColor,	// color of shadow
	DTA_ShadowAlpha,	// alpha of shadow
	DTA_Shadow,			// set shadow color and alphas to defaults
	DTA_VirtualWidth,	// pretend the canvas is this wide
	DTA_VirtualHeight,	// pretend the canvas is this tall
	DTA_TopOffset,		// override texture's top offset
	DTA_LeftOffset,		// override texture's left offset
	DTA_CenterOffset,	// override texture's left and top offsets and set them for the texture's middle
	DTA_CenterBottomOffset,// override texture's left and top offsets and set them for the texture's bottom middle
	DTA_WindowLeft,		// don't draw anything left of this column (on source, not dest)
	DTA_WindowRight,	// don't draw anything at or to the right of this column (on source, not dest)
	DTA_ClipTop,		// don't draw anything above this row (on dest, not source)
	DTA_ClipBottom,		// don't draw anything at or below this row (on dest, not source)
	DTA_ClipLeft,		// don't draw anything to the left of this column (on dest, not source)
	DTA_ClipRight,		// don't draw anything at or to the right of this column (on dest, not source)
	DTA_Masked,			// true(default)=use masks from texture, false=ignore masks
	DTA_HUDRules,		// use fullscreen HUD rules to position and size textures
};

enum
{
	HUD_Normal,
	HUD_HorizCenter
};


//
// VIDEO
//
// [RH] Made screens more implementation-independant:
// This layer isn't really necessary, and it would be nice to remove it, I think.
// But ZDoom is now built around it so much, I'll probably just leave it.
//
class DCanvas : public DObject
{
	DECLARE_ABSTRACT_CLASS (DCanvas, DObject)
public:
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
	virtual bool IsLocked () { return Buffer != NULL; }	// Returns true if the surface is locked

	// Copy blocks from one canvas to another
	virtual void Blit (int srcx, int srcy, int srcwidth, int srcheight, DCanvas *dest, int destx, int desty, int destwidth, int destheight);

	// Draw a linear block of pixels into the canvas
	virtual void DrawBlock (int x, int y, int width, int height, const byte *src) const;

	// Reads a linear block of pixels into the view buffer.
	virtual void GetBlock (int x, int y, int width, int height, byte *dest) const;

	// Dim the entire canvas for the menus
	virtual void Dim () const;

	// Dim part of the canvas
	virtual void Dim (PalEntry color, float amount, int x1, int y1, int w, int h) const;

	// Fill an area with a texture
	void FlatFill (int left, int top, int right, int bottom, FTexture *src);

	// Set an area to a specified color
	virtual void Clear (int left, int top, int right, int bottom, int color) const;

	// Calculate gamma table
	void CalcGamma (float gamma, BYTE gammalookup[256]);

	// Text drawing functions -----------------------------------------------

	virtual void SetFont (FFont *font);

	// 2D Texture drawing
	void STACK_ARGS DrawTexture (FTexture *img, int x, int y, DWORD tags, ...);
	void FillBorder (FTexture *img);	// Fills the border around a 4:3 part of the screen on non-4:3 displays

	// 2D Text drawing
	void STACK_ARGS DrawText (int normalcolor, int x, int y, const char *string, ...);
	void STACK_ARGS DrawChar (int normalcolor, int x, int y, BYTE character, ...);

protected:
	BYTE *Buffer;
	int Width;
	int Height;
	int Pitch;
	int LockCount;

	bool ClipBox (int &left, int &top, int &width, int &height, const byte *&src, const int srcpitch) const;

private:
	// Keep track of canvases, for automatic destruction at exit
	DCanvas *Next;
	static DCanvas *CanvasChain;

	friend void STACK_ARGS FreeCanvasChain ();
};

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

	// Make the surface visible. Also implies Unlock().
	virtual void Update () = 0;

	// Return a pointer to 256 palette entries that can be written to.
	virtual PalEntry *GetPalette () = 0;

	// Stores the palette with flash blended in into 256 dwords
	virtual void GetFlashedPalette (PalEntry palette[256]) = 0;

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

	// Converse of SetFlash
	virtual void GetFlash (PalEntry &rgb, int &amount) = 0;

	// Returns the number of video pages the frame buffer is using.
	virtual int GetPageCount () = 0;

	// Returns true if running fullscreen.
	virtual bool IsFullscreen () = 0;

#ifdef _WIN32
	virtual void PaletteChanged () = 0;
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

int CheckRatio (int width, int height);
extern const int BaseRatioSizes[5][4];

#endif // __V_VIDEO_H__

/*
** v_video.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
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

#include "doomdef.h"
#include "dobject.h"
#include "r_data/renderstyle.h"
#include "c_cvars.h"

extern int CleanWidth, CleanHeight, CleanXfac, CleanYfac;
extern int CleanWidth_1, CleanHeight_1, CleanXfac_1, CleanYfac_1;
extern int DisplayWidth, DisplayHeight, DisplayBits;

bool V_DoModeSetup (int width, int height, int bits);
void V_CalcCleanFacs (int designwidth, int designheight, int realwidth, int realheight, int *cleanx, int *cleany, int *cx1=NULL, int *cx2=NULL);

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

#define TAG_DONE	(0)  /* Used to indicate the end of the Tag list */
#define TAG_END		(0)  /* Ditto									*/
#define TAG_IGNORE	(1)  /* Ignore this Tag							*/
#define TAG_MORE	(2)  /* Ends this list and continues with the	*/
						 /* list pointed to in ti_Data 				*/

#define TAG_USER	((DWORD)(1u<<30))

enum
{
	DTA_Base = TAG_USER + 5000,
	DTA_DestWidth,		// width of area to draw to
	DTA_DestHeight,		// height of area to draw to
	DTA_Alpha,			// alpha value for translucency
	DTA_FillColor,		// color to stencil onto the destination (RGB is the color for truecolor drawers, A is the palette index for paletted drawers)
	DTA_Translation,	// translation table to recolor the source
	DTA_AlphaChannel,	// bool: the source is an alpha channel; used with DTA_FillColor
	DTA_Clean,			// bool: scale texture size and position by CleanXfac and CleanYfac
	DTA_320x200,		// bool: scale texture size and position to fit on a virtual 320x200 screen
	DTA_Bottom320x200,	// bool: same as DTA_320x200 but centers virtual screen on bottom for 1280x1024 targets
	DTA_CleanNoMove,	// bool: like DTA_Clean but does not reposition output position
	DTA_CleanNoMove_1,	// bool: like DTA_CleanNoMove, but uses Clean[XY]fac_1 instead
	DTA_FlipX,			// bool: flip image horizontally	//FIXME: Does not work with DTA_Window(Left|Right)
	DTA_ShadowColor,	// color of shadow
	DTA_ShadowAlpha,	// alpha of shadow
	DTA_Shadow,			// set shadow color and alphas to defaults
	DTA_VirtualWidth,	// pretend the canvas is this wide
	DTA_VirtualHeight,	// pretend the canvas is this tall
	DTA_TopOffset,		// override texture's top offset
	DTA_LeftOffset,		// override texture's left offset
	DTA_CenterOffset,	// bool: override texture's left and top offsets and set them for the texture's middle
	DTA_CenterBottomOffset,// bool: override texture's left and top offsets and set them for the texture's bottom middle
	DTA_WindowLeft,		// don't draw anything left of this column (on source, not dest)
	DTA_WindowRight,	// don't draw anything at or to the right of this column (on source, not dest)
	DTA_ClipTop,		// don't draw anything above this row (on dest, not source)
	DTA_ClipBottom,		// don't draw anything at or below this row (on dest, not source)
	DTA_ClipLeft,		// don't draw anything to the left of this column (on dest, not source)
	DTA_ClipRight,		// don't draw anything at or to the right of this column (on dest, not source)
	DTA_Masked,			// true(default)=use masks from texture, false=ignore masks
	DTA_HUDRules,		// use fullscreen HUD rules to position and size textures
	DTA_KeepRatio,		// doesn't adjust screen size for DTA_Virtual* if the aspect ratio is not 4:3
	DTA_RenderStyle,	// same as render style for actors
	DTA_ColorOverlay,	// DWORD: ARGB to overlay on top of image; limited to black for software
	DTA_BilinearFilter,	// bool: apply bilinear filtering to the image
	DTA_SpecialColormap,// pointer to FSpecialColormapParameters (likely to be forever hardware-only)
	DTA_ColormapStyle,	// pointer to FColormapStyle (hardware-only)
	DTA_Fullscreen,		// Draw image fullscreen (same as DTA_VirtualWidth/Height with graphics size.)

	// floating point duplicates of some of the above:
	DTA_DestWidthF,
	DTA_DestHeightF,
	DTA_TopOffsetF,
	DTA_LeftOffsetF,
	DTA_VirtualWidthF,
	DTA_VirtualHeightF,
	DTA_WindowLeftF,
	DTA_WindowRightF,

	// For DrawText calls:
	DTA_TextLen,		// stop after this many characters, even if \0 not hit
	DTA_CellX,			// horizontal size of character cell
	DTA_CellY,			// vertical size of character cell
};

enum
{
	HUD_Normal,
	HUD_HorizCenter
};


class FFont;
struct FRemapTable;
class player_t;
typedef uint32 angle_t;

//
// VIDEO
//
// [RH] Made screens more implementation-independant:
//
class DCanvas : public DObject
{
	DECLARE_ABSTRACT_CLASS (DCanvas, DObject)
public:
	DCanvas (int width, int height);
	virtual ~DCanvas ();

	// Member variable access
	inline BYTE *GetBuffer () const { return Buffer; }
	inline int GetWidth () const { return Width; }
	inline int GetHeight () const { return Height; }
	inline int GetPitch () const { return Pitch; }

	virtual bool IsValid ();

	// Access control
	virtual bool Lock (bool buffered=true) = 0;		// Returns true if the surface was lost since last time
	virtual void Unlock () = 0;
	virtual bool IsLocked () { return Buffer != NULL; }	// Returns true if the surface is locked

	// Draw a linear block of pixels into the canvas
	virtual void DrawBlock (int x, int y, int width, int height, const BYTE *src) const;

	// Reads a linear block of pixels into the view buffer.
	virtual void GetBlock (int x, int y, int width, int height, BYTE *dest) const;

	// Dim the entire canvas for the menus
	virtual void Dim (PalEntry color = 0);

	// Dim part of the canvas
	virtual void Dim (PalEntry color, float amount, int x1, int y1, int w, int h);

	// Fill an area with a texture
	virtual void FlatFill (int left, int top, int right, int bottom, FTexture *src, bool local_origin=false);

	// Fill a simple polygon with a texture
	virtual void FillSimplePoly(FTexture *tex, FVector2 *points, int npoints,
		double originx, double originy, double scalex, double scaley, angle_t rotation,
		struct FDynamicColormap *colormap, int lightlevel);

	// Set an area to a specified color
	virtual void Clear (int left, int top, int right, int bottom, int palcolor, uint32 color);

	// Draws a line
	virtual void DrawLine(int x0, int y0, int x1, int y1, int palColor, uint32 realcolor);

	// Draws a single pixel
	virtual void DrawPixel(int x, int y, int palcolor, uint32 rgbcolor);

	// Calculate gamma table
	void CalcGamma (float gamma, BYTE gammalookup[256]);


	// Retrieves a buffer containing image data for a screenshot.
	// Hint: Pitch can be negative for upside-down images, in which case buffer
	// points to the last row in the buffer, which will be the first row output.
	virtual void GetScreenshotBuffer(const BYTE *&buffer, int &pitch, ESSType &color_type);

	// Releases the screenshot buffer.
	virtual void ReleaseScreenshotBuffer();

	// Text drawing functions -----------------------------------------------

	// 2D Texture drawing
	void STACK_ARGS DrawTexture (FTexture *img, double x, double y, int tags, ...);
	void FillBorder (FTexture *img);	// Fills the border around a 4:3 part of the screen on non-4:3 displays
	void VirtualToRealCoords(double &x, double &y, double &w, double &h, double vwidth, double vheight, bool vbottom=false, bool handleaspect=true) const;

	// Code that uses these (i.e. SBARINFO) should probably be evaluated for using doubles all around instead.
	void VirtualToRealCoordsFixed(fixed_t &x, fixed_t &y, fixed_t &w, fixed_t &h, int vwidth, int vheight, bool vbottom=false, bool handleaspect=true) const;
	void VirtualToRealCoordsInt(int &x, int &y, int &w, int &h, int vwidth, int vheight, bool vbottom=false, bool handleaspect=true) const;

	// 2D Text drawing
	void STACK_ARGS DrawText (FFont *font, int normalcolor, int x, int y, const char *string, ...);
#ifndef DrawText	// See WinUser.h for the definition of DrawText as a macro
	void STACK_ARGS DrawTextA (FFont *font, int normalcolor, int x, int y, const char *string, ...);
#endif
	void DrawTextV (FFont *font, int normalcolor, int x, int y, const char *string, va_list tags);
	void STACK_ARGS DrawChar (FFont *font, int normalcolor, int x, int y, BYTE character, ...);

	struct DrawParms
	{
		double x, y;
		double texwidth;
		double texheight;
		double destwidth;
		double destheight;
		double virtWidth;
		double virtHeight;
		double windowleft;
		double windowright;
		int dclip;
		int uclip;
		int lclip;
		int rclip;
		double top;
		double left;
		fixed_t alpha;
		uint32 fillcolor;
		FRemapTable *remap;
		const BYTE *translation;
		uint32 colorOverlay;
		INTBOOL alphaChannel;
		INTBOOL flipX;
		fixed_t shadowAlpha;
		int shadowColor;
		INTBOOL keepratio;
		INTBOOL masked;
		INTBOOL bilinear;
		FRenderStyle style;
		struct FSpecialColormap *specialcolormap;
		struct FColormapStyle *colormapstyle;
	};

protected:
	BYTE *Buffer;
	int Width;
	int Height;
	int Pitch;
	int LockCount;

	bool ClipBox (int &left, int &top, int &width, int &height, const BYTE *&src, const int srcpitch) const;
	virtual void STACK_ARGS DrawTextureV (FTexture *img, double x, double y, uint32 tag, va_list tags);
	bool ParseDrawTextureTags (FTexture *img, double x, double y, uint32 tag, va_list tags, DrawParms *parms, bool hw) const;

	DCanvas() {}

private:
	// Keep track of canvases, for automatic destruction at exit
	DCanvas *Next;
	static DCanvas *CanvasChain;

	void PUTTRANSDOT (int xx, int yy, int basecolor, int level);
};

// A canvas in system memory.

class DSimpleCanvas : public DCanvas
{
	DECLARE_CLASS (DSimpleCanvas, DCanvas)
public:
	DSimpleCanvas (int width, int height);
	~DSimpleCanvas ();

	bool IsValid ();
	bool Lock (bool buffered=true);
	void Unlock ();

protected:
	BYTE *MemBuffer;

	DSimpleCanvas() {}
};

// This class represents a native texture, as opposed to an FTexture.
class FNativeTexture
{
public:
	virtual ~FNativeTexture();
	virtual bool Update() = 0;
	virtual bool CheckWrapping(bool wrapping);
};

// This class represents a texture lookup palette.
class FNativePalette
{
public:
	virtual ~FNativePalette();
	virtual bool Update() = 0;
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

	// Changes the vsync setting, if supported by the device.
	virtual void SetVSync (bool vsync);

	// Tells the device to recreate itself with the new setting from vid_refreshrate.
	virtual void NewRefreshRate ();

	// Set the rect defining the area affected by blending.
	virtual void SetBlendingRect (int x1, int y1, int x2, int y2);

	bool Accel2D;	// If true, 2D drawing can be accelerated.

	// Begin 2D drawing operations. This is like Update, but it doesn't end
	// the scene, and it doesn't present the image yet. If you are going to
	// be covering the entire screen with 2D elements, then pass false to
	// avoid copying the software buffer to the screen.
	// Returns true if hardware-accelerated 2D has been entered, false if not.
	virtual bool Begin2D(bool copy3d);

	// DrawTexture calls after Begin2D use native textures.

	// Draws the blending rectangle over the viewwindow if in hardware-
	// accelerated 2D mode.
	virtual void DrawBlendingRect();

	// Create a native texture from a game texture.
	virtual FNativeTexture *CreateTexture(FTexture *gametex, bool wrapping);

	// Create a palette texture from a remap/palette table.
	virtual FNativePalette *CreatePalette(FRemapTable *remap);

	// Precaches or unloads a texture
	virtual void GetHitlist(BYTE *hitlist);

	// Report a game restart
	virtual void GameRestart();

	// Screen wiping
	virtual bool WipeStartScreen(int type);
	virtual void WipeEndScreen();
	virtual bool WipeDo(int ticks);
	virtual void WipeCleanup();

	virtual void ScaleCoordsFromWindow(SWORD &x, SWORD &y) {}

	uint32 GetLastFPS() const { return LastCount; }

#ifdef _WIN32
	virtual void PaletteChanged () = 0;
	virtual int QueryNewPalette () = 0;
	virtual bool Is8BitMode() = 0;
#endif

protected:
	void DrawRateStuff ();
	void CopyFromBuff (BYTE *src, int srcPitch, int width, int height, BYTE *dest);

	DFrameBuffer () {}

private:
	uint32 LastMS, LastSec, FrameCount, LastCount, LastTic;
};


// This is the screen updated by I_FinishUpdate.
extern DFrameBuffer *screen;

#define SCREENWIDTH (screen->GetWidth ())
#define SCREENHEIGHT (screen->GetHeight ())
#define SCREENPITCH (screen->GetPitch ())

EXTERN_CVAR (Float, Gamma)

// Translucency tables

// RGB32k is a normal R5G5B5 -> palette lookup table.

// Use a union so we can "overflow" without warnings.
// Otherwise, we get stuff like this from Clang (when compiled
// with -fsanitize=bounds) while running:
//   src/v_video.cpp:390:12: runtime error: index 1068 out of bounds for type 'BYTE [32]'
//   src/r_draw.cpp:273:11: runtime error: index 1057 out of bounds for type 'BYTE [32]'
union ColorTable32k
{
	BYTE RGB[32][32][32];
	BYTE All[32 *32 *32];
};
extern "C" ColorTable32k RGB32k;

// Col2RGB8 is a pre-multiplied palette for color lookup. It is stored in a
// special R10B10G10 format for efficient blending computation.
//		--RRRRRrrr--BBBBBbbb--GGGGGggg--   at level 64
//		--------rrrr------bbbb------gggg   at level 1
extern "C" DWORD Col2RGB8[65][256];

// Col2RGB8_LessPrecision is the same as Col2RGB8, but the LSB for red
// and blue are forced to zero, so if the blend overflows, it won't spill
// over into the next component's value.
//		--RRRRRrrr-#BBBBBbbb-#GGGGGggg--  at level 64
//      --------rrr#------bbb#------gggg  at level 1
extern "C" DWORD *Col2RGB8_LessPrecision[65];

// Col2RGB8_Inverse is the same as Col2RGB8_LessPrecision, except the source
// palette has been inverted.
extern "C" DWORD Col2RGB8_Inverse[65][256];

// "Magic" numbers used during the blending:
//		--000001111100000111110000011111	= 0x01f07c1f
//		-0111111111011111111101111111111	= 0x3FEFFBFF
//		-1000000000100000000010000000000	= 0x40100400
//		------10000000001000000000100000	= 0x40100400 >> 5
//		--11111-----11111-----11111-----	= 0x40100400 - (0x40100400 >> 5) aka "white"
//		--111111111111111111111111111111	= 0x3FFFFFFF

// Allocates buffer screens, call before R_Init.
void V_Init (bool restart);

// Initializes graphics mode for the first time.
void V_Init2 ();

void V_Shutdown ();

void V_MarkRect (int x, int y, int width, int height);

// Returns the closest color to the one desired. String
// should be of the form "rr gg bb".
int V_GetColorFromString (const DWORD *palette, const char *colorstring);
// Scans through the X11R6RGB lump for a matching color
// and returns a color string suitable for V_GetColorFromString.
FString V_GetColorStringByName (const char *name);

// Tries to get color by name, then by string
int V_GetColor (const DWORD *palette, const char *str);
void V_DrawFrame (int left, int top, int width, int height);

// If the view size is not full screen, draws a border around it.
void V_DrawBorder (int x1, int y1, int x2, int y2);
void V_RefreshViewBorder ();

void V_SetBorderNeedRefresh();

#if defined(X86_ASM) || defined(X64_ASM)
extern "C" void ASM_PatchPitch (void);
#endif

int CheckRatio (int width, int height, int *trueratio=NULL);
static inline int CheckRatio (double width, double height) { return CheckRatio(int(width), int(height)); }
extern const int BaseRatioSizes[5][4];



#endif // __V_VIDEO_H__

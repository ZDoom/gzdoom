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

#include <functional>
#include "doomtype.h"
#include "vectors.h"

#include "doomdef.h"
#include "dobject.h"
#include "r_data/renderstyle.h"
#include "c_cvars.h"
#include "v_colortables.h"
#include "v_2ddrawer.h"
#include "hwrenderer/dynlights/hw_shadowmap.h"

static const int VID_MIN_WIDTH = 640;
static const int VID_MIN_HEIGHT = 400;

struct sector_t;
class IShaderProgram;
class FTexture;
struct FPortalSceneState;
class FSkyVertexBuffer;
class IIndexBuffer;
class IVertexBuffer;
class IDataBuffer;
class FFlatVertexBuffer;
class GLViewpointBuffer;
class FLightBuffer;
struct HWDrawInfo;

enum EHWCaps
{
	// [BB] Added texture compression flags.
	RFL_TEXTURE_COMPRESSION = 1,
	RFL_TEXTURE_COMPRESSION_S3TC = 2,

	RFL_SHADER_STORAGE_BUFFER = 4,
	RFL_BUFFER_STORAGE = 8,

	RFL_NO_CLIP_PLANES = 32,

	RFL_INVALIDATE_BUFFER = 64,
	RFL_DEBUG = 128,
};


struct IntRect
{
	int left, top;
	int width, height;


	void Offset(int xofs, int yofs)
	{
		left += xofs;
		top += yofs;
	}

	void AddToRect(int x, int y)
	{
		if (x < left)
			left = x;
		if (x > left + width)
			width = x - left;

		if (y < top)
			top = y;
		if (y > top + height)
			height = y - top;
	}


};





extern int CleanWidth, CleanHeight, CleanXfac, CleanYfac;
extern int CleanWidth_1, CleanHeight_1, CleanXfac_1, CleanYfac_1;
extern int DisplayWidth, DisplayHeight;

void V_UpdateModeSize (int width, int height);
void V_OutputResized (int width, int height);
void V_CalcCleanFacs (int designwidth, int designheight, int realwidth, int realheight, int *cleanx, int *cleany, int *cx1=NULL, int *cx2=NULL);

EXTERN_CVAR(Int, vid_rendermode)
EXTERN_CVAR(Bool, fullscreen)
EXTERN_CVAR(Int, win_x)
EXTERN_CVAR(Int, win_y)
EXTERN_CVAR(Int, win_w)
EXTERN_CVAR(Int, win_h)
EXTERN_CVAR(Bool, win_maximized)


inline bool V_IsHardwareRenderer()
{
	return vid_rendermode == 4;
}

inline bool V_IsSoftwareRenderer()
{
	return vid_rendermode < 2;
}

inline bool V_IsPolyRenderer()
{
	return vid_rendermode == 2 || vid_rendermode == 3;
}

inline bool V_IsTrueColor()
{
	return vid_rendermode == 1 || vid_rendermode == 3 || vid_rendermode == 4;
}


class FTexture;
struct FColormap;
class FileWriter;
enum FTextureFormat : uint32_t;
class FModelRenderer;
struct SamplerUniform;

// TagItem definitions for DrawTexture. As far as I know, tag lists
// originated on the Amiga.
//
// Think of TagItems as an array of the following structure:
//
// struct TagItem {
//     uint32_t ti_Tag;
//     uint32_t ti_Data;
// };

#define TAG_DONE	(0)  /* Used to indicate the end of the Tag list */
#define TAG_END		(0)  /* Ditto									*/
						 /* list pointed to in ti_Data 				*/

#define TAG_USER	((uint32_t)(1u<<30))

enum
{
	DTA_Base = TAG_USER + 5000,
	DTA_DestWidth,		// width of area to draw to
	DTA_DestHeight,		// height of area to draw to
	DTA_Alpha,			// alpha value for translucency
	DTA_FillColor,		// color to stencil onto the destination
	DTA_TranslationIndex, // translation table to recolor the source
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
	DTA_HUDRulesC,		// only used internally for marking HUD_HorizCenter
	DTA_KeepRatio,		// doesn't adjust screen size for DTA_Virtual* if the aspect ratio is not 4:3
	DTA_RenderStyle,	// same as render style for actors
	DTA_ColorOverlay,	// uint32_t: ARGB to overlay on top of image; limited to black for software
	DTA_BilinearFilter,	// bool: apply bilinear filtering to the image
	DTA_SpecialColormap,// pointer to FSpecialColormapParameters
	DTA_Desaturate,		// explicit desaturation factor (does not do anything in Legacy OpenGL)
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

	// New additions. 
	DTA_Color,
	DTA_FlipY,			// bool: flip image vertically
	DTA_SrcX,			// specify a source rectangle (this supersedes the poorly implemented DTA_WindowLeft/Right
	DTA_SrcY,
	DTA_SrcWidth,
	DTA_SrcHeight,
	DTA_LegacyRenderStyle,	// takes an old-style STYLE_* constant instead of an FRenderStyle
	DTA_Burn,				// activates the burn shader for this element
	DTA_Spacing,			// Strings only: Additional spacing between characters
	DTA_Monospace,			// Fonts only: Use a fixed distance between characters.
};

enum EMonospacing : int
{
	Off = 0,
	CellLeft = 1,
	CellCenter = 2,
	CellRight = 3
};

enum
{
	HUD_Normal,
	HUD_HorizCenter
};


class FFont;
struct FRemapTable;
class player_t;
typedef uint32_t angle_t;

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
	int cleanmode;
	int dclip;
	int uclip;
	int lclip;
	int rclip;
	double top;
	double left;
	float Alpha;
	PalEntry fillcolor;
	FRemapTable *remap;
	PalEntry colorOverlay;
	PalEntry color;
	INTBOOL alphaChannel;
	INTBOOL flipX;
	INTBOOL flipY;
	//float shadowAlpha;
	int shadowColor;
	INTBOOL keepratio;
	INTBOOL masked;
	INTBOOL bilinear;
	FRenderStyle style;
	struct FSpecialColormap *specialcolormap;
	int desaturate;
	int scalex, scaley;
	int cellx, celly;
	int monospace;
	int spacing;
	int maxstrlen;
	bool fortext;
	bool virtBottom;
	double srcx, srcy;
	double srcwidth, srcheight;
	bool burn;
};

struct Va_List
{
	va_list list;
};

struct VMVa_List
{
	VMValue *args;
	int curindex;
	int numargs;
	const uint8_t *reginfo;
};
//
// VIDEO
//
//
class DCanvas
{
public:
	DCanvas (int width, int height, bool bgra);
	~DCanvas ();
	void Resize(int width, int height);

	// Member variable access
	inline uint8_t *GetPixels () const { return Pixels.Data(); }
	inline int GetWidth () const { return Width; }
	inline int GetHeight () const { return Height; }
	inline int GetPitch () const { return Pitch; }
	inline bool IsBgra() const { return Bgra; }

protected:
	TArray<uint8_t> Pixels;
	int Width;
	int Height;
	int Pitch;
	bool Bgra;
};

class FUniquePalette;
class IHardwareTexture;
class FTexture;


class DFrameBuffer
{
protected:

	void DrawTextureV(FTexture *img, double x, double y, uint32_t tag, va_list tags) = delete;
	void DrawTextureParms(FTexture *img, DrawParms &parms);

	template<class T>
	bool ParseDrawTextureTags(FTexture *img, double x, double y, uint32_t tag, T& tags, DrawParms *parms, bool fortext) const;
	template<class T>
	void DrawTextCommon(FFont *font, int normalcolor, double x, double y, const T *string, DrawParms &parms);

	F2DDrawer m2DDrawer;
private:
	int Width = 0;
	int Height = 0;
protected:
	int clipleft = 0, cliptop = 0, clipwidth = -1, clipheight = -1;

public:
	// Hardware render state that needs to be exposed to the API independent part of the renderer. For ease of access this is stored in the base class.
	int hwcaps = 0;								// Capability flags
	float glslversion = 0;						// This is here so that the differences between old OpenGL and new OpenGL/Vulkan can be handled by platform independent code.
	int instack[2] = { 0,0 };					// this is globally maintained state for portal recursion avoidance.
	int stencilValue = 0;						// Global stencil test value
	unsigned int uniformblockalignment = 256;	// Hardware dependent uniform buffer alignment.
	unsigned int maxuniformblock = 65536;
	const char *gl_vendorstring;				// On OpenGL (not Vulkan) we have to account for some issues with Intel.
	FPortalSceneState *mPortalState;			// global portal state.
	FSkyVertexBuffer *mSkyData = nullptr;		// the sky vertex buffer
	FFlatVertexBuffer *mVertexData = nullptr;	// Global vertex data
	GLViewpointBuffer *mViewpoints = nullptr;	// Viewpoint render data.
	FLightBuffer *mLights = nullptr;			// Dynamic lights
	IShadowMap mShadowMap;

	IntRect mScreenViewport;
	IntRect mSceneViewport;
	IntRect mOutputLetterbox;
	float mSceneClearColor[4];


public:
	DFrameBuffer (int width=1, int height=1);
	virtual ~DFrameBuffer();
	virtual void InitializeState() = 0;	// For stuff that needs 'screen' set.

	void SetSize(int width, int height);
	void SetVirtualSize(int width, int height)
	{
		Width = width;
		Height = height;
	}
	inline int GetWidth() const { return Width; }
	inline int GetHeight() const { return Height; }

	FVector2 SceneScale() const
	{
		return { mSceneViewport.width / (float)mScreenViewport.width, mSceneViewport.height / (float)mScreenViewport.height };
	}

	FVector2 SceneOffset() const
	{
		return { mSceneViewport.left / (float)mScreenViewport.width, mSceneViewport.top / (float)mScreenViewport.height };
	}

	// Make the surface visible.
	virtual void Update ();

	// Stores the palette with flash blended in into 256 dwords
	// Mark the palette as changed. It will be updated on the next Update().
	virtual void UpdatePalette() {}

	virtual void SetGamma() {}

	// Returns true if running fullscreen.
	virtual bool IsFullscreen () = 0;
	virtual void ToggleFullscreen(bool yes) {}

	// Changes the vsync setting, if supported by the device.
	virtual void SetVSync (bool vsync);

	// Delete any resources that need to be deleted after restarting with a different IWAD
	virtual void CleanForRestart() {}
	virtual void SetTextureFilterMode() {}
	virtual IHardwareTexture *CreateHardwareTexture() { return nullptr; }
	virtual void PrecacheMaterial(FMaterial *mat, int translation) {}
	virtual FModelRenderer *CreateModelRenderer(int mli) { return nullptr; }
	virtual void UnbindTexUnit(int no) {}
	virtual void TextureFilterChanged() {}
	virtual void BeginFrame() {}
	virtual void SetWindowSize(int w, int h) {}

	virtual int GetClientWidth() = 0;
	virtual int GetClientHeight() = 0;
	virtual void BlurScene(float amount) {}
    
    // Interface to hardware rendering resources
	virtual IShaderProgram *CreateShaderProgram() { return nullptr; }
	virtual IVertexBuffer *CreateVertexBuffer() { return nullptr; }
	virtual IIndexBuffer *CreateIndexBuffer() { return nullptr; }
	virtual IDataBuffer *CreateDataBuffer(int bindingpoint, bool ssbo) { return nullptr; }
	bool BuffersArePersistent() { return !!(hwcaps & RFL_BUFFER_STORAGE); }

	// Begin/End 2D drawing operations.
	void Begin2D() { isIn2D = true; }
	void End2D() { isIn2D = false; }

	void End2DAndUpdate()
	{
		DrawRateStuff();
		End2D();
		Update();
	}


	// Returns true if Begin2D has been called and 2D drawing is now active
	bool HasBegun2D() { return isIn2D; }

	// This is overridable in case Vulkan does it differently.
	virtual bool RenderTextureIsFlipped() const
	{
		return true;
	}

	// Report a game restart
	void SetClearColor(int color);
	virtual uint32_t GetCaps();
	virtual void WriteSavePic(player_t *player, FileWriter *file, int width, int height);
	virtual sector_t *RenderView(player_t *player) { return nullptr;  }

	// Screen wiping
	virtual FTexture *WipeStartScreen();
	virtual FTexture *WipeEndScreen();

	virtual void PostProcessScene(int fixedcm, const std::function<void()> &afterBloomDrawEndScene2D) { if (afterBloomDrawEndScene2D) afterBloomDrawEndScene2D(); }

	void ScaleCoordsFromWindow(int16_t &x, int16_t &y);

	uint64_t GetLastFPS() const { return LastCount; }

	// 2D Texture drawing
	void ClearClipRect() { clipleft = cliptop = 0; clipwidth = clipheight = -1; }
	void SetClipRect(int x, int y, int w, int h);
	void GetClipRect(int *x, int *y, int *w, int *h);

	virtual void Draw2D() {}
	void Clear2D() { m2DDrawer.Clear(); }

	// Dim part of the canvas
	void Dim(PalEntry color, float amount, int x1, int y1, int w, int h, FRenderStyle *style = nullptr);
	void DoDim(PalEntry color, float amount, int x1, int y1, int w, int h, FRenderStyle *style = nullptr);
	FVector4 CalcBlend(sector_t * viewsector, PalEntry *modulateColor);
	void DrawBlend(sector_t * viewsector);

	// Fill an area with a texture
	void FlatFill(int left, int top, int right, int bottom, FTexture *src, bool local_origin = false);

	// Fill a simple polygon with a texture
	void FillSimplePoly(FTexture *tex, FVector2 *points, int npoints,
		double originx, double originy, double scalex, double scaley, DAngle rotation,
		const FColormap &colormap, PalEntry flatcolor, int lightlevel, int bottomclip, uint32_t *indices, size_t indexcount);

	// Set an area to a specified color
	void Clear(int left, int top, int right, int bottom, int palcolor, uint32_t color);

	// Draws a line
	void DrawLine(int x0, int y0, int x1, int y1, int palColor, uint32_t realcolor, uint8_t alpha = 255);

	// Draws a line with thickness
	void DrawThickLine(int x0, int y0, int x1, int y1, double thickness, uint32_t realcolor, uint8_t alpha = 255);

	// Draws a single pixel
	void DrawPixel(int x, int y, int palcolor, uint32_t rgbcolor);


	bool SetTextureParms(DrawParms *parms, FTexture *img, double x, double y) const;
	void DrawTexture(FTexture *img, double x, double y, int tags, ...);
	void DrawTexture(FTexture *img, double x, double y, VMVa_List &);
	void DrawShape(FTexture *img, DShape2D *shape, int tags, ...);
	void DrawShape(FTexture *img, DShape2D *shape, VMVa_List &);
	void FillBorder(FTexture *img);	// Fills the border around a 4:3 part of the screen on non-4:3 displays
	void VirtualToRealCoords(double &x, double &y, double &w, double &h, double vwidth, double vheight, bool vbottom = false, bool handleaspect = true) const;

	// Code that uses these (i.e. SBARINFO) should probably be evaluated for using doubles all around instead.
	void VirtualToRealCoordsInt(int &x, int &y, int &w, int &h, int vwidth, int vheight, bool vbottom = false, bool handleaspect = true) const;

	// Text drawing functions -----------------------------------------------

#ifdef DrawText
#undef DrawText	// See WinUser.h for the definition of DrawText as a macro
#endif
	// 2D Text drawing
	void DrawText(FFont *font, int normalcolor, double x, double y, const char *string, int tag_first, ...);
	void DrawText(FFont *font, int normalcolor, double x, double y, const char *string, VMVa_List &args);
	void DrawChar(FFont *font, int normalcolor, double x, double y, int character, int tag_first, ...);
	void DrawChar(FFont *font, int normalcolor, double x, double y, int character, VMVa_List &args);
	void DrawText(FFont *font, int normalcolor, double x, double y, const char32_t *string, int tag_first, ...);

	void DrawFrame(int left, int top, int width, int height);
	void DrawBorder(FTextureID, int x1, int y1, int x2, int y2);

	// Calculate gamma table
	void CalcGamma(float gamma, uint8_t gammalookup[256]);

	virtual void SetViewportRects(IntRect *bounds);
	int ScreenToWindowX(int x);
	int ScreenToWindowY(int y);


	// Retrieves a buffer containing image data for a screenshot.
	// Hint: Pitch can be negative for upside-down images, in which case buffer
	// points to the last row in the buffer, which will be the first row output.
	virtual TArray<uint8_t> GetScreenshotBuffer(int &pitch, ESSType &color_type, float &gamma) { return TArray<uint8_t>(); }

	static float GetZNear() { return 5.f; }
	static float GetZFar() { return 65536.f; }

	// The original size of the framebuffer as selected in the video menu.
	uint64_t FrameTime = 0;

protected:
	void DrawRateStuff ();

private:

	uint64_t LastMS = 0, LastSec = 0, FrameCount = 0, LastCount = 0, LastTic = 0;

	bool isIn2D = false;
};


// This is the screen updated by I_FinishUpdate.
extern DFrameBuffer *screen;

#define SCREENWIDTH (screen->GetWidth ())
#define SCREENHEIGHT (screen->GetHeight ())
#define SCREENPITCH (screen->GetPitch ())

EXTERN_CVAR (Float, Gamma)


// Allocates buffer screens, call before R_Init.
void V_Init (bool restart);

// Initializes graphics mode for the first time.
void V_Init2 ();

void V_Shutdown ();

class FScanner;
struct FScriptPosition;
// Returns the closest color to the one desired. String
// should be of the form "rr gg bb".
int V_GetColorFromString (const uint32_t *palette, const char *colorstring, FScriptPosition *sc = nullptr);
// Scans through the X11R6RGB lump for a matching color
// and returns a color string suitable for V_GetColorFromString.
FString V_GetColorStringByName (const char *name, FScriptPosition *sc = nullptr);

// Tries to get color by name, then by string
int V_GetColor (const uint32_t *palette, const char *str, FScriptPosition *sc = nullptr);
int V_GetColor(const uint32_t *palette, FScanner &sc);

int CheckRatio (int width, int height, int *trueratio=NULL);
static inline int CheckRatio (double width, double height) { return CheckRatio(int(width), int(height)); }
inline bool IsRatioWidescreen(int ratio) { return (ratio & 3) != 0; }

float ActiveRatio (int width, int height, float *trueratio = NULL);
static inline double ActiveRatio (double width, double height) { return ActiveRatio(int(width), int(height)); }

int AspectBaseWidth(float aspect);
int AspectBaseHeight(float aspect);
double AspectPspriteOffset(float aspect);
int AspectMultiplier(float aspect);
bool AspectTallerThanWide(float aspect);
void ScaleWithAspect(int &w, int &h, int Width, int Height);

int GetUIScale(int altval);
int GetConScale(int altval);

EXTERN_CVAR(Int, uiscale);
EXTERN_CVAR(Int, con_scaletext);
EXTERN_CVAR(Int, con_scale);

inline int active_con_scaletext(bool newconfont = false)
{
	return newconfont? GetConScale(con_scaletext) : GetUIScale(con_scaletext);
}

inline int active_con_scale()
{
	return GetConScale(con_scale);
}


class ScaleOverrider
{
	int savedxfac, savedyfac, savedwidth, savedheight;

public:
	// This is to allow certain elements to use an optimal fullscreen scale which for the menu would be too large.
	// The old code contained far too much mess to compensate for the menus which negatively affected everything else.
	// However, for compatibility reasons the currently used variables cannot be changed so they have to be overridden temporarily.
	// This class provides a safe interface for this because it ensures that the values get restored afterward.
	// Currently, the intermission and the level summary screen use this.
	ScaleOverrider()
	{
		savedxfac = CleanXfac;
		savedyfac = CleanYfac;
		savedwidth = CleanWidth;
		savedheight = CleanHeight;
		V_CalcCleanFacs(320, 200, screen->GetWidth(), screen->GetHeight(), &CleanXfac, &CleanYfac);
		CleanWidth = screen->GetWidth() / CleanXfac;
		CleanHeight = screen->GetHeight() / CleanYfac;
	}

	~ScaleOverrider()
	{
		CleanXfac = savedxfac;
		CleanYfac = savedyfac;
		CleanWidth = savedwidth;
		CleanHeight = savedheight;
	}
};


#endif // __V_VIDEO_H__

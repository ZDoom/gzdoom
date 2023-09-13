#pragma once

#include "v_2ddrawer.h"
#include "c_cvars.h"
#include "intrect.h"

// TagItem definitions for DrawTexture. As far as I know, tag lists
// originated on the Amiga.
//
// Think of TagItems as an array of the following structure:
//
// struct TagItem {
//     uint32_t ti_Tag;
//     uint32_t ti_Data;
// };

enum tags : uint32_t
{
	TAG_DONE = (0),  /* Used to indicate the end of the Tag list */
	TAG_END = (0),  /* Ditto									*/
	/* list pointed to in ti_Data 				*/

	TAG_USER = ((uint32_t)(1u << 30))
};

enum
{
	FSMode_None = 0,
	FSMode_ScaleToFit = 1,
	FSMode_ScaleToFill = 2,
	FSMode_ScaleToFit43 = 3,
	FSMode_ScaleToScreen = 4,
	FSMode_ScaleToFit43Top = 5,
	FSMode_ScaleToFit43Bottom = 6,
	FSMode_ScaleToHeight = 7,


	FSMode_Max,

	// These all use ScaleToFit43, their purpose is to cut down on verbosity because they imply the virtual screen size.
	FSMode_Predefined = 1000,
	FSMode_Fit320x200 = 1000,
	FSMode_Fit320x240,
	FSMode_Fit640x400,
	FSMode_Fit640x480,
	FSMode_Fit320x200Top,
	FSMode_Predefined_Max,
};

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
	DTA_Localize,		// localize text
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

	DTA_FullscreenEx,
	DTA_FullscreenScale,
	DTA_ScaleX,
	DTA_ScaleY,

	DTA_ViewportX,			// Defines the viewport on the screen that should be rendered to.
	DTA_ViewportY,
	DTA_ViewportWidth,
	DTA_ViewportHeight,
	DTA_CenterOffsetRel,	// Apply texture offsets relative to center, instead of top left. This is standard alignment for Build's 2D content.
	DTA_TopLeft,			// always align to top left. Added to have a boolean condition for this alignment.
	DTA_Pin,				// Pin a non-widescreen image to the left/right edge of the screen.
	DTA_Rotate,
	DTA_FlipOffsets,		// Flips offsets when using DTA_FlipX and DTA_FlipY, this cannot be automatic due to unexpected behavior with unoffsetted graphics.
	DTA_Indexed,			// Use an indexed texture combined with the given translation.
	DTA_CleanTop,			// Like DTA_Clean but aligns to the top of the screen instead of the center.
	DTA_NoOffset,			// Ignore 2D drawer's offset.

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
	int TranslationId;
	PalEntry colorOverlay;
	PalEntry color;
	int alphaChannel;
	int flipX;
	int flipY;
	//float shadowAlpha;
	int shadowColor;
	int keepratio;
	int masked;
	int bilinear;
	FRenderStyle style;
	struct FSpecialColormap *specialcolormap;
	int desaturate;
	int scalex, scaley;
	int cellx, celly;
	int monospace;
	int spacing;
	int maxstrlen;
	bool localize;
	bool fortext;
	bool virtBottom;
	bool burn;
	bool flipoffsets;
	bool indexed;
	bool nooffset;
	int8_t fsscalemode;
	double srcx, srcy;
	double srcwidth, srcheight;
	double patchscalex, patchscaley;
	double rotateangle;
	IntRect viewport;

	bool vertexColorChange(const DrawParms& other) {
		return
			this->Alpha         != other.Alpha         ||
			this->fillcolor     != other.fillcolor     ||
			this->colorOverlay  != other.colorOverlay  ||
			this->color         != other.color         ||
			this->style.Flags   != other.style.Flags   ||
			this->style.BlendOp != other.style.BlendOp ||
			this->desaturate    != other.desaturate;
	}
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

float ActiveRatio (int width, int height, float *trueratio = NULL);
inline double ActiveRatio (double width, double height) { return ActiveRatio(int(width), int(height)); }

int AspectBaseWidth(float aspect);
int AspectBaseHeight(float aspect);
double AspectPspriteOffset(float aspect);
int AspectMultiplier(float aspect);
bool AspectTallerThanWide(float aspect);

extern F2DDrawer* twod;

int GetUIScale(F2DDrawer* drawer, int altval);
int GetConScale(F2DDrawer* drawer, int altval);

EXTERN_CVAR(Int, uiscale);
EXTERN_CVAR(Int, con_scale);

inline int active_con_scale(F2DDrawer *drawer)
{
	return GetConScale(drawer, con_scale);
}

#ifdef DrawText
#undef DrawText	// See WinUser.h for the definition of DrawText as a macro
#endif

enum
{
	DrawTexture_Normal,
	DrawTexture_Text,
	DrawTexture_Fill,
};

template<class T>
bool ParseDrawTextureTags(F2DDrawer *drawer, FGameTexture* img, double x, double y, uint32_t tag, T& tags, DrawParms* parms, int type, PalEntry fill = ~0u, double fillalpha = 0.0, bool scriptDifferences = false);

template<class T>
void DrawTextCommon(F2DDrawer *drawer, FFont* font, int normalcolor, double x, double y, const T* string, DrawParms& parms);
bool SetTextureParms(F2DDrawer *drawer, DrawParms* parms, FGameTexture* img, double x, double y);

void GetFullscreenRect(double width, double height, int fsmode, DoubleRect* rect);

void DrawText(F2DDrawer* drawer, FFont* font, int normalcolor, double x, double y, const char* string, int tag_first, ...);
void DrawText(F2DDrawer* drawer, FFont* font, int normalcolor, double x, double y, const char32_t* string, int tag_first, ...);
void DrawChar(F2DDrawer* drawer, FFont* font, int normalcolor, double x, double y, int character, int tag_first, ...);

void DrawTexture(F2DDrawer* drawer, FGameTexture* img, double x, double y, int tags_first, ...);
void DrawTexture(F2DDrawer *drawer, FTextureID texid, bool animate, double x, double y, int tags_first, ...);

void DoDim(F2DDrawer* drawer, PalEntry color, float amount, int x1, int y1, int w, int h, FRenderStyle* style = nullptr);
void Dim(F2DDrawer* drawer, PalEntry color, float damount, int x1, int y1, int w, int h, FRenderStyle* style = nullptr);

void DrawBorder(F2DDrawer* drawer, FTextureID, int x1, int y1, int x2, int y2);
void DrawFrame(F2DDrawer* twod, PalEntry color, int left, int top, int width, int height, int thickness);

// Set an area to a specified color
void ClearRect(F2DDrawer* drawer, int left, int top, int right, int bottom, int palcolor, uint32_t color);

void VirtualToRealCoords(F2DDrawer* drawer, double& x, double& y, double& w, double& h, double vwidth, double vheight, bool vbottom = false, bool handleaspect = true);

// Code that uses these (i.e. SBARINFO) should probably be evaluated for using doubles all around instead.
void VirtualToRealCoordsInt(F2DDrawer* drawer, int& x, int& y, int& w, int& h, int vwidth, int vheight, bool vbottom = false, bool handleaspect = true);

extern int CleanWidth, CleanHeight, CleanXfac, CleanYfac;
extern int CleanWidth_1, CleanHeight_1, CleanXfac_1, CleanYfac_1;

void V_CalcCleanFacs(int designwidth, int designheight, int realwidth, int realheight, int* cleanx, int* cleany, int* cx1 = NULL, int* cx2 = NULL);

class ScaleOverrider
{
	int savedxfac, savedyfac, savedwidth, savedheight;

public:
	// This is to allow certain elements to use an optimal fullscreen scale which for the menu would be too large.
	// The old code contained far too much mess to compensate for the menus which negatively affected everything else.
	// However, for compatibility reasons the currently used variables cannot be changed so they have to be overridden temporarily.
	// This class provides a safe interface for this because it ensures that the values get restored afterward.
	// Currently, the intermission and the level summary screen use this.
	ScaleOverrider(F2DDrawer *drawer)
	{
		savedxfac = CleanXfac;
		savedyfac = CleanYfac;
		savedwidth = CleanWidth;
		savedheight = CleanHeight;

		if (drawer)
		{
			V_CalcCleanFacs(320, 200, drawer->GetWidth(), drawer->GetHeight(), &CleanXfac, &CleanYfac);
			CleanWidth = drawer->GetWidth() / CleanXfac;
			CleanHeight = drawer->GetHeight() / CleanYfac;
		}
	}

	~ScaleOverrider()
	{
		CleanXfac = savedxfac;
		CleanYfac = savedyfac;
		CleanWidth = savedwidth;
		CleanHeight = savedheight;
	}
};

void Draw2D(F2DDrawer* drawer, FRenderState& state);
void Draw2D(F2DDrawer* drawer, FRenderState& state, int x, int y, int width, int height);

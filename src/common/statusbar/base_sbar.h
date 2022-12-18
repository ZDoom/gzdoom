#pragma once

#include "dobject.h"
#include "textureid.h"
#include "zstring.h"
#include "v_draw.h"

class FGameTexture;
class FFont;
extern FGameTexture* CrosshairImage;
void ST_LoadCrosshair(int num, bool alwaysload);
void ST_UnloadCrosshair();
void ST_DrawCrosshair(int phealth, double xpos, double ypos, double scale, DAngle angle = nullAngle);


enum DI_Flags
{
	DI_SKIPICON = 0x1,
	DI_SKIPALTICON = 0x2,
	DI_SKIPSPAWN = 0x4,
	DI_SKIPREADY = 0x8,
	DI_ALTICONFIRST = 0x10,


	DI_TRANSLATABLE = 0x20,
	DI_FORCESCALE = 0x40,
	DI_DIM = 0x80,
	DI_DRAWCURSORFIRST = 0x100,	// only for DrawInventoryBar.
	DI_ALWAYSSHOWCOUNT = 0x200,	// only for DrawInventoryBar.
	DI_DIMDEPLETED = 0x400,
	DI_DONTANIMATE = 0x800,		// do not animate the texture
	DI_MIRROR = 0x1000,		// flip the texture horizontally, like a mirror
	DI_ITEM_RELCENTER = 0x2000,
	DI_MIRRORY = 0x40000000,

	DI_SCREEN_AUTO = 0,					// decide based on given offsets.
	DI_SCREEN_MANUAL_ALIGN = 0x4000,	// If this is on, the following flags will have an effect

	DI_SCREEN_TOP = DI_SCREEN_MANUAL_ALIGN,
	DI_SCREEN_VCENTER = 0x8000 | DI_SCREEN_MANUAL_ALIGN,
	DI_SCREEN_BOTTOM = 0x10000 | DI_SCREEN_MANUAL_ALIGN,
	DI_SCREEN_VOFFSET = 0x18000 | DI_SCREEN_MANUAL_ALIGN,
	DI_SCREEN_VMASK = 0x18000 | DI_SCREEN_MANUAL_ALIGN,

	DI_SCREEN_LEFT = DI_SCREEN_MANUAL_ALIGN,
	DI_SCREEN_HCENTER = 0x20000 | DI_SCREEN_MANUAL_ALIGN,
	DI_SCREEN_RIGHT = 0x40000 | DI_SCREEN_MANUAL_ALIGN,
	DI_SCREEN_HOFFSET = 0x60000 | DI_SCREEN_MANUAL_ALIGN,
	DI_SCREEN_HMASK = 0x60000 | DI_SCREEN_MANUAL_ALIGN,

	DI_SCREEN_LEFT_TOP = DI_SCREEN_TOP | DI_SCREEN_LEFT,
	DI_SCREEN_RIGHT_TOP = DI_SCREEN_TOP | DI_SCREEN_RIGHT,
	DI_SCREEN_LEFT_BOTTOM = DI_SCREEN_BOTTOM | DI_SCREEN_LEFT,
	DI_SCREEN_LEFT_CENTER = DI_SCREEN_VCENTER | DI_SCREEN_LEFT,
	DI_SCREEN_RIGHT_BOTTOM = DI_SCREEN_BOTTOM | DI_SCREEN_RIGHT,
	DI_SCREEN_RIGHT_CENTER = DI_SCREEN_VCENTER | DI_SCREEN_RIGHT,
	DI_SCREEN_CENTER = DI_SCREEN_VCENTER | DI_SCREEN_HCENTER,
	DI_SCREEN_CENTER_TOP = DI_SCREEN_TOP | DI_SCREEN_HCENTER,
	DI_SCREEN_CENTER_BOTTOM = DI_SCREEN_BOTTOM | DI_SCREEN_HCENTER,
	DI_SCREEN_OFFSETS = DI_SCREEN_HOFFSET | DI_SCREEN_VOFFSET,

	DI_ITEM_AUTO = 0,		// equivalent with bottom center, which is the default alignment.

	DI_ITEM_TOP = 0x80000,
	DI_ITEM_VCENTER = 0x100000,
	DI_ITEM_BOTTOM = 0,		// this is the default vertical alignment
	DI_ITEM_VOFFSET = 0x180000,
	DI_ITEM_VMASK = 0x180000,

	DI_ITEM_LEFT = 0x200000,
	DI_ITEM_HCENTER = 0,	// this is the deafault horizontal alignment
	DI_ITEM_RIGHT = 0x400000,
	DI_ITEM_HOFFSET = 0x600000,
	DI_ITEM_HMASK = 0x600000,

	DI_ITEM_LEFT_TOP = DI_ITEM_TOP | DI_ITEM_LEFT,
	DI_ITEM_RIGHT_TOP = DI_ITEM_TOP | DI_ITEM_RIGHT,
	DI_ITEM_LEFT_BOTTOM = DI_ITEM_BOTTOM | DI_ITEM_LEFT,
	DI_ITEM_RIGHT_BOTTOM = DI_ITEM_BOTTOM | DI_ITEM_RIGHT,
	DI_ITEM_CENTER = DI_ITEM_VCENTER | DI_ITEM_HCENTER,
	DI_ITEM_CENTER_BOTTOM = DI_ITEM_BOTTOM | DI_ITEM_HCENTER,
	DI_ITEM_OFFSETS = DI_ITEM_HOFFSET | DI_ITEM_VOFFSET,

	DI_TEXT_ALIGN_LEFT = 0,
	DI_TEXT_ALIGN_RIGHT = 0x800000,
	DI_TEXT_ALIGN_CENTER = 0x1000000,
	DI_TEXT_ALIGN = 0x1800000,

	DI_ALPHAMAPPED = 0x2000000,
	DI_NOSHADOW = 0x4000000,
	DI_ALWAYSSHOWCOUNTERS = 0x8000000,
	DI_ARTIFLASH = 0x10000000,
	DI_FORCEFILL = 0x20000000,

	// These 2 flags are only used by SBARINFO so these duplicate other flags not used by SBARINFO
	DI_DRAWINBOX = DI_TEXT_ALIGN_RIGHT,
	DI_ALTERNATEONFAIL = DI_TEXT_ALIGN_CENTER,

};

//============================================================================
//
// encapsulates all settings a HUD font may need
//
//============================================================================

class DHUDFont : public DObject 
{
	// this blocks CreateNew on this class which is the intent here.
	DECLARE_CLASS(DHUDFont, DObject);

public:
	FFont* mFont;
	int mSpacing;
	EMonospacing mMonospacing;
	int mShadowX;
	int mShadowY;

	DHUDFont() = default;

	DHUDFont(FFont* f, int sp, EMonospacing ms, int sx, int sy)
		: mFont(f), mSpacing(sp), mMonospacing(ms), mShadowX(sx), mShadowY(sy)
	{}
};




class DStatusBarCore : public DObject
{
	DECLARE_CLASS(DStatusBarCore, DObject)

protected:



public:

	enum EAlign
	{
		TOP = 0,
		VCENTER = 1,
		BOTTOM = 2,
		VOFFSET = 3,
		VMASK = 3,

		LEFT = 0,
		HCENTER = 4,
		RIGHT = 8,
		HOFFSET = 12,
		HMASK = 12,

		CENTER = VCENTER | HCENTER,
		CENTER_BOTTOM = BOTTOM | HCENTER
	};

	int ST_X;
	int ST_Y;
	int SBarTop;
	int RelTop;
	int HorizontalResolution, VerticalResolution;
	double Alpha = 1.;
	DVector2 SBarScale;
	DVector2 drawOffset = { 0,0 };			// can be set by subclasses to offset drawing operations
	DVector2 defaultScale;					// factor for clean fully scaled display.
	double drawClip[4] = { 0,0,0,0 };		// defines a clipping rectangle (not used yet)
	bool fullscreenOffsets = false;			// current screen is displayed with fullscreen behavior.
	bool ForcedScale = false;
	bool CompleteBorder = false;

	int BaseRelTop;
	int BaseSBarHorizontalResolution;
	int BaseSBarVerticalResolution;
	int BaseHUDHorizontalResolution;
	int BaseHUDVerticalResolution;


	void BeginStatusBar(int resW, int resH, int relTop, bool forceScaled = false);
	void BeginHUD(int resW, int resH, double Alpha, bool forceScaled = false);
	void SetSize(int reltop = 32, int hres = 320, int vres = 200, int hhres = -1, int hvres = -1);
	virtual DVector2 GetHUDScale() const;
	virtual uint32_t GetTranslation() const { return 0; }
	void SetDrawSize(int reltop, int hres, int vres);
	virtual void SetScale();
	void ValidateResolution(int& hres, int& vres) const;
	void StatusbarToRealCoords(double& x, double& y, double& w, double& h) const;
	void DrawGraphic(FGameTexture* texture, double x, double y, int flags, double Alpha, double boxwidth, double boxheight, double scaleX, double scaleY, ERenderStyle style = STYLE_Translucent, PalEntry color = 0xffffffff, int translation = 0, double clipwidth = -1.0);
	void DrawGraphic(FTextureID texture, double x, double y, int flags, double Alpha, double boxwidth, double boxheight, double scaleX, double scaleY, ERenderStyle style = STYLE_Translucent, PalEntry color = 0xffffffff, int translation = 0, double clipwidth = -1.0);
	void DrawRotated(FTextureID texture, double x, double y, int flags, double angle, double Alpha, double scaleX, double scaleY, PalEntry color = 0xffffffff, int translation = 0, ERenderStyle style = STYLE_Translucent);
	void DrawRotated(FGameTexture* tex, double x, double y, int flags, double angle, double Alpha, double scaleX, double scaleY, PalEntry color = 0xffffffff, int translation = 0, ERenderStyle style = STYLE_Translucent);
	void DrawString(FFont* font, const FString& cstring, double x, double y, int flags, double Alpha, int translation, int spacing, EMonospacing monospacing, int shadowX, int shadowY, double scaleX, double scaleY, int pt, int style);
	void TransformRect(double& x, double& y, double& w, double& h, int flags = 0);
	void Fill(PalEntry color, double x, double y, double w, double h, int flags = 0);
	void SetClipRect(double x, double y, double w, double h, int flags = 0);

};
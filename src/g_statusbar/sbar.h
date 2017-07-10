/*
** sbar.h
** Base status bar definition
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#ifndef __SBAR_H__
#define __SBAR_H__

#include "dobject.h"
#include "v_collection.h"
#include "v_text.h"
#include "r_data/renderstyle.h"

class player_t;
struct FRemapTable;

enum EHudState
{
	HUD_StatusBar,
	HUD_Fullscreen,
	HUD_None,

	HUD_AltHud // Used for passing through popups to the alt hud
};

class AWeapon;

bool ST_IsTimeVisible();
bool ST_IsLatencyVisible();

// HUD Message base object --------------------------------------------------

class DHUDMessage : public DObject
{
	DECLARE_CLASS (DHUDMessage, DObject)
	HAS_OBJECT_POINTERS
public:
	DHUDMessage (FFont *font, const char *text, float x, float y, int hudwidth, int hudheight,
		EColorRange textColor, float holdTime);
	virtual void OnDestroy () override;

	virtual void Serialize(FSerializer &arc);

	void Draw (int bottom, int visibility);
	virtual void ResetText (const char *text);
	virtual void DrawSetup ();
	virtual void DoDraw (int linenum, int x, int y, bool clean, int hudheight);
	virtual bool Tick ();	// Returns true to indicate time for removal
	virtual void ScreenSizeChanged ();

	void SetVisibility(int vis)
	{
		VisibilityFlags = vis;
	}
	void SetRenderStyle(ERenderStyle style)
	{
		Style = style;
	}
	void SetAlpha(float alpha)
	{
		Alpha = alpha;
	}
	void SetNoWrap(bool nowrap)
	{
		NoWrap = nowrap;
		ResetText(SourceText);
	}
	void SetClipRect(int x, int y, int width, int height, bool aspect)
	{
		ClipX = x;
		ClipY = y;
		ClipWidth = width;
		ClipHeight = height;
		HandleAspect = aspect;
	}
	void SetWrapWidth(int wrap)
	{
		WrapWidth = wrap;
		ResetText(SourceText);
	}

protected:
	FBrokenLines *Lines;
	int Width, Height, NumLines;
	float Left, Top;
	bool CenterX, NoWrap;
	int HoldTics;
	int Tics;
	int State;
	int VisibilityFlags;
	int HUDWidth, HUDHeight;
	int ClipX, ClipY, ClipWidth, ClipHeight, WrapWidth;	// in HUD coords
	int ClipLeft, ClipTop, ClipRight, ClipBot;			// in screen coords
	bool HandleAspect;
	EColorRange TextColor;
	FFont *Font;
	FRenderStyle Style;
	double Alpha;

	void CalcClipCoords(int hudheight);
	DHUDMessage () : SourceText(NULL) {}

private:
	TObjPtr<DHUDMessage*> Next;
	uint32_t SBarID;
	char *SourceText;

	friend class DBaseStatusBar;
};

// HUD message visibility flags
enum
{
	HUDMSG_NotWith3DView		= 1,
	HUDMSG_NotWithFullMap		= 2,
	HUDMSG_NotWithOverlayMap	= 4,
};

// HUD Message; appear instantly, then fade out type ------------------------

class DHUDMessageFadeOut : public DHUDMessage
{
	DECLARE_CLASS (DHUDMessageFadeOut, DHUDMessage)
public:
	DHUDMessageFadeOut (FFont *font, const char *text, float x, float y, int hudwidth, int hudheight,
		EColorRange textColor, float holdTime, float fadeOutTime);

	virtual void Serialize(FSerializer &arc);
	virtual void DoDraw (int linenum, int x, int y, bool clean, int hudheight);
	virtual bool Tick ();

protected:
	int FadeOutTics;

	DHUDMessageFadeOut() {}
};

// HUD Message; fade in, then fade out type ---------------------------------

class DHUDMessageFadeInOut : public DHUDMessageFadeOut
{
	DECLARE_CLASS (DHUDMessageFadeInOut, DHUDMessageFadeOut)
public:
	DHUDMessageFadeInOut (FFont *font, const char *text, float x, float y, int hudwidth, int hudheight,
		EColorRange textColor, float holdTime, float fadeInTime, float fadeOutTime);

	virtual void Serialize(FSerializer &arc);
	virtual void DoDraw (int linenum, int x, int y, bool clean, int hudheight);
	virtual bool Tick ();

protected:
	int FadeInTics;

	DHUDMessageFadeInOut() {}
};

// HUD Message; type on, then fade out type ---------------------------------

class DHUDMessageTypeOnFadeOut : public DHUDMessageFadeOut
{
	DECLARE_CLASS (DHUDMessageTypeOnFadeOut, DHUDMessageFadeOut)
public:
	DHUDMessageTypeOnFadeOut (FFont *font, const char *text, float x, float y, int hudwidth, int hudheight,
		EColorRange textColor, float typeTime, float holdTime, float fadeOutTime);

	virtual void Serialize(FSerializer &arc);
	virtual void DoDraw (int linenum, int x, int y, bool clean, int hudheight);
	virtual bool Tick ();
	virtual void ScreenSizeChanged ();

protected:
	float TypeOnTime;
	int CurrLine;
	int LineVisible;
	int LineLen;

	DHUDMessageTypeOnFadeOut() {}
};

// Mug shots ----------------------------------------------------------------

struct FMugShotFrame
{
	TArray<FString> Graphic;
	int Delay;

	FMugShotFrame();
	~FMugShotFrame();
	FTexture *GetTexture(const char *default_face, const char *skin_face, int random, int level=0,
		int direction=0, bool usesLevels=false, bool health2=false, bool healthspecial=false,
		bool directional=false);
};

struct FMugShotState
{
	uint8_t bUsesLevels:1;
	uint8_t bHealth2:1;		// Health level is the 2nd character from the end.
	uint8_t bHealthSpecial:1;	// Like health2 only the 2nd frame gets the normal health type.
	uint8_t bDirectional:1;	// Faces direction of damage.
	uint8_t bFinished:1;

	unsigned int Position;
	int Time;
	int Random;
	FName State;
	TArray<FMugShotFrame> Frames;

	FMugShotState(FName name);
	~FMugShotState();
	void Tick();
	void Reset();
	FMugShotFrame &GetCurrentFrame() { return Frames[Position]; }
	FTexture *GetCurrentFrameTexture(const char *default_face, const char *skin_face, int level=0, int direction=0)
	{
		return GetCurrentFrame().GetTexture(default_face, skin_face, Random, level, direction, bUsesLevels, bHealth2, bHealthSpecial, bDirectional);
	}
private:
	FMugShotState();
};

class player_t;
class FMugShot
{
	public:
		enum StateFlags
		{
			STANDARD = 0x0,

			XDEATHFACE = 0x1,
			ANIMATEDGODMODE = 0x2,
			DISABLEGRIN = 0x4,
			DISABLEOUCH = 0x8,
			DISABLEPAIN = 0x10,
			DISABLERAMPAGE = 0x20,
			CUSTOM = 0x40,
		};

		FMugShot();
		void Grin(bool grin=true) { bEvilGrin = grin; }
		void Reset();
		void Tick(player_t *player);
		bool SetState(const char *state_name, bool wait_till_done=false, bool reset=false);
		int UpdateState(player_t *player, StateFlags stateflags=STANDARD);
		FTexture *GetFace(player_t *player, const char *default_face, int accuracy, StateFlags stateflags=STANDARD);

	private:
		FMugShotState *CurrentState;
		int RampageTimer;
		int LastDamageAngle;
		int FaceHealth;
		bool bEvilGrin;
		bool bDamageFaceActive;
		bool bNormal;
		bool bOuchActive;
};

extern TArray<FMugShotState> MugShotStates;

FMugShotState *FindMugShotState(FName state);
int FindMugShotStateIndex(FName state);

// Base Status Bar ----------------------------------------------------------

class FTexture;

enum
{
	HUDMSGLayer_OverHUD,
	HUDMSGLayer_UnderHUD,
	HUDMSGLayer_OverMap,

	NUM_HUDMSGLAYERS,
	HUDMSGLayer_Default = HUDMSGLayer_OverHUD,
};

class DBaseStatusBar : public DObject
{
	friend class DSBarInfo;
	DECLARE_CLASS (DBaseStatusBar, DObject)
	HAS_OBJECT_POINTERS
public:
	// Popup screens for Strife's status bar
	enum
	{
		POP_NoChange = -1,
		POP_None,
		POP_Log,
		POP_Keys,
		POP_Status
	};

	// Status face stuff
	enum
	{
		ST_NUMPAINFACES		= 5,
		ST_NUMSTRAIGHTFACES	= 3,
		ST_NUMTURNFACES		= 2,
		ST_NUMSPECIALFACES	= 3,
		ST_NUMEXTRAFACES	= 2,
		ST_FACESTRIDE		= ST_NUMSTRAIGHTFACES+ST_NUMTURNFACES+ST_NUMSPECIALFACES,
		ST_NUMFACES			= ST_FACESTRIDE*ST_NUMPAINFACES+ST_NUMEXTRAFACES,

		ST_TURNOFFSET		= ST_NUMSTRAIGHTFACES,
		ST_OUCHOFFSET		= ST_TURNOFFSET + ST_NUMTURNFACES,
		ST_EVILGRINOFFSET	= ST_OUCHOFFSET + 1,
		ST_RAMPAGEOFFSET	= ST_EVILGRINOFFSET + 1,
		ST_GODFACE			= ST_NUMPAINFACES*ST_FACESTRIDE,
		ST_DEADFACE			= ST_GODFACE + 1
	};


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

	DBaseStatusBar ();
	void SetSize(int reltop = 32, int hres = 320, int vres = 200, int hhres = -1, int hvres = -1);
	void OnDestroy() override;

	void AttachMessage (DHUDMessage *msg, uint32_t id=0, int layer=HUDMSGLayer_Default);
	DHUDMessage *DetachMessage (DHUDMessage *msg);
	DHUDMessage *DetachMessage (uint32_t id);
	void DetachAllMessages ();
	void ShowPlayerName ();
	double GetDisplacement() { return Displacement; }
	int GetPlayer ();

	static void AddBlend (float r, float g, float b, float a, float v_blend[4]);

	// do not make this a DObject Serialize function because it's not used like one!
	void SerializeMessages(FSerializer &arc);

	void SetScale();
	virtual void Tick ();
	void CallTick();
	virtual void Draw (EHudState state);
	void CallDraw(EHudState state);
			void DrawBottomStuff (EHudState state);
			void DrawTopStuff (EHudState state);
	void FlashItem (const PClass *itemtype);
	void AttachToPlayer(player_t *player);
	DVector2 GetHUDScale() const;
	virtual void FlashCrosshair ();
	virtual void BlendView (float blend[4]);
	void NewGame ();
	virtual void ScreenSizeChanged ();
	void CallScreenSizeChanged();
	void ShowPop (int popnum);
	virtual bool MustDrawLog(EHudState state);
	virtual void SetMugShotState (const char *state_name, bool wait_till_done=false, bool reset=false);
	void DrawLog();
	uint32_t GetTranslation() const;

	void DrawGraphic(FTextureID texture, double x, double y, int flags, double Alpha, double boxwidth, double boxheight, double scaleX, double scaleY);
	void DrawString(FFont *font, const FString &cstring, double x, double y, int flags, double Alpha, int translation, int spacing, bool monospaced, int shadowX, int shadowY);
	void TransformRect(double &x, double &y, double &w, double &h, int flags = 0);
	void Fill(PalEntry color, double x, double y, double w, double h, int flags = 0);
	void SetClipRect(double x, double y, double w, double h, int flags = 0);

	void BeginStatusBar(int resW, int resH, int relTop, bool forceScaled);
	void BeginHUD(int resW, int resH, double Alpha, bool forceScaled = false);
	bool ForceHUDScale(bool on) { std::swap(ForcedScale, on); return on; }	// This is for SBARINFO which should not use BeginStatusBar or BeginHUD.
	void StatusbarToRealCoords(double &x, double &y, double &w, double &h) const;
	int GetTopOfStatusbar() const
	{
		return SBarTop;
	}

//protected:
	void DrawPowerups ();

	
	void RefreshBackground () const;

public:

	AInventory *ValidateInvFirst (int numVisible) const;
	void DrawCrosshair ();

	// Sizing info for ths status bar.
	int ST_X;
	int ST_Y;
	int SBarTop;
	DVector2 SBarScale;
	int RelTop;
	int HorizontalResolution, VerticalResolution;
	bool Scaled;							// This needs to go away.
	DVector2 defaultScale;					// factor for fully scaled fullscreen display.
	bool ForcedScale = false;

	bool Centering;
	bool FixedOrigin;
	bool CompleteBorder;
	double CrosshairSize;
	double Displacement;
	bool ShowLog;
	int artiflashTick = 0;
	double itemflashFade = 0.75;

	player_t *CPlayer;

	double Alpha = 1.;
	DVector2 drawOffset = { 0,0 };			// can be set by subclasses to offset drawing operations
	double drawClip[4] = { 0,0,0,0 };		// defines a clipping rectangle (not used yet)
	bool fullscreenOffsets = false;			// current screen is displayed with fullscreen behavior.
	FMugShot mugshot;

private:
	bool RepositionCoords (int &x, int &y, int xo, int yo, const int w, const int h) const;
	void DrawMessages (int layer, int bottom);
	void DrawConsistancy () const;
	void DrawWaiting () const;
	void SetDrawSize(int reltop, int hres, int vres);

	TObjPtr<DHUDMessage*> Messages[NUM_HUDMSGLAYERS];

	int BaseRelTop;
	int BaseSBarHorizontalResolution;
	int BaseSBarVerticalResolution;
	int BaseHUDHorizontalResolution;
	int BaseHUDVerticalResolution;

};

extern DBaseStatusBar *StatusBar;

// Status bar factories -----------------------------------------------------

DBaseStatusBar *CreateCustomStatusBar(int script=0);

// Crosshair stuff ----------------------------------------------------------

void ST_FormatMapName(FString &mapname, const char *mapnamecolor = "");
void ST_LoadCrosshair(bool alwaysload=false);
void ST_Clear();
void ST_CreateStatusBar(bool bTitleLevel);
extern FTexture *CrosshairImage;

FTextureID GetInventoryIcon(AInventory *item, uint32_t flags, bool *applyscale);


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
		
	DI_SCREEN_LEFT_TOP = DI_SCREEN_TOP|DI_SCREEN_LEFT,
	DI_SCREEN_RIGHT_TOP = DI_SCREEN_TOP|DI_SCREEN_RIGHT,
	DI_SCREEN_LEFT_BOTTOM = DI_SCREEN_BOTTOM|DI_SCREEN_LEFT,
	DI_SCREEN_RIGHT_BOTTOM = DI_SCREEN_BOTTOM|DI_SCREEN_RIGHT,
	DI_SCREEN_CENTER = DI_SCREEN_VCENTER|DI_SCREEN_HCENTER,
	DI_SCREEN_CENTER_BOTTOM = DI_SCREEN_BOTTOM|DI_SCREEN_HCENTER,
	DI_SCREEN_OFFSETS = DI_SCREEN_HOFFSET|DI_SCREEN_VOFFSET,
		
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
		
	DI_ITEM_LEFT_TOP = DI_ITEM_TOP|DI_ITEM_LEFT,
	DI_ITEM_RIGHT_TOP = DI_ITEM_TOP|DI_ITEM_RIGHT,
	DI_ITEM_LEFT_BOTTOM = DI_ITEM_BOTTOM|DI_ITEM_LEFT,
	DI_ITEM_RIGHT_BOTTOM = DI_ITEM_BOTTOM|DI_ITEM_RIGHT,
	DI_ITEM_CENTER = DI_ITEM_VCENTER|DI_ITEM_HCENTER,
	DI_ITEM_CENTER_BOTTOM = DI_ITEM_BOTTOM|DI_ITEM_HCENTER,
	DI_ITEM_OFFSETS = DI_ITEM_HOFFSET|DI_ITEM_VOFFSET,
		
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

#endif /* __SBAR_H__ */

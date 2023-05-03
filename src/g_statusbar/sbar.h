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

#include "base_sbar.h"
#include "v_collection.h"
#include "v_text.h"
#include "renderstyle.h"
#include "v_2ddrawer.h"
#include "v_draw.h"
#include "c_cvars.h"


EXTERN_CVAR(Int, con_scaletext);
inline int active_con_scaletext(F2DDrawer* drawer, bool newconfont = false)
{
	return newconfont ? GetConScale(drawer, con_scaletext) : GetUIScale(drawer, con_scaletext);
}

class player_t;
struct FRemapTable;
class FGameTexture;

enum EHudState
{
	HUD_StatusBar,
	HUD_Fullscreen,
	HUD_None,

	HUD_AltHud // Used for passing through popups to the alt hud
};

enum EMonospacing : int;

// HUD Message base object --------------------------------------------------

// This is a mo-op base class to allow derived ZScript message types that can be managed by the status bar.
class DHUDMessageBase : public DObject
{
	DECLARE_CLASS(DHUDMessageBase, DObject)
	HAS_OBJECT_POINTERS

public:
	virtual void Serialize(FSerializer &arc);
	virtual bool Tick() { return true; }	// Returns true to indicate time for removal
	virtual void ScreenSizeChanged() {}
	virtual void Draw(int bottom, int visibility) {}

	bool CallTick();	// Returns true to indicate time for removal
	void CallScreenSizeChanged();
	void CallDraw(int bottom, int visibility);

private:
	TObjPtr<DHUDMessageBase*> Next = MakeObjPtr<DHUDMessageBase*>(nullptr);
;
	uint32_t SBarID = 0;
	friend class DBaseStatusBar;

};

// HUD Message  --------------------------------------------------

class DHUDMessage : public DHUDMessageBase
{
	DECLARE_CLASS (DHUDMessage, DHUDMessageBase)
public:
	DHUDMessage (FFont *font, const char *text, float x, float y, int hudwidth, int hudheight,
		EColorRange textColor, float holdTime);
	virtual void OnDestroy () override;

	virtual void Serialize(FSerializer &arc);

	virtual void Draw (int bottom, int visibility) override;
	virtual void ResetText (const char *text);
	virtual void DrawSetup ();
	virtual void DoDraw (int linenum, int x, int y, bool clean, int hudheight);
	virtual bool Tick () override;
	virtual void ScreenSizeChanged () override;

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
	TArray<FBrokenLines> Lines;
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
	char *SourceText;

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
		EColorRange textColor, float typeTime, float holdTime, float fadeOutTimee);

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
	FGameTexture *GetTexture(const char *default_face, const char *skin_face, int random, int level=0,
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
	FName State = NAME_None;
	TArray<FMugShotFrame> Frames;

	FMugShotState(FName name);
	void Tick();
	void Reset();
	FMugShotFrame &GetCurrentFrame() { return Frames[Position]; }
	FGameTexture *GetCurrentFrameTexture(const char *default_face, const char *skin_face, int level=0, int direction=0)
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
		FGameTexture *GetFace(player_t *player, const char *default_face, int accuracy, StateFlags stateflags=STANDARD);

	private:
		FMugShotState *CurrentState;
		int RampageTimer;
		int LastDamageAngle;
		int FaceHealthNow;
		int FaceHealthLast;
		bool bEvilGrin;
		bool bDamageFaceActive;
		bool bNormal;
		bool bOuchActive;
};

extern TArray<FMugShotState> MugShotStates;

FMugShotState *FindMugShotState(FName state);
int FindMugShotStateIndex(FName state);

// Base Status Bar ----------------------------------------------------------

class FGameTexture;

enum
{
	HUDMSGLayer_OverHUD,
	HUDMSGLayer_UnderHUD,
	HUDMSGLayer_OverMap,

	NUM_HUDMSGLAYERS,
	HUDMSGLayer_Default = HUDMSGLayer_OverHUD,
};


class DBaseStatusBar : public DStatusBarCore
{
	friend class DSBarInfo;
	DECLARE_CLASS (DBaseStatusBar, DStatusBarCore)
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


	DBaseStatusBar ();
	void OnDestroy() override;

	void AttachMessage (DHUDMessageBase *msg, uint32_t id=0, int layer=HUDMSGLayer_Default);
	DHUDMessageBase *DetachMessage (DHUDMessageBase *msg);
	DHUDMessageBase *DetachMessage (uint32_t id);
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
	virtual void Draw (EHudState state, double ticFrac);
	void CallDraw(EHudState state, double ticFrac);
    void DrawBottomStuff (EHudState state);
    void DrawTopStuff (EHudState state);
	void AttachToPlayer(player_t *player);
	DVector2 GetHUDScale() const override;
	virtual void FlashCrosshair ();
	void NewGame ();
	virtual void ScreenSizeChanged ();
	void CallScreenSizeChanged();
	void ShowPop (int popnum);
	virtual bool MustDrawLog(EHudState state);
	virtual void SetMugShotState (const char *state_name, bool wait_till_done=false, bool reset=false);
	void DrawLog();
	uint32_t GetTranslation() const override;

	void CreateAltHUD();
	void DrawAltHUD();

	bool ForceHUDScale(bool on) { std::swap(ForcedScale, on); return on; }	// This is for SBARINFO which should not use BeginStatusBar or BeginHUD.
	int GetTopOfStatusbar() const
	{
		return SBarTop;
	}
	void DoDrawAutomapHUD(int crdefault, int highlight);

//protected:
	void DrawPowerups ();

	
	void RefreshBackground () const;
	void RefreshViewBorder ();

private:
	DObject *AltHud = nullptr;

public:

	AActor *ValidateInvFirst (int numVisible) const;
	void DrawCrosshair ();

	// Sizing info for ths status bar.
	bool Scaled;							// This needs to go away.

	bool Centering;
	bool FixedOrigin;
	double CrosshairSize;
	double Displacement;
	bool ShowLog;
	int artiflashTick = 0;
	double itemflashFade = 0.75;

	player_t *CPlayer;

	FMugShot mugshot;

private:
	bool RepositionCoords (int &x, int &y, int xo, int yo, const int w, const int h) const;
	void DrawMessages (int layer, int bottom);
	void DrawConsistancy () const;
	void DrawWaiting () const;

	TObjPtr<DHUDMessageBase*> Messages[NUM_HUDMSGLAYERS];
};

extern DBaseStatusBar *StatusBar;

// Status bar factories -----------------------------------------------------

DBaseStatusBar *CreateCustomStatusBar(int script=0);

// Crosshair stuff ----------------------------------------------------------

void ST_LoadCrosshair(bool alwaysload=false);
void ST_Clear();
void ST_CreateStatusBar(bool bTitleLevel);

int GetInventoryIcon(AActor *item, uint32_t flags, int *applyscale = nullptr);

class FFont;
void C_MidPrint(FFont* font, const char* message, bool bold = false);



#endif /* __SBAR_H__ */

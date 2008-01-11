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

#include "dobject.h"
#include "m_fixed.h"
#include "v_collection.h"
#include "v_text.h"

struct patch_t;
class player_s;
struct FRemapTable;

extern int SB_state;

enum EHudState
{
	HUD_StatusBar,
	HUD_Fullscreen,
	HUD_None
};

class AWeapon;

class DHUDMessage : public DObject
{
	DECLARE_CLASS (DHUDMessage, DObject)
public:
	DHUDMessage (const char *text, float x, float y, int hudwidth, int hudheight,
		EColorRange textColor, float holdTime);
	virtual ~DHUDMessage ();

	virtual void Serialize (FArchive &arc);

	void Draw (int bottom);
	virtual void ResetText (const char *text);
	virtual void DrawSetup ();
	virtual void DoDraw (int linenum, int x, int y, bool clean, int hudheight);
	virtual bool Tick ();	// Returns true to indicate time for removal
	virtual void ScreenSizeChanged ();

protected:
	FBrokenLines *Lines;
	int Width, Height, NumLines;
	float Left, Top;
	bool CenterX;
	int HoldTics;
	int Tics;
	int State;
	int HUDWidth, HUDHeight;
	EColorRange TextColor;
	FFont *Font;

	DHUDMessage () : SourceText(NULL) {}

private:
	DHUDMessage *Next;
	DWORD SBarID;
	char *SourceText;

	friend class FBaseStatusBar;
};

class DHUDMessageFadeOut : public DHUDMessage
{
	DECLARE_CLASS (DHUDMessageFadeOut, DHUDMessage)
public:
	DHUDMessageFadeOut (const char *text, float x, float y, int hudwidth, int hudheight,
		EColorRange textColor, float holdTime, float fadeOutTime);

	virtual void Serialize (FArchive &arc);
	virtual void DoDraw (int linenum, int x, int y, bool clean, int hudheight);
	virtual bool Tick ();

protected:
	int FadeOutTics;

	DHUDMessageFadeOut() {}
};

class DHUDMessageFadeInOut : public DHUDMessageFadeOut
{
	DECLARE_CLASS (DHUDMessageFadeInOut, DHUDMessageFadeOut)
public:
	DHUDMessageFadeInOut (const char *text, float x, float y, int hudwidth, int hudheight,
		EColorRange textColor, float holdTime, float fadeInTime, float fadeOutTime);

	virtual void Serialize (FArchive &arc);
	virtual void DoDraw (int linenum, int x, int y, bool clean, int hudheight);
	virtual bool Tick ();

protected:
	int FadeInTics;

	DHUDMessageFadeInOut() {}
};

class DHUDMessageTypeOnFadeOut : public DHUDMessageFadeOut
{
	DECLARE_CLASS (DHUDMessageTypeOnFadeOut, DHUDMessageFadeOut)
public:
	DHUDMessageTypeOnFadeOut (const char *text, float x, float y, int hudwidth, int hudheight,
		EColorRange textColor, float typeTime, float holdTime, float fadeOutTime);

	virtual void Serialize (FArchive &arc);
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

class FTexture;
class AAmmo;

class FBaseStatusBar
{
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

	FBaseStatusBar (int reltop);
	virtual ~FBaseStatusBar ();

	void SetScaled (bool scale);

	void AttachMessage (DHUDMessage *msg, uint32 id=0);
	DHUDMessage *DetachMessage (DHUDMessage *msg);
	DHUDMessage *DetachMessage (uint32 id);
	void DetachAllMessages ();
	bool CheckMessage (DHUDMessage *msg);
	void ShowPlayerName ();
	fixed_t GetDisplacement () { return Displacement; }
	int GetPlayer ();

	static void AddBlend (float r, float g, float b, float a, float v_blend[4]);

	virtual void Serialize (FArchive &arc);

	virtual void Tick ();
	virtual void Draw (EHudState state);
			void DrawTopStuff (EHudState state);
	virtual void FlashItem (const PClass *itemtype);
	virtual void AttachToPlayer (player_s *player);
	virtual void FlashCrosshair ();
	virtual void BlendView (float blend[4]);
	virtual void SetFace (void *);		// Takes a FPlayerSkin as input
	virtual void NewGame ();
	virtual void ScreenSizeChanged ();
	virtual void MultiplayerChanged ();
	virtual void SetInteger (int pname, int param);
	virtual void ShowPop (int popnum);
	virtual void ReceivedWeapon (AWeapon *weapon);
	virtual bool MustDrawLog(EHudState state);
	void DrawLog();

protected:
	void DrawPowerups ();

	void UpdateRect (int x, int y, int width, int height) const;
	void DrawImage (FTexture *image, int x, int y, FRemapTable *translation=NULL) const;
	void DrawDimImage (FTexture *image, int x, int y, bool dimmed) const;
	void DrawFadedImage (FTexture *image, int x, int y, fixed_t shade) const;
	void DrawPartialImage (FTexture *image, int wx, int ww) const;

	void DrINumber (signed int val, int x, int y, int imgBase=imgINumbers) const;
	void DrBNumber (signed int val, int x, int y, int w=3) const;
	void DrSmallNumber (int val, int x, int y) const;

	void DrINumberOuter (signed int val, int x, int y, bool center=false, int w=9) const;
	void DrBNumberOuter (signed int val, int x, int y, int w=3) const;
	void DrBNumberOuterFont (signed int val, int x, int y, int w=3) const;
	void DrSmallNumberOuter (int val, int x, int y, bool center) const;

	void RefreshBackground () const;

	void GetCurrentAmmo (AAmmo *&ammo1, AAmmo *&ammo2, int &ammocount1, int &ammocount2) const;

public:
	AInventory *ValidateInvFirst (int numVisible) const;
	void DrawCrosshair ();

	int ST_X, ST_Y;
	int RelTop;
	bool Scaled;
	bool Centering;
	bool FixedOrigin;
	fixed_t CrosshairSize;
	fixed_t Displacement;

	enum
	{
		imgLAME = 0,
		imgNEGATIVE = 1,
		imgINumbers = 2,
		imgBNEGATIVE = 12,
		imgBNumbers = 13,
		imgSmNumbers = 23,
		NUM_BASESB_IMAGES = 33
	};
	FImageCollection Images;

	player_s *CPlayer;

private:
	bool RepositionCoords (int &x, int &y, int xo, int yo, const int w, const int h) const;
	void DrawMessages (int bottom) const;
	void DrawConsistancy () const;

	static BYTE DamageToAlpha[114];

	DHUDMessage *Messages;
	bool ShowLog;
};

extern FBaseStatusBar *StatusBar;

FBaseStatusBar *CreateDoomStatusBar ();
FBaseStatusBar *CreateHereticStatusBar ();
FBaseStatusBar *CreateHexenStatusBar ();
FBaseStatusBar *CreateStrifeStatusBar ();
FBaseStatusBar *CreateCustomStatusBar ();

/*
** sbar.h
** Base status bar definition
**
**---------------------------------------------------------------------------
** Copyright 1998-2001 Randy Heit
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

extern int SB_state;

enum EHudState
{
	HUD_StatusBar,
	HUD_Fullscreen,
	HUD_None
};

class DHUDMessage : public DObject
{
	DECLARE_CLASS (DHUDMessage, DObject)
public:
	DHUDMessage (const char *text, float x, float y, EColorRange textColor,
		float holdTime);
	virtual ~DHUDMessage ();

	virtual void Serialize (FArchive &arc);

	void Draw (int bottom);
	virtual void ResetText (const char *text);
	virtual void DrawSetup ();
	virtual void DoDraw (int linenum, int x, int y, int xscale, int yscale, bool clean);
	virtual bool Tick ();	// Returns true to indicate time for removal
	virtual void ScreenSizeChanged ();

protected:
	brokenlines_t *Lines;
	int Width, Height, NumLines;
	float Left, Top;
	bool CenterX;
	int HoldTics;
	int Tics;
	int State;
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
	DHUDMessageFadeOut (const char *text, float x, float y, EColorRange textColor,
		float holdTime, float fadeOutTime);

	virtual void Serialize (FArchive &arc);
	virtual void DoDraw (int linenum, int x, int y, int xscale, int yscale, bool clean);
	virtual bool Tick ();

protected:
	int FadeOutTics;

	DHUDMessageFadeOut() {}
};

class DHUDMessageTypeOnFadeOut : public DHUDMessageFadeOut
{
	DECLARE_CLASS (DHUDMessageTypeOnFadeOut, DHUDMessageFadeOut)
public:
	DHUDMessageTypeOnFadeOut (const char *text, float x, float y, EColorRange textColor,
		float typeTime, float holdTime, float fadeOutTime);

	virtual void Serialize (FArchive &arc);
	virtual void DoDraw (int linenum, int x, int y, int xscale, int yscale, bool clean);
	virtual bool Tick ();
	virtual void ScreenSizeChanged ();

protected:
	float TypeOnTime;
	int CurrLine;
	int LineVisible;
	int LineLen;

	DHUDMessageTypeOnFadeOut() {}
};

class FBaseStatusBar
{
public:
	FBaseStatusBar (int reltop);
	virtual ~FBaseStatusBar ();

	void SetScaled (bool scale);

	void AttachMessage (DHUDMessage *msg, DWORD id=0);
	DHUDMessage *DetachMessage (DHUDMessage *msg);
	DHUDMessage *DetachMessage (DWORD id);
	void DetachAllMessages ();
	bool CheckMessage (DHUDMessage *msg);
	void ShowPlayerName ();
	fixed_t GetDisplacement () { return Displacement; }

	virtual void Serialize (FArchive &arc);

	virtual void Tick ();
	virtual void Draw (EHudState state);
	virtual void FlashArtifact (int arti);
	virtual void AttachToPlayer (player_s *player);
	virtual void FlashCrosshair ();
	virtual void BlendView (float blend[4]);
	virtual void SetFace (void *);		// Takes a FPlayerSkin as input
	virtual void NewGame ();
	virtual void ScreenSizeChanged ();
	virtual void MultiplayerChanged ();

protected:
	void UpdateRect (int x, int y, int width, int height) const;
	void DrawImage (const FImageCollection &collection, int image, int x, int y, byte *translation=NULL) const;
	void DrawImageNoUpdate (const FImageCollection &collection, int image, int x, int y, byte *translation=NULL) const;
	void DrawPartialImage (const FImageCollection &collection, int image, int x, int y, int wx, int wy, int ww, int wh) const;

	void SetHorizCentering (bool which) { Centering = which; }
	void OverrideImageOrigin (bool which) { FixedOrigin = which; }

	void DrawFadedImage (const FImageCollection &collection, int image, int x, int y, fixed_t shade) const;
	void DrawShadowedImage (const FImageCollection &collection, int image, int x, int y, fixed_t shade) const;
	void DrawOuterImage (const FImageCollection &collection, int image, int x, int y) const;
	void DrawOuterPatch (const patch_t *patch, int x, int y) const;

	void DrINumber (signed int val, int x, int y) const;
	void DrBNumber (signed int val, int x, int y, int w=3) const;
	void DrSmallNumber (int val, int x, int y) const;

	void DrINumberOuter (signed int val, int x, int y) const;
	void DrBNumberOuter (signed int val, int x, int y, int w=3) const;
	void DrSmallNumberOuter (int val, int x, int y) const;

	void ShadeChain (int left, int right, int top, int height) const;
	void DrawCrosshair ();

	void CopyToScreen (int x, int y, int w, int h) const;
	void RefreshBackground () const;

	static void AddBlend (float r, float g, float b, float a, float v_blend[4]);


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
	FImageCollection AmmoImages;
	FImageCollection ArtiImages;
	FImageCollection ArmorImages;
	DCanvas *ScaleCopy;

	player_s *CPlayer;

private:
	bool RepositionCoords (int &x, int &y, int xo, int yo, const int w, const int h) const;
	void DrawMessages (int bottom) const;
	void DrawConsistancy () const;

	fixed_t ScaleX, ScaleY;
	fixed_t ScaleIX, ScaleIY;

	static byte DamageToAlpha[114];

	DHUDMessage *Messages;
};

extern FBaseStatusBar *StatusBar;

FBaseStatusBar *CreateDoomStatusBar ();
FBaseStatusBar *CreateHereticStatusBar ();
FBaseStatusBar *CreateHexenStatusBar ();

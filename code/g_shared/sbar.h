#include "dobject.h"
#include "m_fixed.h"
#include "v_collection.h"
#include "v_text.h"

struct patch_s;
class player_s;

extern int SB_state;

enum EHudState
{
	HUD_StatusBar,
	HUD_Fullscreen,
	HUD_None
};

class FHUDMessage
{
public:
	FHUDMessage (const char *text, float x, float y, EColorRange textColor,
		float holdTime);
	virtual ~FHUDMessage ();

	void Draw (int bottom);
	virtual void ResetText (const char *text);
	virtual void DrawSetup ();
	virtual void DoDraw (int linenum, int x, int y, int xscale, int yscale, bool clean);
	virtual bool Tick ();	// Returns true to indicate time for removal

protected:
	brokenlines_t *Lines;
	int Width, Height, NumLines;
	float Left, Top;
	bool CenterX;
	int HoldTics;
	int Tics;
	int State;
	EColorRange TextColor;

private:
	FHUDMessage *Next;
	DWORD SBarID;

	friend class FBaseStatusBar;
};

class FHUDMessageFadeOut : public FHUDMessage
{
public:
	FHUDMessageFadeOut (const char *text, float x, float y, EColorRange textColor,
		float holdTime, float fadeOutTime);

	virtual void DoDraw (int linenum, int x, int y, int xscale, int yscale, bool clean);
	virtual bool Tick ();

protected:
	int FadeOutTics;
};

class FHUDMessageTypeOnFadeOut : public FHUDMessageFadeOut
{
public:
	FHUDMessageTypeOnFadeOut (const char *text, float x, float y, EColorRange textColor,
		float typeTime, float holdTime, float fadeOutTime);

	virtual void DoDraw (int linenum, int x, int y, int xscale, int yscale, bool clean);
	virtual bool Tick ();

protected:
	float TypeOnTime;
	int CurrLine;
	int LineVisible;
	int LineLen;
};

class FBaseStatusBar
{
public:
	FBaseStatusBar (int reltop);
	virtual ~FBaseStatusBar ();

	void SetScaled (bool scale);

	void AttachMessage (FHUDMessage *msg, DWORD id=0);
	FHUDMessage *DetachMessage (FHUDMessage *msg);
	FHUDMessage *DetachMessage (DWORD id);
	void DetachAllMessages ();
	bool CheckMessage (FHUDMessage *msg);
	void ShowPlayerName ();
	fixed_t GetDisplacement () { return Displacement; }

	virtual void Tick ();
	virtual void Draw (EHudState state);
	virtual void FlashArtifact (int arti);
	virtual void AttachToPlayer (player_s *player);
	virtual void FlashCrosshair ();
	virtual void BlendView (float blend[4]);
	virtual void SetFace (void *);		// Takes a FPlayerSkin as input

protected:
	void DrawImage (const FImageCollection &collection, int image, int x, int y, byte *translation=NULL) const;
	void DrawPartialImage (const FImageCollection &collection, int image, int x, int y, int wx, int wy, int ww, int wh) const;

	void SetHorizCentering (bool which) { Centering = which; }
	void OverrideImageOrigin (bool which) { FixedOrigin = which; }

	void DrawFadedImage (const FImageCollection &collection, int image, int x, int y, fixed_t shade) const;
	void DrawShadowedImage (const FImageCollection &collection, int image, int x, int y, fixed_t shade) const;
	void DrawOuterImage (const FImageCollection &collection, int image, int x, int y) const;
	void DrawOuterPatch (const patch_s *patch, int x, int y) const;

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

	FHUDMessage *Messages;
};

extern FBaseStatusBar *StatusBar;

FBaseStatusBar *CreateDoomStatusBar ();
FBaseStatusBar *CreateHereticStatusBar ();
FBaseStatusBar *CreateHexenStatusBar ();
#ifndef __INTERMISSION_H
#define __INTERMISSION_H

#include "doomdef.h"
#include "dobject.h"
#include "m_fixed.h"
#include "textures/textures.h"
#include "s_sound.h"
#include "v_font.h"
#include "g_game.h"

struct event_t;

#define DECLARE_SUPER_CLASS(cls,parent) \
private: \
	typedef parent Super; \
	typedef cls ThisClass;

struct FIntermissionPatch
{
	FName mCondition;
	FString mName;
	double x, y;
};

struct FIIntermissionPatch
{
	FName mCondition;
	FTextureID mPic;
	double x, y;
};

struct FCastSound
{
	uint8_t mSequence;
	uint8_t mIndex;
	FString mSound;
};

struct FICastSound
{
	uint8_t mSequence;
	uint8_t mIndex;
	FSoundID mSound;
};

enum EFadeType
{
	FADE_In,
	FADE_Out,
};

enum EScrollDir
{
	SCROLL_Left,
	SCROLL_Right,
	SCROLL_Up,
	SCROLL_Down,
};

// actions that don't create objects
#define WIPER_ID ((PClass*)intptr_t(-1))
#define TITLE_ID ((PClass*)intptr_t(-2))

//==========================================================================

struct FIntermissionAction
{
	int mSize;
	PClass *mClass;
	FString mMusic;
	int mMusicOrder;
	int mCdTrack;
	int mCdId;
	int mDuration;
	FString mBackground;
	FString mSound;
	bool mFlatfill;
	bool mMusicLooping;
	TArray<FIntermissionPatch> mOverlays;

	FIntermissionAction();
	virtual ~FIntermissionAction() {}
	virtual bool ParseKey(FScanner &sc);
};

struct FIntermissionActionFader : public FIntermissionAction
{
	typedef FIntermissionAction Super;

	EFadeType mFadeType;

	FIntermissionActionFader();
	virtual bool ParseKey(FScanner &sc);
};

struct FIntermissionActionWiper : public FIntermissionAction
{
	typedef FIntermissionAction Super;

	gamestate_t mWipeType;

	FIntermissionActionWiper();
	virtual bool ParseKey(FScanner &sc);
};

struct FIntermissionActionTextscreen : public FIntermissionAction
{
	typedef FIntermissionAction Super;

	FString mText;
	int mTextDelay;
	int mTextSpeed;
	int mTextX, mTextY;
	EColorRange mTextColor;

	FIntermissionActionTextscreen();
	virtual bool ParseKey(FScanner &sc);
};

struct FIntermissionActionCast : public FIntermissionAction
{
	typedef FIntermissionAction Super;

	FString mName;
	FName mCastClass = NAME_None;
	TArray<FCastSound> mCastSounds;

	FIntermissionActionCast();
	virtual bool ParseKey(FScanner &sc);
};

struct FIntermissionActionScroller : public FIntermissionAction
{
	typedef FIntermissionAction Super;

	FString mSecondPic;
	int mScrollDelay;
	int mScrollTime;
	int mScrollDir;

	FIntermissionActionScroller();
	virtual bool ParseKey(FScanner &sc);
};

struct FIntermissionDescriptor
{
	FName mLink = NAME_None;
	TDeletingArray<FIntermissionAction *> mActions;
};

typedef TMap<FName, FIntermissionDescriptor*> FIntermissionDescriptorList;

extern FIntermissionDescriptorList IntermissionDescriptors;

//==========================================================================

class DIntermissionScreen : public DObject
{
	DECLARE_CLASS (DIntermissionScreen, DObject)

protected:
	int mDuration;
	FTextureID mBackground;
	bool mFlatfill;
	TArray<FIIntermissionPatch> mOverlays;

	bool CheckOverlay(int i);

public:
	int mTicker;

	DIntermissionScreen() {}
	virtual void Init(FIntermissionAction *desc, bool first);
	virtual int Responder (event_t *ev);
	virtual int Ticker ();
	virtual void Drawer ();
	void OnDestroy() override;
	FTextureID GetBackground(bool *fill)
	{
		*fill = mFlatfill;
		return mBackground;
	}
	void SetBackground(FTextureID tex, bool fill)
	{
		mBackground = tex;
		mFlatfill = fill;
	}
};

class DIntermissionScreenFader : public DIntermissionScreen
{
	DECLARE_CLASS (DIntermissionScreenFader, DIntermissionScreen)

	EFadeType mType;

public:

	DIntermissionScreenFader() {}
	virtual void Init(FIntermissionAction *desc, bool first);
	virtual int Responder (event_t *ev);
	virtual int Ticker ();
	virtual void Drawer ();
};

class DIntermissionScreenText : public DIntermissionScreen
{
	DECLARE_CLASS (DIntermissionScreenText, DIntermissionScreen)

	FString mText;
	int mTextSpeed;
	int mTextX, mTextY;
	int mTextCounter;
	int mTextDelay;
	int mTextLen;
	EColorRange mTextColor;

public:

	DIntermissionScreenText() {}
	virtual void Init(FIntermissionAction *desc, bool first);
	virtual int Responder (event_t *ev);
	virtual void Drawer ();
};

class DIntermissionScreenCast : public DIntermissionScreen
{
	DECLARE_CLASS (DIntermissionScreenCast, DIntermissionScreen)

	const char *mName;
	PClassActor *mClass;
	AActor *mDefaults;
	TArray<FICastSound> mCastSounds;

	int 			casttics;
	uint32_t		casttranslation;	// [RH] Draw "our hero" with their chosen suit color
	FState*			caststate;
	FState*			basestate;
	FState*			advplayerstate;
	bool	 		castdeath;
	bool	 		castattacking;
	int 			castframes;
	int 			castonmelee;

	void PlayAttackSound();

public:

	DIntermissionScreenCast() {}
	virtual void Init(FIntermissionAction *desc, bool first);
	virtual int Responder (event_t *ev);
	virtual int Ticker ();
	virtual void Drawer ();
};

class DIntermissionScreenScroller : public DIntermissionScreen
{
	DECLARE_CLASS (DIntermissionScreenScroller, DIntermissionScreen)

	FTextureID mFirstPic;
	FTextureID mSecondPic;
	int mScrollDelay;
	int mScrollTime;
	int mScrollDir;

public:

	DIntermissionScreenScroller() {}
	virtual void Init(FIntermissionAction *desc, bool first);
	virtual int Responder (event_t *ev);
	virtual void Drawer ();
};

enum
{
	FSTATE_EndingGame = 0,
	FSTATE_ChangingLevel = 1,
	FSTATE_InLevel = 2
};

class DIntermissionController : public DObject
{
	DECLARE_CLASS (DIntermissionController, DObject)
	HAS_OBJECT_POINTERS

	FIntermissionDescriptor *mDesc;
	TObjPtr<DIntermissionScreen*> mScreen;
	bool mDeleteDesc;
	bool mFirst;
	bool mAdvance, mSentAdvance;
	uint8_t mGameState;
	int mIndex;

	bool NextPage();

public:
	static DIntermissionController *CurrentIntermission;

	DIntermissionController(FIntermissionDescriptor *mDesc = NULL, bool mDeleteDesc = false, uint8_t state = FSTATE_ChangingLevel);
	bool Responder (event_t *ev);
	void Ticker ();
	void Drawer ();
	void OnDestroy() override;

	friend void F_AdvanceIntermission();
	friend void F_StartIntermission(FIntermissionDescriptor *, bool, uint8_t);
};


// Interface for main loop
bool F_Responder (event_t* ev);
void F_Ticker ();
void F_Drawer ();
void F_StartIntermission(FIntermissionDescriptor *desc, bool deleteme, uint8_t state);
void F_StartIntermission(FName desc, uint8_t state);
void F_EndFinale ();
void F_AdvanceIntermission();

// Create an intermission from old cluster data
void F_StartFinale (const char *music, int musicorder, int cdtrack, unsigned int cdid, const char *flat, 
					const char *text, INTBOOL textInLump, INTBOOL finalePic, INTBOOL lookupText, 
					bool ending, FName endsequence = NAME_None);



#endif

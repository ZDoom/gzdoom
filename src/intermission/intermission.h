#ifndef __INTERMISSION_H
#define __INTERMISSION_H

#include "dobject.h"
#include "m_fixed.h"
#include "textures/textures.h"
#include "s_sound.h"

struct event_t;

#define DECLARE_SUPER_CLASS(cls,parent) \
private: \
	typedef parent Super; \
	typedef cls ThisClass;

struct FIntermissionPatch
{
	FString mName;
	double x, y;
};

struct FIIntermissionPatch
{
	FTextureID mPic;
	double x, y;
};

struct FCastSound
{
	BYTE mSequence;
	BYTE mIndex;
	FString mSound;
};

struct FICastSound
{
	BYTE mSequence;
	BYTE mIndex;
	FSoundID mSound;
};

//==========================================================================

struct FIntermissionAction
{
	int mSize;
	const PClass *mClass;
	FString mMusic;
	int mMusicOrder;
	int mCdTrack;
	int mCdId;
	int mDuration;
	FString mBackground;
	FString mSound;
	bool mFlatfill;
	TArray<FIntermissionPatch> mOverlays;
	FName mLink;

	FIntermissionAction();
	virtual bool ParseKey(FScanner &sc);
};

struct FIntermissionActionFader : public FIntermissionAction
{
	typedef FIntermissionAction Super;

	int FadeTime;
	int FadeType;

	virtual bool ParseKey(FScanner &sc);
};

struct FIntermissionActionTextscreen : public FIntermissionAction
{
	typedef FIntermissionAction Super;

	FString mText;
	int mTextSpeed;
	int mTextX, mTextY;

	virtual bool ParseKey(FScanner &sc);
};

struct FIntermissionActionCast : public FIntermissionAction
{
	typedef FIntermissionAction Super;

	FString mWalking;
	FString mAttacking1;
	FString mAttacking2;
	FString mDying;
	TArray<FCastSound> mCastSounds;

	virtual bool ParseKey(FScanner &sc);
};

struct FIntermissionActionScroller : public FIntermissionAction
{
	typedef FIntermissionAction Super;

	FString mSecondPic;
	int mScrollDelay;
	int mScrollTime;
	int mScrollDir;

	virtual bool ParseKey(FScanner &sc);
};

struct FIntermissionDescriptor
{
	TArray<FIntermissionAction *> mActions;
};

typedef TMap<FName, FIntermissionDescriptor*> FIntermissionDescriptorList;

extern FIntermissionDescriptorList IntermissionDescriptors;

//==========================================================================

class DIntermissionScreen : public DObject
{
	DECLARE_CLASS (DIntermissionScreen, DObject)

	FString mMusic;
	int mMusicOrder;
	int mCdTrack;
	int mCdId;
	int mDuration;
	FTextureID mBackground;
	bool mFlatfill;
	TArray<FIIntermissionPatch> mOverlays;

public:

	DIntermissionScreen() {}
	virtual void Init(FIntermissionDescriptor *desc);
	virtual bool Responder (event_t *ev);
	virtual void Ticker ();
	virtual void Drawer ();
};

class DIntermissionScreenFader : public DIntermissionScreen
{
	DECLARE_CLASS (DIntermissionScreenFader, DIntermissionScreen)

	int mTotalTime;
	int mCounter;
	int mType;

public:

	DIntermissionScreenFader() {}
	virtual void Init(FIntermissionDescriptor *desc);
	virtual bool Responder (event_t *ev);
	virtual void Ticker ();
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

public:

	DIntermissionScreenText() {}
	virtual void Init(FIntermissionDescriptor *desc);
	virtual bool Responder (event_t *ev);
	virtual void Ticker ();
	virtual void Drawer ();
};

class DIntermissionScreenCast : public DIntermissionScreen
{
	DECLARE_CLASS (DIntermissionScreenCast, DIntermissionScreen)

	FState *mWalking;
	FState *mAttacking1;
	FState *mAttacking2;
	FState *mDying;
	TArray<FICastSound> mCastSounds;

public:

	DIntermissionScreenCast() {}
	virtual void Init(FIntermissionDescriptor *desc);
	virtual bool Responder (event_t *ev);
	virtual void Ticker ();
	virtual void Drawer ();
};

class DIntermissionScreenScroller : public DIntermissionScreen
{
	DECLARE_CLASS (DIntermissionScreenScroller, DIntermissionScreen)

	FTextureID mSecondPic;
	int mScrollDelay;
	int mScrollTime;
	int mScrollDir;

public:

	DIntermissionScreenScroller() {}
	virtual void Init(FIntermissionDescriptor *desc);
	virtual bool Responder (event_t *ev);
	virtual void Ticker ();
	virtual void Drawer ();
};


class DIntermissionController : public DObject
{
	DECLARE_CLASS (DIntermissionController, DObject)


public:
	static DIntermissionController *CurrentIntermission;

	DIntermissionController() {}
	bool Responder (event_t *ev);
	void Ticker ();
	void Drawer ();
	void Close();
};


// Interface for main loop
bool F_Responder (event_t* ev);
void F_Ticker ();
void F_Drawer ();

// Create an intermission from old cluster data
void F_StartFinale (const char *music, int musicorder, int cdtrack, unsigned int cdid, const char *flat, 
					const char *text, INTBOOL textInLump, INTBOOL finalePic, INTBOOL lookupText, 
					bool ending, int endsequence = 0);



#endif

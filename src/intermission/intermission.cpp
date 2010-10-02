/*
** intermission.cpp
** Framework for intermissions (text screens, slideshows, etc)
**
**---------------------------------------------------------------------------
** Copyright 2010 Christoph Oelckers
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

#include "doomtype.h"
#include "d_event.h"
#include "w_wad.h"
#include "gi.h"
#include "v_video.h"
#include "v_palette.h"
#include "intermission/intermission.h"

FIntermissionDescriptorList IntermissionDescriptors;

IMPLEMENT_CLASS(DIntermissionScreen)
IMPLEMENT_CLASS(DIntermissionScreenFader)
IMPLEMENT_CLASS(DIntermissionScreenText)
IMPLEMENT_CLASS(DIntermissionScreenCast)
IMPLEMENT_CLASS(DIntermissionScreenScroller)
IMPLEMENT_CLASS(DIntermissionController)

//==========================================================================
//
//
//
//==========================================================================

void DIntermissionScreen::Init(FIntermissionAction *desc, bool first)
{
	int lumpnum;

	if (desc->mCdTrack == 0 || !S_ChangeCDMusic (desc->mCdTrack, desc->mCdId))
	{
		if (desc->mMusic.IsEmpty())
		{
			// only start the default music if this is the first action in an intermission
			if (first) S_ChangeMusic (gameinfo.finaleMusic, 0, desc->mMusicLooping);
		}
		else
		{
			S_ChangeMusic (desc->mMusic, desc->mMusicOrder, desc->mMusicLooping);
		}
	}
	mDuration = desc->mDuration;
	mBackground = TexMan.CheckForTexture(desc->mBackground, FTexture::TEX_MiscPatch);
	mFlatfill = desc->mFlatfill;
	S_Sound (CHAN_VOICE | CHAN_UI, desc->mSound, 1.0f, ATTN_NONE);
	if (desc->mPalette.IsNotEmpty() && (lumpnum = Wads.CheckNumForFullName(desc->mPalette, true)) > 0)
	{
		PalEntry *palette;
		const BYTE *orgpal;
		FMemLump lump;
		int i;

		lump = Wads.ReadLump (lumpnum);
		orgpal = (BYTE *)lump.GetMem();
		palette = screen->GetPalette ();
		for (i = 256; i > 0; i--, orgpal += 3)
		{
			*palette++ = PalEntry (orgpal[0], orgpal[1], orgpal[2]);
		}
		screen->UpdatePalette ();
		mPaletteChanged = true;
	}
	mOverlays.Resize(desc->mOverlays.Size());
	for (unsigned i=0; i < mOverlays.Size(); i++)
	{
		mOverlays[i].x = desc->mOverlays[i].x;
		mOverlays[i].y = desc->mOverlays[i].y;
		mOverlays[i].mPic = TexMan.CheckForTexture(desc->mOverlays[i].mName, FTexture::TEX_MiscPatch);
	}
}


bool DIntermissionScreen::Responder (event_t *ev)
{
	return false;
}

void DIntermissionScreen::Ticker ()
{
}

void DIntermissionScreen::Drawer ()
{
}

void DIntermissionScreen::Destroy()
{
	if (mPaletteChanged)
	{
		PalEntry *palette;
		int i;

		palette = screen->GetPalette ();
		for (i = 0; i < 256; ++i)
		{
			palette[i] = GPalette.BaseColors[i];
		}
		screen->UpdatePalette ();
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DIntermissionScreenFader::Init(FIntermissionAction *desc, bool first)
{
	Super::Init(desc, first);
	mType = static_cast<FIntermissionActionFader*>(desc)->mFadeType;
}

bool DIntermissionScreenFader::Responder (event_t *ev)
{
	return false;
}

void DIntermissionScreenFader::Ticker ()
{
}

void DIntermissionScreenFader::Drawer ()
{
}

//==========================================================================
//
//
//
//==========================================================================

void DIntermissionScreenText::Init(FIntermissionAction *desc, bool first)
{
	Super::Init(desc, first);
	mText = static_cast<FIntermissionActionTextscreen*>(desc)->mText;
	mTextSpeed = static_cast<FIntermissionActionTextscreen*>(desc)->mTextSpeed;
	mTextX = static_cast<FIntermissionActionTextscreen*>(desc)->mTextX;
	mTextY = static_cast<FIntermissionActionTextscreen*>(desc)->mTextY;
}

bool DIntermissionScreenText::Responder (event_t *ev)
{
	return false;
}

void DIntermissionScreenText::Ticker ()
{
}

void DIntermissionScreenText::Drawer ()
{
}

//==========================================================================
//
//
//
//==========================================================================

void DIntermissionScreenCast::Init(FIntermissionAction *desc, bool first)
{
	Super::Init(desc, first);
	mName = static_cast<FIntermissionActionCast*>(desc)->mName;
	mClass = PClass::FindClass(static_cast<FIntermissionActionCast*>(desc)->mCastClass);

	mCastSounds.Resize(static_cast<FIntermissionActionCast*>(desc)->mCastSounds.Size());
	for (unsigned i=0; i < mCastSounds.Size(); i++)
	{
		mCastSounds[i].mSequence = static_cast<FIntermissionActionCast*>(desc)->mCastSounds[i].mSequence;
		mCastSounds[i].mIndex = static_cast<FIntermissionActionCast*>(desc)->mCastSounds[i].mIndex;
		mCastSounds[i].mSound = static_cast<FIntermissionActionCast*>(desc)->mCastSounds[i].mSound;
	}
}

bool DIntermissionScreenCast::Responder (event_t *ev)
{
	return false;
}

void DIntermissionScreenCast::Ticker ()
{
}

void DIntermissionScreenCast::Drawer ()
{
}

//==========================================================================
//
//
//
//==========================================================================

void DIntermissionScreenScroller::Init(FIntermissionAction *desc, bool first)
{
	Super::Init(desc, first);
	mSecondPic = TexMan.CheckForTexture(static_cast<FIntermissionActionScroller*>(desc)->mSecondPic, FTexture::TEX_MiscPatch);
	mScrollDelay = static_cast<FIntermissionActionScroller*>(desc)->mScrollDelay;
	mScrollTime = static_cast<FIntermissionActionScroller*>(desc)->mScrollTime;
	mScrollDir = static_cast<FIntermissionActionScroller*>(desc)->mScrollDir;
}

bool DIntermissionScreenScroller::Responder (event_t *ev)
{
	return false;
}

void DIntermissionScreenScroller::Ticker ()
{
}

void DIntermissionScreenScroller::Drawer ()
{
}




static FIntermissionDescriptor DefaultIntermission;


// Called by main loop.
bool F_Responder (event_t* ev) { return true; }

// Called by main loop.
void F_Ticker () {}

// Called by main loop.
void F_Drawer () {}




#if 0

void F_StartFinale (const char *music, int musicorder, int cdtrack, unsigned int cdid, const char *flat, 
					const char *text, INTBOOL textInLump, INTBOOL finalePic, INTBOOL lookupText, 
					bool ending, int endsequence)
{
	bool loopmusic = ending ? !gameinfo.noloopfinalemusic : true;
	gameaction = ga_nothing;
	gamestate = GS_FINALE;
	viewactive = false;
	automapactive = false;

	// Okay - IWAD dependend stuff.
	// This has been changed severely, and some stuff might have changed in the process.
	//
	// [RH] More flexible now (even more severe changes)
	//  FinaleFlat, FinaleText, and music are now determined in G_WorldDone() based on
	//	data in a level_info_t and a cluster_info_t.




	FinaleStage = 0;
	V_SetBlend (0,0,0,0);
	TEXTSPEED = 2;

	FinaleHasPic = !!finalePic;
	FinaleCount = 0;
	FinaleEndCount = 70;
	FadeDir = -1;
	FinaleEnding = ending;
	FinaleSequence = endsequence;

	S_StopAllChannels ();

	if (ending)
	{
		if (EndSequences[FinaleSequence].EndType == END_Chess)
		{
			TEXTSPEED = 3;	// Slow the text to its original rate to match the music.
			S_ChangeMusic ("hall", 0, loopmusic);
			FinaleStage = 10;
			GetFinaleText ("win1msg");
			V_SetBlend (0,0,0,256);
		}
		else if (EndSequences[FinaleSequence].EndType == END_Strife)
		{
			if (players[0].mo->FindInventory (QuestItemClasses[24]) ||
				players[0].mo->FindInventory (QuestItemClasses[27]))
			{
				FinalePart = 10;
			}
			else
			{
				FinalePart = 17;
			}
			FinaleStage = 5;
			FinaleEndCount = 0;
		}
	}
}
#endif

void F_StartIntermission(FIntermissionDescriptor *desc, bool deleteme)
{
}


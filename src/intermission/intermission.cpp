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
#include "doomstat.h"
#include "d_event.h"
#include "w_wad.h"
#include "gi.h"
#include "v_video.h"
#include "v_palette.h"
#include "d_main.h"
#include "gstrings.h"
#include "intermission/intermission.h"

FIntermissionDescriptorList IntermissionDescriptors;

IMPLEMENT_CLASS(DIntermissionScreen)
IMPLEMENT_CLASS(DIntermissionScreenFader)
IMPLEMENT_CLASS(DIntermissionScreenText)
IMPLEMENT_CLASS(DIntermissionScreenCast)
IMPLEMENT_CLASS(DIntermissionScreenScroller)
IMPLEMENT_POINTY_CLASS(DIntermissionController)
	DECLARE_POINTER(mScreen)
END_POINTERS

extern int		NoWipe;

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

	const char *texname = desc->mBackground;
	if (*texname == '@')
	{
		char *pp;
		unsigned int v = strtoul(texname+1, &pp, 10) - 1;
		if (*pp == 0 && v < gameinfo.finalePages.Size())
		{
			texname = gameinfo.finalePages[v].GetChars();
		}
	}
	else if (*texname == '$')
	{
		texname = GStrings[texname+1];
	}
	FTextureID tex = TexMan.CheckForTexture(texname, FTexture::TEX_MiscPatch);
	if (tex.isValid())
	{
		mBackground = tex;
		mFlatfill = desc->mFlatfill;
	}
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
		NoWipe = 1;
	}
	mOverlays.Resize(desc->mOverlays.Size());
	for (unsigned i=0; i < mOverlays.Size(); i++)
	{
		mOverlays[i].x = desc->mOverlays[i].x;
		mOverlays[i].y = desc->mOverlays[i].y;
		mOverlays[i].mPic = TexMan.CheckForTexture(desc->mOverlays[i].mName, FTexture::TEX_MiscPatch);
	}
	mTicker = 0;
}


int DIntermissionScreen::Responder (event_t *ev)
{
	if (ev->type == EV_KeyDown)
	{
		return -1;
	}
	return 0;
}

int DIntermissionScreen::Ticker ()
{
	if (++mTicker >= mDuration && mDuration > 0) return -1;
	return 0;
}

void DIntermissionScreen::Drawer ()
{
	if (mBackground.isValid())
	{
		if (!mFlatfill)
		{
			screen->DrawTexture (TexMan[mBackground], 0, 0, DTA_Fullscreen, true, TAG_DONE);
			screen->FillBorder (NULL);
		}
		else
		{
			screen->FlatFill (0,0, SCREENWIDTH, SCREENHEIGHT, TexMan[mBackground]);
		}
	}
	else
	{
		screen->Clear (0, 0, SCREENWIDTH, SCREENHEIGHT, 0, 0);
	}
	for (unsigned i=0; i < mOverlays.Size(); i++)
	{
		screen->DrawTexture (TexMan[mOverlays[i].mPic], mOverlays[i].x, mOverlays[i].y, DTA_320x200, true, TAG_DONE);
	}
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
		NoWipe = 0;
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

int DIntermissionScreenFader::Responder (event_t *ev)
{
	return Super::Responder(ev);
}

int DIntermissionScreenFader::Ticker ()
{
	return Super::Ticker();
}

void DIntermissionScreenFader::Drawer ()
{
	Super::Drawer();
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
	if (mText[0] == '$') mText = GStrings[&mText[1]];
	mTextSpeed = static_cast<FIntermissionActionTextscreen*>(desc)->mTextSpeed;
	mTextX = static_cast<FIntermissionActionTextscreen*>(desc)->mTextX;
	if (mTextX < 0) mTextX =gameinfo.TextScreenX;
	mTextY = static_cast<FIntermissionActionTextscreen*>(desc)->mTextY;
	if (mTextY < 0) mTextY =gameinfo.TextScreenY;
	mTextLen = (int)strlen(mText);
	mTextDelay = static_cast<FIntermissionActionTextscreen*>(desc)->mTextDelay;
	mTextColor = static_cast<FIntermissionActionTextscreen*>(desc)->mTextColor;
}

int DIntermissionScreenText::Responder (event_t *ev)
{
	if (ev->type == EV_KeyDown)
	{
		if (mTicker < (mTextDelay + mTextLen) * mTextSpeed)
		{
			mTicker = (mTextDelay + mTextLen) * mTextSpeed;
			return 1;
		}
	}
	return Super::Responder(ev);
}

int DIntermissionScreenText::Ticker ()
{
	return Super::Ticker();
}

void DIntermissionScreenText::Drawer ()
{
	Super::Drawer();
	if (mTicker >= mTextDelay)
	{
		FTexture *pic;
		int w;
		size_t count;
		int c;
		const FRemapTable *range;
		
		// draw some of the text onto the screen
		int rowheight = SmallFont->GetHeight () + (gameinfo.gametype & (GAME_DoomStrifeChex) ? 3 : -1);
		bool scale = (CleanXfac != 1 || CleanYfac != 1);

		int cx = mTextX;
		int cy = mTextY;
		const char *ch = mText;
			
		count = (mTicker - mTextDelay) / mTextSpeed;
		range = SmallFont->GetColorTranslation (mTextColor);

		for ( ; count > 0 ; count-- )
		{
			c = *ch++;
			if (!c)
				break;
			if (c == '\n')
			{
				cx = mTextX;
				cy += rowheight;
				continue;
			}

			pic = SmallFont->GetChar (c, &w);
			if (cx+w > SCREENWIDTH)
				continue;
			if (pic != NULL)
			{
				if (scale)
				{
					screen->DrawTexture (pic,
						cx,// + 320 / 2,
						cy,// + 200 / 2,
						DTA_Translation, range,
						DTA_Clean, true,
						TAG_DONE);
				}
				else
				{
					screen->DrawTexture (pic,
						cx,// + 320 / 2,
						cy,// + 200 / 2,
						DTA_Translation, range,
						TAG_DONE);
				}
			}
			cx += w;
		}
	}
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

int DIntermissionScreenCast::Responder (event_t *ev)
{
	return Super::Responder(ev);
}

int DIntermissionScreenCast::Ticker ()
{
	return Super::Ticker();
}

void DIntermissionScreenCast::Drawer ()
{
	Super::Drawer();
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

int DIntermissionScreenScroller::Responder (event_t *ev)
{
	return Super::Responder(ev);
}

int DIntermissionScreenScroller::Ticker ()
{
	return Super::Ticker();
}

void DIntermissionScreenScroller::Drawer ()
{
	Super::Drawer();
}


//==========================================================================
//
//
//
//==========================================================================

DIntermissionController *DIntermissionController::CurrentIntermission;

DIntermissionController::DIntermissionController(FIntermissionDescriptor *Desc, bool DeleteDesc, bool endinggame)
{
	mDesc = Desc;
	mDeleteDesc = DeleteDesc;
	mIndex = 0;
	mAdvance = false;
	mScreen = NULL;
	mFirst = true;
	mEndingGame = endinggame;
	NextPage();
}

bool DIntermissionController::NextPage ()
{
	FTextureID bg;
	bool fill = false;

	if (mIndex == (int)mDesc->mActions.Size()-1 && mDesc->mLink == NAME_None)
	{
		// last page
		return false;
	}

	if (mScreen != NULL) 
	{
		bg = mScreen->GetBackground(&fill);
		mScreen->Destroy();
	}
again:
	while ((unsigned)mIndex < mDesc->mActions.Size())
	{
		FIntermissionAction *action = mDesc->mActions[mIndex++];
		if (action->mClass == WIPER_ID)
		{
			wipegamestate = static_cast<FIntermissionActionWiper*>(action)->mWipeType;
		}
		else if (action->mClass == TITLE_ID)
		{
			Destroy();
			D_StartTitle ();
			return false;
		}
		else
		{
			// create page here
			mScreen = (DIntermissionScreen*)action->mClass->CreateNew();
			mScreen->SetBackground(bg, fill);	// copy last screen's background before initializing
			mScreen->Init(action, mFirst);
			mFirst = false;
			return true;
		}
	}
	if (mDesc->mLink != NAME_None)
	{
		FIntermissionDescriptor **pDesc = IntermissionDescriptors.CheckKey(mDesc->mLink);
		if (pDesc != NULL)
		{
			if (mDeleteDesc) delete mDesc;
			mDeleteDesc = false;
			mIndex = 0;
			mDesc = *pDesc;
			goto again;
		}
	}
	return false;
}

bool DIntermissionController::Responder (event_t *ev)
{
	if (mScreen != NULL)
	{
		int res = mScreen->Responder(ev);
		mAdvance = (res == -1);
		return !!res;
	}
	return false;
}

void DIntermissionController::Ticker ()
{
	if (mScreen != NULL)
	{
		mAdvance |= (mScreen->Ticker() == -1);
	}
	if (mAdvance)
	{
		mAdvance = false;
		if (!NextPage())
		{
			if (!mEndingGame)
			{
				gameaction = ga_worlddone;
				Destroy();
			}
		}
	}
}

void DIntermissionController::Drawer ()
{
	if (mScreen != NULL)
	{
		mScreen->Drawer();
	}
}

void DIntermissionController::Destroy ()
{
	Super::Destroy();
	if (mDeleteDesc) delete mDesc;
	mDesc = NULL;
}


//==========================================================================
//
// starts a new intermission
//
//==========================================================================

void F_StartIntermission(FIntermissionDescriptor *desc, bool deleteme, bool endinggame)
{
	if (DIntermissionController::CurrentIntermission != NULL)
	{
		DIntermissionController::CurrentIntermission->Destroy();
	}
	V_SetBlend (0,0,0,0);
	S_StopAllChannels ();
	gameaction = ga_nothing;
	gamestate = GS_FINALE;
	viewactive = false;
	automapactive = false;
	DIntermissionController::CurrentIntermission = new DIntermissionController(desc, deleteme, endinggame);
}


//==========================================================================
//
// Called by main loop.
//
//==========================================================================

bool F_Responder (event_t* ev) 
{ 
	if (DIntermissionController::CurrentIntermission != NULL)
	{
		return DIntermissionController::CurrentIntermission->Responder(ev); 
	}
	return false;
}

//==========================================================================
//
// Called by main loop.
//
//==========================================================================

void F_Ticker () 
{
	if (DIntermissionController::CurrentIntermission != NULL)
	{
		DIntermissionController::CurrentIntermission->Ticker(); 
	}
}

//==========================================================================
//
// Called by main loop.
//
//==========================================================================

void F_Drawer ()
{
	if (DIntermissionController::CurrentIntermission != NULL)
	{
		DIntermissionController::CurrentIntermission->Drawer(); 
	}
}





#if 0
if (players[0].mo->FindInventory (QuestItemClasses[24]) ||
	players[0].mo->FindInventory (QuestItemClasses[27]))
{
	FinalePart = good;
}
else
{
	FinalePart = ok;
}
#endif


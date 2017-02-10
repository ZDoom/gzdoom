/*
** playerdisplay.cpp
** The player display for the player setup and class selection screen
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
#include "d_player.h"
#include "templates.h"
#include "menu/menu.h"
#include "colormatcher.h"
#include "textures/textures.h"
#include "w_wad.h"
#include "v_font.h"
#include "v_video.h"
#include "g_level.h"
#include "gi.h"
#include "r_defs.h"
#include "r_state.h"
#include "r_data/r_translate.h"


//=============================================================================
//
//
//
//=============================================================================
IMPLEMENT_CLASS(DListMenuItemPlayerDisplay, false, false)
DListMenuItemPlayerDisplay::DListMenuItemPlayerDisplay(DListMenuDescriptor *menu, int x, int y, PalEntry c1, PalEntry c2, bool np, FName action)
: DMenuItemBase(x, y, action)
{
	mOwner = menu;

	FRemapTable *bdremap = translationtables[TRANSLATION_Players][MAXPLAYERS + 1];
	for (int i = 0; i < 256; i++)
	{
		int r = c1.r + c2.r * i / 255;
		int g = c1.g + c2.g * i / 255;
		int b = c1.b + c2.b * i / 255;
		bdremap->Remap[i] = ColorMatcher.Pick (r, g, b);
		bdremap->Palette[i] = PalEntry(255, r, g, b);
	}
	auto id = TexMan.CheckForTexture("PlayerBackdrop", FTexture::TEX_MiscPatch);
	mBackdrop = TexMan[id];
	mPlayerClass = NULL;
	mPlayerState = NULL;
	mNoportrait = np;
	mMode = 0;
	mRotation = 0;
	mTranslate = false;
	mSkin = 0;
	mRandomClass = 0;
	mRandomTimer = 0;
	mClassNum = -1;
}


//=============================================================================
//
//
//
//=============================================================================

void DListMenuItemPlayerDisplay::OnDestroy()
{
}

//=============================================================================
//
//
//
//=============================================================================

void DListMenuItemPlayerDisplay::UpdateRandomClass()
{
	if (--mRandomTimer < 0)
	{
		if (++mRandomClass >= (int)PlayerClasses.Size ()) mRandomClass = 0;
		mPlayerClass = &PlayerClasses[mRandomClass];
		mPlayerState = GetDefaultByType (mPlayerClass->Type)->SeeState;
		if (mPlayerState == NULL)
		{ // No see state, so try spawn state.
			mPlayerState = GetDefaultByType (mPlayerClass->Type)->SpawnState;
		}
		mPlayerTics = mPlayerState != NULL ? mPlayerState->GetTics() : -1;
		mRandomTimer = 6;

		// Since the newly displayed class may used a different translation
		// range than the old one, we need to update the translation, too.
		UpdateTranslation();
	}
}

//=============================================================================
//
//
//
//=============================================================================

void DListMenuItemPlayerDisplay::UpdateTranslation()
{
	int PlayerColor = players[consoleplayer].userinfo.GetColor();
	int	PlayerSkin = players[consoleplayer].userinfo.GetSkin();
	int PlayerColorset = players[consoleplayer].userinfo.GetColorSet();

	if (mPlayerClass != NULL)
	{
		PlayerSkin = R_FindSkin (skins[PlayerSkin].name, int(mPlayerClass - &PlayerClasses[0]));
		R_GetPlayerTranslation(PlayerColor, GetColorSet(mPlayerClass->Type, PlayerColorset),
			&skins[PlayerSkin], translationtables[TRANSLATION_Players][MAXPLAYERS]);
	}
}

//=============================================================================
//
//
//
//=============================================================================

void DListMenuItemPlayerDisplay::SetPlayerClass(int classnum, bool force)
{
	if (classnum < 0 || classnum >= (int)PlayerClasses.Size ())
	{
		if (mClassNum != -1)
		{
			mClassNum = -1;
			mRandomTimer = 0;
			UpdateRandomClass();
		}
	}
	else if (mPlayerClass != &PlayerClasses[classnum] || force)
	{
		mPlayerClass = &PlayerClasses[classnum];
		mPlayerState = GetDefaultByType (mPlayerClass->Type)->SeeState;
		if (mPlayerState == NULL)
		{ // No see state, so try spawn state.
			mPlayerState = GetDefaultByType (mPlayerClass->Type)->SpawnState;
		}
		mPlayerTics = mPlayerState != NULL ? mPlayerState->GetTics() : -1;
		mClassNum = classnum;
	}
}

//=============================================================================
//
//
//
//=============================================================================

bool DListMenuItemPlayerDisplay::UpdatePlayerClass()
{
	if (mOwner->mSelectedItem >= 0)
	{
		int classnum;
		FName seltype = mOwner->mItems[mOwner->mSelectedItem]->GetAction(&classnum);

		if (seltype != NAME_Episodemenu) return false;
		if (PlayerClasses.Size() == 0) return false;

		SetPlayerClass(classnum);
		return true;
	}
	return false;
}

//=============================================================================
//
//
//
//=============================================================================

bool DListMenuItemPlayerDisplay::SetValue(int i, int value)
{
	switch (i)
	{
	case PDF_MODE:
		mMode = value;
		return true;

	case PDF_ROTATION:
		mRotation = value;
		return true;

	case PDF_TRANSLATE:
		mTranslate = value;

	case PDF_CLASS:
		SetPlayerClass(value, true);
		break;

	case PDF_SKIN:
		mSkin = value;
		break;
	}
	return false;
}

//=============================================================================
//
//
//
//=============================================================================

void DListMenuItemPlayerDisplay::Ticker()
{
	if (mClassNum < 0) UpdateRandomClass();

	if (mPlayerState != NULL && mPlayerState->GetTics () != -1 && mPlayerState->GetNextState () != NULL)
	{
		if (--mPlayerTics <= 0)
		{
			mPlayerState = mPlayerState->GetNextState();
			mPlayerTics = mPlayerState->GetTics();
		}
	}
}

//=============================================================================
//
//
//
//=============================================================================

void DListMenuItemPlayerDisplay::Drawer(bool selected)
{
	if (mMode == 0 && !UpdatePlayerClass())
	{
		return;
	}

	FName portrait = ((APlayerPawn*)GetDefaultByType(mPlayerClass->Type))->Portrait;

	if (portrait != NAME_None && !mNoportrait)
	{
		FTextureID texid = TexMan.CheckForTexture(portrait.GetChars(), FTexture::TEX_MiscPatch);
		if (texid.isValid())
		{
			FTexture *tex = TexMan(texid);
			if (tex != NULL)
			{
				screen->DrawTexture (tex, mXpos, mYpos, DTA_Clean, true, TAG_DONE);
				return;
			}
		}
	}
	int x = (mXpos - 160) * CleanXfac + (SCREENWIDTH>>1);
	int y = (mYpos - 100) * CleanYfac + (SCREENHEIGHT>>1);

	screen->DrawTexture(mBackdrop, x, y - 1,
		DTA_DestWidth, 72 * CleanXfac,
		DTA_DestHeight, 80 * CleanYfac,
		DTA_TranslationIndex, TRANSLATION(TRANSLATION_Players, MAXPLAYERS + 1),
		DTA_Masked, true,
		TAG_DONE);

	V_DrawFrame (x, y, 72*CleanXfac, 80*CleanYfac-1);

	spriteframe_t *sprframe = NULL;
	DVector2 Scale;

	if (mPlayerState != NULL)
	{
		if (mSkin == 0)
		{
			sprframe = &SpriteFrames[sprites[mPlayerState->sprite].spriteframes + mPlayerState->GetFrame()];
			Scale = GetDefaultByType(mPlayerClass->Type)->Scale;
		}
		else
		{
			sprframe = &SpriteFrames[sprites[skins[mSkin].sprite].spriteframes + mPlayerState->GetFrame()];
			Scale = skins[mSkin].Scale;
		}
	}

	if (sprframe != NULL)
	{
		FTexture *tex = TexMan(sprframe->Texture[mRotation]);
		if (tex != NULL && tex->UseType != FTexture::TEX_Null)
		{
			int trans = mTranslate? TRANSLATION(TRANSLATION_Players, MAXPLAYERS) : 0;
			screen->DrawTexture (tex,
				x + 36*CleanXfac, y + 71*CleanYfac,
				DTA_DestWidthF, tex->GetScaledWidthDouble() * CleanXfac * Scale.X,
				DTA_DestHeightF, tex->GetScaledHeightDouble() * CleanYfac * Scale.Y,
				DTA_TranslationIndex, trans,
				DTA_FlipX, sprframe->Flip & (1 << mRotation),
				TAG_DONE);
		}
	}
}


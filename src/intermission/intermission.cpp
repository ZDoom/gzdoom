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
#include "g_game.h"
#include "d_event.h"
#include "filesystem.h"
#include "gi.h"
#include "v_video.h"
#include "d_main.h"
#include "gstrings.h"
#include "intermission/intermission.h"
#include "actor.h"
#include "d_player.h"
#include "r_state.h"
#include "c_bind.h"
#include "p_conversation.h"
#include "menu.h"
#include "d_net.h"
#include "g_levellocals.h"
#include "utf8.h"

#include "s_music.h"
#include "texturemanager.h"
#include "v_draw.h"
#include "doommenu.h"
#include "sbar.h"
#include "screenjob.h"
#include "vm.h"

FIntermissionDescriptorList IntermissionDescriptors;

IMPLEMENT_CLASS(DIntermissionScreen, false, false)
IMPLEMENT_CLASS(DIntermissionScreenFader, false, false)
IMPLEMENT_CLASS(DIntermissionScreenText, false, false)
IMPLEMENT_CLASS(DIntermissionScreenCast, false, false)
IMPLEMENT_CLASS(DIntermissionScreenScroller, false, false)
IMPLEMENT_CLASS(DIntermissionController, false, true)

IMPLEMENT_POINTERS_START(DIntermissionController)
	IMPLEMENT_POINTER(mScreen)
IMPLEMENT_POINTERS_END

extern int		NoWipe;

CVAR(Bool, nointerscrollabort, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);

CVAR(Bool, inter_classic_scaling, true, CVAR_ARCHIVE);

DEFINE_ACTION_FUNCTION(_Screen, GetTextScreenSize)
{
	auto scale = active_con_scaletext(twod, generic_ui);
	int hudwidth = twod->GetWidth() / scale;
	int hudheight = twod->GetHeight() / scale;
	ACTION_RETURN_VEC2(DVector2(hudwidth, hudheight));
}

//==========================================================================
//
// This also gets used by the title loop.
//
//==========================================================================

void DrawFullscreenSubtitle(FFont* font, const char *text)
{
	if (!text || !*text || !inter_subtitles) return;
	if (*text == '$') text = GStrings[text + 1];

	// This uses the same scaling as regular HUD messages
	auto scale = active_con_scaletext(twod, generic_ui);
	int hudwidth = twod->GetWidth() / scale;
	int hudheight = twod->GetHeight() / scale;

	int linelen = hudwidth < 640 ? Scale(hudwidth, 9, 10) - 40 : 560;
	auto lines = V_BreakLines(font, linelen, text);
	int height = 20;

	for (unsigned i = 0; i < lines.Size(); i++) height += font->GetHeight();

	int x, y, w;

	if (linelen < 560)
	{
		x = hudwidth / 20;
		y = hudheight * 9 / 10 - height;
		w = hudwidth - 2 * x;
	}
	else
	{
		x = (hudwidth >> 1) - 300;
		y = hudheight * 9 / 10 - height;
		if (y < 0) y = 0;
		w = 600;
	}
	Dim(twod, 0, 0.5f, Scale(x, twod->GetWidth(), hudwidth), Scale(y, twod->GetHeight(), hudheight),
		Scale(w, twod->GetWidth(), hudwidth), Scale(height, twod->GetHeight(), hudheight));
	x += 20;
	y += 10;
	for (const FBrokenLines &line : lines)
	{
		DrawText(twod, font, CR_UNTRANSLATED, x, y, line.Text.GetChars(),
			DTA_KeepRatio, true,
			DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight, TAG_DONE);
		y += font->GetHeight();
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DIntermissionScreen::Init(FIntermissionAction *desc, bool first)
{
	if (!first && desc->mMusic.IsNotEmpty())
	{
		S_ChangeMusic (desc->mMusic.GetChars(), desc->mMusicOrder, desc->mMusicLooping);
	}
	mDuration = desc->mDuration;

	const char *texname = desc->mBackground.GetChars();
	if (*texname == '@')
	{
		char *pp;
		unsigned int v = strtoul(texname+1, &pp, 10) - 1;
		if (*pp == 0 && v < gameinfo.finalePages.Size())
		{
			texname = gameinfo.finalePages[v].GetChars();
		}
		else if (gameinfo.finalePages.Size() > 0)
		{
			texname = gameinfo.finalePages[0].GetChars();
		}
		else
		{
			texname = gameinfo.TitlePage.GetChars();
		}
	}
	else if (*texname == '$')
	{
		texname = GStrings(texname+1);
	}
	if (texname[0] != 0)
	{
		mBackground = TexMan.CheckForTexture(texname, ETextureType::MiscPatch);
		mFlatfill = desc->mFlatfill;
	}
	S_Sound (CHAN_VOICE, CHANF_UI, desc->mSound, 1.0f, ATTN_NONE);
	mOverlays.Resize(desc->mOverlays.Size());
	for (unsigned i=0; i < mOverlays.Size(); i++)
	{
		mOverlays[i].x = desc->mOverlays[i].x;
		mOverlays[i].y = desc->mOverlays[i].y;
		mOverlays[i].mCondition = desc->mOverlays[i].mCondition;
		mOverlays[i].mPic = TexMan.CheckForTexture(desc->mOverlays[i].mName.GetChars(), ETextureType::MiscPatch);
	}
	mTicker = 0;
	mSubtitle = desc->mSubtitle;
	mFirst = first;
	// If this is the first element of an intermission we must delay starting the music until Start() is called.
	mMusic = desc->mMusic;
	mMusicLooping = desc->mMusicLooping;
	mMusicOrder = desc->mMusicOrder;
}

void DIntermissionScreen::Start()
{
	if (mFirst)
	{
		if (mMusic.IsEmpty())
		{
			S_ChangeMusic(gameinfo.finaleMusic.GetChars(), gameinfo.finaleOrder, mMusicLooping);
		}
		else
		{
			S_ChangeMusic(mMusic.GetChars(), mMusicOrder, mMusicLooping);
		}
	}
}

int DIntermissionScreen::Responder (FInputEvent *ev)
{
	if (ev->Type == EV_KeyDown)
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

bool DIntermissionScreen::CheckOverlay(int i)
{
	if (mOverlays[i].mCondition == NAME_Multiplayer && !multiplayer) return false;
	else if (mOverlays[i].mCondition != NAME_None)
	{
		if (multiplayer || players[0].mo == NULL) return false;
		const PClass *cls = PClass::FindClass(mOverlays[i].mCondition);
		if (cls == NULL) return false;
		if (!players[0].mo->IsKindOf(cls)) return false;
	}
	return true;
}

void DIntermissionScreen::Drawer ()
{
	if (mBackground.isValid())
	{
		if (!mFlatfill)
		{
			DrawTexture(twod, mBackground, false, 0, 0, DTA_Fullscreen, true, TAG_DONE);
		}
		else
		{
			twod->AddFlatFill(0,0, twod->GetWidth(), twod->GetHeight(), TexMan.GetGameTexture(mBackground), (inter_classic_scaling ? -1 : 0));
		}
	}
	else
	{
		ClearRect(twod, 0, 0, twod->GetWidth(), twod->GetHeight(), 0, 0);
	}
	for (unsigned i=0; i < mOverlays.Size(); i++)
	{
		if (CheckOverlay(i))
			DrawTexture(twod, mOverlays[i].mPic, false, mOverlays[i].x, mOverlays[i].y, DTA_320x200, true, TAG_DONE);
	}
	FFont* font = generic_ui ? NewSmallFont : SmallFont;
	DrawFullscreenSubtitle(font, mSubtitle.GetChars());
}

void DIntermissionScreen::OnDestroy()
{
	S_StopSound(CHAN_VOICE);
	Super::OnDestroy();
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

//===========================================================================
//
// FadePic
//
//===========================================================================

int DIntermissionScreenFader::Responder (FInputEvent *ev)
{
	if (ev->Type == EV_KeyDown)
	{
		return -1;
	}
	return Super::Responder(ev);
}

int DIntermissionScreenFader::Ticker ()
{
	if (mFlatfill || !mBackground.isValid()) return -1;
	return Super::Ticker();
}

void DIntermissionScreenFader::Drawer ()
{
	if (!mFlatfill && mBackground.isValid())
	{
		double factor = clamp(double(mTicker) / mDuration, 0., 1.);
		if (mType == FADE_In) factor = 1.0 - factor;
		int color = MAKEARGB(int(factor*255), 0,0,0);

		DrawTexture(twod, mBackground, false, 0, 0, DTA_Fullscreen, true, DTA_ColorOverlay, color, TAG_DONE);
		for (unsigned i=0; i < mOverlays.Size(); i++)
		{
			if (CheckOverlay(i))
				DrawTexture(twod, mOverlays[i].mPic, false, mOverlays[i].x, mOverlays[i].y, DTA_320x200, true, DTA_ColorOverlay, color, TAG_DONE);
		}
	}
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
	if (mText[0] == '$') mText = GStrings(&mText[1]);

	auto lines = mText.Split("\n");
	mText = "";
	for (auto& line : lines)
	{
		line.StripRight();
		mText << line << "\n";
	}

	mTextSpeed = static_cast<FIntermissionActionTextscreen*>(desc)->mTextSpeed;
	mTextX = static_cast<FIntermissionActionTextscreen*>(desc)->mTextX;
	usesDefault = mTextX < 0;
	if (mTextX < 0) mTextX =gameinfo.TextScreenX;
	mTextY = static_cast<FIntermissionActionTextscreen*>(desc)->mTextY;
	if (mTextY < 0) mTextY =gameinfo.TextScreenY;

	if (!generic_ui)
	{
		// Todo: Split too long texts

		// If the text is too wide, center it so that it works better on widescreen displays.
		// On 4:3 it'd still be cut off, though.
		int width = SmallFont->StringWidth(mText);
		if (usesDefault && mTextX + width > 320 - mTextX)
		{
			mTextX = (320 - width) / 2;
		}
	}
	else
	{
		// Todo: Split too long texts

		mTextX *= 2;
		mTextY *= 2;
		int width = NewSmallFont->StringWidth(mText);
		if (usesDefault && mTextX + width > 640 - mTextX)
		{
			mTextX = (640 - width) / 2;
		}
	}


	mTextLen = (int)mText.CharacterCount();
	mTextDelay = static_cast<FIntermissionActionTextscreen*>(desc)->mTextDelay;
	mTextColor = static_cast<FIntermissionActionTextscreen*>(desc)->mTextColor;
	// For text screens, the duration only counts when the text is complete.
	if (mDuration > 0) mDuration += mTextDelay + mTextSpeed * mTextLen;
}

int DIntermissionScreenText::Responder (FInputEvent *ev)
{
	if (ev->Type == EV_KeyDown)
	{
		if (mTicker < mTextDelay + (mTextLen * mTextSpeed))
		{
			mTicker = mTextDelay + (mTextLen * mTextSpeed);
			return 1;
		}
	}
	return Super::Responder(ev);
}

void DIntermissionScreenText::Drawer ()
{
	Super::Drawer();
	if (mTicker >= mTextDelay)
	{
		int w;
		size_t count;
		int c;
		const uint8_t *ch = (const uint8_t*)mText.GetChars();

		// Count number of rows in this text. Since it does not word-wrap, we just count
		// line feed characters.
		int numrows;
		auto font = generic_ui ? NewSmallFont : SmallFont;
		auto fontscale = max(generic_ui ? min(twod->GetWidth() / 640, twod->GetHeight() / 400) : min(twod->GetWidth() / 400, twod->GetHeight() / 250), 1);
		int cleanwidth = twod->GetWidth() / fontscale;
		int cleanheight = twod->GetHeight() / fontscale;
		int refwidth = generic_ui ? 640 : 320;
		int refheight = generic_ui ? 400 : 200;
		const int kerning = font->GetDefaultKerning();

		for (numrows = 1, c = 0; ch[c] != '\0'; ++c)
		{
			numrows += (ch[c] == '\n');
		}

		int rowheight = font->GetHeight() * fontscale;
		int rowpadding = (generic_ui? 2 : ((gameinfo.gametype & (GAME_DoomStrifeChex) ? 3 : -1))) * fontscale;

		int cx = (mTextX - refwidth/2) * fontscale + twod->GetWidth() / 2;
		int cy = (mTextY - refheight/2) * fontscale + twod->GetHeight() / 2;
		cx = max<int>(0, cx);
		int startx = cx;

		if (usesDefault)
		{
			int allheight;
			while ((allheight = numrows * (rowheight + rowpadding)), allheight > twod->GetHeight() && rowpadding > 0)
			{
				rowpadding--;
			}
			allheight = numrows * (rowheight + rowpadding);
			if (twod->GetHeight() - cy - allheight < cy)
			{
				cy = (twod->GetHeight() - allheight) / 2;
				if (cy < 0) cy = 0;
			}
		}
		else
		{
			// Does this text fall off the end of the screen? If so, try to eliminate some margins first.
			while (rowpadding > 0 && cy + numrows * (rowheight + rowpadding) - rowpadding > twod->GetHeight())
			{
				rowpadding--;
			}
			// If it's still off the bottom, you are screwed if the origin is fixed.
		}
		rowheight += rowpadding;

		// draw some of the text onto the screen
		count = (mTicker - mTextDelay) / mTextSpeed;

		for ( ; count > 0 ; count-- )
		{
			c = GetCharFromString(ch);
			if (!c)
				break;
			if (c == '\n')
			{
				cx = startx;
				cy += rowheight;
				continue;
			}

			w = font->GetCharWidth(c);
			w += kerning;
			w *= fontscale;
			if (cx + w > twod->GetWidth())
				continue;

			DrawChar(twod, font, mTextColor, cx/fontscale, cy/fontscale, c, DTA_KeepRatio, true, DTA_VirtualWidth, cleanwidth, DTA_VirtualHeight, cleanheight, TAG_DONE);
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
	mName = static_cast<FIntermissionActionCast*>(desc)->mName.GetChars();
	mClass = PClass::FindActor(static_cast<FIntermissionActionCast*>(desc)->mCastClass);
	if (mClass != NULL) mDefaults = GetDefaultByType(mClass);
	else
	{
		mDefaults = NULL;
		caststate = NULL;
		return;
	}

	mCastSounds.Resize(static_cast<FIntermissionActionCast*>(desc)->mCastSounds.Size());
	for (unsigned i=0; i < mCastSounds.Size(); i++)
	{
		mCastSounds[i].mSequence = static_cast<FIntermissionActionCast*>(desc)->mCastSounds[i].mSequence;
		mCastSounds[i].mIndex = static_cast<FIntermissionActionCast*>(desc)->mCastSounds[i].mIndex;
		mCastSounds[i].mSound = S_FindSound(static_cast<FIntermissionActionCast*>(desc)->mCastSounds[i].mSound);
	}
	caststate = mDefaults->SeeState;
	if (mClass->IsDescendantOf(NAME_PlayerPawn))
	{
		advplayerstate = mDefaults->MissileState;
		casttranslation = TRANSLATION(TRANSLATION_Players, consoleplayer);
	}
	else
	{
		advplayerstate = NULL;
		casttranslation = 0;
		if (mDefaults->Translation != 0)
		{
			casttranslation = mDefaults->Translation;
		}
	}
	castdeath = false;
	castframes = 0;
	castonmelee = 0;
	castattacking = false;
	if (mDefaults->SeeSound.isvalid())
	{
		S_Sound (CHAN_VOICE, CHANF_UI, mDefaults->SeeSound, 1, ATTN_NONE);
	}
}

int DIntermissionScreenCast::Responder (FInputEvent *ev)
{
	if (ev->Type != EV_KeyDown) return 0;

	if (castdeath)
		return 1;					// already in dying frames

	castdeath = true;

	if (mClass != NULL)
	{
		FName label[] = {NAME_Death, NAME_Cast};
		caststate = mClass->FindState(2, label);
		if (caststate == NULL) return -1;

		casttics = caststate->GetTics();
		castframes = 0;
		castattacking = false;

		if (mClass->IsDescendantOf(NAME_PlayerPawn))
		{
			auto snd = S_FindSkinnedSound(players[consoleplayer].mo, S_FindSound("*death"));
			if (snd != NO_SOUND) S_Sound (CHAN_VOICE, CHANF_UI, snd, 1, ATTN_NONE);
		}
		else if (mDefaults->DeathSound.isvalid())
		{
			S_Sound (CHAN_VOICE, CHANF_UI, mDefaults->DeathSound, 1, ATTN_NONE);
		}
	}
	return true;
}

void DIntermissionScreenCast::PlayAttackSound()
{
	// sound hacks....
	if (caststate != NULL && castattacking)
	{
		for (unsigned i = 0; i < mCastSounds.Size(); i++)
		{
			if ((!!mCastSounds[i].mSequence) == (basestate != mDefaults->MissileState) &&
				(caststate == basestate + mCastSounds[i].mIndex))
			{
				S_StopAllChannels ();
				S_Sound (CHAN_WEAPON, CHANF_UI, mCastSounds[i].mSound, 1, ATTN_NONE);
				return;
			}
		}
	}

}

int DIntermissionScreenCast::Ticker ()
{
	Super::Ticker();

	if (--casttics > 0 && caststate != NULL)
		return 0; 				// not time to change state yet

	if (caststate == NULL || caststate->GetTics() == -1 || caststate->GetNextState() == NULL ||
		(caststate->GetNextState() == caststate && castdeath))
	{
		return -1;
	}
	else
	{
		// just advance to next state in animation
		if (caststate == advplayerstate)
			goto stopattack;	// Oh, gross hack!

		caststate = caststate->GetNextState();

		PlayAttackSound();
		castframes++;
	}

	if (castframes == 12 && !castdeath)
	{
		// go into attack frame
		castattacking = true;
		if (!mClass->IsDescendantOf(NAME_PlayerPawn))
		{
			if (castonmelee)
				basestate = caststate = mDefaults->MeleeState;
			else
				basestate = caststate = mDefaults->MissileState;
			castonmelee ^= 1;
			if (caststate == NULL)
			{
				if (castonmelee)
					basestate = caststate = mDefaults->MeleeState;
				else
					basestate = caststate = mDefaults->MissileState;
			}
		}
		else
		{
			// The players use the melee state differently so it can't be used here
			basestate = caststate = mDefaults->MissileState;
		}
		PlayAttackSound();
	}

	if (castattacking)
	{
		if (castframes == 24 || caststate == mDefaults->SeeState )
		{
		  stopattack:
			castattacking = false;
			castframes = 0;
			caststate = mDefaults->SeeState;
		}
	}

	casttics = caststate->GetTics();
	if (casttics == -1)
		casttics = 15;
	return 0;
}

void DIntermissionScreenCast::Drawer ()
{
	spriteframe_t*		sprframe;

	Super::Drawer();

	const char *name = mName;
	if (name != NULL)
	{
		auto font = generic_ui ? NewSmallFont : SmallFont;
		if (*name == '$') name = GStrings(name+1);
		DrawText(twod, font, CR_UNTRANSLATED,
			(twod->GetWidth() - font->StringWidth (name) * CleanXfac)/2,
			(twod->GetHeight() * 180) / 200,
			name,
			DTA_CleanNoMove, true, TAG_DONE);
	}

	// draw the current frame in the middle of the screen
	if (caststate != NULL)
	{
		DVector2 castscale = mDefaults->Scale;

		int castsprite = caststate->sprite;

		if (!(mDefaults->flags4 & MF4_NOSKIN) &&
			mDefaults->SpawnState != NULL && caststate->sprite == mDefaults->SpawnState->sprite &&
			mClass->IsDescendantOf(NAME_PlayerPawn) &&
			Skins.Size() > 0)
		{
			// Only use the skin sprite if this class has not been removed from the
			// PlayerClasses list.
			for (unsigned i = 0; i < PlayerClasses.Size(); ++i)
			{
				if (PlayerClasses[i].Type == mClass)
				{
					FPlayerSkin *skin = &Skins[players[consoleplayer].userinfo.GetSkin()];
					castsprite = skin->sprite;

					if (!(mDefaults->flags4 & MF4_NOSKIN))
					{
						castscale = skin->Scale;
					}

				}
			}
		}

		sprframe = &SpriteFrames[sprites[castsprite].spriteframes + caststate->GetFrame()];
		auto pic = TexMan.GetGameTexture(sprframe->Texture[0], true);

		DrawTexture(twod, pic, 160, 170,
			DTA_320x200, true,
			DTA_FlipX, sprframe->Flip & 1,
			DTA_DestHeightF, pic->GetDisplayHeight() * castscale.Y,
			DTA_DestWidthF, pic->GetDisplayWidth() * castscale.X,
			DTA_RenderStyle, mDefaults->RenderStyle,
			DTA_Alpha, mDefaults->Alpha,
			DTA_TranslationIndex, casttranslation,
			TAG_DONE);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DIntermissionScreenScroller::Init(FIntermissionAction *desc, bool first)
{
	Super::Init(desc, first);
	mFirstPic = mBackground;
	mSecondPic = TexMan.CheckForTexture(static_cast<FIntermissionActionScroller*>(desc)->mSecondPic.GetChars(), ETextureType::MiscPatch);
	mScrollDelay = static_cast<FIntermissionActionScroller*>(desc)->mScrollDelay;
	mScrollTime = static_cast<FIntermissionActionScroller*>(desc)->mScrollTime;
	mScrollDir = static_cast<FIntermissionActionScroller*>(desc)->mScrollDir;
}

int DIntermissionScreenScroller::Responder (FInputEvent *ev)
{
	int res = Super::Responder(ev);
	if (res == -1 && !nointerscrollabort)
	{
		mBackground = mSecondPic;
		mTicker = mScrollDelay + mScrollTime;
	}
	return res;
}

void DIntermissionScreenScroller::Drawer ()
{
	auto tex = TexMan.GetGameTexture(mFirstPic);
	auto tex2 = TexMan.GetGameTexture(mSecondPic);

	// These must round down to the nearest full pixel to cover seams between the two textures.
	int fwidth = (int)tex->GetDisplayWidth();
	int fheight = (int)tex->GetDisplayHeight();
	int fwidth2 = (int)tex2->GetDisplayWidth();
	int fheight2 = (int)tex2->GetDisplayHeight();

	double xpos1 = 0, ypos1 = 0, xpos2 = 0, ypos2 = 0;

	int aheight = fheight == 200 ? 240 : fheight == 400 ? 480 : fheight;
	DoubleRect drect;

	if (mScrollDir == SCROLL_Left || mScrollDir == SCROLL_Right)
	{
		// guesstimate the intended aspect ratio.
		int awidth = aheight * 4 / 3;
		int atotalwidth = fwidth + fwidth2;
		int sidespace = (atotalwidth - 2*awidth);
		// Now set a clipping rectangle for the intended viewport
		double displayratio = atotalwidth / double(aheight) - 4./3.;
		double displaywidth = aheight * displayratio;
		GetFullscreenRect(displaywidth, aheight, FSMode_ScaleToFit43, &drect);
		twod->SetClipRect(int(drect.left), int(drect.top), int(drect.width), int(drect.height));

		int ticker = clamp(mTicker - mScrollDelay, 0, mScrollTime);

		switch (mScrollDir)
		{
		case SCROLL_Left:
		default:
			xpos2 = -awidth + double(ticker) * awidth / mScrollTime;
			xpos1 = xpos2 + fwidth2;
			break;

		case SCROLL_Right:
			xpos1 = -double(ticker) * awidth / mScrollTime;
			xpos2 = xpos1 + fwidth;
			break;
		}
		double scale = drect.height / aheight;
		xpos1 *= scale;
		xpos2 *= scale;

		DrawTexture(twod, tex, xpos1 + drect.left, drect.top,
			DTA_DestWidthF, fwidth * scale,
			DTA_DestHeightF, aheight * scale,
			DTA_Masked, false,
			TAG_DONE);
		DrawTexture(twod, tex2, xpos2 + drect.left, drect.top,
			DTA_DestWidthF, fwidth2 * scale,
			DTA_DestHeightF, aheight * scale,
			DTA_Masked, false,
			TAG_DONE);


		twod->ClearClipRect();
	}
	else
	{
		// guesstimate the intended aspect ratio.
		GetFullscreenRect(fwidth, aheight, FSMode_ScaleToFit43, &drect);
		twod->SetClipRect(int(drect.left), int(drect.top), int(drect.width), int(drect.height));

		int ticker = clamp(mTicker - mScrollDelay, 0, mScrollTime);

		switch (mScrollDir)
		{
		case SCROLL_Up:
		default:
			ypos1 = double(ticker) * aheight / mScrollTime;
			ypos2 = ypos1 - aheight;
			break;

		case SCROLL_Down:
			ypos1 = -double(ticker) * aheight / mScrollTime;
			ypos2 = ypos1 + aheight;
			break;
		}
		double scale = drect.height / aheight;
		ypos1 *= scale;
		ypos2 *= scale;

		DrawTexture(twod, tex, drect.left, drect.top + ypos1,
			DTA_DestWidthF, fwidth * scale,
			DTA_DestHeightF, aheight * scale,
			DTA_Masked, false,
			TAG_DONE);
		DrawTexture(twod, tex2, drect.left, drect.top + ypos2,
			DTA_DestWidthF, fwidth2 * scale,
			DTA_DestHeightF, aheight * scale,
			DTA_Masked, false,
			TAG_DONE);


		twod->ClearClipRect();
	}
	mBackground = mSecondPic;
}

//==========================================================================
//
//
//
//==========================================================================

DIntermissionController::DIntermissionController(FIntermissionDescriptor *Desc, bool DeleteDesc, bool ending)
{
	mDesc = Desc;
	mDeleteDesc = DeleteDesc;
	mIndex = 0;
	mAdvance = false;
	mSentAdvance = false;
	mScreen = nullptr;
	mFirst = true;
	mEndGame = ending;
}

bool DIntermissionController::NextPage ()
{
	FTextureID bg;
	bool fill = false;

	if (mIndex == (int)mDesc->mActions.Size() && mDesc->mLink == NAME_None)
	{
		// last page (must block when ending the game.)
		return mEndGame;
	}
	bg.SetInvalid();

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

void DIntermissionController::Start()
{
	if (mScreen) mScreen->Start();
}

bool DIntermissionController::Responder (FInputEvent *ev)
{
	if (mScreen != NULL)
	{
		if (ev->Type == EV_KeyDown)
		{
			const char *cmd = Bindings.GetBind (ev->KeyScan);

			if (cmd != nullptr)
			{
				if (!stricmp(cmd, "toggleconsole") || !stricmp(cmd, "screenshot"))
				{
					return false;
				}
				// The following is needed to be able to enter main menu with a controller,
				// by pressing buttons that are usually assigned to this action, Start and Back by default
				else if (!stricmp(cmd, "menu_main") || !stricmp(cmd, "pause"))
				{
					M_StartControlPanel(true);
					M_SetMenu(NAME_Mainmenu, -1);
					return true;
				}
			}
		}

		if (mScreen->mTicker < 2) return false;	// prevent some leftover events from auto-advancing
		int res = mScreen->Responder(ev);
		if (res == -1)
		{
			mAdvance = true;
		}
		return !!res;
	}
	return false;
}

bool DIntermissionController::Ticker ()
{
	if (mAdvance)
	{
		mSentAdvance = false;
	}
	if (mScreen != NULL)
	{
		mAdvance |= (mScreen->Ticker() == -1);
	}
	if (mAdvance)
	{
		mAdvance = false;
		if (!NextPage())
		{
			return false;
		}
	}
	return true;
}

void DIntermissionController::Drawer ()
{
	if (mScreen != NULL)
	{
		twod->ClearScreen();
		mScreen->Drawer();
	}
}

void DIntermissionController::OnDestroy ()
{
	Super::OnDestroy();
	if (mScreen != NULL) mScreen->Destroy();
	if (mDeleteDesc) delete mDesc;
	mDesc = NULL;
}


//==========================================================================
//
// starts a new intermission
//
//==========================================================================

DIntermissionController* F_StartIntermission(FIntermissionDescriptor *desc, bool deleteme, bool ending)
{
	ScaleOverrider s(twod);
	S_StopAllChannels ();
	gameaction = ga_nothing;
	gamestate = GS_FINALE;
	//if (state == FSTATE_InLevel) wipegamestate = GS_FINALE;	// don't wipe when within a level.
	auto CurrentIntermission = Create<DIntermissionController>(desc, deleteme, ending);

	// If the intermission finishes straight away then cancel the wipe.
	if (!CurrentIntermission->NextPage())
	{
		CurrentIntermission->Destroy();
		return nullptr;
	}

	GC::WriteBarrier(CurrentIntermission);
	return CurrentIntermission;
}


//==========================================================================
//
// starts a new intermission
//
//==========================================================================

DIntermissionController* F_StartIntermission(FName seq)
{
	FIntermissionDescriptor **pdesc = IntermissionDescriptors.CheckKey(seq);
	if (pdesc == nullptr || (*pdesc)->mActions.Size() == 0)
	{
		return nullptr;
	}
	else
	{
		auto desc = *pdesc;
		auto action = desc->mActions[0];
		if (action->mClass == TITLE_ID)
		{
			// is handled elsewhere.
			return nullptr;
		}

		return F_StartIntermission(desc, false);
	}
}


DEFINE_ACTION_FUNCTION(DIntermissionController, Responder)
{
	PARAM_SELF_PROLOGUE(DIntermissionController);
	PARAM_POINTER(evt, FInputEvent);
	ACTION_RETURN_BOOL(self->Responder(evt));
}

DEFINE_ACTION_FUNCTION(DIntermissionController, Ticker)
{
	PARAM_SELF_PROLOGUE(DIntermissionController);
	ACTION_RETURN_BOOL(self->Ticker());
}

DEFINE_ACTION_FUNCTION(DIntermissionController, Start)
{
	PARAM_SELF_PROLOGUE(DIntermissionController);
	self->Start();
	return 0;
}

DEFINE_ACTION_FUNCTION(DIntermissionController, Drawer)
{
	PARAM_SELF_PROLOGUE(DIntermissionController);
	self->Drawer();
	return 0;
}

DEFINE_ACTION_FUNCTION(DIntermissionController, NextPage)
{
	PARAM_SELF_PROLOGUE(DIntermissionController);
	ACTION_RETURN_BOOL(self->NextPage());
}

#include "c_dispatch.h"

CCMD(measureintermissions)
{
	static const char *intermissions[] = {
		"E1TEXT", "E2TEXT", "E3TEXT", "E4TEXT",
		"C1TEXT", "C2TEXT", "C3TEXT", "C4TEXT", "C5TEXT",
		"P1TEXT", "P2TEXT", "P3TEXT", "P4TEXT", "P5TEXT",
		"T1TEXT", "T2TEXT", "T3TEXT", "T4TEXT", "T5TEXT", "NERVETEXT",
		"HE1TEXT", "HE2TEXT", "HE3TEXT", "HE4TEXT", "HE5TEXT",
		"TXT_HEXEN_CLUS1MSG", "TXT_HEXEN_CLUS2MSG","TXT_HEXEN_CLUS3MSG","TXT_HEXEN_CLUS4MSG",
		"TXT_HEXEN_WIN1MSG", "TXT_HEXEN_WIN2MSG","TXT_HEXEN_WIN3MSG",
		"TXT_HEXDD_CLUS1MSG", "TXT_HEXDD_CLUS2MSG",
		"TXT_HEXDD_WIN1MSG", "TXT_HEXDD_WIN2MSG","TXT_HEXDD_WIN3MSG" };

	static const char *languages[] = { "", "cz", "de", "eng", "es", "esm", "fr", "hu", "it", "pl", "pt", "ro", "ru", "sr" };

	for (auto l : languages)
	{
		int langid = *l ? MAKE_ID(l[0], l[1], l[2], 0) : FStringTable::default_table;
		for (auto t : intermissions)
		{
			auto text = GStrings.GetLanguageString(t, langid);
			if (text)
			{
				auto ch = text;
				int numrows, c;
				for (numrows = 1, c = 0; ch[c] != '\0'; ++c)
				{
					numrows += (ch[c] == '\n');
				}
				int width = SmallFont->StringWidth(text);
				if (width > 360 || numrows > 20)
					Printf("%s, %s: %d x %d\n", t, l, width, numrows);
			}
		}
	}
}

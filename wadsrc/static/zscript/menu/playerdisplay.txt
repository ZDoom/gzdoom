/*
** playerdisplay.cpp
** The player display for the player setup and class selection screen
**
**---------------------------------------------------------------------------
** Copyright 2010-2017 Christoph Oelckers
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

//=============================================================================
//
// the player sprite window
//
//=============================================================================

class ListMenuItemPlayerDisplay : ListMenuItem
{
	ListMenuDescriptor mOwner;
	TextureID mBackdrop;
	PlayerClass mPlayerClass;
	State mPlayerState;
	int mPlayerTics;
	bool mNoportrait;
	int8 mRotation;
	int8 mMode;	// 0: automatic (used by class selection), 1: manual (used by player setup)
	int8 mTranslate;
	int mSkin;
	int mRandomClass;
	int mRandomTimer;
	int mClassNum;
	Color mBaseColor;
	Color mAddColor;

	enum EPDFlags
	{
		PDF_ROTATION = 0x10001,
		PDF_SKIN = 0x10002,
		PDF_CLASS = 0x10003,
		PDF_MODE = 0x10004,
		PDF_TRANSLATE = 0x10005,
	};

	//=============================================================================
	//
	//
	//
	//=============================================================================
	void Init(ListMenuDescriptor menu, int x, int y, Color c1, Color c2, bool np = false, Name command = 'None' )
	{
		Super.Init(x, y, command);
		mOwner = menu;
		mBaseColor = c1;
		mAddColor = c2;

		mBackdrop = TexMan.CheckForTexture("B@CKDROP", TexMan.Type_MiscPatch);	// The weird name is to avoid clashes with mods.
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
	
	private void UpdatePlayer(int classnum)
	{
		mPlayerClass = PlayerClasses[classnum];
		mPlayerState = GetDefaultByType (mPlayerClass.Type).SeeState;
		if (mPlayerState == NULL)
		{ // No see state, so try spawn state.
			mPlayerState = GetDefaultByType (mPlayerClass.Type).SpawnState;
		}
		mPlayerTics = mPlayerState != NULL ? mPlayerState.Tics : -1;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	private void UpdateRandomClass()
	{
		if (--mRandomTimer < 0)
		{
			if (++mRandomClass >= PlayerClasses.Size ()) mRandomClass = 0;
			UpdatePlayer(mRandomClass);
			mPlayerTics = mPlayerState != NULL ? mPlayerState.Tics : -1;
			mRandomTimer = 6;

			// Since the newly displayed class may use a different translation
			// range than the old one, we need to update the translation, too.
			Translation.SetPlayerTranslation(TRANSLATION_Players, MAXPLAYERS, consoleplayer, mPlayerClass);
		}
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	void SetPlayerClass(int classnum, bool force = false)
	{
		if (classnum < 0 || classnum >= PlayerClasses.Size ())
		{
			if (mClassNum != -1)
			{
				mClassNum = -1;
				mRandomTimer = 0;
				UpdateRandomClass();
			}
		}
		else if (mPlayerClass != PlayerClasses[classnum] || force)
		{
			UpdatePlayer(classnum);
			mClassNum = classnum;
		}
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	bool UpdatePlayerClass()
	{
		if (mOwner.mSelectedItem >= 0)
		{
			int classnum;
			Name seltype;

			[seltype, classnum] = mOwner.mItems[mOwner.mSelectedItem].GetAction();

			if (seltype != 'Episodemenu') return false;
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

	override bool SetValue(int i, int value)
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

	override void Ticker()
	{
		if (mClassNum < 0) UpdateRandomClass();

		if (mPlayerState != NULL && mPlayerState.Tics != -1 && mPlayerState.NextState != NULL)
		{
			if (--mPlayerTics <= 0)
			{
				mPlayerState = mPlayerState.NextState;
				mPlayerTics = mPlayerState.Tics;
			}
		}
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override void Drawer(bool selected)
	{
		if (mMode == 0 && !UpdatePlayerClass())
		{
			return;
		}
		let playdef = GetDefaultByType((class<PlayerPawn>)(mPlayerClass.Type));

		Name portrait = playdef.Portrait;

		if (portrait != 'None' && !mNoportrait)
		{
			TextureID texid = TexMan.CheckForTexture(portrait, TexMan.Type_MiscPatch);
			screen.DrawTexture (texid, true, mXpos, mYpos, DTA_Clean, true);
		}
		else
		{
			int x = int(mXpos - 160) * CleanXfac + (screen.GetWidth() >> 1);
			int y = int(mYpos - 100) * CleanYfac + (screen.GetHeight() >> 1);

			int r = mBaseColor.r + mAddColor.r;
			int g = mBaseColor.g + mAddColor.g;
			int b = mBaseColor.b + mAddColor.b;
			int m = max(r, g, b);
			r = r * 255 / m;
			g = g * 255 / m;
			b = b * 255 / m;
			Color c = Color(255, r, g, b);
			
			screen.DrawTexture(mBackdrop, false, x, y - 1,
				DTA_DestWidth, 72 * CleanXfac,
				DTA_DestHeight, 80 * CleanYfac,
				DTA_Color, c,
				DTA_Masked, true);

			Screen.DrawFrame (x, y, 72*CleanXfac, 80*CleanYfac-1);

			if (mPlayerState != NULL)
			{
				Vector2 Scale;
				TextureID sprite;
				bool flip;
				
				[sprite, flip, Scale] = mPlayerState.GetSpriteTexture(mRotation, mSkin, playdef.Scale);
			
				if (sprite.IsValid())
				{
					int trans = mTranslate? Translation.MakeID(TRANSLATION_Players, MAXPLAYERS) : 0;
					let tscale = TexMan.GetScaledSize(sprite);
					Scale.X *= CleanXfac * tscale.X;
					Scale.Y *= CleanYfac * tscale.Y;
					
					screen.DrawTexture (sprite, false,
						x + 36*CleanXfac, y + 71*CleanYfac,
						DTA_DestWidthF, Scale.X, DTA_DestHeightF, Scale.Y,
						DTA_TranslationIndex, trans,
						DTA_FlipX, flip);
				}
			}
		}
	}
}
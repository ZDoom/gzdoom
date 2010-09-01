/*
** playermenu.cpp
** The player setup menu
**
**---------------------------------------------------------------------------
** Copyright 2001-2010 Randy Heit
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

#include "menu/menu.h"
#include "v_video.h"
#include "v_font.h"
#include "gi.h"
#include "gstrings.h"
#include "d_player.h"
#include "d_event.h"
#include "d_gui.h"
#include "c_dispatch.h"
#include "teaminfo.h"
#include "v_palette.h"
#include "r_state.h"
#include "r_translate.h"

EXTERN_CVAR (String, playerclass)
EXTERN_CVAR (String, name)
EXTERN_CVAR (Int, team)
EXTERN_CVAR (Float, autoaim)
EXTERN_CVAR(Bool, neverswitchonpickup)

void R_GetPlayerTranslation (int color, const FPlayerColorSet *colorset, FPlayerSkin *skin, FRemapTable *table);

//=============================================================================
//
// Player's name
//
//=============================================================================

FPlayerNameBox::FPlayerNameBox(int x, int y, int frameofs, const char *text, FFont *font, EColorRange color, FName action)
: FListMenuItemSelectable(x, y, action)
{
	mText = copystring(text);
	mFont = font;
	mFontColor = color;
	mFrameSize = frameofs;
	mPlayerName[0] = 0;
	mEntering = false;
}

FPlayerNameBox::~FPlayerNameBox()
{
	if (mText != NULL) delete [] mText;
}

//=============================================================================
//
//
//
//=============================================================================

bool FPlayerNameBox::SetString(int i, const char *s)
{
	if (i == 0)
	{
		strncpy(mPlayerName, s, MAXPLAYERNAME);
		mPlayerName[MAXPLAYERNAME] = 0;
		return true;
	}
	return false;
}

bool FPlayerNameBox::GetString(int i, char *s, int len)
{
	if (i == 0)
	{
		strncpy(s, mPlayerName, len);
		s[len] = 0;
		return true;
	}
	return false;
}

//=============================================================================
//
// [RH] Width of the border is variable
//
//=============================================================================

void FPlayerNameBox::DrawBorder (int x, int y, int len)
{
	if (gameinfo.gametype & (GAME_DoomStrifeChex))
	{
		int i;

		screen->DrawTexture (TexMan["M_LSLEFT"], x-8, y+7, DTA_Clean, true, TAG_DONE);

		for (i = 0; i < len; i++)
		{
			screen->DrawTexture (TexMan["M_LSCNTR"], x, y+7, DTA_Clean, true, TAG_DONE);
			x += 8;
		}

		screen->DrawTexture (TexMan["M_LSRGHT"], x, y+7, DTA_Clean, true, TAG_DONE);
	}
	else
	{
		screen->DrawTexture (TexMan["M_FSLOT"], x, y+1, DTA_Clean, true, TAG_DONE);
	}
}

//=============================================================================
//
//
//
//=============================================================================

void FPlayerNameBox::Drawer()
{
	const char *text = mText;
	if (text != NULL)
	{
		if (*text == '$') text = GStrings(text+1);
		screen->DrawText(mFont, mFontColor, mXpos, mYpos, text, DTA_Clean, true, TAG_DONE);
	}

	// Draw player name box
	int x = mXpos + mFont->StringWidth(text) + 16 + mFrameSize;
	DrawBorder (x, mYpos - mFrameSize, MAXPLAYERNAME+1);
	if (!mEntering)
	{
		screen->DrawText (SmallFont, CR_UNTRANSLATED, x + mFrameSize, mYpos, mPlayerName,
			DTA_Clean, true, TAG_DONE);
	}
	else
	{
		size_t l = strlen(mEditName);
		mEditName[l] = (gameinfo.gametype & (GAME_DoomStrifeChex)) ? '_' : '[';

		screen->DrawText (SmallFont, CR_UNTRANSLATED, x + mFrameSize, mYpos, mEditName,
			DTA_Clean, true, TAG_DONE);
		
		mEditName[l] = 0;
	}
}

//=============================================================================
//
//
//
//=============================================================================

bool FPlayerNameBox::MenuEvent(int mkey, bool fromcontroller)
{
	if (mkey == MKEY_Enter)
	{
		strcpy(mEditName, mPlayerName);
		mEntering = true;
		DMenu *input = new DTextEnterMenu(DMenu::CurrentMenu, mEditName, MAXPLAYERNAME, 2, fromcontroller);
		M_ActivateMenu(input);
		return true;
	}
	else if (mkey == MKEY_Input)
	{
		strcpy(mPlayerName, mEditName);
		mEntering = false;
		return true;
	}
	else if (mkey == MKEY_Abort)
	{
		mEntering = false;
		return true;
	}
	return false;
}

//=============================================================================
//
// items for the player menu
//
//=============================================================================

FValueTextItem::FValueTextItem(int x, int y, const char *text, FFont *font, EColorRange color, EColorRange valuecolor, FName action)
: FListMenuItemSelectable(x, y, action)
{
	mText = copystring(text);
	mFont = font;
	mFontColor = color;
	mFontColor2 = valuecolor;
	mSelection = 0;
}

FValueTextItem::~FValueTextItem()
{
	if (mText != NULL) delete [] mText;
}

//=============================================================================
//
//
//
//=============================================================================

bool FValueTextItem::SetString(int i, const char *s)
{
	// should actually use the index...
	FString str = s;
	if (i==0) mSelections.Clear();
	mSelections.Push(str);
	return true;
}

//=============================================================================
//
//
//
//=============================================================================

bool FValueTextItem::SetValue(int i, int value)
{
	if (i == 0)
	{
		mSelection = value;
		return true;
	}
	return false;
}

bool FValueTextItem::GetValue(int i, int *pvalue)
{
	if (i == 0)
	{
		*pvalue = mSelection;
		return true;
	}
	return false;
}

//=============================================================================
//
//
//
//=============================================================================

bool FValueTextItem::MenuEvent (int mkey, bool fromcontroller)
{
	if (mSelections.Size() > 1)
	{
		if (mkey == MKEY_Left)
		{
			if (--mSelection < 0) mSelection = mSelections.Size() - 1;
			return true;
		}
		else if (mkey == MKEY_Right)
		{
			if (++mSelection >= (int)mSelections.Size()) mSelection = 0;
			return true;
		}
	}
	return false;
}

//=============================================================================
//
//
//
//=============================================================================

void FValueTextItem::Drawer()
{
	const char *text = mText;

	if (*text == '$') text = GStrings(text+1);
	screen->DrawText(mFont, mFontColor, mXpos, mYpos, text, DTA_Clean, true, TAG_DONE);

	int x = mXpos + mFont->StringWidth(text) + 8;
	if (mSelections.Size() > 0) screen->DrawText(mFont, mFontColor2, x, mYpos, mSelections[mSelection], DTA_Clean, true, TAG_DONE);
}

//=============================================================================
//
// items for the player menu
//
//=============================================================================

FSliderItem::FSliderItem(int x, int y, const char *text, FFont *font, EColorRange color, FName action, int min, int max, int step)
: FListMenuItemSelectable(x, y, action)
{
	mText = copystring(text);
	mFont = font;
	mFontColor = color;
	mSelection = 0;
	mMinrange = min;
	mMaxrange = max;
	mStep = step;
}

FSliderItem::~FSliderItem()
{
	if (mText != NULL) delete [] mText;
}

//=============================================================================
//
//
//
//=============================================================================

bool FSliderItem::SetValue(int i, int value)
{
	if (i == 0)
	{
		mSelection = value;
		return true;
	}
	return false;
}

bool FSliderItem::GetValue(int i, int *pvalue)
{
	if (i == 0)
	{
		*pvalue = mSelection;
		return true;
	}
	return false;
}

//=============================================================================
//
//
//
//=============================================================================

bool FSliderItem::MenuEvent (int mkey, bool fromcontroller)
{
	if (mkey == MKEY_Left)
	{
		if ((mSelection -= mStep) < mMinrange) mSelection = mMinrange;
		return true;
	}
	else if (mkey == MKEY_Right)
	{
		if ((mSelection += mStep) > mMaxrange) mSelection = mMaxrange;
		return true;
	}
	return false;
}

//=============================================================================
//
//
//
//=============================================================================

void FSliderItem::DrawSlider (int x, int y)
{
	int range = mMaxrange - mMinrange;
	int cur = mSelection - mMinrange;

	x = (x - 160) * CleanXfac + screen->GetWidth() / 2;
	y = (y - 100) * CleanYfac + screen->GetHeight() / 2;

	screen->DrawText (ConFont, CR_WHITE, x, y,
		"\x10\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x12",
		DTA_CellX, 8 * CleanXfac,
		DTA_CellY, 8 * CleanYfac,
		TAG_DONE);
	screen->DrawText (ConFont, CR_ORANGE, x + (5 + (int)((cur * 78) / range)) * CleanXfac, y,
		"\x13",
		DTA_CellX, 8 * CleanXfac,
		DTA_CellY, 8 * CleanYfac,
		TAG_DONE);
}

//=============================================================================
//
//
//
//=============================================================================

void FSliderItem::Drawer()
{
	const char *text = mText;

	if (*text == '$') text = GStrings(text+1);
	screen->DrawText(mFont, mFontColor, mXpos, mYpos, text, DTA_Clean, true, TAG_DONE);

	int x = SmallFont->StringWidth ("Green") + 8 + mXpos;
	DrawSlider (x, mYpos);
}


//=============================================================================
//
//
//
//=============================================================================

class DPlayerMenu : public DListMenu
{
	DECLARE_CLASS(DPlayerMenu, DListMenu)

	FPlayerClass *PlayerClass;
	TArray<int> PlayerColorSets;
	TArray<int> PlayerSkins;

	void PickPlayerClass (bool usepawn);
	void UpdateColorsets();
	void UpdateSkins();
	void UpdateTranslation();
	void SendNewColor (int red, int green, int blue);

	void PlayerNameChanged(FListMenuItem *li);
	void ColorSetChanged (FListMenuItem *li);
	void ClassChanged (FListMenuItem *li);
	void AutoaimChanged (FListMenuItem *li);
	void SkinChanged (FListMenuItem *li);


public:

	DPlayerMenu() {}
	void Init(DMenu *parent, FListMenuDescriptor *desc);
	bool Responder (event_t *ev);
	bool MenuEvent (int mkey, bool fromcontroller);
	void Ticker ();
	void Drawer ();
};

IMPLEMENT_CLASS(DPlayerMenu)

//=============================================================================
//
//
//
//=============================================================================

void DPlayerMenu::Init(DMenu *parent, FListMenuDescriptor *desc)
{
	FListMenuItem *li;

	Super::Init(parent, desc);
	PickPlayerClass(true);

	li = GetItem(NAME_Playerbox);
	if (li != NULL)
	{
		li->SetString(0, name);
	}

	li = GetItem(NAME_Team);
	if (li != NULL)
	{
		li->SetString(0, "None");
		for(unsigned i=0;i<Teams.Size(); i++)
		{
			li->SetString(i+1, Teams[i].GetName());
		}
		li->SetValue(0, team == TEAM_NONE? 0 : team + 1);
	}

	int mycolorset = players[consoleplayer].userinfo.colorset;
	int color = players[consoleplayer].userinfo.color;

	UpdateColorsets();

	li = GetItem(NAME_Red);
	if (li != NULL)
	{
		li->Enable(mycolorset == -1);
		li->SetValue(0, RPART(color));
	}

	li = GetItem(NAME_Green);
	if (li != NULL)
	{
		li->Enable(mycolorset == -1);
		li->SetValue(0, GPART(color));
	}

	li = GetItem(NAME_Blue);
	if (li != NULL)
	{
		li->Enable(mycolorset == -1);
		li->SetValue(0, BPART(color));
	}

	li = GetItem(NAME_Class);
	if (li != NULL)
	{
		if (PlayerClasses.Size() == 1)
		{
			li->SetString(0, PlayerClasses[0].Type->Meta.GetMetaString (APMETA_DisplayName));
			li->SetValue(0, 0);
		}
		else
		{
			li->SetString(0, "Random");
			for(unsigned i=0; i< PlayerClasses.Size(); i++)
			{
				li->SetString(i+1, PlayerClasses[i].Type->Meta.GetMetaString (APMETA_DisplayName));
				li->SetValue(0, players[consoleplayer].userinfo.PlayerClass + 1);
			}
		}
	}

	UpdateSkins();

	li = GetItem(NAME_Gender);
	if (li != NULL)
	{
		li->SetString(0, "male");
		li->SetString(1, "female");
		li->SetString(2, "other");
		li->SetValue(0, players[consoleplayer].userinfo.gender);
	}

	li = GetItem(NAME_Autoaim);
	if (li != NULL)
	{
		li->SetString(0, "Never");
		li->SetString(1, "Very Low");
		li->SetString(2, "Low");
		li->SetString(3, "Medium");
		li->SetString(4, "High");
		li->SetString(5, "Very High");
		li->SetString(6, "Always");

		int sel = 
			autoaim == 0 ? 0 :
			autoaim <= 0.25 ? 1 :
			autoaim <= 0.5 ? 2 :
			autoaim <= 1 ? 3 :
			autoaim <= 2 ? 4 :
			autoaim <= 3 ? 5:6;
		li->SetValue(0, sel);
	}

	li = GetItem(NAME_Switch);
	if (li != NULL)
	{
		li->SetString(0, "Yes");
		li->SetString(1, "No");
		li->SetValue(0, neverswitchonpickup);
	}

	if (mDesc->mSelectedItem < 0) mDesc->mSelectedItem = 1;

}

//=============================================================================
//
//
//
//=============================================================================

bool DPlayerMenu::Responder (event_t *ev)
{
	if (ev->type == EV_GUI_Event && ev->subtype == EV_GUI_Char && ev->data1 == ' ')
	{
		// turn the player sprite around
		return true;
	}
	return false;
}

//=============================================================================
//
//
//
//=============================================================================

void DPlayerMenu::UpdateTranslation()
{
	int PlayerColor = players[consoleplayer].userinfo.color;
	int	PlayerSkin = players[consoleplayer].userinfo.skin;
	int PlayerColorset = players[consoleplayer].userinfo.colorset;

	R_GetPlayerTranslation(PlayerColor,
		P_GetPlayerColorSet(PlayerClass->Type->TypeName, PlayerColorset),
		&skins[PlayerSkin], translationtables[TRANSLATION_Players][MAXPLAYERS]);
}

//=============================================================================
//
//
//
//=============================================================================

void DPlayerMenu::PickPlayerClass(bool usepawn)
{

	if (players[consoleplayer].mo != NULL)
	{
		PlayerClass = &PlayerClasses[players[consoleplayer].CurrentPlayerClass];
	}
	else
	{
		int pclass = 0;
		// [GRB] Pick a class from player class list
		if (PlayerClasses.Size () > 1)
		{
			pclass = players[consoleplayer].userinfo.PlayerClass;

			if (pclass < 0)
			{
				pclass = (MenuTime>>7) % PlayerClasses.Size ();
			}
		}
		PlayerClass = &PlayerClasses[pclass];
	}
	UpdateTranslation();
}

//=============================================================================
//
//
//
//=============================================================================

void DPlayerMenu::SendNewColor (int red, int green, int blue)
{
	char command[24];

	mysnprintf (command, countof(command), "color \"%02x %02x %02x\"", red, green, blue);
	C_DoCommand (command);
	UpdateTranslation();
}

//=============================================================================
//
//
//
//=============================================================================

void DPlayerMenu::UpdateColorsets()
{
	FListMenuItem *li = GetItem(NAME_Color);
	if (li != NULL)
	{
		int sel = 0;
		P_EnumPlayerColorSets(PlayerClass->Type->TypeName, &PlayerColorSets);
		li->SetString(0, "Custom");
		for(unsigned i=0;i<PlayerColorSets.Size(); i++)
		{
			FPlayerColorSet *colorset = P_GetPlayerColorSet(PlayerClass->Type->TypeName, PlayerColorSets[i]);
			li->SetString(i+1, colorset->Name);
		}
		int mycolorset = players[consoleplayer].userinfo.colorset;
		if (mycolorset != -1)
		{
			for(unsigned i=0;i<PlayerColorSets.Size(); i++)
			{
				if (PlayerColorSets[i] == mycolorset)
				{
					sel = i+1;
				}
			}
		}
		li->SetValue(0, sel);
	}
}

//=============================================================================
//
//
//
//=============================================================================

void DPlayerMenu::UpdateSkins()
{
	FListMenuItem *li = GetItem(NAME_Skin);
	if (li != NULL)
	{
		if (GetDefaultByType (PlayerClass->Type)->flags4 & MF4_NOSKIN ||
			players[consoleplayer].userinfo.PlayerClass == -1)
		{
			li->SetString(0, "Base");
			li->SetValue(0, 0);
			return;
		}
		else
		{
			int sel = 0;
			PlayerSkins.Clear();
			for(unsigned i=0;i<(unsigned)numskins; i++)
			{
				if (PlayerClass->CheckSkin(i))
				{
					int j = PlayerSkins.Push(i);
					li->SetString(j, skins[i].name);
					if (players[consoleplayer].userinfo.skin == i)
					{
						sel = j;
					}
				}
			}
			li->SetValue(0, sel);
		}
	}
	UpdateTranslation();
}

//=============================================================================
//
//
//
//=============================================================================

void DPlayerMenu::PlayerNameChanged(FListMenuItem *li)
{
	char pp[MAXPLAYERNAME+1];
	const char *p;
	if (li->GetString(0, pp, MAXPLAYERNAME+1))
	{
		FString command("name \"");

		// Escape any backslashes or quotation marks before sending the name to the console.
		for (p = pp; *p != '\0'; ++p)
		{
			if (*p == '"' || *p == '\\')
			{
				command << '\\';
			}
			command << *p;
		}
		command << '"';
		C_DoCommand (command);
	}
}

//=============================================================================
//
//
//
//=============================================================================

void DPlayerMenu::ColorSetChanged (FListMenuItem *li)
{
	int	sel;

	if (li->GetValue(0, &sel))
	{
		int mycolorset = -1;

		if (sel > 0) mycolorset = PlayerColorSets[sel-1];

		FListMenuItem *red   = GetItem(NAME_Red);
		FListMenuItem *green = GetItem(NAME_Green);
		FListMenuItem *blue  = GetItem(NAME_Blue);

		// disable the sliders if a valid colorset is selected
		if (red != NULL) red->Enable(mycolorset == -1);
		if (green != NULL) green->Enable(mycolorset == -1);
		if (blue != NULL) blue->Enable(mycolorset == -1);

		char command[24];
		mysnprintf(command, countof(command), "colorset %d", mycolorset);
		C_DoCommand(command);
		UpdateTranslation();
	}
}

//=============================================================================
//
//
//
//=============================================================================

void DPlayerMenu::ClassChanged (FListMenuItem *li)
{
	if (PlayerClasses.Size () == 1)
	{
		return;
	}

	int	sel;

	if (li->GetValue(0, &sel))
	{
		players[consoleplayer].userinfo.PlayerClass = sel-1;
		PickPlayerClass(false);

		cvar_set ("playerclass", 
			sel == 0 ? "Random" : PlayerClass->Type->Meta.GetMetaString (APMETA_DisplayName));
	}
	UpdateColorsets();
}

//=============================================================================
//
//
//
//=============================================================================

void DPlayerMenu::SkinChanged (FListMenuItem *li)
{
	if (GetDefaultByType (PlayerClass->Type)->flags4 & MF4_NOSKIN ||
		players[consoleplayer].userinfo.PlayerClass == -1)
	{
		return;
	}

	int	sel;

	if (li->GetValue(0, &sel))
	{
		sel = PlayerSkins[sel];
		players[consoleplayer].userinfo.skin = sel;
		UpdateTranslation();
		char buffer[10];
		mysnprintf(buffer, 10, "%d", sel);
		cvar_set ("skin", buffer);
	}
}

//=============================================================================
//
//
//
//=============================================================================

void DPlayerMenu::AutoaimChanged (FListMenuItem *li)
{
	static const float ranges[] = { 0, 0.25, 0.5, 1, 2, 3, 5000 };

	int	sel;

	if (li->GetValue(0, &sel))
	{
		autoaim = ranges[sel];
	}
}

//=============================================================================
//
//
//
//=============================================================================

bool DPlayerMenu::MenuEvent (int mkey, bool fromcontroller)
{
	int v;
	if (mDesc->mSelectedItem >= 0)
	{
		FListMenuItem *li = mDesc->mItems[mDesc->mSelectedItem];
		if (li->MenuEvent(mkey, fromcontroller))
		{
			FName current = li->GetAction(NULL);
			switch(current)
			{
				// item specific handling comes here

			case NAME_Playerbox:
				PlayerNameChanged(li);
				break;

			case NAME_Team:
				if (li->GetValue(0, &v))
				{
					team = v==0? TEAM_NONE : v-1;
				}
				break;

			case NAME_Color:
					ColorSetChanged(li);
					break;

			case NAME_Red:
				if (li->GetValue(0, &v))
				{
					int color = players[consoleplayer].userinfo.color;
					SendNewColor (v, GPART(color), BPART(color));
				}
				break;

			case NAME_Green:
				if (li->GetValue(0, &v))
				{
					int color = players[consoleplayer].userinfo.color;
					SendNewColor (RPART(color), v, BPART(color));
				}
				break;

			case NAME_Blue:
				if (li->GetValue(0, &v))
				{
					int color = players[consoleplayer].userinfo.color;
					SendNewColor (RPART(color), GPART(color), v);
				}
				break;

			case NAME_Class:
				ClassChanged(li);
				break;

			case NAME_Skin:
				SkinChanged(li);
				break;

			case NAME_Gender:
				if (li->GetValue(0, &v))
				{
					cvar_set ("gender", v==0? "male" : v==1? "female" : "other");
				}
				break;

			case NAME_Autoaim:
				AutoaimChanged(li);
				break;

			case NAME_Switch:
				if (li->GetValue(0, &v))
				{
					neverswitchonpickup = !v;
				}
				break;

			default:
				break;
			}
			return true;
		}
	}
	return Super::MenuEvent(mkey, fromcontroller);
}

//=============================================================================
//
//
//
//=============================================================================

void DPlayerMenu::Ticker ()
{

	Super::Ticker();
}

//=============================================================================
//
//
//
//=============================================================================

void DPlayerMenu::Drawer ()
{

	Super::Drawer();
}

#if 0

/*
*/


//
// [RH] Player Setup Menu code
//
void M_PlayerSetup (void)
{
	OptionsActive = false;
	drawSkull = true;
	M_DemoNoPlay = true;
	if (demoplayback)
		G_CheckDemoStatus ();

	PlayerState = GetDefaultByType (PlayerClass->Type)->SeeState;
	PlayerTics = PlayerState->GetTics();

}


	{
		spriteframe_t *sprframe;
		fixed_t ScaleX, ScaleY;

		if (GetDefaultByType (PlayerClass->Type)->flags4 & MF4_NOSKIN ||
			players[consoleplayer].userinfo.PlayerClass == -1 ||
			PlayerState->sprite != GetDefaultByType (PlayerClass->Type)->SpawnState->sprite)
		{
			sprframe = &SpriteFrames[sprites[PlayerState->sprite].spriteframes + PlayerState->GetFrame()];
			ScaleX = GetDefaultByType(PlayerClass->Type)->scaleX;
			ScaleY = GetDefaultByType(PlayerClass->Type)->scaleY;
		}
		else
		{
			sprframe = &SpriteFrames[sprites[skins[PlayerSkin].sprite].spriteframes + PlayerState->GetFrame()];
			ScaleX = skins[PlayerSkin].ScaleX;
			ScaleY = skins[PlayerSkin].ScaleY;
		}

		if (sprframe != NULL)
		{
			FTexture *tex = TexMan(sprframe->Texture[0]);
			if (tex != NULL && tex->UseType != FTexture::TEX_Null)
			{
				if (tex->Rotations != 0xFFFF)
				{
					tex = TexMan(SpriteFrames[tex->Rotations].Texture[PlayerRotation]);
				}
				screen->DrawTexture (tex,
					(320 - 52 - 32 + xo - 160)*CleanXfac + (SCREENWIDTH)/2,
					(PSetupDef.y + LINEHEIGHT*3 + 57 - 104)*CleanYfac + (SCREENHEIGHT/2),
					DTA_DestWidth, MulScale16 (tex->GetWidth() * CleanXfac, ScaleX),
					DTA_DestHeight, MulScale16 (tex->GetHeight() * CleanYfac, ScaleY),
					DTA_Translation, translationtables[TRANSLATION_Players](MAXPLAYERS),
					TAG_DONE);
			}
		}

		const char *str = "PRESS " TEXTCOLOR_WHITE "SPACE";
		screen->DrawText (SmallFont, CR_GOLD, 320 - 52 - 32 -
			SmallFont->StringWidth (str)/2,
			PSetupDef.y + LINEHEIGHT*3 + 76, str,
			DTA_Clean, true, TAG_DONE);
		str = PlayerRotation ? "TO SEE FRONT" : "TO SEE BACK";
		screen->DrawText (SmallFont, CR_GOLD, 320 - 52 - 32 -
			SmallFont->StringWidth (str)/2,
			PSetupDef.y + LINEHEIGHT*3 + 76 + SmallFont->GetHeight (), str,
			DTA_Clean, true, TAG_DONE);
	}




static void M_PlayerSetupTicker (void)
{
	// Based on code in f_finale.c
	FPlayerClass *oldclass = PlayerClass;

	if (currentMenu == &ClassMenuDef)
	{
		int item;

		if (itemOn < ClassMenuDef.numitems-1)
			item = itemOn;
		else
			item = (MenuTime>>2) % (ClassMenuDef.numitems-1);

		PlayerClass = &PlayerClasses[D_PlayerClassToInt (ClassMenuItems[item].name)];
		P_EnumPlayerColorSets(PlayerClass->Type->TypeName, &PlayerColorSets);
	}
	else
	{
		PickPlayerClass ();
	}

	if (PlayerClass != oldclass)
	{
		PlayerState = GetDefaultByType (PlayerClass->Type)->SeeState;
		PlayerTics = PlayerState->GetTics();

		PlayerSkin = R_FindSkin (skins[PlayerSkin].name, int(PlayerClass - &PlayerClasses[0]));
		R_GetPlayerTranslation (players[consoleplayer].userinfo.color,
			P_GetPlayerColorSet(PlayerClass->Type->TypeName, players[consoleplayer].userinfo.colorset),
			&skins[PlayerSkin], translationtables[TRANSLATION_Players][MAXPLAYERS]);
	}
}


// item actions

/* cursor stuff*/
			
			// DRAW CURSOR
			if (drawSkull)
			{
				if (currentMenu == &PSetupDef)
				{
					// [RH] Use options menu cursor for the player setup menu.
					if (skullAnimCounter < 6)
					{
						screen->DrawText (ConFont, CR_RED, x - 16,
							currentMenu->y + int(item*PLAYERSETUP_LINEHEIGHT) +
							(!(gameinfo.gametype & (GAME_DoomStrifeChex)) ? 6 : -1), "\xd",
							DTA_Clean, true, TAG_DONE);
					}
				}

			}

#endif
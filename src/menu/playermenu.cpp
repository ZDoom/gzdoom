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
#include "d_event.h"
#include "d_gui.h"


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
	if (mSelections.Size() > 0) screen->DrawText(mFont, mFontColor2, mXpos, mYpos, mSelections[mSelection], DTA_Clean, true, TAG_DONE);
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

	li = GetItem(NAME_Playerbox);
	if (li != NULL)
	{
	}

	li = GetItem(NAME_Team);
	if (li != NULL)
	{
	}

	li = GetItem(NAME_Color);
	if (li != NULL)
	{
	}

	li = GetItem(NAME_Red);
	if (li != NULL)
	{
	}

	li = GetItem(NAME_Green);
	if (li != NULL)
	{
	}

	li = GetItem(NAME_Blue);
	if (li != NULL)
	{
	}

	li = GetItem(NAME_Class);
	if (li != NULL)
	{
	}

	li = GetItem(NAME_Skin);
	if (li != NULL)
	{
	}

	li = GetItem(NAME_Gender);
	if (li != NULL)
	{
	}

	li = GetItem(NAME_Autoaim);
	if (li != NULL)
	{
	}
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

bool DPlayerMenu::MenuEvent (int mkey, bool fromcontroller)
{
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
			case NAME_Team:
			case NAME_Color:
			case NAME_Red:
			case NAME_Green:
			case NAME_Blue:
			case NAME_Class:
			case NAME_Skin:
			case NAME_Gender:
			case NAME_Autoaim:

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
		const char *p;
		FString command("name \"");

		// Escape any backslashes or quotation marks before sending the name to the console.
		for (p = mPlayerName; *p != '\0'; ++p)
		{
			if (*p == '"' || *p == '\\')
			{
				command << '\\';
			}
			command << *p;
		}
		command << '"';
		C_DoCommand (command);
*/

//
// [RH] Player Setup Menu
//
static oldmenuitem_t PlayerSetupMenu[] =
{
	{ 1,0,'n',NULL,M_EditPlayerName, CR_UNTRANSLATED},
	{ 2,0,'t',NULL,M_ChangePlayerTeam, CR_UNTRANSLATED},
	{ 2,0,'c',NULL,M_ChangeColorSet, CR_UNTRANSLATED},
	{ 2,0,'r',NULL,M_SlidePlayerRed, CR_UNTRANSLATED},
	{ 2,0,'g',NULL,M_SlidePlayerGreen, CR_UNTRANSLATED},
	{ 2,0,'b',NULL,M_SlidePlayerBlue, CR_UNTRANSLATED},
	{ 2,0,'t',NULL,M_ChangeClass, CR_UNTRANSLATED},
	{ 2,0,'s',NULL,M_ChangeSkin, CR_UNTRANSLATED},
	{ 2,0,'e',NULL,M_ChangeGender, CR_UNTRANSLATED},
	{ 2,0,'a',NULL,M_ChangeAutoAim, CR_UNTRANSLATED},
	{ 2,0,'p',NULL,M_ChangeSwitchPickup, CR_UNTRANSLATED}
};



//
// [RH] Player Setup Menu code
//
void M_PlayerSetup (void)
{
	OptionsActive = false;
	drawSkull = true;
	strcpy (savegamestring, name);
	M_DemoNoPlay = true;
	if (demoplayback)
		G_CheckDemoStatus ();
	M_SetupNextMenu (&PSetupDef);
	if (players[consoleplayer].mo != NULL)
	{
		PlayerClass = &PlayerClasses[players[consoleplayer].CurrentPlayerClass];
	}
	PlayerSkin = players[consoleplayer].userinfo.skin;
	R_GetPlayerTranslation (players[consoleplayer].userinfo.color,
		P_GetPlayerColorSet(PlayerClass->Type->TypeName, players[consoleplayer].userinfo.colorset),
		&skins[PlayerSkin], translationtables[TRANSLATION_Players][MAXPLAYERS]);
	PlayerState = GetDefaultByType (PlayerClass->Type)->SeeState;
	PlayerTics = PlayerState->GetTics();
	if (FireTexture == NULL)
	{
		FireTexture = new FBackdropTexture;
	}
	P_EnumPlayerColorSets(PlayerClass->Type->TypeName, &PlayerColorSets);
}






/* playernamebox
*/
/* other menu items
*/


	// Draw player team setting
	x = SmallFont->StringWidth ("Team") + 8 + PSetupDef.x;
	screen->DrawText (SmallFont, label, PSetupDef.x, PSetupDef.y + LINEHEIGHT+yo, "Team",
		DTA_Clean, true, TAG_DONE);
	screen->DrawText (SmallFont, value, x, PSetupDef.y + LINEHEIGHT+yo,
		!TeamLibrary.IsValidTeam (players[consoleplayer].userinfo.team) ? "None" :
		Teams[players[consoleplayer].userinfo.team].GetName (),
		DTA_Clean, true, TAG_DONE);

	// Draw player color selection and sliders
	FPlayerColorSet *colorset = P_GetPlayerColorSet(PlayerClass->Type->TypeName, players[consoleplayer].userinfo.colorset);
	x = SmallFont->StringWidth("Color") + 8 + PSetupDef.x;
	screen->DrawText(SmallFont, label, PSetupDef.x, PSetupDef.y + LINEHEIGHT*2+yo, "Color", DTA_Clean, true, TAG_DONE);
	screen->DrawText(SmallFont, value, x, PSetupDef.y + LINEHEIGHT*2+yo,
		colorset != NULL ? colorset->Name.GetChars() : "Custom", DTA_Clean, true, TAG_DONE);

	// Only show the sliders for a custom color set.
	if (colorset == NULL)
	{
		screen->DrawText (SmallFont, label, PSetupDef.x, PSetupDef.y + int(LINEHEIGHT*2.875)+yo, "Red", DTA_Clean, true, TAG_DONE);
		screen->DrawText (SmallFont, label, PSetupDef.x, PSetupDef.y + int(LINEHEIGHT*3.5)+yo, "Green", DTA_Clean, true, TAG_DONE);
		screen->DrawText (SmallFont, label, PSetupDef.x, PSetupDef.y + int(LINEHEIGHT*4.125)+yo, "Blue", DTA_Clean, true, TAG_DONE);

		x = SmallFont->StringWidth ("Green") + 8 + PSetupDef.x;
		color = players[consoleplayer].userinfo.color;

		M_DrawPlayerSlider (x, PSetupDef.y + int(LINEHEIGHT*2.875)+yo, RPART(color));
		M_DrawPlayerSlider (x, PSetupDef.y + int(LINEHEIGHT*3.5)+yo, GPART(color));
		M_DrawPlayerSlider (x, PSetupDef.y + int(LINEHEIGHT*4.125)+yo, BPART(color));
	}

	// [GRB] Draw class setting
	int pclass = players[consoleplayer].userinfo.PlayerClass;
	x = SmallFont->StringWidth ("Class") + 8 + PSetupDef.x;
	screen->DrawText (SmallFont, label, PSetupDef.x, PSetupDef.y + LINEHEIGHT*5+yo, "Class", DTA_Clean, true, TAG_DONE);
	screen->DrawText (SmallFont, value, x, PSetupDef.y + LINEHEIGHT*5+yo,
		pclass == -1 ? "Random" : PlayerClasses[pclass].Type->Meta.GetMetaString (APMETA_DisplayName),
		DTA_Clean, true, TAG_DONE);

	// Draw skin setting
	x = SmallFont->StringWidth ("Skin") + 8 + PSetupDef.x;
	screen->DrawText (SmallFont, label, PSetupDef.x, PSetupDef.y + LINEHEIGHT*6+yo, "Skin", DTA_Clean, true, TAG_DONE);
	if (GetDefaultByType (PlayerClass->Type)->flags4 & MF4_NOSKIN ||
		players[consoleplayer].userinfo.PlayerClass == -1)
	{
		screen->DrawText (SmallFont, value, x, PSetupDef.y + LINEHEIGHT*6+yo, "Base", DTA_Clean, true, TAG_DONE);
	}
	else
	{
		screen->DrawText (SmallFont, value, x, PSetupDef.y + LINEHEIGHT*6+yo,
			skins[PlayerSkin].name, DTA_Clean, true, TAG_DONE);
	}

	// Draw gender setting
	x = SmallFont->StringWidth ("Gender") + 8 + PSetupDef.x;
	screen->DrawText (SmallFont, label, PSetupDef.x, PSetupDef.y + LINEHEIGHT*7+yo, "Gender", DTA_Clean, true, TAG_DONE);
	screen->DrawText (SmallFont, value, x, PSetupDef.y + LINEHEIGHT*7+yo,
		genders[players[consoleplayer].userinfo.gender], DTA_Clean, true, TAG_DONE);

	// Draw autoaim setting
	x = SmallFont->StringWidth ("Autoaim") + 8 + PSetupDef.x;
	screen->DrawText (SmallFont, label, PSetupDef.x, PSetupDef.y + LINEHEIGHT*8+yo, "Autoaim", DTA_Clean, true, TAG_DONE);
	screen->DrawText (SmallFont, value, x, PSetupDef.y + LINEHEIGHT*8+yo,
		autoaim == 0 ? "Never" :
		autoaim <= 0.25 ? "Very Low" :
		autoaim <= 0.5 ? "Low" :
		autoaim <= 1 ? "Medium" :
		autoaim <= 2 ? "High" :
		autoaim <= 3 ? "Very High" : "Always",
		DTA_Clean, true, TAG_DONE);

	// Draw Switch on Pickup setting
	x = SmallFont->StringWidth ("Switch on Pickup") + 8 + PSetupDef.x;
	screen->DrawText (SmallFont, label, PSetupDef.x, PSetupDef.y + LINEHEIGHT*9+yo, "Switch on Pickup", DTA_Clean, true, TAG_DONE);
	screen->DrawText (SmallFont, value, x, PSetupDef.y + LINEHEIGHT*9+yo,
		neverswitchonpickup == false ? "Yes" : "No",
		DTA_Clean, true, TAG_DONE);
		
		


/* player display*/
	// Draw player character
	{
		int x = 320 - 88 - 32 + xo, y = PSetupDef.y + LINEHEIGHT*3 - 18 + yo;

		x = (x-160)*CleanXfac+(SCREENWIDTH>>1);
		y = (y-100)*CleanYfac+(SCREENHEIGHT>>1);
		if (!FireTexture)
		{
			screen->Clear (x, y, x + 72 * CleanXfac, y + 80 * CleanYfac-1, 0, 0);
		}
		else
		{
			screen->DrawTexture (FireTexture, x, y - 1,
				DTA_DestWidth, 72 * CleanXfac,
				DTA_DestHeight, 80 * CleanYfac,
				DTA_Translation, &FireRemap,
				DTA_Masked, false,
				TAG_DONE);
		}

		M_DrawFrame (x, y, 72*CleanXfac, 80*CleanYfac-1);
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

static void M_ChangeClass (int choice)
{
	if (PlayerClasses.Size () == 1)
	{
		return;
	}

	int type = players[consoleplayer].userinfo.PlayerClass;

	if (!choice)
		type = (type < 0) ? (int)PlayerClasses.Size () - 1 : type - 1;
	else
		type = (type < (int)PlayerClasses.Size () - 1) ? type + 1 : -1;

	cvar_set ("playerclass", type < 0 ? "Random" :
		PlayerClasses[type].Type->Meta.GetMetaString (APMETA_DisplayName));
}

static void M_ChangeSkin (int choice)
{
	if (GetDefaultByType (PlayerClass->Type)->flags4 & MF4_NOSKIN ||
		players[consoleplayer].userinfo.PlayerClass == -1)
	{
		return;
	}

	do
	{
		if (!choice)
			PlayerSkin = (PlayerSkin == 0) ? (int)numskins - 1 : PlayerSkin - 1;
		else
			PlayerSkin = (PlayerSkin < (int)numskins - 1) ? PlayerSkin + 1 : 0;
	} while (!PlayerClass->CheckSkin (PlayerSkin));

	R_GetPlayerTranslation (players[consoleplayer].userinfo.color,
		P_GetPlayerColorSet(PlayerClass->Type->TypeName, players[consoleplayer].userinfo.colorset),
		&skins[PlayerSkin], translationtables[TRANSLATION_Players][MAXPLAYERS]);

	cvar_set ("skin", skins[PlayerSkin].name);
}

static void M_ChangeGender (int choice)
{
	int gender = players[consoleplayer].userinfo.gender;

	if (!choice)
		gender = (gender == 0) ? 2 : gender - 1;
	else
		gender = (gender == 2) ? 0 : gender + 1;

	cvar_set ("gender", genders[gender]);
}

static void M_ChangeAutoAim (int choice)
{
	static const float ranges[] = { 0, 0.25, 0.5, 1, 2, 3, 5000 };
	float aim = autoaim;
	int i;

	if (!choice) {
		// Select a lower autoaim

		for (i = 6; i >= 1; i--)
		{
			if (aim >= ranges[i])
			{
				aim = ranges[i - 1];
				break;
			}
		}
	}
	else
	{
		// Select a higher autoaim

		for (i = 5; i >= 0; i--)
		{
			if (aim >= ranges[i])
			{
				aim = ranges[i + 1];
				break;
			}
		}
	}

	autoaim = aim;
}

static void M_ChangeSwitchPickup (int choice)
{
	if (!choice)
		neverswitchonpickup = (neverswitchonpickup == 1) ? 0 : 1;
	else
		neverswitchonpickup = (neverswitchonpickup == 0) ? 1 : 0;
}



static void M_ChangePlayerTeam (int choice)
{
	if (!choice)
	{
		if (team == 0)
		{
			team = TEAM_NONE;
		}
		else if (team == TEAM_NONE)
		{
			team = Teams.Size () - 1;
		}
		else
		{
			team = team - 1;
		}
	}
	else
	{
		if (team == int(Teams.Size () - 1))
		{
			team = TEAM_NONE;
		}
		else if (team == TEAM_NONE)
		{
			team = 0;
		}
		else
		{
			team = team + 1;
		}	
	}
}

static void M_ChangeColorSet (int choice)
{
	int curpos = (int)PlayerColorSets.Size();
	int mycolorset = players[consoleplayer].userinfo.colorset;
	while (--curpos >= 0)
	{
		if (PlayerColorSets[curpos] == mycolorset)
			break;
	}
	if (choice == 0)
	{
		curpos--;
	}
	else
	{
		curpos++;
	}
	if (curpos < -1)
	{
		curpos = (int)PlayerColorSets.Size() - 1;
	}
	else if (curpos >= (int)PlayerColorSets.Size())
	{
		curpos = -1;
	}
	mycolorset = (curpos >= 0) ? PlayerColorSets[curpos] : -1;

	// disable the sliders if a valid colorset is selected
	PlayerSetupMenu[PSM_RED].status =
	PlayerSetupMenu[PSM_GREEN].status =
	PlayerSetupMenu[PSM_BLUE].status = (mycolorset == -1? 2:-1);

	char command[24];
	mysnprintf(command, countof(command), "colorset %d", mycolorset);
	C_DoCommand(command);
	R_GetPlayerTranslation(players[consoleplayer].userinfo.color,
		P_GetPlayerColorSet(PlayerClass->Type->TypeName, mycolorset),
		&skins[PlayerSkin], translationtables[TRANSLATION_Players][MAXPLAYERS]);
}

static void SendNewColor (int red, int green, int blue)
{
	char command[24];

	mysnprintf (command, countof(command), "color \"%02x %02x %02x\"", red, green, blue);
	C_DoCommand (command);
	R_GetPlayerTranslation(MAKERGB (red, green, blue),
		P_GetPlayerColorSet(PlayerClass->Type->TypeName, players[consoleplayer].userinfo.colorset),
		&skins[PlayerSkin], translationtables[TRANSLATION_Players][MAXPLAYERS]);
}

static void M_SlidePlayerRed (int choice)
{
	int color = players[consoleplayer].userinfo.color;
	int red = RPART(color);

	if (choice == 0) {
		red -= 16;
		if (red < 0)
			red = 0;
	} else {
		red += 16;
		if (red > 255)
			red = 255;
	}

	SendNewColor (red, GPART(color), BPART(color));
}

static void M_SlidePlayerGreen (int choice)
{
	int color = players[consoleplayer].userinfo.color;
	int green = GPART(color);

	if (choice == 0) {
		green -= 16;
		if (green < 0)
			green = 0;
	} else {
		green += 16;
		if (green > 255)
			green = 255;
	}

	SendNewColor (RPART(color), green, BPART(color));
}

static void M_SlidePlayerBlue (int choice)
{
	int color = players[consoleplayer].userinfo.color;
	int blue = BPART(color);

	if (choice == 0) {
		blue -= 16;
		if (blue < 0)
			blue = 0;
	} else {
		blue += 16;
		if (blue > 255)
			blue = 255;
	}

	SendNewColor (RPART(color), GPART(color), blue);
}




/* Change the player name*/







static void M_EditPlayerName (int choice)
{
	// we are going to be intercepting all chars
	genStringEnter = 2;
	genStringEnd = M_PlayerNameChanged;
	genStringCancel = M_PlayerNameNotChanged;
	genStringLen = MAXPLAYERNAME;
	
	saveSlot = 0;
	saveCharIndex = strlen (savegamestring);
}

static void M_PlayerNameNotChanged ()
{
	strcpy (savegamestring, name);
}



/* cursor stuff*/
			
			// DRAW CURSOR
			if (drawSkull)
			{
				if (currentMenu == &PSetupDef)
				{
					// [RH] Use options menu cursor for the player setup menu.
					if (skullAnimCounter < 6)
					{
						double item;
						// The green slider is halfway between lines, and the red and
						// blue ones are offset slightly to make room for it.
						if (itemOn < 3)
						{
							item = itemOn;
						}
						else if (itemOn > 5)
						{
							item = itemOn - 1;
						}
						else if (itemOn == 3)
						{
							item = 2.875;
						}
						else if (itemOn == 4)
						{
							item = 3.5;
						}
						else
						{
							item = 4.125;
						}
						screen->DrawText (ConFont, CR_RED, x - 16,
							currentMenu->y + int(item*PLAYERSETUP_LINEHEIGHT) +
							(!(gameinfo.gametype & (GAME_DoomStrifeChex)) ? 6 : -1), "\xd",
							DTA_Clean, true, TAG_DONE);
					}
				}

			}

#endif
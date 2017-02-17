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
#include "r_data/r_translate.h"
#include "v_text.h"

EXTERN_CVAR (String, playerclass)
EXTERN_CVAR (String, name)
EXTERN_CVAR (Int, team)
EXTERN_CVAR (Float, autoaim)
EXTERN_CVAR(Bool, neverswitchonpickup)
EXTERN_CVAR (Bool, cl_run)

//=============================================================================
//
//
//
//=============================================================================

class DPlayerMenu : public DListMenu
{
	DECLARE_CLASS(DPlayerMenu, DListMenu)

public:
	int PlayerClassIndex;
	FPlayerClass *PlayerClass;
	TArray<int> PlayerColorSets;
	TArray<int> PlayerSkins;
	int mRotation;

	void PickPlayerClass ();
	void UpdateColorsets();
	void UpdateSkins();
	void UpdateTranslation();
	void SendNewColor (int red, int green, int blue);

	void PlayerNameChanged(DMenuItemBase *li);
	void ColorSetChanged (DMenuItemBase *li);
	void ClassChanged (DMenuItemBase *li);
	void AutoaimChanged (DMenuItemBase *li);
	void SkinChanged (DMenuItemBase *li);


public:

	DPlayerMenu() {}
	void Init(DMenu *parent, DListMenuDescriptor *desc);
	bool Responder (event_t *ev);
	bool MenuEvent (int mkey, bool fromcontroller);
};

IMPLEMENT_CLASS(DPlayerMenu, false, false)

//=============================================================================
//
//
//
//=============================================================================
enum EPDFlags
{
	ListMenuItemPlayerDisplay_PDF_ROTATION = 0x10001,
	ListMenuItemPlayerDisplay_PDF_SKIN = 0x10002,
	ListMenuItemPlayerDisplay_PDF_CLASS = 0x10003,
	ListMenuItemPlayerDisplay_PDF_MODE = 0x10004,
	ListMenuItemPlayerDisplay_PDF_TRANSLATE = 0x10005,
};

void DPlayerMenu::Init(DMenu *parent, DListMenuDescriptor *desc)
{
	DMenuItemBase *li;

	Super::Init(parent, desc);
	PickPlayerClass();
	mRotation = 0;

	li = GetItem(NAME_Playerdisplay);
	if (li != NULL)
	{
		li->SetValue(ListMenuItemPlayerDisplay_PDF_ROTATION, 0);
		li->SetValue(ListMenuItemPlayerDisplay_PDF_MODE, 1);
		li->SetValue(ListMenuItemPlayerDisplay_PDF_TRANSLATE, 1);
		li->SetValue(ListMenuItemPlayerDisplay_PDF_CLASS, players[consoleplayer].userinfo.GetPlayerClassNum());
		if (PlayerClass != NULL && !(GetDefaultByType (PlayerClass->Type)->flags4 & MF4_NOSKIN) &&
			players[consoleplayer].userinfo.GetPlayerClassNum() != -1)
		{
			li->SetValue(ListMenuItemPlayerDisplay_PDF_SKIN, players[consoleplayer].userinfo.GetSkin());
		}
	}

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

	int mycolorset = players[consoleplayer].userinfo.GetColorSet();
	int color = players[consoleplayer].userinfo.GetColor();

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
			li->SetString(0, GetPrintableDisplayName(PlayerClasses[0].Type));
			li->SetValue(0, 0);
		}
		else
		{
			// [XA] Remove the "Random" option if the relevant gameinfo flag is set.
			if(!gameinfo.norandomplayerclass)
				li->SetString(0, "Random");
			for(unsigned i=0; i< PlayerClasses.Size(); i++)
			{
				const char *cls = GetPrintableDisplayName(PlayerClasses[i].Type);
				li->SetString(gameinfo.norandomplayerclass ? i : i+1, cls);
			}
			int pclass = players[consoleplayer].userinfo.GetPlayerClassNum();
			li->SetValue(0, gameinfo.norandomplayerclass && pclass >= 0 ? pclass : pclass + 1);
		}
	}

	UpdateSkins();

	li = GetItem(NAME_Gender);
	if (li != NULL)
	{
		li->SetValue(0, players[consoleplayer].userinfo.GetGender());
	}

	li = GetItem(NAME_Autoaim);
	if (li != NULL)
	{
		li->SetValue(0, (int)autoaim);
	}

	li = GetItem(NAME_Switch);
	if (li != NULL)
	{
		li->SetValue(0, neverswitchonpickup);
	}

	li = GetItem(NAME_AlwaysRun);
	if (li != NULL)
	{
		li->SetValue(0, cl_run);
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
		mRotation = 8 - mRotation;
		DMenuItemBase *li = GetItem(NAME_Playerdisplay);
		if (li != NULL)
		{
			li->SetValue(ListMenuItemPlayerDisplay_PDF_ROTATION, mRotation);
		}
		return true;
	}
	return Super::Responder(ev);
}

//=============================================================================
//
//
//
//=============================================================================

void DPlayerMenu::UpdateTranslation()
{
	int PlayerColor = players[consoleplayer].userinfo.GetColor();
	int	PlayerSkin = players[consoleplayer].userinfo.GetSkin();
	int PlayerColorset = players[consoleplayer].userinfo.GetColorSet();

	if (PlayerClass != NULL)
	{
		PlayerSkin = R_FindSkin (skins[PlayerSkin].name, int(PlayerClass - &PlayerClasses[0]));
		R_GetPlayerTranslation(PlayerColor, GetColorSet(PlayerClass->Type, PlayerColorset),
			&skins[PlayerSkin], translationtables[TRANSLATION_Players][MAXPLAYERS]);
	}
}

//=============================================================================
//
//
//
//=============================================================================

void DPlayerMenu::PickPlayerClass()
{
	int pclass = 0;
	// [GRB] Pick a class from player class list
	if (PlayerClasses.Size () > 1)
	{
		pclass = players[consoleplayer].userinfo.GetPlayerClassNum();

		if (pclass < 0)
		{
			pclass = (MenuTime>>7) % PlayerClasses.Size ();
		}
	}
	PlayerClassIndex = pclass;
	PlayerClass = &PlayerClasses[PlayerClassIndex];
	UpdateTranslation();
}

//=============================================================================
//
//
//
//=============================================================================

void DPlayerMenu::SendNewColor (int red, int green, int blue)
{
	players[consoleplayer].userinfo.ColorChanged(MAKERGB(red,green,blue));
	UpdateTranslation();
}

DEFINE_ACTION_FUNCTION(DPlayerMenu, ColorChanged)
{
	PARAM_SELF_PROLOGUE(DPlayerMenu);
	PARAM_INT(r);
	PARAM_INT(g);
	PARAM_INT(b);
	// only allow if the menu is active to prevent abuse.
	if (self == DMenu::CurrentMenu)
	{
		players[consoleplayer].userinfo.ColorChanged(MAKERGB(r, g, b));
	}
	return 0;
}


//=============================================================================
//
//
//
//=============================================================================

void DPlayerMenu::UpdateColorsets()
{
	DMenuItemBase *li = GetItem(NAME_Color);
	if (li != NULL)
	{
		int sel = 0;
		EnumColorSets(PlayerClass->Type, &PlayerColorSets);
		li->SetString(0, "Custom");
		for(unsigned i=0;i<PlayerColorSets.Size(); i++)
		{
			FPlayerColorSet *colorset = GetColorSet(PlayerClass->Type, PlayerColorSets[i]);
			li->SetString(i+1, colorset->Name);
		}
		int mycolorset = players[consoleplayer].userinfo.GetColorSet();
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
	int sel = 0;
	int skin;
	DMenuItemBase *li = GetItem(NAME_Skin);
	if (li != NULL)
	{
		if (GetDefaultByType (PlayerClass->Type)->flags4 & MF4_NOSKIN ||
			players[consoleplayer].userinfo.GetPlayerClassNum() == -1)
		{
			li->SetString(0, "Base");
			li->SetValue(0, 0);
			skin = 0;
		}
		else
		{
			PlayerSkins.Clear();
			for(int i=0;i<(int)numskins; i++)
			{
				if (PlayerClass->CheckSkin(i))
				{
					int j = PlayerSkins.Push(i);
					li->SetString(j, skins[i].name);
					if (players[consoleplayer].userinfo.GetSkin() == i)
					{
						sel = j;
					}
				}
			}
			li->SetValue(0, sel);
			skin = PlayerSkins[sel];
		}
		li = GetItem(NAME_Playerdisplay);
		if (li != NULL)
		{
			li->SetValue(ListMenuItemPlayerDisplay_PDF_SKIN, skin);
		}
	}
	UpdateTranslation();
}

//=============================================================================
//
//
//
//=============================================================================

void DPlayerMenu::PlayerNameChanged(DMenuItemBase *li)
{
	char pp[MAXPLAYERNAME+1];
	const char *p;
	if (li->GetString(0, pp, MAXPLAYERNAME))
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

void DPlayerMenu::ColorSetChanged (DMenuItemBase *li)
{
	int	sel;

	if (li->GetValue(0, &sel))
	{
		int mycolorset = -1;

		if (sel > 0) mycolorset = PlayerColorSets[sel-1];

		DMenuItemBase *red   = GetItem(NAME_Red);
		DMenuItemBase *green = GetItem(NAME_Green);
		DMenuItemBase *blue  = GetItem(NAME_Blue);

		// disable the sliders if a valid colorset is selected
		if (red != NULL) red->Enable(mycolorset == -1);
		if (green != NULL) green->Enable(mycolorset == -1);
		if (blue != NULL) blue->Enable(mycolorset == -1);

		players[consoleplayer].userinfo.ColorSetChanged(mycolorset);
		UpdateTranslation();
	}
}

//=============================================================================
//
//
//
//=============================================================================

void DPlayerMenu::ClassChanged (DMenuItemBase *li)
{
	if (PlayerClasses.Size () == 1)
	{
		return;
	}

	int	sel;

	if (li->GetValue(0, &sel))
	{
		players[consoleplayer].userinfo.PlayerClassNumChanged(gameinfo.norandomplayerclass ? sel : sel-1);
		PickPlayerClass();

		cvar_set ("playerclass", sel == 0 && !gameinfo.norandomplayerclass ? "Random" : GetPrintableDisplayName(PlayerClass->Type).GetChars());

		UpdateSkins();
		UpdateColorsets();
		UpdateTranslation();

		li = GetItem(NAME_Playerdisplay);
		if (li != NULL)
		{
			li->SetValue(ListMenuItemPlayerDisplay_PDF_CLASS, players[consoleplayer].userinfo.GetPlayerClassNum());
		}
	}
}

//=============================================================================
//
//
//
//=============================================================================

void DPlayerMenu::SkinChanged (DMenuItemBase *li)
{
	if (GetDefaultByType (PlayerClass->Type)->flags4 & MF4_NOSKIN ||
		players[consoleplayer].userinfo.GetPlayerClassNum() == -1)
	{
		return;
	}

	int	sel;

	if (li->GetValue(0, &sel))
	{
		sel = PlayerSkins[sel];
		players[consoleplayer].userinfo.SkinNumChanged(sel);
		UpdateTranslation();
		cvar_set ("skin", skins[sel].name);

		li = GetItem(NAME_Playerdisplay);
		if (li != NULL)
		{
			li->SetValue(ListMenuItemPlayerDisplay_PDF_SKIN, sel);
		}
	}
}

//=============================================================================
//
//
//
//=============================================================================

void DPlayerMenu::AutoaimChanged (DMenuItemBase *li)
{
	int	sel;

	if (li->GetValue(0, &sel))
	{
		autoaim = (float)sel;
	}
}

DEFINE_ACTION_FUNCTION(DPlayerMenu, AutoaimChanged)
{
	PARAM_SELF_PROLOGUE(DPlayerMenu);
	PARAM_FLOAT(val);
	// only allow if the menu is active to prevent abuse.
	if (self == DMenu::CurrentMenu)
	{
		autoaim = float(val);
	}
	return 0;
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
		DMenuItemBase *li = mDesc->mItems[mDesc->mSelectedItem];
		if (li->MenuEvent(mkey, fromcontroller))
		{
			FName current = li->GetAction(NULL);
			switch(current)
			{
				// item specific handling comes here

			case NAME_Playerbox:
				if (mkey == MKEY_Input)
				{
					PlayerNameChanged(li);
				}
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
					uint32 color = players[consoleplayer].userinfo.GetColor();
					SendNewColor (v, GPART(color), BPART(color));
				}
				break;

			case NAME_Green:
				if (li->GetValue(0, &v))
				{
					uint32 color = players[consoleplayer].userinfo.GetColor();
					SendNewColor (RPART(color), v, BPART(color));
				}
				break;

			case NAME_Blue:
				if (li->GetValue(0, &v))
				{
					uint32 color = players[consoleplayer].userinfo.GetColor();
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
					neverswitchonpickup = !!v;
				}
				break;

			case NAME_AlwaysRun:
				if (li->GetValue(0, &v))
				{
					cl_run = !!v;
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

DEFINE_FIELD(DPlayerMenu, mRotation)
DEFINE_FIELD_NAMED(DPlayerMenu, PlayerClass, mPlayerClass)

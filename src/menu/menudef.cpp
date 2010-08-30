/*
** menudef.cpp
** MENUDEF parser
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
#include "menu/menu.h"
#include "w_wad.h"
#include "sc_man.h"
#include "v_font.h"
#include "g_level.h"
#include "gi.h"

MenuDescriptorList MenuDescriptors;
static FListMenuDescriptor DefaultListMenuSettings;	// contains common settings for all list menus


//=============================================================================
//
//
//
//=============================================================================

static void SkipSubBlock(FScanner &sc)
{
	sc.MustGetStringName("{");
	int depth = 1;
	while (depth > 0)
	{
		sc.MustGetString();
		if (sc.Compare("{")) depth++;
		if (sc.Compare("}")) depth--;
	}
}

//=============================================================================
//
//
//
//=============================================================================

static bool CheckSkipGameBlock(FScanner &sc)
{
	int filter = 0;
	sc.MustGetStringName("(");
	do
	{
		sc.MustGetString();
		if (sc.Compare("Doom")) filter |= GAME_Doom;
		if (sc.Compare("Heretic")) filter |= GAME_Heretic;
		if (sc.Compare("Hexen")) filter |= GAME_Hexen;
		if (sc.Compare("Strife")) filter |= GAME_Strife;
		if (sc.Compare("Chex")) filter |= GAME_Chex;
	}
	while (sc.CheckString(","));
	sc.MustGetStringName(")");
	if (!(gameinfo.gametype & filter))
	{
		SkipSubBlock(sc);
		return true;
	}
	return false;
}

//=============================================================================
//
//
//
//=============================================================================

static bool CheckSkipOptionBlock(FScanner &sc)
{
	bool filter = false;
	sc.MustGetStringName("(");
	do
	{
		sc.MustGetString();
		if (sc.Compare("ReadThis")) filter |= gameinfo.drawreadthis;
	}
	while (sc.CheckString(","));
	sc.MustGetStringName(")");
	if (!filter)
	{
		SkipSubBlock(sc);
		return true;
	}
	return false;
}

//=============================================================================
//
//
//
//=============================================================================

static void ParseListMenuBody(FScanner &sc, FListMenuDescriptor *desc)
{
	sc.MustGetStringName("{");
	while (!sc.CheckString("}"))
	{
		sc.MustGetString();
		if (sc.Compare("ifgame"))
		{
			if (!CheckSkipGameBlock(sc))
			{
				// recursively parse sub-block
				ParseListMenuBody(sc, desc);
			}
		}
		else if (sc.Compare("ifoption"))
		{
			if (!CheckSkipOptionBlock(sc))
			{
				// recursively parse sub-block
				ParseListMenuBody(sc, desc);
			}
		}
		else if (sc.Compare("Selector"))
		{
			sc.MustGetString();
			desc->mSelector = TexMan.CheckForTexture(sc.String, FTexture::TEX_MiscPatch);
			sc.MustGetStringName(",");
			sc.MustGetNumber();
			desc->mSelectOfsX = sc.Number;
			sc.MustGetStringName(",");
			sc.MustGetNumber();
			desc->mSelectOfsY = sc.Number;
		}
		else if (sc.Compare("Linespacing"))
		{
			sc.MustGetNumber();
			desc->mLinespacing = sc.Number;
		}
		else if (sc.Compare("Position"))
		{
			sc.MustGetNumber();
			desc->mXpos = sc.Number;
			sc.MustGetStringName(",");
			sc.MustGetNumber();
			desc->mYpos = sc.Number;
		}
		else if (sc.Compare("StaticPatch"))
		{
			sc.MustGetNumber();
			int x = sc.Number;
			sc.MustGetStringName(",");
			sc.MustGetNumber();
			int y = sc.Number;
			sc.MustGetStringName(",");
			sc.MustGetString();
			FTextureID tex = TexMan.CheckForTexture(sc.String, FTexture::TEX_MiscPatch);

			FListMenuItem *it = new FListMenuItemStaticPatch(x, y, tex);
			desc->mItems.Push(it);
		}
		else if (sc.Compare("StaticAnimation"))
		{
			sc.MustGetNumber();
			int x = sc.Number;
			sc.MustGetStringName(",");
			sc.MustGetNumber();
			int y = sc.Number;
			sc.MustGetStringName(",");
			sc.MustGetString();
			FString templatestr = sc.String;
			sc.MustGetStringName(",");
			sc.MustGetNumber();
			int delay = sc.Number;
			FListMenuItemStaticAnimation *it = new FListMenuItemStaticAnimation(x, y, delay);
			desc->mItems.Push(it);
			sc.MustGetStringName(",");
			do
			{
				sc.MustGetNumber();
				int a = sc.Number;
				sc.MustGetStringName(",");
				sc.MustGetNumber();
				int b = sc.Number;
				int step = b >= a? 1:-1;
				b+=step;
				for(int c = a; c != b; c+=step)
				{
					FString texname;
					texname.Format(templatestr.GetChars(), c);
					FTextureID texid = TexMan.CheckForTexture(texname, FTexture::TEX_MiscPatch);
					it->AddTexture(texid);
				}
			}
			while (sc.CheckString(","));
		}
		else if (sc.Compare("PatchItem"))
		{
			sc.MustGetString();
			FTextureID tex = TexMan.CheckForTexture(sc.String, FTexture::TEX_MiscPatch);
			sc.MustGetStringName(",");
			sc.MustGetString();
			int hotkey = sc.String[0];
			sc.MustGetStringName(",");
			sc.MustGetString();
			FName action = sc.String;

			FListMenuItem *it = new FListMenuItemPatch(desc->mXpos, desc->mYpos, hotkey, tex, action);
			desc->mItems.Push(it);
			desc->mYpos += desc->mLinespacing;
			if (desc->mSelectedItem == -1) desc->mSelectedItem = desc->mItems.Size()-1;
		}
		else if (sc.Compare("TextItem"))
		{
			sc.MustGetString();
			FString text = sc.String;
			sc.MustGetStringName(",");
			sc.MustGetString();
			int hotkey = sc.String[0];
			sc.MustGetStringName(",");
			sc.MustGetString();
			FName action = sc.String;

			FListMenuItem *it = new FListMenuItemText(desc->mXpos, desc->mYpos, hotkey, text, desc->mFont, desc->mFontColor, action);
			desc->mItems.Push(it);
			desc->mYpos += desc->mLinespacing;
			if (desc->mSelectedItem == -1) desc->mSelectedItem = desc->mItems.Size()-1;

		}
		else if (sc.Compare("Font"))
		{
			sc.MustGetString();
			FFont *newfont = V_GetFont(sc.String);
			if (newfont != NULL) desc->mFont = newfont;
			if (sc.CheckString(","))
			{
				sc.MustGetString();
				desc->mFontColor = V_FindFontColor((FName)sc.String);
			}
			else
			{
				desc->mFontColor = CR_UNTRANSLATED;
			}
		}
		else if (sc.Compare("NetgameMessage"))
		{
			sc.MustGetString();
			desc->mNetgameMessage = sc.String;
		}
		else
		{
			sc.ScriptError("Unknown keyword '%s'", sc.String);
		}
	}
}

//=============================================================================
//
//
//
//=============================================================================

static void ParseListMenu(FScanner &sc)
{
	sc.MustGetString();

	FListMenuDescriptor *desc = new FListMenuDescriptor;
	desc->mType = MDESC_ListMenu;
	desc->mMenuName = sc.String;
	desc->mSelectedItem = -1;
	desc->mSelectOfsX = DefaultListMenuSettings.mSelectOfsX;
	desc->mSelectOfsY = DefaultListMenuSettings.mSelectOfsY;
	desc->mSelector = DefaultListMenuSettings.mSelector;
	desc->mDisplayTop = DefaultListMenuSettings.mDisplayTop;
	desc->mXpos = DefaultListMenuSettings.mXpos;
	desc->mYpos = DefaultListMenuSettings.mYpos;
	desc->mLinespacing = DefaultListMenuSettings.mLinespacing;
	desc->mNetgameMessage = DefaultListMenuSettings.mNetgameMessage;
	desc->mFont = DefaultListMenuSettings.mFont;
	desc->mFontColor = DefaultListMenuSettings.mFontColor;

	FMenuDescriptor **pOld = MenuDescriptors.CheckKey(desc->mMenuName);
	if (pOld != NULL && *pOld != NULL) delete *pOld;
	MenuDescriptors[desc->mMenuName] = desc;

	ParseListMenuBody(sc, desc);
}

//=============================================================================
//
//
//
//=============================================================================

void M_ParseMenuDefs()
{
	int lump, lastlump = 0;

	while ((lump = Wads.FindLump ("MENUDEF", &lastlump)) != -1)
	{
		FScanner sc(lump);

		sc.SetCMode(true);
		while (sc.GetString())
		{
			if (sc.Compare("LISTMENU"))
			{
				ParseListMenu(sc);
			}
			else if (sc.Compare("DEFAULTLISTMENU"))
			{
				ParseListMenuBody(sc, &DefaultListMenuSettings);
			}
			else
			{
				sc.ScriptError("Unknown keyword '%s'");
			}
		}
	}

	// Build episode menu
	FMenuDescriptor **desc = MenuDescriptors.CheckKey(NAME_Episodemenu);
	if (desc != NULL)
	{
		if ((*desc)->mType == MDESC_ListMenu)
		{
			FListMenuDescriptor *ld = static_cast<FListMenuDescriptor*>(*desc);
			int posy = ld->mYpos;
			ld->mSelectedItem = ld->mItems.Size();
			for(unsigned i = 0; i < AllEpisodes.Size(); i++)
			{
				FListMenuItem *it;
				if (AllEpisodes[i].mPicName.IsNotEmpty())
				{
					FTextureID tex = TexMan.CheckForTexture(AllEpisodes[i].mPicName, FTexture::TEX_MiscPatch);
					it = new FListMenuItemPatch(ld->mXpos, posy, AllEpisodes[i].mShortcut, 
						tex, NAME_Playerclassmenu, i);
				}
				else
				{
					it = new FListMenuItemText(ld->mXpos, posy, AllEpisodes[i].mShortcut, 
						AllEpisodes[i].mEpisodeName, ld->mFont, ld->mFontColor, NAME_Playerclassmenu, i);
				}
				ld->mItems.Push(it);
				posy += ld->mLinespacing;
			}
		}
	}
}
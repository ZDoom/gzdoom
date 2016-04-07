/*
** readthis.cpp
** Help screens
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
#include "g_level.h"
#include "gi.h"
#include "textures/textures.h"

class DReadThisMenu : public DMenu
{
	DECLARE_CLASS(DReadThisMenu, DMenu)
	int mScreen;
	int mInfoTic;

public:

	DReadThisMenu(DMenu *parent = NULL);
	void Drawer();
	bool MenuEvent(int mkey, bool fromcontroller);
	bool DimAllowed () { return false; }
	bool MouseEvent(int type, int x, int y);
};

IMPLEMENT_CLASS(DReadThisMenu)

//=============================================================================
//
// Read This Menus
//
//=============================================================================

DReadThisMenu::DReadThisMenu(DMenu *parent)
: DMenu(parent)
{
	mScreen = 1;
	mInfoTic = gametic;
}


//=============================================================================
//
//
//
//=============================================================================

void DReadThisMenu::Drawer()
{
	FTexture *tex = NULL, *prevpic = NULL;
	double alpha;

	// Did the mapper choose a custom help page via MAPINFO?
	if ((level.info != NULL) && level.info->F1Pic.Len() != 0)
	{
		tex = TexMan.FindTexture(level.info->F1Pic);
		mScreen = 1;
	}
	
	if (tex == NULL)
	{
		tex = TexMan[gameinfo.infoPages[mScreen-1].GetChars()];
	}

	if (mScreen > 1)
	{
		prevpic = TexMan[gameinfo.infoPages[mScreen-2].GetChars()];
	}

	screen->Dim(0, 1.0, 0,0, SCREENWIDTH, SCREENHEIGHT);
	alpha = MIN((gametic - mInfoTic) * (3. / TICRATE), 1.);
	if (alpha < 1. && prevpic != NULL)
	{
		screen->DrawTexture (prevpic, 0, 0, DTA_Fullscreen, true, TAG_DONE);
	}
	screen->DrawTexture (tex, 0, 0, DTA_Fullscreen, true, DTA_AlphaF, alpha,	TAG_DONE);

}


//=============================================================================
//
//
//
//=============================================================================

bool DReadThisMenu::MenuEvent(int mkey, bool fromcontroller)
{
	if (mkey == MKEY_Enter)
	{
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", snd_menuvolume, ATTN_NONE);
		mScreen++;
		mInfoTic = gametic;
		if ((level.info != NULL && level.info->F1Pic.Len() != 0) || mScreen > int(gameinfo.infoPages.Size()))
		{
			Close();
		}
		return true;
	}
	else return Super::MenuEvent(mkey, fromcontroller);
}

//=============================================================================
//
//
//
//=============================================================================

bool DReadThisMenu::MouseEvent(int type, int x, int y)
{
	if (type == MOUSE_Click)
	{
		return MenuEvent(MKEY_Enter, true);
	}
	return false;
}


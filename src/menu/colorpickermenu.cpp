/*
** colorpickermenu.cpp
** The color picker menu
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
#include <float.h>

#include "menu/menu.h"
#include "c_dispatch.h"
#include "w_wad.h"
#include "sc_man.h"
#include "v_font.h"
#include "g_level.h"
#include "d_player.h"
#include "v_video.h"
#include "gi.h"
#include "i_system.h"
#include "c_bind.h"
#include "v_palette.h"
#include "d_event.h"
#include "d_gui.h"

#define NO_IMP
#include "menu/optionmenuitems.h"

class DColorPickerMenu : public DOptionMenu
{
	DECLARE_CLASS(DColorPickerMenu, DOptionMenu)

public:

	float mRed;
	float mGreen;
	float mBlue;

	int mGridPosX;
	int mGridPosY;

	int mStartItem;

	FColorCVar *mCVar;

	DColorPickerMenu(DMenu *parent, const char *name, DOptionMenuDescriptor *desc, FColorCVar *cvar)
	{
		mStartItem = desc->mItems.Size();
		mRed = (float)RPART(DWORD(*cvar));
		mGreen = (float)GPART(DWORD(*cvar));
		mBlue = (float)BPART(DWORD(*cvar));
		mGridPosX = 0;
		mGridPosY = 0;
		mCVar = cvar;

		// This menu uses some featurs that are hard to implement in an external control lump
		// so it creates its own list of menu items.
		desc->mItems.Resize(mStartItem+8);
		desc->mItems[mStartItem+0] = CreateOptionMenuItemStaticText(name, false);
		desc->mItems[mStartItem+1] = CreateOptionMenuItemStaticText(" ", false);
		desc->mItems[mStartItem+2] = CreateOptionMenuSliderVar("Red", 0, 0, 255, 15, 0);
		desc->mItems[mStartItem+3] = CreateOptionMenuSliderVar("Green", 1, 0, 255, 15, 0);
		desc->mItems[mStartItem+4] = CreateOptionMenuSliderVar("Blue", 2, 0, 255, 15, 0);
		desc->mItems[mStartItem+5] = CreateOptionMenuItemStaticText(" ", false);
		desc->mItems[mStartItem+6] = CreateOptionMenuItemCommand("Undo changes", "undocolorpic");
		desc->mItems[mStartItem+7] = CreateOptionMenuItemStaticText(" ", false);
		for (auto &p : desc->mItems)
		{
			GC::WriteBarrier(p);
		}
		desc->mSelectedItem = mStartItem + 2;
		Init(parent, desc);
		desc->mIndent = 0;
		desc->CalcIndent();
	}

	void OnDestroy() override
	{
		if (mStartItem >= 0)
		{
			mDesc->mItems.Resize(mStartItem);
			UCVarValue val;
			val.Int = MAKERGB(int(mRed), int(mGreen), int(mBlue));
			if (mCVar != NULL) mCVar->SetGenericRep (val, CVAR_Int);
			mStartItem = -1;
		}
	}

	void Reset()
	{
		mRed = (float)RPART(DWORD(*mCVar));
		mGreen = (float)GPART(DWORD(*mCVar));
		mBlue = (float)BPART(DWORD(*mCVar));
	}

};

IMPLEMENT_CLASS(DColorPickerMenu, true, false)

CCMD(undocolorpic)
{
	if (DMenu::CurrentMenu != NULL && DMenu::CurrentMenu->IsKindOf(RUNTIME_CLASS(DColorPickerMenu)))
	{
		static_cast<DColorPickerMenu*>(DMenu::CurrentMenu)->Reset();
	}
}


DMenu *StartPickerMenu(DMenu *parent, const char *name, FColorCVar *cvar)
{
	DMenuDescriptor **desc = MenuDescriptors.CheckKey(NAME_Colorpickermenu);
	if (desc != NULL && (*desc)->IsKindOf(RUNTIME_CLASS(DOptionMenuDescriptor)))
	{
		return new DColorPickerMenu(parent, name, (DOptionMenuDescriptor*)(*desc), cvar);
	}
	else
	{
		return NULL;
	}
}


DEFINE_FIELD(DColorPickerMenu, mRed);
DEFINE_FIELD(DColorPickerMenu, mGreen);
DEFINE_FIELD(DColorPickerMenu, mBlue);
DEFINE_FIELD(DColorPickerMenu, mGridPosX);
DEFINE_FIELD(DColorPickerMenu, mGridPosY);
DEFINE_FIELD(DColorPickerMenu, mStartItem);
DEFINE_FIELD(DColorPickerMenu, mCVar);

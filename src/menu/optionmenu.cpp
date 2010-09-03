/*
** optionmenu.cpp
** Handler class for the option menus and associated items
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

#include "v_video.h"
#include "v_font.h"
#include "cmdlib.h"
#include "gstrings.h"
#include "g_level.h"
#include "gi.h"
#include "d_gui.h"
#include "d_event.h"
#include "menu/menu.h"

IMPLEMENT_CLASS(DOptionMenu)

//=============================================================================
//
//
//
//=============================================================================

DOptionMenu::DOptionMenu(DMenu *parent, FOptionMenuDescriptor *desc)
: DMenu(parent)
{
	mDesc = desc;
}

//=============================================================================
//
//
//
//=============================================================================

void DOptionMenu::Init(DMenu *parent, FOptionMenuDescriptor *desc)
{
	mParentMenu = parent;
	GC::WriteBarrier(this, parent);
	mDesc = desc;
}

//=============================================================================
//
//
//
//=============================================================================
/*
FListMenuItem *DOptionMenu::GetItem(FName name)
{
	for(unsigned i=0;i<mDesc->mItems.Size(); i++)
	{
		FName nm = mDesc->mItems[i]->GetAction(NULL);
		if (nm == name) return mDesc->mItems[i];
	}
	return NULL;
}
*/

//=============================================================================
//
//
//
//=============================================================================

bool DOptionMenu::Responder (event_t *ev)
{
	return false;
}

//=============================================================================
//
//
//
//=============================================================================

bool DOptionMenu::MenuEvent (int mkey, bool fromcontroller)
{
	int startedAt = mDesc->mSelectedItem;

	switch (mkey)
	{
	case MKEY_Up:
		do
		{
			if (--mDesc->mSelectedItem < 0) mDesc->mSelectedItem = mDesc->mItems.Size()-1;
		}
		while (!mDesc->mItems[mDesc->mSelectedItem]->Selectable() && mDesc->mSelectedItem != startedAt);
		return true;

	case MKEY_Down:
		do
		{
			if (++mDesc->mSelectedItem >= (int)mDesc->mItems.Size()) mDesc->mSelectedItem = 0;
		}
		while (!mDesc->mItems[mDesc->mSelectedItem]->Selectable() && mDesc->mSelectedItem != startedAt);
		return true;

	case MKEY_Enter:
		return mDesc->mItems[mDesc->mSelectedItem]->Activate();

	default:
		return Super::MenuEvent(mkey, fromcontroller);
	}
}

//=============================================================================
//
//
//
//=============================================================================

void DOptionMenu::Ticker ()
{
	for(unsigned i=0;i<mDesc->mItems.Size(); i++)
	{
		mDesc->mItems[i]->Ticker();
	}
}

//=============================================================================
//
//
//
//=============================================================================

void DOptionMenu::Drawer ()
{
	/*
	for(unsigned i=0;i<mDesc->mItems.Size(); i++)
	{
		if (mDesc->mItems[i]->mEnabled) mDesc->mItems[i]->Drawer();
	}
	if (mDesc->mSelectedItem >= 0 && mDesc->mSelectedItem < (int)mDesc->mItems.Size())
		mDesc->mItems[mDesc->mSelectedItem]->DrawSelector(mDesc->mSelectOfsX, mDesc->mSelectOfsY, mDesc->mSelector);
	*/
}


//=============================================================================
//
// base class for menu items
//
//=============================================================================

FOptionMenuItem::FOptionMenuItem(int xpos, int ypos, FName action)
{
}

bool FOptionMenuItem::CheckCoordinate(FOptionMenuDescriptor *desc, int x, int y)
{
	return false;
}

void FOptionMenuItem::Drawer(FOptionMenuDescriptor *desc)
{
}

void FOptionMenuItem::DrawSelector(int xofs, int yofs, FTextureID tex)
{
}

bool FOptionMenuItem::Selectable()
{
	return true;
}

//=============================================================================
//
//
//
//=============================================================================

FOptionMenuItemSubmenu::FOptionMenuItemSubmenu(const char *label, const char *menu)
{
}

void FOptionMenuItemSubmenu::Drawer(FOptionMenuDescriptor *desc)
{
}

bool FOptionMenuItemSubmenu::Activate()
{
	return false;
}

//=============================================================================
//
//
//
//=============================================================================

FOptionMenuItemCommand::FOptionMenuItemCommand(const char *label, const char *menu)
: FOptionMenuItemSubmenu(label, menu)
{
}

bool FOptionMenuItemCommand::Activate()
{
	return false;
}

//=============================================================================
//
//
//
//=============================================================================

FOptionMenuItemSafeCommand::FOptionMenuItemSafeCommand(const char *label, const char *menu)
: FOptionMenuItemCommand(label, menu)
{
}

bool FOptionMenuItemSafeCommand::Activate()
{
	return false;
}

//=============================================================================
//
//
//
//=============================================================================

FOptionMenuItemOption::FOptionMenuItemOption(const char *label, const char *menu, const char *values)
{
}

void FOptionMenuItemOption::Drawer(FOptionMenuDescriptor *desc)
{
}

bool FOptionMenuItemOption::Activate()
{
	return false;
}

//=============================================================================
//
//
//
//=============================================================================

FOptionMenuItemControl::FOptionMenuItemControl(const char *label, const char *menu, FKeyBindings *bindings)
{
}

void FOptionMenuItemControl::Drawer(FOptionMenuDescriptor *desc)
{
}

bool FOptionMenuItemControl::Activate()
{
	return false;
}

//=============================================================================
//
//
//
//=============================================================================

FOptionMenuItemStaticText::FOptionMenuItemStaticText(const char *label, EColorRange color)
{
}

void FOptionMenuItemStaticText::Drawer(FOptionMenuDescriptor *desc)
{
}

bool FOptionMenuItemStaticText::Activate()
{
	return false;
}

bool FOptionMenuItemStaticText::Selectable()
{
	return false;
}

//=============================================================================
//
//
//
//=============================================================================

FOptionMenuSliderItem::FOptionMenuSliderItem(const char *label, const char *menu, double min, double max, double step, bool showval)
{
}

void FOptionMenuSliderItem::Drawer(FOptionMenuDescriptor *desc)
{
}

bool FOptionMenuSliderItem::Activate()
{
	return false;
}

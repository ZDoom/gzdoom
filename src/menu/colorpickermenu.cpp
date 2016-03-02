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

	float mRed;
	float mGreen;
	float mBlue;

	int mGridPosX;
	int mGridPosY;

	int mStartItem;

	FColorCVar *mCVar;

public:

	DColorPickerMenu(DMenu *parent, const char *name, FOptionMenuDescriptor *desc, FColorCVar *cvar)
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
		desc->mItems[mStartItem+0] = new FOptionMenuItemStaticText(name, false);
		desc->mItems[mStartItem+1] = new FOptionMenuItemStaticText(" ", false);
		desc->mItems[mStartItem+2] = new FOptionMenuSliderVar("Red", &mRed, 0, 255, 15, 0);
		desc->mItems[mStartItem+3] = new FOptionMenuSliderVar("Green", &mGreen, 0, 255, 15, 0);
		desc->mItems[mStartItem+4] = new FOptionMenuSliderVar("Blue", &mBlue, 0, 255, 15, 0);
		desc->mItems[mStartItem+5] = new FOptionMenuItemStaticText(" ", false);
		desc->mItems[mStartItem+6] = new FOptionMenuItemCommand("Undo changes", "undocolorpic");
		desc->mItems[mStartItem+7] = new FOptionMenuItemStaticText(" ", false);
		desc->mSelectedItem = mStartItem + 2;
		Init(parent, desc);
		desc->mIndent = 0;
		desc->CalcIndent();
	}

	void Destroy()
	{
		if (mStartItem >= 0)
		{
			for(unsigned i=0;i<8;i++)
			{
				delete mDesc->mItems[mStartItem+i];
				mDesc->mItems.Resize(mStartItem);
			}
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

	//=============================================================================
	//
	//
	//
	//=============================================================================

	bool MenuEvent (int mkey, bool fromcontroller)
	{
		int &mSelectedItem = mDesc->mSelectedItem;

		switch (mkey)
		{
		case MKEY_Down:
			if (mSelectedItem == mStartItem+6)	// last valid item
			{
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", snd_menuvolume, ATTN_NONE);
				mGridPosY = 0;
				// let it point to the last static item so that the super class code still has a valid item
				mSelectedItem = mStartItem+7;	
				return true;
			}
			else if (mSelectedItem == mStartItem+7)
			{
				if (mGridPosY < 15)
				{
					S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", snd_menuvolume, ATTN_NONE);
					mGridPosY++;
				}
				return true;
			}
			break;

		case MKEY_Up:
			if (mSelectedItem == mStartItem+7)
			{
				if (mGridPosY > 0)
				{
					S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", snd_menuvolume, ATTN_NONE);
					mGridPosY--;
				}
				else
				{
					S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", snd_menuvolume, ATTN_NONE);
					mSelectedItem = mStartItem+6;
				}
				return true;
			}
			break;

		case MKEY_Left:
			if (mSelectedItem == mStartItem+7)
			{
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", snd_menuvolume, ATTN_NONE);
				if (--mGridPosX < 0) mGridPosX = 15;
				return true;
			}
			break;

		case MKEY_Right:
			if (mSelectedItem == mStartItem+7)
			{
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", snd_menuvolume, ATTN_NONE);
				if (++mGridPosX > 15) mGridPosX = 0;
				return true;
			}
			break;

		case MKEY_Enter:
			if (mSelectedItem == mStartItem+7)
			{
				// Choose selected palette entry
				int index = mGridPosX + mGridPosY * 16;
				mRed = GPalette.BaseColors[index].r;
				mGreen = GPalette.BaseColors[index].g;
				mBlue = GPalette.BaseColors[index].b;
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", snd_menuvolume, ATTN_NONE);
				return true;
			}
			break;
		}
		if (mSelectedItem >= 0 && mSelectedItem < mStartItem+7) 
		{
			if (mDesc->mItems[mDesc->mSelectedItem]->MenuEvent(mkey, fromcontroller)) return true;
		}
		return Super::MenuEvent(mkey, fromcontroller);
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	bool MouseEvent(int type, int mx, int my)
	{
		int olditem = mDesc->mSelectedItem;
		bool res = Super::MouseEvent(type, mx, my);

		if (mDesc->mSelectedItem == -1 || mDesc->mSelectedItem == mStartItem+7)
		{
			int y = (-mDesc->mPosition + BigFont->GetHeight() + mDesc->mItems.Size() * OptionSettings.mLinespacing) * CleanYfac_1;
			int h = (screen->GetHeight() - y) / 16;
			int fh = OptionSettings.mLinespacing * CleanYfac_1;
			int w = fh;
			int yy = y + 2 * CleanYfac_1;
			int indent = (screen->GetWidth() / 2);

			if (h > fh) h = fh;
			else if (h < 4) return res;	// no space to draw it.

			int box_y = y - 2 * CleanYfac_1;
			int box_x = indent - 16*w;

			if (mx >= box_x && mx < box_x + 16*w && my >= box_y && my < box_y + 16*h)
			{
				int cell_x = (mx - box_x) / w;
				int cell_y = (my - box_y) / h;

				if (olditem != mStartItem+7 || cell_x != mGridPosX || cell_y != mGridPosY)
				{
					mGridPosX = cell_x;
					mGridPosY = cell_y;
					//S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", snd_menuvolume, ATTN_NONE);
				}
				mDesc->mSelectedItem = mStartItem+7;
				if (type == MOUSE_Release)
				{
					MenuEvent(MKEY_Enter, true);
					if (m_use_mouse == 2) mDesc->mSelectedItem = -1;
				}
				res = true;
			}
		}
		return res;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	void Drawer()
	{
		Super::Drawer();

		if (mCVar == NULL) return;
		int y = (-mDesc->mPosition + BigFont->GetHeight() + mDesc->mItems.Size() * OptionSettings.mLinespacing) * CleanYfac_1;
		int h = (screen->GetHeight() - y) / 16;
		int fh = OptionSettings.mLinespacing * CleanYfac_1;
		int w = fh;
		int yy = y;

		if (h > fh) h = fh;
		else if (h < 4) return;	// no space to draw it.
		
		int indent = (screen->GetWidth() / 2);
		int p = 0;

		for(int i = 0; i < 16; i++, y += h)
		{
			int box_x, box_y;
			int x1;

			box_y = y - 2 * CleanYfac_1;
			box_x = indent - 16*w;
			for (x1 = 0; x1 < 16; ++x1, p++)
			{
				screen->Clear (box_x, box_y, box_x + w, box_y + h, p, 0);
				if ((mDesc->mSelectedItem == mStartItem+7) && 
					(/*p == CurrColorIndex ||*/ (i == mGridPosY && x1 == mGridPosX)))
				{
					int r, g, b;
					DWORD col;
					double blinky;
					if (i == mGridPosY && x1 == mGridPosX)
					{
						r = 255, g = 128, b = 0;
					}
					else
					{
						r = 200, g = 200, b = 255;
					}
					// Make sure the cursors stand out against similar colors
					// by pulsing them.
					blinky = fabs(sin(I_MSTime()/1000.0)) * 0.5 + 0.5;
					col = MAKEARGB(255,int(r*blinky),int(g*blinky),int(b*blinky));

					screen->Clear (box_x, box_y, box_x + w, box_y + 1, -1, col);
					screen->Clear (box_x, box_y + h-1, box_x + w, box_y + h, -1, col);
					screen->Clear (box_x, box_y, box_x + 1, box_y + h, -1, col);
					screen->Clear (box_x + w - 1, box_y, box_x + w, box_y + h, -1, col);
				}
				box_x += w;
			}
		}
		y = yy;
		DWORD newColor = MAKEARGB(255, int(mRed), int(mGreen), int(mBlue));
		DWORD oldColor = DWORD(*mCVar) | 0xFF000000;

		int x = screen->GetWidth()*2/3;

		screen->Clear (x, y, x + 48*CleanXfac_1, y + 48*CleanYfac_1, -1, oldColor);
		screen->Clear (x + 48*CleanXfac_1, y, x + 48*2*CleanXfac_1, y + 48*CleanYfac_1, -1, newColor);

		y += 49*CleanYfac_1;
		screen->DrawText (SmallFont, CR_GRAY, x+(24-SmallFont->StringWidth("Old")/2)*CleanXfac_1, y,
			"Old", DTA_CleanNoMove_1, true, TAG_DONE);
		screen->DrawText (SmallFont, CR_WHITE, x+(48+24-SmallFont->StringWidth("New")/2)*CleanXfac_1, y,
			"New", DTA_CleanNoMove_1, true, TAG_DONE);
	}
};

IMPLEMENT_ABSTRACT_CLASS(DColorPickerMenu)

CCMD(undocolorpic)
{
	if (DMenu::CurrentMenu != NULL && DMenu::CurrentMenu->IsKindOf(RUNTIME_CLASS(DColorPickerMenu)))
	{
		static_cast<DColorPickerMenu*>(DMenu::CurrentMenu)->Reset();
	}
}


DMenu *StartPickerMenu(DMenu *parent, const char *name, FColorCVar *cvar)
{
	FMenuDescriptor **desc = MenuDescriptors.CheckKey(NAME_Colorpickermenu);
	if (desc != NULL && (*desc)->mType == MDESC_OptionsMenu)
	{
		return new DColorPickerMenu(parent, name, (FOptionMenuDescriptor*)(*desc), cvar);
	}
	else
	{
		return NULL;
	}
}


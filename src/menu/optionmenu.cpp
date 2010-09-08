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
#include "v_palette.h"
#include "d_gui.h"
#include "d_event.h"
#include "c_dispatch.h"
#include "c_console.h"
#include "c_cvars.h"
#include "c_bind.h"
#include "gameconfigfile.h"
#include "menu/menu.h"


//=============================================================================
//
// Draws a string in the console font, scaled to the 8x8 cells
// used by the default console font.
//
//=============================================================================

void M_DrawConText (int color, int x, int y, const char *str)
{
	int len = (int)strlen(str);

	screen->DrawText (ConFont, color, x, y, str,
		DTA_CellX, 8 * CleanXfac_1,
		DTA_CellY, 8 * CleanYfac_1,
		TAG_DONE);
}

//=============================================================================
//
// Draw a slider. Set fracdigits negative to not display the current value numerically.
//
//=============================================================================

void M_DrawSlider (int x, int y, double min, double max, double cur,int fracdigits)
{
	double range;

	range = max - min;
	double ccur = clamp(cur, min, max) - min;

	M_DrawConText(CR_WHITE, x, y, "\x10\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x12");
	M_DrawConText(CR_ORANGE, x + int((5 + ((ccur * 78) / range)) * CleanXfac_1), y, "\x13");

	if (fracdigits >= 0)
	{
		char textbuf[16];
		mysnprintf(textbuf, countof(textbuf), "%.*f", fracdigits, cur);
		screen->DrawText(SmallFont, CR_DARKGRAY, x + (12*8 + 4) * CleanXfac_1, y, textbuf, DTA_CleanNoMove_1, true, TAG_DONE);
	}
}



IMPLEMENT_CLASS(DOptionMenu)

//=============================================================================
//
//
//
//=============================================================================

DOptionMenu::DOptionMenu(DMenu *parent, FOptionMenuDescriptor *desc)
: DMenu(parent)
{
	CanScrollUp = false;
	CanScrollDown = false;
	VisBottom = 0;
	Init(parent, desc);
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
	if (mDesc != NULL && mDesc->mSelectedItem < 0)
	{
		// Go down to the first selectable item
		mDesc->mSelectedItem = -1;
		MenuEvent(MKEY_Down, false);
	}
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
	return Super::Responder(ev);
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
			--mDesc->mSelectedItem;

			if (mDesc->mScrollPos > 0 &&
				mDesc->mSelectedItem == mDesc->mScrollTop + mDesc->mScrollPos)
			{
				mDesc->mScrollPos--;
			}

			if (mDesc->mSelectedItem < 0) 
			{
				// Figure out how many lines of text fit on the menu
				int y = mDesc->mPosition;

				if (y <= 0)
				{
					if (BigFont && mDesc->mTitle.IsNotEmpty())
					{
						y = -y + BigFont->GetHeight();
					}
					else
					{
						y = -y;
					}
				}
				y *= CleanYfac_1;
				int	rowheight = OptionSettings.mLinespacing * CleanYfac_1;
				int maxitems = (screen->GetHeight() - rowheight - y) / rowheight + 1;

				mDesc->mScrollPos = MAX (0, (int)mDesc->mItems.Size() - maxitems + mDesc->mScrollTop);
				mDesc->mSelectedItem = mDesc->mItems.Size()-1;
			}
		}
		while (!mDesc->mItems[mDesc->mSelectedItem]->Selectable() && mDesc->mSelectedItem != startedAt);
		break;

	case MKEY_Down:
		do
		{
			++mDesc->mSelectedItem;
			
			if (CanScrollDown && mDesc->mSelectedItem == VisBottom)
			{
				mDesc->mScrollPos++;
				VisBottom++;
			}
			if (mDesc->mSelectedItem >= (int)mDesc->mItems.Size()) 
			{
				mDesc->mSelectedItem = 0;
				mDesc->mScrollPos = 0;
			}
		}
		while (!mDesc->mItems[mDesc->mSelectedItem]->Selectable() && mDesc->mSelectedItem != startedAt);
		break;

	case MKEY_PageUp:
		if (mDesc->mScrollPos > 0)
		{
			mDesc->mScrollPos -= VisBottom - mDesc->mScrollPos - mDesc->mScrollTop;
			if (mDesc->mScrollPos < 0)
			{
				mDesc->mScrollPos = 0;
			}
			mDesc->mSelectedItem = mDesc->mScrollTop + mDesc->mScrollPos + 1;
			while (!mDesc->mItems[mDesc->mSelectedItem]->Selectable())
			{
				++mDesc->mSelectedItem;
			}
		}
		break;

	case MKEY_PageDown:
		if (CanScrollDown)
		{
			int pagesize = VisBottom - mDesc->mScrollPos - mDesc->mScrollTop;
			mDesc->mScrollPos += pagesize;
			if (mDesc->mScrollPos + mDesc->mScrollTop + pagesize > (int)mDesc->mItems.Size())
			{
				mDesc->mScrollPos = mDesc->mItems.Size() - mDesc->mScrollTop - pagesize;
			}
			mDesc->mSelectedItem = mDesc->mScrollTop + mDesc->mScrollPos;
			while (!mDesc->mItems[mDesc->mSelectedItem]->Selectable())
			{
				++mDesc->mSelectedItem;
			}
		}
		break;

	case MKEY_Enter:
		if (mDesc->mItems[mDesc->mSelectedItem]->Activate()) 
		{
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", snd_menuvolume, ATTN_NONE);
			return true;
		}
		// fall through to default
	default:
		if (mDesc->mItems[mDesc->mSelectedItem]->MenuEvent(mkey, fromcontroller)) return true;
		return Super::MenuEvent(mkey, fromcontroller);
	}

	if (mDesc->mSelectedItem != startedAt)
	{
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", snd_menuvolume, ATTN_NONE);
	}
	return true;
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
	int y = mDesc->mPosition;

	if (y <= 0)
	{
		if (BigFont && mDesc->mTitle.IsNotEmpty())
		{
			screen->DrawText (BigFont, OptionSettings.mTitleColor,
				(screen->GetWidth() - BigFont->StringWidth(mDesc->mTitle) * CleanXfac_1) / 2, 10*CleanYfac_1,
				mDesc->mTitle, DTA_CleanNoMove_1, true, TAG_DONE);
			y = -y + BigFont->GetHeight();
		}
		else
		{
			y = -y;
		}
	}
	//int labelofs = OptionSettings.mLabelOffset * CleanXfac_1;
	//int cursorspace = 14 * CleanXfac_1;
	int fontheight = OptionSettings.mLinespacing * CleanYfac_1;
	y *= CleanYfac_1;

	int indent = mDesc->mIndent;
	if (indent > 280)
	{ // kludge for the compatibility options with their extremely long labels
		if (indent + 40 <= CleanWidth_1)
		{
			indent = (screen->GetWidth() - ((indent + 40) * CleanXfac_1)) / 2 + indent * CleanXfac_1;
		}
		else
		{
			indent = screen->GetWidth() - 40 * CleanXfac_1;
		}
	}
	else
	{
		indent = (indent - 160) * CleanXfac_1 + screen->GetWidth() / 2;
	}

	int ytop = y + mDesc->mScrollTop * 8 * CleanYfac_1;
	int lastrow = screen->GetHeight() - SmallFont->GetHeight() * CleanYfac_1;

	unsigned i;
	for (i = 0; i < mDesc->mItems.Size() && y <= lastrow; i++, y += fontheight)
	{
		// Don't scroll the uppermost items
		if (i == mDesc->mScrollTop)
		{
			i += mDesc->mScrollPos;
			if (i >= mDesc->mItems.Size()) break;	// skipped beyond end of menu 
		}
		int cur_indent = mDesc->mItems[i]->Draw(mDesc, y, indent);
		if (cur_indent >= 0 && mDesc->mSelectedItem == i && mDesc->mItems[i]->Selectable())
		{
			int color = ((DMenu::MenuTime%8) < 4) || DMenu::CurrentMenu != this ? CR_RED:CR_GREY;
			M_DrawConText(color, cur_indent + 3 * CleanXfac_1, y-CleanYfac_1+OptionSettings.mLabelOffset, "\xd");
		}
	}

	CanScrollUp = (mDesc->mScrollPos > 0);
	CanScrollDown = (i < mDesc->mItems.Size());
	VisBottom = i - 1;

	if (CanScrollUp)
	{
		M_DrawConText(CR_ORANGE, 3 * CleanXfac_1, ytop + OptionSettings.mLabelOffset, "\x1a");
	}
	if (CanScrollDown)
	{
		M_DrawConText(CR_ORANGE, 3 * CleanXfac_1, y - 8*CleanYfac_1 + OptionSettings.mLabelOffset, "\x1b");
	}
}


//=============================================================================
//
// base class for menu items
//
//=============================================================================

FOptionMenuItem::~FOptionMenuItem()
{
	if (mLabel != NULL) delete [] mLabel;
}

bool FOptionMenuItem::CheckCoordinate(FOptionMenuDescriptor *desc, int x, int y)
{
	return false;
}

int FOptionMenuItem::Draw(FOptionMenuDescriptor *desc, int y, int indent)
{
	return indent;
}

bool FOptionMenuItem::Selectable()
{
	return true;
}

int  FOptionMenuItem::GetIndent()
{
	return mCentered? 0 : SmallFont->StringWidth(mLabel);
}

void FOptionMenuItem::drawLabel(int indent, int y, EColorRange color, bool grayed)
{
	int overlay = grayed? MAKEARGB(96,48,0,0) : 0;

	int x;
	int w = SmallFont->StringWidth(mLabel) * CleanXfac_1;
	if (!mCentered) x = indent - w;
	else x = (screen->GetWidth() - w) / 2;
	screen->DrawText (SmallFont, color, x, y, mLabel, DTA_CleanNoMove_1, true, DTA_ColorOverlay, overlay, TAG_DONE);
}



void FOptionMenuDescriptor::CalcIndent()
{
	// calculate the menu indent
	int widest = 0, thiswidth;

	for (unsigned i = 0; i < mItems.Size(); i++)
	{
		thiswidth = mItems[i]->GetIndent();
		if (thiswidth > widest) widest = thiswidth;
	}
	mIndent =  widest + 4;
}

//=============================================================================
//
//
//
//=============================================================================

FOptionMenuItem *FOptionMenuDescriptor::GetItem(FName name)
{
	for(unsigned i=0;i<mItems.Size(); i++)
	{
		FName nm = mItems[i]->GetAction(NULL);
		if (nm == name) return mItems[i];
	}
	return NULL;
}




class DGameplayMenu : public DOptionMenu
{
	DECLARE_CLASS(DGameplayMenu, DOptionMenu)

public:
	DGameplayMenu()
	{}

	void Drawer ()
	{
		Super::Drawer();

		char text[64];
		mysnprintf(text, 64, "dmflags = %d   dmflags2 = %d", *dmflags, *dmflags2);
		screen->DrawText (SmallFont, OptionSettings.mFontColorValue,
			(screen->GetWidth() - SmallFont->StringWidth (text) * CleanXfac_1) / 2, 0, text,
			DTA_CleanNoMove_1, true, TAG_DONE);
	}
};

IMPLEMENT_CLASS(DGameplayMenu)

class DCompatibilityMenu : public DOptionMenu
{
	DECLARE_CLASS(DCompatibilityMenu, DOptionMenu)

public:
	DCompatibilityMenu()
	{}

	void Drawer ()
	{
		Super::Drawer();

		char text[64];
		mysnprintf(text, 64, "compatflags = %d", *compatflags);
		screen->DrawText (SmallFont, OptionSettings.mFontColorValue,
			(screen->GetWidth() - SmallFont->StringWidth (text) * CleanXfac_1) / 2, 0, text,
			DTA_CleanNoMove_1, true, TAG_DONE);
	}
};

IMPLEMENT_CLASS(DCompatibilityMenu)

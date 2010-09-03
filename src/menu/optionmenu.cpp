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

#include <float.h>

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
#include "c_cvars.h"
#include "menu/menu.h"


#define CURSORSPACE (14 * CleanXfac_1)

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

static void M_DrawSlider (int x, int y, double min, double max, double cur,int fracdigits)
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
		mDesc->mItems[i]->Draw(mDesc, y, indent);
		if (mDesc->mSelectedItem == i)
		{
			int color = (DMenu::MenuTime%8) < 4? CR_RED:CR_GREY;
			M_DrawConText(color, indent + 3 * CleanXfac_1, y-CleanYfac_1+OptionSettings.mLabelOffset, "\xd");
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

FOptionMenuItem::FOptionMenuItem(const char *label, FName action, bool center)
: FListMenuItem(0, 0, action)
{
	mLabel = copystring(label);
	mCentered = center;
}

FOptionMenuItem::~FOptionMenuItem()
{
	if (mLabel != NULL) delete [] mLabel;
}

bool FOptionMenuItem::CheckCoordinate(FOptionMenuDescriptor *desc, int x, int y)
{
	return false;
}

void FOptionMenuItem::Draw(FOptionMenuDescriptor *desc, int y, int indent)
{
}

void FOptionMenuItem::DrawSelector(int xofs, int yofs, FTextureID tex)
{
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

//=============================================================================
//
//
//
//=============================================================================

FOptionMenuItemSubmenu::FOptionMenuItemSubmenu(const char *label, const char *menu)
: FOptionMenuItem(label, menu)
{
}

void FOptionMenuItemSubmenu::Draw(FOptionMenuDescriptor *desc, int y, int indent)
{
	drawLabel(indent, y, OptionSettings.mFontColorMore);
}

bool FOptionMenuItemSubmenu::Activate()
{
	M_SetMenu(mAction, 0);
	return true;
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
	C_DoCommand(mAction);
	return true;
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

bool FOptionMenuItemSafeCommand::MenuEvent (int mkey, bool fromcontroller)
{
	if (mkey == MKEY_MBYes)
	{
		C_DoCommand(mAction);
		return true;
	}
	return FOptionMenuItemCommand::MenuEvent(mkey, fromcontroller);
}

bool FOptionMenuItemSafeCommand::Activate()
{
	M_StartMessage("Do you really want to do this?", 0);
	return true;
}

//=============================================================================
//
//
//
//=============================================================================

FOptionMenuItemOption::FOptionMenuItemOption(const char *label, const char *menu, const char *values, const char *graycheck)
: FOptionMenuItem(label, menu)
{
	FOptionValues **opt = OptionValues.CheckKey(values);
	if (opt != NULL) 
	{
		mValues = *opt;
		mSelection = -1;
		mCVar = FindCVar(menu, NULL);
		mGrayCheck = (FBoolCVar*)FindCVar(graycheck, NULL);
		if (mGrayCheck != NULL && mGrayCheck->GetRealType() != CVAR_Bool) mGrayCheck = NULL;
		if (mCVar != NULL)
		{
			UCVarValue cv = mCVar->GetGenericRep(CVAR_Float);
			for(unsigned i=0;i<mValues->mValues.Size(); i++)
			{
				if (fabs(cv.Float - mValues->mValues[i].Value) < FLT_EPSILON)
				{
					mSelection = i;
					break;
				}
			}
		}
	}
	else
	{
		mValues = NULL;
	}
}

void FOptionMenuItemOption::Draw(FOptionMenuDescriptor *desc, int y, int indent)
{
	bool grayed = mGrayCheck != NULL && !(**mGrayCheck);
	drawLabel(indent, y, OptionSettings.mFontColor, grayed);

	if (mValues != NULL)
	{
		int overlay = grayed? MAKEARGB(96,48,0,0) : 0;
		const char *text;
		if (mSelection < 0)
		{
			text = "Unknown";
		}
		else
		{
			text = mValues->mValues[mSelection].Text;
		}
		screen->DrawText (SmallFont, OptionSettings.mFontColorValue, indent + CURSORSPACE, y, 
			text, DTA_CleanNoMove_1, true, DTA_ColorOverlay, overlay, TAG_DONE);
	}
}

bool FOptionMenuItemOption::MenuEvent (int mkey, bool fromcontroller)
{
	if (mValues != NULL)
	{
		if (mkey == MKEY_Left)
		{
			if (mSelection == -1) mSelection = 0;
			else if (--mSelection < 0) mSelection = mValues->mValues.Size()-1;
			return true;
		}
		else if (mkey == MKEY_Right || mkey == MKEY_Enter)
		{
			if (++mSelection >= (int)mValues->mValues.Size()) mSelection = 0;
			return true;
		}

	}
	return FOptionMenuItem::MenuEvent(mkey, fromcontroller);
}

bool FOptionMenuItemOption::Selectable()
{
	return !(mGrayCheck != NULL && !(**mGrayCheck));
}

bool FOptionMenuItemOption::Activate()
{
	return false;
}

/* color option

				}
				else
				{
					screen->DrawText (SmallFont, item->type == cdiscrete ? v : ValueColor,
						indent + cursorspace, y,
						item->type != discretes ? item->e.values[v].name : item->e.valuestrings[v].name.GetChars(),
						DTA_CleanNoMove_1, true, DTA_ColorOverlay, overlay, TAG_DONE);
				}

		// print value
*/
//=============================================================================
//
//
//
//=============================================================================

FOptionMenuItemControl::FOptionMenuItemControl(const char *label, const char *menu, FKeyBindings *bindings)
: FOptionMenuItem(label, menu)
{
}

void FOptionMenuItemControl::Draw(FOptionMenuDescriptor *desc, int y, int indent)
{
	drawLabel(indent, y, OptionSettings.mFontColor);
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

FOptionMenuItemStaticText::FOptionMenuItemStaticText(const char *label, bool header)
: FOptionMenuItem(label, NAME_None, true)
{
	mColor = header? OptionSettings.mFontColorHeader : OptionSettings.mFontColor;
}

void FOptionMenuItemStaticText::Draw(FOptionMenuDescriptor *desc, int y, int indent)
{
	drawLabel(indent, y, mColor);
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
: FOptionMenuItem(label, NAME_None)
{
}

void FOptionMenuSliderItem::Draw(FOptionMenuDescriptor *desc, int y, int indent)
{
	drawLabel(indent, y, OptionSettings.mFontColor);
}

bool FOptionMenuSliderItem::Activate()
{
	return false;
}

/*
		if (item->type != screenres &&
			item->type != joymore && (item->type != discrete || item->c.discretecenter != 1))
*/


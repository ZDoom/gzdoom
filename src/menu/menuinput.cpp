/*
** menuinput.cpp
** The string input code
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
#include "c_cvars.h"
#include "d_event.h"
#include "d_gui.h"
#include "v_font.h"
#include "v_palette.h"
#include "cmdlib.h"
// [TP] New #includes
#include "v_text.h"

IMPLEMENT_CLASS(DTextEnterMenu, true, false)

#define INPUTGRID_WIDTH		13
#define INPUTGRID_HEIGHT	5

// Heretic and Hexen do not, by default, come with glyphs for all of these
// characters. Oh well. Doom and Strife do.
static const char InputGridChars[INPUTGRID_WIDTH * INPUTGRID_HEIGHT] =
	"ABCDEFGHIJKLM"
	"NOPQRSTUVWXYZ"
	"0123456789+-="
	".,!?@'\":;[]()"
	"<>^#$%&*/_ \b";


CVAR(Bool, m_showinputgrid, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

//=============================================================================
//
//
//
//=============================================================================

// [TP] Added allowcolors
DTextEnterMenu::DTextEnterMenu(DMenu *parent, const char *textbuffer, int maxlen, int sizemode, bool showgrid, bool allowcolors)
: DMenu(parent)
{
	mEnterString = textbuffer;
	mEnterSize = maxlen < 0 ? UINT_MAX : unsigned(maxlen);
	mSizeMode = sizemode;
	mInputGridOkay = showgrid || m_showinputgrid;
	if (mEnterString.IsNotEmpty())
	{
		InputGridX = INPUTGRID_WIDTH - 1;
		InputGridY = INPUTGRID_HEIGHT - 1;
	}
	else
	{
		// If we are naming a new save, don't start the cursor on "end".
		InputGridX = 0;
		InputGridY = 0;
	}
	AllowColors = allowcolors; // [TP]
}

//=============================================================================
//
//
//
//=============================================================================

bool DTextEnterMenu::Responder(event_t *ev)
{
	if (ev->type == EV_GUI_Event)
	{
		// Save game and player name string input
		if (ev->subtype == EV_GUI_Char)
		{
			mInputGridOkay = false;
			if (mEnterString.Len() < mEnterSize &&
				(mSizeMode == 2/*entering player name*/ || (size_t)SmallFont->StringWidth(mEnterString) < (mEnterSize-1)*8))
			{
				mEnterString.AppendFormat("%c", (char)ev->data1);
			}
			return true;
		}
		char ch = (char)ev->data1;
		if ((ev->subtype == EV_GUI_KeyDown || ev->subtype == EV_GUI_KeyRepeat) && ch == '\b')
		{
			if (mEnterString.IsNotEmpty())
			{
				mEnterString.Truncate(mEnterString.Len() - 1);
			}
		}
		else if (ev->subtype == EV_GUI_KeyDown)
		{
			if (ch == GK_ESCAPE)
			{
				DMenu *parent = mParentMenu;
				Close();
				parent->CallMenuEvent(MKEY_Abort, false);
				return true;
			}
			else if (ch == '\r')
			{
				if (mEnterString.IsNotEmpty())
				{
					// [TP] If we allow color codes, colorize the string now.
					if (AllowColors)
						mEnterString = strbin1(mEnterString);

					DMenu *parent = mParentMenu;
					parent->CallMenuEvent(MKEY_Input, false);
					Close();
					return true;
				}
			}
		}
		if (ev->subtype == EV_GUI_KeyDown || ev->subtype == EV_GUI_KeyRepeat)
		{
			return true;
		}
	}
	return Super::Responder(ev);
}

//=============================================================================
//
//
//
//=============================================================================

bool DTextEnterMenu::MouseEvent(int type, int x, int y)
{
	const int cell_width = 18 * CleanXfac;
	const int cell_height = 12 * CleanYfac;
	const int screen_y = screen->GetHeight() - INPUTGRID_HEIGHT * cell_height;
	const int screen_x = (screen->GetWidth() - INPUTGRID_WIDTH * cell_width) / 2;

	if (x >= screen_x && x < screen_x + INPUTGRID_WIDTH * cell_width && y >= screen_y)
	{
		InputGridX = (x - screen_x) / cell_width;
		InputGridY = (y - screen_y) / cell_height;
		if (type == DMenu::MOUSE_Release)
		{
			if (CallMenuEvent(MKEY_Enter, true))
			{
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", snd_menuvolume, ATTN_NONE);
				if (m_use_mouse == 2) InputGridX = InputGridY = -1;
				return true;
			}
		}
	}
	else
	{
		InputGridX = InputGridY = -1;
	}
	return Super::MouseEvent(type, x, y);
}


		
//=============================================================================
//
//
//
//=============================================================================

bool DTextEnterMenu::MenuEvent (int key, bool fromcontroller)
{
	if (key == MKEY_Back)
	{
		mParentMenu->CallMenuEvent(MKEY_Abort, false);
		return Super::MenuEvent(key, fromcontroller);
	}
	if (fromcontroller)
	{
		mInputGridOkay = true;
	}

	if (mInputGridOkay)
	{
		int ch;

		if (InputGridX == -1 || InputGridY == -1)
		{
			InputGridX = InputGridY = 0;
		}
		switch (key)
		{
		case MKEY_Down:
			InputGridY = (InputGridY + 1) % INPUTGRID_HEIGHT;
			return true;

		case MKEY_Up:
			InputGridY = (InputGridY + INPUTGRID_HEIGHT - 1) % INPUTGRID_HEIGHT;
			return true;

		case MKEY_Right:
			InputGridX = (InputGridX + 1) % INPUTGRID_WIDTH;
			return true;

		case MKEY_Left:
			InputGridX = (InputGridX + INPUTGRID_WIDTH - 1) % INPUTGRID_WIDTH;
			return true;

		case MKEY_Clear:
			if (mEnterString.IsNotEmpty())
			{
				mEnterString.Truncate(mEnterString.Len() - 1);
			}
			return true;

		case MKEY_Enter:
			assert(unsigned(InputGridX) < INPUTGRID_WIDTH && unsigned(InputGridY) < INPUTGRID_HEIGHT);
			if (mInputGridOkay)
			{
				ch = InputGridChars[InputGridX + InputGridY * INPUTGRID_WIDTH];
				if (ch == 0)			// end
				{
					if (mEnterString.IsNotEmpty())
					{
						DMenu *parent = mParentMenu;
						Close();
						parent->CallMenuEvent(MKEY_Input, false);
						return true;
					}
				}
				else if (ch == '\b')	// bs
				{
					if (mEnterString.IsNotEmpty())
					{
						mEnterString.Truncate(mEnterString.Len() - 1);
					}
				}
				else if (mEnterString.Len() < mEnterSize &&
					(mSizeMode == 2/*entering player name*/ || (size_t)SmallFont->StringWidth(mEnterString) < (mEnterSize-1)*8))
				{
					mEnterString += char(ch);
				}
			}
			return true;

		default:
			break;	// Keep GCC quiet
		}
	}
	return false;
}

DEFINE_ACTION_FUNCTION(DTextEnterMenu, Open)
{
	PARAM_PROLOGUE;
	PARAM_OBJECT(parent, DMenu);
	PARAM_STRING(text);
	PARAM_INT(maxlen);
	PARAM_INT(sizemode);
	PARAM_BOOL(fromcontroller);
	auto m = new DTextEnterMenu(parent, text.GetChars(), maxlen, sizemode, fromcontroller, false);
	M_ActivateMenu(m);
	ACTION_RETURN_OBJECT(m);
}

DEFINE_FIELD(DTextEnterMenu, mEnterString);
DEFINE_FIELD(DTextEnterMenu, mEnterSize);
DEFINE_FIELD(DTextEnterMenu, mEnterPos);
DEFINE_FIELD(DTextEnterMenu, mSizeMode); // 1: size is length in chars. 2: also check string width
DEFINE_FIELD(DTextEnterMenu, mInputGridOkay);
DEFINE_FIELD(DTextEnterMenu, InputGridX);
DEFINE_FIELD(DTextEnterMenu, InputGridY);
DEFINE_FIELD(DTextEnterMenu, AllowColors);

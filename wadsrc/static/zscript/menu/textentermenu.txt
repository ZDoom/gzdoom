/*
** menuinput.cpp
** The string input code
**
**---------------------------------------------------------------------------
** Copyright 2001-2010 Randy Heit
** Copyright 2010-2017 Christoph Oelckers
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


class TextEnterMenu : Menu
{
	const INPUTGRID_WIDTH = 13;
	const INPUTGRID_HEIGHT = 5;
	
	const Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-=.,!?@'\":;[]()<>^#$%&*/_ \b";

	String mEnterString;
	int mEnterSize;
	int mEnterPos;
	int mSizeMode; // 1: size is length in chars. 2: also check string width
	bool mInputGridOkay;
	int InputGridX;
	int InputGridY;
	bool AllowColors;
	
	//=============================================================================
	//
	//
	//
	//=============================================================================

	// [TP] Added allowcolors
	private void Init(Menu parent, String textbuffer, int maxlen, int sizemode, bool showgrid, bool allowcolors)
	{
		Super.init(parent);
		mEnterString = textbuffer;
		mEnterSize = maxlen < 0 ? 0x7fffffff : maxlen;
		mSizeMode = sizemode;
		mInputGridOkay = (showgrid && (m_showinputgrid == 0)) || (m_showinputgrid >= 1);
		if (mEnterString.Length() > 0)
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

	static TextEnterMenu Open(Menu parent, String textbuffer, int maxlen, int sizemode, bool showgrid = false, bool allowcolors = false)
	{
		let me = new("TextEnterMenu");
		me.Init(parent, textbuffer, maxlen, sizemode, showgrid, allowcolors);
		return me;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	String GetText()
	{
		return mEnterString;
	}
	
	override bool TranslateKeyboardEvents()
	{
		return mInputGridOkay; 
	}
	
	
	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool OnUIEvent(UIEvent ev)
	{
		// Save game and player name string input
		if (ev.Type == UIEvent.Type_Char)
		{
			mInputGridOkay = false;
			if (mEnterString.Length() < mEnterSize &&
				(mSizeMode == 2/*entering player name*/ || SmallFont.StringWidth(mEnterString) < (mEnterSize-1)*8))
			{
				mEnterString.AppendFormat("%c", ev.KeyChar);
			}
			return true;
		}
		int ch = ev.KeyChar;
		if ((ev.Type == UIEvent.Type_KeyDown || ev.Type == UIEvent.Type_KeyRepeat) && ch == 8)
		{
			if (mEnterString.Length() > 0)
			{
				mEnterString.Truncate(mEnterString.Length() - 1);
			}
		}
		else if (ev.Type == UIEvent.Type_KeyDown)
		{
			if (ch == UIEvent.Key_ESCAPE)
			{
				Menu parent = mParentMenu;
				Close();
				parent.MenuEvent(MKEY_Abort, false);
				return true;
			}
			else if (ch == 13)
			{
				if (mEnterString.Length() > 0)
				{
					// [TP] If we allow color codes, colorize the string now.
					if (AllowColors)
						mEnterString = mEnterString.Filter();

					Menu parent = mParentMenu;
					parent.MenuEvent(MKEY_Input, false);
					Close();
					return true;
				}
			}
		}
		if (ev.Type == UIEvent.Type_KeyDown || ev.Type == UIEvent.Type_KeyRepeat)
		{
			return true;
		}
		return Super.OnUIEvent(ev);
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool MouseEvent(int type, int x, int y)
	{
		int cell_width = 18 * CleanXfac;
		int cell_height = 12 * CleanYfac;
		int screen_y = screen.GetHeight() - INPUTGRID_HEIGHT * cell_height;
		int screen_x = (screen.GetWidth() - INPUTGRID_WIDTH * cell_width) / 2;

		if (x >= screen_x && x < screen_x + INPUTGRID_WIDTH * cell_width && y >= screen_y)
		{
			InputGridX = (x - screen_x) / cell_width;
			InputGridY = (y - screen_y) / cell_height;
			if (type == MOUSE_Release)
			{
				if (MenuEvent(MKEY_Enter, true))
				{
					MenuSound("menu/choose");
					if (m_use_mouse == 2) InputGridX = InputGridY = -1;
				}
			}
			return true;
		}
		else
		{
			InputGridX = InputGridY = -1;
		}
		return Super.MouseEvent(type, x, y);
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool MenuEvent (int key, bool fromcontroller)
	{
		String InputGridChars = Chars;
		if (key == MKEY_Back)
		{
			mParentMenu.MenuEvent(MKEY_Abort, false);
			return Super.MenuEvent(key, fromcontroller);
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
				if (mEnterString.Length() > 0)
				{
					mEnterString.Truncate(mEnterString.Length() - 1);
				}
				return true;

			case MKEY_Enter:
				if (mInputGridOkay)
				{
					String c = InputGridChars.CharAt(InputGridX + InputGridY * INPUTGRID_WIDTH);
					int ch = c.CharCodeAt(0);
					if (ch == 0)			// end
					{
						if (mEnterString.Length() > 0)
						{
							Menu parent = mParentMenu;
							parent.MenuEvent(MKEY_Input, false);
							Close();
							return true;
						}
					}
					else if (ch == 8)	// bs
					{
						if (mEnterString.Length() > 0)
						{
							mEnterString.Truncate(mEnterString.Length() - 1);
						}
					}
					else if (mEnterString.Length() < mEnterSize &&
						(mSizeMode == 2/*entering player name*/ || SmallFont.StringWidth(mEnterString) < (mEnterSize-1)*8))
					{
						mEnterString = mEnterString .. c;
					}
				}
				return true;

			default:
				break;	// Keep GCC quiet
			}
		}
		return false;
	}

	
	//=============================================================================
	//
	//
	//
	//=============================================================================

	override void Drawer ()
	{
		mParentMenu.Drawer();
		if (mInputGridOkay)
		{
			String InputGridChars = Chars;
			int cell_width = 18 * CleanXfac;
			int cell_height = 12 * CleanYfac;
			int top_padding = cell_height / 2 - SmallFont.GetHeight() * CleanYfac / 2;

			// Darken the background behind the character grid.
			screen.Dim(0, 0.8, 0, screen.GetHeight() - INPUTGRID_HEIGHT * cell_height, screen.GetWidth(), INPUTGRID_HEIGHT * cell_height);

			if (InputGridX >= 0 && InputGridY >= 0)
			{
				// Highlight the background behind the selected character.
				screen.Dim(Color(255,248,220), 0.6,
					InputGridX * cell_width - INPUTGRID_WIDTH * cell_width / 2 + screen.GetWidth() / 2,
					InputGridY * cell_height - INPUTGRID_HEIGHT * cell_height + screen.GetHeight(),
					cell_width, cell_height);
			}

			for (int y = 0; y < INPUTGRID_HEIGHT; ++y)
			{
				int yy = y * cell_height - INPUTGRID_HEIGHT * cell_height + screen.GetHeight();
				for (int x = 0; x < INPUTGRID_WIDTH; ++x)
				{
					int xx = x * cell_width - INPUTGRID_WIDTH * cell_width / 2 + screen.GetWidth() / 2;
					int ch = InputGridChars.CharCodeAt(y * INPUTGRID_WIDTH + x);
					int width = SmallFont.GetCharWidth(ch);

					// The highlighted character is yellow; the rest are dark gray.
					int colr = (x == InputGridX && y == InputGridY) ? Font.CR_YELLOW : Font.CR_DARKGRAY;
					Color palcolor = (x == InputGridX && y == InputGridY) ? Color(160,  120,   0) : Color(120, 120, 120);

					if (ch > 32)
					{
						// Draw a normal character.
						screen.DrawChar(SmallFont, colr, xx + cell_width/2 - width*CleanXfac/2, yy + top_padding, ch, DTA_CleanNoMove, true);
					}
					else if (ch == 32)
					{
						// Draw the space as a box outline. We also draw it 50% wider than it really is.
						int x1 = xx + cell_width/2 - width * CleanXfac * 3 / 4;
						int x2 = x1 + width * 3 * CleanXfac / 2;
						int y1 = yy + top_padding;
						int y2 = y1 + SmallFont.GetHeight() * CleanYfac;
						screen.Clear(x1, y1, x2, y1+CleanYfac, palcolor);	// top
						screen.Clear(x1, y2, x2, y2+CleanYfac, palcolor);	// bottom
						screen.Clear(x1, y1+CleanYfac, x1+CleanXfac, y2, palcolor);	// left
						screen.Clear(x2-CleanXfac, y1+CleanYfac, x2, y2, palcolor);	// right
					}
					else if (ch == 8 || ch == 0)
					{
						// Draw the backspace and end "characters".
						String str = ch == 8 ? "BS" : "ED";
						screen.DrawText(SmallFont, colr,
							xx + cell_width/2 - SmallFont.StringWidth(str)*CleanXfac/2,
							yy + top_padding, str, DTA_CleanNoMove, true);
					}
				}
			}
		}
		Super.Drawer();
	}
	
}
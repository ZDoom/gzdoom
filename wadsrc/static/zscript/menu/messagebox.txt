/*
** messagebox.cpp
** Confirmation, notification screens
**
**---------------------------------------------------------------------------
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

class MessageBoxMenu : Menu
{
	BrokenLines mMessage;
	voidptr Handler;
	int mMessageMode;
	int messageSelection;
	int mMouseLeft, mMouseRight, mMouseY;
	Name mAction;

	native static void CallHandler(voidptr hnd);


	//=============================================================================
	//
	//
	//
	//=============================================================================

	virtual void Init(Menu parent, String message, int messagemode, bool playsound = false, Name cmd = 'None', voidptr native_handler = null)
	{
		Super.Init(parent);
		mAction = cmd;
		messageSelection = 0;
		mMouseLeft = 140;
		mMouseY = 0x80000000;
		int mr1 = 170 + SmallFont.StringWidth(Stringtable.Localize("$TXT_YES"));
		int mr2 = 170 + SmallFont.StringWidth(Stringtable.Localize("$TXT_NO"));
		mMouseRight = MAX(mr1, mr2);
		mParentMenu = parent;
		mMessage = SmallFont.BreakLines(Stringtable.Localize(message), 300);
		mMessageMode = messagemode;
		if (playsound)
		{
			MenuSound ("menu/prompt");
		}
		Handler = native_handler;
	}
	
	//=============================================================================
	//
	//
	//
	//=============================================================================

	override void Drawer ()
	{
		int i, y;

		int fontheight = SmallFont.GetHeight();

		y = 100;

		int c = mMessage.Count();
		for (i = 0; i < c; i++)
			y -= SmallFont.GetHeight () / 2;

		for (i = 0; i < c; i++)
		{
			screen.DrawText (SmallFont, Font.CR_UNTRANSLATED, 160 - mMessage.StringWidth(i)/2, y, mMessage.StringAt(i), DTA_Clean, true);
			y += fontheight;
		}

		if (mMessageMode == 0)
		{
			y += fontheight;
			mMouseY = y;
			screen.DrawText(SmallFont, messageSelection == 0? OptionMenuSettings.mFontColorSelection : OptionMenuSettings.mFontColor, 160, y, Stringtable.Localize("$TXT_YES"), DTA_Clean, true);
			screen.DrawText(SmallFont, messageSelection == 1? OptionMenuSettings.mFontColorSelection : OptionMenuSettings.mFontColor, 160, y + fontheight + 1, Stringtable.Localize("$TXT_NO"), DTA_Clean, true);

			if (messageSelection >= 0)
			{
				if ((MenuTime() % 8) < 6)
				{
					screen.DrawText(ConFont, OptionMenuSettings.mFontColorSelection,
						(150 - 160) * CleanXfac + screen.GetWidth() / 2,
						(y + (fontheight + 1) * messageSelection - 100 + fontheight/2 - 5) * CleanYfac + screen.GetHeight() / 2,
						"\xd", DTA_CellX, 8 * CleanXfac, DTA_CellY, 8 * CleanYfac);
				}
			}
		}
	}

	
	//=============================================================================
	//
	//
	//
	//=============================================================================

	protected void CloseSound()
	{
		MenuSound (GetCurrentMenu() != NULL? "menu/backup" : "menu/dismiss");
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	virtual void HandleResult(bool res)
	{
		if (Handler != null)
		{
			if (res) 
			{
				CallHandler(Handler);
			}
			else
			{
				Close();
				CloseSound();
			}
		}
		else if (mParentMenu != NULL)
		{
			if (mMessageMode == 0)
			{
				if (mAction == 'None') 
				{
					mParentMenu.MenuEvent(res? MKEY_MBYes : MKEY_MBNo, false);
					Close();
				}
				else 
				{
					Close();
					if (res) SetMenu(mAction, -1);
				}
				CloseSound();
			}
		}
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool OnUIEvent(UIEvent ev)
	{
		if (ev.type == UIEvent.Type_KeyDown)
		{
			if (mMessageMode == 0)
			{
				// tolower
				int ch = ev.KeyChar;
				ch = ch >= 65 && ch <91? ch + 32 : ch;

				if (ch == 110 /*'n'*/ || ch == 32) 
				{
					HandleResult(false);		
					return true;
				}
				else if (ch == 121 /*'y'*/) 
				{
					HandleResult(true);
					return true;
				}
			}
			else
			{
				Close();
				return true;
			}
			return false;
		}
		return Super.OnUIEvent(ev);
	}
	
	override bool OnInputEvent(InputEvent ev)
	{
		if (ev.type == InputEvent.Type_KeyDown)
		{
			Close();
			return true;
		}
		return Super.OnInputEvent(ev);
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool MenuEvent(int mkey, bool fromcontroller)
	{
		if (mMessageMode == 0)
		{
			if (mkey == MKEY_Up || mkey == MKEY_Down)
			{
				MenuSound("menu/cursor");
				messageSelection = !messageSelection;
				return true;
			}
			else if (mkey == MKEY_Enter)
			{
				// 0 is yes, 1 is no
				HandleResult(!messageSelection);
				return true;
			}
			else if (mkey == MKEY_Back)
			{
				HandleResult(false);
				return true;
			}
			return false;
		}
		else
		{
			Close();
			CloseSound();
			return true;
		}
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool MouseEvent(int type, int x, int y)
	{
		if (mMessageMode == 1)
		{
			if (type == MOUSE_Click)
			{
				return MenuEvent(MKEY_Enter, true);
			}
			return false;
		}
		else
		{
			int sel = -1;
			int fh = SmallFont.GetHeight() + 1;

			// convert x/y from screen to virtual coordinates, according to CleanX/Yfac use in DrawTexture
			x = ((x - (screen.GetWidth() / 2)) / CleanXfac) + 160;
			y = ((y - (screen.GetHeight() / 2)) / CleanYfac) + 100;

			if (x >= mMouseLeft && x <= mMouseRight && y >= mMouseY && y < mMouseY + 2 * fh)
			{
				sel = y >= mMouseY + fh;
			}
			if (sel != -1 && sel != messageSelection)
			{
				//S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", snd_menuvolume, ATTN_NONE);
			}
			messageSelection = sel;
			if (type == MOUSE_Release)
			{
				return MenuEvent(MKEY_Enter, true);
			}
			return true;
		}
	}


}




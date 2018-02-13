/*
** playermenu.txt
** The player setup menu
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

//=============================================================================
//
// items for the player menu
//
//=============================================================================

class ListMenuItemPlayerNameBox : ListMenuItemSelectable
{
	String mText;
	Font mFont;
	int mFontColor;
	int mFrameSize;
	String mPlayerName;
	TextEnterMenu mEnter;

	//=============================================================================
	//
	// Player's name
	//
	//=============================================================================

	void Init(ListMenuDescriptor desc, String text, int frameofs, Name command)
	{
		Super.Init(desc.mXpos, desc.mYpos, desc.mLinespacing, command);
		mText = text;
		mFont = desc.mFont;
		mFontColor = desc.mFontColor;
		mFrameSize = frameofs;
		mPlayerName = "";
		mEnter = null;
	}

	//=============================================================================
	//
	// Player's name
	//
	//=============================================================================

	void InitDirect(double x, double y, int height, int frameofs, String text, Font font, int color, Name command)
	{
		Super.Init(x, y, height, command);
		mText = text;
		mFont = font;
		mFontColor = color;
		mFrameSize = frameofs;
		mPlayerName = "";
		mEnter = null;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool SetString(int i, String s)
	{
		if (i == 0)
		{
			mPlayerName = s.Mid(0, MAXPLAYERNAME);
			return true;
		}
		return false;
	}

	override bool, String GetString(int i)
	{
		if (i == 0)
		{
			return true, mPlayerName;
		}
		return false, "";
	}

	//=============================================================================
	//
	// [RH] Width of the border is variable
	//
	//=============================================================================

	protected void DrawBorder (double x, double y, int len)
	{
		let left = TexMan.CheckForTexture("M_LSLEFT", TexMan.Type_MiscPatch);
		let mid = TexMan.CheckForTexture("M_LSCNTR", TexMan.Type_MiscPatch);
		let right = TexMan.CheckForTexture("M_LSRGHT", TexMan.Type_MiscPatch);
		if (left.IsValid() && right.IsValid() && mid.IsValid())
		{
			int i;

			screen.DrawTexture (left, false, x-8, y+7, DTA_Clean, true);

			for (i = 0; i < len; i++)
			{
				screen.DrawTexture (mid, false, x, y+7, DTA_Clean, true);
				x += 8;
			}

			screen.DrawTexture (right, false, x, y+7, DTA_Clean, true);
		}
		else
		{
			let slot = TexMan.CheckForTexture("M_FSLOT", TexMan.Type_MiscPatch);
			if (slot.IsValid())
			{
				screen.DrawTexture (slot, false, x, y+1, DTA_Clean, true);
			}
			else
			{
				int xx = int(x - 160) * CleanXfac + screen.GetWidth()/2;
				int yy = int(y - 100) * CleanXfac + screen.GetHeight()/2;
				screen.Clear(xx, yy, xx + len*CleanXfac, yy + SmallFont.GetHeight() * CleanYfac * 3/2, 0);
			}
		}
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override void Drawer(bool selected)
	{
		String text = StringTable.Localize(mText);
		if (text.Length() > 0)
		{
			screen.DrawText(mFont, selected? OptionMenuSettings.mFontColorSelection : mFontColor, mXpos, mYpos, text, DTA_Clean, true);
		}

		// Draw player name box
		double x = mXpos + mFont.StringWidth(text) + 16 + mFrameSize;
		DrawBorder (x, mYpos - mFrameSize, MAXPLAYERNAME+1);
		if (!mEnter)
		{
			screen.DrawText (SmallFont, Font.CR_UNTRANSLATED, x + mFrameSize, mYpos, mPlayerName, DTA_Clean, true);
		}
		else
		{
			let printit = mEnter.GetText() .. SmallFont.GetCursor();
			screen.DrawText (SmallFont, Font.CR_UNTRANSLATED, x + mFrameSize, mYpos, printit, DTA_Clean, true);
		}
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool MenuEvent(int mkey, bool fromcontroller)
	{
		if (mkey == Menu.MKEY_Enter)
		{
			Menu.MenuSound ("menu/choose");
			mEnter = TextEnterMenu.Open(Menu.GetCurrentMenu(), mPlayerName, MAXPLAYERNAME, 2, fromcontroller);
			mEnter.ActivateMenu();
			return true;
		}
		else if (mkey == Menu.MKEY_Input)
		{
			mPlayerName = mEnter.GetText();
			mEnter = null;
			return true;
		}
		else if (mkey == Menu.MKEY_Abort)
		{
			mEnter = null;
			return true;
		}
		return false;
	}

}

//=============================================================================
//
// items for the player menu
//
//=============================================================================

class ListMenuItemValueText : ListMenuItemSelectable
{
	Array<String> mSelections;
	String mText;
	int mSelection;
	Font mFont;
	int mFontColor;
	int mFontColor2;

	//=============================================================================
	//
	// items for the player menu
	//
	//=============================================================================

	void Init(ListMenuDescriptor desc, String text, Name command, Name values = 'None')
	{
		Super.Init(desc.mXpos, desc.mYpos, desc.mLinespacing, command);
		mText = text;
		mFont = desc.mFont;
		mFontColor = desc.mFontColor;
		mFontColor2 = desc.mFontColor2;
		mSelection = 0;
		let cnt = OptionValues.GetCount(values);
		for(int i = 0; i < cnt; i++)
		{
			SetString(i, OptionValues.GetText(values, i));
		}
	}

	//=============================================================================
	//
	// items for the player menu
	//
	//=============================================================================

	void InitDirect(double x, double y, int height, String text, Font font, int color, int valuecolor, Name command, Name values)
	{
		Super.Init(x, y, height, command);
		mText = text;
		mFont = font;
		mFontColor = color;
		mFontColor2 = valuecolor;
		mSelection = 0;
		let cnt = OptionValues.GetCount(values);
		for(int i = 0; i < cnt; i++)
		{
			SetString(i, OptionValues.GetText(values, i));
		}
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool SetString(int i, String s)
	{
		// should actually use the index...
		if (i==0) mSelections.Clear();
		mSelections.Push(s);
		return true;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool SetValue(int i, int value)
	{
		if (i == 0)
		{
			mSelection = value;
			return true;
		}
		return false;
	}

	override bool, int GetValue(int i)
	{
		if (i == 0)
		{
			return true, mSelection;
		}
		return false, 0;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool MenuEvent (int mkey, bool fromcontroller)
	{
		if (mSelections.Size() > 1)
		{
			if (mkey == Menu.MKEY_Left)
			{
				Menu.MenuSound("menu/change");
				if (--mSelection < 0) mSelection = mSelections.Size() - 1;
				return true;
			}
			else if (mkey == Menu.MKEY_Right || mkey == Menu.MKEY_Enter)
			{
				Menu.MenuSound("menu/change");
				if (++mSelection >= mSelections.Size()) mSelection = 0;
				return true;
			}
		}
		return (mkey == Menu.MKEY_Enter);	// needs to eat enter keys so that Activate won't get called
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override void Drawer(bool selected)
	{
		String text = Stringtable.Localize(mText);
		screen.DrawText(mFont, selected? OptionMenuSettings.mFontColorSelection : mFontColor, mXpos, mYpos, text, DTA_Clean, true);

		double x = mXpos + mFont.StringWidth(text) + 8;
		if (mSelections.Size() > 0)
		{
			screen.DrawText(mFont, mFontColor2, x, mYpos, mSelections[mSelection], DTA_Clean, true);
		}
	}

}

//=============================================================================
//
// items for the player menu
//
//=============================================================================

class ListMenuItemSlider : ListMenuItemSelectable
{
	String mText;
	Font mFont;
	int mFontColor;
	int mMinrange, mMaxrange;
	int mStep;
	int mSelection;
	int mDrawX;

	//=============================================================================
	//
	// items for the player menu
	//
	//=============================================================================

	void Init(ListMenuDescriptor desc, String text, Name command, int min, int max, int step)
	{
		Super.Init(desc.mXpos, desc.mYpos, desc.mLinespacing, command);
		mText = text;
		mFont = desc.mFont;
		mFontColor = desc.mFontColor;
		mSelection = 0;
		mMinrange = min;
		mMaxrange = max;
		mStep = step;
		mDrawX = 0;
	}

	//=============================================================================
	//
	// items for the player menu
	//
	//=============================================================================

	void InitDirect(double x, double y, int height, String text, Font font, int color, Name command, int min, int max, int step)
	{
		Super.Init(x, y, height, command);
		mText = text;
		mFont = font;
		mFontColor = color;
		mSelection = 0;
		mMinrange = min;
		mMaxrange = max;
		mStep = step;
		mDrawX = 0;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool SetValue(int i, int value)
	{
		if (i == 0)
		{
			mSelection = value;
			return true;
		}
		return false;
	}

	override bool, int GetValue(int i)
	{
		if (i == 0)
		{
			return true, mSelection;
		}
		return false, 0;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool MenuEvent (int mkey, bool fromcontroller)
	{
		if (mkey == Menu.MKEY_Left)
		{
			Menu.MenuSound("menu/change");
			if ((mSelection -= mStep) < mMinrange) mSelection = mMinrange;
			return true;
		}
		else if (mkey == Menu.MKEY_Right || mkey == Menu.MKEY_Enter)
		{
			Menu.MenuSound("menu/change");
			if ((mSelection += mStep) > mMaxrange) mSelection = mMaxrange;
			return true;
		}
		return false;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool MouseEvent(int type, int x, int y)
	{
		let lm = Menu.GetCurrentMenu();
		if (type != Menu.MOUSE_Click)
		{
			if (!lm.CheckFocus(self)) return false;
		}
		if (type == Menu.MOUSE_Release)
		{
			lm.ReleaseFocus();
		}

		int slide_left = mDrawX + 8;
		int slide_right = slide_left + 10*8;	// 12 char cells with 8 pixels each.

		if (type == Menu.MOUSE_Click)
		{
			if (x < slide_left || x >= slide_right) return true;
		}

		x = clamp(x, slide_left, slide_right);
		int v = mMinrange + (x - slide_left) * (mMaxrange - mMinrange) / (slide_right - slide_left);
		if (v != mSelection)
		{
			mSelection = v;
			Menu.MenuSound("menu/change");
		}
		if (type == Menu.MOUSE_Click)
		{
			lm.SetFocus(self);
		}
		return true;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	protected void DrawSlider (double x, double y)
	{
		int range = mMaxrange - mMinrange;
		int cur = mSelection - mMinrange;

		x = (x - 160) * CleanXfac + screen.GetWidth() / 2;
		y = (y - 100) * CleanYfac + screen.GetHeight() / 2;

		screen.DrawText (ConFont, Font.CR_WHITE, x, y, "\x10\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x12", DTA_CellX, 8 * CleanXfac, DTA_CellY, 8 * CleanYfac);
		screen.DrawText (ConFont, Font.FindFontColor(gameinfo.mSliderColor), x + (5 + (int)((cur * 78) / range)) * CleanXfac, y, "\x13", DTA_CellX, 8 * CleanXfac, DTA_CellY, 8 * CleanYfac);
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override void Drawer(bool selected)
	{
		String text = StringTable.Localize(mText);

		screen.DrawText(mFont, selected? OptionMenuSettings.mFontColorSelection : mFontColor, mXpos, mYpos, text, DTA_Clean, true);

		double x = SmallFont.StringWidth ("Green") + 8 + mXpos;
		double x2 = SmallFont.StringWidth (text) + 8 + mXpos;
		mDrawX = int(MAX(x2, x));

		DrawSlider (mDrawX, mYpos);
	}
}

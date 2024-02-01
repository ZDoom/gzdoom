/*
** listmenu.cpp
** A simple menu consisting of a list of items
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


class ListMenuItem : MenuItemBase
{
	protected void DrawText(ListMenuDescriptor desc, Font fnt, int color, double x, double y, String text, bool ontop = false)
	{
		int w = desc ? desc.DisplayWidth() : ListMenuDescriptor.CleanScale;
		int h = desc ? desc.DisplayHeight() : -1;
		if (w == ListMenuDescriptor.CleanScale)
		{
			screen.DrawText(fnt, color, x, y, text, ontop? DTA_CleanTop : DTA_Clean, true);
		}
		else
		{
			screen.DrawText(fnt, color, x, y, text, DTA_VirtualWidth, w, DTA_VirtualHeight, h, DTA_FullscreenScale, FSMode_ScaleToFit43);
		}
	}

	protected void DrawTexture(ListMenuDescriptor desc, TextureID tex, double x, double y, bool ontop = false)
	{
		int w = desc ? desc.DisplayWidth() : ListMenuDescriptor.CleanScale;
		int h = desc ? desc.DisplayHeight() : -1;
		if (w == ListMenuDescriptor.CleanScale)
		{
			screen.DrawTexture(tex, true, x, y, ontop ? DTA_CleanTop : DTA_Clean, true);
		}
		else
		{
			screen.DrawTexture(tex, true, x, y, DTA_VirtualWidth, w, DTA_VirtualHeight, h, DTA_FullscreenScale, FSMode_ScaleToFit43);
		}
	}

	virtual void DrawSelector(double xofs, double yofs, TextureID tex, ListMenuDescriptor desc = null)
	{
		if (tex.isNull())
		{
			if ((Menu.MenuTime() % 8) < 6)
			{
				DrawText(desc, ConFont, OptionMenuSettings.mFontColorSelection, mXpos + xofs, mYpos + yofs + 8, "\xd");
			}
		}
		else
		{
			DrawTexture(desc, tex, mXpos + xofs, mYpos + yofs);
		}
	}

	// We cannot extend Drawer here because it is inherited from the parent class.
	virtual void Draw(bool selected, ListMenuDescriptor desc)
	{
		Drawer(selected);	// fall back to the legacy version, if not overridden
	}
}

//=============================================================================
//
// static patch
//
//=============================================================================

class ListMenuItemStaticPatch : ListMenuItem
{
	TextureID mTexture;
	bool mCentered;
	String mSubstitute;
	Font mFont;
	int mColor;

	void Init(ListMenuDescriptor desc, double x, double y, TextureID patch, bool centered = false, String substitute = "")
	{
		Super.Init(x, y);
		mTexture = patch;
		mCentered = centered;
		mSubstitute = substitute;
		mFont = desc.mFont;
		mColor = desc.mFontColor;

	}

	override void Draw(bool selected, ListMenuDescriptor desc)
	{
		if (!mTexture.Exists())
		{
			return;
		}

		double x = mXpos;
		Vector2 vec = TexMan.GetScaledSize(mTexture);

		if (mSubstitute == "" || TexMan.OkForLocalization(mTexture, mSubstitute))
		{
			if (mCentered) x -= vec.X / 2;
			DrawTexture(desc, mTexture, x, abs(mYpos), mYpos < 0);
		}
		else
		{
			let font = generic_ui ? NewSmallFont : mFont;
			if (mCentered) x -= font.StringWidth(mSubstitute) / 2;
			DrawText(desc, font, mColor, x, abs(mYpos), mSubstitute, mYpos < 0);
		}
	}
}

class ListMenuItemStaticPatchCentered : ListMenuItemStaticPatch
{
	void Init(ListMenuDescriptor desc, double x, double y, TextureID patch)
	{
		Super.Init(desc, x, y, patch, true);
	}
}

//=============================================================================
//
// static text
//
//=============================================================================

class ListMenuItemStaticText : ListMenuItem
{
	String mText;
	Font mFont;
	int mColor;
	bool mCentered;

	void Init(ListMenuDescriptor desc, double x, double y, String text, int color = -1)
	{
		Super.Init(x, y);
		mText = text;
		mFont = desc.mFont;
		mColor = color >= 0? color : desc.mFontColor;
		mCentered = desc.mCenterText;
	}

	void InitDirect(double x, double y, String text, Font font, int color = Font.CR_UNTRANSLATED, bool centered = false)
	{
		Super.Init(x, y);
		mText = text;
		mFont = font;
		mColor = color;
		mCentered = centered;
	}

	override void Draw(bool selected, ListMenuDescriptor desc)
	{
		if (mText.Length() != 0)
		{
			let font = generic_ui? NewSmallFont : mFont;

			String text = Stringtable.Localize(mText);

			double x = mXpos;
			if (mCentered) x -= font.StringWidth(text) / 2;
			DrawText(desc, font, mColor, x, abs(mYpos), text, mYpos < 0);
		}
	}
}

class ListMenuItemStaticTextCentered : ListMenuItemStaticText
{
	void Init(ListMenuDescriptor desc, double x, double y, String text, int color = -1)
	{
		Super.Init(desc, x, y, text, color);
		mCentered = true;
	}
}

//=============================================================================
//
// selectable items
//
//=============================================================================

class ListMenuItemSelectable : ListMenuItem
{
	int mHotkey;
	int mHeight;
	int mParam;

	protected void Init(double x, double y, int height, Name childmenu, int param = -1)
	{
		Super.Init(x, y, childmenu);
		mHeight = height;
		mParam = param;
		mHotkey = 0;
	}

	override bool CheckCoordinate(int x, int y)
	{
		return mEnabled > 0 && y >= mYpos && y < mYpos + mHeight;	// no x check here
	}

	override bool Selectable()
	{
		return mEnabled > 0;
	}

	override bool CheckHotkey(int c)
	{ 
		return c > 0 && c == mHotkey;
	}

	override bool Activate()
	{
		Menu.SetMenu(mAction, mParam);
		return true;
	}

	override bool MouseEvent(int type, int x, int y)
	{
		if (type == Menu.MOUSE_Release)
		{
			let m = Menu.GetCurrentMenu();
			if (m != NULL  && m.MenuEvent(Menu.MKEY_Enter, true))
			{
				return true;
			}
		}
		return false;
	}

	override Name, int GetAction()
	{
		return mAction, mParam;
	}
}

//=============================================================================
//
// text item
//
//=============================================================================

class ListMenuItemTextItem : ListMenuItemSelectable
{
	String mText;
	Font mFont;
	int mColor;
	int mColorSelected;

	void Init(ListMenuDescriptor desc, String text, String hotkey, Name child, int param = 0)
	{
		Super.Init(desc.mXpos, desc.mYpos, desc.mLinespacing, child, param);
		mText = text;
		mFont = desc.mFont;
		mColor = desc.mFontColor;
		mColorSelected = desc.mFontcolor2;
		mHotkey = hotkey.GetNextCodePoint(0);
	}

	void InitDirect(double x, double y, int height, String hotkey, String text, Font font, int color, int color2, Name child, int param = 0)
	{
		Super.Init(x, y, height, child, param);
		mText = text;
		mFont = font;
		mColor = color;
		mColorSelected = color2;
		int pos = 0;
		mHotkey = hotkey.GetNextCodePoint(0);
	}

	override void Draw(bool selected, ListMenuDescriptor desc)
	{
		let font = menuDelegate.PickFont(mFont);
		double x = mXpos;
		if (desc.mCenterText) 
		{
			x -= font.StringWidth(mText) / 2;
		}
		DrawText(desc, font, selected ? mColorSelected : mColor, x, mYpos, mText);
	}

	override int GetWidth()
	{
		let font = menuDelegate.PickFont(mFont);
		return max(1, font.StringWidth(StringTable.Localize(mText))); 
	}
}

//=============================================================================
//
// patch item
//
//=============================================================================

class ListMenuItemPatchItem : ListMenuItemSelectable
{
	TextureID mTexture;

	void Init(ListMenuDescriptor desc, TextureID patch, String hotkey, Name child, int param = 0)
	{
		Super.Init(desc.mXpos, desc.mYpos, desc.mLinespacing, child, param);
		mHotkey = hotkey.GetNextCodePoint(0);
		mTexture = patch;
	}

	void InitDirect(double x, double y, int height, TextureID patch, String hotkey, Name child, int param = 0)
	{
		Super.Init(x, y, height, child, param);
		mHotkey = hotkey.GetNextCodePoint(0);
		mTexture = patch;
	}

	override void Draw(bool selected, ListMenuDescriptor desc)
	{
		DrawTexture(desc, mTexture, mXpos, mYpos);
	}

	override int GetWidth()
	{
		return TexMan.GetSize(mTexture);
	}

}

//=============================================================================
//
// caption - draws a text using the customizer's caption hook
//
//=============================================================================

class ListMenuItemCaptionItem : ListMenuItem
{
	String mText;
	Font mFont;

	void Init(ListMenuDescriptor desc, String text, String fnt = "BigFont")
	{
		Super.Init(0, 0);
		mText = text;
		mFont = Font.FindFont(fnt);
	}

	override void Draw(bool selected, ListMenuDescriptor desc)
	{
		let font = menuDelegate.PickFont(desc.mFont);
		if (font && mText.Length() > 0)
		{
			menuDelegate.DrawCaption(mText, font, 0, true);
		}
	}
}


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
	virtual void DrawSelector(double xofs, double yofs, TextureID tex)
	{
		if (tex.isNull())
		{
			if ((Menu.MenuTime() % 8) < 6)
			{
				screen.DrawText(ConFont, OptionMenuSettings.mFontColorSelection,
					(mXpos + xofs - 160) * CleanXfac + screen.GetWidth() / 2,
					(mYpos + yofs - 100) * CleanYfac + screen.GetHeight() / 2,
					"\xd",
					DTA_CellX, 8 * CleanXfac,
					DTA_CellY, 8 * CleanYfac
					);
			}
		}
		else
		{
			screen.DrawTexture (tex, true, mXpos + xofs, mYpos + yofs, DTA_Clean, true);
		}
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
	
	override void Drawer(bool selected)
	{
		if (!mTexture.Exists())
		{
			return;
		}

		let font = generic_ui? NewSmallFont : mFont;

		double x = mXpos;
		Vector2 vec = TexMan.GetScaledSize(mTexture);
		if (mYpos >= 0)
		{
			if (mSubstitute == "" || TexMan.OkForLocalization(mTexture, mSubstitute))
			{
				if (mCentered) x -= vec.X / 2;
				screen.DrawTexture (mTexture, true, x, mYpos, DTA_Clean, true);
			}
			else
			{
				if (mCentered) x -= font.StringWidth(mSubstitute)/2;
				screen.DrawText(font, mColor, x, mYpos, mSubstitute, DTA_Clean, true);
			}
		}
		else
		{
			x = (mXpos - 160) * CleanXfac + (Screen.GetWidth()>>1);
			if (mSubstitute == "" || TexMan.OkForLocalization(mTexture, mSubstitute))
			{
				if (mCentered) x -= (vec.X * CleanXfac)/2;
				screen.DrawTexture (mTexture, true, x, -mYpos*CleanYfac, DTA_CleanNoMove, true);
			}
			else
			{
				if (mCentered) x -= (font.StringWidth(mSubstitute) * CleanXfac)/2;
				screen.DrawText(font, mColor, x, mYpos, mSubstitute, DTA_CleanNoMove, true);
			}
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
		mCentered = false;
	}
	
	void InitDirect(double x, double y, String text, Font font, int color = Font.CR_UNTRANSLATED, bool centered = false)
	{
		Super.Init(x, y);
		mText = text;
		mFont = font;
		mColor = color;
		mCentered = centered;
	}
	
	override void Drawer(bool selected)
	{
		if (mText.Length() != 0)
		{
			let font = generic_ui? NewSmallFont : mFont;

			String text = Stringtable.Localize(mText);
			if (mYpos >= 0)
			{
				double x = mXpos;
				if (mCentered) x -= font.StringWidth(text)/2;
				screen.DrawText(font, mColor, x, mYpos, text, DTA_Clean, true);
			}
			else
			{
				double x = (mXpos - 160) * CleanXfac + (Screen.GetWidth() >> 1);
				if (mCentered) x -= (font.StringWidth(text) * CleanXfac)/2;
				screen.DrawText (font, mColor, x, -mYpos*CleanYfac, text, DTA_CleanNoMove, true);
			}
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
		return mEnabled && y >= mYpos && y < mYpos + mHeight;	// no x check here
	}
	
	override bool Selectable()
	{
		return mEnabled;
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
	
	override void Drawer(bool selected)
	{
		let font = generic_ui? NewSmallFont : mFont;
		screen.DrawText(font, selected ? mColorSelected : mColor, mXpos, mYpos, mText, DTA_Clean, true);
	}
	
	override int GetWidth()
	{
		let font = generic_ui? NewSmallFont : mFont;
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
	
	override void Drawer(bool selected)
	{
		screen.DrawTexture (mTexture, true, mXpos, mYpos, DTA_Clean, true);
	}
	
	override int GetWidth()
	{
		return TexMan.GetSize(mTexture);
	}
	
}


/*
** listmenu.zs
** The main menu class
**
**---------------------------------------------------------------------------
** Copyright 2010-2020 Christoph Oelckers
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



class ListMenuDescriptor : MenuDescriptor native
{
	enum EScale
	{
		CleanScale = -1,
		OptCleanScale = -2
	};
	native Array<ListMenuItem> mItems;
	native int mSelectedItem;
	native double mSelectOfsX;
	native double mSelectOfsY;
	native TextureID mSelector;
	native int mDisplayTop;
	native double mXpos, mYpos;
	native int mWLeft, mWRight;
	native int mLinespacing;	// needs to be stored for dynamically created menus
	native int mAutoselect;	// this can only be set by internal menu creation functions
	native Font mFont;
	native int mFontColor;
	native int mFontColor2;
	native bool mCenter;
	native bool mCenterText;
	native bool mAnimatedTransition;
	native bool mAnimated;
	native bool mDontBlur;
	native bool mDontDim;
	native int mVirtWidth, mVirtHeight;

	native void Reset();
	int DisplayWidth()
	{
		if (mVirtWidth == OptCleanScale) return m_cleanscale ? CleanScale : 320;
		return mVirtWidth;
	}
	int DisplayHeight()
	{
		if (mVirtWidth == OptCleanScale) return m_cleanscale ? CleanScale : 200;
		return mVirtHeight;
	}
}

//=============================================================================
//
// list menu class runs a menu described by a DListMenuDescriptor
//
//=============================================================================

class ListMenu : Menu
{
	ListMenuDescriptor mDesc;
	MenuItemBase mFocusControl;

	virtual void Init(Menu parent = NULL, ListMenuDescriptor desc = NULL)
	{
		Super.Init(parent);
		mDesc = desc;
		AnimatedTransition = mDesc.mAnimatedTransition;
		Animated = mDesc.mAnimated;
		DontBlur = mDesc.mDontBlur;
		DontDim = mDesc.mDontDim;
		if (desc.mCenter)
		{
			double center = 160;
			for(int i=0; i < mDesc.mItems.Size(); i++)
			{
				double xpos = mDesc.mItems[i].GetX();
				int width = mDesc.mItems[i].GetWidth();
				double curx = mDesc.mSelectOfsX;

				if (width > 0 && mDesc.mItems[i].Selectable())
				{
					double left = 160 - (width - curx) / 2 - curx;
					if (left < center) center = left;
				}
			}
			for(int i=0;i<mDesc.mItems.Size(); i++)
			{
				int width = mDesc.mItems[i].GetWidth();

				if (width > 0)
				{
					mDesc.mItems[i].SetX(center);
				}
			}
		}
		// notify all items that the menu was just created.
		for(int i=0;i<mDesc.mItems.Size(); i++)
		{
			mDesc.mItems[i].OnMenuCreated();
		}
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	ListMenuItem GetItem(Name name)
	{
		for(int i = 0; i < mDesc.mItems.Size(); i++)
		{
			Name nm = mDesc.mItems[i].GetAction();
			if (nm == name) return mDesc.mItems[i];
		}
		return NULL;
	}


	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool OnUIEvent(UIEvent ev)
	{
		if (ev.Type == UIEvent.Type_KeyDown && ev.KeyChar > 0)
		{
			// tolower
			int ch = ev.KeyChar;
			ch = ch >= 65 && ch < 91 ? ch + 32 : ch;

			for(int i = mDesc.mSelectedItem + 1; i < mDesc.mItems.Size(); i++)
			{
				if (mDesc.mitems[i].Selectable() && mDesc.mItems[i].CheckHotkey(ch))
				{
					mDesc.mSelectedItem = i;
					MenuSound("menu/cursor");
					return true;
				}
			}
			for(int i = 0; i < mDesc.mSelectedItem; i++)
			{
				if (mDesc.mitems[i].Selectable() && mDesc.mItems[i].CheckHotkey(ch))
				{
					mDesc.mSelectedItem = i;
					MenuSound("menu/cursor");
					return true;
				}
			}
		}
		return Super.OnUIEvent(ev);
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool MenuEvent (int mkey, bool fromcontroller)
	{
		int oldSelect = mDesc.mSelectedItem;
		int startedAt;

		switch (mkey)
		{
		case MKEY_Up:
			startedAt = mDesc.mSelectedItem < 0 ? 0 : mDesc.mSelectedItem;
			do
			{
				if (--mDesc.mSelectedItem < 0) mDesc.mSelectedItem = mDesc.mItems.Size()-1;
			}
			while (!mDesc.mItems[mDesc.mSelectedItem].Selectable() && mDesc.mSelectedItem != startedAt);
			if (mDesc.mSelectedItem == startedAt) mDesc.mSelectedItem = oldSelect;
			MenuSound("menu/cursor");
			return true;

		case MKEY_Down:
			startedAt = mDesc.mSelectedItem < 0 ? mDesc.mItems.Size()-1 : mDesc.mSelectedItem;
			do
			{
				if (++mDesc.mSelectedItem >= mDesc.mItems.Size()) mDesc.mSelectedItem = 0;
			}
			while (!mDesc.mItems[mDesc.mSelectedItem].Selectable() && mDesc.mSelectedItem != startedAt);
			if (mDesc.mSelectedItem == startedAt) mDesc.mSelectedItem = oldSelect;
			MenuSound("menu/cursor");
			return true;

		case MKEY_Enter:
			if (mDesc.mSelectedItem >= 0 && mDesc.mItems[mDesc.mSelectedItem].Activate())
			{
				MenuSound("menu/advance");
			}
			return true;

		default:
			return Super.MenuEvent(mkey, fromcontroller);
		}
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool MouseEvent(int type, int x, int y)
	{
		int sel = -1;

		int w = mDesc.DisplayWidth();
		double sx, sy;
		if (w == ListMenuDescriptor.CleanScale)
		{
			// convert x/y from screen to virtual coordinates, according to CleanX/Yfac use in DrawTexture
			x = ((x - (screen.GetWidth() / 2)) / CleanXfac) + 160;
			y = ((y - (screen.GetHeight() / 2)) / CleanYfac) + 100;
		}
		else
		{
			// for fullscreen scale, transform coordinates so that for the given rect the coordinates are within (0, 0, w, h)
			int h = mDesc.DisplayHeight();
			double fx, fy, fw, fh;
			[fx, fy, fw, fh] = Screen.GetFullscreenRect(w, h, FSMode_ScaleToFit43);

			x = int((x - fx) * w / fw);
			y = int((y - fy) * h / fh);
		}

		if (mFocusControl != NULL)
		{
			mFocusControl.MouseEvent(type, x, y);
			return true;
		}
		else
		{
			if ((mDesc.mWLeft <= 0 || x > mDesc.mWLeft) &&
				(mDesc.mWRight <= 0 || x < mDesc.mWRight))
			{
				for(int i=0;i<mDesc.mItems.Size(); i++)
				{
					if (mDesc.mItems[i].Selectable() && mDesc.mItems[i].CheckCoordinate(x, y))
					{
						if (i != mDesc.mSelectedItem)
						{
							//MenuSound("menu/cursor");
						}
						mDesc.mSelectedItem = i;
						mDesc.mItems[i].MouseEvent(type, x, y);
						return true;
					}
				}
			}
		}
		mDesc.mSelectedItem = -1;
		return Super.MouseEvent(type, x, y);
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override void Ticker ()
	{
		Super.Ticker();
		for(int i=0;i<mDesc.mItems.Size(); i++)
		{
			mDesc.mItems[i].Ticker();
		}
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override void Drawer ()
	{
		for(int i=0;i<mDesc.mItems.Size(); i++)
		{
			if (mDesc.mItems[i].mEnabled) mDesc.mItems[i].Draw(mDesc.mSelectedItem == i, mDesc);
		}
		if (mDesc.mSelectedItem >= 0 && mDesc.mSelectedItem < mDesc.mItems.Size())
		{
			if (!menuDelegate.DrawSelector(mDesc))
				mDesc.mItems[mDesc.mSelectedItem].DrawSelector(mDesc.mSelectOfsX, mDesc.mSelectOfsY, mDesc.mSelector, mDesc);
		}
		Super.Drawer();
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override void SetFocus(MenuItemBase fc)
	{
		mFocusControl = fc;
	}
	override bool CheckFocus(MenuItemBase fc)
	{
		return mFocusControl == fc;
	}
	override void ReleaseFocus()
	{
		mFocusControl = NULL;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	void ChangeLineSpacing(int newspace)
	{
		double top = -32767;

		for (int i = 0; i < mDesc.mItems.Size(); i++)
		{
			let selitem = ListMenuItemSelectable(mDesc.mItems[i]);
			if (selitem)
			{
				let y = mDesc.mItems[i].GetY();
				if (top == -32767)
				{
					top = y;
				}
				else
				{
					let newy = top + (y - top) / mDesc.mLineSpacing * newspace;
					mDesc.mItems[i].SetY(newy);
				}
			}
		}
		mDesc.mLineSpacing = newspace;
	}
}



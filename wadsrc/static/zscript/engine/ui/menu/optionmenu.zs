/*
** optionmenu.cpp
** Handler class for the option menus and associated items
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

struct FOptionMenuSettings native version("2.4")
{
	int mTitleColor;
	int mFontColor;
	int mFontColorValue;
	int mFontColorMore;
	int mFontColorHeader;
	int mFontColorHighlight;
	int mFontColorSelection;
	int mLinespacing;
}

class OptionMenuDescriptor : MenuDescriptor native
{
	native Array<OptionMenuItem> mItems;
	native String mTitle;
	native int mSelectedItem;
	native int mDrawTop;
	native int mScrollTop;
	native int mScrollPos;
	native int mIndent;
	native int mPosition;
	native bool mDontDim;
	native bool mDontBlur;
	native bool mAnimatedTransition;
	native bool mAnimated;
	native Font mFont;

	void Reset()
	{
		// Reset the default settings (ignore all other values in the struct)
		mPosition = 0;
		mScrollTop = 0;
		mIndent = 0;
		mDontDim = 0;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	void CalcIndent()
	{
		// calculate the menu indent
		int widest = 0, thiswidth;

		for (int i = 0; i < mItems.Size(); i++)
		{
			if (!mItems[i].Visible()) continue;
			thiswidth = mItems[i].GetIndent();
			if (thiswidth > widest) widest = thiswidth;
		}
		mIndent =  widest + 4;
	}
}


class OptionMenu : Menu
{
	OptionMenuDescriptor mDesc;
	bool CanScrollUp;
	bool CanScrollDown;
	int VisBottom;
	OptionMenuItem mFocusControl;

	//=============================================================================
	//
	//
	//
	//=============================================================================

	virtual void Init(Menu parent, OptionMenuDescriptor desc)
	{
		mParentMenu = parent;
		mDesc = desc;
		DontDim = desc.mDontDim;
		DontBlur = desc.mDontBlur;
		AnimatedTransition = desc.mAnimatedTransition;
		Animated = desc.mAnimated;

		let itemCount = mDesc.mItems.size();
		if (itemCount > 0)
		{
			let last = mDesc.mItems[itemCount - 1];
			bool lastIsText = (last is "OptionMenuItemStaticText");
			if (lastIsText)
			{
				String text = last.mLabel;
				bool lastIsSpace = (text == "" || text == " ");
				if (lastIsSpace)
				{
					mDesc.mItems.Pop();
				}
			}
		}

		if (mDesc.mSelectedItem == -1) mDesc.mSelectedItem = FirstSelectable();
		mDesc.CalcIndent();

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

	OptionMenuItem GetItem(Name name)
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

	int FirstSelectable()
	{
		// Go down to the first selectable item
		int i = -1;
		do
		{
			i++;
		}
		while (i < mDesc.mItems.Size() && !(mDesc.mItems[i].Selectable() && mDesc.mItems[i].Visible()));
		if (i>=0 && i < mDesc.mItems.Size()) return i;
		else return -1;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	int LastVisibleItem()
	{
		int i = mDesc.mItems.Size();
		do
		{
			i--;
		}
		while (i >= 0 && !mDesc.mItems[i].Visible());
		return i;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	int RemainingVisibleItems(int start)
	{
		int count = 0;
		for (int i = start+1; i < mDesc.mItems.Size(); i++)
		{
			if (mDesc.mItems[i].Visible())
			{
				count++;
			}
		}
		return count;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool OnUIEvent(UIEvent ev)
	{
		if (ev.type == UIEvent.Type_WheelUp)
		{
			return MenuEvent(MKEY_Up, false);
		}
		else if (ev.type == UIEvent.Type_WheelDown)
		{
			return MenuEvent(MKEY_Down, false);
		}
		else if (ev.type == UIEvent.Type_Char)
		{
			int key = String.CharLower(ev.keyChar);
			int itemsNumber = mDesc.mItems.Size();
			int direction = ev.IsAlt ? -1 : 1;
			for (int i = 0; i < itemsNumber; ++i)
			{
				int index = (mDesc.mSelectedItem + direction * (i + 1) + itemsNumber) % itemsNumber;
				if (!(mDesc.mItems[index].Selectable() && mDesc.mItems[index].Visible())) continue;
				String label = StringTable.Localize(mDesc.mItems[index].mLabel);
				int firstLabelCharacter = String.CharLower(label.GetNextCodePoint(0));
				if (firstLabelCharacter == key)
				{
					mDesc.mSelectedItem = index;
					break;
				}
			}
			if (mDesc.mSelectedItem <= mDesc.mScrollTop + mDesc.mScrollPos
				|| mDesc.mSelectedItem > VisBottom)
			{
				int pagesize = VisBottom - mDesc.mScrollPos - mDesc.mScrollTop;
				mDesc.mScrollPos = clamp(mDesc.mSelectedItem - mDesc.mScrollTop - 1, 0, RemainingVisibleItems(mDesc.mSelectedItem) - pagesize - 1);
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
		int startedAt = mDesc.mSelectedItem;

		switch (mkey)
		{
		case MKEY_Up:
		{
			if (mDesc.mSelectedItem == -1)
			{
				mDesc.mSelectedItem = LastVisibleItem();
				break;
			}

			int previousSelection = mDesc.mSelectedItem;
			do
			{
				mDesc.mSelectedItem--;
				if (mDesc.mSelectedItem < 0)
				{
					mDesc.mSelectedItem = mDesc.mItems.Size() - 1;
				}
			}
			while (!(mDesc.mItems[mDesc.mSelectedItem].Selectable() && mDesc.mItems[mDesc.mSelectedItem].Visible()) && mDesc.mSelectedItem != previousSelection);

			if (mDesc.mSelectedItem != previousSelection)
			{
				int viewTop = mDesc.mScrollTop + mDesc.mScrollPos;

				if (previousSelection == FirstSelectable() && mDesc.mSelectedItem == LastVisibleItem())
				{
					int y = mDesc.mPosition;
					if (y <= 0) y = DrawCaption(mDesc.mTitle, -y, false);
					int lastrow = screen.GetHeight() - OptionHeight() * CleanYfac_1;
					int rowheight = OptionMenuSettings.mLinespacing * CleanYfac_1 + 1;

					int maxitems = (lastrow - y) / rowheight + 1;
					if (maxitems < RemainingVisibleItems(0))
					{
						maxItems -= 2;
					}
					if (maxitems <= 0) maxitems = 1;

					int lastItem = LastVisibleItem();
					int newTopIndex = 0;
					int visibleItemsOnPage = 0;
					for (int i = lastItem; i >= 0; i--)
					{
						if (mDesc.mItems[i].Visible())
						{
							visibleItemsOnPage++;
							if (visibleItemsOnPage >= maxitems)
							{
								newTopIndex = i;
								break;
							}
						}
					}

					mDesc.mScrollPos = newTopIndex - mDesc.mScrollTop;
				}
				else if (mDesc.mSelectedItem < viewTop)
				{
					int visibleLinesJumped = 0;
					for (int i = previousSelection - 1; i >= mDesc.mSelectedItem; i--)
					{
						if (mDesc.mItems[i].Visible())
						{
							visibleLinesJumped++;
						}
					}

					int visibleLinesToScroll = 0;
					int newScrollPos = mDesc.mScrollPos;
					while (visibleLinesToScroll < visibleLinesJumped && newScrollPos > 0)
					{
						newScrollPos--;
						if ((newScrollPos + mDesc.mScrollTop) >= 0 && mDesc.mItems[newScrollPos + mDesc.mScrollTop].Visible())
						{
							visibleLinesToScroll++;
						}
					}
					mDesc.mScrollPos = newScrollPos;
				}
			}
			break;
		}

		case MKEY_Down:
		{
			if (mDesc.mSelectedItem == -1)
			{
				mDesc.mSelectedItem = FirstSelectable();
				break;
			}

			int previousSelection = mDesc.mSelectedItem;
			do
			{
				mDesc.mSelectedItem++;
				if (mDesc.mSelectedItem >= mDesc.mItems.Size())
					mDesc.mSelectedItem = 0;
			}
			while (!(mDesc.mItems[mDesc.mSelectedItem].Selectable() && mDesc.mItems[mDesc.mSelectedItem].Visible()) && mDesc.mSelectedItem != previousSelection);

			if (mDesc.mSelectedItem != previousSelection)
			{
				if (previousSelection == LastVisibleItem())
				{
					mDesc.mScrollPos = 0;
				}
				else if (mDesc.mSelectedItem > VisBottom && VisBottom != -1)
				{
					int visibleLinesJumped = 0;
					for (int i = previousSelection + 1; i <= mDesc.mSelectedItem; i++)
					{
						if (mDesc.mItems[i].Visible())
						{
							visibleLinesJumped++;
						}
					}
					int visibleLinesToScroll = 0;
					int newScrollPos = mDesc.mScrollPos;
					while (visibleLinesToScroll < visibleLinesJumped && newScrollPos < mDesc.mItems.Size() - 1)
					{
						newScrollPos++;
						if ((newScrollPos + mDesc.mScrollTop) < mDesc.mItems.Size() && mDesc.mItems[newScrollPos + mDesc.mScrollTop].Visible())
						{
							visibleLinesToScroll++;
						}
					}
					mDesc.mScrollPos = newScrollPos;
				}
			}
			break;
		}

		case MKEY_PageUp:
			if (mDesc.mScrollPos > 0)
			{
				mDesc.mScrollPos -= VisBottom - mDesc.mScrollPos - mDesc.mScrollTop;
				if (mDesc.mScrollPos < 0)
				{
					mDesc.mScrollPos = 0;
				}
				if (mDesc.mSelectedItem != -1)
				{
					mDesc.mSelectedItem = mDesc.mScrollTop + mDesc.mScrollPos + 1;
					while (!(mDesc.mItems[mDesc.mSelectedItem].Selectable() && mDesc.mItems[mDesc.mSelectedItem].Visible()))
					{
						if (++mDesc.mSelectedItem >= RemainingVisibleItems(0))
						{
							mDesc.mSelectedItem = 0;
						}
					}
					if (mDesc.mScrollPos > mDesc.mSelectedItem)
					{
						mDesc.mScrollPos = mDesc.mSelectedItem;
					}
				}
			}
			break;

		case MKEY_PageDown:
			if (CanScrollDown)
			{
				int pagesize = VisBottom - mDesc.mScrollPos - mDesc.mScrollTop;
				if (pagesize > 0) mDesc.mScrollPos += pagesize;
				else mDesc.mScrollPos++;

				if (mDesc.mScrollPos + mDesc.mScrollTop + pagesize > mDesc.mItems.Size())
				{
					mDesc.mScrollPos = mDesc.mItems.Size() - mDesc.mScrollTop - pagesize;
				}
				if (mDesc.mSelectedItem != -1)
				{
					mDesc.mSelectedItem = mDesc.mScrollTop + mDesc.mScrollPos;
					while (!(mDesc.mItems[mDesc.mSelectedItem].Selectable() && mDesc.mItems[mDesc.mSelectedItem].Visible()))
					{
						if (++mDesc.mSelectedItem >= mDesc.mItems.Size())
						{
							mDesc.mSelectedItem = 0;
						}
					}
					if (mDesc.mScrollPos > mDesc.mSelectedItem)
					{
						mDesc.mScrollPos = mDesc.mSelectedItem;
					}
				}
			}
			break;

		case MKEY_Enter:
			if (mDesc.mSelectedItem >= 0 && mDesc.mItems[mDesc.mSelectedItem].Activate())
			{
				return true;
			}
			// fall through to default
		default:
			if (mDesc.mSelectedItem >= 0 &&
				mDesc.mItems[mDesc.mSelectedItem].MenuEvent(mkey, fromcontroller)) return true;
			return Super.MenuEvent(mkey, fromcontroller);
		}

		if (mDesc.mSelectedItem != startedAt)
		{
			MenuSound ("menu/cursor");
		}
		return true;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool MouseEvent(int type, int x, int y)
	{
		y = (y / CleanYfac_1) - mDesc.mDrawTop;

		if (mFocusControl)
		{
			mFocusControl.MouseEvent(type, x, y);
			return true;
		}
		else
		{
			int visual_line_clicked = y / OptionMenuSettings.mLinespacing;
			int current_visual_line = 0;

			for (int i = 0; i < mDesc.mItems.Size(); i++)
			{
				if (i == mDesc.mScrollTop)
				{
					i += mDesc.mScrollPos;

					if (i >= mDesc.mItems.Size())
					{
						break;
					}
				}

				if (!mDesc.mItems[i].Visible())
				{
					continue;
				}

				if (current_visual_line == visual_line_clicked)
				{
					if (mDesc.mItems[i].Selectable())
					{
						if (i != mDesc.mSelectedItem)
						{
							MenuSound ("menu/cursor");
							mDesc.mSelectedItem = i;
						}
						mDesc.mItems[i].MouseEvent(type, x, y);
						return true;
					}

					break;
				}

				current_visual_line++;
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
		for(int i = 0; i < mDesc.mItems.Size(); i++)
		{
			mDesc.mItems[i].Ticker();
		}
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	virtual int GetIndent()
	{
		int indent = max(0, (mDesc.mIndent + 40) - CleanWidth_1 / 2);
		return screen.GetWidth() / 2 + indent * CleanXfac_1;
	}

	//=============================================================================
	//
	// draws and/or measures the caption.
	//
	//=============================================================================

	virtual int DrawCaption(String title, int y, bool drawit)
	{
		let font = menuDelegate.PickFont(mDesc.mFont);
		if (font && mDesc.mTitle.Length() > 0)
		{
			return menuDelegate.DrawCaption(title, font, y, drawit);
		}
		else
		{
			return y;
		}

	}

	//=============================================================================
	//
	//
	//
	//=============================================================================
	override void Drawer ()
	{
		int y = mDesc.mPosition;

		if (y <= 0)
		{
			y = DrawCaption(mDesc.mTitle, -y, true);
		}
		mDesc.mDrawTop = y / CleanYfac_1; // mouse checks are done in clean space.
		int fontheight = OptionMenuSettings.mLinespacing * CleanYfac_1;

		int indent = GetIndent();

		int ytop = y + mDesc.mScrollTop * 8 * CleanYfac_1;
		int lastrow = screen.GetHeight() - OptionHeight() * CleanYfac_1;

		int i;
		int lastDrawnItemIndex = -1;
		for (i = 0; i < mDesc.mItems.Size() && y <= lastrow; i++)
		{
			// Don't scroll the uppermost items
			if (i == mDesc.mScrollTop)
			{
				i += mDesc.mScrollPos;
				if (i >= mDesc.mItems.Size()) break;	// skipped beyond end of menu
			}

			if (!mDesc.mItems[i].Visible())
			{
				continue;
			}

			lastDrawnItemIndex = i;

			bool isSelected = mDesc.mSelectedItem == i;
			int cur_indent = mDesc.mItems[i].Draw(mDesc, y, indent, isSelected);
			if (cur_indent >= 0 && isSelected && mDesc.mItems[i].Selectable() && mDesc.mItems[i].Visible())
			{
				if (((MenuTime() % 8) < 6) || GetCurrentMenu() != self)
				{
					DrawOptionText(cur_indent + 3 * CleanXfac_1, y, OptionMenuSettings.mFontColorSelection, "◄");
				}
			}
			y += fontheight;
		}

		CanScrollUp = (mDesc.mScrollPos > 0);
		CanScrollDown = LastVisibleItem() > i;
		VisBottom = lastDrawnItemIndex;

		if (CanScrollUp)
		{
			DrawOptionText(screen.GetWidth() - 11 * CleanXfac_1, ytop, OptionMenuSettings.mFontColorSelection, "▲");
		}
		if (CanScrollDown)
		{
			DrawOptionText(screen.GetWidth() - 11 * CleanXfac_1 , y - 8*CleanYfac_1, OptionMenuSettings.mFontColorSelection, "▼");
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
		mFocusControl = OptionMenuItem(fc);
	}

	override bool CheckFocus(MenuItemBase fc)
	{
		return mFocusControl == fc;
	}

	override void ReleaseFocus()
	{
		mFocusControl = NULL;
	}
}


class GLTextureGLOptions : OptionMenu
{
	private int mWarningIndex;
	private string mWarningLabel;

	override void Init(Menu parent, OptionMenuDescriptor desc)
	{
		super.Init(parent, desc);

		// Find index of warning item placeholder
		mWarningIndex = -1;
		mWarningLabel = "!HQRESIZE_WARNING!";

		for (int i=0; i < mDesc.mItems.Size(); ++i)
		{
			if (mDesc.mItems[i].mLabel == mWarningLabel)
			{
				mWarningIndex = i;
				break;
			}
		}
	}

	override void OnDestroy()
	{
		// Restore warning item placeholder
		if (mWarningIndex >= 0)
		{
			mDesc.mItems[mWarningIndex].mLabel = mWarningLabel;
		}

		Super.OnDestroy();
	}

	override void Ticker()
	{
		Super.Ticker();

		if (mWarningIndex >= 0)
		{
			string message;

			if (gl_texture_hqresizemult > 1 && gl_texture_hqresizemode > 0)
			{
				int multiplier = gl_texture_hqresizemult * gl_texture_hqresizemult;

				message = StringTable.Localize("$GLTEXMNU_HQRESIZEWARN");
				string mult = String.Format("%d", multiplier);
				message.Replace("%d", mult);
			}

			mDesc.mItems[mWarningIndex].mLabel = Font.TEXTCOLOR_CYAN .. message;
		}
	}
}

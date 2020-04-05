

class ListMenuDescriptor : MenuDescriptor native
{
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

	void Reset()
	{
		// Reset the default settings (ignore all other values in the struct)
		mSelectOfsX = 0;
		mSelectOfsY = 0;
		mSelector.SetInvalid();
		mDisplayTop = 0;
		mXpos = 0;
		mYpos = 0;
		mLinespacing = 0;
		mNetgameMessage = "";
		mFont = NULL;
		mFontColor = Font.CR_UNTRANSLATED;
		mFontColor2 = Font.CR_UNTRANSLATED;
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
		int startedAt = mDesc.mSelectedItem;

		switch (mkey)
		{
		case MKEY_Up:
			do
			{
				if (--mDesc.mSelectedItem < 0) mDesc.mSelectedItem = mDesc.mItems.Size()-1;
			}
			while (!mDesc.mItems[mDesc.mSelectedItem].Selectable() && mDesc.mSelectedItem != startedAt);
			if (mDesc.mSelectedItem == startedAt) mDesc.mSelectedItem = oldSelect;
			MenuSound("menu/cursor");
			return true;

		case MKEY_Down:
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
				MenuSound("menu/choose");
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

		// convert x/y from screen to virtual coordinates, according to CleanX/Yfac use in DrawTexture
		x = ((x - (screen.GetWidth() / 2)) / CleanXfac) + 160;
		y = ((y - (screen.GetHeight() / 2)) / CleanYfac) + 100;

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
					if (mDesc.mItems[i].CheckCoordinate(x, y))
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
			if (mDesc.mItems[i].mEnabled) mDesc.mItems[i].Drawer(mDesc.mSelectedItem == i);
		}
		if (mDesc.mSelectedItem >= 0 && mDesc.mSelectedItem < mDesc.mItems.Size())
			mDesc.mItems[mDesc.mSelectedItem].DrawSelector(mDesc.mSelectOfsX, mDesc.mSelectOfsY, mDesc.mSelector);
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
}



/*
** colorpickermenu.txt
** The color picker menu
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


//=============================================================================
//
// This is only used by the color picker
//
//=============================================================================

class OptionMenuSliderVar : OptionMenuSliderBase
{
	int mIndex;

	OptionMenuSliderVar Init(String label, int index, double min, double max, double step, int showval)
	{
		Super.Init(label, min, max, step, showval);
		mIndex = index;
		return self;
	}

	override double GetSliderValue()
	{
		return ColorpickerMenu(Menu.GetCurrentMenu()).GetColor(mIndex);
	}

	override void SetSliderValue(double val)
	{
		ColorpickerMenu(Menu.GetCurrentMenu()).setColor(mIndex, val);
	}
}

class ColorpickerMenu : OptionMenu
{
	float mRed;
	float mGreen;
	float mBlue;

	int mGridPosX;
	int mGridPosY;

	int mStartItem;

	CVar mCVar;

	double GetColor(int index)
	{
		double v = index == 0? mRed : index == 1? mGreen : mBlue;
		return v;
	}

	void SetColor(int index, double val)
	{
		if (index == 0) mRed = val;
		else if (index == 1) mGreen = val;
		else mBlue = val;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	void Init(Menu parent, String name, OptionMenuDescriptor desc, CVar cv)
	{
		Super.Init(parent, desc);

		mStartItem = mDesc.mItems.Size();
		mCVar = cv;

		ResetColor();
		mGridPosX = 0;
		mGridPosY = 0;

		// This menu uses some features that are hard to implement in an external control lump
		// so it creates its own list of menu items.
		mDesc.mItems.Resize(mStartItem+8);
		mDesc.mItems[mStartItem+0] = new ("OptionMenuItemStaticText").Init(name, false);
		mDesc.mItems[mStartItem+1] = new ("OptionMenuItemStaticText").Init(" ", false);
		mDesc.mItems[mStartItem+2] = new ("OptionMenuSliderVar").Init("$TXT_COLOR_RED", 0, 0, 255, 15, 0);
		mDesc.mItems[mStartItem+3] = new ("OptionMenuSliderVar").Init("$TXT_COLOR_GREEN", 1, 0, 255, 15, 0);
		mDesc.mItems[mStartItem+4] = new ("OptionMenuSliderVar").Init("$TXT_COLOR_BLUE", 2, 0, 255, 15, 0);
		mDesc.mItems[mStartItem+5] = new ("OptionMenuItemStaticText").Init(" ", false);
		mDesc.mItems[mStartItem+6] = new ("OptionMenuItemCommand").Init("$TXT_UNDOCHANGES", "undocolorpic");
		mDesc.mItems[mStartItem+7] = new ("OptionMenuItemStaticText").Init(" ", false);
		mDesc.mSelectedItem = mStartItem + 2;
		mDesc.mIndent = 0;
		mDesc.CalcIndent();
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool MenuEvent (int mkey, bool fromcontroller)
	{
		switch (mkey)
		{
		case MKEY_Down:
			if (mDesc.mSelectedItem == mStartItem+6)	// last valid item
			{
				MenuSound ("menu/cursor");
				mGridPosY = 0;
				// let it point to the last static item so that the super class code still has a valid item
				mDesc.mSelectedItem = mStartItem+7;	
				return true;
			}
			else if (mDesc.mSelectedItem == mStartItem+7)
			{
				if (mGridPosY < 15)
				{
					MenuSound ("menu/cursor");
					mGridPosY++;
				}
				return true;
			}
			break;

		case MKEY_Up:
			if (mDesc.mSelectedItem == mStartItem+7)
			{
				if (mGridPosY > 0)
				{
					MenuSound ("menu/cursor");
					mGridPosY--;
				}
				else
				{
					MenuSound ("menu/cursor");
					mDesc.mSelectedItem = mStartItem+6;
				}
				return true;
			}
			break;

		case MKEY_Left:
			if (mDesc.mSelectedItem == mStartItem+7)
			{
				MenuSound ("menu/cursor");
				if (--mGridPosX < 0) mGridPosX = 15;
				return true;
			}
			break;

		case MKEY_Right:
			if (mDesc.mSelectedItem == mStartItem+7)
			{
				MenuSound ("menu/cursor");
				if (++mGridPosX > 15) mGridPosX = 0;
				return true;
			}
			break;

		case MKEY_Enter:
			if (mDesc.mSelectedItem == mStartItem+7)
			{
				// Choose selected palette entry
				int index = mGridPosX + mGridPosY * 16;
				color col = Screen.PaletteColor(index);
				mRed = col.r;
				mGreen = col.g;
				mBlue = col.b;
				MenuSound ("menu/choose");
				return true;
			}
			break;
		}
		if (mDesc.mSelectedItem >= 0 && mDesc.mSelectedItem < mStartItem+7) 
		{
			if (mDesc.mItems[mDesc.mSelectedItem].MenuEvent(mkey, fromcontroller)) return true;
		}
		return Super.MenuEvent(mkey, fromcontroller);
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool MouseEvent(int type, int mx, int my)
	{
		int olditem = mDesc.mSelectedItem;
		bool res = Super.MouseEvent(type, mx, my);

		if (mDesc.mSelectedItem == -1 || mDesc.mSelectedItem == mStartItem+7)
		{
			int y = (-mDesc.mPosition + BigFont.GetHeight() + mDesc.mItems.Size() * OptionMenuSettings.mLinespacing) * CleanYfac_1;
			int h = (screen.GetHeight() - y) / 16;
			int fh = OptionMenuSettings.mLinespacing * CleanYfac_1;
			int w = fh;
			int yy = y + 2 * CleanYfac_1;
			int indent = (screen.GetWidth() / 2);

			if (h > fh) h = fh;
			else if (h < 4) return res;	// no space to draw it.

			int box_y = y - 2 * CleanYfac_1;
			int box_x = indent - 16*w;

			if (mx >= box_x && mx < box_x + 16*w && my >= box_y && my < box_y + 16*h)
			{
				int cell_x = (mx - box_x) / w;
				int cell_y = (my - box_y) / h;

				if (olditem != mStartItem+7 || cell_x != mGridPosX || cell_y != mGridPosY)
				{
					mGridPosX = cell_x;
					mGridPosY = cell_y;
				}
				mDesc.mSelectedItem = mStartItem+7;
				if (type == MOUSE_Release)
				{
					MenuEvent(MKEY_Enter, true);
					if (m_use_mouse == 2) mDesc.mSelectedItem = -1;
				}
				res = true;
			}
		}
		return res;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override void Drawer()
	{
		Super.Drawer();

		if (mCVar == null) return;
		int y = (-mDesc.mPosition + BigFont.GetHeight() + mDesc.mItems.Size() * OptionMenuSettings.mLinespacing) * CleanYfac_1;
		int fh = OptionMenuSettings.mLinespacing * CleanYfac_1;
		int h = (screen.GetHeight() - y) / 16;
		int w = fh;
		int yy = y;

		if (h > fh) h = fh;
		else if (h < 4) return;	// no space to draw it.

		int indent = (screen.GetWidth() / 2);
		int p = 0;

		for(int i = 0; i < 16; i++)
		{
			int box_x, box_y;
			int x1;

			box_y = y - 2 * CleanYfac_1;
			box_x = indent - 16*w;
			for (x1 = 0; x1 < 16; ++x1)
			{
				screen.Clear (box_x, box_y, box_x + w, box_y + h, 0, p);
				if ((mDesc.mSelectedItem == mStartItem+7) && 
					(/*p == CurrColorIndex ||*/ (i == mGridPosY && x1 == mGridPosX)))
				{
					int r, g, b;
					Color col;
					double blinky;
					if (i == mGridPosY && x1 == mGridPosX)
					{
						r = 255; g = 128; b = 0;
					}
					else
					{
						r = 200; g = 200; b = 255;
					}
					// Make sure the cursors stand out against similar colors
					// by pulsing them.
					blinky = abs(sin(MSTimeF()/1000.0)) * 0.5 + 0.5;
					col = Color(255, int(r*blinky), int(g*blinky), int(b*blinky));

					screen.Clear (box_x, box_y, box_x + w, box_y + 1, col);
					screen.Clear (box_x, box_y + h-1, box_x + w, box_y + h, col);
					screen.Clear (box_x, box_y, box_x + 1, box_y + h, col);
					screen.Clear (box_x + w - 1, box_y, box_x + w, box_y + h, col);
				}
				box_x += w;
				p++;
			}
			y += h;
		}
		y = yy;
		color newColor = Color(255, int(mRed), int(mGreen), int(mBlue));
		color oldColor = mCVar.GetInt() | 0xFF000000;

		int x = screen.GetWidth()*2/3;

		screen.Clear (x, y, x + 48*CleanXfac_1, y + 48*CleanYfac_1, oldColor);
		screen.Clear (x + 48*CleanXfac_1, y, x + 48*2*CleanXfac_1, y + 48*CleanYfac_1, newColor);

		y += 49*CleanYfac_1;
		screen.DrawText (SmallFont, Font.CR_GRAY, x+(48-SmallFont.StringWidth("---->")/2)*CleanXfac_1, y, "---->", DTA_CleanNoMove_1, true);
	}

	override void OnDestroy()
	{
		if (mStartItem >= 0)
		{
			mDesc.mItems.Resize(mStartItem);
			if (mCVar != null) 
			{
				mCVar.SetInt(Color(int(mRed), int(mGreen), int(mBlue)));
			}
			mStartItem = -1;
		}
	}

	override void ResetColor()
	{
		if (mCVar != null) 
		{
			Color clr = Color(mCVar.GetInt());
			mRed = clr.r;
			mGreen = clr.g;
			mBlue = clr.b;
		}
	}
}
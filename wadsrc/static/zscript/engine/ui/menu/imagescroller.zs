/*
** imagescroller.cpp
** Scrolls through multiple fullscreen image pages,
**
**---------------------------------------------------------------------------
** Copyright 2019-220 Christoph Oelckers
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

class ImageScrollerDescriptor : MenuDescriptor native
{
	native Array<ImageScrollerPage> mItems;
	native Font textFont;
	native TextureID textBackground;
	native Color textBackgroundBrightness;
	native double textScale;
	native bool mAnimatedTransition;
	native bool mAnimated;
	native int virtWidth, virtHeight;
}

class ImageScrollerPage : MenuItemBase
{
	int virtWidth, virtHeight;

	protected void DrawText(Font fnt, int color, double x, double y, String text)
	{
		screen.DrawText(fnt, color, x, y, text, DTA_VirtualWidth, virtWidth, DTA_VirtualHeight, virtHeight, DTA_FullscreenScale, FSMode_ScaleToFit43);
	}

 	protected void DrawTexture(TextureID tex, double x, double y)
	{
		screen.DrawTexture(tex, true, x, y, DTA_VirtualWidth, virtWidth, DTA_VirtualHeight, virtHeight, DTA_FullscreenScale, FSMode_ScaleToFit43);
	}

	virtual void OnStartPage()
	{}

	virtual void OnEndPage()
	{}
}

//=============================================================================
//
// an image page
//
//=============================================================================

class ImageScrollerPageImageItem : ImageScrollerPage
{
	TextureID mTexture;

	void Init(ImageScrollerDescriptor desc, String patch)
	{
		Super.Init();
		mTexture = TexMan.CheckForTexture(patch);
	} 

	override void Drawer(bool selected)
	{
		Screen.DrawTexture(mTexture, true, 0, 0, DTA_FullscreenEx, FSMode_ScaleToFit43, DTA_LegacyRenderStyle, STYLE_Normal);
	}
}

//=============================================================================
//
// a simple text page
//
//=============================================================================

class ImageScrollerPageTextItem : ImageScrollerPage
{
	Font mFont;
	BrokenLines mText;
	TextureID mTexture;
	Color mBrightness;
	double mTextScale;

	void Init(ImageScrollerDescriptor desc, String txt, int y = -1)
	{
		Super.Init();
		mTexture = desc.textBackground;
		mBrightness = desc.textBackgroundBrightness;
		mFont = desc.textFont;
		mTextScale = desc.textScale;
		virtWidth = desc.virtWidth;
		virtHeight = desc.virtHeight;

		mText = mFont.BreakLines(Stringtable.Localize(txt.Filter()), int(virtWidth / mTextScale));
		mYpos = y >= 0? y : virtHeight / 2 - mText.Count() * mFont.GetHeight() * mTextScale / 2;

	} 

	override void Drawer(bool selected)
	{
		Screen.DrawTexture(mTexture, true, 0, 0, DTA_FullscreenEx, FSMode_ScaleToFit43, DTA_LegacyRenderStyle, STYLE_Normal, DTA_Color, mBrightness);

		let fontheight = mFont.GetHeight() * mTextScale;
		let y = mYpos;
		let c = mText.Count();
		for (int i = 0; i < c; i++)
		{
			screen.DrawText (mFont, Font.CR_UNTRANSLATED, virtWidth/2 - mText.StringWidth(i) * mTextScale / 2, y, mText.StringAt(i), DTA_ScaleX, mTextScale, DTA_ScaleY, mTextScale,
				DTA_VirtualWidth, virtWidth, DTA_VirtualHeight, virtHeight, DTA_FullscreenScale, FSMode_ScaleToFit43);
			y += fontheight;
		}
	}
}

//=============================================================================
//
// The main class
//
//=============================================================================

class ImageScrollerMenu : Menu
{
	ImageScrollerPage previous;
	ImageScrollerPage current;

	double start;
	int length;
	int dir;
	int index;
	ImageScrollerDescriptor mDesc;


	private void StartTransition(ImageScrollerPage to, int animtype)
	{
		if (AnimatedTransition)
		{
			start = MSTime() * (120. / 1000.);
			length = 30;
			dir = animtype;
			previous = current;
		}
		to.onStartPage();
		current = to;
	}

	virtual void Init(Menu parent, ImageScrollerDescriptor desc)
	{
		mParentMenu = parent;
		index = 0;
		mDesc = desc;
		AnimatedTransition = desc.mAnimatedTransition;
		Animated = desc.mAnimated;
		current = mDesc.mItems[0];
		current.onStartPage();
		previous = null;
	}

	//=============================================================================
	//
	// 
	//
	//=============================================================================

	override bool MenuEvent(int mkey, bool fromcontroller)
	{
		if (mDesc.mItems.Size() <= 1)
		{
			if (mkey == MKEY_Enter) mkey = MKEY_Back;
			else if (mkey == MKEY_Right || mkey == MKEY_Left) return true;
		}
		switch (mkey)
		{
		case MKEY_Back:
			// Before going back the currently running transition must be terminated.
			previous = null;
			return Super.MenuEvent(mkey, fromcontroller);


		case MKEY_Left:
			if (previous == null)
			{
				if (--index < 0) index = mDesc.mItems.Size() - 1;
				let next = mDesc.mItems[index];
				StartTransition(next, -1);
				MenuSound("menu/choose");
			}
			return true;

		case MKEY_Right:
		case MKEY_Enter:
			if (previous == null)
			{
				int oldindex = index;
				if (++index >= mDesc.mItems.Size()) index = 0;
				let next = mDesc.mItems[index];
				StartTransition(next, 1);
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
		// Todo: Implement some form of drag event to switch between pages.
		if (type == MOUSE_Release)
		{
			return MenuEvent(MKEY_Enter, false);
		}
		return Super.MouseEvent(type, x, y);
	}

	//=============================================================================
	//
	// 
	//
	//=============================================================================

	private bool DrawTransition()
	{
		double now = MSTime() * (120. / 1000.);
		if (now < start + length)
		{
			double factor = screen.GetWidth()/2;
			double phase = (now - start) / length * 180. + 90.;

			screen.SetOffset(factor * dir * (sin(phase) - 1.), 0);
			previous.Drawer(false);
			screen.SetOffset(factor * dir * (sin(phase) + 1.), 0);
			current.Drawer(false);
			screen.SetOffset(0, 0);
			return true;
		}
		previous.OnEndPage();
		previous = null;
		return false;
	}

	//=============================================================================
	//
	// 
	//
	//=============================================================================

	override void Drawer()
	{
		if (previous != null)
		{
			bool wasAnimated = Animated;
			Animated = true;
			if (DrawTransition()) return;
			previous = null;
			Animated = wasAnimated;
		}
		current.Drawer(false);
	}
}

/*
** listmenu.cpp
** A simple menu consisting of a list of items
**
**---------------------------------------------------------------------------
** Copyright 2010 Christoph Oelckers
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

#include "v_video.h"
#include "v_font.h"
#include "cmdlib.h"
#include "menu/menu.h"

IMPLEMENT_CLASS(DListMenu)



//=============================================================================
//
//
//
//=============================================================================

DListMenu::DListMenu(DMenu *parent, FListMenuDescriptor *desc)
: DMenu(parent)
{
	mDesc = desc;
}

//=============================================================================
//
//
//
//=============================================================================

DListMenu::~DListMenu()
{
}

//=============================================================================
//
//
//
//=============================================================================

bool DListMenu::Responder (event_t *ev)
{
	return false;
}

//=============================================================================
//
//
//
//=============================================================================

void DListMenu::Ticker ()
{
	for(unsigned i=0;i<mDesc->mItems.Size(); i++)
	{
		mDesc->mItems[i]->Ticker();
	}
}

//=============================================================================
//
//
//
//=============================================================================

void DListMenu::Drawer ()
{
	for(unsigned i=0;i<mDesc->mItems.Size(); i++)
	{
		mDesc->mItems[i]->Drawer();
	}
	mDesc->mItems[mDesc->mSelectedItem]->DrawSelector(mDesc->mSelectOfsX, mDesc->mSelectOfsY, mDesc->mSelector);
}

//=============================================================================
//
// base class for menu items
//
//=============================================================================

FListMenuItem::FListMenuItem(int x, int y) 
{
	mXpos = x;
	mYpos = y;
}

FListMenuItem::~FListMenuItem()
{
}

bool FListMenuItem::CheckCoordinate(int x, int y)
{
	return false;
}

void FListMenuItem::Ticker()
{
}

void FListMenuItem::Drawer()
{
}

bool FListMenuItem::Selectable()
{
	return false;
}

void FListMenuItem::DrawSelector(int xofs, int yofs, FTextureID tex)
{
	screen->DrawTexture (TexMan(tex), mXpos + xofs, mYpos + yofs, DTA_Clean, true, TAG_DONE);
}

//=============================================================================
//
// static patch
//
//=============================================================================

FListMenuItemStaticPatch::FListMenuItemStaticPatch(int x, int y, FTextureID patch)
: FListMenuItem(x, y)
{
	mTexture = patch;
}
	
void FListMenuItemStaticPatch::Drawer()
{
	screen->DrawTexture (TexMan(mTexture), mXpos, mYpos, DTA_Clean, true, TAG_DONE);
}

//=============================================================================
//
// static animation
//
//=============================================================================

FListMenuItemStaticAnimation::FListMenuItemStaticAnimation(int x, int y, int frametime)
: FListMenuItemStaticPatch(x, y, FNullTextureID())
{
	mFrameTime = frametime;
	mFrameCount = 0;
}

void FListMenuItemStaticAnimation::AddTexture(FTextureID tex)
{
	if (!mTexture.isValid()) mTexture = tex;
	if (tex.isValid()) mFrames.Push(tex);
}
	
void FListMenuItemStaticAnimation::Ticker()
{
	if (++mFrameCount > mFrameTime)
	{
		mFrameCount = 0;
		if (++mFrame >= mFrames.Size())
		{
			mFrame = 0;
		}
		mTexture = mFrames[mFrame];
	}
}

//=============================================================================
//
// base class for selectable items
//
//=============================================================================

FListMenuItemSelectable::FListMenuItemSelectable(int x, int y, FName childmenu)
: FListMenuItem(x, y), mChild(childmenu)
{
}

void FListMenuItemSelectable::SetHotspot(int x, int y, int w, int h)
{
	mHotspot.set(x, y, w, h);
}

bool FListMenuItemSelectable::CheckCoordinate(int x, int y)
{
	return mHotspot.inside(x, y);
}

bool FListMenuItemSelectable::Selectable()
{
	return true;
}

//=============================================================================
//
// text item
//
//=============================================================================

FListMenuItemText::FListMenuItemText(int x, int y, int hotkey, const char *text, FFont *font, EColorRange color, FName child)
: FListMenuItemSelectable(x, y, child)
{
	mText = ncopystring(text);
	mFont = font;
	mColor = color;
	mHotkey = hotkey;
}

FListMenuItemText::~FListMenuItemText()
{
	if (mText != NULL)
	{
		delete [] mText;
	}
}

void FListMenuItemText::Drawer()
{
	if (mText != NULL)
	{
		screen->DrawText(mFont, mColor, mXpos, mYpos, mText, DTA_Clean, true, TAG_DONE);
	}
}

//=============================================================================
//
// patch item
//
//=============================================================================

FListMenuItemPatch::FListMenuItemPatch(int x, int y, int hotkey, FTextureID patch, FName child)
: FListMenuItemSelectable(x, y, child)
{
	mTexture = patch;
}

void FListMenuItemPatch::Drawer()
{
	screen->DrawTexture (TexMan(mTexture), mXpos, mYpos, DTA_Clean, true, TAG_DONE);
}

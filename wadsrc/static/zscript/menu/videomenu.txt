/*
** videomenu.txt
** The video modes menu
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
//
//
//=============================================================================

class VideoModeMenu : OptionMenu
{
	native static bool SetSelectedSize();
	
	override bool MenuEvent(int mkey, bool fromcontroller)
	{
		if ((mkey == MKEY_Up || mkey == MKEY_Down) && mDesc.mSelectedItem >= 0 && mDesc.mSelectedItem < mDesc.mItems.Size())
		{
			int sel;
			bool selected;
			[selected, sel] = mDesc.mItems[mDesc.mSelectedItem].GetValue(OptionMenuItemScreenResolution.SRL_SELECTION);
			bool res = Super.MenuEvent(mkey, fromcontroller);
			if (selected) mDesc.mItems[mDesc.mSelectedItem].SetValue(OptionMenuItemScreenResolution.SRL_SELECTION, sel);
			return res;
		}
		return Super.MenuEvent(mkey, fromcontroller);
	}

	override bool OnUIEvent(UIEvent ev)
	{
		if (ev.Type == UIEvent.Type_KeyDown && (ev.KeyChar == 0x54 || ev.KeyChar == 0x74))
		{
			if (SetSelectedSize())
			{
				MenuSound ("menu/choose");
				return true;
			}
		}
		return Super.OnUIEvent(ev);
	}
}


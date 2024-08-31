/*
** readthis.cpp
** Help screens
**
**---------------------------------------------------------------------------
** Copyright 2001-2010 Randy Heit
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

class ReadThisMenu : GenericMenu
{
	int mScreen;
	int mInfoTic;

	//=============================================================================
	//
	//
	//
	//=============================================================================
	
	override void Init(Menu parent)
	{
		Super.Init(parent);
		mScreen = 1;
		mInfoTic = gametic;
	}

	override void Drawer()
	{
		double alpha;
		TextureID tex, prevpic;
		
		// Did the mapper choose a custom help page via MAPINFO?
		if (Level.F1Pic.Length() != 0)
		{
			tex = TexMan.CheckForTexture(Level.F1Pic, TexMan.Type_MiscPatch);
			mScreen = 1;
		}
		
		if (!tex.IsValid())
		{
			tex = TexMan.CheckForTexture(gameinfo.infoPages[mScreen-1], TexMan.Type_MiscPatch);
		}

		if (mScreen > 1)
		{
			prevpic = TexMan.CheckForTexture(gameinfo.infoPages[mScreen-2], TexMan.Type_MiscPatch);
		}

		screen.Dim(0, 1.0, 0,0, screen.GetWidth(), screen.GetHeight());
		alpha = MIN((gametic - mInfoTic) * (3. / GameTicRate), 1.);
		if (alpha < 1. && prevpic.IsValid())
		{
			screen.DrawTexture (prevpic, false, 0, 0, DTA_Fullscreen, true);
		}
		else alpha = 1;
		screen.DrawTexture (tex, false, 0, 0, DTA_Fullscreen, true, DTA_Alpha, alpha);

	}


	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool MenuEvent(int mkey, bool fromcontroller)
	{
		if (mkey == MKEY_Enter)
		{
			MenuSound("menu/choose");
			mScreen++;
			mInfoTic = gametic;
			if (Level.F1Pic.Length() != 0 || mScreen > gameinfo.infoPages.Size())
			{
				Close();
			}
			return true;
		}
		else return Super.MenuEvent(mkey, fromcontroller);
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool MouseEvent(int type, int x, int y)
	{
		if (type == MOUSE_Click)
		{
			return MenuEvent(MKEY_Enter, true);
		}
		return false;
	}

}
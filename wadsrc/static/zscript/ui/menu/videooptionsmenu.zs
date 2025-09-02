/*
** The Video Options menu
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

class VideoOptions : OptionMenu
{
	const MARGIN = 20;
	OptionMenuItem mylist[3];
	TextureID sampletex;
	bool once;

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override void Drawer()
	{
		if (!once)
		{
			once = true; // Is there any other virtual that we can move this to?
			mylist[0] = GetItem('vid_fixgamma');
			mylist[1] = GetItem('vid_blackpoint');
			mylist[2] = GetItem('vid_whitepoint');
			sampletex = TexMan.CheckForTexture("GAMMA1"); // Replace with whatever texture lump from gzdoom.pk3
		}

		if (sampletex && mDesc.mSelectedItem >= 0)
		{
			OptionMenuItem li = mDesc.mItems[mDesc.mSelectedItem];
			if (li && (li == mylist[0] || li == mylist[1] || li == mylist[2]))
			{
				int x = MARGIN;
				int y = MARGIN + screen.GetHeight()/4;
				int xsize, ysize;
				[xsize, ysize] = TexMan.GetSize(sampletex);
				screen.DrawTexture(sampletex, false, x, y, DTA_DestWidth, screen.GetWidth()/4,
								   DTA_DestHeight, screen.GetWidth()*ysize/4/xsize);
				DontDim = true;
				DontBlur = true;
			}
			else
			{
				DontDim = mDesc.mDontDim;
				DontBlur = mDesc.mDontBlur;
			}
		}
		Super.Drawer();
	}
}

//=============================================================================
//
//
//
//=============================================================================

class VideoOptionsSimple : VideoOptions
{
}

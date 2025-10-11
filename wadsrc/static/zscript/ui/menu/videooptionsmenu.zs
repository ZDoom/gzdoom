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

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override void Drawer()
	{
		int x = MARGIN;
		int y = MARGIN + screen.GetHeight()/4 - 7 * NewSmallFont.GetHeight();
		PPShader.SetUniform1i("GammaTestPattern", "uXmin", MARGIN);
		PPShader.SetUniform1i("GammaTestPattern", "uXmax", Screen.GetWidth()/4 + MARGIN);
		PPShader.SetUniform1i("GammaTestPattern", "uYmin", Screen.GetHeight()/4 - MARGIN);
		PPShader.SetUniform1i("GammaTestPattern", "uYmax", 3*Screen.GetHeight()/4 + MARGIN);
		PPShader.SetUniform1f("GammaTestPattern", "uInvGamma", 1.0/vid_gamma);
		PPShader.SetUniform1f("GammaTestPattern", "uWhitePoint", vid_whitepoint);
		PPShader.SetUniform1f("GammaTestPattern", "uBlackPoint", vid_blackpoint);
		PPShader.SetEnabled("GammaTestPattern", true);
		Screen.DrawText(NewSmallFont, Font.CR_CYAN, x, y,
						"Adjust until this looks like\na uniform Gray color.",
						DTA_CleanNoMove_1, true);
		DontDim = true;
		DontBlur = true;

		Super.Drawer();
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool MenuEvent(int mkey, bool fromcontroller)
	{
		if (mkey == MKEY_Back)
		{
			PPShader.SetEnabled("GammaTestPattern", false);
		}
		return Super.MenuEvent(mkey, fromcontroller);
	}
}

/*
** hudmessages.cpp
** Neato messages for the HUD
**
**---------------------------------------------------------------------------
** Copyright 1998-2001 Randy Heit
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

#include "templates.h"
#include "doomdef.h"
#include "sbar.h"
#include "c_cvars.h"
#include "v_video.h"
#include "cmdlib.h"

EXTERN_CVAR (Bool, con_scaletext)

IMPLEMENT_CLASS (DHUDMessage)
IMPLEMENT_CLASS (DHUDMessageFadeOut)
IMPLEMENT_CLASS (DHUDMessageTypeOnFadeOut)

//
// Basic HUD message. Appears and disappears without any special effects
//

DHUDMessage::DHUDMessage (const char *text, float x, float y, int hudwidth, int hudheight,
						  EColorRange textColor, float holdTime)
{
	if (hudwidth == 0 || hudheight == 0)
	{
		// for y range [-1.0, 0.0]: Positions top edge of box
		// for y range [0.0, 1.0]: Positions center of box
		// for x range [-2.0, -1.0]: Positions left edge of box, and centers text inside it
		// for x range [-1.0, 0.0]: Positions left edge of box
		// for x range [0.0, 1.0]: Positions center of box
		// for x range [1.0, 2.0]: Positions center of box, and centers text inside it
		HUDWidth = HUDHeight = 0;
		if (fabs (x) > 2.f)
		{
			CenterX = true;
			Left = 0.5f;
		}
		else
		{
			Left = x < -1.f ? x + 1.f : x > 1.f ? x - 1.f : x;
			if (fabs(x) > 1.f)
			{
				CenterX = true;
			}
			else
			{
				CenterX = false;
			}
		}
	}
	else
	{ // HUD size is specified, so coordinates are in pixels, but you can add
	  // some fractions to affect positioning:
	  // For y: .1 = positions top edge of box
	  //		.2 = positions bottom edge of box
	  // For x: .1 = positions left edge of box
	  //        .2 = positions right edge of box
	  //		.4 = centers text inside box
	  //			 .4 can also be added to either .1 or .2
		Top = y;
		HUDWidth = hudwidth;
		HUDHeight = hudheight;
		
		float intpart;
		int fracpart = (int)(fabsf (modff (x, &intpart)) * 10.f + 0.5f);
		if (fracpart & 4)
		{
			CenterX = true;
		}
		else
		{
			CenterX = false;
		}
		if (x > 0)
		{
			Left = intpart + (float)(fracpart & 3) / 10.f;
		}
		else
		{
			Left = intpart - (float)(fracpart & 3) / 10.f;
		}
	}
	Top = y;
	Next = NULL;
	Lines = NULL;
	HoldTics = (int)(holdTime * TICRATE);
	Tics = 0;
	TextColor = textColor;
	State = 0;
	SourceText = copystring (text);
	Font = screen->Font;
	ResetText (SourceText);
}

DHUDMessage::~DHUDMessage ()
{
	if (Lines)
	{
		V_FreeBrokenLines (Lines);
		Lines = NULL;
		BorderNeedRefresh = screen->GetPageCount ();
	}
	if (SourceText != NULL)
	{
		delete[] SourceText;
	}
}

void DHUDMessage::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << Left << Top << CenterX << HoldTics
		<< Tics << State << TextColor
		<< SBarID << SourceText << Font << Next;
	if (SaveVersion >= 215)
	{
		arc << HUDWidth << HUDHeight;
	}
	else
	{
		HUDWidth = HUDHeight = 0;
	}
	if (arc.IsLoading ())
	{
		Lines = NULL;
		ResetText (SourceText);
	}
}

void DHUDMessage::ScreenSizeChanged ()
{
	if (HUDWidth == 0)
	{
		ResetText (SourceText);
	}
}

void DHUDMessage::ResetText (const char *text)
{
	FFont *oldfont = screen->Font;
	int width;

	if (HUDWidth != 0)
	{
		width = HUDWidth;
	}
	else
	{
		width = con_scaletext ? SCREENWIDTH / CleanXfac : SCREENWIDTH;
	}

	if (Lines != NULL)
	{
		V_FreeBrokenLines (Lines);
	}

	screen->SetFont (Font);

	Lines = V_BreakLines (width, (byte *)text);

	NumLines = 0;
	Width = 0;
	Height = 0;

	if (Lines)
	{
		for (; Lines[NumLines].width != -1; NumLines++)
		{
			Height += Font->GetHeight ();
			Width = MAX<int> (Width, Lines[NumLines].width);
		}
	}

	screen->SetFont (oldfont);
}

bool DHUDMessage::Tick ()
{
	Tics++;
	if (HoldTics != 0 && HoldTics <= Tics)
	{ // This message has expired
		return true;
	}
	return false;
}

void DHUDMessage::Draw (int bottom)
{
	int xscale, yscale;
	int x, y;
	int ystep;
	int i;
	bool clean = false;
	FFont *oldfont = screen->Font;
	int hudheight;

	screen->SetFont (Font);

	DrawSetup ();

	if (HUDWidth == 0 && con_scaletext)
	{
		clean = true;
		xscale = CleanXfac;
		yscale = CleanYfac;
	}
	else
	{
		xscale = yscale = 1;
	}

	if (HUDWidth == 0)
	{
		if (Left > 0.f)
		{ // Position center
			x = (int)((float)(SCREENWIDTH - Width * xscale) * Left);
		}
		else
		{ // Position edge
			x = (int)((float)SCREENWIDTH * -Left);
		}
		if (Top > 0.f)
		{ // Position center
			y = (int)((float)(bottom - Height * yscale) * Top);
		}
		else
		{ // Position edge
			y = (int)((float)bottom * -Top);
		}
	}
	else
	{
		float intpart;
		int fracpart;

		fracpart = (int)(fabsf (modff (Left, &intpart)) * 10.f + 0.5f);
		x = (int)intpart;
		switch (fracpart & 3)
		{
		case 0: // Position center
			x -= Width / 2;
			break;

		case 2: // Position right
			x -= Width;
			break;
		}

		fracpart = (int)(fabsf (modff (Top, &intpart)) * 10.f + 0.5f);
		y = (int)intpart;
		switch (fracpart & 3)
		{
		case 0: // Position center
			y -= Height / 2;
			break;

		case 2: // Position bottom
			y -= Height;
			break;
		}
	}

	if (CenterX)
	{
		x += Width * xscale / 2;
	}

	ystep = Font->GetHeight() * yscale;

	if (HUDHeight < 0)
	{
		// A negative height means the HUD size covers the status bar
		hudheight = -HUDHeight;
	}
	else
	{
		// A positive height means the HUD size does not cover the status bar
		hudheight = Scale (HUDHeight, SCREENHEIGHT, bottom);
	}

	for (i = 0; i < NumLines; i++)
	{
		int drawx;

		drawx = CenterX ? x - Lines[i].width*xscale/2 : x;
		DoDraw (i, drawx, y, clean, hudheight);
		y += ystep;
	}

	screen->SetFont (oldfont);
}

void DHUDMessage::DrawSetup ()
{
}

void DHUDMessage::DoDraw (int linenum, int x, int y, bool clean, int hudheight)
{
	if (hudheight == 0)
	{
		screen->DrawText (TextColor, x, y, Lines[linenum].string,
			DTA_CleanNoMove, clean,
			TAG_DONE);
	}
	else
	{
		screen->DrawText (TextColor, x, y, Lines[linenum].string,
			DTA_VirtualWidth, HUDWidth,
			DTA_VirtualHeight, hudheight,
			TAG_DONE);
	}
}

//
// HUD message that fades out
//

DHUDMessageFadeOut::DHUDMessageFadeOut (const char *text, float x, float y,
	int hudwidth, int hudheight,									
	EColorRange textColor, float holdTime, float fadeOutTime)
	: DHUDMessage (text, x, y, hudwidth, hudheight, textColor, holdTime)
{
	FadeOutTics = (int)(fadeOutTime * TICRATE);
	State = 1;
}

void DHUDMessageFadeOut::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << FadeOutTics;
}

bool DHUDMessageFadeOut::Tick ()
{
	Tics++;
	if (State == 1 && HoldTics <= Tics)
	{
		State++;
		Tics -= HoldTics;
	}
	if (State == 2 && FadeOutTics <= Tics)
	{
		return true;
	}
	return false;
}

void DHUDMessageFadeOut::DoDraw (int linenum, int x, int y, bool clean, int hudheight)
{
	if (State == 1)
	{
		DHUDMessage::DoDraw (linenum, x, y, clean, hudheight);
	}
	else
	{
		fixed_t trans = -(Tics - FadeOutTics) * FRACUNIT / FadeOutTics;
		if (hudheight == 0)
		{
			screen->DrawText (TextColor, x, y, Lines[linenum].string,
				DTA_CleanNoMove, clean,
				DTA_Alpha, trans,
				TAG_DONE);
		}
		else
		{
			screen->DrawText (TextColor, x, y, Lines[linenum].string,
				DTA_VirtualWidth, HUDWidth,
				DTA_VirtualHeight, hudheight,
				DTA_Alpha, trans,
				TAG_DONE);
		}
		BorderNeedRefresh = screen->GetPageCount ();
	}
}

//
// HUD message that gets "typed" on, then fades out
//

DHUDMessageTypeOnFadeOut::DHUDMessageTypeOnFadeOut (const char *text, float x, float y,
	int hudwidth, int hudheight,
	EColorRange textColor, float typeTime, float holdTime, float fadeOutTime)
	: DHUDMessageFadeOut (text, x, y, hudwidth, hudheight, textColor, holdTime, fadeOutTime)
{
	TypeOnTime = typeTime * TICRATE;
	if (TypeOnTime == 0.f)
		TypeOnTime = 0.1f;
	CurrLine = 0;
	LineLen = (int)strlen (Lines[0].string);
	LineVisible = 0;
	State = 3;
}

void DHUDMessageTypeOnFadeOut::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << TypeOnTime << CurrLine << LineVisible << LineLen;
}

bool DHUDMessageTypeOnFadeOut::Tick ()
{
	if (!Super::Tick ())
	{
		if (State == 3)
		{
			LineVisible = (int)(Tics / TypeOnTime);
			while (LineVisible > LineLen && State == 3)
			{
				LineVisible -= LineLen;
				Tics = (int)(LineVisible * TypeOnTime);
				CurrLine++;
				if (CurrLine >= NumLines)
				{
					State = 1;
				}
				else
				{
					LineLen = (int)strlen (Lines[CurrLine].string);
				}
			}
		}
		return false;
	}
	return true;
}

void DHUDMessageTypeOnFadeOut::ScreenSizeChanged ()
{
	int charCount = 0, i;

	for (i = 0; i < CurrLine; ++i)
	{
		charCount += (int)strlen (Lines[i].string);
	}
	charCount += LineVisible;

	Super::ScreenSizeChanged ();
	if (State == 3)
	{
		CurrLine = 0;
		LineLen = (int)strlen (Lines[0].string);
		Tics = (int)(charCount * TypeOnTime) - 1;
		Tick ();
	}
}

void DHUDMessageTypeOnFadeOut::DoDraw (int linenum, int x, int y, bool clean, int hudheight)
{
	if (State == 3)
	{
		if (linenum < CurrLine)
		{
			DHUDMessage::DoDraw (linenum, x, y, clean, hudheight);
		}
		else if (linenum == CurrLine)
		{
			char stop;

			stop = Lines[linenum].string[LineVisible];
			Lines[linenum].string[LineVisible] = 0;
			if (hudheight == 0)
			{
				screen->DrawText (TextColor, x, y, Lines[linenum].string,
					DTA_CleanNoMove, clean,
					TAG_DONE);
			}
			else
			{
				screen->DrawText (TextColor, x, y, Lines[linenum].string,
					DTA_VirtualWidth, HUDWidth,
					DTA_VirtualHeight, hudheight,
					TAG_DONE);
			}
			Lines[linenum].string[LineVisible] = stop;
		}
	}
	else
	{
		DHUDMessageFadeOut::DoDraw (linenum, x, y, clean, hudheight);
	}
}

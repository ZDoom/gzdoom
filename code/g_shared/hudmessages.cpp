#include "doomdef.h"
#include "sbar.h"
#include "c_cvars.h"
#include "v_video.h"

EXTERN_CVAR (Bool, con_scaletext)

//
// Basic HUD message. Appears and disappears without any special FX
//

FHUDMessage::FHUDMessage (const char *text, float x, float y, EColorRange textColor,
						  float holdTime)
{
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
	Top = y;
	Next = NULL;
	Lines = NULL;
	HoldTics = (int)(holdTime * TICRATE);
	Tics = 0;
	TextColor = textColor;
	State = 0;

	ResetText (text);
}

FHUDMessage::~FHUDMessage ()
{
	if (Lines)
	{
		V_FreeBrokenLines (Lines);
		Lines = NULL;
		BorderNeedRefresh = screen->GetPageCount ();
	}
}

void FHUDMessage::ResetText (const char *text)
{
	Lines = V_BreakLines (*con_scaletext ?
		SCREENWIDTH / CleanXfac : SCREENWIDTH, (byte *)text);

	NumLines = 0;
	Width = 0;
	Height = 0;

	if (Lines)
	{
		for (; Lines[NumLines].width != -1; NumLines++)
		{
			Height += SmallFont->GetHeight ();
			Width = MAX (Width, Lines[NumLines].width);
		}
	}
}

bool FHUDMessage::Tick ()
{
	Tics++;
	if (HoldTics != 0 && HoldTics <= Tics)
	{ // This message has expired
		return true;
	}
	return false;
}

void FHUDMessage::Draw (int bottom)
{
	int xscale, yscale;
	int x, y;
	int ystep;
	int i;
	bool clean;

	DrawSetup ();

	if ( (clean = (*con_scaletext != 0.f)) )
	{
		xscale = CleanXfac;
		yscale = CleanYfac;
	}
	else
	{
		xscale = yscale = 1;
	}

	if (Left > 0.f)
	{
		x = (int)((float)(SCREENWIDTH - Width * xscale) * Left);
	}
	else
	{
		x = (int)((float)SCREENWIDTH * -Left);
	}
	if (CenterX)
	{
		x += Width * xscale / 2;
	}

	if (Top > 0.f)
	{
		y = (int)((float)(bottom - Height * yscale) * Top);
	}
	else
	{
		y = (int)((float)bottom * -Top);
	}

	ystep = SmallFont->GetHeight() * yscale;

	for (i = 0; i < NumLines; i++)
	{
		int drawx;

		drawx = CenterX ? x - Lines[i].width*xscale/2 : x;
		DoDraw (i, drawx, y, xscale, yscale, clean);
		y += ystep;
	}
}

void FHUDMessage::DrawSetup ()
{
}

void FHUDMessage::DoDraw (int linenum, int x, int y, int xscale, int yscale,
						  bool clean)
{
	if (clean)
	{
		screen->DrawTextClean (TextColor, x, y, Lines[linenum].string);
	}
	else
	{
		screen->DrawText (TextColor, x, y, Lines[linenum].string);
	}
}

//
// HUD message that fades out
//

FHUDMessageFadeOut::FHUDMessageFadeOut (const char *text, float x, float y,
	EColorRange textColor, float holdTime, float fadeOutTime)
	: FHUDMessage (text, x, y, textColor, holdTime)
{
	FadeOutTics = (int)(fadeOutTime * TICRATE);
	State = 1;
}

bool FHUDMessageFadeOut::Tick ()
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

void FHUDMessageFadeOut::DoDraw (int linenum, int x, int y, int xscale,
								 int yscale, bool clean)
{
	if (State == 1)
	{
		FHUDMessage::DoDraw (linenum, x, y, xscale, yscale, clean);
	}
	else
	{
		fixed_t trans = -(Tics - FadeOutTics) * FRACUNIT / FadeOutTics;
		if (clean)
		{
			screen->DrawTextCleanLuc (TextColor, x, y, Lines[linenum].string,
				trans);
		}
		else
		{
			screen->DrawTextLuc (TextColor, x, y, Lines[linenum].string,
				trans);
		}
		BorderNeedRefresh = screen->GetPageCount ();
	}
}

//
// HUD message that gets "typed" on, then fades out
//

FHUDMessageTypeOnFadeOut::FHUDMessageTypeOnFadeOut (const char *text, float x, float y,
	EColorRange textColor, float typeTime, float holdTime, float fadeOutTime)
	: FHUDMessageFadeOut (text, x, y, textColor, holdTime, fadeOutTime)
{
	TypeOnTime = typeTime * TICRATE;
	if (TypeOnTime == 0.f)
		TypeOnTime = 0.1f;
	CurrLine = 0;
	LineLen = strlen (Lines[0].string);
	LineVisible = 0;
	State = 3;
}

bool FHUDMessageTypeOnFadeOut::Tick ()
{
	if (!FHUDMessageFadeOut::Tick ())
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
					LineLen = strlen (Lines[CurrLine].string);
				}
			}
		}
		return false;
	}
	return true;
}

void FHUDMessageTypeOnFadeOut::DoDraw (int linenum, int x, int y, int xscale,
								 int yscale, bool clean)
{
	if (State == 3)
	{
		if (linenum < CurrLine)
		{
			FHUDMessage::DoDraw (linenum, x, y, xscale, yscale, clean);
		}
		else if (linenum == CurrLine)
		{
			char stop;

			stop = Lines[linenum].string[LineVisible];
			Lines[linenum].string[LineVisible] = 0;
			if (clean)
			{
				screen->DrawTextClean (TextColor, x, y, Lines[linenum].string);
			}
			else
			{
				screen->DrawText (TextColor, x, y, Lines[linenum].string);
			}
			Lines[linenum].string[LineVisible] = stop;
		}
	}
	else
	{
		FHUDMessageFadeOut::DoDraw (linenum, x, y, xscale, yscale, clean);
	}
}

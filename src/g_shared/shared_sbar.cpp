/*
** shared_sbar.cpp
** Base status bar implementation
**
**---------------------------------------------------------------------------
** Copyright 1998-2003 Randy Heit
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
#include "sbar.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "v_video.h"
#include "m_swap.h"
#include "r_draw.h"
#include "w_wad.h"
#include "v_text.h"
#include "s_sound.h"
#include "gi.h"
#include "p_effect.h"
#include "../version.h"

#define XHAIRSHRINKSIZE		(FRACUNIT/18)
#define XHAIRPICKUPSIZE		(FRACUNIT*2+XHAIRSHRINKSIZE)

EXTERN_CVAR (Bool, am_showmonsters)
EXTERN_CVAR (Bool, am_showsecrets)
EXTERN_CVAR (Bool, am_showitems)
EXTERN_CVAR (Bool, am_showtime)
EXTERN_CVAR (Bool, noisedebug)

FBaseStatusBar *StatusBar;
CVAR (Bool, hud_scale, false, CVAR_ARCHIVE);

int ST_X, ST_Y;
int SB_state = 3;

static FTexture *CrosshairImage;

// [RH] Base blending values (for e.g. underwater)
int BaseBlendR, BaseBlendG, BaseBlendB;
float BaseBlendA;

// Stretch status bar to full screen width?
CUSTOM_CVAR (Bool, st_scale, true, CVAR_ARCHIVE)
{
	if (StatusBar)
	{
		StatusBar->SetScaled (self);
		setsizeneeded = true;
	}
}

CUSTOM_CVAR (Int, crosshair, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	int num = self;
	char name[16], size;
	int lump;

	if (CrosshairImage != NULL)
	{
		CrosshairImage->Unload ();
	}
	if (num == 0)
	{
		CrosshairImage = NULL;
		return;
	}
	if (num < 0)
	{
		num = -num;
	}
	size = (SCREENWIDTH < 640) ? 'S' : 'B';
	sprintf (name, "XHAIR%c%d", size, num);
	if ((lump = Wads.CheckNumForName (name)) == -1)
	{
		sprintf (name, "XHAIR%c1", size);
		if ((lump = Wads.CheckNumForName (name)) == -1)
		{
			strcpy (name, "XHAIRS1");
		}
	}
	CrosshairImage = TexMan[TexMan.AddPatch (name)];
}

CVAR (Color, crosshaircolor, 0xff0000, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
CVAR (Bool, crosshairhealth, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
CVAR (Bool, crosshairscale, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
CVAR (Bool, crosshairgrow, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);

CVAR (Bool, idmypos, false, 0);

// [RH] Amount of red flash for up to 114 damage points. Calculated by hand
//		using a logarithmic scale and my trusty HP48G.
byte FBaseStatusBar::DamageToAlpha[114] =
{
	  0,   8,  16,  23,  30,  36,  42,  47,  53,  58,  62,  67,  71,  75,  79,
	 83,  87,  90,  94,  97, 100, 103, 107, 109, 112, 115, 118, 120, 123, 125,
	128, 130, 133, 135, 137, 139, 141, 143, 145, 147, 149, 151, 153, 155, 157,
	159, 160, 162, 164, 165, 167, 169, 170, 172, 173, 175, 176, 178, 179, 181,
	182, 183, 185, 186, 187, 189, 190, 191, 192, 194, 195, 196, 197, 198, 200,
	201, 202, 203, 204, 205, 206, 207, 209, 210, 211, 212, 213, 214, 215, 216,
	217, 218, 219, 220, 221, 221, 222, 223, 224, 225, 226, 227, 228, 229, 229,
	230, 231, 232, 233, 234, 235, 235, 236, 237
};

//---------------------------------------------------------------------------
//
// Constructor
//
//---------------------------------------------------------------------------

FBaseStatusBar::FBaseStatusBar (int reltop)
{
	Centering = false;
	FixedOrigin = false;
	CrosshairSize = FRACUNIT;
	RelTop = reltop;
	Messages = NULL;
	Displacement = 0;

	SetScaled (st_scale);
	AmmoImages.Init (AmmoPics, MANA_BOTH+1);
	ArtiImages.Init (ArtiPics, NUMARTIFACTS);
	ArmorImages.Init (ArmorPics, NUMARMOR);
}

//---------------------------------------------------------------------------
//
// Destructor
//
//---------------------------------------------------------------------------

FBaseStatusBar::~FBaseStatusBar ()
{
	DHUDMessage *msg;

	msg = Messages;
	while (msg)
	{
		DHUDMessage *next = msg->Next;
		delete msg;
		msg = next;
	}
}

//---------------------------------------------------------------------------
//
// PROC SetScaled
//
//---------------------------------------------------------------------------

void FBaseStatusBar::SetScaled (bool scale)
{
	Scaled = RelTop != 0 && (SCREENWIDTH != 320 && scale);
	if (!Scaled)
	{
		ST_X = (SCREENWIDTH - 320) / 2;
		ST_Y = SCREENHEIGHT - RelTop;
		::ST_Y = ST_Y;
		if (RelTop > 0)
		{
			Displacement = ((ST_Y * 200 / SCREENHEIGHT) - (200 - RelTop))*FRACUNIT/RelTop;
		}
		else
		{
			Displacement = 0;
		}
	}
	else
	{
		ST_X = 0;
		ST_Y = 200 - RelTop;
		::ST_Y = Scale (ST_Y, SCREENHEIGHT, 200);
		ScaleX = (SCREENWIDTH << FRACBITS) / 320;
		ScaleY = (SCREENHEIGHT << FRACBITS) / 200;
		ScaleIX = (320 << FRACBITS) / SCREENWIDTH;
		ScaleIY = (200 << FRACBITS) / SCREENHEIGHT;
		Displacement = 0;
	}
	::ST_X = ST_X;
	SB_state = screen->GetPageCount ();
}

//---------------------------------------------------------------------------
//
// PROC AttachToPlayer
//
//---------------------------------------------------------------------------

void FBaseStatusBar::AttachToPlayer (player_s *player)
{
	CPlayer = player;
	SB_state = screen->GetPageCount ();
}

//---------------------------------------------------------------------------
//
// PROC MultiplayerChanged
//
//---------------------------------------------------------------------------

void FBaseStatusBar::MultiplayerChanged ()
{
	SB_state = screen->GetPageCount ();
}

//---------------------------------------------------------------------------
//
// PROC Tick
//
//---------------------------------------------------------------------------

void FBaseStatusBar::Tick ()
{
	DHUDMessage *msg = Messages;
	DHUDMessage **prev = &Messages;

	while (msg)
	{
		DHUDMessage *next = msg->Next;

		if (msg->Tick ())
		{
			*prev = next;
			delete msg;
		}
		else
		{
			prev = &msg->Next;
		}
		msg = next;
	}
}

//---------------------------------------------------------------------------
//
// PROC AttachMessage
//
//---------------------------------------------------------------------------

void FBaseStatusBar::AttachMessage (DHUDMessage *msg, DWORD id)
{
	DHUDMessage *old = NULL;
	DHUDMessage **prev;

	old = (id == 0 || id == 0xFFFFFFFF) ? NULL : DetachMessage (id);
	if (old != NULL)
	{
		delete old;
	}

	prev = &Messages;

	// The ID serves as a priority, where lower numbers appear in front of
	// higher numbers. (i.e. The list is sorted in descending order, since
	// it gets drawn back to front.)
	while (*prev != NULL && (*prev)->SBarID > id)
	{
		prev = &(*prev)->Next;
	}

	msg->Next = *prev;
	msg->SBarID = id;
	*prev = msg;
}

//---------------------------------------------------------------------------
//
// PROC DetachMessage
//
//---------------------------------------------------------------------------

DHUDMessage *FBaseStatusBar::DetachMessage (DHUDMessage *msg)
{
	DHUDMessage *probe = Messages;
	DHUDMessage **prev = &Messages;

	while (probe && probe != msg)
	{
		prev = &probe->Next;
		probe = probe->Next;
	}
	if (probe != NULL)
	{
		*prev = probe->Next;
		probe->Next = NULL;
		// Redraw the status bar in case it was covered
		SB_state = screen->GetPageCount ();
	}
	return probe;
}

DHUDMessage *FBaseStatusBar::DetachMessage (DWORD id)
{
	DHUDMessage *probe = Messages;
	DHUDMessage **prev = &Messages;

	while (probe && probe->SBarID != id)
	{
		prev = &probe->Next;
		probe = probe->Next;
	}
	if (probe != NULL)
	{
		*prev = probe->Next;
		probe->Next = NULL;
		// Redraw the status bar in case it was covered
		SB_state = screen->GetPageCount ();
	}
	return probe;
}

//---------------------------------------------------------------------------
//
// PROC DetachAllMessages
//
//---------------------------------------------------------------------------

void FBaseStatusBar::DetachAllMessages ()
{
	DHUDMessage *probe = Messages;

	Messages = NULL;
	while (probe != NULL)
	{
		DHUDMessage *next = probe->Next;
		delete probe;
		probe = next;
	}
}

//---------------------------------------------------------------------------
//
// PROC CheckMessage
//
//---------------------------------------------------------------------------

bool FBaseStatusBar::CheckMessage (DHUDMessage *msg)
{
	DHUDMessage *probe = Messages;
	while (probe && probe != msg)
	{
		probe = probe->Next;
	}
	return (probe == msg);
}

//---------------------------------------------------------------------------
//
// PROC ShowPlayerName
//
//---------------------------------------------------------------------------

void FBaseStatusBar::ShowPlayerName ()
{
	EColorRange color;

	color = (CPlayer == &players[consoleplayer]) ? CR_GOLD : CR_GREEN;
	AttachMessage (new DHUDMessageFadeOut (CPlayer->userinfo.netname,
		1.5f, 0.92f, 0, 0, color, 2.f, 0.35f), 'PNAM');
}

//---------------------------------------------------------------------------
//
// PROC DrawImage
//
// Draws an image with the status bar's upper-left corner as the origin.
//
//---------------------------------------------------------------------------

void FBaseStatusBar::DrawImage (FTexture *img,
	int x, int y, byte *translation) const
{
	if (img != NULL)
	{
		screen->DrawTexture (img, x + ST_X, y + ST_Y,
			DTA_Translation, translation,
			DTA_320x200, Scaled,
			TAG_DONE);
	}
}

//---------------------------------------------------------------------------
//
// PROC DrawImage
//
// Draws a translucent image with the status bar's upper-left corner as the
// origin.
//
//---------------------------------------------------------------------------

void FBaseStatusBar::DrawFadedImage (FTexture *img,
	int x, int y, fixed_t shade) const
{
	if (img != NULL)
	{
		screen->DrawTexture (img, x + ST_X, y + ST_Y,
			DTA_Alpha, shade,
			DTA_320x200, Scaled,
			TAG_DONE);
	}
}

//---------------------------------------------------------------------------
//
// PROC DrawPartialImage
//
// Draws a portion of an image with the status bar's upper-left corner as
// the origin. The image should be the same size as the status bar.
// Used for Doom's status bar.
//
//---------------------------------------------------------------------------

void FBaseStatusBar::DrawPartialImage (FTexture *img, int wx, int ww) const
{
	if (img != NULL)
	{
		screen->DrawTexture (img, ST_X, ST_Y,
			DTA_WindowLeft, wx,
			DTA_WindowRight, wx + ww,
			DTA_320x200, Scaled,
			TAG_DONE);
	}
}

//---------------------------------------------------------------------------
//
// PROC RepositionCoords
//
// Repositions coordinates for the outside status bar drawers. Returns
// true if the image should be stretched.
//
//---------------------------------------------------------------------------

bool FBaseStatusBar::RepositionCoords (int &x, int &y, int xo, int yo,
	const int w, const int h) const
{
	if (FixedOrigin)
	{
		x += xo - w / 2;
		y += yo - h;
	}
	if (hud_scale)
	{
		x *= CleanXfac;
		if (Centering)
			x += SCREENWIDTH / 2;
		else if (x < 0)
			x = SCREENWIDTH + x;
		y *= CleanYfac;
		if (y < 0)
			y = SCREENHEIGHT + y;
		return true;
	}
	else
	{
		if (Centering)
			x += SCREENWIDTH / 2;
		else if (x < 0)
			x = SCREENWIDTH + x;
		if (y < 0)
			y = SCREENHEIGHT + y;
		return false;
	}
}

//---------------------------------------------------------------------------
//
// PROC DrawOuterFadedImage
//
// Draws a translucent image outside the status bar, possibly scaled.
//
//---------------------------------------------------------------------------

void FBaseStatusBar::DrawOuterFadedImage (FTexture *img, int x, int y, fixed_t shade) const
{
	if (img != NULL)
	{
		int w = img->GetWidth ();
		int h = img->GetHeight ();
		bool cnm = RepositionCoords (x, y, img->LeftOffset, img->TopOffset, w, h);

		screen->DrawTexture (img, x, y, DTA_Alpha, shade, DTA_CleanNoMove, cnm, TAG_DONE);
	}
}

//---------------------------------------------------------------------------
//
// PROC DrawShadowedImage
//
// Draws a shadowed image outside the status bar, possibly scaled.
//
//---------------------------------------------------------------------------

void FBaseStatusBar::DrawShadowedImage (FTexture *img, int x, int y, fixed_t shade) const
{
	if (img != NULL)
	{
		int w = img->GetWidth ();
		int h = img->GetHeight ();
		bool cnm = RepositionCoords (x, y, img->LeftOffset, img->TopOffset, w, h);

		screen->DrawTexture (img, x, y,
			DTA_CleanNoMove, cnm,
			DTA_ShadowAlpha, shade,
			DTA_ShadowColor, 0,
			TAG_DONE);
	}
}

//---------------------------------------------------------------------------
//
// PROC DrawOuterImage
//
// Draws an image outside the status bar, possibly scaled.
//
//---------------------------------------------------------------------------

void FBaseStatusBar::DrawOuterImage (FTexture *img, int x, int y) const
{
	if (img != NULL)
	{
		int w = img->GetWidth ();
		int h = img->GetHeight ();
		bool cnm = RepositionCoords (x, y, img->LeftOffset, img->TopOffset, w, h);

		screen->DrawTexture (img, x, y, DTA_CleanNoMove, cnm, TAG_DONE);
	}
}

//---------------------------------------------------------------------------
//
// PROC DrawOuterTexture
//
// Draws a texture outside the status bar, possibly scaled.
//
//---------------------------------------------------------------------------

void FBaseStatusBar::DrawOuterTexture (FTexture *tex, int x, int y) const
{
	if (tex)
	{
		bool cnm = RepositionCoords (x, y, 0, 0, 0, 0);

		screen->DrawTexture (tex, x, y, DTA_CleanNoMove, cnm, TAG_DONE);
	}
}

//---------------------------------------------------------------------------
//
// PROC DrINumber
//
// Draws a three digit number.
//
//---------------------------------------------------------------------------

void FBaseStatusBar::DrINumber (signed int val, int x, int y, int imgBase) const
{
	int oldval;

	if (val > 999)
		val = 999;
	oldval = val;
	if (val < 0)
	{
		if (val < -9)
		{
			DrawImage (Images[imgLAME], x+1, y+1);
			return;
		}
		val = -val;
		DrawImage (Images[imgBase+val], x+18, y);
		DrawImage (Images[imgNEGATIVE], x+9, y);
		return;
	}
	if (val > 99)
	{
		DrawImage (Images[imgBase+val/100], x, y);
	}
	val = val % 100;
	if (val > 9 || oldval > 99)
	{
		DrawImage (Images[imgBase+val/10], x+9, y);
	}
	val = val % 10;
	DrawImage (Images[imgBase+val], x+18, y);
}

//---------------------------------------------------------------------------
//
// PROC DrBNumber
//
// Draws an x digit number using the big font.
//
//---------------------------------------------------------------------------

void FBaseStatusBar::DrBNumber (signed int val, int x, int y, int size) const
{
	int xpos;
	int index;
	int w, h;
	bool neg;
	int i;
	int power;
	FTexture *pic = Images[imgBNumbers+3];

	if (pic != NULL)
	{
		w = pic->GetWidth ();
		h = pic->GetHeight ();
	}
	else
	{
		w = h = 0;
	}

	xpos = x + w/2 + w*size;

	for (i = size-1, power = 10; i > 0; i--)
	{
		power *= 10;
	}

	if (val >= power)
	{
		val = power - 1;
	}
	if ( (neg = val < 0) )
	{
		if (size == 2 && val < -9)
		{
			val = -9;
		}
		else if (size == 3 && val < -99)
		{
			val = -99;
		}
		val = -val;
		size--;
	}
	if (val == 0)
	{
		pic = Images[imgBNumbers];
		DrawImage (pic, xpos - pic->GetWidth()/2 - w, y);
		return;
	}
	while (val != 0 && size--)
	{
		xpos -= w;
		int oldval = val;
		val /= 10;
		index = imgBNumbers + (oldval - val*10);
		pic = Images[index];
		DrawImage (pic, xpos - pic->GetWidth()/2, y);
	}
	if (neg)
	{
		xpos -= w;
		pic = Images[imgBNEGATIVE];
		DrawImage (pic, xpos - pic->GetWidth()/2, y);
	}
}

//---------------------------------------------------------------------------
//
// PROC DrSmallNumber
//
// Draws a small three digit number.
//
//---------------------------------------------------------------------------

void FBaseStatusBar::DrSmallNumber (int val, int x, int y) const
{
	int digit = 0;

	if (val > 999)
	{
		val = 999;
	}
	if (val > 99)
	{
		digit = val / 100;
		DrawImage (Images[imgSmNumbers + digit], x, y);
		val -= digit * 100;
	}
	if (val > 9 || digit)
	{
		digit = val / 10;
		DrawImage (Images[imgSmNumbers + digit], x+4, y);
		val -= digit * 10;
	}
	DrawImage (Images[imgSmNumbers + val], x+8, y);
}

//---------------------------------------------------------------------------
//
// PROC DrINumberOuter
//
// Draws a three digit number outside the status bar, possibly scaled.
//
//---------------------------------------------------------------------------

void FBaseStatusBar::DrINumberOuter (signed int val, int x, int y) const
{
	int oldval;

	if (val > 999)
	{
		val = 999;
	}
	oldval = val;
	if (val < 0)
	{
		if (val < -9)
		{
			DrawOuterImage (Images[imgLAME], x+1, y+1);
			return;
		}
		val = -val;
		DrawOuterImage (Images[imgINumbers+val], x+18, y);
		DrawOuterImage (Images[imgNEGATIVE], x+9, y);
		return;
	}
	if (val > 99)
	{
		DrawOuterImage (Images[imgINumbers+val/100], x, y);
	}
	val = val % 100;
	if (val > 9 || oldval > 99)
	{
		DrawOuterImage (Images[imgINumbers+val/10], x+9, y);
	}
	val = val % 10;
	DrawOuterImage (Images[imgINumbers+val], x+18, y);
}

//---------------------------------------------------------------------------
//
// PROC DrBNumberOuter
//
// Draws a three digit number using the big font outside the status bar.
//
//---------------------------------------------------------------------------

void FBaseStatusBar::DrBNumberOuter (signed int val, int x, int y, int size) const
{
	int xpos;
	int i;
	int w;
	int div;
	bool neg;
	int first;
	FTexture *pic;

	pic = Images[imgBNumbers+3];
	if (pic != NULL)
	{
		w = pic->GetWidth();
	}
	else
	{
		w = 0;
	}

	div = 1;
	for (i = size-1; i>0; i--)
		div *= 10;

	xpos = x + w/2;

	if (val == 0)
	{
		pic = Images[imgBNumbers];
		DrawShadowedImage (pic, xpos - pic->GetWidth()/2 + w*(size-1), y, HR_SHADOW);
		return;
	}

	if (val >= div*10)
	{
		val = div*10 - 1;
	}
	if ( (neg = (val < 0)) )
	{
		if (val <= -div)
		{
			val = -div + 1;
		}
		val = -val;
		size--;
		div /= 10;
	}
	first = -99999;
	while (size--)
	{
		i = val / div;

		if (i != 0 || first != -99999)
		{
			if (first == -99999)
			{
				first = xpos;
			}
			pic = Images[i + imgBNumbers];
			DrawShadowedImage (pic, xpos - pic->GetWidth()/2, y, HR_SHADOW);
			val -= i * div;
		}
		div /= 10;
		xpos += w;
	}
	if (neg)
	{
		pic = Images[imgBNEGATIVE];
		DrawShadowedImage (pic, first - pic->GetWidth()/2 - w, y, HR_SHADOW);
	}
}

//---------------------------------------------------------------------------
//
// PROC DrSmallNumberOuter
//
// Draws a small three digit number outside the status bar.
//
//---------------------------------------------------------------------------

void FBaseStatusBar::DrSmallNumberOuter (int val, int x, int y) const
{
	int digit = 0;

	if (val > 999)
	{
		val = 999;
	}
	if (val > 99)
	{
		digit = val / 100;
		DrawOuterImage (Images[imgSmNumbers + digit], x, y);
		val -= digit * 100;
	}
	if (val > 9 || digit)
	{
		digit = val / 10;
		DrawOuterImage (Images[imgSmNumbers + digit], x+4, y);
		val -= digit * 10;
	}
	DrawOuterImage (Images[imgSmNumbers + val], x+8, y);
}

//---------------------------------------------------------------------------
//
// RefreshBackground
//
//---------------------------------------------------------------------------

void FBaseStatusBar::RefreshBackground () const
{
	if (SCREENWIDTH > 320)
	{
		R_DrawBorder (0, ST_Y, ST_X, SCREENHEIGHT);
		R_DrawBorder (SCREENWIDTH - ST_X, ST_Y, SCREENWIDTH, SCREENHEIGHT);
	}
}

//---------------------------------------------------------------------------
//
// DrawCrosshair
//
//---------------------------------------------------------------------------

void FBaseStatusBar::DrawCrosshair ()
{
	static DWORD prevcolor = 0xffffffff;
	static int palettecolor = 0;

	DWORD color;
	fixed_t size;
	int w, h;

	if (CrosshairSize > FRACUNIT)
	{
		CrosshairSize -= XHAIRSHRINKSIZE;
		if (CrosshairSize < FRACUNIT)
		{
			CrosshairSize = FRACUNIT;
		}
	}

	// Don't draw the crosshair in chasecam mode
	if (players[consoleplayer].cheats & CF_CHASECAM)
		return;

	// Don't draw the crosshair if there is none
	if (CrosshairImage == NULL)
	{
		return;
	}

	if (crosshairscale)
	{
		size = SCREENWIDTH * FRACUNIT / 320;
	}
	else
	{
		size = FRACUNIT;
	}

	if (crosshairgrow)
	{
		size = FixedMul (size, CrosshairSize);
	}
	w = (CrosshairImage->GetWidth() * size) >> FRACBITS;
	h = (CrosshairImage->GetHeight() * size) >> FRACBITS;

	if (crosshairhealth)
	{
		int health = CPlayer->health;

		if (health >= 85)
		{
			color = 0x00ff00;
		}
		else 
		{
			int red, green;
			health -= 25;
			if (health < 0)
			{
				health = 0;
			}
			if (health < 30)
			{
				red = 255;
				green = health * 255 / 30;
			}
			else
			{
				red = (60 - health) * 255 / 30;
				green = 255;
			}
			color = (red<<16) | (green<<8);
		}
	}
	else
	{
		color = crosshaircolor;
	}

	if (color != prevcolor)
	{
		prevcolor = color;
		palettecolor = ColorMatcher.Pick (RPART(color), GPART(color), BPART(color));
	}

	screen->DrawTexture (CrosshairImage,
		realviewwidth / 2 + viewwindowx,
		realviewheight / 2 + viewwindowy,
		DTA_DestWidth, w,
		DTA_DestHeight, h,
		DTA_AlphaChannel, true,
		DTA_FillColor, palettecolor,
		TAG_DONE);
}

//---------------------------------------------------------------------------
//
// FlashCrosshair
//
//---------------------------------------------------------------------------

void FBaseStatusBar::FlashCrosshair ()
{
	CrosshairSize = XHAIRPICKUPSIZE;
}

//---------------------------------------------------------------------------
//
// DrawMessages
//
//---------------------------------------------------------------------------

void FBaseStatusBar::DrawMessages (int bottom) const
{
	DHUDMessage *msg = Messages;
	while (msg)
	{
		msg->Draw (bottom);
		msg = msg->Next;
	}
}

//---------------------------------------------------------------------------
//
// Draw
//
//---------------------------------------------------------------------------

void FBaseStatusBar::Draw (EHudState state)
{
	float blend[4];
	char line[64+10];

	blend[0] = blend[1] = blend[2] = blend[3] = 0;
	BlendView (blend);

	if (SB_state != 0 && state == HUD_StatusBar && !Scaled)
	{
		RefreshBackground ();
	}

	if (idmypos)
	{ // Draw current coordinates
		int height = screen->Font->GetHeight();
		int y = ::ST_Y - height;
		char labels[3] = { 'X', 'Y', 'Z' };
		fixed_t *value;
		int i;

		value = &CPlayer->mo->z;
		for (i = 2, value = &CPlayer->mo->z; i >= 0; y -= height, --value, --i)
		{
			sprintf (line, "%c: %ld", labels[i], *value >> FRACBITS);
			screen->DrawText (CR_GREEN, SCREENWIDTH - 80, y, line, TAG_DONE);
			BorderNeedRefresh = screen->GetPageCount();
		}
	}

	if (viewactive)
	{
		if (CPlayer && CPlayer->camera->player)
		{
			DrawCrosshair ();
		}
	}
	else if (automapactive)
	{
		int y, i, time = level.time / TICRATE, height;
		EColorRange highlight = (gameinfo.gametype == GAME_Doom) ?
			CR_UNTRANSLATED : CR_YELLOW;

		height = screen->Font->GetHeight () * CleanYfac;

		// Draw timer
		if (am_showtime)
		{
			sprintf (line, "%02d:%02d:%02d", time/3600, (time%3600)/60, time%60);	// Time
			screen->DrawText (CR_GREY, SCREENWIDTH - 80*CleanXfac, 8, line, DTA_CleanNoMove, true, TAG_DONE);
		}

		// Draw map name
		y = ::ST_Y - height;
		if (gameinfo.gametype == GAME_Heretic && SCREENWIDTH > 320 && !Scaled)
		{
			y -= 8;
		}
		else if (gameinfo.gametype == GAME_Hexen)
		{
			if (Scaled)
			{
				y -= Scale (10, SCREENHEIGHT, 200);
			}
			else
			{
				if (SCREENWIDTH < 640)
				{
					y -= 11;
				}
				else
				{ // Get past the tops of the gargoyles' wings
					y -= 26;
				}
			}
		}
		else if (gameinfo.gametype == GAME_Strife)
		{
			if (Scaled)
			{
				y -= Scale (8, SCREENHEIGHT, 200);
			}
			else
			{
				y -= 8;
			}
		}
		cluster_info_t *cluster = FindClusterInfo (level.cluster);
		i = 0;
		if (cluster == NULL || !(cluster->flags & CLUSTER_HUB))
		{
			i = sprintf (line, "%s: ", level.mapname);
		}
		line[i] = TEXTCOLOR_ESCAPE;
		line[i+1] = CR_GREY + 'A';
		strcpy (&line[i+2], level.level_name);
		screen->DrawText (highlight,
			(SCREENWIDTH - SmallFont->StringWidth (line)*CleanXfac)/2, y, line,
			DTA_CleanNoMove, true, TAG_DONE);

		if (!deathmatch)
		{
			int y = 8;

			// Draw monster count
			if (am_showmonsters)
			{
				sprintf (line, "MONSTERS:"
							   TEXTCOLOR_GREY " %d/%d",
							   level.killed_monsters, level.total_monsters);
				screen->DrawText (highlight, 8, y, line,
					DTA_CleanNoMove, true, TAG_DONE);
				y += height;
			}

			// Draw secret count
			if (am_showsecrets)
			{
				sprintf (line, "SECRETS:"
							   TEXTCOLOR_GREY " %d/%d",
							   level.found_secrets, level.total_secrets);
				screen->DrawText (highlight, 8, y, line,
					DTA_CleanNoMove, true, TAG_DONE);
				y += height;
			}

			// Draw item count
			if (am_showitems)
			{
				sprintf (line, "ITEMS:"
							   TEXTCOLOR_GREY " %d/%d",
							   level.found_items, level.total_items);
				screen->DrawText (highlight, 8, y, line,
					DTA_CleanNoMove, true, TAG_DONE);
			}
		}
	}

	if (noisedebug)
	{
		S_NoiseDebug ();
	}

	if (demoplayback && demover != GAMEVER)
	{
		screen->DrawText (CR_TAN, 0, ST_Y - 40 * CleanYfac,
			"Demo was recorded with a different version\n"
			"of ZDoom. Expect it to go out of sync.",
			DTA_CleanNoMove, true, TAG_DONE);
	}

	if (state == HUD_StatusBar)
	{
		DrawMessages (::ST_Y);
	}
	else
	{
		DrawMessages (SCREENHEIGHT);
	}

	DrawConsistancy ();
}

/*
=============
SV_AddBlend
[RH] This is from Q2.
=============
*/
void FBaseStatusBar::AddBlend (float r, float g, float b, float a, float v_blend[4])
{
	float a2, a3;

	if (a <= 0)
		return;
	a2 = v_blend[3] + (1-v_blend[3])*a;	// new total alpha
	a3 = v_blend[3]/a2;		// fraction of color from old

	v_blend[0] = v_blend[0]*a3 + r*(1-a3);
	v_blend[1] = v_blend[1]*a3 + g*(1-a3);
	v_blend[2] = v_blend[2]*a3 + b*(1-a3);
	v_blend[3] = a2;
}

//---------------------------------------------------------------------------
//
// BlendView
//
//---------------------------------------------------------------------------

void FBaseStatusBar::BlendView (float blend[4])
{
	int cnt, i;

	AddBlend (BaseBlendR / 255.f, BaseBlendG / 255.f, BaseBlendB / 255.f, BaseBlendA, blend);

	// [RH] All powerups can effect the screen blending now
	for (i = 0; i < NUMPOWERS; ++i)
	{
		if (i == pw_invulnerability && (CPlayer->mo->effects & FX_RESPAWNINVUL))
		{ // Don't show effect if it's just temporary from respawning
			continue;
		}
		if (i == pw_strength)
		{ // berserk is different
			continue;
		}
		if (CPlayer->powers[i] <= 0 ||
			(CPlayer->powers[i] <= 4*32 && !(CPlayer->powers[i]&8)))
		{
			continue;
		}

		int a = APART(PowerupColors[i]);

		if (a == 0)
		{
			continue;
		}

		AddBlend (RPART(PowerupColors[i])/255.f,
			GPART(PowerupColors[i])/255.f,
			BPART(PowerupColors[i])/255.f,
			a/255.f, blend);
	}

	if (CPlayer->bonuscount)
	{
		cnt = CPlayer->bonuscount << 3;
		AddBlend (0.8431f, 0.7333f, 0.2706f, cnt > 128 ? 0.5f : cnt / 255.f, blend);
	}

	if (CPlayer->powers[pw_strength] && APART(PowerupColors[pw_strength]))
	{
		// slowly fade the berzerk out
		cnt = 128 - ((CPlayer->powers[pw_strength]>>3) & (~0x1f));

		if (cnt > 0)
		{
			AddBlend (RPART(PowerupColors[pw_strength])/255.f,
					  GPART(PowerupColors[pw_strength])/255.f,
					  BPART(PowerupColors[pw_strength])/255.f,
					  (APART(PowerupColors[pw_strength])*cnt)/(255.f*128.f),
					  blend);
		}
	}

	cnt = DamageToAlpha[MIN (113, CPlayer->damagecount)];
		
	if (cnt)
	{
		if (cnt > 228)
			cnt = 228;

		AddBlend (1.f, 0.f, 0.f, cnt / 255.f, blend);
	}

	// Unlike Doom, I did not have any utility source to look at to find the
	// exact numbers to use here, so I've had to guess by looking at how they
	// affect the white color in Hexen's palette and picking an alpha value
	// that seems reasonable.

	if (CPlayer->mstaffcount)
	{ // The bloodscourge's flash isn't really describable using a blend
	  // because the blue component of each color is the same for each level.
		AddBlend (0.59216f, 0.43529f, 0.f, CPlayer->mstaffcount/6.f, blend);
	}
	if (CPlayer->cholycount)
	{
		AddBlend (0.51373f, 0.51373f, 0.51373f, CPlayer->cholycount/6.f, blend);
	}
	if (CPlayer->poisoncount)
	{
		cnt = MIN (CPlayer->poisoncount, 64);
		AddBlend (0.04f, 0.2571f, 0.f, cnt/93.2571428571f, blend);
	}
	if (CPlayer->mo->flags2 & MF2_ICEDAMAGE)
	{
		AddBlend (0.25f, 0.25f, 0.853f, 0.4f, blend);
	}

	if (CPlayer->camera != NULL && CPlayer->camera->player != NULL)
	{
		player_t *player = CPlayer->camera->player;
		AddBlend (player->BlendR, player->BlendG, player->BlendB, player->BlendA, blend);
	}

	V_SetBlend ((int)(blend[0] * 255.0f), (int)(blend[1] * 255.0f),
				(int)(blend[2] * 255.0f), (int)(blend[3] * 256.0f));
}

void FBaseStatusBar::DrawConsistancy () const
{
	static bool firsttime = true;
	int i;
	char conbuff[64], *buff_p;

	if (!netgame)
		return;

	buff_p = NULL;
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && players[i].inconsistant)
		{
			if (buff_p == NULL)
			{
				strcpy (conbuff, "Out of sync with:");
				buff_p = conbuff + 17;
			}
			*buff_p++ = ' ';
			*buff_p++ = '1' + i;
			*buff_p = 0;
		}
	}

	if (buff_p != NULL)
	{
		if (firsttime)
		{
			firsttime = false;
			if (debugfile)
			{
				fprintf (debugfile, "%s as of tic %d (%d)\n", conbuff,
					players[1-consoleplayer].inconsistant,
					players[1-consoleplayer].inconsistant/ticdup);
			}
#ifdef _DEBUG
			AddCommandString ("showrngs");
#endif
		}
		screen->DrawText (CR_GREEN, 
			(screen->GetWidth() - SmallFont->StringWidth (conbuff)*CleanXfac) / 2,
			0, conbuff, DTA_CleanNoMove, true, TAG_DONE);
		BorderTopRefresh = screen->GetPageCount ();
	}
}

void FBaseStatusBar::FlashArtifact (int arti)
{
}

void FBaseStatusBar::SetFace (void *)
{
}

void FBaseStatusBar::NewGame ()
{
}

void FBaseStatusBar::SetInteger (int pname, int param)
{
}

void FBaseStatusBar::Serialize (FArchive &arc)
{
	arc << Messages;
}

void FBaseStatusBar::ScreenSizeChanged ()
{
	st_scale.Callback ();
	SB_state = screen->GetPageCount ();

	DHUDMessage *message = Messages;
	while (message != NULL)
	{
		message->ScreenSizeChanged ();
		message = message->Next;
	}
}

//---------------------------------------------------------------------------
//
// PROC FindInventoryPos
//
//---------------------------------------------------------------------------

void FBaseStatusBar::FindInventoryPos (int &pos, bool &moreleft, bool &moreright) const
{
	int i, x;
	int countleft, countright;
	int lowest;

	countleft = 0;
	countright = 0;
	lowest = 1;

	x = CPlayer->readyArtifact - 1;
	for (i = 0; i < 3 && x > 0; x--)
	{
		if (CPlayer->inventory[x])
		{
			lowest = x;
			i++;
		}
	}
	pos = lowest;
	countleft = i;
	if (x > 0)
	{
		for (i = x; i > 0; i--)
		{
			if (CPlayer->inventory[i])
			{
				countleft++;
				lowest = i;
			}
		}
	}
	for (x = CPlayer->readyArtifact + 1; x < NUMINVENTORYSLOTS; x++)
	{
		if (CPlayer->inventory[x])
			countright++;
	}
	if (countleft + countright <= 6)
	{
		pos = lowest;
		moreleft = false;
		moreright = false;
		return;
	}
	if (countright < 3 && countleft > 3)
	{
		for (i = pos - 1; i > 0 && countright < 3; i--)
		{
			if (CPlayer->inventory[x])
			{
				pos = i;
				countleft--;
				countright++;
			}
		}
	}
	moreleft = (countleft > 3);
	moreright = (countright > 3);
}


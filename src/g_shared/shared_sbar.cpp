/*
** shared_sbar.cpp
** Base status bar implementation
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
#include "sbar.h"
#include "c_cvars.h"
#include "v_video.h"
#include "m_swap.h"
#include "r_draw.h"
#include "w_wad.h"
#include "z_zone.h"
#include "v_text.h"
#include "s_sound.h"
#include "gi.h"
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

static FImageCollection CrosshairImage;

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
	const char *namelist;
	int lump;

	if (num == 0)
	{
		CrosshairImage.Uninit ();
		return;
	}
	if (num < 0)
	{
		num = -num;
	}
	size = (SCREENWIDTH < 640) ? 'S' : 'B';
	sprintf (name, "XHAIR%c%d", size, num);
	if ((lump = W_CheckNumForName (name)) == -1)
	{
		sprintf (name, "XHAIR%c1", size);
		if ((lump = W_CheckNumForName (name)) == -1)
		{
			strcpy (name, "XHAIRS1");
		}
	}
	namelist = name;
	CrosshairImage.Init (&namelist, 1);
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
	ScaleCopy = NULL;
	Messages = NULL;
	Displacement = 0;

	SetScaled (st_scale);
	AmmoImages.Init (AmmoPics, NUMAMMO);
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

	if (ScaleCopy)
	{
		delete ScaleCopy;
	}
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
		::ST_Y = SCREENHEIGHT - Scale (RelTop, SCREENHEIGHT, 200);
		if (ScaleCopy == NULL)
		{
			ScaleCopy = new DSimpleCanvas (320, RelTop);
		}
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

	old = (id == 0) ? NULL : DetachMessage (id);
	if (old != NULL)
	{
		delete old;
	}
	msg->Next = Messages;
	msg->SBarID = id;
	Messages = msg;
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
		1.5f, 1.f, color, 2.f, 0.35f), 'PNAM');
}

//---------------------------------------------------------------------------
//
// PROC CopyToScreen
//
// Copies a region from ScaleCopy to the visible screen. We have to be
// very careful with our texture coordinates so that the end result looks
// like one continuous status bar instead of several individual pieces
// slapped on top of a status bar.
//
//---------------------------------------------------------------------------

void FBaseStatusBar::CopyToScreen (int x, int y, int w, int h) const
{
	fixed_t ix = ScaleIX;
	fixed_t iy = ScaleIY;
	int nx = (x * ScaleX) >> FRACBITS;
	int ny = (y * ScaleY) >> FRACBITS;
	fixed_t left = ix * nx;
	fixed_t err = iy * ny;
	w += x - (left >> FRACBITS);
	h += y - (err >> FRACBITS);
	int ow = (((x + w) * ScaleX) >> FRACBITS) - nx;
	int oh = (((y + h) * ScaleY) >> FRACBITS) - ny;

	if (ny + ::ST_Y + oh > SCREENHEIGHT)
		oh = SCREENHEIGHT - ny - ::ST_Y;
	if (nx + ow > SCREENWIDTH)
		ow = SCREENWIDTH - nx;

	if ((oh | ow) <= 0)
		return;

	int frompitch = ScaleCopy->GetPitch();
	byte *from = ScaleCopy->GetBuffer() + frompitch * (err >> FRACBITS) + (left >> FRACBITS);
	err &= FRACUNIT-1;
	left &= FRACUNIT-1;
	byte *to = screen->GetBuffer() + nx + screen->GetPitch() * (ny + ::ST_Y);
	int toskip = screen->GetPitch() - ow;

	do
	{
		int l = ow;
		fixed_t cx = left;
		do
		{
			//*to++ = x;
			*to++ = from[cx>>FRACBITS];
			cx += ix;
		} while (--l);
		to += toskip;
		err += iy;
		if (err >= FRACUNIT)
		{
			from += frompitch;
			err &= FRACUNIT-1;
		}
	} while (--oh);
}

//---------------------------------------------------------------------------
//
// PROC UpdateRect
//
// If scaling, copies the given rectangle on the status bar to the screen.
// If not scaling, does nothing.
//
//---------------------------------------------------------------------------

void FBaseStatusBar::UpdateRect (int x, int y, int w, int h) const
{
	if (Scaled)
	{
		ScaleCopy->Lock ();
		CopyToScreen (x, y, w, h);
		ScaleCopy->Unlock ();
	}
}

//---------------------------------------------------------------------------
//
// PROC DrawImage
//
// Draws an image with the status bar's upper-left corner as the origin.
//
//---------------------------------------------------------------------------

void FBaseStatusBar::DrawImageNoUpdate (const FImageCollection &coll, int img,
	int x, int y, byte *translation) const
{
	int w, h, xo, yo;
	byte *data = coll.GetImage (img, &w, &h, &xo, &yo);
	if (data)
	{
		x -= xo;
		y -= yo;
		if (Scaled)
		{
			if (y < 0)
			{
				screen->ScaleMaskedBlock (x * screen->GetWidth() / 320,
					::ST_Y + y * screen->GetHeight() / 200, w, h,
					(w * screen->GetWidth()) / 320, (h * screen->GetHeight()) / 200,
					data, NULL);
			}
			else
			{
				ScaleCopy->Lock ();
				ScaleCopy->DrawMaskedBlock (x, y, w, h, data, translation);
				ScaleCopy->Unlock ();
			}
		}
		else
		{
			screen->DrawMaskedBlock (x + ST_X, y + ST_Y, w, h, data, translation);
		}
	}
}

void FBaseStatusBar::DrawImage (const FImageCollection &coll, int img,
	int x, int y, byte *translation) const
{
	DrawImageNoUpdate (coll, img, x, y, translation);
	if (Scaled)
	{
		int w, h, xo, yo;
		byte *data = coll.GetImage (img, &w, &h, &xo, &yo);
		x -= xo;
		y -= yo;
		if (data && y >= 0)
		{
			ScaleCopy->Lock ();
			CopyToScreen (x, y, w, h);
			ScaleCopy->Unlock ();
		}
	}
}

//---------------------------------------------------------------------------
//
// PROC DrawPartialImage
//
// Draws a portion of an image with the status bar's upper-left corner as
// the origin. The image must not have any mask bytes. Used for Doom's sbar.
//
//---------------------------------------------------------------------------

void FBaseStatusBar::DrawPartialImage (const FImageCollection &coll, int img,
	int x, int y, int wx, int wy, int ww, int wh) const
{
	int w, h, xo, yo;
	byte *data = coll.GetImage (img, &w, &h, &xo, &yo);
	if (data)
	{
		byte *dest;
		int pitch;

		data += wx + wy*w;

		if (Scaled)
		{
			ScaleCopy->Lock ();
			dest = ScaleCopy->GetBuffer() + x + y*ScaleCopy->GetPitch();
			pitch = ScaleCopy->GetPitch();
		}
		else
		{
			dest = screen->GetBuffer() + x+ST_X + (y+ST_Y)*screen->GetPitch();
			pitch = screen->GetPitch();
		}

		do
		{
			memcpy (dest, data, ww);
			dest += pitch;
			data += w;
		} while (--wh);

		if (Scaled)
		{
			ScaleCopy->Unlock ();
		}
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
		xo = w / 2;
		yo = h;
	}
	if (hud_scale)
	{
		x *= CleanXfac;
		if (Centering)
			x += SCREENWIDTH / 2;
		else if (x < 0)
			x = SCREENWIDTH + x;
		x -= xo * CleanXfac;
		y *= CleanYfac;
		if (y < 0)
			y = SCREENHEIGHT + y;
		y -= yo * CleanYfac;
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
		x -= xo;
		y -= yo;
		return false;
	}
}

//---------------------------------------------------------------------------
//
// PROC DrawFadedImage
//
// Draws a translucenct image outside the status bar, possibly scaled.
//
//---------------------------------------------------------------------------

void FBaseStatusBar::DrawFadedImage (const FImageCollection &coll, int img,
	int x, int y, fixed_t shade) const
{
	int w, h, xo, yo;
	byte *data = coll.GetImage (img, &w, &h, &xo, &yo);
	if (data)
	{
		if (RepositionCoords (x, y, xo, yo, w, h))
		{
			screen->ScaleTranslucentMaskedBlock (x, y, w, h, w * CleanXfac,
				h * CleanYfac, data, NULL, shade);
		}
		else
		{
			screen->DrawTranslucentMaskedBlock (x, y, w, h, data, NULL, shade);
		}
	}
}

//---------------------------------------------------------------------------
//
// PROC DrawShadowedImage
//
// Draws a shadowed image outside the status bar, possibly scaled.
//
//---------------------------------------------------------------------------

void FBaseStatusBar::DrawShadowedImage (const FImageCollection &coll, int img,
	int x, int y, fixed_t shade) const
{
	int w, h, xo, yo;
	byte *data = coll.GetImage (img, &w, &h, &xo, &yo);
	if (data)
	{
		if (RepositionCoords (x, y, xo, yo, w, h))
		{
			screen->ScaleShadowedMaskedBlock (x, y, w, h, w * CleanXfac,
				h * CleanYfac, data, NULL, shade);
		}
		else
		{
			screen->DrawShadowedMaskedBlock (x, y, w, h, data, NULL, shade);
		}
	}
}

//---------------------------------------------------------------------------
//
// PROC DrawOuterImage
//
// Draws an image outside the status bar, possibly scaled.
//
//---------------------------------------------------------------------------

void FBaseStatusBar::DrawOuterImage (const FImageCollection &coll, int img,
	int x, int y) const
{
	int w, h, xo, yo;
	byte *data = coll.GetImage (img, &w, &h, &xo, &yo);
	if (data)
	{
		if (RepositionCoords (x, y, xo, yo, w, h))
		{
			screen->ScaleMaskedBlock (x, y, w, h, w * CleanXfac,
				h * CleanYfac, data, NULL);
		}
		else
		{
			screen->DrawMaskedBlock (x, y, w, h, data, NULL);
		}
	}
}

//---------------------------------------------------------------------------
//
// PROC DrawOuterPatch
//
// Draws a patch outside the status bar, possibly scaled.
//
//---------------------------------------------------------------------------

void FBaseStatusBar::DrawOuterPatch (const patch_t *patch, int x, int y) const
{
	if (patch)
	{
		if (RepositionCoords (x, y, 0, 0, 0, 0))
		{
			screen->DrawPatchStretched (patch, x, y,
				SHORT(patch->width) * CleanXfac, SHORT(patch->height) * CleanYfac);
		}
		else
		{
			screen->DrawPatch (patch, x, y);
		}
	}
}

//---------------------------------------------------------------------------
//
// PROC DrINumber
//
// Draws a three digit number.
//
//---------------------------------------------------------------------------

void FBaseStatusBar::DrINumber (signed int val, int x, int y) const
{
	int oldval;

	if (val > 999)
		val = 999;
	oldval = val;
	if (val < 0)
	{
		if (val < -9)
		{
			DrawImage (Images, imgLAME, x+1, y+1);
			return;
		}
		val = -val;
		DrawImage (Images, imgINumbers+val, x+18, y);
		DrawImage (Images, imgNEGATIVE, x+9, y);
		return;
	}
	if (val > 99)
	{
		DrawImage (Images, imgINumbers+val/100, x, y);
	}
	val = val % 100;
	if (val > 9 || oldval > 99)
	{
		DrawImage (Images, imgINumbers+val/10, x+9, y);
	}
	val = val % 10;
	DrawImage (Images, imgINumbers+val, x+18, y);
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

	w = Images.GetImageWidth (imgBNumbers+3);
	h = Images.GetImageHeight (imgBNumbers+3);

	if (Scaled)
	{
		ScaleCopy->Lock ();
		CopyToScreen (x, y, size*w, h);
		ScaleCopy->Unlock ();
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
		DrawImage (Images, imgBNumbers,
			xpos - Images.GetImageWidth (imgBNumbers)/2 - w, y);
		return;
	}
	while (val != 0 && size--)
	{
		xpos -= w;
		int oldval = val;
		val /= 10;
		index = imgBNumbers + (oldval - val*10);
		DrawImage (Images, index, xpos - Images.GetImageWidth (index)/2, y);
	}
	if (neg)
	{
		xpos -= w;
		DrawImage (Images, imgBNEGATIVE,
			xpos - Images.GetImageWidth (imgBNEGATIVE)/2, y);
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

	if (Scaled)
	{
		ScaleCopy->Lock ();
		CopyToScreen (x, y, 3*4, 6);
		ScaleCopy->Unlock ();
	}
	if (val > 999)
	{
		val = 999;
	}
	if (val > 99)
	{
		digit = val / 100;
		DrawImage (Images, imgSmNumbers + digit, x, y);
		val -= digit * 100;
	}
	if (val > 9 || digit)
	{
		digit = val / 10;
		DrawImage (Images, imgSmNumbers + digit, x+4, y);
		val -= digit * 10;
	}
	DrawImage (Images, imgSmNumbers + val, x+8, y);
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
			DrawOuterImage (Images, imgLAME, x+1, y+1);
			return;
		}
		val = -val;
		DrawOuterImage (Images, imgINumbers+val, x+18, y);
		DrawOuterImage (Images, imgNEGATIVE, x+9, y);
		return;
	}
	if (val > 99)
	{
		DrawOuterImage (Images, imgINumbers+val/100, x, y);
	}
	val = val % 100;
	if (val > 9 || oldval > 99)
	{
		DrawOuterImage (Images, imgINumbers+val/10, x+9, y);
	}
	val = val % 10;
	DrawOuterImage (Images, imgINumbers+val, x+18, y);
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

	Images.GetImage (imgBNumbers+3, &w, &xpos, &xpos, &xpos);
	w = Images.GetImageWidth (imgBNumbers+3);

	div = 1;
	for (i = size-1; i>0; i--)
		div *= 10;

	xpos = x + w/2;

	if (val == 0)
	{
		DrawShadowedImage (Images, imgBNumbers,
			xpos - Images.GetImageWidth (imgBNumbers)/2 + w*(size-1), y, HR_SHADOW);
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
			DrawShadowedImage (Images, i + imgBNumbers,
				xpos - Images.GetImageWidth (i + imgBNumbers)/2, y, HR_SHADOW);
			val -= i * div;
		}
		div /= 10;
		xpos += w;
	}
	if (neg)
	{
		DrawShadowedImage (Images, imgBNEGATIVE,
			first - Images.GetImageWidth (imgBNEGATIVE)/2 - w, y, HR_SHADOW);
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
		DrawOuterImage (Images, imgSmNumbers + digit, x, y);
		val -= digit * 100;
	}
	if (val > 9 || digit)
	{
		digit = val / 10;
		DrawOuterImage (Images, imgSmNumbers + digit, x+4, y);
		val -= digit * 10;
	}
	DrawOuterImage (Images, imgSmNumbers + val, x+8, y);
}

//---------------------------------------------------------------------------
//
// PROC ShadeChain
//
//---------------------------------------------------------------------------

void FBaseStatusBar::ShadeChain (int left, int right, int top, int height) const
{
	int i;
	int pitch;
	int diff;
	byte *top_p;

	if (Scaled)
	{
		ScaleCopy->Lock ();
		pitch = ScaleCopy->GetPitch();
		top_p = ScaleCopy->GetBuffer();
	}
	else
	{
		top += ST_Y;
		left += ST_X;
		right += ST_X;
		pitch = screen->GetPitch();
		top_p = screen->GetBuffer();
	}
	top_p += top*pitch + left;
	diff = right+15-left;

	for (i = 0; i < 16; i++)
	{
		DWORD *darkener = Col2RGB8[18 + i*2];
		int h = height;
		byte *dest = top_p;
		do
		{
			unsigned int lbg = darkener[*dest] | 0x1f07c1f;
			unsigned int rbg = darkener[*(dest+diff)] | 0x1f07c1f;
			*dest = RGB32k[0][0][lbg & (lbg>>15)];
			*(dest+diff) = RGB32k[0][0][rbg & (rbg>>15)];
			dest += pitch;
		} while (--h);
		top_p++;
		diff -= 2;
	}

	if (Scaled)
	{
		CopyToScreen (left, top, 16, height);
		CopyToScreen (right, top, 16, height);
		ScaleCopy->Unlock ();
	}
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
	int iw, ih, iox, ioy;
	byte *image;

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

	image = CrosshairImage.GetImage (0, &iw, &ih, &iox, &ioy);
	// Don't draw the crosshair if there is none
	if (image == NULL)
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
	w = (iw * size) >> FRACBITS;
	h = (ih * size) >> FRACBITS;

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

	screen->ScaleAlphaMaskedBlock (
		realviewwidth / 2 + viewwindowx - ((iox * size) >> FRACBITS),
		realviewheight / 2  + viewwindowy- ((ioy * size) >> FRACBITS),
		iw, ih, w, h, image, palettecolor);
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
			screen->DrawText (CR_GREEN, SCREENWIDTH - 80, y, line);
			BorderNeedRefresh = screen->GetPageCount();
		}
	}

	if (viewactive && CPlayer && CPlayer->camera->player)
	{
		DrawCrosshair ();
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
			screen->DrawTextClean (CR_GREY, SCREENWIDTH - 80*CleanXfac, 8, line);
		}

		// Draw map name
		y = ::ST_Y - height;
		if ((gameinfo.gametype == GAME_Heretic && SCREENWIDTH > 320 && !Scaled)
			|| (gameinfo.gametype == GAME_Hexen))
		{
			y -= 8;
		}
		cluster_info_t *cluster = FindClusterInfo (level.cluster);
		i = 0;
		if (cluster == NULL || !(cluster->flags & CLUSTER_HUB))
		{
			while (i < 8 && level.mapname[i])
			{
				line[i++] = level.mapname[i];
			}
			line[i] = ':';
			line[i+1] = ' ';
			i += 2;
		}
		line[i] = TEXTCOLOR_ESCAPE;
		line[i+1] = CR_GREY + 'A';
		strcpy (&line[i+2], level.level_name);
		screen->DrawTextClean (highlight,
			(SCREENWIDTH - screen->StringWidth (line)*CleanXfac)/2, y, line);

		if (!deathmatch)
		{
			int y = 8;

			// Draw monster count
			if (am_showmonsters)
			{
				sprintf (line, "MONSTERS:"
							   TEXTCOLOR_GREY " %d/%d",
							   level.killed_monsters, level.total_monsters);
				screen->DrawTextClean (highlight, 8, y, line);
				y += height;
			}

			// Draw secret count
			if (am_showsecrets)
			{
				sprintf (line, "SECRETS:"
							   TEXTCOLOR_GREY " %d/%d",
							   level.found_secrets, level.total_secrets);
				screen->DrawTextClean (highlight, 8, y, line);
				y += height;
			}

			// Draw item count
			if (am_showitems)
			{
				sprintf (line, "ITEMS:"
							   TEXTCOLOR_GREY " %d/%d",
							   level.found_items, level.total_items);
				screen->DrawTextClean (highlight, 8, y, line);
			}
		}
	}

	if (noisedebug)
	{
		S_NoiseDebug ();
	}

	if (demoplayback && demover != GAMEVER)
	{
		screen->DrawTextClean (CR_TAN, 0, ST_Y - 40 * CleanYfac,
			"Demo was recorded with a different version\n"
			"of ZDoom. Expect it to go out of sync.");
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
	int cnt;

	AddBlend (BaseBlendR / 255.0f, BaseBlendG / 255.0f,
		BaseBlendB / 255.0f, BaseBlendA, blend);

	if (CPlayer->powers[pw_ironfeet] > 4*32 || CPlayer->powers[pw_ironfeet]&8)
	{
		AddBlend (0.0f, 1.0f, 0.0f, 0.125f, blend);
	}
	if (CPlayer->bonuscount)
	{
		cnt = CPlayer->bonuscount << 3;
		AddBlend (0.8431f, 0.7294f, 0.2706f, cnt > 128 ? 0.5f : cnt / 255.0f, blend);
	}

	cnt = DamageToAlpha[MIN (113, CPlayer->damagecount)];

	if (CPlayer->powers[pw_strength])
	{
		// slowly fade the berzerk out
		int bzc = 128 - ((CPlayer->powers[pw_strength]>>3) & (~0x1f));

		if (bzc > cnt)
			cnt = bzc;
	}
		
	if (cnt)
	{
		if (cnt > 228)
			cnt = 228;

		AddBlend (1.0f, 0.0f, 0.0f, cnt / 255.0f, blend);
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
				strcpy (conbuff, "Consistency failure: ");
				buff_p = conbuff + 21;
			}
			*buff_p++ = '0' + i;
			*buff_p = 0;
		}
	}

	if (buff_p != NULL)
	{
		screen->DrawTextClean (CR_GREEN, 
			(screen->GetWidth() - screen->StringWidth (conbuff)*CleanXfac) / 2,
			0, conbuff);
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

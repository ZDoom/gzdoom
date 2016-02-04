/*
** shared_sbar.cpp
** Base status bar implementation
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#include <assert.h>

#include "templates.h"
#include "sbar.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "c_console.h"
#include "v_video.h"
#include "m_swap.h"
#include "w_wad.h"
#include "v_text.h"
#include "s_sound.h"
#include "gi.h"
#include "doomstat.h"
#include "g_level.h"
#include "d_net.h"
#include "colormatcher.h"
#include "v_palette.h"
#include "d_player.h"
#include "farchive.h"
#include "a_hexenglobal.h"
#include "gstrings.h"

#include "../version.h"

#define XHAIRSHRINKSIZE		(FRACUNIT/18)
#define XHAIRPICKUPSIZE		(FRACUNIT*2+XHAIRSHRINKSIZE)
#define POWERUPICONSIZE		32

IMPLEMENT_POINTY_CLASS(DBaseStatusBar)
	DECLARE_POINTER(Messages[0])
	DECLARE_POINTER(Messages[1])
	DECLARE_POINTER(Messages[2])
END_POINTERS

EXTERN_CVAR (Bool, am_showmonsters)
EXTERN_CVAR (Bool, am_showsecrets)
EXTERN_CVAR (Bool, am_showitems)
EXTERN_CVAR (Bool, am_showtime)
EXTERN_CVAR (Bool, am_showtotaltime)
EXTERN_CVAR (Bool, noisedebug)
EXTERN_CVAR (Int, con_scaletext)

DBaseStatusBar *StatusBar;

extern int setblocks;

int ST_X, ST_Y;
int SB_state = 3;

FTexture *CrosshairImage;
static int CrosshairNum;

// [RH] Base blending values (for e.g. underwater)
int BaseBlendR, BaseBlendG, BaseBlendB;
float BaseBlendA;

CVAR (Int, paletteflash, 0, CVAR_ARCHIVE)
CVAR (Flag, pf_hexenweaps,	paletteflash, PF_HEXENWEAPONS)
CVAR (Flag, pf_poison,		paletteflash, PF_POISON)
CVAR (Flag, pf_ice,			paletteflash, PF_ICE)
CVAR (Flag, pf_hazard,		paletteflash, PF_HAZARD)

// Stretch status bar to full screen width?
CUSTOM_CVAR (Bool, st_scale, true, CVAR_ARCHIVE)
{
	if (StatusBar)
	{
		StatusBar->SetScaled (self);
		setsizeneeded = true;
	}
}

CVAR (Int, crosshair, 0, CVAR_ARCHIVE)
CVAR (Bool, crosshairforce, false, CVAR_ARCHIVE)
CVAR (Color, crosshaircolor, 0xff0000, CVAR_ARCHIVE);
CVAR (Bool, crosshairhealth, true, CVAR_ARCHIVE);
CVAR (Bool, crosshairscale, false, CVAR_ARCHIVE);
CVAR (Bool, crosshairgrow, false, CVAR_ARCHIVE);
CUSTOM_CVAR(Int, am_showmaplabel, 2, CVAR_ARCHIVE)
{
	if (self < 0 || self > 2) self = 2;
}

CVAR (Bool, idmypos, false, 0);

//---------------------------------------------------------------------------
//
// Format the map name, include the map label if wanted
//
//---------------------------------------------------------------------------

void ST_FormatMapName(FString &mapname, const char *mapnamecolor)
{
	cluster_info_t *cluster = FindClusterInfo (level.cluster);
	bool ishub = (cluster != NULL && (cluster->flags & CLUSTER_HUB));

	if (am_showmaplabel == 1 || (am_showmaplabel == 2 && !ishub))
	{
		mapname << level.MapName << ": ";
	}
	mapname << mapnamecolor << level.LevelName;
}

//---------------------------------------------------------------------------
//
// Load crosshair definitions
//
//---------------------------------------------------------------------------

void ST_LoadCrosshair(bool alwaysload)
{
	int num = 0;
	char name[16], size;

	if (!crosshairforce &&
		players[consoleplayer].camera != NULL &&
		players[consoleplayer].camera->player != NULL &&
		players[consoleplayer].camera->player->ReadyWeapon != NULL)
	{
		num = players[consoleplayer].camera->player->ReadyWeapon->Crosshair;
	}
	if (num == 0)
	{
		num = crosshair;
	}
	if (!alwaysload && CrosshairNum == num && CrosshairImage != NULL)
	{ // No change.
		return;
	}

	if (CrosshairImage != NULL)
	{
		CrosshairImage->Unload ();
	}
	if (num == 0)
	{
		CrosshairNum = 0;
		CrosshairImage = NULL;
		return;
	}
	if (num < 0)
	{
		num = -num;
	}
	size = (SCREENWIDTH < 640) ? 'S' : 'B';

	mysnprintf (name, countof(name), "XHAIR%c%d", size, num);
	FTextureID texid = TexMan.CheckForTexture(name, FTexture::TEX_MiscPatch, FTextureManager::TEXMAN_TryAny | FTextureManager::TEXMAN_ShortNameOnly);
	if (!texid.isValid())
	{
		mysnprintf (name, countof(name), "XHAIR%c1", size);
		texid = TexMan.CheckForTexture(name, FTexture::TEX_MiscPatch, FTextureManager::TEXMAN_TryAny | FTextureManager::TEXMAN_ShortNameOnly);
		if (!texid.isValid())
		{
			texid = TexMan.CheckForTexture("XHAIRS1", FTexture::TEX_MiscPatch, FTextureManager::TEXMAN_TryAny | FTextureManager::TEXMAN_ShortNameOnly);
		}
	}
	CrosshairNum = num;
	CrosshairImage = TexMan[texid];
}

//---------------------------------------------------------------------------
//
// ST_Clear
//
//---------------------------------------------------------------------------

void ST_Clear()
{
	if (StatusBar != NULL)
	{
		StatusBar->Destroy();
		StatusBar = NULL;
	}
	CrosshairImage = NULL;
	CrosshairNum = 0;
}

//---------------------------------------------------------------------------
//
// ST_SetNeedRefresh
//
//---------------------------------------------------------------------------

void ST_SetNeedRefresh()
{
	SB_state = (StatusBar == NULL || screen == NULL) ? 0 : screen->GetPageCount();
}

//---------------------------------------------------------------------------
//
// Constructor
//
//---------------------------------------------------------------------------

DBaseStatusBar::DBaseStatusBar (int reltop, int hres, int vres)
{
	CompleteBorder = false;
	Centering = false;
	FixedOrigin = false;
	CrosshairSize = FRACUNIT;
	RelTop = reltop;
	memset(Messages, 0, sizeof(Messages));
	Displacement = 0;
	CPlayer = NULL;
	ShowLog = false;
	HorizontalResolution = hres;
	VirticalResolution = vres;

	SetScaled (st_scale);
}

//---------------------------------------------------------------------------
//
// PROP Destroy
//
//---------------------------------------------------------------------------

void DBaseStatusBar::Destroy ()
{
	for (unsigned int i = 0; i < countof(Messages); ++i)
	{
		DHUDMessage *msg = Messages[i];
		while (msg)
		{
			DHUDMessage *next = msg->Next;
			msg->Destroy();
			msg = next;
		}
		Messages[i] = NULL;
	}
	Super::Destroy();
}

//---------------------------------------------------------------------------
//
// PROC SetScaled
//
//---------------------------------------------------------------------------

//[BL] Added force argument to have forcescaled mean forcescaled.
// - Also, if the VirticalResolution is something other than the default (200)
//   We should always obey the value of scale.
void DBaseStatusBar::SetScaled (bool scale, bool force)
{
	Scaled = (RelTop != 0 || force) && ((SCREENWIDTH != 320 || HorizontalResolution != 320) && scale);

	if (!Scaled)
	{
		ST_X = (SCREENWIDTH - HorizontalResolution) / 2;
		ST_Y = SCREENHEIGHT - RelTop;
		::ST_Y = ST_Y;
		if (RelTop > 0)
		{
			Displacement = ((ST_Y * VirticalResolution / SCREENHEIGHT) - (VirticalResolution - RelTop))*FRACUNIT/RelTop;
		}
		else
		{
			Displacement = 0;
		}
	}
	else
	{
		ST_X = 0;
		ST_Y = VirticalResolution - RelTop;
		if (CheckRatio(SCREENWIDTH, SCREENHEIGHT) != 4)
		{ // Normal resolution
			::ST_Y = Scale (ST_Y, SCREENHEIGHT, VirticalResolution);
		}
		else
		{ // 5:4 resolution
			::ST_Y = Scale(ST_Y - VirticalResolution/2, SCREENHEIGHT*3, Scale(VirticalResolution, BaseRatioSizes[4][1], 200)) + SCREENHEIGHT/2
				+ (SCREENHEIGHT - SCREENHEIGHT * BaseRatioSizes[4][3] / 48) / 2;
		}
		Displacement = 0;
	}
	::ST_X = ST_X;
	ST_SetNeedRefresh();
}

//---------------------------------------------------------------------------
//
// PROC AttachToPlayer
//
//---------------------------------------------------------------------------

void DBaseStatusBar::AttachToPlayer (player_t *player)
{
	CPlayer = player;
	ST_SetNeedRefresh();
}

//---------------------------------------------------------------------------
//
// PROC GetPlayer
//
//---------------------------------------------------------------------------

int DBaseStatusBar::GetPlayer ()
{
	return int(CPlayer - players);
}

//---------------------------------------------------------------------------
//
// PROC MultiplayerChanged
//
//---------------------------------------------------------------------------

void DBaseStatusBar::MultiplayerChanged ()
{
	ST_SetNeedRefresh();
}

//---------------------------------------------------------------------------
//
// PROC Tick
//
//---------------------------------------------------------------------------

void DBaseStatusBar::Tick ()
{
	for (unsigned int i = 0; i < countof(Messages); ++i)
	{
		DHUDMessage *msg = Messages[i];
		DHUDMessage **prev = &Messages[i];

		while (msg)
		{
			DHUDMessage *next = msg->Next;

			if (msg->Tick ())
			{
				*prev = next;
				msg->Destroy();
			}
			else
			{
				prev = &msg->Next;
			}
			msg = next;
		}

		// If the crosshair has been enlarged, shrink it.
		if (CrosshairSize > FRACUNIT)
		{
			CrosshairSize -= XHAIRSHRINKSIZE;
			if (CrosshairSize < FRACUNIT)
			{
				CrosshairSize = FRACUNIT;
			}
		}
	}
}

//---------------------------------------------------------------------------
//
// PROC AttachMessage
//
//---------------------------------------------------------------------------

void DBaseStatusBar::AttachMessage (DHUDMessage *msg, DWORD id, int layer)
{
	DHUDMessage *old = NULL;
	DHUDMessage **prev;
	DObject *container = this;

	old = (id == 0 || id == 0xFFFFFFFF) ? NULL : DetachMessage (id);
	if (old != NULL)
	{
		old->Destroy();
	}

	// Merge unknown layers into the default layer.
	if ((size_t)layer >= countof(Messages))
	{
		layer = HUDMSGLayer_Default;
	}

	prev = &Messages[layer];

	// The ID serves as a priority, where lower numbers appear in front of
	// higher numbers. (i.e. The list is sorted in descending order, since
	// it gets drawn back to front.)
	while (*prev != NULL && (*prev)->SBarID > id)
	{
		container = *prev;
		prev = &(*prev)->Next;
	}

	msg->Next = *prev;
	msg->SBarID = id;
	*prev = msg;
	GC::WriteBarrier(container, msg);
}

//---------------------------------------------------------------------------
//
// PROC DetachMessage
//
//---------------------------------------------------------------------------

DHUDMessage *DBaseStatusBar::DetachMessage (DHUDMessage *msg)
{
	for (unsigned int i = 0; i < countof(Messages); ++i)
	{
		DHUDMessage *probe = Messages[i];
		DHUDMessage **prev = &Messages[i];

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
			if (screen != NULL)
			{
				ST_SetNeedRefresh();
			}
			return probe;
		}
	}
	return NULL;
}

DHUDMessage *DBaseStatusBar::DetachMessage (DWORD id)
{
	for (unsigned int i = 0; i < countof(Messages); ++i)
	{
		DHUDMessage *probe = Messages[i];
		DHUDMessage **prev = &Messages[i];

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
			if (screen != NULL)
			{
				ST_SetNeedRefresh();
			}
			return probe;
		}
	}
	return NULL;
}

//---------------------------------------------------------------------------
//
// PROC DetachAllMessages
//
//---------------------------------------------------------------------------

void DBaseStatusBar::DetachAllMessages ()
{
	for (unsigned int i = 0; i < countof(Messages); ++i)
	{
		DHUDMessage *probe = Messages[i];

		Messages[i] = NULL;
		while (probe != NULL)
		{
			DHUDMessage *next = probe->Next;
			probe->Destroy();
			probe = next;
		}
	}
}

//---------------------------------------------------------------------------
//
// PROC ShowPlayerName
//
//---------------------------------------------------------------------------

void DBaseStatusBar::ShowPlayerName ()
{
	EColorRange color;

	color = (CPlayer == &players[consoleplayer]) ? CR_GOLD : CR_GREEN;
	AttachMessage (new DHUDMessageFadeOut (SmallFont, CPlayer->userinfo.GetName(),
		1.5f, 0.92f, 0, 0, color, 2.f, 0.35f), MAKE_ID('P','N','A','M'));
}

//---------------------------------------------------------------------------
//
// PROC DrawImage
//
// Draws an image with the status bar's upper-left corner as the origin.
//
//---------------------------------------------------------------------------

void DBaseStatusBar::DrawImage (FTexture *img,
	int x, int y, FRemapTable *translation) const
{
	if (img != NULL)
	{
		screen->DrawTexture (img, x + ST_X, y + ST_Y,
			DTA_Translation, translation,
			DTA_Bottom320x200, Scaled,
			TAG_DONE);
	}
}

//---------------------------------------------------------------------------
//
// PROC DrawImage
//
// Draws an optionally dimmed image with the status bar's upper-left corner
// as the origin.
//
//---------------------------------------------------------------------------

void DBaseStatusBar::DrawDimImage (FTexture *img,
	int x, int y, bool dimmed) const
{
	if (img != NULL)
	{
		screen->DrawTexture (img, x + ST_X, y + ST_Y,
			DTA_ColorOverlay, dimmed ? DIM_OVERLAY : 0,
			DTA_Bottom320x200, Scaled,
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

void DBaseStatusBar::DrawFadedImage (FTexture *img,
	int x, int y, fixed_t shade) const
{
	if (img != NULL)
	{
		screen->DrawTexture (img, x + ST_X, y + ST_Y,
			DTA_Alpha, shade,
			DTA_Bottom320x200, Scaled,
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

void DBaseStatusBar::DrawPartialImage (FTexture *img, int wx, int ww) const
{
	if (img != NULL)
	{
		screen->DrawTexture (img, ST_X, ST_Y,
			DTA_WindowLeft, wx,
			DTA_WindowRight, wx + ww,
			DTA_Bottom320x200, Scaled,
			TAG_DONE);
	}
}

//---------------------------------------------------------------------------
//
// PROC DrINumber
//
// Draws a three digit number.
//
//---------------------------------------------------------------------------

void DBaseStatusBar::DrINumber (signed int val, int x, int y, int imgBase) const
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

void DBaseStatusBar::DrBNumber (signed int val, int x, int y, int size) const
{
	bool neg;
	int i, w;
	int power;
	FTexture *pic;

	pic = Images[imgBNumbers];
	w = (pic != NULL) ? pic->GetWidth() : 0;

	if (val == 0)
	{
		if (pic != NULL)
		{
			DrawImage (pic, x - w, y);
		}
		return;
	}

	if ( (neg = val < 0) )
	{
		val = -val;
		size--;
	}
	for (i = size-1, power = 10; i > 0; i--)
	{
		power *= 10;
	}
	if (val >= power)
	{
		val = power - 1;
	}
	while (val != 0 && size--)
	{
		x -= w;
		pic = Images[imgBNumbers + val % 10];
		val /= 10;
		if (pic != NULL)
		{
			DrawImage (pic, x, y);
		}
	}
	if (neg)
	{
		pic = Images[imgBNEGATIVE];
		if (pic != NULL)
		{
			DrawImage (pic, x - w, y);
		}
	}
}

//---------------------------------------------------------------------------
//
// PROC DrSmallNumber
//
// Draws a small three digit number.
//
//---------------------------------------------------------------------------

void DBaseStatusBar::DrSmallNumber (int val, int x, int y) const
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
// Draws a number outside the status bar, possibly scaled.
//
//---------------------------------------------------------------------------

void DBaseStatusBar::DrINumberOuter (signed int val, int x, int y, bool center, int w) const
{
	bool negative = false;

	x += w*2;
	if (val < 0)
	{
		negative = true;
		val = -val;
	}
	else if (val == 0)
	{
		screen->DrawTexture (Images[imgINumbers], x + 1, y + 1,
			DTA_FillColor, 0, DTA_Alpha, HR_SHADOW,
			DTA_HUDRules, center ? HUD_HorizCenter : HUD_Normal, TAG_DONE);
		screen->DrawTexture (Images[imgINumbers], x, y,
			DTA_HUDRules, center ? HUD_HorizCenter : HUD_Normal, TAG_DONE);
		return;
	}

	int oval = val;
	int ox = x;

	// First the shadow
	while (val != 0)
	{
		screen->DrawTexture (Images[imgINumbers + val % 10], x + 1, y + 1,
			DTA_FillColor, 0, DTA_Alpha, HR_SHADOW,
			DTA_HUDRules, center ? HUD_HorizCenter : HUD_Normal, TAG_DONE);
		x -= w;
		val /= 10;
	}
	if (negative)
	{
		screen->DrawTexture (Images[imgNEGATIVE], x + 1, y + 1,
			DTA_FillColor, 0, DTA_Alpha, HR_SHADOW,
			DTA_HUDRules, center ? HUD_HorizCenter : HUD_Normal, TAG_DONE);
	}

	// Then the real deal
	val = oval;
	x = ox;
	while (val != 0)
	{
		screen->DrawTexture (Images[imgINumbers + val % 10], x, y,
			DTA_HUDRules, center ? HUD_HorizCenter : HUD_Normal, TAG_DONE);
		x -= w;
		val /= 10;
	}
	if (negative)
	{
		screen->DrawTexture (Images[imgNEGATIVE], x, y,
			DTA_HUDRules, center ? HUD_HorizCenter : HUD_Normal, TAG_DONE);
	}
}

//---------------------------------------------------------------------------
//
// PROC DrBNumberOuter
//
// Draws a three digit number using the big font outside the status bar.
//
//---------------------------------------------------------------------------

void DBaseStatusBar::DrBNumberOuter (signed int val, int x, int y, int size) const
{
	int xpos;
	int w;
	bool negative = false;
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

	xpos = x + w/2 + (size-1)*w;

	if (val == 0)
	{
		pic = Images[imgBNumbers];
		if (pic != NULL)
		{
			screen->DrawTexture (pic, xpos - pic->GetWidth()/2 + 2, y + 2,
				DTA_HUDRules, HUD_Normal,
				DTA_Alpha, HR_SHADOW,
				DTA_FillColor, 0,
				TAG_DONE);
			screen->DrawTexture (pic, xpos - pic->GetWidth()/2, y,
				DTA_HUDRules, HUD_Normal,
				TAG_DONE);
		}
		return;
	}
	else if (val < 0)
	{
		negative = true;
		val = -val;
	}

	int oval = val;
	int oxpos = xpos;

	// Draw shadow first
	while (val != 0)
	{
		pic = Images[val % 10 + imgBNumbers];
		if (pic != NULL)
		{
			screen->DrawTexture (pic, xpos - pic->GetWidth()/2 + 2, y + 2,
				DTA_HUDRules, HUD_Normal,
				DTA_Alpha, HR_SHADOW,
				DTA_FillColor, 0,
				TAG_DONE);
		}
		val /= 10;
		xpos -= w;
	}
	if (negative)
	{
		pic = Images[imgBNEGATIVE];
		if (pic != NULL)
		{
			screen->DrawTexture (pic, xpos - pic->GetWidth()/2 + 2, y + 2,
				DTA_HUDRules, HUD_Normal,
				DTA_Alpha, HR_SHADOW,
				DTA_FillColor, 0,
				TAG_DONE);
		}
	}

	// Then draw the real thing
	val = oval;
	xpos = oxpos;
	while (val != 0)
	{
		pic = Images[val % 10 + imgBNumbers];
		if (pic != NULL)
		{
			screen->DrawTexture (pic, xpos - pic->GetWidth()/2, y,
				DTA_HUDRules, HUD_Normal,
				TAG_DONE);
		}
		val /= 10;
		xpos -= w;
	}
	if (negative)
	{
		pic = Images[imgBNEGATIVE];
		if (pic != NULL)
		{
			screen->DrawTexture (pic, xpos - pic->GetWidth()/2, y,
				DTA_HUDRules, HUD_Normal,
				TAG_DONE);
		}
	}
}

//---------------------------------------------------------------------------
//
// PROC DrBNumberOuter
//
// Draws a three digit number using the real big font outside the status bar.
//
//---------------------------------------------------------------------------

void DBaseStatusBar::DrBNumberOuterFont (signed int val, int x, int y, int size) const
{
	int xpos;
	int w, v;
	bool negative = false;
	FTexture *pic;

	w = 0;
	BigFont->GetChar ('0', &w);

	if (w > 1)
	{
		w--;
	}
	xpos = x + w/2 + (size-1)*w;

	if (val == 0)
	{
		pic = BigFont->GetChar ('0', &v);
		screen->DrawTexture (pic, xpos - v/2 + 2, y + 2,
			DTA_HUDRules, HUD_Normal,
			DTA_Alpha, HR_SHADOW,
			DTA_FillColor, 0,
			DTA_Translation, BigFont->GetColorTranslation (CR_UNTRANSLATED),
			TAG_DONE);
		screen->DrawTexture (pic, xpos - v/2, y,
			DTA_HUDRules, HUD_Normal,
			DTA_Translation, BigFont->GetColorTranslation (CR_UNTRANSLATED),
			TAG_DONE);
		return;
	}
	else if (val < 0)
	{
		negative = true;
		val = -val;
	}

	int oval = val;
	int oxpos = xpos;

	// First the shadow
	while (val != 0)
	{
		pic = BigFont->GetChar ('0' + val % 10, &v);
		screen->DrawTexture (pic, xpos - v/2 + 2, y + 2,
			DTA_HUDRules, HUD_Normal,
			DTA_Alpha, HR_SHADOW,
			DTA_FillColor, 0,
			DTA_Translation, BigFont->GetColorTranslation (CR_UNTRANSLATED),
			TAG_DONE);
		val /= 10;
		xpos -= w;
	}
	if (negative)
	{
		pic = BigFont->GetChar ('-', &v);
		if (pic != NULL)
		{
			screen->DrawTexture (pic, xpos - v/2 + 2, y + 2,
				DTA_HUDRules, HUD_Normal,
				DTA_Alpha, HR_SHADOW,
				DTA_FillColor, 0,
				DTA_Translation, BigFont->GetColorTranslation (CR_UNTRANSLATED),
				TAG_DONE);
		}
	}

	// Then the foreground number
	val = oval;
	xpos = oxpos;
	while (val != 0)
	{
		pic = BigFont->GetChar ('0' + val % 10, &v);
		screen->DrawTexture (pic, xpos - v/2, y,
			DTA_HUDRules, HUD_Normal,
			DTA_Translation, BigFont->GetColorTranslation (CR_UNTRANSLATED),
			TAG_DONE);
		val /= 10;
		xpos -= w;
	}
	if (negative)
	{
		pic = BigFont->GetChar ('-', &v);
		if (pic != NULL)
		{
			screen->DrawTexture (pic, xpos - v/2, y,
				DTA_HUDRules, HUD_Normal,
				DTA_Translation, BigFont->GetColorTranslation (CR_UNTRANSLATED),
				TAG_DONE);
		}
	}
}

//---------------------------------------------------------------------------
//
// PROC DrSmallNumberOuter
//
// Draws a small three digit number outside the status bar.
//
//---------------------------------------------------------------------------

void DBaseStatusBar::DrSmallNumberOuter (int val, int x, int y, bool center) const
{
	int digit = 0;

	if (val > 999)
	{
		val = 999;
	}
	if (val > 99)
	{
		digit = val / 100;
		screen->DrawTexture (Images[imgSmNumbers + digit], x, y,
			DTA_HUDRules, center ? HUD_HorizCenter : HUD_Normal, TAG_DONE);
		val -= digit * 100;
	}
	if (val > 9 || digit)
	{
		digit = val / 10;
		screen->DrawTexture (Images[imgSmNumbers + digit], x+4, y,
			DTA_HUDRules, center ? HUD_HorizCenter : HUD_Normal, TAG_DONE);
		val -= digit * 10;
	}
	screen->DrawTexture (Images[imgSmNumbers + val], x+8, y,
		DTA_HUDRules, center ? HUD_HorizCenter : HUD_Normal, TAG_DONE);
}

//---------------------------------------------------------------------------
//
// RefreshBackground
//
//---------------------------------------------------------------------------

void DBaseStatusBar::RefreshBackground () const
{
	int x, x2, y, ratio;

	ratio = CheckRatio (SCREENWIDTH, SCREENHEIGHT);
	x = (!(ratio & 3) || !Scaled) ? ST_X : SCREENWIDTH*(48-BaseRatioSizes[ratio][3])/(48*2);
	y = x == ST_X && x > 0 ? ST_Y : ::ST_Y;

	if(!CompleteBorder)
	{
		if(y < SCREENHEIGHT)
		{
			V_DrawBorder (x+1, y, SCREENWIDTH, y+1);
			V_DrawBorder (x+1, SCREENHEIGHT-1, SCREENWIDTH, SCREENHEIGHT);
		}
	}
	else
	{
		x = SCREENWIDTH;
	}

	if (x > 0)
	{
		if(!CompleteBorder)
		{
			x2 = !(ratio & 3) || !Scaled ? ST_X+HorizontalResolution :
				SCREENWIDTH - (SCREENWIDTH*(48-BaseRatioSizes[ratio][3])+48*2-1)/(48*2);
		}
		else
		{
			x2 = SCREENWIDTH;
		}

		V_DrawBorder (0, y, x+1, SCREENHEIGHT);
		V_DrawBorder (x2-1, y, SCREENWIDTH, SCREENHEIGHT);

		if (setblocks >= 10)
		{
			FTexture *p = TexMan[gameinfo.Border.b];
			if (p != NULL)
			{
				screen->FlatFill(0, y, x, y + p->GetHeight(), p, true);
				screen->FlatFill(x2, y, SCREENWIDTH, y + p->GetHeight(), p, true);
			}
		}
	}
}

//---------------------------------------------------------------------------
//
// DrawCrosshair
//
//---------------------------------------------------------------------------

void DBaseStatusBar::DrawCrosshair ()
{
	static DWORD prevcolor = 0xffffffff;
	static int palettecolor = 0;

	DWORD color;
	fixed_t size;
	int w, h;

	// Don't draw the crosshair in chasecam mode
	if (players[consoleplayer].cheats & CF_CHASECAM)
		return;

	ST_LoadCrosshair();

	// Don't draw the crosshair if there is none
	if (CrosshairImage == NULL || gamestate == GS_TITLELEVEL || camera->health <= 0)
	{
		return;
	}

	if (crosshairscale)
	{
		size = SCREENHEIGHT * FRACUNIT / 200;
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
		int health = Scale(CPlayer->health, 100, CPlayer->mo->GetDefault()->health);

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
		viewwidth / 2 + viewwindowx,
		viewheight / 2 + viewwindowy,
		DTA_DestWidth, w,
		DTA_DestHeight, h,
		DTA_AlphaChannel, true,
		DTA_FillColor, (palettecolor << 24) | (color & 0xFFFFFF),
		TAG_DONE);
}

//---------------------------------------------------------------------------
//
// FlashCrosshair
//
//---------------------------------------------------------------------------

void DBaseStatusBar::FlashCrosshair ()
{
	CrosshairSize = XHAIRPICKUPSIZE;
}

//---------------------------------------------------------------------------
//
// DrawMessages
//
//---------------------------------------------------------------------------

void DBaseStatusBar::DrawMessages (int layer, int bottom)
{
	DHUDMessage *msg = Messages[layer];
	int visibility = 0;

	if (viewactive)
	{
		visibility |= HUDMSG_NotWith3DView;
	}
	if (automapactive)
	{
		visibility |= viewactive ? HUDMSG_NotWithOverlayMap : HUDMSG_NotWithFullMap;
	}
	while (msg)
	{
		DHUDMessage *next = msg->Next;
		msg->Draw (bottom, visibility);
		msg = next;
	}
}

//---------------------------------------------------------------------------
//
// Draw
//
//---------------------------------------------------------------------------

void DBaseStatusBar::Draw (EHudState state)
{
	// HUD_AltHud state is for popups only
	if (state == HUD_AltHud)
		return;

	char line[64+10];

	if ((SB_state != 0 || BorderNeedRefresh) && state == HUD_StatusBar)
	{
		RefreshBackground ();
	}

	if (idmypos)
	{ // Draw current coordinates
		int height = SmallFont->GetHeight();
		char labels[3] = { 'X', 'Y', 'Z' };
		fixed_t *value;
		int i;

		int vwidth;
		int vheight;
		int xpos;
		int y;

		if (con_scaletext == 0)
		{
			vwidth = SCREENWIDTH;
			vheight = SCREENHEIGHT;
			xpos = vwidth - 80;
			y = ::ST_Y - height;
		}
		else
		{
			vwidth = SCREENWIDTH/2;
			vheight = SCREENHEIGHT/2;
			xpos = vwidth - SmallFont->StringWidth("X: -00000")-6;
			y = ::ST_Y/2 - height;
		}

		if (gameinfo.gametype == GAME_Strife)
		{
			if (con_scaletext == 0)
				y -= height * 4;
			else
				y -= height * 2;
		}

		fixedvec3 pos = CPlayer->mo->Pos();
		for (i = 2, value = &pos.z; i >= 0; y -= height, --value, --i)
		{
			mysnprintf (line, countof(line), "%c: %d", labels[i], *value >> FRACBITS);
			screen->DrawText (SmallFont, CR_GREEN, xpos, y, line, 
				DTA_KeepRatio, true,
				DTA_VirtualWidth, vwidth, DTA_VirtualHeight, vheight, 				
				TAG_DONE);
			V_SetBorderNeedRefresh();
		}
	}

	if (viewactive)
	{
		if (CPlayer && CPlayer->camera && CPlayer->camera->player)
		{
			DrawCrosshair ();
		}
	}
	else if (automapactive)
	{
		int y, time = Tics2Seconds(level.time), height;
		int totaltime = Tics2Seconds(level.totaltime);
		EColorRange highlight = (gameinfo.gametype & GAME_DoomChex) ?
			CR_UNTRANSLATED : CR_YELLOW;

		height = SmallFont->GetHeight () * CleanYfac;

		// Draw timer
		y = 8;
		if (am_showtime)
		{
			mysnprintf (line, countof(line), "%02d:%02d:%02d", time/3600, (time%3600)/60, time%60);	// Time
			screen->DrawText (SmallFont, CR_GREY, SCREENWIDTH - 80*CleanXfac, y, line, DTA_CleanNoMove, true, TAG_DONE);
			y+=8*CleanYfac;
		}
		if (am_showtotaltime)
		{
			mysnprintf (line, countof(line), "%02d:%02d:%02d", totaltime/3600, (totaltime%3600)/60, totaltime%60);	// Total time
			screen->DrawText (SmallFont, CR_GREY, SCREENWIDTH - 80*CleanXfac, y, line, DTA_CleanNoMove, true, TAG_DONE);
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
				y -= Scale (11, SCREENHEIGHT, 200);
			}
			else
			{
				if (SCREENWIDTH < 640)
				{
					y -= 12;
				}
				else
				{ // Get past the tops of the gargoyles' wings
					y -= 28;
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
		FString mapname;

		ST_FormatMapName(mapname, TEXTCOLOR_GREY);
		screen->DrawText (SmallFont, highlight,
			(SCREENWIDTH - SmallFont->StringWidth (mapname)*CleanXfac)/2, y, mapname,
			DTA_CleanNoMove, true, TAG_DONE);

		if (!deathmatch)
		{
			int y = 8;

			// Draw monster count
			if (am_showmonsters)
			{
				mysnprintf (line, countof(line), "%s" TEXTCOLOR_GREY " %d/%d",
					GStrings("AM_MONSTERS"), level.killed_monsters, level.total_monsters);
				screen->DrawText (SmallFont, highlight, 8, y, line,
					DTA_CleanNoMove, true, TAG_DONE);
				y += height;
			}

			// Draw secret count
			if (am_showsecrets)
			{
				mysnprintf (line, countof(line), "%s" TEXTCOLOR_GREY " %d/%d",
					GStrings("AM_SECRETS"), level.found_secrets, level.total_secrets);
				screen->DrawText (SmallFont, highlight, 8, y, line,
					DTA_CleanNoMove, true, TAG_DONE);
				y += height;
			}

			// Draw item count
			if (am_showitems)
			{
				mysnprintf (line, countof(line), "%s" TEXTCOLOR_GREY " %d/%d",
					GStrings("AM_ITEMS"), level.found_items, level.total_items);
				screen->DrawText (SmallFont, highlight, 8, y, line,
					DTA_CleanNoMove, true, TAG_DONE);
			}
		}
	}
}


void DBaseStatusBar::DrawLog ()
{
	int hudwidth, hudheight;

	if (CPlayer->LogText && *CPlayer->LogText)
	{
		// This uses the same scaling as regular HUD messages
		switch (con_scaletext)
		{
		default:
			hudwidth = SCREENWIDTH;
			hudheight = SCREENHEIGHT;
			break;

		case 1:
			hudwidth = SCREENWIDTH / CleanXfac;
			hudheight = SCREENHEIGHT / CleanYfac;
			break;

		case 2:
			hudwidth = SCREENWIDTH / 2;
			hudheight = SCREENHEIGHT / 2;
			break;
		}

		int linelen = hudwidth<640? Scale(hudwidth,9,10)-40 : 560;
		FBrokenLines *lines = V_BreakLines (SmallFont, linelen, CPlayer->LogText);
		int height = 20;

		for (int i = 0; lines[i].Width != -1; i++) height += SmallFont->GetHeight () + 1;

		int x,y,w;

		if (linelen<560)
		{
			x=hudwidth/20;
			y=hudheight/8;
			w=hudwidth-2*x;
		}
		else
		{
			x=(hudwidth>>1)-300;
			y=hudheight*3/10-(height>>1);
			if (y<0) y=0;
			w=600;
		}
		screen->Dim(0, 0.5f, Scale(x, SCREENWIDTH, hudwidth), Scale(y, SCREENHEIGHT, hudheight), 
							 Scale(w, SCREENWIDTH, hudwidth), Scale(height, SCREENHEIGHT, hudheight));
		x+=20;
		y+=10;
		for (int i = 0; lines[i].Width != -1; i++)
		{

			screen->DrawText (SmallFont, CR_UNTRANSLATED, x, y, lines[i].Text,
				DTA_KeepRatio, true,
				DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight, TAG_DONE);
			y += SmallFont->GetHeight ()+1;
		}

		V_FreeBrokenLines (lines);
	}
}

bool DBaseStatusBar::MustDrawLog(EHudState)
{
	return true;
}

void DBaseStatusBar::SetMugShotState(const char *stateName, bool waitTillDone, bool reset)
{
}

//---------------------------------------------------------------------------
//
// DrawBottomStuff
//
//---------------------------------------------------------------------------

void DBaseStatusBar::DrawBottomStuff (EHudState state)
{
	DrawMessages (HUDMSGLayer_UnderHUD, (state == HUD_StatusBar) ? ::ST_Y : SCREENHEIGHT);
}

//---------------------------------------------------------------------------
//
// DrawTopStuff
//
//---------------------------------------------------------------------------

void DBaseStatusBar::DrawTopStuff (EHudState state)
{
	if (demoplayback && demover != DEMOGAMEVERSION)
	{
		screen->DrawText (SmallFont, CR_TAN, 0, ST_Y - 40 * CleanYfac,
			"Demo was recorded with a different version\n"
			"of " GAMENAME ". Expect it to go out of sync.",
			DTA_CleanNoMove, true, TAG_DONE);
	}

	DrawPowerups ();
	if (automapactive && !viewactive)
	{
		DrawMessages (HUDMSGLayer_OverMap, (state == HUD_StatusBar) ? ::ST_Y : SCREENHEIGHT);
	}
	DrawMessages (HUDMSGLayer_OverHUD, (state == HUD_StatusBar) ? ::ST_Y : SCREENHEIGHT);
	DrawConsistancy ();
	DrawWaiting ();
	if (ShowLog && MustDrawLog(state)) DrawLog ();

	if (noisedebug)
	{
		S_NoiseDebug ();
	}
}

//---------------------------------------------------------------------------
//
// DrawPowerups
//
//---------------------------------------------------------------------------

void DBaseStatusBar::DrawPowerups ()
{
	// Each icon gets a 32x32 block to draw itself in.
	int x, y;
	AInventory *item;
	const int yshift = SmallFont->GetHeight();

	x = -20;
	y = 17 
		+ (ST_IsTimeVisible()    ? yshift : 0)
		+ (ST_IsLatencyVisible() ? yshift : 0);
	for (item = CPlayer->mo->Inventory; item != NULL; item = item->Inventory)
	{
		if (item->DrawPowerup (x, y))
		{
			x -= POWERUPICONSIZE;
			if (x < -POWERUPICONSIZE*5)
			{
				x = -20;
				y += POWERUPICONSIZE*2;
			}
		}
	}
}

//---------------------------------------------------------------------------
//
// BlendView
//
//---------------------------------------------------------------------------

void DBaseStatusBar::BlendView (float blend[4])
{
	V_AddBlend (BaseBlendR / 255.f, BaseBlendG / 255.f, BaseBlendB / 255.f, BaseBlendA, blend);
	V_AddPlayerBlend(CPlayer, blend, 1.0f, 228);

	if (screen->Accel2D || (CPlayer->camera != NULL && menuactive == MENU_Off && ConsoleState == c_up))
	{
		player_t *player = (CPlayer->camera != NULL && CPlayer->camera->player != NULL) ? CPlayer->camera->player : CPlayer;
		V_AddBlend (player->BlendR, player->BlendG, player->BlendB, player->BlendA, blend);
	}

	V_SetBlend ((int)(blend[0] * 255.0f), (int)(blend[1] * 255.0f),
				(int)(blend[2] * 255.0f), (int)(blend[3] * 256.0f));
}

void DBaseStatusBar::DrawConsistancy () const
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
		}
		screen->DrawText (SmallFont, CR_GREEN, 
			(screen->GetWidth() - SmallFont->StringWidth (conbuff)*CleanXfac) / 2,
			0, conbuff, DTA_CleanNoMove, true, TAG_DONE);
		BorderTopRefresh = screen->GetPageCount ();
	}
}

void DBaseStatusBar::DrawWaiting () const
{
	int i;
	char conbuff[64], *buff_p;

	if (!netgame)
		return;

	buff_p = NULL;
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && players[i].waiting)
		{
			if (buff_p == NULL)
			{
				strcpy (conbuff, "Waiting for:");
				buff_p = conbuff + 12;
			}
			*buff_p++ = ' ';
			*buff_p++ = '1' + i;
			*buff_p = 0;
		}
	}

	if (buff_p != NULL)
	{
		screen->DrawText (SmallFont, CR_ORANGE, 
			(screen->GetWidth() - SmallFont->StringWidth (conbuff)*CleanXfac) / 2,
			SmallFont->GetHeight()*CleanYfac, conbuff, DTA_CleanNoMove, true, TAG_DONE);
		BorderTopRefresh = screen->GetPageCount ();
	}
}

void DBaseStatusBar::FlashItem (const PClass *itemtype)
{
}

void DBaseStatusBar::NewGame ()
{
}

void DBaseStatusBar::SetInteger (int pname, int param)
{
}

void DBaseStatusBar::ShowPop (int popnum)
{
	ShowLog = (popnum == POP_Log && !ShowLog);
}

void DBaseStatusBar::ReceivedWeapon (AWeapon *weapon)
{
}

void DBaseStatusBar::Serialize (FArchive &arc)
{
	if (SaveVersion < 3821)
	{
		memset(Messages, 0, sizeof(Messages));
		arc << Messages[HUDMSGLayer_Default];
	}
	else
	{
		for (unsigned int i = 0; i < countof(Messages); ++i)
		{
			arc << Messages[i];
		}
	}
}

void DBaseStatusBar::ScreenSizeChanged ()
{
	st_scale.Callback ();
	ST_SetNeedRefresh();

	for (unsigned int i = 0; i < countof(Messages); ++i)
	{
		DHUDMessage *message = Messages[i];
		while (message != NULL)
		{
			message->ScreenSizeChanged ();
			message = message->Next;
		}
	}
}

//---------------------------------------------------------------------------
//
// ValidateInvFirst
//
// Returns an inventory item that, when drawn as the first item, is sure to
// include the selected item in the inventory bar.
//
//---------------------------------------------------------------------------

AInventory *DBaseStatusBar::ValidateInvFirst (int numVisible) const
{
	AInventory *item;
	int i;

	if (CPlayer->mo->InvFirst == NULL)
	{
		CPlayer->mo->InvFirst = CPlayer->mo->FirstInv();
		if (CPlayer->mo->InvFirst == NULL)
		{ // Nothing to show
			return NULL;
		}
	}

	assert (CPlayer->mo->InvFirst->Owner == CPlayer->mo);

	// If there are fewer than numVisible items shown, see if we can shift the
	// view left to show more.
	for (i = 0, item = CPlayer->mo->InvFirst; item != NULL && i < numVisible; ++i, item = item->NextInv())
	{ }

	while (i < numVisible)
	{
		item = CPlayer->mo->InvFirst->PrevInv ();
		if (item == NULL)
		{
			break;
		}
		else
		{
			CPlayer->mo->InvFirst = item;
			++i;
		}
	}

	if (CPlayer->mo->InvSel == NULL)
	{
		// Nothing selected, so don't move the view.
		return CPlayer->mo->InvFirst == NULL ? CPlayer->mo->Inventory : CPlayer->mo->InvFirst;
	}
	else
	{
		// Check if InvSel is already visible
		for (item = CPlayer->mo->InvFirst, i = numVisible;
			 item != NULL && i != 0;
			 item = item->NextInv(), --i)
		{
			if (item == CPlayer->mo->InvSel)
			{
				return CPlayer->mo->InvFirst;
			}
		}
		// Check if InvSel is to the right of the visible range
		for (i = 1; item != NULL; item = item->NextInv(), ++i)
		{
			if (item == CPlayer->mo->InvSel)
			{
				// Found it. Now advance InvFirst
				for (item = CPlayer->mo->InvFirst; i != 0; --i)
				{
					item = item->NextInv();
				}
				return item;
			}
		}
		// Check if InvSel is to the left of the visible range
		for (item = CPlayer->mo->Inventory;
			item != CPlayer->mo->InvSel;
			item = item->NextInv())
		{ }
		if (item != NULL)
		{
			// Found it, so let it become the first item shown
			return item;
		}
		// Didn't find the selected item, so don't move the view.
		// This should never happen, so let debug builds assert.
		assert (item != NULL);
		return CPlayer->mo->InvFirst;
	}
}

//============================================================================
//
// DBaseStatusBar :: GetCurrentAmmo
//
// Returns the types and amounts of ammo used by the current weapon. If the
// weapon only uses one type of ammo, it is always returned as ammo1.
//
//============================================================================

void DBaseStatusBar::GetCurrentAmmo (AAmmo *&ammo1, AAmmo *&ammo2, int &ammocount1, int &ammocount2) const
{
	if (CPlayer->ReadyWeapon != NULL)
	{
		ammo1 = CPlayer->ReadyWeapon->Ammo1;
		ammo2 = CPlayer->ReadyWeapon->Ammo2;
		if (ammo1 == NULL)
		{
			ammo1 = ammo2;
			ammo2 = NULL;
		}
	}
	else
	{
		ammo1 = ammo2 = NULL;
	}
	ammocount1 = ammo1 != NULL ? ammo1->Amount : 0;
	ammocount2 = ammo2 != NULL ? ammo2->Amount : 0;
}

//============================================================================
//
// CCMD showpop
//
// Asks the status bar to show a pop screen.
//
//============================================================================

CCMD (showpop)
{
	if (argv.argc() != 2)
	{
		Printf ("Usage: showpop <popnumber>\n");
	}
	else if (StatusBar != NULL)
	{
		int popnum = atoi (argv[1]);
		if (popnum < 0)
		{
			popnum = 0;
		}
		StatusBar->ShowPop (popnum);
	}
}

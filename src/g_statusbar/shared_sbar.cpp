/*
** shared_sbar.cpp
** Base status bar implementation
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** Copyright 2017 Christoph Oelckers
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
#include "serializer.h"
#include "gstrings.h"
#include "r_utility.h"
#include "cmdlib.h"
#include "g_levellocals.h"
#include "vm.h"
#include "p_acs.h"
#include "r_data/r_translate.h"
#include "sbarinfo.h"
#include "events.h"

#include "../version.h"

#define XHAIRSHRINKSIZE		(1./18)
#define XHAIRPICKUPSIZE		(2+XHAIRSHRINKSIZE)
#define POWERUPICONSIZE		32

IMPLEMENT_CLASS(DBaseStatusBar, false, true)

IMPLEMENT_POINTERS_START(DBaseStatusBar)
	IMPLEMENT_POINTER(Messages[0])
	IMPLEMENT_POINTER(Messages[1])
	IMPLEMENT_POINTER(Messages[2])
IMPLEMENT_POINTERS_END

EXTERN_CVAR (Bool, am_showmonsters)
EXTERN_CVAR (Bool, am_showsecrets)
EXTERN_CVAR (Bool, am_showitems)
EXTERN_CVAR (Bool, am_showtime)
EXTERN_CVAR (Bool, am_showtotaltime)
EXTERN_CVAR (Bool, noisedebug)
EXTERN_CVAR (Int, con_scaletext)
EXTERN_CVAR(Bool, vid_fps)
CVAR(Int, hud_scale, 0, CVAR_ARCHIVE);


DBaseStatusBar *StatusBar;

extern int setblocks;

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
CUSTOM_CVAR (Int, st_scale, 0, CVAR_ARCHIVE)
{
	if (self < -1)
	{
		self = -1;
		return;
	}
	if (StatusBar)
	{
		StatusBar->SetScale();
		setsizeneeded = true;
	}
}
CUSTOM_CVAR(Bool, hud_aspectscale, false, CVAR_ARCHIVE)
{
	if (StatusBar)
	{
		StatusBar->SetScale();
		setsizeneeded = true;
	}
}

CVAR (Int, crosshair, 0, CVAR_ARCHIVE)
CVAR (Bool, crosshairforce, false, CVAR_ARCHIVE)
CVAR (Color, crosshaircolor, 0xff0000, CVAR_ARCHIVE);
CVAR (Bool, crosshairhealth, true, CVAR_ARCHIVE);
CVAR (Float, crosshairscale, 1.0, CVAR_ARCHIVE);
CVAR (Bool, crosshairgrow, false, CVAR_ARCHIVE);
CUSTOM_CVAR(Int, am_showmaplabel, 2, CVAR_ARCHIVE)
{
	if (self < 0 || self > 2) self = 2;
}

CVAR (Bool, idmypos, false, 0);
CVAR(Float, underwater_fade_scalar, 1.0f, CVAR_ARCHIVE) // [Nash] user-settable underwater blend intensity

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

DEFINE_ACTION_FUNCTION(FLevelLocals, FormatMapName)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_INT(cr);
	char mapnamecolor[3] = { '\34', char(cr + 'A'), 0 };
	FString rets;
	ST_FormatMapName(rets, mapnamecolor);
	ACTION_RETURN_STRING(rets);
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
// create a new status bar
//
//---------------------------------------------------------------------------

static void CreateBaseStatusBar()
{
	assert(nullptr == StatusBar);

	PClass* const statusBarClass = PClass::FindClass("BaseStatusBar");
	assert(nullptr != statusBarClass);

	StatusBar = static_cast<DBaseStatusBar*>(statusBarClass->CreateNew());
	StatusBar->SetSize(0);
}

void ST_CreateStatusBar(bool bTitleLevel)
{
	if (StatusBar != NULL)
	{
		StatusBar->Destroy();
		StatusBar = NULL;
	}

	if (bTitleLevel)
	{
		CreateBaseStatusBar();
	}
	else
	{
		// The old rule of 'what came last wins' goes here, as well.
		// If the most recent SBARINFO definition comes before a status bar class definition it will be picked,
		// if the class is defined later, this will be picked. If both come from the same file, the class definition will win.
		int sbarinfolump = Wads.CheckNumForName("SBARINFO");
		int sbarinfofile = Wads.GetLumpFile(sbarinfolump);
		if (gameinfo.statusbarclassfile >= gameinfo.statusbarfile && gameinfo.statusbarclassfile >= sbarinfofile)
		{
			auto cls = PClass::FindClass(gameinfo.statusbarclass);
			if (cls != nullptr)
			{
				StatusBar = (DBaseStatusBar *)cls->CreateNew();
			}
		}
	}
	if (StatusBar == nullptr && SBarInfoScript[SCRIPT_CUSTOM] != nullptr)
	{
		int cstype = SBarInfoScript[SCRIPT_CUSTOM]->GetGameType();

		//Did the user specify a "base"
		if (cstype == GAME_Any) //Use the default, empty or custom.
		{
			StatusBar = CreateCustomStatusBar(SCRIPT_CUSTOM);
		}
		else
		{
			StatusBar = CreateCustomStatusBar(SCRIPT_DEFAULT);
		}
		// SBARINFO failed so try the current statusbarclass again.
		if (StatusBar == nullptr)
		{
			auto cls = PClass::FindClass(gameinfo.statusbarclass);
			if (cls != nullptr)
			{
				StatusBar = (DBaseStatusBar *)cls->CreateNew();
			}
		}
	}
	if (StatusBar == nullptr)
	{
		FName defname;

		if (gameinfo.gametype & GAME_DoomChex) defname = "DoomStatusBar";
		else if (gameinfo.gametype == GAME_Heretic) defname = "HereticStatusBar";
		else if (gameinfo.gametype == GAME_Hexen) defname = "HexenStatusBar";
		else if (gameinfo.gametype == GAME_Strife) defname = "StrifeStatusBar";
		if (defname != NAME_None)
		{
			auto cls = PClass::FindClass(defname);
			if (cls != nullptr)
			{
				StatusBar = (DBaseStatusBar *)cls->CreateNew();
			}
		}
	}
	if (StatusBar == nullptr)
	{
		CreateBaseStatusBar();
	}

	IFVIRTUALPTR(StatusBar, DBaseStatusBar, Init)
	{
		VMValue params[] = { StatusBar };
		VMCall(func, params, 1, nullptr, 0);
	}

	GC::WriteBarrier(StatusBar);
	StatusBar->AttachToPlayer(&players[consoleplayer]);
	StatusBar->NewGame();
}
//---------------------------------------------------------------------------
//
// Constructor
//
//---------------------------------------------------------------------------

DBaseStatusBar::DBaseStatusBar ()
{
	CompleteBorder = false;
	Centering = false;
	FixedOrigin = false;
	CrosshairSize = 1.;
	memset(Messages, 0, sizeof(Messages));
	Displacement = 0;
	CPlayer = NULL;
	ShowLog = false;
	defaultScale = { (double)CleanXfac, (double)CleanYfac };
}

void DBaseStatusBar::SetSize(int reltop, int hres, int vres, int hhres, int hvres)
{
	BaseRelTop = reltop;
	BaseSBarHorizontalResolution = hres;
	BaseSBarVerticalResolution = vres;
	BaseHUDHorizontalResolution = hhres < 0? hres : hhres;
	BaseHUDVerticalResolution = hvres < 0? vres : hvres;
	SetDrawSize(reltop, hres, vres);
}

void DBaseStatusBar::SetDrawSize(int reltop, int hres, int vres)
{
	RelTop = reltop;
	HorizontalResolution = hres;
	VerticalResolution = vres;
	int x, y;
	V_CalcCleanFacs(hres, vres, SCREENWIDTH, SCREENHEIGHT, &x, &y);
	defaultScale = { (double)x, (double)y };

	SetScale();	// recalculate positioning info.
}


DEFINE_ACTION_FUNCTION(DBaseStatusBar, SetSize)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	PARAM_INT(rt);
	PARAM_INT(vw);
	PARAM_INT(vh);
	PARAM_INT_DEF(hvw);
	PARAM_INT_DEF(hvh);
	self->SetSize(rt, vw, vh, hvw, hvh);
	return 0;
}


//---------------------------------------------------------------------------
//
// PROP Destroy
//
//---------------------------------------------------------------------------

void DBaseStatusBar::OnDestroy ()
{
	for (size_t i = 0; i < countof(Messages); ++i)
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
	Super::OnDestroy();
}

//---------------------------------------------------------------------------
//
// PROC SetScaled
//
//---------------------------------------------------------------------------

void DBaseStatusBar::SetScale ()
{
	int w = SCREENWIDTH;
	int h = SCREENHEIGHT;
	if (st_scale < 0 || ForcedScale)
	{
		// This is the classic fullscreen scale with aspect ratio compensation.
		int sby = VerticalResolution - RelTop;
		float aspect = ActiveRatio(w, h);
		if (!AspectTallerThanWide(aspect))
		{ 
			// Wider or equal than 4:3 
			SBarTop = Scale(sby, h, VerticalResolution);
			double width4_3 = w * 1.333 / aspect;
			ST_X = int((w - width4_3) / 2);
		}
		else
		{ // 5:4 resolution
			ST_X = 0;

			// this was far more obtuse before...
			double height4_3 = h * aspect / 1.333;
			SBarTop = int(h - height4_3 + sby * height4_3 / VerticalResolution);
		}
		Displacement = 0;
		SBarScale.X = -1;
		ST_Y = 0;
	}
	else
	{
		// Since status bars and HUDs can be designed for non 320x200 screens this needs to be factored in here.
		// The global scaling factors are for resources at 320x200, so if the actual ones are higher resolution
		// the resulting scaling factor needs to be reduced accordingly.
		int realscale = clamp((320 * GetUIScale(st_scale)) / HorizontalResolution, 1, w / HorizontalResolution);

		double realscaley = realscale * (hud_aspectscale ? 1.2 : 1.);

		ST_X = (w - HorizontalResolution * realscale) / 2;
		SBarTop = int(h - RelTop * realscaley);
		if (RelTop > 0)
		{
			Displacement = double((SBarTop * VerticalResolution / h) - (VerticalResolution - RelTop))/RelTop/realscaley;
		}
		else
		{
			Displacement = 0;
		}
		SBarScale.X = realscale;
		SBarScale.Y = realscaley;
		ST_Y = int(h - VerticalResolution * realscaley);
	}
}

//---------------------------------------------------------------------------
//
// PROC GetHUDScale
//
//---------------------------------------------------------------------------

DVector2 DBaseStatusBar::GetHUDScale() const
{
	int scale;
	if (hud_scale < 0 || ForcedScale)	// a negative value is the equivalent to the old boolean hud_scale. This can yield different values for x and y for higher resolutions.
	{
		return defaultScale;
	}
	scale = GetUIScale(hud_scale);

	// Since status bars and HUDs can be designed for non 320x200 screens this needs to be factored in here.
	// The global scaling factors are for resources at 320x200, so if the actual ones are higher resolution
	// the resulting scaling factor needs to be reduced accordingly.
	int realscale = MAX<int>(1, (320 * scale) / HorizontalResolution);
	return{ double(realscale), double(realscale * (hud_aspectscale ? 1.2 : 1.)) };
}

DEFINE_ACTION_FUNCTION(DBaseStatusBar, GetHUDScale)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	ACTION_RETURN_VEC2(self->GetHUDScale());
}

//---------------------------------------------------------------------------
//
// PROC GetHUDScale
//
//---------------------------------------------------------------------------

void DBaseStatusBar::BeginStatusBar(int resW, int resH, int relTop, bool forceScaled)
{
	SetDrawSize(relTop < 0? BaseRelTop : relTop, resW < 0? BaseSBarHorizontalResolution : resW, resH < 0? BaseSBarVerticalResolution : resH);
	ForcedScale = forceScaled;
	fullscreenOffsets = false;
}

DEFINE_ACTION_FUNCTION(DBaseStatusBar, BeginStatusBar)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	PARAM_BOOL_DEF(fs);
	PARAM_INT_DEF(w);
	PARAM_INT_DEF(h);
	PARAM_INT_DEF(r);
	self->BeginStatusBar(w, h, r, fs);
	return 0;
}
//---------------------------------------------------------------------------
//
// PROC GetHUDScale
//
//---------------------------------------------------------------------------

void DBaseStatusBar::BeginHUD(int resW, int resH, double Alpha, bool forcescaled)
{
	SetDrawSize(RelTop, resW < 0? BaseHUDHorizontalResolution : resW, resH < 0? BaseHUDVerticalResolution : resH);	
	this->Alpha = Alpha;
	ForcedScale = forcescaled;
	CompleteBorder = false;
	fullscreenOffsets = true;
}

DEFINE_ACTION_FUNCTION(DBaseStatusBar, BeginHUD)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	PARAM_FLOAT_DEF(a);
	PARAM_BOOL_DEF(fs);
	PARAM_INT_DEF(w);
	PARAM_INT_DEF(h);
	self->BeginHUD(w, h, a, fs);
	return 0;
}

//---------------------------------------------------------------------------
//
// PROC AttachToPlayer
//
//---------------------------------------------------------------------------

void DBaseStatusBar::AttachToPlayer(player_t *player)
{
	IFVIRTUAL(DBaseStatusBar, AttachToPlayer)
	{
		VMValue params[] = { (DObject*)this, player };
		VMCall(func, params, countof(params), nullptr, 0);
	}
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
// PROC Tick
//
//---------------------------------------------------------------------------

void DBaseStatusBar::Tick ()
{
	for (size_t i = 0; i < countof(Messages); ++i)
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
		if (CrosshairSize > 1.)
		{
			CrosshairSize -= XHAIRSHRINKSIZE;
			if (CrosshairSize < 1.)
			{
				CrosshairSize = 1.;
			}
		}
	}

	if (artiflashTick > 0)
		artiflashTick--;

	if (itemflashFade > 0)
	{
		itemflashFade -= 1 / 14.;
		if (itemflashFade < 0)
		{
			itemflashFade = 0;
		}
	}

}

DEFINE_ACTION_FUNCTION(DBaseStatusBar, Tick)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	self->Tick();
	return 0;
}

void DBaseStatusBar::CallTick()
{
	IFVIRTUAL(DBaseStatusBar, Tick)
	{
		VMValue params[] = { (DObject*)this };
		VMCall(func, params, countof(params), nullptr, 0);
	}
	else Tick();
	mugshot.Tick(CPlayer);
}

//---------------------------------------------------------------------------
//
// PROC AttachMessage
//
//---------------------------------------------------------------------------

void DBaseStatusBar::AttachMessage (DHUDMessage *msg, uint32_t id, int layer)
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
	for (size_t i = 0; i < countof(Messages); ++i)
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
			return probe;
		}
	}
	return NULL;
}

DHUDMessage *DBaseStatusBar::DetachMessage (uint32_t id)
{
	for (size_t i = 0; i < countof(Messages); ++i)
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
	for (size_t i = 0; i < countof(Messages); ++i)
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
	AttachMessage (Create<DHUDMessageFadeOut> (SmallFont, CPlayer->userinfo.GetName(),
		1.5f, 0.92f, 0, 0, color, 2.f, 0.35f), MAKE_ID('P','N','A','M'));
}

//---------------------------------------------------------------------------
//
// RefreshBackground
//
//---------------------------------------------------------------------------

void DBaseStatusBar::RefreshBackground () const
{
	int x, x2, y;

	float ratio = ActiveRatio (SCREENWIDTH, SCREENHEIGHT);
	x = ST_X;
	y = SBarTop;

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
			x2 = SCREENWIDTH - ST_X;
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
	uint32_t color;
	double size;
	int w, h;

	// Don't draw the crosshair in chasecam mode
	if (players[consoleplayer].cheats & CF_CHASECAM)
		return;

	ST_LoadCrosshair();

	// Don't draw the crosshair if there is none
	if (CrosshairImage == NULL || gamestate == GS_TITLELEVEL || r_viewpoint.camera->health <= 0)
	{
		return;
	}

	if (crosshairscale > 0.0f)
	{
		size = SCREENHEIGHT * crosshairscale / 200.;
	}
	else
	{
		size = 1.;
	}

	if (crosshairgrow)
	{
		size *= CrosshairSize;
	}
	w = int(CrosshairImage->GetWidth() * size);
	h = int(CrosshairImage->GetHeight() * size);

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

	screen->DrawTexture (CrosshairImage,
		viewwidth / 2 + viewwindowx,
		viewheight / 2 + viewwindowy,
		DTA_DestWidth, w,
		DTA_DestHeight, h,
		DTA_AlphaChannel, true,
		DTA_FillColor, color & 0xFFFFFF,
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

	if (state == HUD_StatusBar)
	{
		RefreshBackground ();
	}

	if (idmypos)
	{ 
		// Draw current coordinates
		IFVIRTUAL(DBaseStatusBar, DrawMyPos)
		{
			VMValue params[] = { (DObject*)this };
			VMCall(func, params, countof(params), nullptr, 0);
		}
		V_SetBorderNeedRefresh();
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
		IFVIRTUAL(DBaseStatusBar, DrawAutomapHUD)
		{
			VMValue params[] = { (DObject*)this, r_viewpoint.TicFrac };
			VMCall(func, params, countof(params), nullptr, 0);
		}
	}
}

DEFINE_ACTION_FUNCTION(DBaseStatusBar, Draw)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	PARAM_INT(state);
	self->Draw((EHudState)state);
	return 0;
}

void DBaseStatusBar::CallDraw(EHudState state)
{
	IFVIRTUAL(DBaseStatusBar, Draw)
	{
		VMValue params[] = { (DObject*)this, state, r_viewpoint.TicFrac };
		VMCall(func, params, countof(params), nullptr, 0);
	}
	else Draw(state);
	screen->ClearClipRect();	// make sure the scripts don't leave a valid clipping rect behind.
	BeginStatusBar(BaseSBarHorizontalResolution, BaseSBarVerticalResolution, BaseRelTop, false);
}



void DBaseStatusBar::DrawLog ()
{
	int hudwidth, hudheight;

	if (CPlayer->LogText.IsNotEmpty())
	{
		// This uses the same scaling as regular HUD messages
		auto scale = active_con_scaletext();
		hudwidth = SCREENWIDTH / scale;
		hudheight = SCREENHEIGHT / scale;

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

bool DBaseStatusBar::MustDrawLog(EHudState state)
{
	IFVIRTUAL(DBaseStatusBar, MustDrawLog)
	{
		VMValue params[] = { (DObject*)this, int(state) };
		int rv;
		VMReturn ret(&rv);
		VMCall(func, params, countof(params), &ret, 1);
		return !!rv;
	}
	return true;
}

DEFINE_ACTION_FUNCTION(DBaseStatusBar, SetMugshotState)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	PARAM_STRING(statename);
	PARAM_BOOL(wait);
	PARAM_BOOL(reset);
	self->mugshot.SetState(statename, wait, reset);
	return 0;
}

void DBaseStatusBar::SetMugShotState(const char *stateName, bool waitTillDone, bool reset)
{
	IFVIRTUAL(DBaseStatusBar, SetMugShotState)
	{
		FString statestring = stateName;
		VMValue params[] = { (DObject*)this, &statestring, waitTillDone, reset };
		VMCall(func, params, countof(params), nullptr, 0);
	}
}

//---------------------------------------------------------------------------
//
// DrawBottomStuff
//
//---------------------------------------------------------------------------

void DBaseStatusBar::DrawBottomStuff (EHudState state)
{
	DrawMessages (HUDMSGLayer_UnderHUD, (state == HUD_StatusBar) ? GetTopOfStatusbar() : SCREENHEIGHT);
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
		screen->DrawText (SmallFont, CR_TAN, 0, GetTopOfStatusbar() - 40 * CleanYfac,
			"Demo was recorded with a different version\n"
			"of " GAMENAME ". Expect it to go out of sync.",
			DTA_CleanNoMove, true, TAG_DONE);
	}

	if (state != HUD_AltHud)
	{
		auto saved = fullscreenOffsets;
		fullscreenOffsets = true;
		IFVIRTUAL(DBaseStatusBar, DrawPowerups)
		{
			VMValue params[] = { (DObject*)this };
			VMCall(func, params, 1, nullptr, 0);
		}
		fullscreenOffsets = saved;
	}

	if (automapactive && !viewactive)
	{
		DrawMessages (HUDMSGLayer_OverMap, (state == HUD_StatusBar) ? GetTopOfStatusbar() : SCREENHEIGHT);
	}
	DrawMessages (HUDMSGLayer_OverHUD, (state == HUD_StatusBar) ? GetTopOfStatusbar() : SCREENHEIGHT);
	E_RenderOverlay(state);

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
// BlendView
//
//---------------------------------------------------------------------------

void DBaseStatusBar::BlendView (float blend[4])
{
	// [Nash] Allow user to set blend intensity
	float cnt = (BaseBlendA * underwater_fade_scalar);

	V_AddBlend (BaseBlendR / 255.f, BaseBlendG / 255.f, BaseBlendB / 255.f, cnt, blend);
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
	IFVIRTUAL(DBaseStatusBar, FlashItem)
	{
		VMValue params[] = { (DObject*)this, (PClass*)itemtype };
		VMCall(func, params, countof(params), nullptr, 0);
	}
}

void DBaseStatusBar::NewGame ()
{
	IFVIRTUAL(DBaseStatusBar, NewGame)
	{
		VMValue params[] = { (DObject*)this };
		VMCall(func, params, countof(params), nullptr, 0);
	}
	mugshot.Reset();
}

void DBaseStatusBar::ShowPop(int pop)
{
	IFVIRTUAL(DBaseStatusBar, ShowPop)
	{
		VMValue params[] = { (DObject*)this, pop };
		VMCall(func, params, countof(params), nullptr, 0);
	}
}



void DBaseStatusBar::SerializeMessages(FSerializer &arc)
{
	arc.Array("hudmessages", Messages, 3, true);
}

void DBaseStatusBar::ScreenSizeChanged ()
{
	// We need to recalculate the sizing info
	SetSize(RelTop, HorizontalResolution, VerticalResolution);

	for (size_t i = 0; i < countof(Messages); ++i)
	{
		DHUDMessage *message = Messages[i];
		while (message != NULL)
		{
			message->ScreenSizeChanged ();
			message = message->Next;
		}
	}
}

DEFINE_ACTION_FUNCTION(DBaseStatusBar, ScreenSizeChanged)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	self->ScreenSizeChanged();
	return 0;
}

void DBaseStatusBar::CallScreenSizeChanged()
{
	IFVIRTUAL(DBaseStatusBar, ScreenSizeChanged)
	{
		VMValue params[] = { (DObject*)this };
		VMCall(func, params, countof(params), nullptr, 0);
	}
	else ScreenSizeChanged();
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

DEFINE_ACTION_FUNCTION(DBaseStatusBar, ValidateInvFirst)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	PARAM_INT(num);
	ACTION_RETURN_POINTER(self->ValidateInvFirst(num));
}


uint32_t DBaseStatusBar::GetTranslation() const
{
	if (gameinfo.gametype & GAME_Raven)
		return TRANSLATION(TRANSLATION_PlayersExtra, int(CPlayer - players));
	return TRANSLATION(TRANSLATION_Players, int(CPlayer - players));
}

//============================================================================
//
// draw stuff
//
//============================================================================

void DBaseStatusBar::StatusbarToRealCoords(double &x, double &y, double &w, double &h) const
{
	if (SBarScale.X == -1 || ForcedScale)
	{
		screen->VirtualToRealCoords(x, y, w, h, HorizontalResolution, VerticalResolution, true, true);
	}
	else
	{
		x = ST_X + x * SBarScale.X;
		y = ST_Y + y * SBarScale.Y;
		w *= SBarScale.X;
		h *= SBarScale.Y;
	}
}

DEFINE_ACTION_FUNCTION(DBaseStatusBar, StatusbarToRealCoords)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	PARAM_FLOAT(x);
	PARAM_FLOAT_DEF(y);
	PARAM_FLOAT_DEF(w);
	PARAM_FLOAT_DEF(h);
	self->StatusbarToRealCoords(x, y, w, h);
	if (numret > 0) ret[0].SetFloat(x);
	if (numret > 1) ret[1].SetFloat(y);
	if (numret > 2) ret[2].SetFloat(w);
	if (numret > 3) ret[3].SetFloat(h);
	return MIN(4, numret);
}


DEFINE_ACTION_FUNCTION(DBaseStatusBar, GetTopOfStatusbar)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	ACTION_RETURN_INT(self->GetTopOfStatusbar());
}

//============================================================================
//
// draw stuff
//
//============================================================================

void DBaseStatusBar::DrawGraphic(FTextureID texture, double x, double y, int flags, double Alpha, double boxwidth, double boxheight, double scaleX, double scaleY)
{
	if (!texture.isValid())
		return;

	FTexture *tex = (flags & DI_DONTANIMATE)?  TexMan[texture] : TexMan(texture);

	double texwidth = tex->GetScaledWidthDouble() * scaleX;
	double texheight = tex->GetScaledHeightDouble() * scaleY;

	if (boxwidth > 0 || boxheight > 0)
	{
		if (!(flags & DI_FORCEFILL))
		{
			double scale1 = 1., scale2 = 1.;

			if (boxwidth > 0 && (boxwidth < texwidth || (flags & DI_FORCESCALE)))
			{
				scale1 = boxwidth / texwidth;
			}
			if (boxheight != -1 && (boxheight < texheight || (flags & DI_FORCESCALE)))
			{
				scale2 = boxheight / texheight;
			}

			if (flags & DI_FORCESCALE)
			{
				if (boxwidth <= 0 || (boxheight > 0 && scale2 < scale1))
					scale1 = scale2;
			}
			else scale1 = MIN(scale1, scale2);

			boxwidth = texwidth * scale1;
			boxheight = texheight * scale1;
		}
	}
	else
	{
		boxwidth = texwidth;
		boxheight = texheight;
	}

	// resolve auto-alignment before making any adjustments to the position values.
	if (!(flags & DI_SCREEN_MANUAL_ALIGN))
	{
		if (x < 0) flags |= DI_SCREEN_RIGHT;
		else flags |= DI_SCREEN_LEFT;
		if (y < 0) flags |= DI_SCREEN_BOTTOM;
		else flags |= DI_SCREEN_TOP;
	}

	Alpha *= this->Alpha;
	if (Alpha <= 0) return;
	x += drawOffset.X;
	y += drawOffset.Y;

	switch (flags & DI_ITEM_HMASK)
	{
	case DI_ITEM_HCENTER:	x -= boxwidth / 2; break;
	case DI_ITEM_RIGHT:		x -= boxwidth; break;
	case DI_ITEM_HOFFSET:	x -= tex->GetScaledLeftOffsetDouble() * boxwidth / texwidth; break;
	}

	switch (flags & DI_ITEM_VMASK)
	{
	case DI_ITEM_VCENTER: y -= boxheight / 2; break;
	case DI_ITEM_BOTTOM:  y -= boxheight; break;
	case DI_ITEM_VOFFSET: y -= tex->GetScaledTopOffsetDouble() * boxheight / texheight; break;
	}

	if (!fullscreenOffsets)
	{
		StatusbarToRealCoords(x, y, boxwidth, boxheight);
	}
	else
	{
		double orgx, orgy;

		switch (flags & DI_SCREEN_HMASK)
		{
		default: orgx = 0; break;
		case DI_SCREEN_HCENTER: orgx = screen->GetWidth() / 2; break;
		case DI_SCREEN_RIGHT:   orgx = screen->GetWidth(); break;
		}

		switch (flags & DI_SCREEN_VMASK)
		{
		default: orgy = 0; break;
		case DI_SCREEN_VCENTER: orgy = screen->GetHeight() / 2; break;
		case DI_SCREEN_BOTTOM: orgy = screen->GetHeight(); break;
		}

		// move stuff in the top right corner a bit down if the fps counter is on.
		if ((flags & (DI_SCREEN_HMASK|DI_SCREEN_VMASK)) == DI_SCREEN_RIGHT_TOP && vid_fps) y += 10;

		DVector2 Scale = GetHUDScale();

		x *= Scale.X;
		y *= Scale.Y;
		boxwidth *= Scale.X;
		boxheight *= Scale.Y;
		x += orgx;
		y += orgy;
	}
	screen->DrawTexture(tex, x, y, 
		DTA_TopOffset, 0,
		DTA_LeftOffset, 0,
		DTA_DestWidthF, boxwidth,
		DTA_DestHeightF, boxheight,
		DTA_TranslationIndex, (flags & DI_TRANSLATABLE) ? GetTranslation() : 0,
		DTA_ColorOverlay, (flags & DI_DIM) ? MAKEARGB(170, 0, 0, 0) : 0,
		DTA_Alpha, Alpha,
		DTA_AlphaChannel, !!(flags & DI_ALPHAMAPPED),
		DTA_FillColor, (flags & DI_ALPHAMAPPED) ? 0 : -1,
		TAG_DONE);
}


DEFINE_ACTION_FUNCTION(DBaseStatusBar, DrawTexture)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	PARAM_INT(texid);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_INT_DEF(flags);
	PARAM_FLOAT_DEF(alpha);
	PARAM_FLOAT_DEF(w);
	PARAM_FLOAT_DEF(h);
	PARAM_FLOAT_DEF(scaleX);
	PARAM_FLOAT_DEF(scaleY);
	if (!screen->HasBegun2D()) ThrowAbortException(X_OTHER, "Attempt to draw to screen outside a draw function");
	self->DrawGraphic(FSetTextureID(texid), x, y, flags, alpha, w, h, scaleX, scaleY);
	return 0;
}

DEFINE_ACTION_FUNCTION(DBaseStatusBar, DrawImage)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	PARAM_STRING(texid);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_INT_DEF(flags);
	PARAM_FLOAT_DEF(alpha);
	PARAM_FLOAT_DEF(w);
	PARAM_FLOAT_DEF(h);
	PARAM_FLOAT_DEF(scaleX);
	PARAM_FLOAT_DEF(scaleY);
	if (!screen->HasBegun2D()) ThrowAbortException(X_OTHER, "Attempt to draw to screen outside a draw function");
	self->DrawGraphic(TexMan.CheckForTexture(texid, FTexture::TEX_Any), x, y, flags, alpha, w, h, scaleX, scaleY);
	return 0;
}

//============================================================================
//
// encapsulates all settings a HUD font may need
//
//============================================================================

class DHUDFont : public DObject
{
	// this blocks CreateNew on this class which is the intent here.
	DECLARE_ABSTRACT_CLASS(DHUDFont, DObject);

public:
	FFont *mFont;
	int mSpacing;
	bool mMonospaced;
	int mShadowX;
	int mShadowY;

	DHUDFont(FFont *f, int sp, bool ms, int sx, int sy)
		: mFont(f), mSpacing(sp), mMonospaced(ms), mShadowX(sx), mShadowY(sy)
	{}
};

IMPLEMENT_CLASS(DHUDFont, true, false);

DEFINE_ACTION_FUNCTION(DHUDFont, Create)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(fnt, FFont);
	PARAM_INT_DEF(spac);
	PARAM_BOOL_DEF(mono);
	PARAM_INT_DEF(sx);
	PARAM_INT_DEF(sy);
	ACTION_RETURN_POINTER(Create<DHUDFont>(fnt, spac, mono, sy, sy));
}

DEFINE_FIELD(DHUDFont, mFont);

//============================================================================
//
// draw a string
//
//============================================================================

void DBaseStatusBar::DrawString(FFont *font, const FString &cstring, double x, double y, int flags, double Alpha, int translation, int spacing, bool monospaced, int shadowX, int shadowY)
{
	switch (flags & DI_TEXT_ALIGN)
	{
	default:
		break;
	case DI_TEXT_ALIGN_RIGHT:
		if (!monospaced)
			x -= static_cast<int> (font->StringWidth(cstring) + (spacing * cstring.Len()));
		else //monospaced, so just multiply the character size
			x -= static_cast<int> ((spacing) * cstring.Len());
		break;
	case DI_TEXT_ALIGN_CENTER:
		if (!monospaced)
			x -= static_cast<int> (font->StringWidth(cstring) + (spacing * cstring.Len())) / 2;
		else //monospaced, so just multiply the character size
			x -= static_cast<int> ((spacing)* cstring.Len()) / 2;
		break;
	}

	const uint8_t* str = (const uint8_t*)cstring.GetChars();
	const EColorRange boldTranslation = EColorRange(translation ? translation - 1 : NumTextColors - 1);
	int fontcolor = translation;
	double orgx = 0, orgy = 0;
	DVector2 Scale;

	if (fullscreenOffsets)
	{
		Scale = GetHUDScale();
		shadowX *= (int)Scale.X;
		shadowY *= (int)Scale.Y;

		switch (flags & DI_SCREEN_HMASK)
		{
		default: orgx = 0; break;
		case DI_SCREEN_HCENTER: orgx = screen->GetWidth() / 2; break;
		case DI_SCREEN_RIGHT:   orgx = screen->GetWidth(); break;
		}

		switch (flags & DI_SCREEN_VMASK)
		{
		default: orgy = 0; break;
		case DI_SCREEN_VCENTER: orgy = screen->GetHeight() / 2; break;
		case DI_SCREEN_BOTTOM: orgy = screen->GetHeight(); break;
		}

		// move stuff in the top right corner a bit down if the fps counter is on.
		if ((flags & (DI_SCREEN_HMASK | DI_SCREEN_VMASK)) == DI_SCREEN_RIGHT_TOP && vid_fps) y += 10;
	}
	else
	{
		Scale = { 1.,1. };
	}
	int ch;
	while (ch = *str++, ch != '\0')
	{
		if (ch == ' ')
		{
			x += monospaced ? spacing : font->GetSpaceWidth() + spacing;
			continue;
		}
		else if (ch == TEXTCOLOR_ESCAPE)
		{
			EColorRange newColor = V_ParseFontColor(str, translation, boldTranslation);
			if (newColor != CR_UNDEFINED)
				fontcolor = newColor;
			continue;
		}

		int width;
		FTexture* c = font->GetChar((unsigned char)ch, &width);
		if (c == NULL) //missing character.
		{
			continue;
		}

		if (!monospaced) //If we are monospaced lets use the offset
			x += (c->LeftOffset + 1); //ignore x offsets since we adapt to character size

		double rx, ry, rw, rh;
		rx = x + drawOffset.X;
		ry = y + drawOffset.Y;
		rw = c->GetScaledWidthDouble();
		rh = c->GetScaledHeightDouble();

		if (!fullscreenOffsets)
		{
			StatusbarToRealCoords(rx, ry, rw, rh);
		}
		else
		{
			rx *= Scale.X;
			ry *= Scale.Y;
			rw *= Scale.X;
			rh *= Scale.Y;

			rx += orgx;
			ry += orgy;
		}
		// This is not really such a great way to draw shadows because they can overlap with previously drawn characters.
		// This may have to be changed to draw the shadow text up front separately.
		if ((shadowX != 0 || shadowY != 0) && !(flags & DI_NOSHADOW))
		{
			screen->DrawChar(font, CR_UNTRANSLATED, rx + shadowX, ry + shadowY, ch,
				DTA_DestWidthF, rw,
				DTA_DestHeightF, rh,
				DTA_Alpha, (Alpha * HR_SHADOW),
				DTA_FillColor, 0,
				TAG_DONE);
		}
		screen->DrawChar(font, fontcolor, rx, ry, ch,
			DTA_DestWidthF, rw,
			DTA_DestHeightF, rh,
			DTA_Alpha, Alpha,
			TAG_DONE);

		if (!monospaced)
			x += width + spacing - (c->LeftOffset + 1);
		else
			x += spacing;
	}

}

DEFINE_ACTION_FUNCTION(DBaseStatusBar, DrawString)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	PARAM_POINTER(font, DHUDFont);
	PARAM_STRING(string);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_INT_DEF(flags);
	PARAM_INT_DEF(trans);
	PARAM_FLOAT_DEF(alpha);
	PARAM_INT_DEF(wrapwidth);
	PARAM_INT_DEF(linespacing);

	if (!screen->HasBegun2D()) ThrowAbortException(X_OTHER, "Attempt to draw to screen outside a draw function");

	// resolve auto-alignment before making any adjustments to the position values.
	if (!(flags & DI_SCREEN_MANUAL_ALIGN))
	{
		if (x < 0) flags |= DI_SCREEN_RIGHT;
		else flags |= DI_SCREEN_LEFT;
		if (y < 0) flags |= DI_SCREEN_BOTTOM;
		else flags |= DI_SCREEN_TOP;
	}

	if (wrapwidth > 0)
	{
		FBrokenLines *brk = V_BreakLines(font->mFont, wrapwidth, string, true);
		for (int i = 0; brk[i].Width >= 0; i++)
		{
			self->DrawString(font->mFont, brk[i].Text, x, y, flags, alpha, trans, font->mSpacing, font->mMonospaced, font->mShadowX, font->mShadowY);
			y += font->mFont->GetHeight() + linespacing;
		}
		V_FreeBrokenLines(brk);
	}
	else
	{
		self->DrawString(font->mFont, string, x, y, flags, alpha, trans, font->mSpacing, font->mMonospaced, font->mShadowX, font->mShadowY);
	}
	return 0;
}


//============================================================================
//
// draw stuff
//
//============================================================================

void DBaseStatusBar::TransformRect(double &x, double &y, double &w, double &h, int flags)
{
	// resolve auto-alignment before making any adjustments to the position values.
	if (!(flags & DI_SCREEN_MANUAL_ALIGN))
	{
		if (x < 0) flags |= DI_SCREEN_RIGHT;
		else flags |= DI_SCREEN_LEFT;
		if (y < 0) flags |= DI_SCREEN_BOTTOM;
		else flags |= DI_SCREEN_TOP;
	}

	x += drawOffset.X;
	y += drawOffset.Y;

	if (!fullscreenOffsets)
	{
		StatusbarToRealCoords(x, y, w, h);
	}
	else
	{
		double orgx, orgy;

		switch (flags & DI_SCREEN_HMASK)
		{
		default: orgx = 0; break;
		case DI_SCREEN_HCENTER: orgx = screen->GetWidth() / 2; break;
		case DI_SCREEN_RIGHT:   orgx = screen->GetWidth(); break;
		}

		switch (flags & DI_SCREEN_VMASK)
		{
		default: orgy = 0; break;
		case DI_SCREEN_VCENTER: orgy = screen->GetHeight() / 2; break;
		case DI_SCREEN_BOTTOM: orgy = screen->GetHeight(); break;
		}

		// move stuff in the top right corner a bit down if the fps counter is on.
		if ((flags & (DI_SCREEN_HMASK | DI_SCREEN_VMASK)) == DI_SCREEN_RIGHT_TOP && vid_fps) y += 10;

		DVector2 Scale = GetHUDScale();

		x *= Scale.X;
		y *= Scale.Y;
		w *= Scale.X;
		h *= Scale.Y;
		x += orgx;
		y += orgy;
	}
}


DEFINE_ACTION_FUNCTION(DBaseStatusBar, TransformRect)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(w);
	PARAM_FLOAT(h);
	PARAM_INT_DEF(flags);
	self->TransformRect(x, y, w, h, flags);
	if (numret > 0) ret[0].SetFloat(x);
	if (numret > 1) ret[1].SetFloat(y);
	if (numret > 2) ret[2].SetFloat(w);
	if (numret > 3) ret[3].SetFloat(h);
	return MIN(4, numret);
}

//============================================================================
//
// draw stuff
//
//============================================================================

void DBaseStatusBar::Fill(PalEntry color, double x, double y, double w, double h, int flags)
{
	double Alpha = color.a * this->Alpha / 255;
	if (Alpha <= 0) return;

	TransformRect(x, y, w, h, flags);

	int x1 = int(x);
	int y1 = int(y);
	int ww = int(x + w - x1);	// account for scaling to non-integers. Truncating the values separately would fail for cases like 
	int hh = int(y + h - y1);	// y=3.5, height = 5.5 where adding both values gives a larger integer than adding the two integers.

	screen->Dim(color, float(Alpha), x1, y1, ww, hh);
}


DEFINE_ACTION_FUNCTION(DBaseStatusBar, Fill)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	PARAM_COLOR(color);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(w);
	PARAM_FLOAT(h);
	PARAM_INT_DEF(flags);
	if (!screen->HasBegun2D()) ThrowAbortException(X_OTHER, "Attempt to draw to screen outside a draw function");
	self->Fill(color, x, y, w, h, flags);
	return 0;
}

//============================================================================
//
// draw stuff
//
//============================================================================

void DBaseStatusBar::SetClipRect(double x, double y, double w, double h, int flags)
{
	TransformRect(x, y, w, h, flags);
	int x1 = int(x);
	int y1 = int(y);
	int ww = int(x + w - x1);	// account for scaling to non-integers. Truncating the values separately would fail for cases like 
	int hh = int(y + h - y1); // y=3.5, height = 5.5 where adding both values gives a larger integer than adding the two integers.
	screen->SetClipRect(x1, y1, ww, hh);
}


DEFINE_ACTION_FUNCTION(DBaseStatusBar, SetClipRect)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(w);
	PARAM_FLOAT(h);
	PARAM_INT_DEF(flags);
	self->SetClipRect(x, y, w, h, flags);
	return 0;
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

DEFINE_FIELD(DBaseStatusBar, RelTop);
DEFINE_FIELD(DBaseStatusBar, HorizontalResolution);
DEFINE_FIELD(DBaseStatusBar, VerticalResolution);
DEFINE_FIELD(DBaseStatusBar, Centering);
DEFINE_FIELD(DBaseStatusBar, FixedOrigin);
DEFINE_FIELD(DBaseStatusBar, CompleteBorder);
DEFINE_FIELD(DBaseStatusBar, CrosshairSize);
DEFINE_FIELD(DBaseStatusBar, Displacement);
DEFINE_FIELD(DBaseStatusBar, CPlayer);
DEFINE_FIELD(DBaseStatusBar, ShowLog);
DEFINE_FIELD(DBaseStatusBar, Alpha);
DEFINE_FIELD(DBaseStatusBar, drawOffset);
DEFINE_FIELD(DBaseStatusBar, drawClip);
DEFINE_FIELD(DBaseStatusBar, fullscreenOffsets);
DEFINE_FIELD(DBaseStatusBar, defaultScale);
DEFINE_FIELD(DBaseStatusBar, artiflashTick);
DEFINE_FIELD(DBaseStatusBar, itemflashFade);

DEFINE_GLOBAL(StatusBar);


static DObject *InitObject(PClass *type, int paramnum, VM_ARGS)
{
	auto obj =  type->CreateNew();
	// Todo: init
	return obj;
}


DEFINE_ACTION_FUNCTION(DBaseStatusBar, GetGlobalACSString)
{
	PARAM_PROLOGUE;
	PARAM_INT(index);
	ACTION_RETURN_STRING(FBehavior::StaticLookupString(ACS_GlobalVars[index]));
}

DEFINE_ACTION_FUNCTION(DBaseStatusBar, GetGlobalACSArrayString)
{
	PARAM_PROLOGUE;
	PARAM_INT(arrayno);
	PARAM_INT(index);
	ACTION_RETURN_STRING(FBehavior::StaticLookupString(ACS_GlobalArrays[arrayno][index]));
}

DEFINE_ACTION_FUNCTION(DBaseStatusBar, GetGlobalACSValue)
{
	PARAM_PROLOGUE;
	PARAM_INT(index);
	ACTION_RETURN_INT(ACS_GlobalVars[index]);
}

DEFINE_ACTION_FUNCTION(DBaseStatusBar, GetGlobalACSArrayValue)
{
	PARAM_PROLOGUE;
	PARAM_INT(arrayno);
	PARAM_INT(index);
	ACTION_RETURN_INT(ACS_GlobalArrays[arrayno][index]);
}

enum ENumFlags
{
	FNF_FILLZEROS,
	FNF_WHENNOTZERO,
};

DEFINE_ACTION_FUNCTION(DBaseStatusBar, FormatNumber)
{
	PARAM_PROLOGUE;
	PARAM_INT(number);
	PARAM_INT_DEF(minsize);
	PARAM_INT_DEF(maxsize);
	PARAM_INT_DEF(flags);
	PARAM_STRING_DEF(prefix);
	static int maxvals[] = { 1, 9, 99, 999, 9999, 99999, 999999, 9999999, 99999999, 999999999 };

	if (number == 0 && (flags & FNF_WHENNOTZERO)) ACTION_RETURN_STRING("");
	if (maxsize > 0 && maxsize < 10)
	{
		number = clamp(number, -maxvals[maxsize - 1], maxvals[maxsize]);
	}
	FString fmt;
	if (minsize <= 1) fmt.Format("%s%d", prefix.GetChars(), number);
	else if (flags & FNF_FILLZEROS) fmt.Format("%s%0*d", prefix.GetChars(), minsize, number);
	else fmt.Format("%s%*d", prefix.GetChars(), minsize, number);
	ACTION_RETURN_STRING(fmt);
}

DEFINE_ACTION_FUNCTION(DBaseStatusBar, ReceivedWeapon)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	self->mugshot.Grin();
	return 0;
}

DEFINE_ACTION_FUNCTION(DBaseStatusBar, GetMugshot)
{
	PARAM_SELF_PROLOGUE(DBaseStatusBar);
	PARAM_INT(accuracy);
	PARAM_INT_DEF(stateflags);
	PARAM_STRING_DEF(def_face);
	auto tex = self->mugshot.GetFace(self->CPlayer, def_face, accuracy, (FMugShot::StateFlags)stateflags);
	ACTION_RETURN_INT(tex ? tex->id.GetIndex() : -1);
}


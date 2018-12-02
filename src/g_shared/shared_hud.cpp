/*
** Enhanced heads up 'overlay' for fullscreen
**
**---------------------------------------------------------------------------
** Copyright 2003-2008 Christoph Oelckers
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

// NOTE: Some stuff in here might seem a little redundant but I wanted this
// to be as true as possible to my original intent which means that it
// only uses that code from ZDoom's status bar that is the same as any
// copy would be.

#include "doomtype.h"
#include "doomdef.h"
#include "v_video.h"
#include "gi.h"
#include "w_wad.h"
#include "a_keys.h"
#include "sbar.h"
#include "sc_man.h"
#include "p_local.h"
#include "doomstat.h"
#include "g_level.h"
#include "d_net.h"
#include "d_player.h"
#include "r_utility.h"
#include "cmdlib.h"
#include "g_levellocals.h"
#include "vm.h"

#include <time.h>


#define HUMETA_AltIcon 0x10f000

EXTERN_CVAR(Bool,am_follow)
EXTERN_CVAR (Int, con_scaletext)
EXTERN_CVAR (Bool, idmypos)
EXTERN_CVAR (Int, screenblocks)
EXTERN_CVAR(Bool, hud_aspectscale)

EXTERN_CVAR (Bool, am_showtime)
EXTERN_CVAR (Bool, am_showtotaltime)

CVAR(Int,hud_althudscale, 0, CVAR_ARCHIVE)				// Scale the hud to 640x400?
CVAR(Bool,hud_althud, false, CVAR_ARCHIVE)				// Enable/Disable the alternate HUD

														// These are intentionally not the same as in the automap!
CVAR (Bool,  hud_showsecrets,	true,CVAR_ARCHIVE);		// Show secrets on HUD
CVAR (Bool,  hud_showmonsters,	true,CVAR_ARCHIVE);		// Show monster stats on HUD
CVAR (Bool,  hud_showitems,		false,CVAR_ARCHIVE);	// Show item stats on HUD
CVAR (Bool,  hud_showstats,		false,	CVAR_ARCHIVE);	// for stamina and accuracy. 
CVAR (Bool,  hud_showscore,		false,	CVAR_ARCHIVE);	// for user maintained score
CVAR (Bool,  hud_showweapons,	true, CVAR_ARCHIVE);	// Show weapons collected
CVAR (Int ,  hud_showammo,		2, CVAR_ARCHIVE);		// Show ammo collected
CVAR (Int ,  hud_showtime,		0,	    CVAR_ARCHIVE);	// Show time on HUD
CVAR (Int ,  hud_timecolor,		CR_GOLD,CVAR_ARCHIVE);	// Color of in-game time on HUD
CVAR (Int ,  hud_showlag,		0, CVAR_ARCHIVE);		// Show input latency (maketic - gametic difference)

CVAR (Int, hud_ammo_order, 0, CVAR_ARCHIVE);				// ammo image and text order
CVAR (Int, hud_ammo_red, 25, CVAR_ARCHIVE)					// ammo percent less than which status is red    
CVAR (Int, hud_ammo_yellow, 50, CVAR_ARCHIVE)				// ammo percent less is yellow more green        
CVAR (Int, hud_health_red, 25, CVAR_ARCHIVE)				// health amount less than which status is red   
CVAR (Int, hud_health_yellow, 50, CVAR_ARCHIVE)				// health amount less than which status is yellow
CVAR (Int, hud_health_green, 100, CVAR_ARCHIVE)				// health amount above is blue, below is green   
CVAR (Int, hud_armor_red, 25, CVAR_ARCHIVE)					// armor amount less than which status is red    
CVAR (Int, hud_armor_yellow, 50, CVAR_ARCHIVE)				// armor amount less than which status is yellow 
CVAR (Int, hud_armor_green, 100, CVAR_ARCHIVE)				// armor amount above is blue, below is green    

CVAR (Bool, hud_berserk_health, true, CVAR_ARCHIVE);		// when found berserk pack instead of health box
CVAR (Bool, hud_showangles, false, CVAR_ARCHIVE)			// show player's pitch, yaw, roll

CVAR (Int, hudcolor_titl, CR_YELLOW, CVAR_ARCHIVE)			// color of automap title
CVAR (Int, hudcolor_time, CR_RED, CVAR_ARCHIVE)				// color of level/hub time
CVAR (Int, hudcolor_ltim, CR_ORANGE, CVAR_ARCHIVE)			// color of single level time
CVAR (Int, hudcolor_ttim, CR_GOLD, CVAR_ARCHIVE)			// color of total time
CVAR (Int, hudcolor_xyco, CR_GREEN, CVAR_ARCHIVE)			// color of coordinates

CVAR (Int, hudcolor_statnames, CR_RED, CVAR_ARCHIVE)		// For the letters before the stats
CVAR (Int, hudcolor_stats, CR_GREEN, CVAR_ARCHIVE)			// For the stats values themselves


CVAR(Bool, map_point_coordinates, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)	// show player or map coordinates?

static FFont * HudFont;						// The font for the health and armor display
static FFont * IndexFont;					// The font for the inventory indices

// Icons
static FTexture * healthpic;				// Health icon
static FTexture * berserkpic;				// Berserk icon (Doom only)
static FTexture * fragpic;					// Frags icon
static FTexture * invgems[2];				// Inventory arrows
static FTextureID tnt1a0;					// We need this to check for empty sprites.

static int hudwidth, hudheight;				// current width/height for HUD display
static int statspace;

DObject *althud;								// scripted parts. This is here to make a gradual transition

//---------------------------------------------------------------------------
//
// Draws an image into a box with its bottom center at the bottom
// center of the box. The image is scaled down if it doesn't fit
//
//---------------------------------------------------------------------------
static void DrawImageToBox(FTexture * tex, int x, int y, int w, int h, double trans = 0.75)
{
	IFVM(AltHud, DrawImageToBox)
	{
		VMValue params[] = { althud, tex->id.GetIndex(), x, y, w, h, trans };
		VMCall(func, params, countof(params), nullptr, 0);
	}
}


//---------------------------------------------------------------------------
//
// Draws a text but uses a fixed width for all characters
//
//---------------------------------------------------------------------------

static void DrawHudText(FFont *font, int color, char * text, int x, int y, double trans = 0.75)
{
	IFVM(AltHud, DrawHudText)
	{
		FString string = text;
		VMValue params[] = { althud, font, color, &string, x, y, trans };
		VMCall(func, params, countof(params), nullptr, 0);
	}
}


//---------------------------------------------------------------------------
//
// Draws a number with a fixed width for all digits
//
//---------------------------------------------------------------------------

static void DrawHudNumber(FFont *font, int color, int num, int x, int y, double trans = 0.75)
{
	IFVM(AltHud, DrawHudNumber)
	{
		VMValue params[] = { althud, font, color, num, x, y, trans };
		VMCall(func, params, countof(params), nullptr, 0);
	}
}

//---------------------------------------------------------------------------
//
// draw the overlay
//
//---------------------------------------------------------------------------

void DrawHUD()
{
	player_t * CPlayer = StatusBar->CPlayer;

	players[consoleplayer].inventorytics = 0;
	int scale = GetUIScale(hud_althudscale);
	hudwidth = SCREENWIDTH / scale;
	hudheight = hud_aspectscale ? int(SCREENHEIGHT / (scale*1.2)) : SCREENHEIGHT / scale;

	// Until the script export is complete we need to do some manual setup here
	auto cls = PClass::FindClass("AltHud");
	if (!cls) return;

	althud = cls->CreateNew();
	althud->IntVar("hudwidth") = hudwidth;
	althud->IntVar("hudheight") = hudheight;
	althud->IntVar("statspace") = statspace;
	althud->IntVar("healthpic") = healthpic? healthpic->id.GetIndex() : -1;
	althud->IntVar("berserkpic") = berserkpic? berserkpic->id.GetIndex() : -1;
	althud->IntVar("tnt1a0") = tnt1a0.GetIndex();
	althud->IntVar("invgem_left") = invgems[0]->id.GetIndex();
	althud->IntVar("invgem_right") = invgems[1]->id.GetIndex();
	althud->IntVar("fragpic") = fragpic? fragpic->id.GetIndex() : -1;
	althud->PointerVar<FFont>("HUDFont") = HudFont;
	althud->PointerVar<FFont>("IndexFont") = IndexFont;

	if (!automapactive)
	{
		IFVIRTUALPTRNAME(althud, "AltHud", DrawInGame)
		{
			VMValue params[] = { althud, CPlayer };
			VMCall(func, params, countof(params), nullptr, 0);
		}
	}
	else
	{
		FString mapname;
		char printstr[256];
		int seconds;
		int length=8*SmallFont->GetCharWidth('0');
		int fonth=SmallFont->GetHeight()+1;
		int bottom=hudheight-1;

		if (am_showtotaltime)
		{
			seconds = Tics2Seconds(level.totaltime);
			mysnprintf(printstr, countof(printstr), "%02i:%02i:%02i", seconds/3600, (seconds%3600)/60, seconds%60);
			DrawHudText(SmallFont, hudcolor_ttim, printstr, hudwidth-length, bottom, 0x10000);
			bottom -= fonth;
		}

		if (am_showtime)
		{
			if (level.clusterflags&CLUSTER_HUB)
			{
				seconds = Tics2Seconds(level.time);
				mysnprintf(printstr, countof(printstr), "%02i:%02i:%02i", seconds/3600, (seconds%3600)/60, seconds%60);
				DrawHudText(SmallFont, hudcolor_time, printstr, hudwidth-length, bottom, 0x10000);
				bottom -= fonth;
			}

			// Single level time for hubs
			seconds= Tics2Seconds(level.maptime);
			mysnprintf(printstr, countof(printstr), "%02i:%02i:%02i", seconds/3600, (seconds%3600)/60, seconds%60);
			DrawHudText(SmallFont, hudcolor_ltim, printstr, hudwidth-length, bottom, 0x10000);
		}

		ST_FormatMapName(mapname);
		screen->DrawText(SmallFont, hudcolor_titl, 1, hudheight-fonth-1, mapname,
			DTA_KeepRatio, true,
			DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight, TAG_DONE);

		//DrawCoordinates(CPlayer);
	}

	if (althud) althud->Destroy();
	althud = nullptr;
}

/////////////////////////////////////////////////////////////////////////
//
// Initialize the fonts and other data
//
/////////////////////////////////////////////////////////////////////////

void HUD_InitHud()
{
	switch (gameinfo.gametype)
	{
	case GAME_Heretic:
	case GAME_Hexen:
		healthpic = TexMan.FindTexture("ARTIPTN2");
		HudFont=FFont::FindFont("HUDFONT_RAVEN");
		break;

	case GAME_Strife:
		healthpic = TexMan.FindTexture("I_MDKT");
		HudFont=BigFont;	// Strife doesn't have anything nice so use the standard font
		break;

	default:
		healthpic = TexMan.FindTexture("MEDIA0");
		berserkpic = TexMan.FindTexture("PSTRA0");
		HudFont=FFont::FindFont("HUDFONT_DOOM");
		break;
	}

	IndexFont = V_GetFont("INDEXFONT");

	if (HudFont == NULL) HudFont = BigFont;
	if (IndexFont == NULL) IndexFont = ConFont;	// Emergency fallback

	invgems[0] = TexMan.FindTexture("INVGEML1");
	invgems[1] = TexMan.FindTexture("INVGEMR1");
	tnt1a0 = TexMan.CheckForTexture("TNT1A0", ETextureType::Sprite);

	fragpic = TexMan.FindTexture("HU_FRAGS");	// Sadly, I don't have anything usable for this. :(

	statspace = SmallFont->StringWidth("Ac:");



	// Now read custom icon overrides
	int lump, lastlump = 0;

	while ((lump = Wads.FindLump ("ALTHUDCF", &lastlump)) != -1)
	{
		FScanner sc(lump);
		while (sc.GetString())
		{
			if (sc.Compare("Health"))
			{
				sc.MustGetString();
				FTextureID tex = TexMan.CheckForTexture(sc.String, ETextureType::MiscPatch);
				if (tex.isValid()) healthpic = TexMan[tex];
			}
			else if (sc.Compare("Berserk"))
			{
				sc.MustGetString();
				FTextureID tex = TexMan.CheckForTexture(sc.String, ETextureType::MiscPatch);
				if (tex.isValid()) berserkpic = TexMan[tex];
			}
			else
			{
				PClass *ti = PClass::FindClass(sc.String);
				if (!ti)
				{
					Printf("Unknown item class '%s' in ALTHUDCF\n", sc.String);
				}
				else if (!ti->IsDescendantOf(RUNTIME_CLASS(AInventory)))
				{
					Printf("Invalid item class '%s' in ALTHUDCF\n", sc.String);
					ti=NULL;
				}
				sc.MustGetString();
				FTextureID tex;

				if (!sc.Compare("0") && !sc.Compare("NULL") && !sc.Compare(""))
				{
					tex = TexMan.CheckForTexture(sc.String, ETextureType::MiscPatch);
				}
				else tex.SetInvalid();

				if (ti) ((AInventory*)GetDefaultByType(ti))->AltHUDIcon = tex;
			}
		}
	}
}


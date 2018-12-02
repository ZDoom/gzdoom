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

DVector2 AM_GetPosition();

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


//===========================================================================
//
// draw the status (number of kills etc)
//
//===========================================================================

static void DrawStatus(player_t * CPlayer, int x, int y)
{
	IFVM(AltHud, DrawStatus)
	{
		VMValue params[] = { althud, CPlayer, x, y };
		VMCall(func, params, countof(params), nullptr, 0);
	}
}


//===========================================================================
//
// draw health
//
//===========================================================================

static void DrawHealth(player_t *CPlayer, int x, int y)
{
	IFVM(AltHud, DrawHealth)
	{
		VMValue params[] = { althud, CPlayer, x, y };
		VMCall(func, params, countof(params), nullptr, 0);
	}
}

//===========================================================================
//
// Draw Armor.
// very similar to drawhealth, but adapted to handle Hexen armor too
//
//===========================================================================

static void DrawArmor(AInventory * barmor, AInventory * harmor, int x, int y)
{
	IFVM(AltHud, DrawArmor)
	{
		VMValue params[] = { althud, barmor, harmor, x, y };
		VMCall(func, params, countof(params), nullptr, 0);
	}
}

//===========================================================================
//
// KEYS
//
//===========================================================================

//---------------------------------------------------------------------------
//
// Draw all keys
//
//---------------------------------------------------------------------------

static int DrawKeys(player_t * CPlayer, int x, int y)
{
	IFVM(AltHud, DrawKeys)
	{
		VMValue params[] = { althud, CPlayer, x, y };
		int retv;
		VMReturn ret(&retv);
		VMCall(func, params, countof(params), &ret, 1);
		return retv;
	}
	return 0;
}

//---------------------------------------------------------------------------
//
// Drawing Ammo
//
//---------------------------------------------------------------------------

static int DrawAmmo(player_t *CPlayer, int x, int y)
{
	IFVM(AltHud, DrawAmmo)
	{
		VMValue params[] = { althud, CPlayer, x, y };
		int retv;
		VMReturn ret(&retv);
		VMCall(func, params, countof(params), &ret, 1);
		return retv;
	}
	return 0;
}


static void DrawOneWeapon(player_t * CPlayer, int x, int & y, AInventory * weapon)
{
	double trans;

	// Powered up weapons and inherited sister weapons are not displayed.
	if (weapon->IntVar(NAME_WeaponFlags) & WIF_POWERED_UP) return;
	auto SisterWeapon = weapon->PointerVar<AInventory>(NAME_SisterWeapon);
	if (SisterWeapon && weapon->IsKindOf(SisterWeapon->GetClass())) return;

	trans=0.4;
	if (CPlayer->ReadyWeapon)
	{
		if (weapon==CPlayer->ReadyWeapon || SisterWeapon == CPlayer->ReadyWeapon) trans = 0.85;
	}

	FTextureID picnum = GetInventoryIcon(weapon, DI_ALTICONFIRST);

	if (picnum.isValid())
	{
		FTexture * tex = TexMan[picnum];
		int w = tex->GetWidth();
		int h = tex->GetHeight();
		int rh;
		if (w>h) rh=8;
		else rh=16,y-=8;		// don't draw tall sprites too small!
		DrawImageToBox(tex, x-24, y, 20, rh, trans);
		y-=10;
	}
}


static void DrawWeapons(player_t *CPlayer, int x, int y)
{
	int k,j;
	AInventory *inv;

	// First draw all weapons in the inventory that are not assigned to a weapon slot
	for(inv = CPlayer->mo->Inventory; inv; inv = inv->Inventory)
	{
		if (inv->IsKindOf(NAME_Weapon) && 
			!CPlayer->weapons.LocateWeapon(inv->GetClass(), NULL, NULL))
		{
			DrawOneWeapon(CPlayer, x, y, inv);
		}
	}

	// And now everything in the weapon slots back to front
	for (k = NUM_WEAPON_SLOTS - 1; k >= 0; k--) for(j = CPlayer->weapons.SlotSize(k) - 1; j >= 0; j--)
	{
		PClassActor *weap = CPlayer->weapons.GetWeapon(k, j);
		if (weap) 
		{
			inv=CPlayer->mo->FindInventory(weap);
			if (inv) 
			{
				DrawOneWeapon(CPlayer, x, y, inv);
			}
		}
	}
}


//---------------------------------------------------------------------------
//
// Draw the Inventory
//
//---------------------------------------------------------------------------

static void DrawInventory(player_t * CPlayer, int x,int y)
{
	AInventory * rover;
	int numitems = (hudwidth - 2*x) / 32;
	int i;

	CPlayer->mo->InvFirst = rover = StatusBar->ValidateInvFirst(numitems);
	if (rover!=NULL)
	{
		if(rover->PrevInv())
		{
			screen->DrawTexture(invgems[0], x-10, y,
				DTA_KeepRatio, true,
				DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight, DTA_Alpha, 0.4, TAG_DONE);
		}

		for(i=0;i<numitems && rover;rover=rover->NextInv())
		{
			if (rover->Amount>0)
			{
				FTextureID AltIcon = rover->AltHUDIcon;

				if (AltIcon.Exists() && (rover->Icon.isValid() || AltIcon.isValid()) )
				{
					double trans = rover==CPlayer->mo->InvSel ? 1.0 : 0.4;

					DrawImageToBox(TexMan[AltIcon.isValid()? AltIcon : rover->Icon], x, y, 19, 25, trans);
					if (rover->Amount>1)
					{
						char buffer[10];
						int xx;
						mysnprintf(buffer, countof(buffer), "%d", rover->Amount);
						if (rover->Amount>=1000) xx = 32 - IndexFont->StringWidth(buffer);
						else xx = 22;

						screen->DrawText(IndexFont, CR_GOLD, x+xx, y+20, buffer, 
							DTA_KeepRatio, true,
							DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight, DTA_Alpha, trans, TAG_DONE);
					}
					
					x+=32;
					i++;
				}
			}
		}
		if(rover)
		{
			screen->DrawTexture(invgems[1], x-10, y,
				DTA_KeepRatio, true,
				DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight, DTA_Alpha, 0.4, TAG_DONE);
		}
	}
}

//---------------------------------------------------------------------------
//
// Draw the Frags
//
//---------------------------------------------------------------------------

static void DrawFrags(player_t * CPlayer, int x, int y)
{
	DrawImageToBox(fragpic, x, y, 31, 17);
	DrawHudNumber(HudFont, CR_GRAY, CPlayer->fragcount, x + 33, y + 17);
}



//---------------------------------------------------------------------------
//
// PROC DrawCoordinates
//
//---------------------------------------------------------------------------

static void DrawCoordinates(player_t * CPlayer)
{
	DVector3 pos;
	char coordstr[18];
	int h = SmallFont->GetHeight()+1;

	
	if (!map_point_coordinates || !automapactive) 
	{
		pos = CPlayer->mo->Pos();
	}
	else 
	{
		DVector2 apos = AM_GetPosition();
		double z = P_PointInSector(apos)->floorplane.ZatPoint(apos);
		pos = DVector3(apos, z);
	}

	int xpos = hudwidth - SmallFont->StringWidth("X: -00000")-6;
	int ypos = 18;

	screen->DrawText(SmallFont, hudcolor_titl, hudwidth - 6 - SmallFont->StringWidth(level.MapName), ypos, level.MapName,
		DTA_KeepRatio, true,
		DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight, TAG_DONE);

	screen->DrawText(SmallFont, hudcolor_titl, hudwidth - 6 - SmallFont->StringWidth(level.LevelName), ypos + h, level.LevelName,
		DTA_KeepRatio, true,
		DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight, TAG_DONE);

	int linenum = 3;

	typedef struct CoordEntry
	{
		const char* const format;
		double value;
	}
	CoordEntryList[3];

	const auto drawentries = [&](CoordEntryList&& entries)
	{
		for (const auto& entry : entries)
		{
			mysnprintf(coordstr, countof(coordstr), entry.format, entry.value);
			screen->DrawText(SmallFont, hudcolor_xyco, xpos, ypos + linenum * h, coordstr,
							 DTA_KeepRatio, true,
							 DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight, TAG_DONE);
			++linenum;
		}
	};

	drawentries({
		{ "X: %.0f", pos.X },
		{ "Y: %.0f", pos.Y },
		{ "Z: %.0f", pos.Z }
	});

	if (hud_showangles)
	{
		const DRotator& angles = CPlayer->mo->Angles;
		++linenum;

		drawentries({
			{ "P: %.1f", angles.Pitch.Degrees },
			{ "Y: %.1f", (90.0 - angles.Yaw).Normalized360().Degrees },
			{ "R: %.1f", angles.Roll.Degrees },
		});
	}
}

//---------------------------------------------------------------------------
//
// Draw in-game time
//
// Check AltHUDTime option value in wadsrc/static/menudef.txt
// for meaning of all display modes
//
//---------------------------------------------------------------------------

static void DrawTime()
{
	if (!ST_IsTimeVisible())
	{
		return;
	}

	int hours   = 0;
	int minutes = 0;
	int seconds = 0;

	if (hud_showtime < 8)
	{
		const int timeTicks =
			hud_showtime < 4
				? level.maptime
				: (hud_showtime < 6
					? level.time
					: level.totaltime);
		const int timeSeconds = Tics2Seconds(timeTicks);

		hours   =  timeSeconds / 3600;
		minutes = (timeSeconds % 3600) / 60;
		seconds =  timeSeconds % 60;
	}
	else
	{
		time_t now;
		time(&now);

		struct tm* timeinfo = localtime(&now);

		if (NULL != timeinfo)
		{
			hours   = timeinfo->tm_hour;
			minutes = timeinfo->tm_min;
			seconds = timeinfo->tm_sec;
		}
	}

	const bool showMillis  = 1 == hud_showtime;
	const bool showSeconds = showMillis || (0 == hud_showtime % 2);

	char timeString[sizeof "HH:MM:SS.MMM"];

	if (showMillis)
	{
		const int millis  = (level.time % TICRATE) * (1000 / TICRATE);

		mysnprintf(timeString, sizeof(timeString), "%02i:%02i:%02i.%03i", hours, minutes, seconds, millis);
	}
	else if (showSeconds)
	{
		mysnprintf(timeString, sizeof(timeString), "%02i:%02i:%02i", hours, minutes, seconds);
	}
	else
	{
		mysnprintf(timeString, sizeof(timeString), "%02i:%02i", hours, minutes);
	}

	const int characterCount = static_cast<int>( sizeof "HH:MM" - 1
		+ (showSeconds ? sizeof ":SS"  - 1 : 0)
		+ (showMillis  ? sizeof ".MMM" - 1 : 0) );
	const int width  = SmallFont->GetCharWidth('0') * characterCount + 2; // small offset from screen's border
	const int height = SmallFont->GetHeight();

	DrawHudText(SmallFont, hud_timecolor, timeString, hudwidth - width, height, 0x10000);
}

static bool IsAltHUDTextVisible()
{
	return hud_althud
		&& !automapactive
		&& (SCREENHEIGHT == viewheight)
		&& (11 == screenblocks);
}

bool ST_IsTimeVisible()
{
	return IsAltHUDTextVisible()
		&& (hud_showtime > 0) 
		&& (hud_showtime <= 9);
}

//---------------------------------------------------------------------------
//
// Draw in-game latency
//
//---------------------------------------------------------------------------

static void DrawLatency()
{
	if (!ST_IsLatencyVisible())
	{
		return;
	}
	int i, localdelay = 0, arbitratordelay = 0;
	for (i = 0; i < BACKUPTICS; i++) localdelay += netdelay[0][i];
	for (i = 0; i < BACKUPTICS; i++) arbitratordelay += netdelay[nodeforplayer[Net_Arbitrator]][i];
	localdelay = ((localdelay / BACKUPTICS) * ticdup) * (1000 / TICRATE);
	arbitratordelay = ((arbitratordelay / BACKUPTICS) * ticdup) * (1000 / TICRATE);
	int color = CR_GREEN;
	if (MAX(localdelay, arbitratordelay) > 200)
	{
		color = CR_YELLOW;
	}
	if (MAX(localdelay, arbitratordelay) > 400)
	{
		color = CR_ORANGE;
	}
	if (MAX(localdelay, arbitratordelay) >= ((BACKUPTICS / 2 - 1) * ticdup) * (1000 / TICRATE))
	{
		color = CR_RED;
	}

	char tempstr[32];

	const int millis = (level.time % TICRATE) * (1000 / TICRATE);
	mysnprintf(tempstr, sizeof(tempstr), "a:%dms - l:%dms", arbitratordelay, localdelay);

	const int characterCount = (int)strlen(tempstr);
	const int width = SmallFont->GetCharWidth('0') * characterCount + 2; // small offset from screen's border
	const int height = SmallFont->GetHeight() * (ST_IsTimeVisible() ? 2 : 1);

	DrawHudText(SmallFont, color, tempstr, hudwidth - width, height, 0x10000);
}

bool ST_IsLatencyVisible()
{
	return IsAltHUDTextVisible()
		&& (hud_showlag > 0)
		&& (hud_showlag != 1 || netgame)
		&& (hud_showlag <= 2);
}

//---------------------------------------------------------------------------
//
// draw the overlay
//
//---------------------------------------------------------------------------

static void DrawPowerups(player_t *CPlayer)
{
	// Each icon gets a 32x32 block to draw itself in.
	int x, y;
	AInventory *item;
	const int yshift = SmallFont->GetHeight();
	const int POWERUPICONSIZE = 32;

	x = hudwidth -20;
	y = POWERUPICONSIZE * 5/4
		+ (ST_IsTimeVisible() ? yshift : 0)
		+ (ST_IsLatencyVisible() ? yshift : 0);

	for (item = CPlayer->mo->Inventory; item != NULL; item = item->Inventory)
	{
		if (item->IsKindOf(NAME_Powerup))
		{
			IFVIRTUALPTRNAME(item, NAME_Powerup, GetPowerupIcon)
			{
				VMValue param[] = { item };
				int rv;
				VMReturn ret(&rv);
				VMCall(func, param, 1, &ret, 1);
				auto tex = FSetTextureID(rv);
				if (!tex.isValid()) continue;
				auto texture = TexMan(tex);

				IFVIRTUALPTRNAME(item, NAME_Powerup, IsBlinking)
				{
					// Reuse the parameters from GetPowerupIcon
					VMCall(func, param, 1, &ret, 1);
					if (!rv)
					{

						screen->DrawTexture(texture, x, y, DTA_KeepRatio, true, DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight, DTA_CenterBottomOffset, true, TAG_DONE);

						x -= POWERUPICONSIZE;
						if (x < -hudwidth / 2)
						{
							x = -20;
							y += POWERUPICONSIZE * 3 / 2;
						}
					}
				}
			}
		}
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
	althud->PointerVar<FFont>("HUDFont") = HudFont;

	if (!automapactive)
	{
		int i;

		// No HUD in the title level!
		if (gamestate == GS_TITLELEVEL || !CPlayer) return;

		if (!deathmatch) DrawStatus(CPlayer, 5, hudheight-50);
		else
		{
			DrawStatus(CPlayer, 5, hudheight-75);
			DrawFrags(CPlayer, 5, hudheight-70);
		}
		DrawHealth(CPlayer, 5, hudheight-45);
		DrawArmor(CPlayer->mo->FindInventory(NAME_BasicArmor), CPlayer->mo->FindInventory(NAME_HexenArmor), 5, hudheight-20);
		i=DrawKeys(CPlayer, hudwidth-4, hudheight-10);
		i=DrawAmmo(CPlayer, hudwidth-5, i);
		if (hud_showweapons) DrawWeapons(CPlayer, hudwidth - 5, i);
		DrawInventory(CPlayer, 144, hudheight-28);
		if (idmypos) DrawCoordinates(CPlayer);

		DrawTime();
		DrawLatency();
		DrawPowerups(CPlayer);
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

		DrawCoordinates(CPlayer);
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

//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
//
// DESCRIPTION:  the automap code
//
//-----------------------------------------------------------------------------

#include <stdio.h>

#include "doomdef.h"
#include "templates.h"
#include "g_level.h"
#include "st_stuff.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "w_wad.h"
#include "a_sharedglobal.h"
#include "d_event.h"
#include "gi.h"
#include "p_setup.h"
#include "c_bind.h"
#include "serializer.h"
#include "r_renderer.h"
#include "r_sky.h"
#include "sbar.h"
#include "d_player.h"
#include "p_blockmap.h"

#include "m_cheat.h"
#include "c_dispatch.h"
#include "d_netinf.h"

// State.
#include "r_state.h"
#include "r_utility.h"

// Data.
#include "gstrings.h"

#include "am_map.h"
#include "po_man.h"
#include "a_keys.h"
#include "g_levellocals.h"
#include "actorinlines.h"


//=============================================================================
//
// CVARs
//
//=============================================================================

CVAR (Int,   am_rotate,				0,			CVAR_ARCHIVE);
CVAR (Int,   am_overlay,			0,			CVAR_ARCHIVE);
CVAR (Bool,  am_showsecrets,		true,		CVAR_ARCHIVE);
CVAR (Bool,  am_showmonsters,		true,		CVAR_ARCHIVE);
CVAR (Bool,  am_showitems,			false,		CVAR_ARCHIVE);
CVAR (Bool,  am_showtime,			true,		CVAR_ARCHIVE);
CVAR (Bool,  am_showtotaltime,		false,		CVAR_ARCHIVE);
CVAR (Int,   am_colorset,			0,			CVAR_ARCHIVE);
CVAR (Bool,  am_customcolors,		true,		CVAR_ARCHIVE);
CVAR (Int,   am_map_secrets,		1,			CVAR_ARCHIVE);
CVAR (Int,	 am_drawmapback,		1,			CVAR_ARCHIVE);
CVAR (Bool,  am_showkeys,			true,		CVAR_ARCHIVE);
CVAR (Int,   am_showtriggerlines,	0,			CVAR_ARCHIVE);
CVAR (Int,   am_showthingsprites,		0,		CVAR_ARCHIVE);

CUSTOM_CVAR (Int, am_emptyspacemargin, 0, CVAR_ARCHIVE)
{
	if (self < 0)
	{
		self = 0;
	}
	else if (self > 90)
	{
		self = 90;
	}

	if (nullptr != StatusBar)
	{
		AM_NewResolution();
	}
}

//=============================================================================
//
// Automap colors
//
//=============================================================================

CVAR (Color, am_backcolor,			0x6c5440,	CVAR_ARCHIVE);
CVAR (Color, am_yourcolor,			0xfce8d8,	CVAR_ARCHIVE);
CVAR (Color, am_wallcolor,			0x2c1808,	CVAR_ARCHIVE);
CVAR (Color, am_secretwallcolor,	0x000000,	CVAR_ARCHIVE);
CVAR (Color, am_specialwallcolor,	0xffffff,	CVAR_ARCHIVE);
CVAR (Color, am_tswallcolor,		0x888888,	CVAR_ARCHIVE);
CVAR (Color, am_fdwallcolor,		0x887058,	CVAR_ARCHIVE);
CVAR (Color, am_cdwallcolor,		0x4c3820,	CVAR_ARCHIVE);
CVAR (Color, am_efwallcolor,		0x665555,	CVAR_ARCHIVE);
CVAR (Color, am_thingcolor,			0xfcfcfc,	CVAR_ARCHIVE);
CVAR (Color, am_gridcolor,			0x8b5a2b,	CVAR_ARCHIVE);
CVAR (Color, am_xhaircolor,			0x808080,	CVAR_ARCHIVE);
CVAR (Color, am_notseencolor,		0x6c6c6c,	CVAR_ARCHIVE);
CVAR (Color, am_lockedcolor,		0x007800,	CVAR_ARCHIVE);
CVAR (Color, am_intralevelcolor,	0x0000ff,	CVAR_ARCHIVE);
CVAR (Color, am_interlevelcolor,	0xff0000,	CVAR_ARCHIVE);
CVAR (Color, am_secretsectorcolor,	0xff00ff,	CVAR_ARCHIVE);
CVAR (Color, am_unexploredsecretcolor,	0xff00ff,	CVAR_ARCHIVE);
CVAR (Color, am_thingcolor_friend,	0xfcfcfc,	CVAR_ARCHIVE);
CVAR (Color, am_thingcolor_monster,	0xfcfcfc,	CVAR_ARCHIVE);
CVAR (Color, am_thingcolor_ncmonster,	0xfcfcfc,	CVAR_ARCHIVE);
CVAR (Color, am_thingcolor_item,	0xfcfcfc,	CVAR_ARCHIVE);
CVAR (Color, am_thingcolor_citem,	0xfcfcfc,	CVAR_ARCHIVE);
CVAR (Color, am_portalcolor,		0x404040,	CVAR_ARCHIVE);

CVAR (Color, am_ovyourcolor,		0xfce8d8,	CVAR_ARCHIVE);
CVAR (Color, am_ovwallcolor,		0x00ff00,	CVAR_ARCHIVE);
CVAR (Color, am_ovsecretwallcolor,	0x008844,	CVAR_ARCHIVE);
CVAR (Color, am_ovspecialwallcolor,	0xffffff,	CVAR_ARCHIVE);
CVAR (Color, am_ovotherwallscolor,	0x008844,	CVAR_ARCHIVE);
CVAR (Color, am_ovlockedcolor,		0x008844,	CVAR_ARCHIVE);
CVAR (Color, am_ovefwallcolor,		0x008844,	CVAR_ARCHIVE);
CVAR (Color, am_ovfdwallcolor,		0x008844,	CVAR_ARCHIVE);
CVAR (Color, am_ovcdwallcolor,		0x008844,	CVAR_ARCHIVE);
CVAR (Color, am_ovunseencolor,		0x00226e,	CVAR_ARCHIVE);
CVAR (Color, am_ovtelecolor,		0xffff00,	CVAR_ARCHIVE);
CVAR (Color, am_ovinterlevelcolor,	0xffff00,	CVAR_ARCHIVE);
CVAR (Color, am_ovsecretsectorcolor,0x00ffff,	CVAR_ARCHIVE);
CVAR (Color, am_ovunexploredsecretcolor,0x00ffff,	CVAR_ARCHIVE);
CVAR (Color, am_ovthingcolor,		0xe88800,	CVAR_ARCHIVE);
CVAR (Color, am_ovthingcolor_friend,	0xe88800,	CVAR_ARCHIVE);
CVAR (Color, am_ovthingcolor_monster,	0xe88800,	CVAR_ARCHIVE);
CVAR (Color, am_ovthingcolor_ncmonster,	0xe88800,	CVAR_ARCHIVE);
CVAR (Color, am_ovthingcolor_item,		0xe88800,	CVAR_ARCHIVE);
CVAR (Color, am_ovthingcolor_citem,		0xe88800,	CVAR_ARCHIVE);
CVAR (Color, am_ovportalcolor,			0x004022,	CVAR_ARCHIVE);

//=============================================================================
//
// internal representation of a single color
//
//=============================================================================

struct AMColor
{
	int Index;
	uint32_t RGB;

	void FromCVar(FColorCVar & cv)
	{
		Index = cv.GetIndex();
		RGB = uint32_t(cv) | MAKEARGB(255, 0, 0, 0);
	}

	void FromRGB(int r,int g, int b)
	{
		RGB = MAKEARGB(255, r, g, b);
		Index = ColorMatcher.Pick(r, g, b);
	}

	void setInvalid()
	{
		Index = -1;
		RGB = -1;
	}

	bool isValid() const
	{
		return Index > -1;
	}
};

//=============================================================================
//
// a complete color set
//
//=============================================================================

static const char *ColorNames[] = {
		"Background", 
		"YourColor", 
		"WallColor", 
		"TwoSidedWallColor",
		"FloorDiffWallColor", 
		"CeilingDiffWallColor", 
		"ExtraFloorWallColor", 
		"ThingColor",
		"ThingColor_Item", 
		"ThingColor_CountItem", 
		"ThingColor_Monster", 
		"ThingColor_NocountMonster", 
		"ThingColor_Friend",
		"SpecialWallColor", 
		"SecretWallColor", 
		"GridColor", 
		"XHairColor",
		"NotSeenColor",
		"LockedColor",
		"IntraTeleportColor", 
		"InterTeleportColor",
		"SecretSectorColor",
		"UnexploredSecretColor",
		"PortalColor",
		"AlmostBackgroundColor",
		NULL
};

struct AMColorset
{
	enum
	{
		Background, 
		YourColor, 
		WallColor, 
		TSWallColor,
		FDWallColor, 
		CDWallColor, 
		EFWallColor, 
		ThingColor,
		ThingColor_Item, 
		ThingColor_CountItem, 
		ThingColor_Monster, 
		ThingColor_NocountMonster, 
		ThingColor_Friend,
		SpecialWallColor, 
		SecretWallColor, 
		GridColor, 
		XHairColor,
		NotSeenColor,
		LockedColor,
		IntraTeleportColor, 
		InterTeleportColor,
		SecretSectorColor,
		UnexploredSecretColor,
		PortalColor,
		AlmostBackgroundColor,
		AM_NUM_COLORS
	};

	AMColor c[AM_NUM_COLORS];
	bool displayLocks;
	bool forcebackground;
	bool defined;	// only for mod specific colorsets: must be true to be usable

	void initFromCVars(FColorCVar **values)
	{
		for(int i=0;i<AlmostBackgroundColor; i++)
		{
			c[i].FromCVar(*values[i]);
		}

		uint32_t ba = *(values[0]);

		int r = RPART(ba) - 16;
		int g = GPART(ba) - 16;
		int b = BPART(ba) - 16;

		if (r < 0)
			r += 32;
		if (g < 0)
			g += 32;
		if (b < 0)
			b += 32;

		c[AlmostBackgroundColor].FromRGB(r, g, b);
		displayLocks = true;
		forcebackground = false;
	}

	void initFromColors(const unsigned char *colors, bool showlocks)
	{
		for(int i=0, j=0; i<AM_NUM_COLORS; i++, j+=3)
		{
			if (colors[j] == 1 && colors[j+1] == 0 && colors[j+2] == 0)
			{
				c[i].setInvalid();
			}
			else
			{
				c[i].FromRGB(colors[j], colors[j+1], colors[j+2]);
			}
		}
		displayLocks = showlocks;
		forcebackground = false;
	}

	void setWhite()
	{
		c[0].FromRGB(0,0,0);
		for(int i=1; i<AM_NUM_COLORS; i++)
		{
			c[i].FromRGB(255,255,255);
		}
	}

	const AMColor &operator[](int index) const
	{
		return c[index];
	}

	bool isValid(int index) const
	{
		return c[index].isValid();
	}
};

//=============================================================================
//
// automap colors forced by linedef
//
//=============================================================================

static const int AUTOMAP_LINE_COLORS[AMLS_COUNT] =
{
	-1, 								// AMLS_Default (unused)
	AMColorset::WallColor, 				// AMLS_OneSided,
	AMColorset::TSWallColor, 			// AMLS_TwoSided
	AMColorset::FDWallColor, 			// AMLS_FloorDiff
	AMColorset::CDWallColor, 			// AMLS_CeilingDiff
	AMColorset::EFWallColor, 			// AMLS_ExtraFloor
	AMColorset::SpecialWallColor, 		// AMLS_Special
	AMColorset::SecretWallColor, 		// AMLS_Secret
	AMColorset::NotSeenColor, 			// AMLS_NotSeen
	AMColorset::LockedColor, 			// AMLS_Locked
	AMColorset::IntraTeleportColor, 	// AMLS_IntraTeleport
	AMColorset::InterTeleportColor, 	// AMLS_InterTeleport
	AMColorset::UnexploredSecretColor, 	// AMLS_UnexploredSecret
	AMColorset::PortalColor, 			// AMLS_Portal
};

//=============================================================================
//
// predefined colorsets
//
//=============================================================================

static FColorCVar *cv_standard[] = {
	&am_backcolor,
	&am_yourcolor,
	&am_wallcolor,
	&am_tswallcolor,
	&am_fdwallcolor,
	&am_cdwallcolor,
	&am_efwallcolor,
	&am_thingcolor,
	&am_thingcolor_item,
	&am_thingcolor_citem,
	&am_thingcolor_monster,
	&am_thingcolor_ncmonster,
	&am_thingcolor_friend,
	&am_specialwallcolor,
	&am_secretwallcolor,
	&am_gridcolor,
	&am_xhaircolor,
	&am_notseencolor,
	&am_lockedcolor,
	&am_intralevelcolor,
	&am_interlevelcolor,
	&am_secretsectorcolor,
	&am_unexploredsecretcolor,
	&am_portalcolor
};

static FColorCVar *cv_overlay[] = {
	&am_backcolor,	// this will not be used in overlay mode
	&am_ovyourcolor,
	&am_ovwallcolor,
	&am_ovotherwallscolor,
	&am_ovfdwallcolor,
	&am_ovcdwallcolor,
	&am_ovefwallcolor,
	&am_ovthingcolor,
	&am_ovthingcolor_item,
	&am_ovthingcolor_citem,
	&am_ovthingcolor_monster,
	&am_ovthingcolor_ncmonster,
	&am_ovthingcolor_friend,
	&am_ovspecialwallcolor,
	&am_ovsecretwallcolor,
	&am_gridcolor,	// this will not be used in overlay mode
	&am_xhaircolor,	// this will not be used in overlay mode
	&am_ovunseencolor,
	&am_ovlockedcolor,
	&am_ovtelecolor,
	&am_ovinterlevelcolor,
	&am_ovsecretsectorcolor,
	&am_ovunexploredsecretcolor,
	&am_ovportalcolor
};

CCMD(am_restorecolors)
{
	for (unsigned i = 0; i < countof(cv_standard); i++)
	{
		cv_standard[i]->ResetToDefault();
	}
	for (unsigned i = 0; i < countof(cv_overlay); i++)
	{
		cv_overlay[i]->ResetToDefault();
	}
}



#define NOT_USED 1,0,0	// use almost black as indicator for an unused color

static unsigned char DoomColors[]= {
	0x00,0x00,0x00, // background
	0xff,0xff,0xff, // yourcolor
	0xfc,0x00,0x00, // wallcolor
	0x80,0x80,0x80, // tswallcolor
	0xbc,0x78,0x48,	// fdwallcolor
	0xfc,0xfc,0x00, // cdwallcolor
	0xbc,0x78,0x48,	// efwallcolor
	0x74,0xfc,0x6c, // thingcolor
	0x74,0xfc,0x6c, // thingcolor_item
	0x74,0xfc,0x6c, // thingcolor_citem
	0x74,0xfc,0x6c, // thingcolor_monster
	0x74,0xfc,0x6c, // thingcolor_ncmonster
	0x74,0xfc,0x6c, // thingcolor_friend
	NOT_USED,		// specialwallcolor
	NOT_USED,		// secretwallcolor
	0x4c,0x4c,0x4c,	// gridcolor
	0x80,0x80,0x80, // xhaircolor
	0x6c,0x6c,0x6c,	// notseencolor
	0xfc,0xfc,0x00, // lockedcolor
	NOT_USED,		// intrateleport
	NOT_USED,		// interteleport
	NOT_USED,		// secretsector
	NOT_USED,		// unexploredsecretsector
	0x10,0x10,0x10,	// almostbackground
	0x40,0x40,0x40	// portal
};

static unsigned char StrifeColors[]= {
	0x00,0x00,0x00, // background
	239, 239,   0,	// yourcolor
	199, 195, 195,	// wallcolor
	119, 115, 115,	// tswallcolor
	 55,  59,  91,	// fdwallcolor
	119, 115, 115,	// cdwallcolor
	 55,  59,  91,	// efwallcolor
	187,  59,   0,	// thingcolor
	219, 171,   0,	// thingcolor_item
	219, 171,   0,	// thingcolor_citem
	0xfc,0x00,0x00,	// thingcolor_monster
	0xfc,0x00,0x00,	// thingcolor_ncmonster
	0xfc,0x00,0x00, // thingcolor_friend
	NOT_USED,		// specialwallcolor
	NOT_USED,		// secretwallcolor
	0x4c,0x4c,0x4c,	// gridcolor
	0x80,0x80,0x80, // xhaircolor
	0x6c,0x6c,0x6c,	// notseencolor
	119, 115, 115,	// lockedcolor
	NOT_USED,		// intrateleport
	NOT_USED,		// interteleport
	NOT_USED,		// secretsector
	NOT_USED,		// unexploredsecretsector
	0x10,0x10,0x10,	// almostbackground
	0x40,0x40,0x40	// portal
};

static unsigned char RavenColors[]= {
	0x6c,0x54,0x40, // background
	0xff,0xff,0xff, // yourcolor
	 75,  50,  16,	// wallcolor
	 88,  93,  86,	// tswallcolor
	208, 176, 133,  // fdwallcolor
	103,  59,  31,	// cdwallcolor
	208, 176, 133,  // efwallcolor
	236, 236, 236,	// thingcolor
	236, 236, 236,	// thingcolor_item
	236, 236, 236,	// thingcolor_citem
	236, 236, 236,	// thingcolor_monster
	236, 236, 236,	// thingcolor_ncmonster
	236, 236, 236,	// thingcolor_friend
	NOT_USED,		// specialwallcolor
	NOT_USED,		// secretwallcolor
	 75,  50,  16,	// gridcolor
	0x00,0x00,0x00, // xhaircolor
	0x00,0x00,0x00,	// notseencolor
	103,  59,  31,	// lockedcolor
	NOT_USED,		// intrateleport
	NOT_USED,		// interteleport
	NOT_USED,		// secretsector
	NOT_USED,		// unexploredsecretsector
	0x10,0x10,0x10,	// almostbackground
	0x50,0x50,0x50	// portal
};

#undef NOT_USED

static AMColorset AMColors;
static AMColorset AMMod;
static AMColorset AMModOverlay;


void AM_ClearColorsets()
{
	AMModOverlay.defined = false;
	AMMod.defined = false;
}
//=============================================================================
//
//
//
//=============================================================================

void FMapInfoParser::ParseAMColors(bool overlay)
{
	bool colorset = false;

	AMColorset &cset = overlay? AMModOverlay : AMMod;

	cset.setWhite();
	cset.defined = true;
	sc.MustGetToken('{');
	while(sc.GetToken())
	{
		if (sc.TokenType == '}') return;

		sc.TokenMustBe(TK_Identifier);
		FString nextKey = sc.String;
		sc.MustGetToken('=');

		if (nextKey.CompareNoCase("base") == 0)
		{
			if (colorset) sc.ScriptError("'base' must be specified before the first color");
			sc.MustGetToken(TK_StringConst);
			if (sc.Compare("doom"))
			{
				cset.initFromColors(DoomColors, false);
			}
			else if (sc.Compare("raven"))
			{
				cset.initFromColors(RavenColors, true);
			}
			else if (sc.Compare("strife"))
			{
				cset.initFromColors(StrifeColors, false);
			}
			else
			{
				sc.ScriptError("Unknown value for 'base'. Must be 'Doom', 'Strife' or 'Raven'.");
			}
		}
		else if (nextKey.CompareNoCase("showlocks") == 0)
		{
			if(sc.CheckToken(TK_False)) 
				cset.displayLocks = false; 
			else 
			{ 
				sc.MustGetToken(TK_True); 
				cset.displayLocks = true; 
			} 
		}
		else
		{
			int i;
			for (i = 0; ColorNames[i] != NULL; i++)
			{
				if (nextKey.CompareNoCase(ColorNames[i]) == 0)
				{
					sc.MustGetToken(TK_StringConst);
					FString color = sc.String;
					FString colorName = V_GetColorStringByName(color);
					if(!colorName.IsEmpty()) color = colorName;
					int colorval = V_GetColorFromString(NULL, color);
					cset.c[i].FromRGB(RPART(colorval), GPART(colorval), BPART(colorval)); 
					colorset = true;
					break;
				}
			}
			if (ColorNames[i]== NULL)
			{
				sc.ScriptError("Unknown key '%s'", nextKey.GetChars());
			}
		}
	}
}

//=============================================================================
//
//
//
//=============================================================================

// scale on entry
#define INITSCALEMTOF .2
// used by MTOF to scale from map-to-frame-buffer coords
static double scale_mtof = INITSCALEMTOF;
// used by FTOM to scale from frame-buffer-to-map coords (=1/scale_mtof)
static double scale_ftom;

// translates between frame-buffer and map distances
inline double FTOM(double x)
{
	return x * scale_ftom;
}

inline double MTOF(double x)
{
	return x * scale_mtof;
}

static int bigstate = 0;
static bool textured = 1;	// internal toggle for texture mode
static int MapPortalGroup;

CUSTOM_CVAR(Bool, am_textured, false, CVAR_ARCHIVE)
{
	textured |= self;
}

CVAR(Int, am_showsubsector, -1, 0);


// Disable the ML_DONTDRAW line flag if x% of all lines in a map are flagged with it
// (To counter annoying mappers who think they are smart by making the automap unusable)
bool am_showallenabled;

CUSTOM_CVAR (Int, am_showalllines, -1, 0)	// This is a cheat so don't save it.
{
	int flagged = 0;
	int total = 0;
	if (self > 0 && level.lines.Size() > 0)
	{
		for(auto &line : level.lines)
		{
			// disregard intra-sector lines
			if (line.frontsector == line.backsector) continue;	

			// disregard control sectors for deep water
			if (line.frontsector->e->FakeFloor.Sectors.Size() > 0) continue;	

			// disregard control sectors for 3D-floors
			if (line.frontsector->e->XFloor.attached.Size() > 0) continue;	

			total++;
			if (line.flags & ML_DONTDRAW) flagged++;
		}
		am_showallenabled =  (flagged * 100 / total >= self);
	}
	else if (self == 0)
	{
		am_showallenabled = true;
	}
	else
	{
		am_showallenabled = false;
	}
}

EXTERN_CVAR (Bool, sv_cheats)
CUSTOM_CVAR (Int, am_cheat, 0, 0)
{
	// No automap cheat in net games when cheats are disabled!
	if (netgame && !sv_cheats && self != 0)
	{
		self = 0;
	}
}



#define AM_NUMMARKPOINTS 10

// player radius for automap checking
#define PLAYERRADIUS	16.

// how much the automap moves window per tic in frame-buffer coordinates
// moves 140 pixels at 320x200 in 1 second
#define F_PANINC		(140/TICRATE)
// how much zoom-in per tic
// goes to 2x in 1 second
#define M_ZOOMIN        (1.02)
// how much zoom-out per tic
// pulls out to 0.5x in 1 second
#define M_ZOOMOUT       (1/1.02)

// translates between frame-buffer and map coordinates
#define CXMTOF(x)  int(MTOF((x)-m_x)/* - f_x*/)
#define CYMTOF(y)  int(f_h - MTOF((y)-m_y)/* + f_y*/)

struct  fpoint_t
{
	int x, y;
};

struct fline_t
{
	fpoint_t a, b;
};

struct mpoint_t
{
	double x, y;
};

struct mline_t
{
	mpoint_t a, b;
};

struct islope_t
{
	double slp, islp;
};



//=============================================================================
//
// The vector graphics for the automap.
//  A line drawing of the player pointing right,
//   starting from the middle.
//
//=============================================================================

static TArray<mline_t> MapArrow;
static TArray<mline_t> CheatMapArrow;
static TArray<mline_t> CheatKey;
static TArray<mline_t> EasyKey;

#define L(a,b,c,d) { {(a), (b)}, {(c), (d)} }
static mline_t triangle_guy[] = {
	L (-.867,-.5, .867,-.5),
	L (.867,-.5, 0,1),
	L (0,1, -.867,-.5)
};
#define NUMTRIANGLEGUYLINES (sizeof(triangle_guy)/sizeof(mline_t))

static mline_t thintriangle_guy[] = {
	L (-.5,-.7, 1,0),
	L (1,0, -.5,.7),
	L (-.5,.7, -.5,-.7)
};
#define NUMTHINTRIANGLEGUYLINES (sizeof(thintriangle_guy)/sizeof(mline_t))

static mline_t square_guy[] = {
	L (0,1,1,0),
	L (1,0,0,-1),
	L (0,-1,-1,0),
	L (-1,0,0,1)
};
#define NUMSQUAREGUYLINES (sizeof(square_guy)/sizeof(mline_t))

//=============================================================================
//
//
//
//=============================================================================

static int 	grid = 0;

bool		automapactive = false;

// location of window on screen
static int	f_x;
static int	f_y;

// size of window on screen
static int	f_w;
static int	f_h;

static int	amclock;

static mpoint_t	m_paninc;		// how far the window pans each tic (map coords)
static double	mtof_zoommul;	// how far the window zooms in each tic (map coords)
static double	am_zoomdir;

static double	m_x, m_y;		// LL x,y where the window is on the map (map coords)
static double	m_x2, m_y2;		// UR x,y where the window is on the map (map coords)

//
// width/height of window on map (map coords)
//
static double	m_w;
static double	m_h;

// based on level size
static double	min_x, min_y, max_x, max_y;

static double	max_w; // max_x-min_x,
static double	max_h; // max_y-min_y

// based on player size
static double	min_w;
static double	min_h;


static double	min_scale_mtof; // used to tell when to stop zooming out
static double	max_scale_mtof; // used to tell when to stop zooming in

// old stuff for recovery later
static double old_m_w, old_m_h;
static double old_m_x, old_m_y;

// old location used by the Follower routine
static mpoint_t f_oldloc;

static FTextureID marknums[10]; // numbers used for marking by the automap
static mpoint_t markpoints[AM_NUMMARKPOINTS]; // where the points are
static int markpointnum = 0; // next point to be assigned

static FTextureID mapback;	// the automap background
static double mapystart=0; // y-value for the start of the map bitmap...used in the parallax stuff.
static double mapxstart=0; //x-value for the bitmap.

static bool stopped = true;

static void AM_calcMinMaxMtoF();

static void DrawMarker (FTexture *tex, double x, double y, int yadjust,
	INTBOOL flip, double xscale, double yscale, int translation, double alpha, uint32_t fillcolor, FRenderStyle renderstyle);

void AM_rotatePoint (double *x, double *y);
void AM_rotate (double *x, double *y, DAngle an);
void AM_doFollowPlayer ();


//=============================================================================
//
// map functions
//
//=============================================================================

bool AM_addMark ();
bool AM_clearMarks ();
void AM_saveScaleAndLoc ();
void AM_restoreScaleAndLoc ();
void AM_minOutWindowScale ();


CVAR(Bool, am_followplayer, true, CVAR_ARCHIVE)
CVAR(Bool, am_portaloverlay, true, CVAR_ARCHIVE)


CCMD(am_togglefollow)
{
	am_followplayer = !am_followplayer;
	f_oldloc.x = FLT_MAX;
	Printf ("%s\n", GStrings(am_followplayer ? "AMSTR_FOLLOWON" : "AMSTR_FOLLOWOFF"));
}

CCMD(am_togglegrid)
{
	grid = !grid;
	Printf ("%s\n", GStrings(grid ? "AMSTR_GRIDON" : "AMSTR_GRIDOFF"));
}

CCMD(am_toggletexture)
{
	if (am_textured && hasglnodes)
	{
		textured = !textured;
		Printf ("%s\n", GStrings(textured ? "AMSTR_TEXON" : "AMSTR_TEXOFF"));
	}
}

CCMD(am_setmark)
{
	if (AM_addMark())
	{
		Printf ("%s %d\n", GStrings("AMSTR_MARKEDSPOT"), markpointnum);
	}
}

CCMD(am_clearmarks)
{
	if (AM_clearMarks())
	{
		Printf ("%s\n", GStrings("AMSTR_MARKSCLEARED"));
	}
}

CCMD(am_gobig)
{
	bigstate = !bigstate;
	if (bigstate)
	{
		AM_saveScaleAndLoc();
		AM_minOutWindowScale();
	}
	else
		AM_restoreScaleAndLoc();
}

//=============================================================================
//
// vector graphics
//
//=============================================================================

void AM_ParseArrow(TArray<mline_t> &Arrow, const char *lumpname)
{
	const int R = int((8*PLAYERRADIUS)/7);
	FScanner sc;
	int lump = Wads.CheckNumForFullName(lumpname, true);
	if (lump >= 0)
	{
		sc.OpenLumpNum(lump);
		sc.SetCMode(true);
		while (sc.GetToken())
		{
			mline_t line;
			sc.TokenMustBe('(');
			sc.MustGetFloat();
			line.a.x = sc.Float*R;
			sc.MustGetToken(',');
			sc.MustGetFloat();
			line.a.y = sc.Float*R;
			sc.MustGetToken(')');
			sc.MustGetToken(',');
			sc.MustGetToken('(');
			sc.MustGetFloat();
			line.b.x = sc.Float*R;
			sc.MustGetToken(',');
			sc.MustGetFloat();
			line.b.y = sc.Float*R;
			sc.MustGetToken(')');
			Arrow.Push(line);
		}
	}
}

void AM_StaticInit()
{
	MapArrow.Clear();
	CheatMapArrow.Clear();
	CheatKey.Clear();
	EasyKey.Clear();

	if (gameinfo.mMapArrow.IsNotEmpty()) AM_ParseArrow(MapArrow, gameinfo.mMapArrow);
	if (gameinfo.mCheatMapArrow.IsNotEmpty()) AM_ParseArrow(CheatMapArrow, gameinfo.mCheatMapArrow);
	AM_ParseArrow(CheatKey, gameinfo.mCheatKey);
	AM_ParseArrow(EasyKey, gameinfo.mEasyKey);
	if (MapArrow.Size() == 0) I_FatalError("No automap arrow defined");

	char namebuf[9];

	for (int i = 0; i < 10; i++)
	{
		mysnprintf (namebuf, countof(namebuf), "AMMNUM%d", i);
		marknums[i] = TexMan.CheckForTexture (namebuf, ETextureType::MiscPatch);
	}
	markpointnum = 0;
	mapback.SetInvalid();
}

//=============================================================================
//
// called by the coordinate drawer
//
//=============================================================================

DVector2 AM_GetPosition()
{
	return DVector2((m_x + m_w / 2), (m_y + m_h / 2));
}

//=============================================================================
//
//
//
//=============================================================================

void AM_activateNewScale ()
{
	m_x += m_w/2;
	m_y += m_h/2;
	m_w = FTOM(f_w);
	m_h = FTOM(f_h);
	m_x -= m_w/2;
	m_y -= m_h/2;
	m_x2 = m_x + m_w;
	m_y2 = m_y + m_h;
}

//=============================================================================
//
//
//
//=============================================================================

void AM_saveScaleAndLoc ()
{
	old_m_x = m_x;
	old_m_y = m_y;
	old_m_w = m_w;
	old_m_h = m_h;
}

//=============================================================================
//
//
//
//=============================================================================

void AM_restoreScaleAndLoc ()
{
	m_w = old_m_w;
	m_h = old_m_h;
	if (!am_followplayer)
	{
		m_x = old_m_x;
		m_y = old_m_y;
    }
	else
	{
		m_x = players[consoleplayer].camera->X() - m_w/2;
		m_y = players[consoleplayer].camera->Y() - m_h/2;
    }
	m_x2 = m_x + m_w;
	m_y2 = m_y + m_h;

	// Change the scaling multipliers
	scale_mtof = f_w / m_w;
	scale_ftom = 1. / scale_mtof;
}

//=============================================================================
//
// adds a marker at the current location
//
//=============================================================================

bool AM_addMark ()
{
	if (marknums[0].isValid())
	{
		markpoints[markpointnum].x = m_x + m_w/2;
		markpoints[markpointnum].y = m_y + m_h/2;
		markpointnum = (markpointnum + 1) % AM_NUMMARKPOINTS;
		return true;
	}
	return false;
}

//=============================================================================
//
// Determines bounding box of all vertices,
// sets global variables controlling zoom range.
//
//=============================================================================

static void AM_findMinMaxBoundaries ()
{
	min_x = min_y = FLT_MAX;
	max_x = max_y = FIXED_MIN;
  
	for (auto &vert : level.vertexes)
	{
		if (vert.fX() < min_x)
			min_x = vert.fX();
		else if (vert.fX() > max_x)
			max_x = vert.fX();
    
		if (vert.fY() < min_y)
			min_y = vert.fY();
		else if (vert.fY() > max_y)
			max_y = vert.fY();
	}
  
	max_w = max_x - min_x;
	max_h = max_y - min_y;

	min_w = 2*PLAYERRADIUS; // const? never changed?
	min_h = 2*PLAYERRADIUS;

	AM_calcMinMaxMtoF();
}

//=============================================================================
//
//
//
//=============================================================================

static void AM_calcMinMaxMtoF()
{
	const double safe_frame = 1.0 - am_emptyspacemargin / 100.0;
	double a = safe_frame * (SCREENWIDTH / max_w);
	double b = safe_frame * (StatusBar->GetTopOfStatusbar() / max_h);

	min_scale_mtof = a < b ? a : b;
	max_scale_mtof = SCREENHEIGHT / (2*PLAYERRADIUS);
}

//=============================================================================
//
//
//
//=============================================================================

static void AM_ClipRotatedExtents (double pivotx, double pivoty)
{
	if (am_rotate == 0 || (am_rotate == 2 && !viewactive))
	{
		if (m_x + m_w/2 > max_x)
			m_x = max_x - m_w/2;
		else if (m_x + m_w/2 < min_x)
			m_x = min_x - m_w/2;
	  
		if (m_y + m_h/2 > max_y)
			m_y = max_y - m_h/2;
		else if (m_y + m_h/2 < min_y)
			m_y = min_y - m_h/2;
	}
	else
	{
#if 0
		double rmin_x, rmin_y, rmax_x, rmax_y;
		double xs[5], ys[5];
		int i;

		xs[0] = min_x;	ys[0] = min_y;
		xs[1] = max_x;	ys[1] = min_y;
		xs[2] = max_x;	ys[2] = max_y;
		xs[3] = min_x;	ys[3] = max_y;
		xs[4] = m_x + m_w/2; ys[4] = m_y + m_h/2;
		rmin_x = rmin_y = FLT_MAX;
		rmax_x = rmax_y = FIXED_MIN;

		for (i = 0; i < 5; ++i)
		{
			xs[i] -= pivotx;
			ys[i] -= pivoty;
			AM_rotate (&xs[i], &ys[i], 90. - players[consoleplayer].camera->Angles.Yaw);

			if (i == 5)
				break;
//			xs[i] += pivotx;
//			ys[i] += pivoty;

			if (xs[i] < rmin_x)	rmin_x = xs[i];
			if (xs[i] > rmax_x) rmax_x = xs[i];
			if (ys[i] < rmin_y) rmin_y = ys[i];
			if (ys[i] > rmax_y) rmax_y = ys[i];
		}
		if (rmax_x < 0)
			xs[4] = -rmax_x;
		else if (rmin_x > 0)
			xs[4] = -rmin_x;
	  
//		if (ys[4] > rmax_y)
//			ys[4] = rmax_y;
//		else if (ys[4] < rmin_y)
//			ys[4] = rmin_y;
		AM_rotate (&xs[4], &ys[4], 270. - players[consoleplayer].camera->Angles.Yaw);
		m_x = xs[4] + pivotx - m_w/2;
		m_y = ys[4] + pivoty - m_h/2;
#endif
	}

	m_x2 = m_x + m_w;
	m_y2 = m_y + m_h;
}

//=============================================================================
//
//
//
//=============================================================================

static void AM_ScrollParchment (double dmapx, double dmapy)
{
	mapxstart = mapxstart - dmapx * scale_mtof;
	mapystart = mapystart - dmapy * scale_mtof;

	mapxstart = clamp(mapxstart, -40000., 40000.);
	mapystart = clamp(mapystart, -40000., 40000.);

	if (mapback.isValid())
	{
		FTexture *backtex = TexMan[mapback];

		if (backtex != NULL)
		{
			int pwidth = backtex->GetWidth();
			int pheight = backtex->GetHeight();

			while(mapxstart > 0)
				mapxstart -= pwidth;
			while(mapxstart <= -pwidth)
				mapxstart += pwidth;
			while(mapystart > 0)
				mapystart -= pheight;
			while(mapystart <= -pheight)
				mapystart += pheight;
		}
	}
}

//=============================================================================
//
//
//
//=============================================================================

void AM_changeWindowLoc ()
{
	if (m_paninc.x || m_paninc.y)
	{
		am_followplayer = false;
		f_oldloc.x = FLT_MAX;
	}

	double oldmx = m_x, oldmy = m_y;
	double incx, incy, oincx, oincy;
	
	incx = m_paninc.x;
	incy = m_paninc.y;

	oincx = incx = m_paninc.x * SCREENWIDTH / 320;
	oincy = incy = m_paninc.y * SCREENHEIGHT / 200;
	if (am_rotate == 1 || (am_rotate == 2 && viewactive))
	{
		AM_rotate(&incx, &incy, players[consoleplayer].camera->Angles.Yaw - 90.);
	}

	m_x += incx;
	m_y += incy;

	AM_ClipRotatedExtents (oldmx + m_w/2, oldmy + m_h/2);
	AM_ScrollParchment (m_x != oldmx ? oincx : 0, m_y != oldmy ? -oincy : 0);
}


//=============================================================================
//
//
//
//=============================================================================

void AM_initVariables ()
{
	int pnum;

	automapactive = true;

	// Reset AM buttons
	Button_AM_PanLeft.Reset();
	Button_AM_PanRight.Reset();
	Button_AM_PanUp.Reset();
	Button_AM_PanDown.Reset();
	Button_AM_ZoomIn.Reset();
	Button_AM_ZoomOut.Reset();


	f_oldloc.x = FLT_MAX;
	amclock = 0;

	m_paninc.x = m_paninc.y = 0;
	mtof_zoommul = 1.;

	m_w = FTOM(SCREENWIDTH);
	m_h = FTOM(SCREENHEIGHT);

	// find player to center on initially
	if (!playeringame[pnum = consoleplayer])
		for (pnum=0;pnum<MAXPLAYERS;pnum++)
			if (playeringame[pnum])
				break;
	assert(pnum >= 0 && pnum < MAXPLAYERS);
	m_x = players[pnum].camera->X() - m_w/2;
	m_y = players[pnum].camera->Y() - m_h/2;
	AM_changeWindowLoc();

	// for saving & restoring
	old_m_x = m_x;
	old_m_y = m_y;
	old_m_w = m_w;
	old_m_h = m_h;
}

//=============================================================================
//
//
//
//=============================================================================

static void AM_initColors (bool overlayed)
{
	if (overlayed)
	{
		if (am_customcolors && AMModOverlay.defined)
		{
			AMColors = AMModOverlay;
		}
		else
		{
			AMColors.initFromCVars(cv_overlay);
		}
	}
	else if (am_customcolors && AMMod.defined)
	{
		AMColors = AMMod;
	}
	else switch(am_colorset)
	{
		default:
			/* Use the custom colors in the am_* cvars */
			AMColors.initFromCVars(cv_standard);
			break;

		case 1:	// Doom
			// Use colors corresponding to the original Doom's
			AMColors.initFromColors(DoomColors, false);
			break;

		case 2:	// Strife
			// Use colors corresponding to the original Strife's
			AMColors.initFromColors(StrifeColors, false);
			break;

		case 3:	// Raven
			// Use colors corresponding to the original Raven's
			AMColors.initFromColors(RavenColors, true);
			break;

	}
}

//=============================================================================
//
//
//
//=============================================================================

bool AM_clearMarks ()
{
	for (int i = AM_NUMMARKPOINTS-1; i >= 0; i--)
		markpoints[i].x = -1; // means empty
	markpointnum = 0;
	return marknums[0].isValid();
}

//=============================================================================
//
// called right after the level has been loaded
//
//=============================================================================

void AM_LevelInit ()
{
	if (level.info->MapBackground.Len() == 0)
	{
		mapback = TexMan.CheckForTexture("AUTOPAGE", ETextureType::MiscPatch);
	}
	else
	{
		mapback = TexMan.CheckForTexture(level.info->MapBackground, ETextureType::MiscPatch);
	}

	AM_clearMarks();

	AM_findMinMaxBoundaries();
	scale_mtof = min_scale_mtof / 0.7;
	if (scale_mtof > max_scale_mtof)
		scale_mtof = min_scale_mtof;
	scale_ftom = 1 / scale_mtof;

	am_showalllines.Callback();
}

//=============================================================================
//
//
//
//=============================================================================

void AM_Stop ()
{
	automapactive = false;
	stopped = true;
	viewactive = true;
}

//=============================================================================
//
//
//
//=============================================================================

void AM_Start ()
{
	if (!stopped) AM_Stop();
	stopped = false;
	AM_initVariables();
}



//=============================================================================
//
// set the window scale to the maximum size
//
//=============================================================================

void AM_minOutWindowScale ()
{
	scale_mtof = min_scale_mtof;
	scale_ftom = 1/ scale_mtof;
}

//=============================================================================
//
// set the window scale to the minimum size
//
//=============================================================================

void AM_maxOutWindowScale ()
{
	scale_mtof = max_scale_mtof;
	scale_ftom = 1 / scale_mtof;
}

//=============================================================================
//
// Called right after the resolution has changed
//
//=============================================================================

void AM_NewResolution()
{
	double oldmin = min_scale_mtof;
	
	if ( oldmin == 0 ) 
	{
		return; // [SP] Not in a game, exit!
	}	
	AM_calcMinMaxMtoF();
	scale_mtof = scale_mtof * min_scale_mtof / oldmin;
	scale_ftom = 1 / scale_mtof;
	if (scale_mtof < min_scale_mtof)
		AM_minOutWindowScale();
	else if (scale_mtof > max_scale_mtof)
		AM_maxOutWindowScale();
	f_w = screen->GetWidth();
	f_h = StatusBar->GetTopOfStatusbar();
	AM_activateNewScale();
}


//=============================================================================
//
//
//
//=============================================================================

CCMD (togglemap)
{
	if (gameaction == ga_nothing)
	{
		gameaction = ga_togglemap;
	}
}

//=============================================================================
//
//
//
//=============================================================================

void AM_ToggleMap ()
{
	if (gamestate != GS_LEVEL)
		return;

	// Don't activate the automap if we're not allowed to use it.
	if (dmflags2 & DF2_NO_AUTOMAP)
		return;

	if (!automapactive)
	{
		AM_Start ();
		viewactive = (am_overlay != 0.f);
	}
	else
	{
		if (am_overlay==1 && viewactive)
		{
			viewactive = false;
		}
		else
		{
			AM_Stop ();
		}
	}
}

//=============================================================================
//
// Handle events (user inputs) in automap mode
//
//=============================================================================

bool AM_Responder (event_t *ev, bool last)
{
	if (automapactive && (ev->type == EV_KeyDown || ev->type == EV_KeyUp))
	{
		if (am_followplayer)
		{
			// check for am_pan* and ignore in follow mode
			const char *defbind = AutomapBindings.GetBind(ev->data1);
			if (!strnicmp(defbind, "+am_pan", 7)) return false;
		}

		bool res = C_DoKey(ev, &AutomapBindings, NULL);
		if (res && ev->type == EV_KeyUp && !last)
		{
			// If this is a release event we also need to check if it released a button in the main Bindings
			// so that that button does not get stuck.
			const char *defbind = Bindings.GetBind(ev->data1);
			return (defbind[0] != '+'); // Let G_Responder handle button releases
		}
		return res;
	}
	return false;
}


//=============================================================================
//
// Zooming
//
//=============================================================================

void AM_changeWindowScale ()
{
	double mtof_zoommul;

	if (am_zoomdir > 0)
	{
		mtof_zoommul = M_ZOOMIN * am_zoomdir;
	}
	else if (am_zoomdir < 0)
	{
		mtof_zoommul = M_ZOOMOUT / -am_zoomdir;
	}
	else if (Button_AM_ZoomIn.bDown)
	{
		mtof_zoommul = M_ZOOMIN;
	}
	else if (Button_AM_ZoomOut.bDown)
	{
		mtof_zoommul = M_ZOOMOUT;
	}
	else
	{
		mtof_zoommul = 1;
	}
	am_zoomdir = 0;

	// Change the scaling multipliers
	scale_mtof = scale_mtof * mtof_zoommul;
	scale_ftom = 1 / scale_mtof;

	if (scale_mtof < min_scale_mtof)
		AM_minOutWindowScale();
	else if (scale_mtof > max_scale_mtof)
		AM_maxOutWindowScale();
}

CCMD(am_zoom)
{
	if (argv.argc() >= 2)
	{
		am_zoomdir = atof(argv[1]);
	}
}

//=============================================================================
//
//
//
//=============================================================================

void AM_doFollowPlayer ()
{
	double sx, sy;
	auto cam = players[consoleplayer].camera;
	if (cam != nullptr)
	{
		double delta = cam->player ? cam->player->viewz - cam->Z() : cam->GetCameraHeight();
		DVector3 ampos = cam->GetPortalTransition(delta);

		if (f_oldloc.x != ampos.X || f_oldloc.y != ampos.Y)
		{
			m_x = ampos.X - m_w / 2;
			m_y = ampos.Y - m_h / 2;
			m_x2 = m_x + m_w;
			m_y2 = m_y + m_h;

			// do the parallax parchment scrolling.
			sx = (ampos.X - f_oldloc.x);
			sy = (f_oldloc.y - ampos.Y);
			if (am_rotate == 1 || (am_rotate == 2 && viewactive))
			{
				AM_rotate(&sx, &sy, cam->Angles.Yaw - 90);
			}
			AM_ScrollParchment(sx, sy);

			f_oldloc.x = ampos.X;
			f_oldloc.y = ampos.Y;
		}
	}
}

//=============================================================================
//
// Updates on Game Tick
//
//=============================================================================

void AM_Ticker ()
{
	if (!automapactive)
		return;

	amclock++;

	if (am_followplayer)
	{
		AM_doFollowPlayer();
	}
	else
	{
		m_paninc.x = m_paninc.y = 0;
		if (Button_AM_PanLeft.bDown) m_paninc.x -= FTOM(F_PANINC);
		if (Button_AM_PanRight.bDown) m_paninc.x += FTOM(F_PANINC);
		if (Button_AM_PanUp.bDown) m_paninc.y += FTOM(F_PANINC);
		if (Button_AM_PanDown.bDown) m_paninc.y -= FTOM(F_PANINC);
	}

	// Change the zoom if necessary
	if (Button_AM_ZoomIn.bDown || Button_AM_ZoomOut.bDown || am_zoomdir != 0)
		AM_changeWindowScale();

	// Change x,y location
	//if (m_paninc.x || m_paninc.y)
		AM_changeWindowLoc();
}


//=============================================================================
//
// Clear automap frame buffer.
//
//=============================================================================

void AM_clearFB (const AMColor &color)
{
	bool drawback = mapback.isValid() && am_drawmapback != 0;
	if (am_drawmapback == 2)
	{
		// only draw background when using a mod defined custom color set or Raven colors, if am_drawmapback is 2.
		if (!am_customcolors || !AMMod.defined)
		{
			drawback &= (am_colorset == 3);
		}
	}

	if (!drawback)
	{
		screen->Clear (0, 0, f_w, f_h, color.Index, color.RGB);
	}
	else
	{
		FTexture *backtex = TexMan[mapback];
		if (backtex != NULL)
		{
			int pwidth = backtex->GetWidth();
			int pheight = backtex->GetHeight();
			int x, y;

			//blit the automap background to the screen.
			for (y = int(mapystart); y < f_h; y += pheight)
			{
				for (x = int(mapxstart); x < f_w; x += pwidth)
				{
					screen->DrawTexture (backtex, x, y, DTA_ClipBottom, f_h, DTA_TopOffset, 0, DTA_LeftOffset, 0, TAG_DONE);
				}
			}
		}
	}
}


//=============================================================================
//
// Automap clipping of lines.
//
// Based on Cohen-Sutherland clipping algorithm but with a slightly
// faster reject and precalculated slopes.  If the speed is needed,
// use a hash algorithm to handle the common cases.
//
//=============================================================================

bool AM_clipMline (mline_t *ml, fline_t *fl)
{
	enum {
		LEFT	=1,
		RIGHT	=2,
		BOTTOM	=4,
		TOP		=8
	};

	int outcode1 = 0;
	int outcode2 = 0;
	int outside;

	fpoint_t tmp = { 0, 0 };
	int dx;
	int dy;

#define DOOUTCODE(oc, mx, my) \
	(oc) = 0; \
	if ((my) < 0) (oc) |= TOP; \
	else if ((my) >= f_h) (oc) |= BOTTOM; \
	if ((mx) < 0) (oc) |= LEFT; \
	else if ((mx) >= f_w) (oc) |= RIGHT;

	// do trivial rejects and outcodes
	if (ml->a.y > m_y2)
		outcode1 = TOP;
	else if (ml->a.y < m_y)
		outcode1 = BOTTOM;

	if (ml->b.y > m_y2)
		outcode2 = TOP;
	else if (ml->b.y < m_y)
		outcode2 = BOTTOM;

	if (outcode1 & outcode2)
		return false; // trivially outside

	if (ml->a.x < m_x)
		outcode1 |= LEFT;
	else if (ml->a.x > m_x2)
		outcode1 |= RIGHT;

	if (ml->b.x < m_x)
		outcode2 |= LEFT;
	else if (ml->b.x > m_x2)
		outcode2 |= RIGHT;

	if (outcode1 & outcode2)
		return false; // trivially outside

	// transform to frame-buffer coordinates.
	fl->a.x = CXMTOF(ml->a.x);
	fl->a.y = CYMTOF(ml->a.y);
	fl->b.x = CXMTOF(ml->b.x);
	fl->b.y = CYMTOF(ml->b.y);

	DOOUTCODE(outcode1, fl->a.x, fl->a.y);
	DOOUTCODE(outcode2, fl->b.x, fl->b.y);

	if (outcode1 & outcode2)
		return false;

	while (outcode1 | outcode2) {
		// may be partially inside box
		// find an outside point
		if (outcode1)
			outside = outcode1;
		else
			outside = outcode2;
	
		// clip to each side
		if (outside & TOP)
		{
			dy = fl->a.y - fl->b.y;
			dx = fl->b.x - fl->a.x;
			tmp.x = fl->a.x + Scale(dx, fl->a.y, dy);
			tmp.y = 0;
		}
		else if (outside & BOTTOM)
		{
			dy = fl->a.y - fl->b.y;
			dx = fl->b.x - fl->a.x;
			tmp.x = fl->a.x + Scale(dx, fl->a.y - f_h, dy);
			tmp.y = f_h-1;
		}
		else if (outside & RIGHT)
		{
			dy = fl->b.y - fl->a.y;
			dx = fl->b.x - fl->a.x;
			tmp.y = fl->a.y + Scale(dy, f_w-1 - fl->a.x, dx);
			tmp.x = f_w-1;
		}
		else if (outside & LEFT)
		{
			dy = fl->b.y - fl->a.y;
			dx = fl->b.x - fl->a.x;
			tmp.y = fl->a.y + Scale(dy, -fl->a.x, dx);
			tmp.x = 0;
		}

		if (outside == outcode1)
		{
			fl->a = tmp;
			DOOUTCODE(outcode1, fl->a.x, fl->a.y);
		}
		else
		{
			fl->b = tmp;
			DOOUTCODE(outcode2, fl->b.x, fl->b.y);
		}
	
		if (outcode1 & outcode2)
			return false; // trivially outside
	}

	return true;
}
#undef DOOUTCODE


//=============================================================================
//
// Clip lines, draw visible parts of lines.
//
//=============================================================================

void AM_drawMline (mline_t *ml, const AMColor &color)
{
	fline_t fl;

	if (AM_clipMline (ml, &fl))
	{
		screen->DrawLine (f_x + fl.a.x, f_y + fl.a.y, f_x + fl.b.x, f_y + fl.b.y, color.Index, color.RGB);
	}
}

inline void AM_drawMline (mline_t *ml, int colorindex)
{
	AM_drawMline(ml, AMColors[colorindex]);
}

//=============================================================================
//
// Draws flat (floor/ceiling tile) aligned grid lines.
//
//=============================================================================

void AM_drawGrid (int color)
{
	double x, y;
	double start, end;
	mline_t ml;
	double minlen, extx, exty;
	double minx, miny;
	auto bmaporgx = level.blockmap.bmaporgx;
	auto bmaporgy = level.blockmap.bmaporgy;

	// [RH] Calculate a minimum for how long the grid lines should be so that
	// they cover the screen at any rotation.
	minlen = sqrt (m_w*m_w + m_h*m_h);
	extx = (minlen - m_w) / 2;
	exty = (minlen - m_h) / 2;

	minx = m_x;
	miny = m_y;

	// Figure out start of vertical gridlines
	start = minx - extx;
	start = ceil((start - bmaporgx) / FBlockmap::MAPBLOCKUNITS) * FBlockmap::MAPBLOCKUNITS + bmaporgx;

	end = minx + minlen - extx;

	// draw vertical gridlines
	for (x = start; x < end; x += FBlockmap::MAPBLOCKUNITS)
	{
		ml.a.x = x;
		ml.b.x = x;
		ml.a.y = miny - exty;
		ml.b.y = ml.a.y + minlen;
		if (am_rotate == 1 || (am_rotate == 2 && viewactive))
		{
			AM_rotatePoint (&ml.a.x, &ml.a.y);
			AM_rotatePoint (&ml.b.x, &ml.b.y);
		}
		AM_drawMline(&ml, color);
	}

	// Figure out start of horizontal gridlines
	start = miny - exty;
	start = ceil((start - bmaporgy) / FBlockmap::MAPBLOCKUNITS) * FBlockmap::MAPBLOCKUNITS + bmaporgy;
	end = miny + minlen - exty;

	// draw horizontal gridlines
	for (y=start; y<end; y+=FBlockmap::MAPBLOCKUNITS)
	{
		ml.a.x = minx - extx;
		ml.b.x = ml.a.x + minlen;
		ml.a.y = y;
		ml.b.y = y;
		if (am_rotate == 1 || (am_rotate == 2 && viewactive))
		{
			AM_rotatePoint (&ml.a.x, &ml.a.y);
			AM_rotatePoint (&ml.b.x, &ml.b.y);
		}
		AM_drawMline (&ml, color);
	}
}

//==========================================================================
//
// This was previously using the variants from the renderers but with
// all globals being factored out this will become dangerouns and unpredictable
// as the original R_FakeFlat heavily depended on global variables from
// the last rendered scene.
//
//==========================================================================

sector_t * AM_FakeFlat(AActor *viewer, sector_t * sec, sector_t * dest)
{
	if (sec->GetHeightSec() == nullptr) return sec;

	DVector3 pos = viewer->Pos();
	
	if (viewer->player)
	{
		pos.Z = viewer->player->viewz;
	}
	else
	{
		pos.Z += viewer->GetCameraHeight();
	}

	int in_area;
	if (viewer->Sector->GetHeightSec() == nullptr)
	{
		in_area = 0;
	}
	else
	{
		in_area = pos.Z <= viewer->Sector->heightsec->floorplane.ZatPoint(pos) ? -1 :
			(pos.Z > viewer->Sector->heightsec->ceilingplane.ZatPoint(pos) && !(viewer->Sector->heightsec->MoreFlags&SECMF_FAKEFLOORONLY)) ? 1 : 0;
	}

	int diffTex = (sec->heightsec->MoreFlags & SECMF_CLIPFAKEPLANES);
	sector_t * s = sec->heightsec;

	memcpy(dest, sec, sizeof(sector_t));

	// Replace floor height with control sector's heights.
	// The automap is only interested in the floor so let's skip the ceiling.
	if (diffTex)
	{
		if (s->floorplane.CopyPlaneIfValid(&dest->floorplane, &sec->ceilingplane))
		{
			dest->SetTexture(sector_t::floor, s->GetTexture(sector_t::floor), false);
			dest->SetPlaneTexZQuick(sector_t::floor, s->GetPlaneTexZ(sector_t::floor));
		}
		else if (s->MoreFlags & SECMF_FAKEFLOORONLY)
		{
			if (in_area == -1)
			{
				dest->Colormap = s->Colormap;
				if (!(s->MoreFlags & SECMF_NOFAKELIGHT))
				{
					dest->lightlevel = s->lightlevel;
					dest->SetPlaneLight(sector_t::floor, s->GetPlaneLight(sector_t::floor));
					dest->ChangeFlags(sector_t::floor, -1, s->GetFlags(sector_t::floor));
				}
				return dest;
			}
			return sec;
		}
	}
	else
	{
		dest->SetPlaneTexZQuick(sector_t::floor, s->GetPlaneTexZ(sector_t::floor));
		dest->floorplane = s->floorplane;
	}

	if (in_area == -1)
	{
		dest->Colormap = s->Colormap;
		dest->SetPlaneTexZQuick(sector_t::floor, sec->GetPlaneTexZ(sector_t::floor));
		dest->floorplane = sec->floorplane;
		if (!(s->MoreFlags & SECMF_NOFAKELIGHT))
		{
			dest->lightlevel = s->lightlevel;
		}

		dest->SetTexture(sector_t::floor, diffTex ? sec->GetTexture(sector_t::floor) : s->GetTexture(sector_t::floor), false);
		dest->planes[sector_t::floor].xform = s->planes[sector_t::floor].xform;

		if (!(s->MoreFlags & SECMF_NOFAKELIGHT))
		{
			dest->SetPlaneLight(sector_t::floor, s->GetPlaneLight(sector_t::floor));
			dest->ChangeFlags(sector_t::floor, -1, s->GetFlags(sector_t::floor));
		}
	}
	else if (in_area == 1)
	{
		dest->Colormap = s->Colormap;
		dest->SetPlaneTexZQuick(sector_t::floor, s->GetPlaneTexZ(sector_t::ceiling));
		dest->floorplane = s->ceilingplane;
		if (!(s->MoreFlags & SECMF_NOFAKELIGHT))
		{
			dest->lightlevel = s->lightlevel;
		}

		dest->SetTexture(sector_t::floor, s->GetTexture(sector_t::ceiling), false);

		if (s->GetTexture(sector_t::floor) != skyflatnum)
		{
			dest->SetTexture(sector_t::floor, s->GetTexture(sector_t::floor), false);
			dest->planes[sector_t::floor].xform = s->planes[sector_t::floor].xform;
		}

		if (!(s->MoreFlags & SECMF_NOFAKELIGHT))
		{
			dest->lightlevel = s->lightlevel;
			dest->SetPlaneLight(sector_t::floor, s->GetPlaneLight(sector_t::floor));
			dest->ChangeFlags(sector_t::floor, -1, s->GetFlags(sector_t::floor));
		}
	}
	return dest;
}

//=============================================================================
//
// AM_drawSubsectors
//
//=============================================================================

void AM_drawSubsectors()
{
	static TArray<FVector2> points;
	double scale = scale_mtof;
	DAngle rotation;
	sector_t tempsec;
	int floorlight;
	double scalex, scaley;
	double originx, originy;
	FColormap colormap;
	PalEntry flatcolor;
	mpoint_t originpt;

	auto &subsectors = level.subsectors;
	for (unsigned i = 0; i < subsectors.Size(); ++i)
	{
		if (subsectors[i].flags & SSECF_POLYORG)
		{
			continue;
		}

		if ((!(subsectors[i].flags & SSECMF_DRAWN) || (subsectors[i].render_sector->MoreFlags & SECMF_HIDDEN)) && am_cheat == 0)
		{
			continue;
		}

		if (am_portaloverlay && subsectors[i].render_sector->PortalGroup != MapPortalGroup && subsectors[i].render_sector->PortalGroup != 0)
		{
			continue;
		}

		// Fill the points array from the subsector.
		points.Resize(subsectors[i].numlines);
		for (uint32_t j = 0; j < subsectors[i].numlines; ++j)
		{
			mpoint_t pt = { subsectors[i].firstline[j].v1->fX(),
							subsectors[i].firstline[j].v1->fY() };
			if (am_rotate == 1 || (am_rotate == 2 && viewactive))
			{
				AM_rotatePoint(&pt.x, &pt.y);
			}
			points[j].X = float(f_x + ((pt.x - m_x) * scale));
			points[j].Y = float(f_y + (f_h - (pt.y - m_y) * scale));
		}
		// For lighting and texture determination
		sector_t *sec = AM_FakeFlat(players[consoleplayer].camera, subsectors[i].render_sector, &tempsec);
		floorlight = sec->GetFloorLight();
		// Find texture origin.
		originpt.x = -sec->GetXOffset(sector_t::floor);
		originpt.y = sec->GetYOffset(sector_t::floor);
		rotation = -sec->GetAngle(sector_t::floor);
		// Coloring for the polygon
		colormap = sec->Colormap;

		FTextureID maptex = sec->GetTexture(sector_t::floor);
		flatcolor = sec->SpecialColors[sector_t::floor];

		scalex = sec->GetXScale(sector_t::floor);
		scaley = sec->GetYScale(sector_t::floor);

		if (sec->e->XFloor.ffloors.Size())
		{
			secplane_t *floorplane = &sec->floorplane;

			// Look for the highest floor below the camera viewpoint.
			// Check the center of the subsector's sector. Do not check each
			// subsector separately because that might result in different planes for
			// different subsectors of the same sector which is not wanted here.
			// (Make the comparison in floating point to avoid overflows and improve performance.)
			double secx;
			double secy;
			double seczb, seczt;
            auto &vp = r_viewpoint;
			double cmpz = vp.Pos.Z;

			if (players[consoleplayer].camera && sec == players[consoleplayer].camera->Sector)
			{
				// For the actual camera sector use the current viewpoint as reference.
				secx = vp.Pos.X;
				secy = vp.Pos.Y;
			}
			else
			{
				secx = sec->centerspot.X;
				secy = sec->centerspot.Y;
			}
			seczb = floorplane->ZatPoint(secx, secy);
			seczt = sec->ceilingplane.ZatPoint(secx, secy);

			for (unsigned int i = 0; i < sec->e->XFloor.ffloors.Size(); ++i)
			{
				F3DFloor *rover = sec->e->XFloor.ffloors[i];
				if (!(rover->flags & FF_EXISTS)) continue;
				if (rover->flags & (FF_FOG|FF_THISINSIDE)) continue;
				if (!(rover->flags & FF_RENDERPLANES)) continue;
				if (rover->alpha == 0) continue;
				double roverz = rover->top.plane->ZatPoint(secx, secy);
				// Ignore 3D floors that are above or below the sector itself:
				// they are hidden. Since 3D floors are sorted top to bottom,
				// if we get below the sector floor, we can stop.
				if (roverz > seczt) continue;
				if (roverz < seczb) break;
				if (roverz < cmpz)
				{
					maptex = *(rover->top.texture);
					flatcolor = *(rover->top.flatcolor);
					floorplane = rover->top.plane;
					sector_t *model = rover->top.model;
					int selector = (rover->flags & FF_INVERTPLANES) ? sector_t::floor : sector_t::ceiling;
					rotation = -model->GetAngle(selector);
					scalex = model->GetXScale(selector);
					scaley = model->GetYScale(selector);
					originpt.x = -model->GetXOffset(selector);
					originpt.y = model->GetYOffset(selector);
					break;
				}
			}

			lightlist_t *light = P_GetPlaneLight(sec, floorplane, false);
			floorlight = *light->p_lightlevel;
			colormap = light->extra_colormap;
		}
		if (maptex == skyflatnum)
		{
			continue;
		}

		// Apply the floor's rotation to the texture origin.
		if (rotation != 0)
		{
			AM_rotate(&originpt.x, &originpt.y, rotation);
		}
		// Apply the automap's rotation to the texture origin.
		if (am_rotate == 1 || (am_rotate == 2 && viewactive))
		{
			rotation = rotation + 90. - players[consoleplayer].camera->Angles.Yaw;
			AM_rotatePoint(&originpt.x, &originpt.y);
		}
		originx = f_x + ((originpt.x - m_x) * scale);
		originy = f_y + (f_h - (originpt.y - m_y) * scale);

		// If this subsector has not actually been seen yet (because you are cheating
		// to see it on the map), tint and desaturate it.
		if (!(subsectors[i].flags & SSECMF_DRAWN))
		{
			colormap.LightColor = PalEntry(
				(colormap.LightColor.r + 255) / 2,
				(colormap.LightColor.g + 200) / 2,
				(colormap.LightColor.b + 160) / 2);
			colormap.Desaturation = 255 - (255 - colormap.Desaturation) / 4;
		}
		// make table based fog visible on the automap as well.
		if (level.flags & LEVEL_HASFADETABLE)
		{
			colormap.FadeColor = PalEntry(0, 128, 128, 128);
		}

		// Draw the polygon.
		FTexture *pic = TexMan(maptex);
		if (pic != NULL && pic->UseType != ETextureType::Null)
		{
			screen->FillSimplePoly(TexMan(maptex),
				&points[0], points.Size(),
				originx, originy,
				scale / scalex,
				scale / scaley,
				rotation,
				colormap,
				flatcolor,
				floorlight,
				f_y + f_h
				);
		}
	}
}

//=============================================================================
//
//
//
//=============================================================================

static int AM_CheckSecret(line_t *line)
{
	if (AMColors.isValid(AMColors.SecretSectorColor))
	{
		if (line->frontsector != NULL)
		{
			if (line->frontsector->wasSecret())
			{
				if (am_map_secrets!=0 && !line->frontsector->isSecret()) return 1;
				if (am_map_secrets==2 && !(line->flags & ML_SECRET)) return 2;
			}
		}
		if (line->backsector != NULL)
		{
			if (line->backsector->wasSecret())
			{
				if (am_map_secrets!=0 && !line->backsector->isSecret()) return 1;
				if (am_map_secrets==2 && !(line->flags & ML_SECRET)) return 2;
			}
		}
	}
	return 0;
}


//=============================================================================
//
// Polyobject debug stuff
//
//=============================================================================

void AM_drawSeg(seg_t *seg, const AMColor &color)
{
	mline_t l;
	l.a.x = seg->v1->fX();
	l.a.y = seg->v1->fY();
	l.b.x = seg->v2->fX();
	l.b.y = seg->v2->fY();

	if (am_rotate == 1 || (am_rotate == 2 && viewactive))
	{
		AM_rotatePoint (&l.a.x, &l.a.y);
		AM_rotatePoint (&l.b.x, &l.b.y);
	}
	AM_drawMline(&l, color);
}

void AM_drawPolySeg(FPolySeg *seg, const AMColor &color)
{
	mline_t l;
	l.a.x = seg->v1.pos.X;
	l.a.y = seg->v1.pos.Y;
	l.b.x = seg->v2.pos.X;
	l.b.y = seg->v2.pos.Y;

	if (am_rotate == 1 || (am_rotate == 2 && viewactive))
	{
		AM_rotatePoint (&l.a.x, &l.a.y);
		AM_rotatePoint (&l.b.x, &l.b.y);
	}
	AM_drawMline(&l, color);
}

void AM_showSS()
{
	if (am_showsubsector >= 0 && (unsigned)am_showsubsector < level.subsectors.Size())
	{
		AMColor yellow;
		yellow.FromRGB(255,255,0);
		AMColor red;
		red.FromRGB(255,0,0);

		subsector_t *sub = &level.subsectors[am_showsubsector];
		for (unsigned int i = 0; i < sub->numlines; i++)
		{
			AM_drawSeg(sub->firstline + i, yellow);
		}

		for (int i = 0; i <po_NumPolyobjs; i++)
		{
			FPolyObj *po = &polyobjs[i];
			FPolyNode *pnode = po->subsectorlinks;

			while (pnode != NULL)
			{
				if (pnode->subsector == sub)
				{
					for (unsigned j = 0; j < pnode->segs.Size(); j++)
					{
						AM_drawPolySeg(&pnode->segs[j], red);
					}
				}
				pnode = pnode->snext;
			}
		}
	}
}

//=============================================================================
//
// Determines if a 3D floor boundary should be drawn
//
//=============================================================================

bool AM_Check3DFloors(line_t *line)
{
	TArray<F3DFloor*> &ff_front = line->frontsector->e->XFloor.ffloors;
	TArray<F3DFloor*> &ff_back = line->backsector->e->XFloor.ffloors;

	// No 3D floors so there's no boundary
	if (ff_back.Size() == 0 && ff_front.Size() == 0) return false;

	int realfrontcount = 0;
	int realbackcount = 0;

	for(unsigned i=0;i<ff_front.Size();i++)
	{
		F3DFloor *rover = ff_front[i];
		if (rover->flags & FF_THISINSIDE) continue;
		if (!(rover->flags & FF_EXISTS)) continue;
		if (rover->alpha == 0) continue;
		realfrontcount++;
	}

	for(unsigned i=0;i<ff_back.Size();i++)
	{
		F3DFloor *rover = ff_back[i];
		if (rover->flags & FF_THISINSIDE) continue;
		if (!(rover->flags & FF_EXISTS)) continue;
		if (rover->alpha == 0) continue;
		realbackcount++;
	}
	// if the amount of 3D floors does not match there is a boundary
	if (realfrontcount != realbackcount) return true;

	for(unsigned i=0;i<ff_front.Size();i++)
	{
		F3DFloor *rover = ff_front[i];
		if (rover->flags & FF_THISINSIDE) continue;	// only relevant for software rendering.
		if (!(rover->flags & FF_EXISTS)) continue;
		if (rover->alpha == 0) continue;

		bool found = false;
		for(unsigned j=0;j<ff_back.Size();j++)
		{
			F3DFloor *rover2 = ff_back[j];
			if (rover2->flags & FF_THISINSIDE) continue;	// only relevant for software rendering.
			if (!(rover2->flags & FF_EXISTS)) continue;
			if (rover2->alpha == 0) continue;
			if (rover->model == rover2->model && rover->flags == rover2->flags) 
			{
				found = true;
				break;
			}
		}
		// At least one 3D floor in the front sector didn't have a match in the back sector so there is a boundary.
		if (!found) return true;
	}
	// All 3D floors could be matched so let's not draw a boundary.
	return false;
}

// [TP] Check whether a sector can trigger a special that satisfies the provided function.
// If found, specialptr and argsptr will be filled by the special and the arguments
// If needUseActivated is true, the special must be activated by use.
bool AM_checkSectorActions (sector_t *sector, bool (*function)(int, int *), int *specialptr, int **argsptr, bool needUseActivated)
{
	// This code really stands in the way of a more generic and flexible implementation of sector actions because it makes far too many assumptions
	// about their internal workings. Well, it can't be helped. Let's just hope that nobody abuses the special and the health field in a way that breaks this.
	for (AActor* action = sector->SecActTarget; action; action = action->tracer)
	{
		if (((action->health & (SECSPAC_Use | SECSPAC_UseWall)) || false == needUseActivated)
			&& (*function)(action->special, action->args)
			&& !(action->flags & MF_FRIENDLY))
		{
			*specialptr = action->special;
			*argsptr = action->args;
			return true;
		}
	}

	return false;
}

// [TP] Check whether there's a boundary on the provided line for a special that satisfies the provided function.
// It's a boundary if the line can activate the special or the line's bordering sectors can activate it.
// If found, specialptr and argsptr will be filled with special and args if given.
bool AM_checkSpecialBoundary (line_t &line, bool (*function)(int, int *), int *specialptr = NULL, int **argsptr = NULL)
{
	if (specialptr == NULL)
	{
		static int sink;
		specialptr = &sink;
	}

	if (argsptr == NULL)
	{
		static int *sink;
		argsptr = &sink;
	}

	// Check if the line special qualifies for this
	if ((line.activation & SPAC_PlayerActivate) && (*function)(line.special, line.args))
	{
		*specialptr = line.special;
		*argsptr = line.args;
		return true;
	}

	// Check sector actions in the line's front sector -- the action has to be use-activated in order to
	// show up if this is a one-sided line, because the player cannot trigger sector actions by crossing
	// a one-sided line (since that's impossible, duh).
	if (AM_checkSectorActions(line.frontsector, function, specialptr, argsptr, line.backsector == NULL))
		return true;

	// If it has a back sector, check sector actions in that.
	return (line.backsector && AM_checkSectorActions(line.backsector, function, specialptr, argsptr, false));
}

bool AM_isTeleportBoundary (line_t &line)
{
	return AM_checkSpecialBoundary(line, [](int special, int *)
	{
		return (special == Teleport ||
			special == Teleport_NoFog ||
			special == Teleport_ZombieChanger ||
			special == Teleport_Line);
	});
}

bool AM_isExitBoundary (line_t& line)
{
	return AM_checkSpecialBoundary(line, [](int special, int *)
	{
		return (special == Teleport_NewMap ||
			special == Teleport_EndGame ||
			special == Exit_Normal ||
			special == Exit_Secret);
	});
}

bool AM_isTriggerBoundary (line_t &line)
{
	return am_showtriggerlines == 1? AM_checkSpecialBoundary(line, [](int special, int *)
	{
		FLineSpecial *spec = P_GetLineSpecialInfo(special);
		return spec != NULL
			&& spec->max_args >= 0
			&& special != Door_Open
			&& special != Door_Close
			&& special != Door_CloseWaitOpen
			&& special != Door_Raise
			&& special != Door_Animated
			&& special != Generic_Door;
	}) : AM_checkSpecialBoundary(line, [](int special, int *)
	{
		FLineSpecial *spec = P_GetLineSpecialInfo(special);
		return spec != NULL
			&& spec->max_args >= 0;
	});
}

bool AM_isLockBoundary (line_t &line, int *lockptr = NULL)
{
	if (lockptr == NULL)
	{
		static int sink;
		lockptr = &sink;
	}

	if (line.locknumber)
	{
		*lockptr = line.locknumber;
		return true;
	}

	int special;
	int *args;
	bool result = AM_checkSpecialBoundary(line, [](int special, int* args)
	{
		return special == Door_LockedRaise
			|| special == ACS_LockedExecute
			|| special == ACS_LockedExecuteDoor
			|| (special == Door_Animated && args[3] != 0)
			|| (special == Generic_Door && args[4] != 0)
			|| (special == FS_Execute && args[2] != 0);
	}, &special, &args);

	if (result)
	{
		switch (special)
		{
		case FS_Execute:
			*lockptr = args[2];
			break;

		case Door_Animated:
		case Door_LockedRaise:
			*lockptr = args[3];
			break;

		default:
			*lockptr = args[4];
			break;
		}
	}

	return result;
}

//=============================================================================
//
// Determines visible lines, draws them.
// This is LineDef based, not LineSeg based.
//
//=============================================================================

void AM_drawWalls (bool allmap)
{
	static mline_t l;
	int lock, color;

	int numportalgroups = am_portaloverlay ? level.Displacements.size : 0;

	for (int p = numportalgroups - 1; p >= -1; p--)
	{
		if (p == MapPortalGroup) continue;


		for (auto &line : level.lines)
		{
			int pg;
			
			if (line.sidedef[0]->Flags & WALLF_POLYOBJ)
			{
				// For polyobjects we must test the surrounding sector to get the proper group.
				pg = P_PointInSector(line.v1->fX() + line.Delta().X / 2, line.v1->fY() + line.Delta().Y / 2)->PortalGroup;
			}
			else
			{
				pg = line.frontsector->PortalGroup;
			}
			DVector2 offset;
			bool portalmode = numportalgroups > 0 &&  pg != MapPortalGroup;
			if (pg == p)
			{
				offset = level.Displacements.getOffset(pg, MapPortalGroup);
			}
			else if (p == -1 && (pg == MapPortalGroup || !am_portaloverlay))
			{
				offset = { 0, 0 };
			}
			else continue;

			l.a.x = (line.v1->fX() + offset.X);
			l.a.y = (line.v1->fY() + offset.Y);
			l.b.x = (line.v2->fX() + offset.X);
			l.b.y = (line.v2->fY() + offset.Y);

			if (am_rotate == 1 || (am_rotate == 2 && viewactive))
			{
				AM_rotatePoint(&l.a.x, &l.a.y);
				AM_rotatePoint(&l.b.x, &l.b.y);
			}

			if (am_cheat != 0 || (line.flags & ML_MAPPED))
			{
				if ((line.flags & ML_DONTDRAW) && (am_cheat == 0 || am_cheat >= 4))
				{
					if (!am_showallenabled || CheckCheatmode(false))
					{
						continue;
					}
				}

				if (line.automapstyle > AMLS_Default && line.automapstyle < AMLS_COUNT
					&& (am_cheat == 0 || am_cheat >= 4))
				{
					AM_drawMline(&l, AUTOMAP_LINE_COLORS[line.automapstyle]);
					continue;
				}

				if (portalmode)
				{
					AM_drawMline(&l, AMColors.PortalColor);
				}
				else if (AM_CheckSecret(&line) == 1)
				{
					// map secret sectors like Boom
					AM_drawMline(&l, AMColors.SecretSectorColor);
				}
				else if (AM_CheckSecret(&line) == 2)
				{
					AM_drawMline(&l, AMColors.UnexploredSecretColor);
				}
				else if (line.flags & ML_SECRET)
				{ // secret door
					if (am_cheat != 0 && line.backsector != NULL)
						AM_drawMline(&l, AMColors.SecretWallColor);
					else
						AM_drawMline(&l, AMColors.WallColor);
				}
				else if (AM_isTeleportBoundary(line) && AMColors.isValid(AMColors.IntraTeleportColor))
				{ // intra-level teleporters
					AM_drawMline(&l, AMColors.IntraTeleportColor);
				}
				else if (AM_isExitBoundary(line) && AMColors.isValid(AMColors.InterTeleportColor))
				{ // inter-level/game-ending teleporters
					AM_drawMline(&l, AMColors.InterTeleportColor);
				}
				else if (AM_isLockBoundary(line, &lock))
				{
					if (AMColors.displayLocks)
					{
						color = P_GetMapColorForLock(lock);

						AMColor c;

						if (color >= 0)	c.FromRGB(RPART(color), GPART(color), BPART(color));
						else c = AMColors[AMColors.LockedColor];

						AM_drawMline(&l, c);
					}
					else
					{
						AM_drawMline(&l, AMColors.LockedColor);  // locked special
					}
				}
				else if (am_showtriggerlines
					&& AMColors.isValid(AMColors.SpecialWallColor)
					&& AM_isTriggerBoundary(line))
				{
					AM_drawMline(&l, AMColors.SpecialWallColor);	// wall with special non-door action the player can do
				}
				else if (line.backsector == NULL)
				{
					AM_drawMline(&l, AMColors.WallColor);	// one-sided wall
				}
				else if (line.backsector->floorplane
					!= line.frontsector->floorplane)
				{
					AM_drawMline(&l, AMColors.FDWallColor); // floor level change
				}
				else if (line.backsector->ceilingplane
					!= line.frontsector->ceilingplane)
				{
					AM_drawMline(&l, AMColors.CDWallColor); // ceiling level change
				}
				else if (AM_Check3DFloors(&line))
				{
					AM_drawMline(&l, AMColors.EFWallColor); // Extra floor border
				}
				else if (am_cheat > 0 && am_cheat < 4)
				{
					AM_drawMline(&l, AMColors.TSWallColor);
				}
			}
			else if (allmap || (line.flags & ML_REVEALED))
			{
				if ((line.flags & ML_DONTDRAW) && (am_cheat == 0 || am_cheat >= 4))
				{
					if (!am_showallenabled || CheckCheatmode(false))
					{
						continue;
					}
				}
				AM_drawMline(&l, AMColors.NotSeenColor);
			}
		}
	}
}


//=============================================================================
//
// Rotation in 2D.
// Used to rotate player arrow line character.
//
//=============================================================================

void AM_rotate(double *xp, double *yp, DAngle a)
{
	static DAngle angle_saved = 0.;
	static double sinrot = 0;
	static double cosrot = 1;

	if (angle_saved != a)
	{
		angle_saved = a;
		sinrot = sin(a.Radians());
		cosrot = cos(a.Radians());
	}

	double x = *xp;
	double y = *yp;
	double tmpx = (x * cosrot) - (y * sinrot);
	y = (x * sinrot) + (y * cosrot);
	x = tmpx;
	*xp = x;
	*yp = y;
}

//=============================================================================
//
//
//
//=============================================================================

void AM_rotatePoint (double *x, double *y)
{
	double pivotx = m_x + m_w/2;
	double pivoty = m_y + m_h/2;
	*x -= pivotx;
	*y -= pivoty;
	AM_rotate (x, y, -players[consoleplayer].camera->Angles.Yaw + 90.);
	*x += pivotx;
	*y += pivoty;
}

//=============================================================================
//
//
//
//=============================================================================

void
AM_drawLineCharacter
( const mline_t *lineguy,
  int		lineguylines,
  double	scale,
  DAngle	angle,
  const AMColor &color,
  double	x,
  double	y )
{
	int		i;
	mline_t	l;

	for (i=0;i<lineguylines;i++) {
		l.a.x = lineguy[i].a.x;
		l.a.y = lineguy[i].a.y;

		if (scale) {
			l.a.x *= scale;
			l.a.y *= scale;
		}

		if (angle != 0)
			AM_rotate(&l.a.x, &l.a.y, angle);

		l.a.x += x;
		l.a.y += y;

		l.b.x = lineguy[i].b.x;
		l.b.y = lineguy[i].b.y;

		if (scale) {
			l.b.x *= scale;
			l.b.y *= scale;
		}

		if (angle != 0)
			AM_rotate(&l.b.x, &l.b.y, angle);

		l.b.x += x;
		l.b.y += y;

		AM_drawMline(&l, color);
	}
}

//=============================================================================
//
//
//
//=============================================================================

void AM_drawPlayers ()
{
	if (am_cheat >= 2 && am_cheat != 4 && am_showthingsprites > 0)
	{
		// Player sprites are drawn with the others
		return;
	}

	mpoint_t pt;
	DAngle angle;
	int i;

	if (!multiplayer)
	{
		mline_t *arrow;
		int numarrowlines;

		double vh = players[consoleplayer].viewheight;
		DVector2 pos = am_portaloverlay? players[consoleplayer].camera->GetPortalTransition(vh) : players[consoleplayer].camera->Pos();
		pt.x = pos.X;
		pt.y = pos.Y;
		if (am_rotate == 1 || (am_rotate == 2 && viewactive))
		{
			angle = 90.;
			AM_rotatePoint (&pt.x, &pt.y);
		}
		else
		{
			angle = players[consoleplayer].camera->Angles.Yaw;
		}
		
		if (am_cheat != 0 && CheatMapArrow.Size() > 0)
		{
			arrow = &CheatMapArrow[0];
			numarrowlines = CheatMapArrow.Size();
		}
		else
		{
			arrow = &MapArrow[0];
			numarrowlines = MapArrow.Size();
		}
		AM_drawLineCharacter(arrow, numarrowlines, 0, angle, AMColors[AMColors.YourColor], pt.x, pt.y);
		return;
	}

	for (i = 0; i < MAXPLAYERS; i++)
	{
		player_t *p = &players[i];
		AMColor color;

		if (!playeringame[i] || p->mo == NULL)
		{
			continue;
		}

		// We don't always want to show allies on the automap.
		if (dmflags2 & DF2_NO_AUTOMAP_ALLIES && i != consoleplayer)
			continue;
		
		if (deathmatch && !demoplayback &&
			!p->mo->IsTeammate (players[consoleplayer].mo) &&
			p != players[consoleplayer].camera->player)
		{
			continue;
		}

		if (p->mo->Alpha < 1.)
		{
			color = AMColors[AMColors.AlmostBackgroundColor];
		}
		else
		{
			float h, s, v, r, g, b;

			D_GetPlayerColor (i, &h, &s, &v, NULL);
			HSVtoRGB (&r, &g, &b, h, s, v);

			color.FromRGB(clamp (int(r*255.f),0,255), clamp (int(g*255.f),0,255), clamp (int(b*255.f),0,255));
		}

		if (p->mo != NULL)
		{
			DVector3 pos = p->mo->PosRelative(MapPortalGroup);
			pt.x = pos.X;
			pt.y = pos.Y;

			angle = p->mo->Angles.Yaw;

			if (am_rotate == 1 || (am_rotate == 2 && viewactive))
			{
				AM_rotatePoint (&pt.x, &pt.y);
				angle -= players[consoleplayer].camera->Angles.Yaw - 90.;
			}

			AM_drawLineCharacter(&MapArrow[0], MapArrow.Size(), 0, angle, color, pt.x, pt.y);
		}
    }
}

//=============================================================================
//
//
//
//=============================================================================

void AM_drawKeys ()
{
	AMColor color;
	mpoint_t p;
	DAngle	 angle;

	TThinkerIterator<AInventory> it(NAME_Key);
	AInventory *key;

	while ((key = it.Next()) != NULL)
	{
		DVector3 pos = key->PosRelative(MapPortalGroup);
		p.x = pos.X;
		p.y = pos.Y;

		angle = key->Angles.Yaw;

		if (am_rotate == 1 || (am_rotate == 2 && viewactive))
		{
			AM_rotatePoint (&p.x, &p.y);
			angle += -players[consoleplayer].camera->Angles.Yaw + 90.;
		}

		if (key->flags & MF_SPECIAL)
		{
			// Find the key's own color.
			// Only works correctly if single-key locks have lower numbers than any-key locks.
			// That is the case for all default keys, however.
			int P_GetMapColorForKey (AInventory * key);
			int c = P_GetMapColorForKey(key);

			if (c >= 0)	color.FromRGB(RPART(c), GPART(c), BPART(c));
			else color = AMColors[AMColors.ThingColor_CountItem];
			AM_drawLineCharacter(&EasyKey[0], EasyKey.Size(), 0, 0., color, p.x, p.y);
		}
	}
}

//=============================================================================
//
//
//
//=============================================================================
void AM_drawThings ()
{
	AMColor color;
	AActor*	 t;
	mpoint_t p;
	DAngle	 angle;

	for (auto &sec : level.sectors)
	{
		t = sec.thinglist;
		while (t)
		{
			if (am_cheat > 0 || !(t->flags6 & MF6_NOTONAUTOMAP))
			{
				DVector3 pos = t->PosRelative(MapPortalGroup);
				p.x = pos.X;
				p.y = pos.Y;

				if (am_showthingsprites > 0 && t->sprite > 0)
				{
					FTexture *texture = NULL;
					spriteframe_t *frame;
					int rotation = 0;

					// try all modes backwards until a valid texture has been found.	
					for(int show = am_showthingsprites; show > 0 && texture == NULL; show--)
					{
						const spritedef_t& sprite = sprites[t->sprite];
						const size_t spriteIndex = sprite.spriteframes + (show > 1 ? t->frame : 0);

						frame = &SpriteFrames[spriteIndex];
						DAngle angle = 270. + 22.5 - t->Angles.Yaw;
						if (frame->Texture[0] != frame->Texture[1]) angle += 180. / 16;
						if (am_rotate == 1 || (am_rotate == 2 && viewactive))
						{
							angle += players[consoleplayer].camera->Angles.Yaw - 90.;
						}
						rotation = int((angle.Normalized360() * (16. / 360.)).Degrees);

						const FTextureID textureID = frame->Texture[show > 2 ? rotation : 0];
						texture = TexMan(textureID);
					}

					if (texture == NULL) goto drawTriangle;	// fall back to standard display if no sprite can be found.

					const double spriteXScale = (t->Scale.X * (10. / 16.) * scale_mtof);
					const double spriteYScale = (t->Scale.Y * (10. / 16.) * scale_mtof);

					DrawMarker (texture, p.x, p.y, 0, !!(frame->Flip & (1 << rotation)),
						spriteXScale, spriteYScale, t->Translation, 1., 0, LegacyRenderStyles[STYLE_Normal]);
				}
				else
				{
			drawTriangle:
					angle = t->Angles.Yaw;

					if (am_rotate == 1 || (am_rotate == 2 && viewactive))
					{
						AM_rotatePoint (&p.x, &p.y);
						angle += -players[consoleplayer].camera->Angles.Yaw + 90.;
					}

					color = AMColors[AMColors.ThingColor];

					// use separate colors for special thing types
					if (t->flags3&MF3_ISMONSTER && !(t->flags&MF_CORPSE))
					{
						if (t->flags & MF_FRIENDLY) color = AMColors[AMColors.ThingColor_Friend];
						else if (!(t->flags & MF_COUNTKILL)) color = AMColors[AMColors.ThingColor_NocountMonster];
						else color = AMColors[AMColors.ThingColor_Monster];
					}
					else if (t->flags&MF_SPECIAL)
					{
						// Find the key's own color.
						// Only works correctly if single-key locks have lower numbers than any-key locks.
						// That is the case for all default keys, however.
						if (t->IsKindOf(NAME_Key))
						{
							if (G_SkillProperty(SKILLP_EasyKey))
							{
								// Already drawn by AM_drawKeys(), so don't draw again
								color.Index = -1;
							}
							else if (am_showkeys)
							{
								int P_GetMapColorForKey (AInventory * key);
								int c = P_GetMapColorForKey(static_cast<AInventory *>(t));

								if (c >= 0)	color.FromRGB(RPART(c), GPART(c), BPART(c));
								else color = AMColors[AMColors.ThingColor_CountItem];
								AM_drawLineCharacter(&CheatKey[0], CheatKey.Size(), 0, 0., color, p.x, p.y);
								color.Index = -1;
							}
							else
							{
								color = AMColors[AMColors.ThingColor_Item];
							}
						}
						else if (t->flags&MF_COUNTITEM)
							color = AMColors[AMColors.ThingColor_CountItem];
						else
							color = AMColors[AMColors.ThingColor_Item];
					}

					if (color.Index != -1)
					{
						AM_drawLineCharacter(thintriangle_guy, NUMTHINTRIANGLEGUYLINES,	16, angle, color, p.x, p.y);
					}

					if (am_cheat == 3 || am_cheat == 6)
					{
						static const mline_t box[4] =
						{
							{ { -1, -1 }, {  1, -1 } },
							{ {  1, -1 }, {  1,  1 } },
							{ {  1,  1 }, { -1,  1 } },
							{ { -1,  1 }, { -1, -1 } },
						};

						AM_drawLineCharacter (box, 4, t->radius, angle - t->Angles.Yaw, color, p.x, p.y);
					}
				}
			}
			t = t->snext;
		}
	}
}

//=============================================================================
//
//
//
//=============================================================================

static void DrawMarker (FTexture *tex, double x, double y, int yadjust,
	INTBOOL flip, double xscale, double yscale, int translation, double alpha, uint32_t fillcolor, FRenderStyle renderstyle)
{
	if (tex == NULL || tex->UseType == ETextureType::Null)
	{
		return;
	}
	if (am_rotate == 1 || (am_rotate == 2 && viewactive))
	{
		AM_rotatePoint (&x, &y);
	}
	screen->DrawTexture (tex, CXMTOF(x) + f_x, CYMTOF(y) + yadjust + f_y,
		DTA_DestWidthF, tex->GetScaledWidthDouble() * CleanXfac * xscale,
		DTA_DestHeightF, tex->GetScaledHeightDouble() * CleanYfac * yscale,
		DTA_ClipTop, f_y,
		DTA_ClipBottom, f_y + f_h,
		DTA_ClipLeft, f_x,
		DTA_ClipRight, f_x + f_w,
		DTA_FlipX, flip,
		DTA_TranslationIndex, translation,
		DTA_Alpha, alpha,
		DTA_FillColor, fillcolor,
		DTA_RenderStyle, renderstyle.AsDWORD,
		TAG_DONE);
}

//=============================================================================
//
//
//
//=============================================================================

void AM_drawMarks ()
{
	for (int i = 0; i < AM_NUMMARKPOINTS; i++)
	{
		if (markpoints[i].x != -1)
		{
			DrawMarker (TexMan(marknums[i]), markpoints[i].x, markpoints[i].y, -3, 0,
				1, 1, 0, 1, 0, LegacyRenderStyles[STYLE_Normal]);
		}
	}
}

//=============================================================================
//
//
//
//=============================================================================

void AM_drawAuthorMarkers ()
{
	// [RH] Draw any actors derived from AMapMarker on the automap.
	// If args[0] is 0, then the actor's sprite is drawn at its own location.
	// Otherwise, its sprite is drawn at the location of any actors whose TIDs match args[0].
	TThinkerIterator<AActor> it ("MapMarker", STAT_MAPMARKER);
	AActor *mark;

	while ((mark = it.Next()) != NULL)
	{
		if (mark->flags2 & MF2_DORMANT)
		{
			continue;
		}

		FTextureID picnum;
		FTexture *tex;
		uint16_t flip = 0;

		if (mark->picnum.isValid())
		{
			tex = TexMan(mark->picnum);
			if (tex->Rotations != 0xFFFF)
			{
				spriteframe_t *sprframe = &SpriteFrames[tex->Rotations];
				picnum = sprframe->Texture[0];
				flip = sprframe->Flip & 1;
				tex = TexMan[picnum];
			}
		}
		else
		{
			spritedef_t *sprdef = &sprites[mark->sprite];
			if (mark->frame >= sprdef->numframes)
			{
				continue;
			}
			else
			{
				spriteframe_t *sprframe = &SpriteFrames[sprdef->spriteframes + mark->frame];
				picnum = sprframe->Texture[0];
				flip = sprframe->Flip & 1;
				tex = TexMan[picnum];
			}
		}
		FActorIterator it (mark->args[0]);
		AActor *marked = mark->args[0] == 0 ? mark : it.Next();

		while (marked != NULL)
		{
			// Use more correct info if we have GL nodes available
			if (mark->args[1] == 0 ||
				(mark->args[1] == 1 && (hasglnodes ?
				 marked->subsector->flags & SSECMF_DRAWN :
				 marked->Sector->MoreFlags & SECMF_DRAWN)))
			{
				DrawMarker (tex, marked->X(), marked->Y(), 0, flip, mark->Scale.X, mark->Scale.Y, mark->Translation,
					mark->Alpha, mark->fillcolor, mark->RenderStyle);
			}
			marked = mark->args[0] != 0 ? it.Next() : NULL;
		}
	}
}

//=============================================================================
//
//
//
//=============================================================================

void AM_drawCrosshair (const AMColor &color)
{
	screen->DrawPixel(f_w/2, (f_h+1)/2, color.Index, color.RGB);
}

//=============================================================================
//
//
//
//=============================================================================

void AM_Drawer (int bottom)
{
	if (!automapactive)
		return;

	bool allmap = (level.flags2 & LEVEL2_ALLMAP) != 0;
	bool allthings = allmap && players[consoleplayer].mo->FindInventory(NAME_PowerScanner, true) != nullptr;

	if (am_portaloverlay)
	{
		sector_t *sec;
		double vh = players[consoleplayer].viewheight;
		players[consoleplayer].camera->GetPortalTransition(vh, &sec);
		MapPortalGroup = sec->PortalGroup;
	}
	else MapPortalGroup = 0;
	AM_initColors (viewactive);

	if (!viewactive)
	{
		// [RH] Set f_? here now to handle automap overlaying
		// and view size adjustments.
		f_x = f_y = 0;
		f_w = screen->GetWidth ();
		f_h = bottom;

		AM_clearFB(AMColors[AMColors.Background]);
	}
	else 
	{
		f_x = viewwindowx;
		f_y = viewwindowy;
		f_w = viewwidth;
		f_h = viewheight;
	}
	AM_activateNewScale();

	if (am_textured && hasglnodes && textured && !viewactive)
		AM_drawSubsectors();

	if (grid)	
		AM_drawGrid(AMColors.GridColor);

	AM_drawWalls(allmap);
	AM_drawPlayers();
	if (G_SkillProperty(SKILLP_EasyKey))
		AM_drawKeys();
	if ((am_cheat >= 2 && am_cheat != 4) || allthings)
		AM_drawThings();

	AM_drawAuthorMarkers();

	if (!viewactive)
		AM_drawCrosshair(AMColors[AMColors.XHairColor]);

	AM_drawMarks();

	AM_showSS();
}

//=============================================================================
//
//
//
//=============================================================================

void AM_SerializeMarkers(FSerializer &arc)
{
	if (arc.BeginObject("automarkers"))
	{
		arc("markpointnum", markpointnum)
			.Array("markpoints", &markpoints[0].x, AM_NUMMARKPOINTS*2)	// write as a double array.
			("scale_mtof", scale_mtof)
			("scale_ftom", scale_ftom)
			.EndObject();
	}
}

// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//		Intermission screens.
//
//-----------------------------------------------------------------------------


#include <ctype.h>
#include <stdio.h>

#include "m_random.h"
#include "m_swap.h"
#include "i_system.h"
#include "w_wad.h"
#include "g_game.h"
#include "g_level.h"
#include "r_local.h"
#include "s_sound.h"
#include "doomstat.h"
#include "v_video.h"
#include "i_video.h"
#include "wi_stuff.h"
#include "c_console.h"
#include "hu_stuff.h"
#include "v_palette.h"
#include "s_sndseq.h"
#include "gi.h"

// States for the intermission
typedef enum
{
	NoState = -1,
	StatCount,
	ShowNextLoc,
	LeavingIntermission
} stateenum_t;

CVAR (Bool, wi_percents, true, CVAR_ARCHIVE)

void WI_loadData ();
void WI_unloadData ();

#define NEXTSTAGE		(gameinfo.gametype == GAME_Doom ? "weapons/rocklx" : "doors/dr1_clos")
#define PASTSTATS		(gameinfo.gametype == GAME_Doom ? "weapons/shotgr" : "plats/pt1_stop")
//
// Data needed to add patches to full screen intermission pics.
// Patches are statistics messages, and animations.
// Loads of by-pixel layout and placement, offsets etc.
//

#define NUMEPISODES 	6
#define NUMMAPS 		9

// GLOBAL LOCATIONS
#define WI_TITLEY				2
#define WI_SPACINGY 			33

// SINGPLE-PLAYER STUFF
#define SP_STATSX				50
#define SP_STATSY				50

#define SP_TIMEX				16
#define SP_TIMEY				(200-32)


// NET GAME STUFF
#define NG_STATSY				50
#define NG_STATSX				(32 + SHORT(star->width)/2 + 32*!dofrags)

#define NG_SPACINGX 			64


// DEATHMATCH STUFF
#define DM_MATRIXX				42
#define DM_MATRIXY				68

#define DM_SPACINGX 			40

#define DM_TOTALSX				269

#define DM_KILLERSX 			10
#define DM_KILLERSY 			100
#define DM_VICTIMSX 			5
#define DM_VICTIMSY 			50




typedef enum
{
	ANIM_ALWAYS,
	ANIM_LEVEL
} animenum_t;

typedef struct
{
	int x, y;
} yahpt_t;

typedef struct
{
	animenum_t	type;
	int 		period;	// period in tics between animations
	int 		nanims;	// number of animation frames
	yahpt_t 	loc;	// location of animation
	int 		data;	// ALWAYS: n/a, RANDOM: period deviation (<256)
	patch_t*	p[3];	// actual graphics for frames of animations

	// following must be initialized to zero before use!
	int 		nexttic;	// next value of bcnt (used in conjunction with period)
	int 		ctr;		// next frame number to animate
	int 		state;		// used by RANDOM and LEVEL when animating
} in_anim_t;

static yahpt_t lnodes[NUMEPISODES][NUMMAPS] =
{
//
// Doom 1 Episodes
//
	// Episode 1 World Map
	{
		{ 185, 164 },	// location of level 1 (CJ)
		{ 148, 143 },	// location of level 2 (CJ)
		{ 69, 122 },	// location of level 3 (CJ)
		{ 209, 102 },	// location of level 4 (CJ)
		{ 116, 89 },	// location of level 5 (CJ)
		{ 166, 55 },	// location of level 6 (CJ)
		{ 71, 56 }, 	// location of level 7 (CJ)
		{ 135, 29 },	// location of level 8 (CJ)
		{ 71, 24 }		// location of level 9 (CJ)
	},

	// Episode 2 World Map
	{
		{ 254, 25 },	// location of level 1 (CJ)
		{ 97, 50 }, 	// location of level 2 (CJ)
		{ 188, 64 },	// location of level 3 (CJ)
		{ 128, 78 },	// location of level 4 (CJ)
		{ 214, 92 },	// location of level 5 (CJ)
		{ 133, 130 },	// location of level 6 (CJ)
		{ 208, 136 },	// location of level 7 (CJ)
		{ 148, 140 },	// location of level 8 (CJ)
		{ 235, 158 }	// location of level 9 (CJ)
	},

	// Episode 3 World Map
	{
		{ 156, 168 },	// location of level 1 (CJ)
		{ 48, 154 },	// location of level 2 (CJ)
		{ 174, 95 },	// location of level 3 (CJ)
		{ 265, 75 },	// location of level 4 (CJ)
		{ 130, 48 },	// location of level 5 (CJ)
		{ 279, 23 },	// location of level 6 (CJ)
		{ 198, 48 },	// location of level 7 (CJ)
		{ 140, 25 },	// location of level 8 (CJ)
		{ 281, 136 }	// location of level 9 (CJ)
	},
//
// Heretic Episodes
//
	{
		{ 172, 78 },
		{ 86, 90 },
		{ 73, 66 },
		{ 159, 95 },
		{ 148, 126 },
		{ 132, 54 },
		{ 131, 74 },
		{ 208, 138 },
		{ 52, 101 }
	},
	{
		{ 218, 57 },
		{ 137, 81 },
		{ 155, 124 },
		{ 171, 68 },
		{ 250, 86 },
		{ 136, 98 },
		{ 203, 90 },
		{ 220, 140 },
		{ 279, 106 }
	},
	{
		{ 86, 99 },
		{ 124, 103 },
		{ 154, 79 },
		{ 202, 83 },
		{ 178, 59 },
		{ 142, 58 },
		{ 219, 66 },
		{ 247, 57 },
		{ 107, 80 }
	}
};


//
// Animation locations for episode 0 (1).
// Using patches saves a lot of space,
//	as they replace 320x200 full screen frames.
//
static in_anim_t epsd0animinfo[] =
{
	{ ANIM_ALWAYS, TICRATE/3, 3, { 224, 104 } },
	{ ANIM_ALWAYS, TICRATE/3, 3, { 184, 160 } },
	{ ANIM_ALWAYS, TICRATE/3, 3, { 112, 136 } },
	{ ANIM_ALWAYS, TICRATE/3, 3, { 72, 112 } },
	{ ANIM_ALWAYS, TICRATE/3, 3, { 88, 96 } },
	{ ANIM_ALWAYS, TICRATE/3, 3, { 64, 48 } },
	{ ANIM_ALWAYS, TICRATE/3, 3, { 192, 40 } },
	{ ANIM_ALWAYS, TICRATE/3, 3, { 136, 16 } },
	{ ANIM_ALWAYS, TICRATE/3, 3, { 80, 16 } },
	{ ANIM_ALWAYS, TICRATE/3, 3, { 64, 24 } }
};

static in_anim_t epsd1animinfo[] =
{
	{ ANIM_LEVEL, TICRATE/3, 1, { 128, 136 }, 1 },
	{ ANIM_LEVEL, TICRATE/3, 1, { 128, 136 }, 2 },
	{ ANIM_LEVEL, TICRATE/3, 1, { 128, 136 }, 3 },
	{ ANIM_LEVEL, TICRATE/3, 1, { 128, 136 }, 4 },
	{ ANIM_LEVEL, TICRATE/3, 1, { 128, 136 }, 5 },
	{ ANIM_LEVEL, TICRATE/3, 1, { 128, 136 }, 6 },
	{ ANIM_LEVEL, TICRATE/3, 1, { 128, 136 }, 7 },
	{ ANIM_LEVEL, TICRATE/3, 3, { 192, 144 }, 8 },
	{ ANIM_LEVEL, TICRATE/3, 1, { 128, 136 }, 8 }
};

static in_anim_t epsd2animinfo[] =
{
	{ ANIM_ALWAYS, TICRATE/3, 3, { 104, 168 } },
	{ ANIM_ALWAYS, TICRATE/3, 3, { 40, 136 } },
	{ ANIM_ALWAYS, TICRATE/3, 3, { 160, 96 } },
	{ ANIM_ALWAYS, TICRATE/3, 3, { 104, 80 } },
	{ ANIM_ALWAYS, TICRATE/3, 3, { 120, 32 } },
	{ ANIM_ALWAYS, TICRATE/4, 3, { 40, 0 } }
};

static int NUMANIMS[NUMEPISODES] =
{
	sizeof(epsd0animinfo)/sizeof(in_anim_t),
	sizeof(epsd1animinfo)/sizeof(in_anim_t),
	sizeof(epsd2animinfo)/sizeof(in_anim_t)
};

static in_anim_t *anims[NUMEPISODES] =
{
	epsd0animinfo,
	epsd1animinfo,
	epsd2animinfo
};

// [RH] Map name -> index mapping
static char names[NUMEPISODES][NUMMAPS][8] =
{
	{ "E1M1", "E1M2", "E1M3", "E1M4", "E1M5", "E1M6", "E1M7", "E1M8", "E1M9" },
	{ "E2M1", "E2M2", "E2M3", "E2M4", "E2M5", "E2M6", "E2M7", "E2M8", "E2M9" },
	{ "E3M1", "E3M2", "E3M3", "E3M4", "E3M5", "E3M6", "E3M7", "E3M8", "E3M9" }
};

//
// GENERAL DATA
//

//
// Locally used stuff.
//
#define FB (screen)


// States for single-player
#define SP_KILLS				0
#define SP_ITEMS				2
#define SP_SECRET				4
#define SP_FRAGS				6 
#define SP_TIME 				8 
#define SP_PAR					ST_TIME

#define SP_PAUSE				1

#define SHOWNEXTLOCDELAY		4			// in seconds

static int				acceleratestage;	// used to accelerate or skip a stage
static int				me;					// wbs->pnum
static stateenum_t		state;				// specifies current state
static wbstartstruct_t *wbs;				// contains information passed into intermission
static wbplayerstruct_t*plrs;				// wbs->plyr[]
static int				cnt;				// used for general timing
static int				bcnt;				// used for timing of background animation
static int				epsd;				// episode currently displayed

static int				cnt_kills[MAXPLAYERS];
static int				cnt_items[MAXPLAYERS];
static int				cnt_secret[MAXPLAYERS];
static int				cnt_time;
static int				cnt_par;
static int				cnt_pause;

//
//		GRAPHICS
//

static patch_t* 		yah[2];		// You Are Here graphic
static patch_t* 		splat;		// splat
static patch_t* 		percent;	// %, : graphics
static patch_t* 		colon;
static patch_t*			slash;
static patch_t* 		num[10];	// 0-9 graphic
static patch_t* 		wiminus;	// minus sign
static patch_t* 		finished;	// "Finished!" graphics
static patch_t* 		entering;	// "Entering" graphic
static patch_t* 		sp_secret;	// "secret"
static patch_t* 		kills;		// "Kills", "Scrt", "Items", "Frags"
static patch_t* 		secret;
static patch_t* 		items;
static patch_t* 		frags;
static patch_t* 		timepic;	// Time sucks.
static patch_t* 		par;
static patch_t* 		sucks;
static patch_t* 		killers;	// "killers", "victims"
static patch_t* 		victims;
static patch_t* 		total;		// "Total", your face, your dead face
static patch_t* 		star;
static patch_t* 		bstar;
static patch_t* 		p;			// Player graphic
static patch_t*			lnames[2];	// Name graphics of each level (centered)

// [RH] Info to dynamically generate the level name graphics
static int				lnamewidths[2];
static char				*lnametexts[2];

static DCanvas			*background;

//
// CODE
//

void WI_slamBackground ()
{
	if (background)
	{
		background->Blit (0, 0, background->GetWidth(), background->GetHeight(),
			FB, 0, 0, SCREENWIDTH, SCREENHEIGHT);
	}
	else if (state != NoState)
	{
		int lump = W_CheckNumForName ("FLOOR16", ns_flats);
		if (lump >= 0)
		{
			const byte *flat = (byte *)W_MapLumpNum (lump);
			FB->FlatFill (0, 0, SCREENWIDTH, SCREENHEIGHT, flat);
			W_UnMapLump (flat);
		}
		else
		{
			FB->Clear (0, 0, SCREENWIDTH, SCREENHEIGHT, 0);
		}
	}
}

static void WI_DrawCharPatch (patch_t *patch, int x, int y)
{
	if (gameinfo.gametype == GAME_Doom)
	{
		FB->DrawPatchClean (patch, x, y);
	}
	else
	{
		FB->DrawShadowedPatchClean (patch, x, y);
	}
}

static int WI_DrawName (char *str, int x, int y, bool nomove=false)
{
	screen->SetFont (BigFont);
	if (nomove)
	{
		x = (SCREENWIDTH - x*CleanXfac) / 2;
		if (gameinfo.gametype == GAME_Doom)
		{
			screen->DrawTextClean (CR_UNTRANSLATED, x, y, str);
		}
		else
		{
			screen->DrawTextCleanShadow (CR_UNTRANSLATED, x, y, str);
		}
	}
	else
	{
		x = 160 - x/2;
		if (gameinfo.gametype == GAME_Doom)
		{
			screen->DrawTextCleanMove (CR_UNTRANSLATED, x, y, str);
		}
		else
		{
			screen->DrawTextCleanShadowMove (CR_UNTRANSLATED, x, y, str);
		}
	}
	screen->SetFont (SmallFont);
	return BigFont->GetHeight()*5/4;
}

static int WI_CalcWidth (char *str)
{
	int w;

	if (!str)
		return 0;

	screen->SetFont (BigFont);
	w = screen->StringWidth (str);
	screen->SetFont (SmallFont);

	return w;
}

// Draws "<Levelname> Finished!"
void WI_drawLF ()
{
	if (gameinfo.gametype == GAME_Doom)
	{
		int y;

		if (!lnames[0] && !lnamewidths[0])
			return;

		y = WI_TITLEY;

		if (lnames[0])
		{ // draw <LevelName> 
			FB->DrawPatchClean (lnames[0], (320 - SHORT(lnames[0]->width))/2, y);
			y += (5*SHORT(lnames[0]->height))/4;
		}
		else
		{ // [RH] draw a dynamic title string
			y += WI_DrawName (lnametexts[0], lnamewidths[0], y);
		}

		// draw "Finished!"
		FB->DrawPatchClean (finished, (320 - SHORT(finished->width))/2, y);
	}
	else
	{
		WI_DrawName (lnametexts[0], lnamewidths[0], 3);
		screen->DrawTextCleanMove (CR_UNTRANSLATED,
			160 - screen->StringWidth ("FINISHED")/2, 25, "FINISHED");
	}
}

// Draws "Entering <LevelName>"
void WI_drawEL ()
{
	if (gameinfo.gametype == GAME_Doom)
	{
		int y = WI_TITLEY;

		if (!lnames[1] && !lnamewidths[1])
			return;

		y = WI_TITLEY;

		// draw "Entering"
		FB->DrawPatchCleanNoMove (entering, (SCREENWIDTH - SHORT(entering->width)*CleanXfac)/2, y);

		// [RH] Changed to adjust by height of entering patch instead of title
		y += (5*SHORT(entering->height))/4*CleanYfac;

		if (lnames[1])
		{ // draw level
			FB->DrawPatchCleanNoMove (lnames[1], (SCREENWIDTH - SHORT(lnames[1]->width)*CleanXfac)/2, y);
		}
		else
		{ // [RH] draw a dynamic title string
			WI_DrawName (lnametexts[1], lnamewidths[1], y, true);
		}
	}
	else
	{
		screen->DrawTextClean (CR_UNTRANSLATED,
			(SCREENWIDTH - screen->StringWidth ("NOW ENTERING:") * CleanXfac)/2, 10, "NOW ENTERING:");
		WI_DrawName (lnametexts[1], lnamewidths[1], 10+10*CleanYfac, true);
	}
}

int WI_MapToIndex (char *map, int ep)
{
	int i;

	if (gameinfo.gametype == GAME_Heretic)
	{
		ep -= 10;
	}

	for (i = 0; i < NUMMAPS; i++)
	{
		if (!strnicmp (names[ep][i], map, 8))
			break;
	}
	return i;
}

void WI_drawOnLnode (int n, patch_t *c[], int episode)
{

	int 	i;
	int 	left;
	int 	top;
	int 	right;
	int 	bottom;
	BOOL 	fits = false;

	if (gameinfo.gametype == GAME_Heretic)
	{
		episode -= 7;
	}
	i = 0;
	do
	{
		left = lnodes[episode][n].x - SHORT(c[i]->leftoffset);
		top = lnodes[episode][n].y - SHORT(c[i]->topoffset);
		right = left + SHORT(c[i]->width);
		bottom = top + SHORT(c[i]->height);

		if (left >= 0 && right < 320 && top >= 0 && bottom < 200)
		{
			fits = true;
		}
		else
		{
			i++;
		}
	} while (!fits && i != 2);

	if (fits && i<2)
	{
		FB->DrawPatchIndirect (c[i], lnodes[episode][n].x, lnodes[episode][n].y);
	}
	else
	{ // DEBUG
		DPrintf ("Could not place patch on level %d", n+1); 
	}
}

void WI_initAnimatedBack ()
{
	int i;
	in_anim_t *a;

	if (gamemode == commercial)
	{
		return;
	}

	if (gameinfo.gametype == GAME_Doom)
	{
		if (epsd > 2)
		{
			return;
		}
		for (i = 0; i < NUMANIMS[epsd]; i++)
		{
			a = &anims[epsd][i];

			// init variables
			a->ctr = -1;

			// specify the next time to draw it
			if (a->type == ANIM_ALWAYS)
				a->nexttic = bcnt + 1 + (M_Random()%a->period);
			else if (a->type == ANIM_LEVEL)
				a->nexttic = bcnt + 1;
		}
	}
	else if (state == ShowNextLoc)
	{
		char name[9];
		const patch_t *bg;
		int lump;

		sprintf (name, "MAPE%d", epsd - 9);
		lump = W_CheckNumForName (name);
		if (lump >= 0)
		{
			bg = (patch_t *)W_MapLumpNum (lump);
			background = I_NewStaticCanvas (SHORT(bg->width), SHORT(bg->height));
			background->Lock ();
			background->DrawPatch (bg, 0, 0);
			background->Unlock ();
			W_UnMapLump (bg);
		}
	}
}

void WI_DrawDoomBack ()
{
	char name[9];
	const patch_t *bg;

	if ((gamemode == commercial) ||
		(gamemode == retail && epsd >= 3))
		strcpy (name, "INTERPIC");
	else 
		sprintf (name, "WIMAP%d", epsd);

	// background
	bg = (patch_t *)W_MapLumpName (name);
	if (background != NULL &&
		(SHORT(bg->width) != background->GetWidth() ||
		 SHORT(bg->height) != background->GetHeight()))
	{
		delete background;
		background = NULL;
	}
	if (background == NULL)
	{
		background = I_NewStaticCanvas (SHORT(bg->width), SHORT(bg->height));
	}
	background->Lock ();
	background->DrawPatch (bg, 0, 0);
	if (FindLevelInfo ("E2M9")->flags & LEVEL_VISITED && epsd == 1)
	{ // Add the Fortress of Mystery if it has been visited
		background->DrawPatch (anims[1][7].p[2], anims[1][7].loc.x, anims[1][7].loc.y);
		// anims[1][7].data = 800;	// Don't animate it again if the user uses changemap E2M9
	}
	else
	{
		anims[1][7].data = 8;
	}
	background->Unlock ();
	W_UnMapLump (bg);
}

void WI_updateAnimatedBack (void)
{
	int i;
	in_anim_t *a;

	if (gameinfo.gametype != GAME_Doom ||
		gamemode == commercial ||
		epsd > 2)
	{
		return;
	}

	for (i = 0; i < NUMANIMS[epsd]; i++)
	{
		a = &anims[epsd][i];

		if (bcnt == a->nexttic)
		{
			switch (a->type)
			{
			case ANIM_ALWAYS:
				if (++a->ctr >= a->nanims)
					a->ctr = 0;
				a->nexttic = bcnt + a->period;
				break;

			case ANIM_LEVEL:
				// gawd-awful hack for level anims

				if (!(state == StatCount && i == 7)
					&& WI_MapToIndex (wbs->next, wbs->next_ep) == a->data)
				{
					a->ctr++;
					if (a->ctr == a->nanims)
						a->ctr--;
					a->nexttic = bcnt + a->period;
				}

				break;
			}
		}

	}
}

void WI_drawAnimatedBack (void)
{
	int i;
	in_anim_t *a;

	if (gameinfo.gametype == GAME_Doom &&
		gamemode != commercial &&
		epsd <= 2)
	{
		background->Lock ();
		for (i = 0; i < NUMANIMS[epsd]; i++)
		{
			a = &anims[epsd][i];

			if (a->ctr >= 0)
				background->DrawPatch (a->p[a->ctr], a->loc.x, a->loc.y);
		}
		background->Unlock ();
	}

	WI_slamBackground ();
}

//
// Draws a number.
// If digits > 0, then use that many digits minimum,
//	otherwise only use as many as necessary.
// Returns new x position.
//

int WI_drawNum (int x, int y, int n, int digits, bool leadingzeros = true)
{
	// if non-number, do not draw it
	if (n == 1994)
		return 0;

	int fontwidth = SHORT(num[3]->width);
	int xofs;
	char text[8];
	char *text_p;

	if (leadingzeros)
	{
		sprintf (text, "%07d", n);
	}
	else
	{
		sprintf (text, "%7d", n);
		if (digits < 0)
		{
			text_p = strrchr (text, ' ');
			digits = (text_p == NULL) ? 7 : 6 - (int)(text_p - text);
			x -= digits * fontwidth;
		}
	}

	text_p = strchr (text, '-');
	if (text_p == NULL || text_p - text > 7 - digits)
	{
		text_p = text + 7 - digits;
	}

	xofs = x;

	if (*text_p == '-')
	{
		x -= fontwidth;
		WI_DrawCharPatch (wiminus, x, y);
	}

	// draw the new number
	while (*text_p)
	{
		if (*text_p >= '0' && *text_p <= '9')
		{
			patch_t *p = num[*text_p - '0'];
			WI_DrawCharPatch (p, xofs + (fontwidth - SHORT(p->width))/2, y);
		}
		text_p++;
		xofs += fontwidth;
	}
	return x;

}

void WI_drawPercent (int x, int y, int p, int b)
{
	if (p < 0)
		return;

	if (wi_percents)
	{
		FB->DrawPatchClean (percent, x, y);
		if (b == 0)
			WI_drawNum (x, y, 100, -1, false);
		else
			WI_drawNum(x, y, p * 100 / b, -1, false);
	}
	else
	{
		int y2 = y + percent->height - screen->Font->GetHeight ();
		x = WI_drawNum (x, y, b, -1, false);
		x -= screen->StringWidth (" OF ");
		screen->DrawTextCleanMove (CR_UNTRANSLATED, x, y2, " OF");
		WI_drawNum (x, y, p, -1, false);
	}
}



//
// Display level completion time and par, or "sucks" message if overflow.
//
void WI_drawTime (int x, int y, int t)
{
	if (t<0)
		return;

	if (t <= 61*59 || gameinfo.gametype != GAME_Doom)
	{
		int hours = t / 3600;
		t -= hours * 3600;
		int minutes = t / 60;
		t -= minutes * 60;
		int seconds = t;
		/*
		int numspacing = SHORT(num[0]->width)*2;
		int spacing = numspacing + SHORT(colon->width);
		*/

		x -= 34*2 + 26;
		if (hours)
		{
			WI_drawNum (x, y, hours, 2);
			WI_DrawCharPatch (colon, x + 26, y);
		}
		x += 34;
		if (minutes | hours)
		{
			WI_drawNum (x, y, minutes, 2);
		}
		x += 34;
		WI_DrawCharPatch (colon, x - 8, y);
		WI_drawNum (x, y, seconds, 2);
	}
	else
	{ // "sucks"
		FB->DrawPatchClean (sucks, x - SHORT(sucks->width), y); 
	}
}

void WI_End ()
{
	state = LeavingIntermission;
	WI_unloadData ();
	if (background)
	{
		delete background;
		background = NULL;
	}

	//Added by mc
	if (deathmatch)
	{
		bglobal.RemoveAllBots (consoleplayer != Net_Arbitrator);
	}
}

void WI_initNoState ()
{
	state = NoState;
	acceleratestage = 0;
	cnt = 10;
}

void WI_updateNoState ()
{
	WI_updateAnimatedBack();

	if (!--cnt)
	{
		WI_End();
		G_WorldDone();
	}
}

static BOOL snl_pointeron = false;

void WI_initShowNextLoc ()
{
	state = ShowNextLoc;
	acceleratestage = 0;
	cnt = SHOWNEXTLOCDELAY * TICRATE;

	if (epsd != wbs->next_ep && gameinfo.gametype == GAME_Doom)
	{
		WI_unloadData ();
		epsd = wbs->next_ep;
		WI_loadData ();
	}

	WI_initAnimatedBack();
}

void WI_updateShowNextLoc ()
{
	WI_updateAnimatedBack();

	if (!--cnt || acceleratestage)
		WI_initNoState();
	else
		snl_pointeron = (cnt & 31) < 20;
}

void WI_drawShowNextLoc ()
{
	int i;

	// draw animated background
	WI_drawAnimatedBack (); 

	if (gamemode != commercial)
	{
		if ((gameinfo.gametype != GAME_Doom || epsd > 2) &&
			(gameinfo.gametype != GAME_Heretic || epsd > 12 || epsd < 10))
		{
			WI_drawEL();
			return;
		}

		int ep = (gameinfo.gametype == GAME_Doom) ? epsd : epsd - 10;

		// draw a splat on taken cities.
		for (i = 0; i < NUMMAPS; i++)
		{
			if (FindLevelInfo (names[ep][i])->flags & LEVEL_VISITED)
				WI_drawOnLnode (i, &splat, epsd);
		}

		// draw flashing ptr
		if (snl_pointeron)
			WI_drawOnLnode (WI_MapToIndex (wbs->next, wbs->next_ep), yah, wbs->next_ep); 
	}

	// draws which level you are entering..
	WI_drawEL();  

}

void WI_drawNoState ()
{
	snl_pointeron = true;
	WI_drawShowNextLoc ();
}

int WI_fragSum (int playernum)
{
	int i;
	int frags = 0;
	
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i]
			&& i!=playernum)
		{
			frags += plrs[playernum].frags[i];
		}
	}
		
	// JDC hack - negative frags.
	frags -= plrs[playernum].frags[playernum];

	return frags;
}

static int dm_state;
static int dm_frags[MAXPLAYERS][MAXPLAYERS];
static int dm_totals[MAXPLAYERS];

void WI_initDeathmatchStats (void)
{

	int i, j;

	state = StatCount;
	acceleratestage = 0;
	dm_state = 1;

	cnt_pause = TICRATE;

	for (i=0 ; i<MAXPLAYERS ; i++)
	{
		if (playeringame[i])
		{
			for (j=0 ; j<MAXPLAYERS ; j++)
				if (playeringame[j])
					dm_frags[i][j] = 0;

			dm_totals[i] = 0;
		}
	}
	
	WI_initAnimatedBack();
}

void WI_updateDeathmatchStats ()
{
	/*
	int i, j;
	BOOL stillticking;
	*/

	WI_updateAnimatedBack();

	if (acceleratestage && dm_state != 4)
	{
		/*
		acceleratestage = 0;

		for (i=0 ; i<MAXPLAYERS ; i++)
		{
			if (playeringame[i])
			{
				for (j=0 ; j<MAXPLAYERS ; j++)
					if (playeringame[j])
						dm_frags[i][j] = plrs[i].frags[j];

				dm_totals[i] = WI_fragSum(i);
			}
		}
		
		S_Sound (CHAN_VOICE, NEXTSTAGE, 1, ATTN_NONE);
		*/
		dm_state = 4;
	}

	if (dm_state == 2)
	{
		/*
		if (!(bcnt&3))
			S_Sound (CHAN_VOICE, "weapons/pistol", 1, ATTN_NONE);
		
		stillticking = false;

		for (i=0 ; i<MAXPLAYERS ; i++)
		{
			if (playeringame[i])
			{
				for (j=0 ; j<MAXPLAYERS ; j++)
				{
					if (playeringame[j]
						&& dm_frags[i][j] != plrs[i].frags[j])
					{
						if (plrs[i].frags[j] < 0)
							dm_frags[i][j]--;
						else
							dm_frags[i][j]++;

						if (dm_frags[i][j] > 99)
							dm_frags[i][j] = 99;

						if (dm_frags[i][j] < -99)
							dm_frags[i][j] = -99;
						
						stillticking = true;
					}
				}
				dm_totals[i] = WI_fragSum(i);

				if (dm_totals[i] > 99)
					dm_totals[i] = 99;
				
				if (dm_totals[i] < -99)
					dm_totals[i] = -99;
			}
			
		}
		if (!stillticking)
		{
			S_Sound (CHAN_VOICE, NEXTSTAGE, 1, ATTN_NONE);
			dm_state++;
		}
		*/
		dm_state = 3;
	}
	else if (dm_state == 4)
	{
		if (acceleratestage)
		{
			S_Sound (CHAN_VOICE, "players/male/gibbed", 1, ATTN_NONE);

			if (gamemode == commercial)
				WI_initNoState();
			else
				WI_initShowNextLoc();
		}
	}
	else if (dm_state & 1)
	{
		if (!--cnt_pause)
		{
			dm_state++;
			cnt_pause = TICRATE;
		}
	}
}



void WI_drawDeathmatchStats ()
{

	// draw animated background
	WI_drawAnimatedBack(); 
	WI_drawLF();

	// [RH] Draw heads-up scores display
	HU_DrawScores (&players[me]);
	
/*
	int 		i;
	int 		j;
	int 		x;
	int 		y;
	int 		w;
	
	int 		lh; 	// line height

	lh = WI_SPACINGY;

	// draw stat titles (top line)
	V_DrawPatchClean(DM_TOTALSX-SHORT(total->width)/2,
				DM_MATRIXY-WI_SPACINGY+10,
				&FB,
				total);
	
	V_DrawPatchClean(DM_KILLERSX, DM_KILLERSY, &FB, killers);
	V_DrawPatchClean(DM_VICTIMSX, DM_VICTIMSY, &FB, victims);

	// draw P?
	x = DM_MATRIXX + DM_SPACINGX;
	y = DM_MATRIXY;

	for (i=0 ; i<MAXPLAYERS ; i++)
	{
		if (playeringame[i])
		{
			V_DrawPatchClean(x-SHORT(p[i]->width)/2,
						DM_MATRIXY - WI_SPACINGY,
						&FB,
						p[i]);
			
			V_DrawPatchClean(DM_MATRIXX-SHORT(p[i]->width)/2,
						y,
						&FB,
						p[i]);

			if (i == me)
			{
				V_DrawPatchClean(x-SHORT(p[i]->width)/2,
							DM_MATRIXY - WI_SPACINGY,
							&FB,
							bstar);

				V_DrawPatchClean(DM_MATRIXX-SHORT(p[i]->width)/2,
							y,
							&FB,
							star);
			}
		}
		x += DM_SPACINGX;
		y += WI_SPACINGY;
	}

	// draw stats
	y = DM_MATRIXY+10;
	w = SHORT(num[0]->width);

	for (i=0 ; i<MAXPLAYERS ; i++)
	{
		x = DM_MATRIXX + DM_SPACINGX;

		if (playeringame[i])
		{
			for (j=0 ; j<MAXPLAYERS ; j++)
			{
				if (playeringame[j])
					WI_drawNum(x+w, y, dm_frags[i][j], 2);

				x += DM_SPACINGX;
			}
			WI_drawNum(DM_TOTALSX+w, y, dm_totals[i], 2);
		}
		y += WI_SPACINGY;
	}
*/
}

static int cnt_frags[MAXPLAYERS];
static int dofrags;
static int ng_state;

void WI_initNetgameStats ()
{

	int i;

	state = StatCount;
	acceleratestage = 0;
	ng_state = 1;

	cnt_pause = TICRATE;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;

		cnt_kills[i] = cnt_items[i] = cnt_secret[i] = cnt_frags[i] = 0;

		dofrags += WI_fragSum (i);
	}

	dofrags = !!dofrags;

	WI_initAnimatedBack ();
}



void WI_updateNetgameStats ()
{

	int i;
	int fsum;
	BOOL stillticking;

	WI_updateAnimatedBack ();

	if (acceleratestage && ng_state != 10)
	{
		acceleratestage = 0;

		for (i=0 ; i<MAXPLAYERS ; i++)
		{
			if (!playeringame[i])
				continue;

			cnt_kills[i] = plrs[i].skills;
			cnt_items[i] = plrs[i].sitems;
			cnt_secret[i] = plrs[i].ssecret;

			if (dofrags)
				cnt_frags[i] = WI_fragSum (i);
		}
		S_Sound (CHAN_VOICE, NEXTSTAGE, 1, ATTN_NONE);
		ng_state = 10;
	}

	if (ng_state == 2)
	{
		if (!(bcnt&3))
			S_Sound (CHAN_VOICE, "weapons/pistol", 1, ATTN_NONE);

		stillticking = false;

		for (i=0 ; i<MAXPLAYERS ; i++)
		{
			if (!playeringame[i])
				continue;

			cnt_kills[i] += 2;

			if (cnt_kills[i] > plrs[i].skills)
				cnt_kills[i] = plrs[i].skills;
			else
				stillticking = true;
		}
		
		if (!stillticking)
		{
			S_Sound (CHAN_VOICE, NEXTSTAGE, 1, ATTN_NONE);
			ng_state++;
		}
	}
	else if (ng_state == 4)
	{
		if (!(bcnt&3))
			S_Sound (CHAN_VOICE, "weapons/pistol", 1, ATTN_NONE);

		stillticking = false;

		for (i=0 ; i<MAXPLAYERS ; i++)
		{
			if (!playeringame[i])
				continue;

			cnt_items[i] += 2;
			if (cnt_items[i] > plrs[i].sitems)
				cnt_items[i] = plrs[i].sitems;
			else
				stillticking = true;
		}
		if (!stillticking)
		{
			S_Sound (CHAN_VOICE, NEXTSTAGE, 1, ATTN_NONE);
			ng_state++;
		}
	}
	else if (ng_state == 6)
	{
		if (!(bcnt&3))
			S_Sound (CHAN_VOICE, "weapons/pistol", 1, ATTN_NONE);

		stillticking = false;

		for (i=0 ; i<MAXPLAYERS ; i++)
		{
			if (!playeringame[i])
				continue;

			cnt_secret[i] += 2;

			if (cnt_secret[i] > plrs[i].ssecret)
				cnt_secret[i] = plrs[i].ssecret;
			else
				stillticking = true;
		}
		
		if (!stillticking)
		{
			S_Sound (CHAN_VOICE, NEXTSTAGE, 1, ATTN_NONE);
			ng_state += 1 + 2*!dofrags;
		}
	}
	else if (ng_state == 8)
	{
		if (!(bcnt&3))
			S_Sound (CHAN_VOICE, "weapons/pistol", 1, ATTN_NONE);

		stillticking = false;

		for (i=0 ; i<MAXPLAYERS ; i++)
		{
			if (!playeringame[i])
				continue;

			cnt_frags[i] += 1;

			if (cnt_frags[i] >= (fsum = WI_fragSum(i)))
				cnt_frags[i] = fsum;
			else
				stillticking = true;
		}
		
		if (!stillticking)
		{
			S_Sound (CHAN_VOICE, "player/male/death1", 1, ATTN_NONE);
			ng_state++;
		}
	}
	else if (ng_state == 10)
	{
		if (acceleratestage)
		{
			S_Sound (CHAN_VOICE, PASTSTATS, 1, ATTN_NONE);
			if ( gamemode == commercial )
				WI_initNoState();
			else
				WI_initShowNextLoc();
		}
	}
	else if (ng_state & 1)
	{
		if (!--cnt_pause)
		{
			ng_state++;
			cnt_pause = TICRATE;
		}
	}
}

void WI_drawNetgameStats ()
{
	int i, x, y;
	int pwidth = SHORT(percent->width);

	// draw animated background
	WI_drawAnimatedBack(); 

	WI_drawLF();

	if (gameinfo.gametype == GAME_Doom)
	{
		// draw stat titles (top line)
		FB->DrawPatchClean (kills, NG_STATSX+NG_SPACINGX-SHORT(kills->width), NG_STATSY);
		FB->DrawPatchClean (items, NG_STATSX+2*NG_SPACINGX-SHORT(items->width), NG_STATSY);
		FB->DrawPatchClean (secret, NG_STATSX+3*NG_SPACINGX-SHORT(secret->width), NG_STATSY);

		if (dofrags)
			FB->DrawPatchClean (frags, NG_STATSX+4*NG_SPACINGX-SHORT(frags->width), NG_STATSY);

		// draw stats
		y = NG_STATSY + SHORT(kills->height);

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (y >= 200-WI_SPACINGY)
				break;

			if (!playeringame[i])
				continue;

			x = NG_STATSX;
			// [RH] Only use one graphic for the face backgrounds
			V_ColorMap = translationtables[TRANSLATION_Players] + i*256;
			FB->DrawTranslatedPatchClean (p, x-SHORT(p->width), y);

			if (i == me)
				FB->DrawTranslatedPatchClean (star, x-SHORT(p->width), y);

			x += NG_SPACINGX;
			WI_drawPercent (x-pwidth, y+10, cnt_kills[i], wbs->maxkills);	x += NG_SPACINGX;
			WI_drawPercent (x-pwidth, y+10, cnt_items[i], wbs->maxitems);	x += NG_SPACINGX;
			WI_drawPercent (x-pwidth, y+10, cnt_secret[i], wbs->maxsecret);	x += NG_SPACINGX;

			if (dofrags)
				WI_drawNum(x, y+10, cnt_frags[i], -1, false);

			y += WI_SPACINGY;
		}
	}
	else
	{
		FB->SetFont (BigFont);
		FB->DrawTextCleanShadowMove (CR_UNTRANSLATED, 95, 35, "KILLS");
		FB->DrawTextCleanShadowMove (CR_UNTRANSLATED, 155, 35, "BONUS");
		FB->DrawTextCleanShadowMove (CR_UNTRANSLATED, 232, 35, "SECRET");
		WI_drawLF ();

		y = 50;
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (y >= 200-WI_SPACINGY)
				break;
			if (!playeringame[i])
				continue;
			V_ColorMap = translationtables[TRANSLATION_Players] + i*256;
			screen->DrawTranslatedPatchClean (star, 25, y);
			WI_drawPercent (127, y+10, cnt_kills[i], wbs->maxkills);
			if (ng_state >= 4)
			{
				WI_drawPercent (202, y+10, cnt_items[i], wbs->maxitems);
				if (ng_state >= 6)
				{
					WI_drawPercent (279, y+10, cnt_secret[i], wbs->maxsecret);
				}
			}
			y += 37;
		}
	}
}

static int sp_state;

void WI_initStats ()
{
	state = StatCount;
	acceleratestage = 0;
	sp_state = 1;
	cnt_kills[0] = cnt_items[0] = cnt_secret[0] = -1;
	cnt_time = cnt_par = -1;
	cnt_pause = TICRATE;

	WI_initAnimatedBack();
}

void WI_updateStats ()
{
	WI_updateAnimatedBack ();

	if ((gameinfo.gametype != GAME_Doom || acceleratestage)
		&& sp_state != 10)
	{
		if (acceleratestage)
		{
			acceleratestage = 0;
			sp_state = 10;
			S_Sound (CHAN_VOICE, NEXTSTAGE, 1, ATTN_NONE);
		}
		cnt_kills[0] = plrs[me].skills;
		cnt_items[0] = plrs[me].sitems;
		cnt_secret[0] = plrs[me].ssecret;
		cnt_time = plrs[me].stime / TICRATE;
		cnt_par = wbs->partime / TICRATE;
	}

	if (sp_state == 2)
	{
		if (gameinfo.gametype == GAME_Doom)
		{
			cnt_kills[0] += 2;

			if (!(bcnt&3))
				S_Sound (CHAN_VOICE, "weapons/pistol", 1, ATTN_NONE);
		}
		if (cnt_kills[0] >= plrs[me].skills)
		{
			cnt_kills[0] = plrs[me].skills;
			S_Sound (CHAN_VOICE, NEXTSTAGE, 1, ATTN_NONE);
			sp_state++;
		}
	}
	else if (sp_state == 4)
	{
		if (gameinfo.gametype == GAME_Doom)
		{
			cnt_items[0] += 2;

			if (!(bcnt&3))
				S_Sound (CHAN_VOICE, "weapons/pistol", 1, ATTN_NONE);
		}
		if (cnt_items[0] >= plrs[me].sitems)
		{
			cnt_items[0] = plrs[me].sitems;
			S_Sound (CHAN_VOICE, NEXTSTAGE, 1, ATTN_NONE);
			sp_state++;
		}
	}
	else if (sp_state == 6)
	{
		if (gameinfo.gametype == GAME_Doom)
		{
			cnt_secret[0] += 2;

			if (!(bcnt&3))
				S_Sound (CHAN_VOICE, "weapons/pistol", 1, ATTN_NONE);
		}
		if (cnt_secret[0] >= plrs[me].ssecret)
		{
			cnt_secret[0] = plrs[me].ssecret;
			S_Sound (CHAN_VOICE, NEXTSTAGE, 1, ATTN_NONE);
			sp_state++;
		}
	}
	else if (sp_state == 8)
	{
		if (gameinfo.gametype == GAME_Doom)
		{
			if (!(bcnt&3))
				S_Sound (CHAN_VOICE, "weapons/pistol", 1, ATTN_NONE);

			cnt_time += 3;
			cnt_par += 3;
		}

		if (cnt_time >= plrs[me].stime / TICRATE)
			cnt_time = plrs[me].stime / TICRATE;

		if (cnt_par >= wbs->partime / TICRATE)
		{
			cnt_par = wbs->partime / TICRATE;

			if (cnt_time >= plrs[me].stime / TICRATE)
			{
				S_Sound (CHAN_VOICE, NEXTSTAGE, 1, ATTN_NONE);
				sp_state++;
			}
		}
	}
	else if (sp_state == 10)
	{
		if (acceleratestage)
		{
			S_Sound (CHAN_VOICE, PASTSTATS, 1, ATTN_NONE);

			if (gamemode == commercial)
				WI_initNoState();
			else
				WI_initShowNextLoc();
		}
	}
	else if (sp_state & 1)
	{
		if (!--cnt_pause)
		{
			sp_state++;
			cnt_pause = TICRATE;
		}
	}
}

void WI_drawStats (void)
{
	// line height
	int lh; 	

	lh = (3*SHORT(num[0]->height))/2;

	// draw animated background
	WI_drawAnimatedBack();

	WI_drawLF();

	if (gameinfo.gametype == GAME_Doom)
	{
		FB->DrawPatchClean (kills, SP_STATSX, SP_STATSY);
		WI_drawPercent (320 - SP_STATSX, SP_STATSY, cnt_kills[0], wbs->maxkills);

		FB->DrawPatchClean (items, SP_STATSX, SP_STATSY+lh);
		WI_drawPercent (320 - SP_STATSX, SP_STATSY+lh, cnt_items[0], wbs->maxitems);

		FB->DrawPatchClean (sp_secret, SP_STATSX, SP_STATSY+2*lh);
		WI_drawPercent(320 - SP_STATSX, SP_STATSY+2*lh, cnt_secret[0], wbs->maxsecret);

		FB->DrawPatchClean (timepic, SP_TIMEX, SP_TIMEY);
		WI_drawTime (160 - SP_TIMEX, SP_TIMEY, cnt_time);

		if (wbs->partime)
		{
			FB->DrawPatchClean (par, 160 + SP_TIMEX, SP_TIMEY);
			WI_drawTime (320 - SP_TIMEX, SP_TIMEY, cnt_par);
		}
	}
	else
	{
		screen->SetFont (BigFont);
		screen->DrawTextCleanShadowMove (CR_UNTRANSLATED, 50, 65, "KILLS");
		screen->DrawTextCleanShadowMove (CR_UNTRANSLATED, 50, 90, "ITEMS");
		screen->DrawTextCleanShadowMove (CR_UNTRANSLATED, 50, 115, "SECRETS");

		if (sp_state >= 2)
		{
			WI_drawNum (200, 65, cnt_kills[0], 3, false);
			screen->DrawShadowedPatchClean (slash, 237, 65);
			WI_drawNum (248, 65, wbs->maxkills, 3, false);
		}
		if (sp_state >= 4)
		{
			WI_drawNum (200, 90, cnt_items[0], 3, false);
			screen->DrawShadowedPatchClean (slash, 237, 90);
			WI_drawNum (248, 90, wbs->maxitems, 3, false);
		}
		if (sp_state >= 6)
		{
			WI_drawNum (200, 115, cnt_secret[0], 3, false);
			screen->DrawShadowedPatchClean (slash, 237, 115);
			WI_drawNum (248, 115, wbs->maxsecret, 3, false);
		}
		if (sp_state >= 8)
		{
			screen->DrawTextCleanShadowMove (CR_UNTRANSLATED, 85, 160, "TIME");
			WI_drawTime (249, 160, cnt_time);
		}
		screen->SetFont (SmallFont);
	}
}

void WI_checkForAccelerate (void)
{
	int i;
	player_t *player;

	// check for button presses to skip delays
	for (i = 0, player = players; i < MAXPLAYERS; i++, player++)
	{
		if (playeringame[i])
		{
			if ((player->cmd.ucmd.buttons ^ player->oldbuttons) &&
				((players[i].cmd.ucmd.buttons & players[i].oldbuttons)
					== players[i].oldbuttons))
			{
				acceleratestage = 1;
			}
			player->oldbuttons = player->cmd.ucmd.buttons;
		}
	}
}

// Updates stuff each tick
void WI_Ticker ()
{
	// counter for general background animation
	bcnt++;  

	if (bcnt == 1)
	{
		// intermission music
		if (gameinfo.gametype == GAME_Heretic)
			S_ChangeMusic ("mus_intr");
		else if (gameinfo.gametype == GAME_Hexen)
			S_ChangeMusic ("hub");
		else if (gamemode == commercial)
			S_ChangeMusic ("d_dm2int");
		else
			S_ChangeMusic ("d_inter"); 
	}

	WI_checkForAccelerate ();

	switch (state)
	{
	case StatCount:
		if (deathmatch)
			WI_updateDeathmatchStats ();
		else if (multiplayer)
			WI_updateNetgameStats ();
		else
			WI_updateStats ();
		break;
	
	case ShowNextLoc:
		WI_updateShowNextLoc ();
		break;
	
	case NoState:
		WI_updateNoState ();
		break;

	case LeavingIntermission:
		break;	// Silence GCC
	}
}

void WI_loadData ()
{
	int i, j;
	char name[9];
	in_anim_t *a;

	if (gameinfo.gametype != GAME_Doom)
	{
		for (i = 0; i < 2; i++)
		{
			lnames[i] = NULL;
			lnametexts[i] = FindLevelInfo (i == 0 ? wbs->current : wbs->next)->level_name;
			lnamewidths[i] = WI_CalcWidth (lnametexts[i]);
		}
		if (gameinfo.gametype == GAME_Hexen)
		{
			const byte *bg = (byte *)W_MapLumpName ("INTERPIC");
			background = I_NewStaticCanvas (320, 200);
			background->Lock ();
			background->DrawPageBlock (bg);
			background->Unlock ();
			W_UnMapLump (bg);
			return;
		}
	}

	if (gameinfo.gametype == GAME_Doom &&
		gamemode != commercial &&
		epsd < 3)
	{
		for (j = 0; j < NUMANIMS[epsd]; j++)
		{
			a = &anims[epsd][j];
			for (i = 0; i < a->nanims; i++)
			{
				// MONDO HACK!
				if (epsd != 1 || j != 8) 
				{
					// animations
					sprintf (name, "WIA%d%.2d%.2d", epsd, j, i);  
					a->p[i] = (patch_t *)W_MapLumpName (name);
				}
				else
				{
					// HACK ALERT!
					a->p[i] = anims[1][4].p[i];
				}
			}
		}
	}

	if (gameinfo.gametype == GAME_Doom)
	{
		WI_DrawDoomBack ();

		for (i = 0; i < 2; i++)
		{
			char *lname = (i == 0 ? wbs->lname0 : wbs->lname1);

			j = lname ? W_CheckNumForName (lname) : -1;

			if (j >= 0)
			{
				lnames[i] = (patch_t *)W_MapLumpNum (j);
			}
			else
			{
				lnames[i] = NULL;
				lnametexts[i] = FindLevelInfo (i == 0 ? wbs->current : wbs->next)->level_name;
				lnamewidths[i] = WI_CalcWidth (lnametexts[i]);
			}
		}
	}
	else
	{
		background = NULL;
	}

	if (gameinfo.gametype == GAME_Doom)
	{
		if (gamemode != commercial)
		{
			yah[0] = (patch_t *)W_MapLumpName ("WIURH0");	// you are here
			yah[1] = (patch_t *)W_MapLumpName ("WIURH1");	// you are here (alt.)
			splat = (patch_t *)W_MapLumpName ("WISPLAT");	// splat
		}
		wiminus = (patch_t *)W_MapLumpName ("WIMINUS");		// minus sign
		percent = (patch_t *)W_MapLumpName ("WIPCNT");		// percent sign
		finished = (patch_t *)W_MapLumpName ("WIF");		// "finished"
		entering = (patch_t *)W_MapLumpName ("WIENTER");	// "entering"
		kills = (patch_t *)W_MapLumpName ("WIOSTK");		// "kills"
		secret = (patch_t *)W_MapLumpName ("WIOSTS");		// "scrt"
		sp_secret = (patch_t *)W_MapLumpName ("WISCRT2");	// "secret"
		items = (patch_t *)W_MapLumpName ("WIOSTI");		// "items"
		frags = (patch_t *)W_MapLumpName ("WIFRGS");		// "frgs"
		colon = (patch_t *)W_MapLumpName ("WICOLON");		// ":"
		timepic = (patch_t *)W_MapLumpName ("WITIME");		// "time"
		sucks = (patch_t *)W_MapLumpName ("WISUCKS");		// "sucks"
		par = (patch_t *)W_MapLumpName ("WIPAR");			// "par"
		killers = (patch_t *)W_MapLumpName ("WIKILRS");		// "killers" (vertical)
		victims = (patch_t *)W_MapLumpName ("WIVCTMS");		// "victims" (horiz)
		total = (patch_t *)W_MapLumpName ("WIMSTT");		// "total"
		star = (patch_t *)W_MapLumpName ("STFST01");		// your face
		bstar = (patch_t *)W_MapLumpName("STFDEAD0");		// dead face
		p = (patch_t *)W_MapLumpName ("STPBANY");

		for (i = 0; i < 10; i++)
		{ // numbers 0-9
			sprintf (name, "WINUM%d", i);	 
			num[i] = (patch_t *)W_MapLumpName (name);
		}
	}
	else
	{
		yah[0] =
		yah[1] = (patch_t *)W_MapLumpName ("IN_YAH");
		splat = (patch_t *)W_MapLumpName ("IN_X");
		wiminus = (patch_t *)W_MapLumpName ("FONTB13");
		percent = (patch_t *)W_MapLumpName ("FONTB05");
		colon = (patch_t *)W_MapLumpName ("FONTB26");
		slash = (patch_t *)W_MapLumpName ("FONTB15");
		star = (patch_t *)W_MapLumpName ("FACEA0");
		bstar = (patch_t *)W_MapLumpName ("FACEB0");

		for (i = 0; i < 10; i++)
		{
			sprintf (name, "FONTB%d", 16 + i);
			num[i] = (patch_t *)W_MapLumpName (name);
		}
	}
}

void WI_unloadData ()
{
	int i, j;

	if (gameinfo.gametype == GAME_Hexen)
	{
		return;
	}

	W_UnMapLump (wiminus);

	for (i = 0; i < 10; i++)
	{
		W_UnMapLump (num[i]);
	}

	for (i = 0; i < 2; i++)
	{
		if (lnames[i])
		{
			W_UnMapLump (lnames[i]);
			lnames[i] = NULL;
		}
	}
	
	if (gamemode != commercial)
	{
		W_UnMapLump (yah[0]);
		W_UnMapLump (yah[1]);
		W_UnMapLump (splat);
		
		if (gameinfo.gametype == GAME_Doom && epsd < 3)
		{
			for (j = 0; j < NUMANIMS[epsd]; j++)
			{
				if (epsd != 1 || j != 8)
				{
					for (i = 0; i < anims[epsd][j].nanims; i++)
						W_UnMapLump (anims[epsd][j].p[i]);
				}
			}
		}
	}

	if (percent)	{ W_UnMapLump (percent);	percent = NULL; }
	if (colon)		{ W_UnMapLump (colon);		colon = NULL; }
	if (finished)	{ W_UnMapLump (finished);	finished = NULL; }
	if (entering)	{ W_UnMapLump (entering);	entering = NULL; }
	if (kills)		{ W_UnMapLump (kills);		kills = NULL; }
	if (secret)		{ W_UnMapLump (secret);		secret = NULL; }
	if (sp_secret)	{ W_UnMapLump (sp_secret);	sp_secret = NULL; }
	if (items)		{ W_UnMapLump (items);		items = NULL; }
	if (frags)		{ W_UnMapLump (frags);		frags = NULL; }
	if (timepic)	{ W_UnMapLump (timepic);	timepic = NULL; }
	if (sucks)		{ W_UnMapLump (sucks);		sucks = NULL; }
	if (par)		{ W_UnMapLump (par);		par = NULL; }
	if (victims)	{ W_UnMapLump (victims);	victims = NULL; }
	if (killers)	{ W_UnMapLump (killers);	killers = NULL; }
	if (total)		{ W_UnMapLump (total);		total = NULL; }
	if (p)			{ W_UnMapLump (p);			p = NULL; }
	if (slash)		{ W_UnMapLump (slash);		slash = NULL; }
	if (star)		{ W_UnMapLump (star);		star = NULL; }
	if (bstar)		{ W_UnMapLump (bstar);		bstar = NULL; }
}

void WI_Drawer (void)
{
	switch (state)
	{
	case StatCount:
		if (deathmatch)
			WI_drawDeathmatchStats();
		else if (multiplayer)
			WI_drawNetgameStats();
		else
			WI_drawStats();
		break;
	
	case ShowNextLoc:
		WI_drawShowNextLoc();
		break;
	
	case LeavingIntermission:
		break;

	default:
		WI_drawNoState();
		break;
	}
}


void WI_initVariables (wbstartstruct_t *wbstartstruct)
{
	wbs = wbstartstruct;

	acceleratestage = 0;
	cnt = bcnt = 0;
	me = wbs->pnum;
	plrs = wbs->plyr;
	epsd = wbs->finished_ep;
}

void WI_Start (wbstartstruct_t *wbstartstruct)
{
	V_SetBlend (0,0,0,0);
	WI_initVariables (wbstartstruct);
	WI_loadData ();
	if (deathmatch)
		WI_initDeathmatchStats();
	else if (multiplayer)
		WI_initNetgameStats();
	else
		WI_initStats();
	S_StopAllChannels ();
	SN_StopAllSequences ();
}

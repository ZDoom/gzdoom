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

#include "z_zone.h"
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
#include "wi_stuff.h"
#include "c_consol.h"
#include "hu_stuff.h"
#include "v_palett.h"
#include "s_sndseq.h"

cvar_t *WI_Percents;

void WI_unloadData(void);

//
// Data needed to add patches to full screen intermission pics.
// Patches are statistics messages, and animations.
// Loads of by-pixel layout and placement, offsets etc.
//


//
// Different vetween registered DOOM (1994) and
//	Ultimate DOOM - Final edition (retail, 1995?).
// This is supposedly ignored for commercial
//	release (aka DOOM II), which had 32 maps
//	in one episode. So there.
#define NUMEPISODES 	4
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
	// Episode 0 World Map
	{
		{ 185, 164 },	// location of level 0 (CJ)
		{ 148, 143 },	// location of level 1 (CJ)
		{ 69, 122 },	// location of level 2 (CJ)
		{ 209, 102 },	// location of level 3 (CJ)
		{ 116, 89 },	// location of level 4 (CJ)
		{ 166, 55 },	// location of level 5 (CJ)
		{ 71, 56 }, 	// location of level 6 (CJ)
		{ 135, 29 },	// location of level 7 (CJ)
		{ 71, 24 }		// location of level 8 (CJ)
	},

	// Episode 1 World Map
	{
		{ 254, 25 },	// location of level 0 (CJ)
		{ 97, 50 }, 	// location of level 1 (CJ)
		{ 188, 64 },	// location of level 2 (CJ)
		{ 128, 78 },	// location of level 3 (CJ)
		{ 214, 92 },	// location of level 4 (CJ)
		{ 133, 130 },	// location of level 5 (CJ)
		{ 208, 136 },	// location of level 6 (CJ)
		{ 148, 140 },	// location of level 7 (CJ)
		{ 235, 158 }	// location of level 8 (CJ)
	},

	// Episode 2 World Map
	{
		{ 156, 168 },	// location of level 0 (CJ)
		{ 48, 154 },	// location of level 1 (CJ)
		{ 174, 95 },	// location of level 2 (CJ)
		{ 265, 75 },	// location of level 3 (CJ)
		{ 130, 48 },	// location of level 4 (CJ)
		{ 279, 23 },	// location of level 5 (CJ)
		{ 198, 48 },	// location of level 6 (CJ)
		{ 140, 25 },	// location of level 7 (CJ)
		{ 281, 136 }	// location of level 8 (CJ)
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
static char names[NUMEPISODES][NUMMAPS][8] = {
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

// in seconds
#define SHOWNEXTLOCDELAY		4
//#define SHOWLASTLOCDELAY		SHOWNEXTLOCDELAY


// used to accelerate or skip a stage
static int				acceleratestage;

// wbs->pnum
static int				me;

 // specifies current state
static stateenum_t		state;

// contains information passed into intermission
static wbstartstruct_t* wbs;

static wbplayerstruct_t* plrs;	// wbs->plyr[]

// used for general timing
static int				cnt;  

// used for timing of background animation
static int				bcnt;

static int				cnt_kills[MAXPLAYERS];
static int				cnt_items[MAXPLAYERS];
static int				cnt_secret[MAXPLAYERS];
static int				cnt_time;
static int				cnt_par;
static int				cnt_pause;


//
//		GRAPHICS
//

// You Are Here graphic
static patch_t* 		yah[2]; 

// splat
static patch_t* 		splat;

// %, : graphics
static patch_t* 		percent;
static patch_t* 		colon;

// 0-9 graphic
static patch_t* 		num[10];

// minus sign
static patch_t* 		wiminus;

// "Finished!" graphics
static patch_t* 		finished;

// "Entering" graphic
static patch_t* 		entering; 

// "secret"
static patch_t* 		sp_secret;

 // "Kills", "Scrt", "Items", "Frags"
static patch_t* 		kills;
static patch_t* 		secret;
static patch_t* 		items;
static patch_t* 		frags;

// Time sucks.
static patch_t* 		time;
static patch_t* 		par;
static patch_t* 		sucks;

// "killers", "victims"
static patch_t* 		killers;
static patch_t* 		victims; 

// "Total", your face, your dead face
static patch_t* 		total;
static patch_t* 		star;
static patch_t* 		bstar;

// "red P[1..MAXPLAYERS]"
static patch_t* 		p;		// [RH] Only one, not MAXPLAYERS

 // Name graphics of each level (centered)
static patch_t*			lnames[2];

// [RH] Info to dynamically generate the level name graphics
static int				lnamewidths[2];
static char				*lnametexts[2];

static screen_t			background;

//
// CODE
//

// slam background
// UNUSED static unsigned char *background=0;


void WI_slamBackground (void)
{
	V_Blit (&background, 0, 0, background.width, background.height,
			&FB, 0, 0, FB.width, FB.height);
}

static int WI_DrawName (char *str, int x, int y)
{
	int lump;
	patch_t *p = NULL;
	char charname[9];

	while (*str) {
		sprintf (charname, "FONTB%02u", toupper(*str) - 32);
		lump = W_CheckNumForName (charname);
		if (lump != -1) {
			p = W_CacheLumpNum (lump, PU_CACHE);
			V_DrawPatchClean (x, y, &FB, p);
			x += SHORT(p->width) - 1;
		} else {
			x += 12;
		}
		str++;
	}

	p = W_CacheLumpName ("FONTB39", PU_CACHE);
	return (5*(SHORT(p->height)-SHORT(p->topoffset)))/4;
}


// Draws "<Levelname> Finished!"
void WI_drawLF (void)
{
	int y;

	if (!lnames[0] && !lnamewidths[0])
		return;

	y = WI_TITLEY;

	if (lnames[0]) {
		// draw <LevelName> 
		V_DrawPatchClean((320 - SHORT(lnames[0]->width))/2,
					y, &FB, lnames[0]);
		y += (5*SHORT(lnames[0]->height))/4;
	} else {
		// [RH] draw a dynamic title string
		y += WI_DrawName (lnametexts[0], 160 - lnamewidths[0] / 2, y);
	}

	// draw "Finished!"
	V_DrawPatchClean((320 - SHORT(finished->width))/2,
				y, &FB, finished);
}



// Draws "Entering <LevelName>"
void WI_drawEL (void)
{
	int y = WI_TITLEY;

	if (!lnames[1] && !lnamewidths[1])
		return;

	y = WI_TITLEY;

	// draw "Entering"
	V_DrawPatchClean((320 - SHORT(entering->width))/2,
				y, &FB, entering);

	// [RH] Changed to adjust by height of entering patch instead of title
	y += (5*SHORT(entering->height))/4;

	if (lnames[1]) {
		// draw level
		V_DrawPatchClean((320 - SHORT(lnames[1]->width))/2,
					y, &FB, lnames[1]);
	} else {
		// [RH] draw a dynamic title string
		WI_DrawName (lnametexts[1], 160 - lnamewidths[1] / 2, y);
	}
}

int WI_MapToIndex (char *map)
{
	int i;

	for (i = 0; i < NUMMAPS; i++) {
		if (!strnicmp (names[wbs->epsd][i], map, 8))
			break;
	}
	return i;
}

void WI_drawOnLnode (int n, patch_t *c[])
{

	int 	i;
	int 	left;
	int 	top;
	int 	right;
	int 	bottom;
	BOOL 	fits = false;

	i = 0;
	do
	{
		left = lnodes[wbs->epsd][n].x - SHORT(c[i]->leftoffset);
		top = lnodes[wbs->epsd][n].y - SHORT(c[i]->topoffset);
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
		V_DrawPatchIndirect (lnodes[wbs->epsd][n].x, lnodes[wbs->epsd][n].y, &FB, c[i]);
	}
	else
	{
		// DEBUG
		DPrintf ("Could not place patch on level %d", n+1); 
	}
}



void WI_initAnimatedBack (void)
{
	int i;
	in_anim_t *a;

	if (gamemode == commercial || wbs->epsd > 2)
		return;

	for (i = 0; i < NUMANIMS[wbs->epsd]; i++)
	{
		a = &anims[wbs->epsd][i];

		// init variables
		a->ctr = -1;

		// specify the next time to draw it
		if (a->type == ANIM_ALWAYS)
			a->nexttic = bcnt + 1 + (M_Random()%a->period);
		else if (a->type == ANIM_LEVEL)
			a->nexttic = bcnt + 1;
	}

}

void WI_updateAnimatedBack (void)
{
	int i;
	in_anim_t *a;

	if (gamemode == commercial || wbs->epsd > 2)
		return;

	for (i = 0; i < NUMANIMS[wbs->epsd]; i++)
	{
		a = &anims[wbs->epsd][i];

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
					&& (WI_MapToIndex (wbs->next) + 1) == a->data)
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

	if (gamemode != commercial && wbs->epsd <= 2)
	{
		V_LockScreen (&background);
		for (i = 0; i < NUMANIMS[wbs->epsd]; i++)
		{
			a = &anims[wbs->epsd][i];

			if (a->ctr >= 0)
				V_DrawPatch (a->loc.x, a->loc.y, &background, a->p[a->ctr]);
		}
		V_UnlockScreen (&background);
	}

	WI_slamBackground ();
}

//
// Draws a number.
// If digits > 0, then use that many digits minimum,
//	otherwise only use as many as necessary.
// Returns new x position.
//

int WI_drawNum (int x, int y, int n, int digits)
{

	int fontwidth = SHORT(num[0]->width);
	int neg;
	int temp;

	if (digits < 0)
	{
		if (!n)
		{
			// make variable-length zeros 1 digit long
			digits = 1;
		}
		else
		{
			// figure out # of digits in #
			digits = 0;
			temp = n;

			while (temp)
			{
				temp /= 10;
				digits++;
			}
		}
	}

	neg = n < 0;
	if (neg)
		n = -n;

	// if non-number, do not draw it
	if (n == 1994)
		return 0;

	// draw the new number
	while (digits--)
	{
		x -= fontwidth;
		V_DrawPatchClean (x, y, &FB, num[ n % 10 ]);
		n /= 10;
	}

	// draw a minus sign if necessary
	if (neg)
		V_DrawPatchClean (x-=8, y, &FB, wiminus);

	return x;

}

#include "hu_stuff.h"
extern patch_t *hu_font[HU_FONTSIZE];

void WI_drawPercent (int x, int y, int p, int b)
{
	if (p < 0)
		return;

	if (WI_Percents->value) {
		V_DrawPatchClean(x, y, &FB, percent);
		if (b == 0)
			WI_drawNum (x, y, 100, -1);
		else
			WI_drawNum(x, y, p * 100 / b, -1);
	} else {
		int y2 = y + percent->height - hu_font[HU_FONTSTART]->height;

		x = WI_drawNum(x + percent->width, y, b, -1);
		x -= hu_font['F'-HU_FONTSTART]->width << 1;
		V_DrawPatchClean(x, y2, &FB, hu_font['F'-HU_FONTSTART]);
		x -= hu_font['F'-HU_FONTSTART]->width;
		V_DrawPatchClean(x, y2, &FB, hu_font['O'-HU_FONTSTART]);
		x -= hu_font['O'-HU_FONTSTART]->width;
		WI_drawNum(x, y, p, -1);
	}
}



//
// Display level completion time and par,
//	or "sucks" message if overflow.
//
void WI_drawTime (int x, int y, int t)
{
	int div;
	int n;

	if (t<0)
		return;

	if (t <= 61*59)
	{
		div = 1;

		do
		{
			n = (t / div) % 60;
			x = WI_drawNum(x, y, n, 2) - SHORT(colon->width);
			div *= 60;

			// draw
			if (div==60 || t / div)
				V_DrawPatchClean(x, y, &FB, colon);
			
		} while (t / div);
	}
	else
	{
		// "sucks"
		V_DrawPatchClean(x - SHORT(sucks->width), y, &FB, sucks); 
	}
}


void WI_End (void)
{
	WI_unloadData();
	V_FreeScreen (&background);
}

void WI_initNoState (void)
{
	state = NoState;
	acceleratestage = 0;
	cnt = 10;
}

void WI_updateNoState (void)
{
	WI_updateAnimatedBack();

	if (!--cnt)
	{
		WI_End();
		G_WorldDone();
	}

}

static BOOL snl_pointeron = false;

void WI_initShowNextLoc (void)
{
	state = ShowNextLoc;
	acceleratestage = 0;
	cnt = SHOWNEXTLOCDELAY * TICRATE;

	WI_initAnimatedBack();
}

void WI_updateShowNextLoc (void)
{
	WI_updateAnimatedBack();

	if (!--cnt || acceleratestage)
		WI_initNoState();
	else
		snl_pointeron = (cnt & 31) < 20;
}

void WI_drawShowNextLoc (void)
{
	int i;

	// draw animated background
	WI_drawAnimatedBack (); 

	if (gamemode != commercial)
	{
		if (wbs->epsd > 2)
		{
			WI_drawEL();
			return;
		}

		// draw a splat on taken cities.
		for (i=0; i < NUMMAPS; i++) {
			if (FindLevelInfo (names[wbs->epsd][i])->flags & LEVEL_VISITED)
				WI_drawOnLnode(i, &splat);
		}

		// draw flashing ptr
		if (snl_pointeron)
			WI_drawOnLnode(WI_MapToIndex (wbs->next), yah); 
	}

	// draws which level you are entering..
	WI_drawEL();  

}

void WI_drawNoState (void)
{
	snl_pointeron = true;
	WI_drawShowNextLoc();
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



void WI_updateDeathmatchStats (void)
{

	int i, j;
	BOOL stillticking;

	WI_updateAnimatedBack();

	if (acceleratestage && dm_state != 4)
	{
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
		

		S_Sound (NULL, CHAN_VOICE, "weapons/rocklx", 1, ATTN_NONE);
		dm_state = 4;
	}

	
	if (dm_state == 2)
	{
		if (!(bcnt&3))
			S_Sound (NULL, CHAN_VOICE, "weapons/pistol", 1, ATTN_NONE);
		
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
			S_Sound (NULL, CHAN_VOICE, "weapons/rocklx", 1, ATTN_NONE);
			dm_state++;
		}

	}
	else if (dm_state == 4)
	{
		if (acceleratestage)
		{
			S_Sound (NULL, CHAN_VOICE, "players/male/gibbed", 1, ATTN_NONE);

			if ( gamemode == commercial)
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



void WI_drawDeathmatchStats (void)
{

	// draw animated background
	WI_drawAnimatedBack(); 
	WI_drawLF();

	// [RH] Draw heads-up scores display
	HU_DrawScores (players + me);
	
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

void WI_initNetgameStats (void)
{

	int i;

	state = StatCount;
	acceleratestage = 0;
	ng_state = 1;

	cnt_pause = TICRATE;

	for (i=0 ; i<MAXPLAYERS ; i++)
	{
		if (!playeringame[i])
			continue;

		cnt_kills[i] = cnt_items[i] = cnt_secret[i] = cnt_frags[i] = 0;

		dofrags += WI_fragSum(i);
	}

	dofrags = !!dofrags;

	WI_initAnimatedBack();
}



void WI_updateNetgameStats (void)
{

	int i;
	int fsum;
	BOOL stillticking;

	WI_updateAnimatedBack();

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
		S_Sound (NULL, CHAN_VOICE, "weapons/rocklx", 1, ATTN_NONE);
		ng_state = 10;
	}

	if (ng_state == 2)
	{
		if (!(bcnt&3))
			S_Sound (NULL, CHAN_VOICE, "weapons/pistol", 1, ATTN_NONE);

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
			S_Sound (NULL, CHAN_VOICE, "weapons/rocklx", 1, ATTN_NONE);
			ng_state++;
		}
	}
	else if (ng_state == 4)
	{
		if (!(bcnt&3))
			S_Sound (NULL, CHAN_VOICE, "weapons/pistol", 1, ATTN_NONE);

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
			S_Sound (NULL, CHAN_VOICE, "weapons/rocklx", 1, ATTN_NONE);
			ng_state++;
		}
	}
	else if (ng_state == 6)
	{
		if (!(bcnt&3))
			S_Sound (NULL, CHAN_VOICE, "weapons/pistol", 1, ATTN_NONE);

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
			S_Sound (NULL, CHAN_VOICE, "weapons/rocklx", 1, ATTN_NONE);
			ng_state += 1 + 2*!dofrags;
		}
	}
	else if (ng_state == 8)
	{
		if (!(bcnt&3))
			S_Sound (NULL, CHAN_VOICE, "weapons/pistol", 1, ATTN_NONE);

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
			S_Sound (NULL, CHAN_VOICE, "player/male/death1", 1, ATTN_NONE);
			ng_state++;
		}
	}
	else if (ng_state == 10)
	{
		if (acceleratestage)
		{
			S_Sound (NULL, CHAN_VOICE, "weapons/shotgr", 1, ATTN_NONE);
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



void WI_drawNetgameStats(void)
{
	int i, x, y;
	int pwidth = SHORT(percent->width);

	// draw animated background
	WI_drawAnimatedBack(); 

	WI_drawLF();

	// draw stat titles (top line)
	V_DrawPatchClean(NG_STATSX+NG_SPACINGX-SHORT(kills->width),
				NG_STATSY, &FB, kills);

	V_DrawPatchClean(NG_STATSX+2*NG_SPACINGX-SHORT(items->width),
				NG_STATSY, &FB, items);

	V_DrawPatchClean(NG_STATSX+3*NG_SPACINGX-SHORT(secret->width),
				NG_STATSY, &FB, secret);
	
	if (dofrags)
		V_DrawPatchClean(NG_STATSX+4*NG_SPACINGX-SHORT(frags->width),
					NG_STATSY, &FB, frags);

	// draw stats
	y = NG_STATSY + SHORT(kills->height);

	for (i=0 ; i<MAXPLAYERS ; i++)
	{
		// [RH] Quick hack: Only show the first four players.
		if (i >= 4)
			break;

		if (!playeringame[i])
			continue;

		x = NG_STATSX;
		// [RH] Only use one graphic for the face backgrounds
		V_ColorMap = translationtables + i * 256;
		V_DrawTranslatedPatchClean(x-SHORT(p->width), y, &FB, p);

		if (i == me)
			V_DrawPatchClean(x-SHORT(p->width), y, &FB, star);

		x += NG_SPACINGX;
		WI_drawPercent(x-pwidth, y+10, cnt_kills[i], wbs->maxkills);	x += NG_SPACINGX;
		WI_drawPercent(x-pwidth, y+10, cnt_items[i], wbs->maxitems);	x += NG_SPACINGX;
		WI_drawPercent(x-pwidth, y+10, cnt_secret[i], wbs->maxsecret);	x += NG_SPACINGX;

		if (dofrags)
			WI_drawNum(x, y+10, cnt_frags[i], -1);

		y += WI_SPACINGY;
	}

}

static int sp_state;

void WI_initStats (void)
{
	state = StatCount;
	acceleratestage = 0;
	sp_state = 1;
	cnt_kills[0] = cnt_items[0] = cnt_secret[0] = -1;
	cnt_time = cnt_par = -1;
	cnt_pause = TICRATE;

	WI_initAnimatedBack();
}

void WI_updateStats (void)
{

	WI_updateAnimatedBack();

	if (acceleratestage && sp_state != 10)
	{
		acceleratestage = 0;
		cnt_kills[0] = plrs[me].skills;
		cnt_items[0] = plrs[me].sitems;
		cnt_secret[0] = plrs[me].ssecret;
		cnt_time = plrs[me].stime / TICRATE;
		cnt_par = wbs->partime / TICRATE;
		S_Sound (NULL, CHAN_VOICE, "weapons/rocklx", 1, ATTN_NONE);
		sp_state = 10;
	}

	if (sp_state == 2)
	{
		cnt_kills[0] += 2;

		if (!(bcnt&3))
			S_Sound (NULL, CHAN_VOICE, "weapons/pistol", 1, ATTN_NONE);

		if (cnt_kills[0] >= plrs[me].skills)
		{
			cnt_kills[0] = plrs[me].skills;
			S_Sound (NULL, CHAN_VOICE, "weapons/rocklx", 1, ATTN_NONE);
			sp_state++;
		}
	}
	else if (sp_state == 4)
	{
		cnt_items[0] += 2;

		if (!(bcnt&3))
			S_Sound (NULL, CHAN_VOICE, "weapons/pistol", 1, ATTN_NONE);

		if (cnt_items[0] >= plrs[me].sitems)
		{
			cnt_items[0] = plrs[me].sitems;
			S_Sound (NULL, CHAN_VOICE, "weapons/rocklx", 1, ATTN_NONE);
			sp_state++;
		}
	}
	else if (sp_state == 6)
	{
		cnt_secret[0] += 2;

		if (!(bcnt&3))
			S_Sound (NULL, CHAN_VOICE, "weapons/pistol", 1, ATTN_NONE);

		if (cnt_secret[0] >= plrs[me].ssecret)
		{
			cnt_secret[0] = plrs[me].ssecret;
			S_Sound (NULL, CHAN_VOICE, "weapons/rocklx", 1, ATTN_NONE);
			sp_state++;
		}
	}

	else if (sp_state == 8)
	{
		if (!(bcnt&3))
			S_Sound (NULL, CHAN_VOICE, "weapons/pistol", 1, ATTN_NONE);

		cnt_time += 3;

		if (cnt_time >= plrs[me].stime / TICRATE)
			cnt_time = plrs[me].stime / TICRATE;

		cnt_par += 3;

		if (cnt_par >= wbs->partime / TICRATE)
		{
			cnt_par = wbs->partime / TICRATE;

			if (cnt_time >= plrs[me].stime / TICRATE)
			{
				S_Sound (NULL, CHAN_VOICE, "weapons/rocklx", 1, ATTN_NONE);
				sp_state++;
			}
		}
	}
	else if (sp_state == 10)
	{
		if (acceleratestage)
		{
			S_Sound (NULL, CHAN_VOICE, "weapons/shotgr", 1, ATTN_NONE);

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

	V_DrawPatchClean(SP_STATSX, SP_STATSY, &FB, kills);
	WI_drawPercent(320 - SP_STATSX, SP_STATSY, cnt_kills[0], wbs->maxkills);

	V_DrawPatchClean(SP_STATSX, SP_STATSY+lh, &FB, items);
	WI_drawPercent(320 - SP_STATSX, SP_STATSY+lh, cnt_items[0], wbs->maxitems);

	V_DrawPatchClean(SP_STATSX, SP_STATSY+2*lh, &FB, sp_secret);
	WI_drawPercent(320 - SP_STATSX, SP_STATSY+2*lh, cnt_secret[0], wbs->maxsecret);

	V_DrawPatchClean(SP_TIMEX, SP_TIMEY, &FB, time);
	WI_drawTime(160 - SP_TIMEX, SP_TIMEY, cnt_time);

	if (wbs->partime)
	{
		V_DrawPatchClean(160 + SP_TIMEX, SP_TIMEY, &FB, par);
		WI_drawTime(320 - SP_TIMEX, SP_TIMEY, cnt_par);
	}

}

void WI_checkForAccelerate (void)
{
	int   i;
	player_t  *player;

	// check for button presses to skip delays
	for (i=0, player = players ; i<MAXPLAYERS ; i++, player++)
	{
		if (playeringame[i])
		{
			if (player->cmd.ucmd.buttons & BT_ATTACK)
			{
				if (!player->attackdown)
					acceleratestage = 1;
				player->attackdown = true;
			}
			else
				player->attackdown = false;
			if (player->cmd.ucmd.buttons & BT_USE)
			{
				if (!player->usedown)
					acceleratestage = 1;
				player->usedown = true;
			}
			else
				player->usedown = false;
		}
	}
}



// Updates stuff each tick
void WI_Ticker (void)
{
	// counter for general background animation
	bcnt++;  

	if (bcnt == 1)
	{
		// intermission music
		if (gamemode == commercial)
			S_ChangeMusic ("d_dm2int", true);
		else
			S_ChangeMusic ("d_inter", true); 
	}

	WI_checkForAccelerate();

	switch (state)
	{
		case StatCount:
			if (deathmatch->value) WI_updateDeathmatchStats();
			else if (netgame) WI_updateNetgameStats();
			else WI_updateStats();
			break;
		
		case ShowNextLoc:
			WI_updateShowNextLoc();
			break;
		
		case NoState:
			WI_updateNoState();
			break;
	}

}

static int WI_CalcWidth (char *str)
{
	int w = 0;
	int lump;
	patch_t *p;
	char charname[9];

	if (!str)
		return 0;

	while (*str) {
		sprintf (charname, "FONTB%02u", toupper(*str) - 32);
		lump = W_CheckNumForName (charname);
		if (lump != -1) {
			p = W_CacheLumpNum (lump, PU_CACHE);
			w += SHORT(p->width) - 1;
		} else {
			w += 12;
		}
		str++;
	}

	return w;
}

void WI_loadData (void)
{
	int i, j;
	char name[9];
	in_anim_t *a;
	patch_t *bg;

	if ((gamemode == commercial) ||
		(gamemode == retail && wbs->epsd >= 3))
		strcpy (name, "INTERPIC");
	else 
		sprintf (name, "WIMAP%d", wbs->epsd);

	// background
	bg = W_CacheLumpName (name, PU_CACHE);
	V_AllocScreen (&background, SHORT(bg->width), SHORT(bg->height), 8);
	V_LockScreen (&background);
	V_DrawPatch (0, 0, &background, bg);
	V_UnlockScreen (&background);

	for (i = 0; i < 2; i++) {
		char *lname = (i == 0 ? wbs->lname0 : wbs->lname1);

		if (lname)
			j = W_CheckNumForName (lname);
		else
			j = -1;

		if (j >= 0) {
			lnames[i] = W_CacheLumpNum (j, PU_STATIC);
		} else {
			lnames[i] = NULL;
			lnametexts[i] = FindLevelInfo (i == 0 ? wbs->current : wbs->next)->level_name;
			lnamewidths[i] = WI_CalcWidth (lnametexts[i]);
		}
	}

	if (gamemode != commercial)
	{
		// you are here
		yah[0] = W_CacheLumpName ("WIURH0", PU_STATIC);

		// you are here (alt.)
		yah[1] = W_CacheLumpName ("WIURH1", PU_STATIC);

		// splat
		splat = W_CacheLumpName ("WISPLAT", PU_STATIC); 
		
		if (wbs->epsd < 3)
		{
			for (j=0;j<NUMANIMS[wbs->epsd];j++)
			{
				a = &anims[wbs->epsd][j];
				for (i=0;i<a->nanims;i++)
				{
					// MONDO HACK!
					if (wbs->epsd != 1 || j != 8) 
					{
						// animations
						sprintf(name, "WIA%d%.2d%.2d", wbs->epsd, j, i);  
						a->p[i] = W_CacheLumpName (name, PU_STATIC);
					}
					else
					{
						// HACK ALERT!
						a->p[i] = anims[1][4].p[i]; 
					}
				}
			}
		}
	}

	// More hacks on minus sign.
	wiminus = W_CacheLumpName ("WIMINUS", PU_STATIC); 

	for (i = 0; i < 10; i++)
	{
		 // numbers 0-9
		sprintf (name, "WINUM%d", i);	 
		num[i] = W_CacheLumpName (name, PU_STATIC);
	}

	// percent sign
	percent = W_CacheLumpName ("WIPCNT", PU_STATIC);

	// "finished"
	finished = W_CacheLumpName ("WIF", PU_STATIC);

	// "entering"
	entering = W_CacheLumpName ("WIENTER", PU_STATIC);

	// "kills"
	kills = W_CacheLumpName ("WIOSTK", PU_STATIC);	

	// "scrt"
	secret = W_CacheLumpName ("WIOSTS", PU_STATIC);

	 // "secret"
	sp_secret = W_CacheLumpName ("WISCRT2", PU_STATIC);

	// "items"
	items = W_CacheLumpName ("WIOSTI", PU_STATIC);

	// "frgs"
	frags = W_CacheLumpName ("WIFRGS", PU_STATIC);	 

	// ":"
	colon = W_CacheLumpName ("WICOLON", PU_STATIC); 

	// "time"
	time = W_CacheLumpName ("WITIME", PU_STATIC);   

	// "sucks"
	sucks = W_CacheLumpName ("WISUCKS", PU_STATIC);	

	// "par"
	par = W_CacheLumpName ("WIPAR", PU_STATIC);	 

	// "killers" (vertical)
	killers = W_CacheLumpName ("WIKILRS", PU_STATIC);

	// "victims" (horiz)
	victims = W_CacheLumpName ("WIVCTMS", PU_STATIC);

	// "total"
	total = W_CacheLumpName ("WIMSTT", PU_STATIC);	

	// your face
	star = W_CacheLumpName ("STFST01", PU_STATIC);

	// dead face
	bstar = W_CacheLumpName("STFDEAD0", PU_STATIC);    

	p = W_CacheLumpName ("STPBANY", PU_STATIC);
}

void WI_unloadData (void)
{
	int i, j;

	Z_ChangeTag (wiminus, PU_CACHE);

	for (i = 0; i < 10; i++)
		Z_ChangeTag (num[i], PU_CACHE);

	for (i = 0; i < 2; i++) {
		if (lnames[i]) {
			Z_ChangeTag (lnames[i], PU_CACHE);
			lnames[i] = NULL;
		}
	}
	
	if (gamemode != commercial)
	{
		Z_ChangeTag (yah[0], PU_CACHE);
		Z_ChangeTag (yah[1], PU_CACHE);

		Z_ChangeTag (splat, PU_CACHE);
		
		if (wbs->epsd < 3)
		{
			for (j=0;j<NUMANIMS[wbs->epsd];j++)
			{
				if (wbs->epsd != 1 || j != 8)
					for (i=0;i<anims[wbs->epsd][j].nanims;i++)
						Z_ChangeTag (anims[wbs->epsd][j].p[i], PU_CACHE);
			}
		}
	}

	Z_ChangeTag (percent, PU_CACHE);
	Z_ChangeTag (colon, PU_CACHE);
	Z_ChangeTag (finished, PU_CACHE);
	Z_ChangeTag (entering, PU_CACHE);
	Z_ChangeTag (kills, PU_CACHE);
	Z_ChangeTag (secret, PU_CACHE);
	Z_ChangeTag (sp_secret, PU_CACHE);
	Z_ChangeTag (items, PU_CACHE);
	Z_ChangeTag (frags, PU_CACHE);
	Z_ChangeTag (time, PU_CACHE);
	Z_ChangeTag (sucks, PU_CACHE);
	Z_ChangeTag (par, PU_CACHE);

	Z_ChangeTag (victims, PU_CACHE);
	Z_ChangeTag (killers, PU_CACHE);
	Z_ChangeTag (total, PU_CACHE);
	//	Z_ChangeTag(star, PU_CACHE);
	//	Z_ChangeTag(bstar, PU_CACHE);
	
	Z_ChangeTag (p, PU_CACHE);
}

void WI_Drawer (void)
{
	// If the background screen has been freed, then we really shouldn't
	// be in here. (But it happens anyway.)
	if (background.impdata)
	{
		switch (state)
		{
			case StatCount:
				if (deathmatch->value)
					WI_drawDeathmatchStats();
				else if (netgame)
					WI_drawNetgameStats();
				else
					WI_drawStats();
				break;
			
			case ShowNextLoc:
				WI_drawShowNextLoc();
				break;
			
			default:
				WI_drawNoState();
				break;
		}
	}
}


void WI_initVariables (wbstartstruct_t *wbstartstruct)
{
	wbs = wbstartstruct;

	acceleratestage = 0;
	cnt = bcnt = 0;
	me = wbs->pnum;
	plrs = wbs->plyr;
}

void WI_Start (wbstartstruct_t *wbstartstruct)
{
	WI_initVariables (wbstartstruct);
	WI_loadData ();

	if (deathmatch->value)
		WI_initDeathmatchStats();
	else if (netgame)
		WI_initNetgameStats();
	else
		WI_initStats();
	V_SetBlend (0,0,0,0);
	S_StopAllChannels ();
	SN_StopAllSequences ();
}

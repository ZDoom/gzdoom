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
//		Game completion, final screen animation.
//
//-----------------------------------------------------------------------------


static const char
rcsid[] = "$Id: f_finale.c,v 1.5 1997/02/03 21:26:34 b1 Exp $";

#include <ctype.h>
#include <math.h>

// Functions.
#include "i_system.h"
#include "m_swap.h"
#include "z_zone.h"
#include "v_video.h"
#include "w_wad.h"
#include "s_sound.h"

// Data.
#include "dstrings.h"
#include "sounds.h"

#include "doomstat.h"
#include "r_state.h"

// ?
//#include "doomstat.h"
//#include "r_local.h"
//#include "f_finale.h"

// Stage of animation:
//	0 = text, 1 = art screen, 2 = character cast
unsigned int	finalestage;

unsigned int	finalecount;

#define TEXTSPEED		2
#define TEXTWAIT		250

char*	finaletext;
char*	finaleflat;

void	F_StartCast (void);
void	F_CastTicker (void);
boolean F_CastResponder (event_t *ev);
void	F_CastDrawer (void);

//
// F_StartFinale
//
void F_StartFinale (char *music, char *flat, char *text)
{
	gameaction = ga_nothing;
	gamestate = GS_FINALE;
	viewactive = false;
	automapactive = false;

	// Okay - IWAD dependend stuff.
	// This has been changed severly, and
	//	some stuff might have changed in the process.
	// [RH] More flexible now (even more severe changes)
	//  finaleflat, finaletext, and music are now
	//  determined in G_WorldDone() based on data in
	//  a level_info_t and a cluster_info_t.

 	S_ChangeMusic (music, true);
	finaleflat = flat;
	finaletext = text;

	finalestage = 0;
	finalecount = 0;
}



boolean F_Responder (event_t *event)
{
	if (finalestage == 2)
		return F_CastResponder (event);

	return false;
}


//
// F_Ticker
//
void F_Ticker (void)
{
	int 		i;
	
	// check for skipping
	// [RH] Non-commercial can be skipped now, too
	if (finalecount > 50) {
		// go on to the next level
		// [RH] or just reveal the entire message if we're still ticking it
		for (i=0 ; i<MAXPLAYERS ; i++)
			if (players[i].cmd.ucmd.buttons)
				break;

		if (i < MAXPLAYERS) {
			if (finalecount < strlen (finaletext)*TEXTSPEED) {
				finalecount = strlen (finaletext)*TEXTSPEED;
			} else {
				if (!strncmp (level.nextmap, "EndGame", 7)) {
					if (level.nextmap[7] == 'C') {
						F_StartCast ();
					} else {
						finalecount = 0;
						finalestage = 1;
						wipegamestate = -1;		// force a wipe
						if (level.nextmap[7] == '3')
							S_StartMusic ("d_bunny");
					}
				} else {
					gameaction = ga_worlddone;
				}
			}
		}
	}
	
	// advance animation
	finalecount++;
		
	if (finalestage == 2)
	{
		F_CastTicker ();
		return;
	}
}



//
// F_TextWrite
//

#include "hu_stuff.h"
extern	patch_t *hu_font[HU_FONTSIZE];


void F_TextWrite (void)
{
	byte*		src;
	byte*		dest;
	
	int 		x,y,w;
	int 		count;
	char*		ch;
	int 		c;
	int 		cx;
	int 		cy;
	
	// erase the entire screen to a tiled background
	src = W_CacheLumpName ( finaleflat , PU_CACHE);
	dest = screens[0];
		
	for (y=0 ; y<SCREENHEIGHT ; y++)
	{
		for (x=0 ; x<SCREENWIDTH/64 ; x++)
		{
			memcpy (dest, src+((y&63)<<6), 64);
			dest += 64;
		}
		if (SCREENWIDTH&63)
		{
			memcpy (dest, src+((y&63)<<6), SCREENWIDTH&63);
			dest += (SCREENWIDTH&63);
		}
		dest += SCREENPITCH - SCREENWIDTH;
	}

	V_MarkRect (0, 0, SCREENWIDTH, SCREENHEIGHT);
	
	// draw some of the text onto the screen
	cx = 10;
	cy = 10;
	ch = finaletext;
		
	count = (finalecount - 10)/TEXTSPEED;
	if (count < 0)
		return;
	for ( ; count ; count-- )
	{
		c = *ch++;
		if (!c)
			break;
		if (c == '\n')
		{
			cx = 10;
			cy += 11;
			continue;
		}
				
		c = toupper(c) - HU_FONTSTART;
		if (c < 0 || c> HU_FONTSIZE)
		{
			cx += 4;
			continue;
		}
				
		w = SHORT (hu_font[c]->width);
		if (cx+w > SCREENWIDTH)
			break;
		V_DrawPatchClean(cx, cy, 0, hu_font[c]);
		cx+=w;
	}
		
}

//
// Final DOOM 2 animation
// Casting by id Software.
//	 in order of appearance
//
typedef struct
{
	char				*name;
	mobjtype_t	type;
} castinfo_t;

castinfo_t		castorder[] = {
	{CC_ZOMBIE, MT_POSSESSED},
	{CC_SHOTGUN, MT_SHOTGUY},
	{CC_HEAVY, MT_CHAINGUY},
	{CC_IMP, MT_TROOP},
	{CC_DEMON, MT_SERGEANT},
	{CC_LOST, MT_SKULL},
	{CC_CACO, MT_HEAD},
	{CC_HELL, MT_KNIGHT},
	{CC_BARON, MT_BRUISER},
	{CC_ARACH, MT_BABY},
	{CC_PAIN, MT_PAIN},
	{CC_REVEN, MT_UNDEAD},
	{CC_MANCU, MT_FATSO},
	{CC_ARCH, MT_VILE},
	{CC_SPIDER, MT_SPIDER},
	{CC_CYBER, MT_CYBORG},
	{CC_HERO, MT_PLAYER},

	{NULL,0}
};

int 			castnum;
int 			casttics;
state_t*		caststate;
boolean 		castdeath;
int 			castframes;
int 			castonmelee;
boolean 		castattacking;


//
// F_StartCast
//
extern	gamestate_t 	wipegamestate;


void F_StartCast (void)
{
	wipegamestate = -1; 		// force a screen wipe
	castnum = 0;
	caststate = &states[mobjinfo[castorder[castnum].type].seestate];
	casttics = caststate->tics;
	castdeath = false;
	finalestage = 2;	
	castframes = 0;
	castonmelee = 0;
	castattacking = false;
	S_ChangeMusic("d_evil", true);
}


//
// F_CastTicker
//
void F_CastTicker (void)
{
	int 		st;
	int 		sfx;
	void		*origin;

	if (--casttics > 0)
		return; 				// not time to change state yet
				
	if (caststate->tics == -1 || caststate->nextstate == S_NULL)
	{
		// switch from deathstate to next monster
		castnum++;
		castdeath = false;
		if (castorder[castnum].name == NULL)
			castnum = 0;
		if (mobjinfo[castorder[castnum].type].seesound) {
			switch (castorder[castnum].type) {
				case MT_CYBORG:
				case MT_SPIDER:
					origin = ORIGIN_SURROUND;
					break;
				default:
					origin = ORIGIN_AMBIENT;
					break;
			}
			S_StartSound (origin, mobjinfo[castorder[castnum].type].seesound);
		}
		caststate = &states[mobjinfo[castorder[castnum].type].seestate];
		castframes = 0;
	}
	else
	{
		// just advance to next state in animation
		if (caststate == &states[S_PLAY_ATK1])
			goto stopattack;	// Oh, gross hack!
		st = caststate->nextstate;
		caststate = &states[st];
		castframes++;
		
		// sound hacks....
		switch (st)
		{
		  case S_PLAY_ATK1: 	sfx = sfx_dshtgn; break;
		  case S_POSS_ATK2: 	sfx = sfx_pistol; break;
		  case S_SPOS_ATK2: 	sfx = sfx_shotgn; break;
		  case S_VILE_ATK2: 	sfx = sfx_vilatk; break;
		  case S_SKEL_FIST2:	sfx = sfx_skeswg; break;
		  case S_SKEL_FIST4:	sfx = sfx_skepch; break;
		  case S_SKEL_MISS2:	sfx = sfx_skeatk; break;
		  case S_FATT_ATK8:
		  case S_FATT_ATK5:
		  case S_FATT_ATK2: 	sfx = sfx_firsht; break;
		  case S_CPOS_ATK2:
		  case S_CPOS_ATK3:
		  case S_CPOS_ATK4: 	sfx = sfx_shotgn; break;
		  case S_TROO_ATK3: 	sfx = sfx_claw; break;
		  case S_SARG_ATK2: 	sfx = sfx_sgtatk; break;
		  case S_BOSS_ATK2:
		  case S_BOS2_ATK2:
		  case S_HEAD_ATK2: 	sfx = sfx_firsht; break;
		  case S_SKULL_ATK2:	sfx = sfx_sklatk; break;
		  case S_SPID_ATK2:
		  case S_SPID_ATK3: 	sfx = sfx_shotgn; break;
		  case S_BSPI_ATK2: 	sfx = sfx_plasma; break;
		  case S_CYBER_ATK2:
		  case S_CYBER_ATK4:
		  case S_CYBER_ATK6:	sfx = sfx_rlaunc; break;
		  case S_PAIN_ATK3: 	sfx = sfx_sklatk; break;
		  default: sfx = 0; break;
		}
				
		if (sfx)
			S_StartSound (ORIGIN_AMBIENT, sfx);
	}
		
	if (castframes == 12)
	{
		// go into attack frame
		castattacking = true;
		if (castonmelee)
			caststate=&states[mobjinfo[castorder[castnum].type].meleestate];
		else
			caststate=&states[mobjinfo[castorder[castnum].type].missilestate];
		castonmelee ^= 1;
		if (caststate == &states[S_NULL])
		{
			if (castonmelee)
				caststate=
					&states[mobjinfo[castorder[castnum].type].meleestate];
			else
				caststate=
					&states[mobjinfo[castorder[castnum].type].missilestate];
		}
	}
		
	if (castattacking)
	{
		if (castframes == 24
			||	caststate == &states[mobjinfo[castorder[castnum].type].seestate] )
		{
		  stopattack:
			castattacking = false;
			castframes = 0;
			caststate = &states[mobjinfo[castorder[castnum].type].seestate];
		}
	}
		
	casttics = caststate->tics;
	if (casttics == -1)
		casttics = 15;
}


//
// F_CastResponder
//

boolean F_CastResponder (event_t* ev)
{
	void *origin;

	if (ev->type != ev_keydown)
		return false;
				
	if (castdeath)
		return true;					// already in dying frames
				
	// go into death frame
	castdeath = true;
	caststate = &states[mobjinfo[castorder[castnum].type].deathstate];
	casttics = caststate->tics;
	castframes = 0;
	castattacking = false;
	if (mobjinfo[castorder[castnum].type].deathsound) {
		switch (castorder[castnum].type) {
			case MT_CYBORG:
			case MT_SPIDER:
				origin = ORIGIN_SURROUND;
				break;
			default:
				origin = ORIGIN_AMBIENT;
				break;
		}
		S_StartSound (origin, mobjinfo[castorder[castnum].type].deathsound);
	}
		
	return true;
}


void F_CastPrint (char* text)
{
	char*		ch;
	int 		c;
	int 		cx;
	int 		w;
	int 		width;
	
	// find width
	ch = text;
	width = 0;
		
	while (ch)
	{
		c = *ch++;
		if (!c)
			break;
		c = toupper(c) - HU_FONTSTART;
		if (c < 0 || c> HU_FONTSIZE)
		{
			width += 4;
			continue;
		}
				
		w = SHORT (hu_font[c]->width);
		width += w;
	}
	
	// draw it
	cx = (CleanWidth-width)/2;
	ch = text;
	while (ch)
	{
		c = *ch++;
		if (!c)
			break;
		c = toupper(c) - HU_FONTSTART;
		if (c < 0 || c> HU_FONTSIZE)
		{
			cx += 4;
			continue;
		}
				
		w = SHORT (hu_font[c]->width);
		V_DrawPatchCleanNoMove (cx, (SCREENHEIGHT * 180) / 200, 0, hu_font[c]);
		cx+=w;
	}
		
}

int V_DrawPatchFlipped (int, int, int, patch_t *);
//
// F_CastDrawer
//
void F_CastDrawer (void)
{
	spritedef_t*		sprdef;
	spriteframe_t*		sprframe;
	int 				lump;
	boolean 			flip;
	patch_t*			patch;
	
	// erase the entire screen to a background
	V_DrawPatchIndirect (0,0,0, W_CacheLumpName ("BOSSBACK", PU_CACHE));

	F_CastPrint (castorder[castnum].name);
	
	// draw the current frame in the middle of the screen
	sprdef = &sprites[caststate->sprite];
	sprframe = &sprdef->spriteframes[ caststate->frame & FF_FRAMEMASK];
	lump = sprframe->lump[0];
	flip = (boolean)sprframe->flip[0];
						
	patch = W_CacheLumpNum (lump+firstspritelump, PU_CACHE);
	if (flip)
		V_DrawPatchFlipped (160,170,0,patch);
	else
		V_DrawPatchIndirect (160,170,0,patch);
}


//
// F_DrawPatchCol
//
void F_DrawPatchCol (int x, patch_t *patch, int col, int scrn)
{
	column_t*	column;
	byte*		source;
	byte*		dest;
	byte*		desttop;
	unsigned	count;
	int			repeat;
	int			c;
	unsigned	step;
	unsigned	invstep;
	float		mul;
	float		fx;
	byte		p;

	// [RH] figure out how many times to repeat this column
	// (for screens wider than 320 pixels)
	mul = SCREENWIDTH / (float)320;
	fx = (float)x;
	repeat = (int)(floor (mul*(fx+1)) - floor(mul*fx));
	if (repeat == 0)
		return;

	// [RH] Remap virtual-x to real-x
	x = (int)floor (mul*x);

	// [RH] Figure out per-row fixed-point step
	step = (200<<16) / SCREENHEIGHT;
	invstep = (SCREENHEIGHT<<16) / 200;

	column = (column_t *)((byte *)patch + LONG(patch->columnofs[col]));
	desttop = screens[scrn]+x;

	// step through the posts in a column
	while (column->topdelta != 0xff )
	{
		source = (byte *)column + 3;
		dest = desttop + ((column->topdelta*invstep)>>16)*SCREENPITCH;
		count = (column->length * invstep) >> 16;
		c = 0;

		switch (repeat) {
			case 1:
				do {
					*dest = source[c>>16];
					dest += SCREENPITCH;
					c += step;
				} while (--count);
				break;
			case 2:
				do {
					p = source[c>>16];
					dest[0] = p;
					dest[1] = p;
					dest += SCREENPITCH;
					c += step;
				} while (--count);
				break;
			case 3:
				do {
					p = source[c>>16];
					dest[0] = p;
					dest[1] = p;
					dest[2] = p;
					dest += SCREENPITCH;
					c += step;
				} while (--count);
				break;
			case 4:
				do {
					p = source[c>>16];
					dest[0] = p;
					dest[1] = p;
					dest[2] = p;
					dest[3] = p;
					dest += SCREENPITCH;
					c += step;
				} while (--count);
				break;
			default:
				{
					int count2;

					do {
						p = source[c>>16];
						for (count2 = repeat; count2; count2--) {
							dest[count2] = p;
						}
						dest += SCREENPITCH;
						c += step;
					} while (--count);
				}
				break;
		}
		column = (column_t *)(	(byte *)column + column->length + 4 );
	}
}


//
// F_BunnyScroll
//
void F_BunnyScroll (void)
{
	int 		scrolled;
	int 		x;
	patch_t*	p1;
	patch_t*	p2;
	char		name[10];
	int 		stage;
	static int	laststage;
				
	p1 = W_CacheLumpName ("PFUB2", PU_LEVEL);
	p2 = W_CacheLumpName ("PFUB1", PU_LEVEL);

	V_MarkRect (0, 0, SCREENWIDTH, SCREENHEIGHT);
		
	scrolled = 320 - (finalecount-230)/2;
	if (scrolled > 320)
		scrolled = 320;
	if (scrolled < 0)
		scrolled = 0;
				
	for ( x=0 ; x<320 ; x++)
	{
		if (x+scrolled < 320)
			F_DrawPatchCol (x, p1, x+scrolled, 0);
		else
			F_DrawPatchCol (x, p2, x+scrolled - 320, 0);			
	}
		
	if (finalecount < 1130)
		return;
	if (finalecount < 1180)
	{
		V_DrawPatchIndirect ((320-13*8)/2,
					 (200-8*8)/2,0, W_CacheLumpName ("END0",PU_CACHE));
		laststage = 0;
		return;
	}
		
	stage = (finalecount-1180) / 5;
	if (stage > 6)
		stage = 6;
	if (stage > laststage)
	{
		S_StartSound (ORIGIN_AMBIENT, sfx_pistol);
		laststage = stage;
	}
		
	sprintf (name,"END%i",stage);
	V_DrawPatchIndirect ((320-13*8)/2, (200-8*8)/2,0, W_CacheLumpName (name,PU_CACHE));
}


//
// F_Drawer
//
void F_Drawer (void)
{
	switch (finalestage) {
		case 0:
			F_TextWrite ();
			break;

		case 1:
			switch (level.nextmap[7])
			{
			  case '1':
				if ( gamemode == retail )
				  V_DrawPatchIndirect (0,0,0,W_CacheLumpName("CREDIT",PU_CACHE));
				else
				  V_DrawPatchIndirect (0,0,0,W_CacheLumpName("HELP2",PU_CACHE));
				break;
			  case '2':
				V_DrawPatchIndirect (0,0,0,W_CacheLumpName("VICTORY2",PU_CACHE));
				break;
			  case '3':
				F_BunnyScroll ();
				break;
			  case '4':
				V_DrawPatchIndirect (0,0,0,W_CacheLumpName("ENDPIC",PU_CACHE));
				break;
			  // [RH] sucks
			  default:
				  V_DrawPatchIndirect (0,0,0,W_CacheLumpName("HELP2",PU_CACHE));
				break;
			}
			break;

		case 2:
			F_CastDrawer ();
			break;
	}
}
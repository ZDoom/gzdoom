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



#include <ctype.h>
#include <math.h>

// Functions.
#include "i_system.h"
#include "m_swap.h"
#include "z_zone.h"
#include "v_video.h"
#include "v_text.h"
#include "w_wad.h"
#include "s_sound.h"

// Data.
#include "dstrings.h"

#include "doomstat.h"
#include "r_state.h"

#include "hu_stuff.h"

// ?
//#include "doomstat.h"
//#include "r_local.h"
//#include "f_finale.h"

// Stage of animation:
//	0 = text, 1 = art screen, 2 = character cast
unsigned int	finalestage;

int	finalecount;

#define TEXTSPEED		2
#define TEXTWAIT		250
static int TextSpeed;	// [RH] Var for (ha ha) compatibility with old demos

char*	finaletext;
char*	finaleflat;

void	F_StartCast (void);
void	F_CastTicker (void);
BOOL	F_CastResponder (event_t *ev);
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

	if (*music == 0) {
		if (gamemode == commercial)
			S_ChangeMusic ("D_READ_M", true);
		else
			S_ChangeMusic ("D_VICTOR", true);
	} else
 		S_ChangeMusic (music, true);

	if (*flat == 0) {
		if (gamemode == commercial)
			finaleflat = "SLIME16";
		else
			finaleflat = "FLOOR4_8";
	} else
		finaleflat = flat;

	if (text)
		finaletext = text;
	else
		finaletext = "Empty message";

	finalestage = 0;
	finalecount = 0;

	// [RH] Set TextSpeed based on state of olddemo flag
	if (olddemo)
		TextSpeed = 3;		// Speed in original Doom
	else
		TextSpeed = TEXTSPEED;
}



BOOL F_Responder (event_t *event)
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
	int i;
	
	// check for skipping
	// [RH] Non-commercial can be skipped now, too
	if (finalecount > 50 && finalestage != 1) {
		// go on to the next level
		// [RH] or just reveal the entire message if we're still ticking it
		for (i=0 ; i<MAXPLAYERS ; i++)
			if (players[i].cmd.ucmd.buttons)
				break;

		if (i < MAXPLAYERS) {
			if (finalecount < (signed)(strlen (finaletext)*TextSpeed)) {
				finalecount = strlen (finaletext)*TextSpeed;
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
extern patch_t *hu_font[HU_FONTSIZE];


void F_TextWrite (void)
{
	int 		w;
	int 		count;
	char*		ch;
	int 		c;
	int 		cx;
	int 		cy;
	
	// erase the entire screen to a tiled background
	{
		int lump = R_FlatNumForName (finaleflat) + firstflat;
		V_FlatFill (0,0,screens[0].width,screens[0].height,&screens[0],
					W_CacheLumpNum (lump, PU_CACHE));
	}
	V_MarkRect (0, 0, screens[0].width, screens[0].height);
	
	// draw some of the text onto the screen
	cx = 10;
	cy = 10;
	ch = finaletext;
		
	if (finalecount < 11)
		return;

	count = (finalecount - 10)/TextSpeed;
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
		if (cx+w > screens[0].width)
			break;
		V_DrawPatchClean(cx, cy, &screens[0], hu_font[c]);
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
	char		*name;
	mobjtype_t	type;
} castinfo_t;

castinfo_t		castorder[] = {
	{NULL, MT_POSSESSED},
	{NULL, MT_SHOTGUY},
	{NULL, MT_CHAINGUY},
	{NULL, MT_TROOP},
	{NULL, MT_SERGEANT},
	{NULL, MT_SKULL},
	{NULL, MT_HEAD},
	{NULL, MT_KNIGHT},
	{NULL, MT_BRUISER},
	{NULL, MT_BABY},
	{NULL, MT_PAIN},
	{NULL, MT_UNDEAD},
	{NULL, MT_FATSO},
	{NULL, MT_VILE},
	{NULL, MT_SPIDER},
	{NULL, MT_CYBORG},
	{NULL, MT_PLAYER},

	{NULL,0}
};

int 			castnum;
int 			casttics;
int				castsprite;	// [RH] For overriding the player sprite with a skin
state_t*		caststate;
BOOL	 		castdeath;
int 			castframes;
int 			castonmelee;
BOOL	 		castattacking;


//
// F_StartCast
//
extern	gamestate_t 	wipegamestate;


void F_StartCast (void)
{
	// [RH] Set the names for the cast
	castorder[0].name = CC_ZOMBIE;
	castorder[1].name = CC_SHOTGUN;
	castorder[2].name = CC_HEAVY;
	castorder[3].name = CC_IMP;
	castorder[4].name = CC_DEMON;
	castorder[5].name = CC_LOST;
	castorder[6].name = CC_CACO;
	castorder[7].name = CC_HELL;
	castorder[8].name = CC_BARON;
	castorder[9].name = CC_ARACH;
	castorder[10].name = CC_PAIN;
	castorder[11].name = CC_REVEN;
	castorder[12].name = CC_MANCU;
	castorder[13].name = CC_ARCH;
	castorder[14].name = CC_SPIDER;
	castorder[15].name = CC_CYBER;
	castorder[16].name = CC_HERO;

	wipegamestate = -1; 		// force a screen wipe
	castnum = 0;
	caststate = &states[mobjinfo[castorder[castnum].type].seestate];
	if (castorder[castnum].type == MT_PLAYER)
		castsprite = skins[players[consoleplayer].userinfo.skin].sprite;
	else
		castsprite = caststate->sprite;
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
	int st;
	void *origin;

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
			S_StartSound (origin, mobjinfo[castorder[castnum].type].seesound, 90);
		}
		caststate = &states[mobjinfo[castorder[castnum].type].seestate];
		if (castorder[castnum].type == MT_PLAYER)
			castsprite = skins[players[consoleplayer].userinfo.skin].sprite;
		else
			castsprite = caststate->sprite;
		castframes = 0;
	}
	else
	{
		char *sfx;

		// just advance to next state in animation
		if (caststate == &states[S_PLAY_ATK1])
			goto stopattack;	// Oh, gross hack!
		st = caststate->nextstate;
		caststate = &states[st];
		castframes++;
		
		// sound hacks....
		switch (st)
		{
		  case S_PLAY_ATK1: 	sfx = "weapons/sshotf"; break;
		  case S_POSS_ATK2: 	sfx = "grunt/attack"; break;
		  case S_SPOS_ATK2: 	sfx = "shotguy/attack"; break;
		  case S_VILE_ATK2: 	sfx = "vile/start"; break;
		  case S_SKEL_FIST2:	sfx = "skeleton/swing"; break;
		  case S_SKEL_FIST4:	sfx = "skeleton/melee"; break;
		  case S_SKEL_MISS2:	sfx = "skeleton/attack"; break;
		  case S_FATT_ATK8:
		  case S_FATT_ATK5:
		  case S_FATT_ATK2: 	sfx = "fatso/attack"; break;
		  case S_CPOS_ATK2:
		  case S_CPOS_ATK3:
		  case S_CPOS_ATK4: 	sfx = "chainguy/attack"; break;
		  case S_TROO_ATK3: 	sfx = "imp/attack"; break;
		  case S_SARG_ATK2: 	sfx = "demon/melee"; break;
		  case S_BOSS_ATK2:
		  case S_BOS2_ATK2:
		  case S_HEAD_ATK2: 	sfx = "caco/attack"; break;
		  case S_SKULL_ATK2:	sfx = "skull/melee"; break;
		  case S_SPID_ATK2:
		  case S_SPID_ATK3: 	sfx = "spider/attack"; break;
		  case S_BSPI_ATK2: 	sfx = "baby/attack"; break;
		  case S_CYBER_ATK2:
		  case S_CYBER_ATK4:
		  case S_CYBER_ATK6:	sfx = "weapons/rocklf"; break;
		  case S_PAIN_ATK3: 	sfx = "skull/melee"; break;
		  default: sfx = 0; break;
		}
				
		if (sfx)
			S_StartSound (ORIGIN_AMBIENT, sfx, 90);
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

BOOL F_CastResponder (event_t* ev)
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
		if (castorder[castnum].type == MT_PLAYER) {
			static const char sndtemplate[] = "player/%s/death1";
			static const char *genders[] = { "male", "female", "neuter" };
			char nametest[128];
			int sndnum;

			sprintf (nametest, sndtemplate, skins[players[consoleplayer].userinfo.skin].name);
			sndnum = S_FindSound (nametest);
			if (sndnum == -1) {
				sprintf (nametest, sndtemplate, genders);
				sndnum = S_FindSound (nametest);
				if (sndnum == -1)
					sndnum = S_FindSound ("player/male/death1");
			}
			S_StartSfx (origin, sndnum, 90);
		} else
			S_StartSound (origin, mobjinfo[castorder[castnum].type].deathsound, 90);
	}
		
	return true;
}


void F_CastPrint (char *text)
{
	char text2[80], *t2 = text2;

	while (*text)
		*t2++ = *text++ ^ 0x80;
	*t2 = 0;
	V_DrawTextClean ((screens[0].width - V_StringWidth (text2) * CleanXfac) >> 1,
					 (screens[0].height * 180) / 200, text2);
}

int V_DrawPatchFlipped (int, int, screen_t *, patch_t *);
//
// F_CastDrawer
//
void F_CastDrawer (void)
{
	spritedef_t*		sprdef;
	spriteframe_t*		sprframe;
	int 				lump;
	BOOL	 			flip;
	patch_t*			patch;
	
	// erase the entire screen to a background
	V_DrawPatchIndirect (0,0,&screens[0], W_CacheLumpName ("BOSSBACK", PU_CACHE));

	F_CastPrint (castorder[castnum].name);
	
	// draw the current frame in the middle of the screen
	sprdef = &sprites[castsprite];
	sprframe = &sprdef->spriteframes[ caststate->frame & FF_FRAMEMASK];
	lump = sprframe->lump[0];
	flip = (BOOL)sprframe->flip[0];
						
	patch = W_CacheLumpNum (lump, PU_CACHE);
	if (flip)
		V_DrawPatchFlipped (160,170,&screens[0],patch);
	else
		V_DrawPatchIndirect (160,170,&screens[0],patch);
}


//
// F_DrawPatchCol
//
void F_DrawPatchCol (int x, patch_t *patch, int col, screen_t *scrn)
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
	int			pitch;

	// [RH] figure out how many times to repeat this column
	// (for screens wider than 320 pixels)
	mul = scrn->width / (float)320;
	fx = (float)x;
	repeat = (int)(floor (mul*(fx+1)) - floor(mul*fx));
	if (repeat == 0)
		return;

	// [RH] Remap virtual-x to real-x
	x = (int)floor (mul*x);

	// [RH] Figure out per-row fixed-point step
	step = (200<<16) / scrn->height;
	invstep = (scrn->height<<16) / 200;

	column = (column_t *)((byte *)patch + LONG(patch->columnofs[col]));
	desttop = scrn->buffer + x;
	pitch = scrn->pitch;

	// step through the posts in a column
	while (column->topdelta != 0xff )
	{
		source = (byte *)column + 3;
		dest = desttop + ((column->topdelta*invstep)>>16)*pitch;
		count = (column->length * invstep) >> 16;
		c = 0;

		switch (repeat) {
			case 1:
				do {
					*dest = source[c>>16];
					dest += pitch;
					c += step;
				} while (--count);
				break;
			case 2:
				do {
					p = source[c>>16];
					dest[0] = p;
					dest[1] = p;
					dest += pitch;
					c += step;
				} while (--count);
				break;
			case 3:
				do {
					p = source[c>>16];
					dest[0] = p;
					dest[1] = p;
					dest[2] = p;
					dest += pitch;
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
					dest += pitch;
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
						dest += pitch;
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

	V_MarkRect (0, 0, screens[0].width, screens[0].height);
		
	scrolled = 320 - (finalecount-230)/2;
	if (scrolled > 320)
		scrolled = 320;
	if (scrolled < 0)
		scrolled = 0;
				
	for ( x=0 ; x<320 ; x++)
	{
		if (x+scrolled < 320)
			F_DrawPatchCol (x, p1, x+scrolled, &screens[0]);
		else
			F_DrawPatchCol (x, p2, x+scrolled - 320, &screens[0]);			
	}
		
	if (finalecount < 1130)
		return;
	if (finalecount < 1180)
	{
		V_DrawPatchIndirect ((320-13*8)/2,
					 (200-8*8)/2,&screens[0], W_CacheLumpName ("END0",PU_CACHE));
		laststage = 0;
		return;
	}
		
	stage = (finalecount-1180) / 5;
	if (stage > 6)
		stage = 6;
	if (stage > laststage)
	{
		S_StartSound (ORIGIN_AMBIENT, "weapons/pistol", 64);
		laststage = stage;
	}
		
	sprintf (name,"END%i",stage);
	V_DrawPatchIndirect ((320-13*8)/2, (200-8*8)/2,&screens[0], W_CacheLumpName (name,PU_CACHE));
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
				  V_DrawPatchIndirect (0,0,&screens[0],W_CacheLumpName("CREDIT",PU_CACHE));
				else
				  V_DrawPatchIndirect (0,0,&screens[0],W_CacheLumpName("HELP2",PU_CACHE));
				break;
			  case '2':
				V_DrawPatchIndirect (0,0,&screens[0],W_CacheLumpName("VICTORY2",PU_CACHE));
				break;
			  case '3':
				F_BunnyScroll ();
				break;
			  case '4':
				V_DrawPatchIndirect (0,0,&screens[0],W_CacheLumpName("ENDPIC",PU_CACHE));
				break;
			  // [RH] sucks
			  default:
				  V_DrawPatchIndirect (0,0,&screens[0],W_CacheLumpName("HELP2",PU_CACHE));
				break;
			}
			break;

		case 2:
			F_CastDrawer ();
			break;
	}
}
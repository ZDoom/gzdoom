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

#include "i_system.h"
#include "m_swap.h"
#include "z_zone.h"
#include "v_video.h"
#include "v_text.h"
#include "w_wad.h"
#include "s_sound.h"
#include "dstrings.h"
#include "doomstat.h"
#include "r_state.h"
#include "hu_stuff.h"

#include "gi.h"

// Stage of animation:
//	0 = text, 1 = art screen, 2 = character cast
unsigned int	finalestage;

int	finalecount;

#define TEXTSPEED		2
#define TEXTWAIT		250

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

	if (*music == 0)
		S_ChangeMusic (gameinfo.finaleMusic,
			!(gameinfo.flags & GI_NOLOOPFINALEMUSIC));
	else
 		S_ChangeMusic (music, !(gameinfo.flags & GI_NOLOOPFINALEMUSIC));

	if (*flat == 0)
		finaleflat = gameinfo.finaleFlat;
	else
		finaleflat = flat;

	if (text)
		finaletext = text;
	else
		finaletext = "Empty message";

	finalestage = 0;
	finalecount = 0;
	V_SetBlend (0,0,0,0);
	S_StopAllChannels ();
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
		for (i = 0; i < MAXPLAYERS; i++)
			if (players[i].cmd.ucmd.buttons)
				break;

		if (i < MAXPLAYERS) {
			if (finalecount < (signed)(strlen (finaletext)*TEXTSPEED)) {
				finalecount = strlen (finaletext)*TEXTSPEED;
			} else {
				if (!strncmp (level.nextmap, "EndGame", 7)) {
					if (level.nextmap[7] == 'C') {
						F_StartCast ();
					} else {
						finalecount = 0;
						finalestage = 1;
						wipegamestate = GS_FORCEWIPE;
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
		int lump = W_CheckNumForName (finaleflat, ns_flats);
		if (lump >= 0)
		{
			screen->FlatFill (0,0, screen->width, screen->height,
						(byte *)W_CacheLumpNum (lump, PU_CACHE));
		}
		else
		{
			screen->Clear (0, 0, screen->width, screen->height, 0);
		}
	}
	V_MarkRect (0, 0, screen->width, screen->height);
	
	// draw some of the text onto the screen
	cx = 10;
	cy = 10;
	ch = finaletext;
		
	if (finalecount < 11)
		return;

	count = (finalecount - 10)/TEXTSPEED;
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
		if (cx+w > screen->width)
			break;
		screen->DrawPatchClean (hu_font[c], cx, cy);
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

	{NULL,}
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

	wipegamestate = GS_FORCEWIPE;
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
	int atten;

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
			if (mobjinfo[castorder[castnum].type].flags2 & MF2_BOSS)
				atten = ATTN_SURROUND;
			else
				atten = ATTN_NONE;
			S_Sound (CHAN_VOICE, mobjinfo[castorder[castnum].type].seesound, 1, atten);
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
				
		if (sfx) {
			S_StopAllChannels ();
			S_Sound (CHAN_WEAPON, sfx, 1, ATTN_NONE);
		}
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
	int attn;

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
				attn = ATTN_SURROUND;
				break;
			default:
				attn = ATTN_NONE;
				break;
		}
		if (castorder[castnum].type == MT_PLAYER) {
			static const char sndtemplate[] = "player/%s/death1";
			static const char *genders[] = { "male", "female", "cyborg" };
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
			S_SoundID (CHAN_VOICE, sndnum, 1, ATTN_NONE);
		} else
			S_Sound (CHAN_VOICE, mobjinfo[castorder[castnum].type].deathsound, 1, attn);
	}
		
	return true;
}

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
	screen->DrawPatchIndirect ((patch_t *)W_CacheLumpName ("BOSSBACK", PU_CACHE), 0, 0);

	screen->DrawTextClean (CR_RED,
		(screen->width - V_StringWidth (castorder[castnum].name) * CleanXfac)/2,
		(screen->height * 180) / 200, castorder[castnum].name);
	
	// draw the current frame in the middle of the screen
	sprdef = &sprites[castsprite];
	sprframe = &sprdef->spriteframes[caststate->frame & FF_FRAMEMASK];
	lump = sprframe->lump[0];
	flip = (BOOL)sprframe->flip[0];
						
	patch = (patch_t *)W_CacheLumpNum (lump, PU_CACHE);
	if (flip)
		screen->DrawPatchFlipped (patch, 160, 170);
	else
		screen->DrawPatchIndirect (patch, 160, 170);
}


//
// F_DrawPatchCol
//
void F_DrawPatchCol (int x, const patch_t *patch, int col, const DCanvas *scrn)
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
				
	p1 = (patch_t *)W_CacheLumpName ("PFUB2", PU_LEVEL);
	p2 = (patch_t *)W_CacheLumpName ("PFUB1", PU_LEVEL);

	V_MarkRect (0, 0, screen->width, screen->height);
		
	scrolled = 320 - (finalecount-230)/2;
	if (scrolled > 320)
		scrolled = 320;
	if (scrolled < 0)
		scrolled = 0;
				
	for ( x=0 ; x<320 ; x++)
	{
		if (x+scrolled < 320)
			F_DrawPatchCol (x, p1, x+scrolled, screen);
		else
			F_DrawPatchCol (x, p2, x+scrolled - 320, screen);			
	}
		
	if (finalecount < 1130)
		return;
	if (finalecount < 1180)
	{
		screen->DrawPatchIndirect ((patch_t *)W_CacheLumpName ("END0",PU_CACHE),
			(320-13*8)/2, (200-8*8)/2);
		laststage = 0;
		return;
	}
		
	stage = (finalecount-1180) / 5;
	if (stage > 6)
		stage = 6;
	if (stage > laststage)
	{
		S_Sound (CHAN_WEAPON, "weapons/pistol", 1, ATTN_NONE);
		laststage = stage;
	}
		
	sprintf (name,"END%i",stage);
	screen->DrawPatchIndirect ((patch_t *)W_CacheLumpName (name,PU_CACHE),
		(320-13*8)/2, (200-8*8)/2);
}


//
// F_Drawer
//
void F_Drawer (void)
{
	switch (finalestage)
	{
		case 0:
			F_TextWrite ();
			break;

		case 1:
			switch (level.nextmap[7])
			{
				default:
				case '1':
					screen->DrawPatchIndirect ((patch_t *)W_CacheLumpName (gameinfo.finalePage1, PU_CACHE), 0, 0);
					break;
				case '2':
					screen->DrawPatchIndirect ((patch_t *)W_CacheLumpName (gameinfo.finalePage2, PU_CACHE), 0, 0);
					break;
				case '3':
					F_BunnyScroll ();
					break;
				case '4':
					screen->DrawPatchIndirect ((patch_t *)W_CacheLumpName (gameinfo.finalePage3, PU_CACHE), 0, 0);
					break;
			}
			break;

		case 2:
			F_CastDrawer ();
			break;
	}
}
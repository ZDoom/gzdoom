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
#include "i_video.h"
#include "v_text.h"
#include "w_wad.h"
#include "s_sound.h"
#include "gstrings.h"
#include "doomstat.h"
#include "r_state.h"
#include "hu_stuff.h"

#include "gi.h"

static void FadePic ();
static void GetFinaleText (const char *msgLumpName);

// Stage of animation:
//	0 = text
//	1 = art screen
//	2 = underwater screen
//	3 = character cast
//	4 = Heretic title
static unsigned int	FinaleStage;

static size_t FinaleCount, FinaleEndCount;

static int TEXTSPEED;
#define TEXTWAIT		250

static int FinaleSequence;
static BYTE *DemonBuffer;
static SBYTE FadeDir;
static bool FinaleHasPic;

static const char *FinaleText;
static size_t FinaleTextLen;
static char *FinaleFlat;

void	F_StartCast (void);
void	F_CastTicker (void);
BOOL	F_CastResponder (event_t *ev);
void	F_CastDrawer (void);

//
// F_StartFinale
//
void F_StartFinale (char *music, int musicorder, int cdtrack, unsigned int cdid, char *flat, char *text,
					BOOL textInLump, BOOL finalePic)
{
	bool ending = strncmp (level.nextmap, "enDSeQ", 6) == 0;
	bool loopmusic = ending ? !(gameinfo.flags & GI_NOLOOPFINALEMUSIC) : true;
	gameaction = ga_nothing;
	gamestate = GS_FINALE;
	viewactive = false;
	automapactive = false;

	// Okay - IWAD dependend stuff.
	// This has been changed severly, and some stuff might have changed in the process.
	//
	// [RH] More flexible now (even more severe changes)
	//  FinaleFlat, FinaleText, and music are now determined in G_WorldDone() based on
	//	data in a level_info_t and a cluster_info_t.

	if (cdtrack == 0 || !S_ChangeCDMusic (cdtrack, cdid))
	{
		if (music == NULL)
		{
			S_ChangeMusic (gameinfo.finaleMusic, 0, loopmusic);
		}
		else
		{
 			S_ChangeMusic (music, musicorder, loopmusic);
		}
	}

	FinaleFlat = (flat != NULL && *flat != 0) ? flat : gameinfo.finaleFlat;

	if (textInLump)
	{
		GetFinaleText (text);
	}
	else
	{
		FinaleText = (text != NULL) ? text : "Empty message";
		FinaleTextLen = strlen (FinaleText);
	}

	FinaleStage = 0;
	V_SetBlend (0,0,0,0);
	TEXTSPEED = 2;

	if (ending)
	{
		TEXTSPEED = 3;	// Slow the text to its original rate to match the music.
		FinaleSequence = *((WORD *)&level.nextmap[6]);
		S_ChangeMusic ("hall", 0, loopmusic);
		if (EndSequences[FinaleSequence].EndType == END_Chess)
		{
			FinaleStage = 10;
			GetFinaleText ("win1msg");
			V_SetBlend (0,0,0,256);
		}
	}

	FinaleHasPic = !!finalePic;
	FinaleCount = 0;
	FinaleEndCount = 70;
	FadeDir = -1;
	S_StopAllChannels ();
}

BOOL F_Responder (event_t *event)
{
	if (FinaleStage == 3)
	{
		return F_CastResponder (event);
	}
	else if (FinaleStage == 2 && event->type == EV_KeyDown)
	{ // We're showing the water pic; make any key kick to demo mode
		FinaleStage = 4;
		V_ForceBlend (0, 0, 0, 0);
		return true;
	}

	return false;
}


//
// F_Ticker
//
void F_Ticker ()
{
	int i;
	
	// check for skipping
	// [RH] Non-commercial can be skipped now, too
	if (FinaleCount > 50 && FinaleStage == 0)
	{
		// go on to the next level
		// [RH] or just reveal the entire message if we're still ticking it
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if ((players[i].cmd.ucmd.buttons ^ players[i].oldbuttons)
				&& ((players[i].cmd.ucmd.buttons & players[i].oldbuttons)
					== players[i].oldbuttons))
			{
				break;
			}
		}

		if (i < MAXPLAYERS ||
			(gamemode != commercial && FinaleCount > FinaleTextLen*TEXTSPEED+TEXTWAIT))
		{
			if (FinaleCount < FinaleTextLen*TEXTSPEED)
			{
				FinaleCount = FinaleTextLen*TEXTSPEED;
			}
			else
			{
				if (strncmp (level.nextmap, "enDSeQ", 6) == 0)
				{
					FinaleSequence = *((WORD *)&level.nextmap[6]);
					if (EndSequences[FinaleSequence].EndType == END_Cast)
					{
						F_StartCast ();
					}
					else
					{
						FinaleCount = 0;
						FinaleStage = 1;
						wipegamestate = GS_FORCEWIPE;
						if (EndSequences[FinaleSequence].EndType == END_Bunny)
						{
							S_StartMusic ("d_bunny");
						}
						else if (EndSequences[FinaleSequence].EndType == END_Demon)
						{
							DemonBuffer = (byte *)Z_Malloc (128000, PU_LEVEL, &DemonBuffer);
							W_ReadLump (W_GetNumForName ("FINAL2"), DemonBuffer);
							W_ReadLump (W_GetNumForName ("FINAL1"), DemonBuffer+64000);
						}
					}
				}
				else
				{
					gameaction = ga_worlddone;
				}
			}
		}

		for (i = 0; i < MAXPLAYERS; i++)
		{
			players[i].oldbuttons = players[i].cmd.ucmd.buttons;
		}
	}
	else if (FinaleStage >= 10)
	{
		if (FinaleStage < 15 && FinaleCount >= FinaleEndCount)
		{
			FinaleCount = 0;
			FinaleStage++;
			switch (FinaleStage)
			{
			case 11: // Text 1
				FinaleEndCount = FinaleTextLen*TEXTSPEED+TEXTWAIT;
				break;

			case 12: // Pic 2, Text 2
				GetFinaleText ("win2msg");
				FinaleEndCount = FinaleTextLen*TEXTSPEED+TEXTWAIT;
				S_ChangeMusic ("orb", 0, !(gameinfo.flags & GI_NOLOOPFINALEMUSIC));
				break;

			case 13: // Pic 2 -- Fade out
				FinaleEndCount = 70;
				FadeDir = 1;
				break;

			case 14: // Pic 3 -- Fade in
				FinaleEndCount = 71;
				FadeDir = -1;
				S_ChangeMusic ("chess", 0, !(gameinfo.flags & GI_NOLOOPFINALEMUSIC));
				break;

			case 15: // Pic 3, Text 3
				GetFinaleText ("win3msg");
				FinaleEndCount = FinaleTextLen*TEXTSPEED+TEXTWAIT;
				break;
			}
			return;
		}
		if (FinaleStage == 10 || FinaleStage == 13 || FinaleStage == 14)
		{
			FadePic ();
		}
	}
	
	// advance animation
	FinaleCount++;
		
	if (FinaleStage == 3)
	{
		F_CastTicker ();
		return;
	}
}

//===========================================================================
//
// FadePic
//
//===========================================================================

static void FadePic ()
{
	int blend = 256*FinaleCount/70;

	if (FadeDir < 0)
	{
		blend = 256 - blend;
	}
	V_SetBlend (0,0,0,blend);
}

//
// F_TextWrite
//
void F_TextWrite (void)
{
	int w, h, xo, yo;
	size_t count;
	const char *ch;
	int c;
	int cx;
	int cy;
	const byte *data;
	const byte *range;
	int leftmargin;
	int rowheight;
	bool scale;
	
	if (FinaleCount < 11)
		return;

	// draw some of the text onto the screen
	leftmargin = (gameinfo.gametype == GAME_Doom ? 10 : 20) - 160;
	rowheight = screen->Font->GetHeight () +
		(gameinfo.gametype == GAME_Doom ? 3 : -1);
	scale = (CleanXfac != 1 || CleanYfac != 1);

	cx = leftmargin;
	if (FinaleStage == 15)
	{
		cy = 135 - 100;
	}
	else
	{
		cy = (gameinfo.gametype == GAME_Doom ? 10 : 5) - 100;
	}
	ch = FinaleText;
		
	count = (FinaleCount - 10)/TEXTSPEED;
	range = screen->Font->GetColorTranslation (CR_UNTRANSLATED);

	for ( ; count ; count-- )
	{
		c = *ch++;
		if (!c)
			break;
		if (c == '\n')
		{
			cx = leftmargin;
			cy += rowheight;
			continue;
		}

		data = screen->Font->GetChar (c, &w, &h, &xo, &yo);
		if (cx+w > SCREENWIDTH)
			continue;
		if (data != NULL)
		{
			if (scale)
			{
				screen->ScaleMaskedBlock (
					(cx - xo) * CleanXfac + SCREENWIDTH / 2,
					(cy - yo) * CleanYfac + SCREENHEIGHT / 2,
					w, h, w * CleanXfac, h * CleanYfac, data, range);
			}
			else
			{
				screen->DrawMaskedBlock (
					(cx - xo) + SCREENWIDTH / 2,
					(cy - yo) + SCREENHEIGHT / 2,
					w, h, data, range);
			}
		}
		cx += w;
	}
		
}

//
// Final DOOM 2 animation
// Casting by id Software.
//	 in order of appearance
//
typedef struct
{
	int				name;
	const char		*type;
	const AActor	*info;
} castinfo_t;

castinfo_t castorder[] =
{
	{CC_ZOMBIE,		"ZombieMan"},
	{CC_SHOTGUN,	"ShotgunGuy"},
	{CC_HEAVY,		"ChaingunGuy"},
	{CC_IMP,		"DoomImp"},
	{CC_DEMON,		"Demon"},
	{CC_LOST,		"LostSoul"},
	{CC_CACO,		"Cacodemon"},
	{CC_HELL,		"HellKnight"},
	{CC_BARON,		"BaronOfHell"},
	{CC_ARACH,		"Arachnotron"},
	{CC_PAIN,		"PainElemental"},
	{CC_REVEN,		"Revenant"},
	{CC_MANCU,		"Fatso"},
	{CC_ARCH,		"Archvile"},
	{CC_SPIDER,		"SpiderMastermind"},
	{CC_CYBER,		"Cyberdemon"},
	{CC_HERO,		"DoomPlayer"},

	{0, NULL}
};

struct
{
	const char *type;
	byte melee;
	byte ofs;
	const char *sound;
	FState *match;
} atkstates[] =
{
	{ "DoomPlayer", 0, 0, "weapons/sshotf" },
	{ "ZombieMan", 0, 1, "grunt/attack" },
	{ "ShotgunGuy", 0, 1, "shotguy/attack" },
	{ "Archvile", 0, 1, "vile/start" },
	{ "Revenant", 1, 1, "skeleton/swing" },
	{ "Revenant", 1, 3, "skeleton/melee" },
	{ "Revenant", 0, 1, "skeleton/attack" },
	{ "Fatso", 0, 1, "fatso/attack" },
	{ "Fatso", 0, 4, "fatso/attack" },
	{ "Fatso", 0, 7, "fatso/attack" },
	{ "ChaingunGuy", 0, 1, "chainguy/attack" },
	{ "ChaingunGuy", 0, 2, "chainguy/attack" },
	{ "ChaingunGuy", 0, 3, "chainguy/attack" },
	{ "DoomImp", 0, 2, "imp/attack" },
	{ "Demon", 1, 1, "demon/melee" },
	{ "BaronOfHell", 0, 1, "baron/attack" },
	{ "HellKnight", 0, 1, "baron/attack" },
	{ "Cacodemon", 0, 1, "caco/attack" },
	{ "LostSoul", 0, 1, "skull/melee" },
	{ "SpiderMastermind", 0, 1, "spider/attack" },
	{ "SpiderMastermind", 0, 2, "spider/attack" },
	{ "Arachnotron", 0, 1, "baby/attack" },
	{ "Cyberdemon", 0, 1, "weapons/rocklf" },
	{ "Cyberdemon", 0, 3, "weapons/rocklf" },
	{ "Cyberdemon", 0, 5, "weapons/rocklf" },
	{ "PainElemental", 0, 2, "skull/melee" },
	{ NULL }
};

int 			castnum;
int 			casttics;
int				castsprite;	// [RH] For overriding the player sprite with a skin
FState*			caststate;
BOOL	 		castdeath;
int 			castframes;
int 			castonmelee;
BOOL	 		castattacking;

static FState *advplayerstate;

//
// F_StartCast
//
extern	gamestate_t 	wipegamestate;


void F_StartCast (void)
{
	const TypeInfo *type;
	int i;

	// [RH] Set the names and defaults for the cast
	for (i = 0; castorder[i].type; i++)
	{
		type = TypeInfo::FindType (castorder[i].type);
		if (type == NULL)
			castorder[i].info = GetDefault<AActor>();
		else
			castorder[i].info = GetDefaultByType (type);
	}

	for (i = 0; atkstates[i].type; i++)
	{
		type = TypeInfo::FindType (atkstates[i].type);
		if (type != NULL)
		{
			if (atkstates[i].melee)
				atkstates[i].match = ((AActor *)(type->ActorInfo->Defaults))->MeleeState + atkstates[i].ofs;
			else
				atkstates[i].match = ((AActor *)(type->ActorInfo->Defaults))->MissileState + atkstates[i].ofs;
		}
		else
		{
			atkstates[i].match = NULL;
		}
	}

	type = TypeInfo::FindType ("DoomPlayer");
	if (type != NULL)
		advplayerstate = ((AActor *)(type->ActorInfo->Defaults))->MissileState;

	wipegamestate = GS_FORCEWIPE;
	castnum = 0;
	caststate = castorder[castnum].info->SeeState;
	if (castnum == 16)
		castsprite = skins[players[consoleplayer].userinfo.skin].sprite;
	else
		castsprite = caststate->sprite.index;
	casttics = caststate->GetTics ();
	castdeath = false;
	FinaleStage = 3;
	castframes = 0;
	castonmelee = 0;
	castattacking = false;
	S_ChangeMusic ("d_evil");
}


//
// F_CastTicker
//
void F_CastTicker (void)
{
	int atten;

	if (--casttics > 0)
		return; 				// not time to change state yet
				
	if (caststate->GetTics() == -1 || caststate->GetNextState() == NULL)
	{
		// switch from deathstate to next monster
		castnum++;
		castdeath = false;
		if (castorder[castnum].name == 0)
			castnum = 0;
		if (castorder[castnum].info->SeeSound)
		{
			if (castorder[castnum].info->flags2 & MF2_BOSS)
				atten = ATTN_SURROUND;
			else
				atten = ATTN_NONE;
			S_SoundID (CHAN_VOICE, castorder[castnum].info->SeeSound, 1, atten);
		}
		caststate = castorder[castnum].info->SeeState;
		if (castnum == 16)
			castsprite = skins[players[consoleplayer].userinfo.skin].sprite;
		else
			castsprite = caststate->sprite.index;
		castframes = 0;
	}
	else
	{
		// just advance to next state in animation
		if (caststate == advplayerstate)
			goto stopattack;	// Oh, gross hack!
		caststate = caststate->GetNextState();
		castframes++;
		
		// sound hacks....
		if (caststate)
		{
			int i;

			for (i = 0; atkstates[i].type; i++)
			{
				if (atkstates[i].match == caststate)
				{
					S_StopAllChannels ();
					S_Sound (CHAN_WEAPON, atkstates[i].sound, 1, ATTN_NONE);
					break;
				}
			}
		}
	}
		
	if (castframes == 12)
	{
		// go into attack frame
		castattacking = true;
		if (castonmelee)
			caststate = castorder[castnum].info->MeleeState;
		else
			caststate = castorder[castnum].info->MissileState;
		castonmelee ^= 1;
		if (caststate == NULL)
		{
			if (castonmelee)
				caststate = castorder[castnum].info->MeleeState;
			else
				caststate = castorder[castnum].info->MissileState;
		}
	}
		
	if (castattacking)
	{
		if (castframes == 24
			|| caststate == castorder[castnum].info->SeeState )
		{
		  stopattack:
			castattacking = false;
			castframes = 0;
			caststate = castorder[castnum].info->SeeState;
		}
	}
		
	casttics = caststate->GetTics();
	if (casttics == -1)
		casttics = 15;
}


//
// F_CastResponder
//

BOOL F_CastResponder (event_t* ev)
{
	int attn;

	if (ev->type != EV_KeyDown)
		return false;
				
	if (castdeath)
		return true;					// already in dying frames
				
	// go into death frame
	castdeath = true;
	caststate = castorder[castnum].info->DeathState;
	casttics = caststate->GetTics();
	castframes = 0;
	castattacking = false;
	if (castorder[castnum].info->DeathSound)
	{
		if (castnum == 15 || castnum == 14)
			attn = ATTN_SURROUND;
		else
			attn = ATTN_NONE;
		if (castnum == 16)
		{
			static const char sndtemplate[] = "player/%s/death1";
			static const char *genders[] = { "male", "female", "cyborg" };
			char nametest[128];
			int sndnum;

			sprintf (nametest, sndtemplate, skins[players[consoleplayer].userinfo.skin].name);
			sndnum = S_FindSound (nametest);
			if (sndnum == 0)
			{
				sprintf (nametest, sndtemplate, genders[players[consoleplayer].userinfo.gender]);
				sndnum = S_FindSound (nametest);
				if (sndnum == 0)
				{
					sndnum = S_FindSound ("player/male/death1");
				}
			}
			S_SoundID (CHAN_VOICE, sndnum, 1, ATTN_NONE);
		}
		else
		{
			S_SoundID (CHAN_VOICE, castorder[castnum].info->DeathSound, 1, attn);
		}
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
		(SCREENWIDTH - screen->StringWidth (GStrings(castorder[castnum].name)) * CleanXfac)/2,
		(SCREENHEIGHT * 180) / 200, GStrings(castorder[castnum].name));
	
	// draw the current frame in the middle of the screen
	sprdef = &sprites[castsprite];
	sprframe = &SpriteFrames[sprdef->spriteframes + caststate->GetFrame()];
	lump = sprframe->lump[0];
	flip = (BOOL)sprframe->flip & 1;
						
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
	mul = SCREENWIDTH / 320.f;
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
	desttop = scrn->GetBuffer() + x;
	pitch = scrn->GetPitch();

	// step through the posts in a column
	int top = 0;	// DeePsea tall patches support
	while (column->topdelta != 0xff)
	{
		if (column->topdelta <= top)
		{
			top += column->topdelta;
		}
		else
		{
			top = column->topdelta;
		}
		if (column->length != 0)
		{
			count = (column->length * invstep) >> 16;
			source = (byte *)column + 3;
			dest = desttop + ((top*invstep)>>16)*pitch;
			c = 0;

			switch (repeat)
			{
			case 1:
				do
				{
					*dest = source[c>>16];
					dest += pitch;
					c += step;
				} while (--count);
				break;
			case 2:
				do
				{
					p = source[c>>16];
					dest[0] = p;
					dest[1] = p;
					dest += pitch;
					c += step;
				} while (--count);
				break;
			case 3:
				do
				{
					p = source[c>>16];
					dest[0] = p;
					dest[1] = p;
					dest[2] = p;
					dest += pitch;
					c += step;
				} while (--count);
				break;
			case 4:
				do
				{
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

					do
					{
						p = source[c>>16];
						for (count2 = repeat; count2; count2--)
						{
							dest[count2] = p;
						}
						dest += pitch;
						c += step;
					} while (--count);
				}
				break;
			}
		}
		column = (column_t *)((byte *)column + column->length + 4);
	}
}

/*
==================
=
= F_DemonScroll
=
==================
*/

void F_DemonScroll ()
{
	static int yval = 0;
	static size_t nextscroll = 0;

	if (FinaleCount < 70)
	{
		screen->DrawPageBlock (DemonBuffer+64000);
		nextscroll = FinaleCount;
		yval = 0;
		return;
	}
	if (yval < 64000)
	{
		screen->DrawPageBlock (DemonBuffer+64000-yval);
		if (FinaleCount >= nextscroll)
		{
			yval += 320;
			nextscroll = FinaleCount+3;
		}
	}
	else
	{ //else, we'll just sit here and wait, for now
		screen->DrawPageBlock (DemonBuffer);
	}
}

/*
==================
=
= F_DrawUnderwater
=
==================
*/

void F_DrawUnderwater(void)
{
	extern bool menuactive;

	switch (FinaleStage)
	{
	case 1:
		{
		PalEntry *palette;
		byte *orgpal;
		int i;

		orgpal = (byte *)W_CacheLumpName ("E2PAL", PU_CACHE);
		palette = screen->GetPalette ();
		for (i = 256; i > 0; i--, orgpal += 3)
		{
			*palette++ = PalEntry (orgpal[0], orgpal[1], orgpal[2]);
		}
		screen->UpdatePalette ();
		screen->DrawPageBlock ((byte *)W_CacheLumpName ("E2END", PU_CACHE));
		FinaleStage = 2;
		}

		// intentional fall-through
	case 2:
		paused = false;
		menuactive = false;
		break;

	case 4:
		{
		PalEntry *palette;
		byte *orgpal;
		int i;

		orgpal = (byte *)W_CacheLumpName ("PLAYPAL", PU_CACHE);
		palette = screen->GetPalette ();
		for (i = 256; i > 0; i--, orgpal += 3)
		{
			*palette++ = PalEntry (orgpal[0], orgpal[1], orgpal[2]);
		}
		screen->UpdatePalette ();
		screen->DrawPageBlock ((byte *)W_CacheLumpName ("TITLE", PU_CACHE));
		break;
		}
	}
}

/*
==================
=
= F_BunnyScroll
=
==================
*/
void F_BunnyScroll (void)
{
	int 		scrolled;
	int 		x;
	patch_t*	p1;
	patch_t*	p2;
	char		name[10];
	size_t 		stage;
	static size_t laststage;

	p1 = (patch_t *)W_CacheLumpName ("PFUB2", PU_LEVEL);
	p2 = (patch_t *)W_CacheLumpName ("PFUB1", PU_LEVEL);

	V_MarkRect (0, 0, SCREENWIDTH, SCREENHEIGHT);

	scrolled = 320 - ((signed)FinaleCount-230)/2;
	if (scrolled > 320)
		scrolled = 320;
	else if (scrolled < 0)
		scrolled = 0;

	for (x = 0; x < 320; x++)
	{
		if (x+scrolled < 320)
			F_DrawPatchCol (x, p1, x+scrolled, screen);
		else
			F_DrawPatchCol (x, p2, x+scrolled - 320, screen);
	}

	if (FinaleCount < 1130)
		return;
	if (FinaleCount < 1180)
	{
		screen->DrawPatchIndirect ((patch_t *)W_CacheLumpName ("END0",PU_CACHE),
			(320-13*8)/2, (200-8*8)/2);
		laststage = 0;
		return;
	}

	stage = (FinaleCount-1180) / 5;
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
	const char *picname = NULL;

	switch (FinaleStage)
	{
	case 0: // Intermission or end-of-episode text
		// erase the entire screen to a tiled background (or picture)
		if (!FinaleHasPic)
		{
			int lump = W_CheckNumForName (FinaleFlat, ns_flats);
			if (lump >= 0)
			{
				screen->FlatFill (0,0, SCREENWIDTH, SCREENHEIGHT,
					(byte *)W_CacheLumpNum (lump, PU_CACHE));
			}
			else
			{
				screen->Clear (0, 0, SCREENWIDTH, SCREENHEIGHT, 0);
			}
		}
		else
		{
			picname = FinaleFlat;
		}
		V_MarkRect (0, 0, SCREENWIDTH, SCREENHEIGHT);
		break;

	case 1:
	case 2:
	case 4:
		switch (EndSequences[FinaleSequence].EndType)
		{
		default:
		case END_Pic1:
			picname = gameinfo.finalePage1;
			screen->DrawPatchIndirect ((patch_t *)W_CacheLumpName (gameinfo.finalePage1, PU_CACHE), 0, 0);
			break;
		case END_Pic2:
			picname = gameinfo.finalePage2;
			screen->DrawPatchIndirect ((patch_t *)W_CacheLumpName (gameinfo.finalePage2, PU_CACHE), 0, 0);
			break;
		case END_Pic3:
			picname = gameinfo.finalePage3;
			screen->DrawPatchIndirect ((patch_t *)W_CacheLumpName (gameinfo.finalePage3, PU_CACHE), 0, 0);
			break;
		case END_Pic:
			picname = EndSequences[FinaleSequence].PicName;
			break;
		case END_Bunny:
			F_BunnyScroll ();
			break;
		case END_Underwater:
			F_DrawUnderwater ();
			break;
		case END_Demon:
			F_DemonScroll ();
			break;
		}
		break;

	case 3:
		F_CastDrawer ();
		break;

	case 10:
	case 11:
		picname = "FINALE1";
		break;

	case 12:
	case 13:
		picname = "FINALE2";
		break;

	case 14:
	case 15:
		picname = "FINALE3";
		break;
	}
	if (picname != NULL)
	{
		patch_t *patch;

		if (gameinfo.flags & GI_PAGESARERAW)
		{
			byte *data = (byte *)W_CacheLumpName (picname, PU_CACHE);
			screen->DrawPageBlock (data);
		}
		else
		{
			patch = (patch_t *)W_CacheLumpName (picname, PU_CACHE);
			screen->DrawPatchIndirect (patch, 0, 0);
		}
		if (FinaleStage >= 14)
		{ // Chess pic, draw the correct character graphic
			if (multiplayer)
			{
				patch = (patch_t *)W_CacheLumpName ("chessall", PU_CACHE);
				screen->DrawPatchIndirect (patch, 20, 0);
			}
			else if (players[consoleplayer].userinfo.PlayerClass > 0)
			{
				patch = (patch_t *)W_CacheLumpNum (W_GetNumForName ("chessc")+
					players[consoleplayer].userinfo.PlayerClass-1, PU_CACHE);
				screen->DrawPatchIndirect (patch, 60, 0);
			}
		}
	}
	switch (FinaleStage)
	{
	case 0:
	case 11:
	case 12:
	case 15:
		F_TextWrite ();
		break;
	}
}

//==========================================================================
//
// GetFinaleText
//
//==========================================================================

static void GetFinaleText (const char *msgLumpName)
{
	int msgLump;

	msgLump = W_CheckNumForName(msgLumpName);
	if (msgLump != -1)
	{
		FinaleTextLen = W_LumpLength(msgLump);
		FinaleText = (char *)W_CacheLumpNum (msgLump, PU_LEVEL);
	}
	else
	{
		FinaleTextLen = strlen (msgLumpName) + sizeof("Unknown message ") + 1;
		FinaleText = (const char *)Z_Malloc (FinaleTextLen, PU_LEVEL, 0);
		sprintf (const_cast<char *>(FinaleText), "Unknown message %s", msgLumpName);
	}
}

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
#include "v_video.h"
#include "i_video.h"
#include "v_text.h"
#include "w_wad.h"
#include "s_sound.h"
#include "gstrings.h"
#include "doomstat.h"
#include "r_state.h"
#include "r_draw.h"
#include "hu_stuff.h"
#include "cmdlib.h"
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

static char *FinaleText;
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
					BOOL textInLump, BOOL finalePic, BOOL lookupText)
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
		const char *from = (text != NULL) ? text : "Empty message";
		FinaleTextLen = strlen (from) + 1;
		FinaleText = new char[FinaleTextLen];
		memcpy (FinaleText, from, FinaleTextLen);
	}
	if (lookupText)
	{
		int strnum = GStrings.FindString (FinaleText);
		if (strnum >= 0)
		{
			ReplaceString (&FinaleText, GStrings(strnum));
			FinaleTextLen = strlen (FinaleText) + 1;
		}
	}

	FinaleStage = 0;
	V_SetBlend (0,0,0,0);
	TEXTSPEED = 2;

	if (ending)
	{
		FinaleSequence = *((WORD *)&level.nextmap[6]);
		if (EndSequences[FinaleSequence].EndType == END_Chess)
		{
			TEXTSPEED = 3;	// Slow the text to its original rate to match the music.
			S_ChangeMusic ("hall", 0, loopmusic);
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

void F_EndFinale ()
{
	if (FinaleText != NULL)
	{
		delete[] FinaleText;
		FinaleText = NULL;
		FinaleTextLen = 0;
	}
	if (DemonBuffer != NULL)
	{
		delete[] DemonBuffer;
		DemonBuffer = NULL;
	}
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
	bool interrupt = false;

	// check for skipping
	for (i = 0; i < MAXPLAYERS; i++)
	{
		// Only for buttons going down
		if (!interrupt)
		{
			for (size_t j = 0; j < sizeof(players[i].cmd.ucmd.buttons)*8; ++j)
			{
				if (((players[i].cmd.ucmd.buttons >> j) & 1) &&
					!((players[i].oldbuttons >> j) & 1))
				{
					interrupt = true;
					break;
				}
			}
		}
		players[i].oldbuttons = players[i].cmd.ucmd.buttons;
	}
	
	// [RH] Non-commercial can be skipped now, too
	if (FinaleStage == 0)
	{
		if (interrupt ||
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
							if (DemonBuffer == NULL)
							{
								DemonBuffer = new BYTE[128000];
								Wads.ReadLump (Wads.GetNumForName ("FINAL2"), DemonBuffer);
								Wads.ReadLump (Wads.GetNumForName ("FINAL1"), DemonBuffer+64000);
							}
						}
					}
				}
				else
				{
					gameaction = ga_worlddone;
				}
			}
		}
	}
	else if (FinaleStage >= 10)
	{
		// Hexen chess ending with three pages of text.
		// [RH] This can be interrupted to speed it up.
		if (interrupt)
		{
			if (FinaleStage == 11 || FinaleStage == 12 || FinaleStage == 15)
			{ // Stages that display text
				if (FinaleCount < FinaleEndCount-TEXTWAIT)
				{
					FinaleCount = FinaleEndCount-TEXTWAIT;
				}
				else
				{
					FinaleCount = FinaleEndCount;
				}
			}
			else if (FinaleCount < 69)
			{ // Stages that fade pictures
				FinaleCount = 69;
			}
		}
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
	FTexture *pic;
	int w;
	size_t count;
	const char *ch;
	int c;
	int cx;
	int cy;
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

		pic = screen->Font->GetChar (c, &w);
		if (cx+w > SCREENWIDTH)
			continue;
		if (pic != NULL)
		{
			if (scale)
			{
				screen->DrawTexture (pic,
					cx + 320 / 2,
					cy + 200 / 2,
					DTA_Translation, range,
					DTA_Clean, true,
					TAG_DONE);
			}
			else
			{
				screen->DrawTexture (pic,
					cx + 320 / 2,
					cy + 200 / 2,
					DTA_Translation, range,
					TAG_DONE);
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
int				castsprite;			// [RH] For overriding the player sprite with a skin
const BYTE *	casttranslation;	// [RH] Draw "our hero" with their chosen suit color
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
	castsprite = caststate->sprite.index;
	casttranslation = NULL;
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

	if (--casttics > 0 && caststate != NULL)
		return; 				// not time to change state yet
				
	if (caststate == NULL || caststate->GetTics() == -1 || caststate->GetNextState() == NULL)
	{
		// switch from deathstate to next monster
		do
		{
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
			// [RH] Skip monsters that have been hacked to no longer have attack states
			if (castorder[castnum].info->MissileState == NULL &&
				castorder[castnum].info->MeleeState == NULL)
			{
				caststate = NULL;
			}
		}
		while (caststate == NULL);
		if (castnum == 16)
		{
			castsprite = skins[players[consoleplayer].userinfo.skin].sprite;
			casttranslation = translationtables[TRANSLATION_Players] + 256*consoleplayer;
		}
		else
		{
			castsprite = caststate->sprite.index;
			casttranslation = NULL;
		}
		castframes = 0;
	}
	else
	{
		// sound hacks....
		if (caststate != NULL)
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

		// just advance to next state in animation
		if (caststate == advplayerstate)
			goto stopattack;	// Oh, gross hack!

		caststate = caststate->GetNextState();
		castframes++;
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
	if (ev->type != EV_KeyDown)
		return false;
				
	if (castdeath)
		return true;					// already in dying frames
				
	// go into death frame
	castdeath = true;
	caststate = castorder[castnum].info->DeathState;
	if (caststate != NULL)
	{
		casttics = caststate->GetTics();
		castframes = 0;
		castattacking = false;
		if (castnum == 16)
		{
			//int id = S_LookupPlayerSound (
			S_Sound (players[consoleplayer].mo, CHAN_VOICE, "*death", 1, ATTN_NONE);
		}
		else if (castorder[castnum].info->DeathSound)
		{
			S_SoundID (CHAN_VOICE, castorder[castnum].info->DeathSound, 1,
				castnum == 15 || castnum == 14 ? ATTN_SURROUND : ATTN_NONE);
		}
	}
		
	return true;
}

//
// F_CastDrawer
//
void F_CastDrawer (void)
{
	spriteframe_t*		sprframe;
	FTexture*			pic;
	
	// erase the entire screen to a background
	screen->DrawTexture (TexMan["BOSSBACK"], 0, 0,
		DTA_DestWidth, screen->GetWidth(),
		DTA_DestHeight, screen->GetHeight(),
		TAG_DONE);

	screen->DrawText (CR_RED,
		(SCREENWIDTH - SmallFont->StringWidth (GStrings(castorder[castnum].name)) * CleanXfac)/2,
		(SCREENHEIGHT * 180) / 200,
		GStrings(castorder[castnum].name),
		DTA_CleanNoMove, true, TAG_DONE);
	
	// draw the current frame in the middle of the screen
	if (caststate != NULL)
	{
		sprframe = &SpriteFrames[sprites[castsprite].spriteframes + caststate->GetFrame()];
		pic = TexMan(sprframe->Texture[0]);

		screen->DrawTexture (pic, 160, 170,
			DTA_320x200, true,
			DTA_FlipX, sprframe->Flip & 1,
			DTA_Translation, casttranslation,
			TAG_DONE);
	}
}


//
// F_DrawPatchCol
//
void F_DrawPatchCol (int x, FTexture *tex, int col, const DCanvas *scrn)
{
	const BYTE *column;
	const FTexture::Span *spans;
	const BYTE *source;
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
	if (repeat <= 0)
		return;

	// [RH] Remap virtual-x to real-x
	x = (int)floor (mul*x);

	// [RH] Figure out per-row fixed-point step
	step = (200<<16) / SCREENHEIGHT;
	invstep = (SCREENHEIGHT<<16) / 200;

	column = tex->GetColumn (col, &spans);
	desttop = scrn->GetBuffer() + x;
	pitch = scrn->GetPitch();

	// step through the posts in a column
	while (spans->Length != 0)
	{
		count = (spans->Length * invstep) >> 16;
		source = column + spans->TopOffset;
		dest = desttop + ((spans->TopOffset*invstep)>>16)*pitch;
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
					dest[0] = p;
					dest[1] = p;
					dest[2] = p;
					dest[3] = p;
					for (count2 = repeat-4; count2; count2--)
					{
						dest[count2+4] = p;
					}
					dest += pitch;
					c += step;
				} while (--count);
			}
			break;
		}
		spans++;
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
	extern EMenuState menuactive;

	switch (FinaleStage)
	{
	case 1:
		{
		PalEntry *palette;
		FMemLump lump;
		const byte *orgpal;
		int i;

		lump = Wads.ReadLump ("E2PAL");
		orgpal = (byte *)lump.GetMem();
		palette = screen->GetPalette ();
		for (i = 256; i > 0; i--, orgpal += 3)
		{
			*palette++ = PalEntry (orgpal[0], orgpal[1], orgpal[2]);
		}
		screen->UpdatePalette ();

		lump = Wads.ReadLump ("E2END");
		screen->DrawPageBlock ((byte *)lump.GetMem());
		FinaleStage = 2;
		}

		// intentional fall-through
	case 2:
		paused = false;
		menuactive = MENU_Off;
		break;

	case 4:
		{
		PalEntry *palette;
		FMemLump lump;
		int i;

		palette = screen->GetPalette ();
		for (i = 0; i < 256; ++i)
		{
			palette[i] = GPalette.BaseColors[i];
		}
		screen->UpdatePalette ();

		screen->UpdatePalette ();
		lump = Wads.ReadLump ("TITLE");
		screen->DrawPageBlock ((byte *)lump.GetMem());
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
	FTexture	*p1;
	FTexture	*p2;
	char		name[10];
	size_t 		stage;
	static size_t laststage;

	V_MarkRect (0, 0, SCREENWIDTH, SCREENHEIGHT);

	scrolled = 320 - ((signed)FinaleCount-230)/2;
	if (scrolled > 320)
		scrolled = 320;
	else if (scrolled < 0)
		scrolled = 0;

	p1 = TexMan["PFUB2"];
	p2 = TexMan["PFUB1"];

	for (x = 0; x < 320; x++)
	{
		if (x+scrolled < 320)
			F_DrawPatchCol (x, p1, x+scrolled, screen);
		else
			F_DrawPatchCol (x, p2, x+scrolled - 320, screen);
	}

	if (FinaleCount < 1130)
	{
		return;
	}
	if (FinaleCount < 1180)
	{
		screen->DrawTexture (TexMan["END0"], (320-13*8)/2, (200-8*8)/2, DTA_320x200, true, TAG_DONE);
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

	sprintf (name, "END%i", stage);
	screen->DrawTexture (TexMan[name], (320-13*8)/2, (200-8*8)/2, DTA_320x200, true, TAG_DONE);
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
			int picnum = TexMan.CheckForTexture (FinaleFlat, FTexture::TEX_Flat, FTextureManager::TEXMAN_Overridable);
			if (picnum >= 0)
			{
				screen->FlatFill (0,0, SCREENWIDTH, SCREENHEIGHT, TexMan(picnum));
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
			screen->DrawTexture (TexMan[picname], 0, 0, DTA_320x200, true, TAG_DONE);
			break;
		case END_Pic2:
			picname = gameinfo.finalePage2;
			screen->DrawTexture (TexMan[picname], 0, 0, DTA_320x200, true, TAG_DONE);
			break;
		case END_Pic3:
			picname = gameinfo.finalePage3;
			screen->DrawTexture (TexMan[picname], 0, 0, DTA_320x200, true, TAG_DONE);
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
		screen->DrawTexture (TexMan[picname], 0, 0,
			DTA_DestWidth, screen->GetWidth(),
			DTA_DestHeight, screen->GetHeight(),
			TAG_DONE);
		if (FinaleStage >= 14)
		{ // Chess pic, draw the correct character graphic
			if (multiplayer)
			{
				screen->DrawTexture (TexMan["CHESSALL"], 20, 0, DTA_320x200, true, TAG_DONE);
			}
			else if (players[consoleplayer].CurrentPlayerClass > 0)
			{
				picname = players[consoleplayer].CurrentPlayerClass == 1 ? "CHESSC" : "CHESSM";
				screen->DrawTexture (TexMan[picname], 60, 0, DTA_320x200, true, TAG_DONE);
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

	msgLump = Wads.CheckNumForName(msgLumpName);
	if (msgLump != -1)
	{
		FinaleTextLen = Wads.LumpLength(msgLump);
		FinaleText = new char[FinaleTextLen];
		Wads.ReadLump (msgLump, FinaleText);
	}
	else
	{
		FinaleTextLen = strlen (msgLumpName) + sizeof("Unknown message ") + 1;
		FinaleText = new char[FinaleTextLen];
		sprintf (const_cast<char *>(FinaleText), "Unknown message %s", msgLumpName);
	}
}

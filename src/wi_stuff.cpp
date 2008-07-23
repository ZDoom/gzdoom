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

// Enhancements by Graf Zahl

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
#include "sc_man.h"
#include "v_text.h"
#include "gi.h"
#include "r_translate.h"

// States for the intermission
typedef enum
{
	NoState = -1,
	StatCount,
	ShowNextLoc,
	LeavingIntermission
} stateenum_t;

CVAR (Bool, wi_percents, true, CVAR_ARCHIVE)
CVAR (Bool, wi_showtotaltime, true, CVAR_ARCHIVE)
CVAR (Bool, wi_noautostartmap, false, CVAR_ARCHIVE)


void WI_loadData ();
void WI_unloadData ();

#define NEXTSTAGE		(gameinfo.gametype == GAME_Doom ? "weapons/rocklx" : "doors/dr1_clos")
#define PASTSTATS		(gameinfo.gametype == GAME_Doom ? "weapons/shotgr" : "plats/pt1_stop")

// GLOBAL LOCATIONS
#define WI_TITLEY				2
#define WI_SPACINGY 			33

// SINGPLE-PLAYER STUFF
#define SP_STATSX				50
#define SP_STATSY				50

#define SP_TIMEX				8
#define SP_TIMEY				(200-32)


// NET GAME STUFF
#define NG_STATSY				50
#define NG_STATSX				(32 + star->GetWidth()/2 + 32*!dofrags)

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

// These animation variables, structures, etc. are used for the
// DOOM/Ultimate DOOM intermission screen animations.  This is
// totally different from any sprite or texture/flat animations
typedef enum
{
	ANIM_ALWAYS,	// determined by patch entry
	ANIM_PIC,		// continuous

	// condition bitflags
	ANIM_IFVISITED=8,
	ANIM_IFNOTVISITED=16,
	ANIM_IFENTERING=32,
	ANIM_IFNOTENTERING=64,
	ANIM_IFLEAVING=128,
	ANIM_IFNOTLEAVING=256,
	ANIM_IFTRAVELLING=512,
	ANIM_IFNOTTRAVELLING=1024,

	ANIM_TYPE=7,
	ANIM_CONDITION=~7,
		
} animenum_t;

typedef struct
{
	int x, y;
} yahpt_t;

struct lnode_t
{
	int   x;       // x/y coordinate pair structure
	int   y;
	char level[9];
} ;


#define FACEBACKOFS 4


//
// Animation.
// There is another anim_t used in p_spec.
// (which is why I have renamed this one!)
//

#define MAX_ANIMATION_FRAMES 20
typedef struct
{
	int			type;	// Made an int so I can use '|'
	int 		period;	// period in tics between animations
	int 		nanims;	// number of animation frames
	yahpt_t 	loc;	// location of animation
	int 		data;	// ALWAYS: n/a, RANDOM: period deviation (<256)
	FTexture *	p[MAX_ANIMATION_FRAMES];	// actual graphics for frames of animations

	// following must be initialized to zero before use!
	int 		nexttic;	// next value of bcnt (used in conjunction with period)
	int 		ctr;		// next frame number to animate
	int 		state;		// used by RANDOM and LEVEL when animating
	
	char		levelname[9];
	char		levelname2[9];
} in_anim_t;

static TArray<lnode_t> lnodes;
static TArray<in_anim_t> anims;


//
// GENERAL DATA
//

//
// Locally used stuff.
//


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
static int				cnt_kills[MAXPLAYERS];
static int				cnt_items[MAXPLAYERS];
static int				cnt_secret[MAXPLAYERS];
static int				cnt_time;
static int				cnt_total_time;
static int				cnt_par;
static int				cnt_pause;
static bool				noautostartmap;

//
//		GRAPHICS
//

static TArray<FTexture *> yah; 		// You Are Here graphic
static FTexture* 		splat;		// splat
static FTexture* 		percent;	// %, : graphics
static FTexture* 		colon;
static FTexture*		slash;
static FTexture* 		num[10];	// 0-9 graphic
static FTexture* 		wiminus;	// minus sign
static FTexture* 		finished;	// "Finished!" graphics
static FTexture* 		entering;	// "Entering" graphic
static FTexture* 		sp_secret;	// "secret"
static FTexture* 		kills;		// "Kills", "Scrt", "Items", "Frags"
static FTexture* 		secret;
static FTexture* 		items;
static FTexture* 		frags;
static FTexture* 		timepic;	// Time sucks.
static FTexture* 		par;
static FTexture* 		sucks;
static FTexture* 		killers;	// "killers", "victims"
static FTexture* 		victims;
static FTexture* 		total;		// "Total", your face, your dead face
static FTexture* 		star;
static FTexture* 		bstar;
static FTexture* 		p;			// Player graphic
static FTexture*		lnames[2];	// Name graphics of each level (centered)

// [RH] Info to dynamically generate the level name graphics
static const char		*lnametexts[2];

static FTexture			*background;

//
// CODE
//

// ====================================================================
//
// Background script commands
//
// ====================================================================

static const char *WI_Cmd[]={
	"Background",
	"Splat",
	"Pointer",
	"Spots",

	"IfEntering",
	"IfNotEntering",
	"IfVisited",
	"IfNotVisited",
	"IfLeaving",
	"IfNotLeaving",
	"IfTravelling",
	"IfNotTravelling",

	"Animation",
	"Pic",

	"NoAutostartMap",

	NULL
};

//====================================================================
// 
//	Loads the background - either from a single texture
//	or an intermission lump.
//	Unfortunately the texture manager is incapable of recognizing text
//	files so if you use a script you have to prefix its name by '$' in 
//  MAPINFO.
//
//====================================================================
static bool IsExMy(const char * name)
{
	// Only check for the first 3 episodes. They are the only ones with default intermission scripts.
	// Level names can be upper- and lower case so use tolower to check!
	return (tolower(name[0])=='e' && name[1]>='1' && name[1]<='3' && tolower(name[2])=='m');
}

void WI_LoadBackground(bool isenterpic)
{
	const char * lumpname = NULL;
	char buffer[10];
	in_anim_t an;
	lnode_t pt;
	FTextureID texture;

	bcnt=0;

	texture.SetInvalid();
	if (isenterpic)
	{
		level_info_t * li = FindLevelInfo(wbs->next);
		if (li != NULL) lumpname = li->enterpic;
	}
	else
	{
		lumpname = level.info->exitpic;
	}

	// Try to get a default if nothing specified
	if (lumpname == NULL || lumpname[0]==0) 
	{
		lumpname = NULL;
		switch(gameinfo.gametype)
		{
		case GAME_Doom:
			if (gamemode != commercial)
			{
				const char *level = isenterpic ? wbs->next : wbs->current;
				if (IsExMy(level))
				{
					mysnprintf(buffer, countof(buffer), "$IN_EPI%c", level[1]);
					lumpname = buffer;
				}
			}
			if (!lumpname) 
			{
				if (isenterpic) 
				{
					// One special case needs to be handled here!
					// If going from E1-E3 to E4 the default should be used, not the exit pic.

					// Not if the exit pic is user defined!
					if (level.info->exitpic != NULL && level.info->exitpic[0]!=0) return;

					// E1-E3 need special treatment when playing Doom 1.
					if (gamemode!=commercial)
					{
						// not if the last level is not from the first 3 episodes
						if (!IsExMy(wbs->current)) return;

						// not if the next level is one of the first 3 episodes
						if (IsExMy(wbs->next)) return;
					}
				}
				lumpname = "INTERPIC";
			}
			break;

		case GAME_Heretic:
			if (isenterpic)
			{
				if (IsExMy(wbs->next))
				{
					mysnprintf(buffer, countof(buffer), "$IN_HTC%c", wbs->next[1]);
					lumpname = buffer;
				}
			}
			if (!lumpname) 
			{
				if (isenterpic) return;
				lumpname = "FLOOR16";
			}
			break;

		case GAME_Hexen:
			if (isenterpic) return;
			lumpname = "INTERPIC";
			break;

		case GAME_Strife:
		default:
			// Strife doesn't have an intermission pic so choose something neutral.
			if (isenterpic) return;
			lumpname = gameinfo.borderFlat;
			break;
		}
	}
	if (lumpname == NULL)
	{
		// shouldn't happen!
		background = NULL;
		return;
	}

	lnodes.Clear();
	anims.Clear();
	yah.Clear();
	splat = NULL;

	// a name with a starting '$' indicates an intermission script
	if (*lumpname!='$')
	{
		texture = TexMan.CheckForTexture(lumpname, FTexture::TEX_MiscPatch, FTextureManager::TEXMAN_TryAny);
	}
	else
	{
		int lumpnum=Wads.CheckNumForFullName(lumpname+1, true);
		if (lumpnum>=0)
		{
			FScanner sc(lumpnum);
			while (sc.GetString())
			{
				memset(&an,0,sizeof(an));
				int caseval = sc.MustMatchString(WI_Cmd);
				switch(caseval)
				{
				case 0:		// Background
					sc.MustGetString();
					texture=TexMan.CheckForTexture(sc.String, FTexture::TEX_MiscPatch,FTextureManager::TEXMAN_TryAny);
					break;

				case 1:		// Splat
					sc.MustGetString();
					splat=TexMan[sc.String];
					break;

				case 2:		// Pointers
					while (sc.GetString() && !sc.Crossed)
					{
						yah.Push(TexMan[sc.String]);
					}
					if (sc.Crossed) sc.UnGet();
					break;

				case 3:		// Spots
					sc.MustGetStringName("{");
					while (!sc.CheckString("}"))
					{
						sc.MustGetString();
						strncpy(pt.level, sc.String,8);
						pt.level[8] = 0;
						sc.MustGetNumber();
						pt.x = sc.Number;
						sc.MustGetNumber();
						pt.y = sc.Number;
						lnodes.Push(pt);
					}
					break;

				case 4:		// IfEntering
					an.type = ANIM_IFENTERING;
					goto readanimation;

				case 5:		// IfEntering
					an.type = ANIM_IFNOTENTERING;
					goto readanimation;

				case 6:		// IfVisited
					an.type = ANIM_IFVISITED;
					goto readanimation;

				case 7:		// IfNotVisited
					an.type = ANIM_IFNOTVISITED;
					goto readanimation;

				case 8:		// IfLeaving
					an.type = ANIM_IFLEAVING;
					goto readanimation;
				
				case 9:		// IfNotLeaving
					an.type = ANIM_IFNOTLEAVING;
					goto readanimation;

				case 10:	// IfTravelling
					an.type = ANIM_IFTRAVELLING;
					sc.MustGetString();
					strncpy(an.levelname2, sc.String, 8);
					an.levelname2[8] = 0;
					goto readanimation;

				case 11:	// IfNotTravelling
					an.type = ANIM_IFTRAVELLING;
					sc.MustGetString();
					strncpy(an.levelname2, sc.String, 8);
					an.levelname2[8] = 0;
					goto readanimation;

				case 14:	// NoAutostartMap
					noautostartmap = true;
					break;

				readanimation:
					sc.MustGetString();
					strncpy(an.levelname, sc.String, 8);
					an.levelname[8] = 0;
					sc.MustGetString();
					caseval=sc.MustMatchString(WI_Cmd);

				default:
					switch (caseval)
					{
					case 12:	// Animation
						an.type |= ANIM_ALWAYS;
						sc.MustGetNumber();
						an.loc.x = sc.Number;
						sc.MustGetNumber();
						an.loc.y = sc.Number;
						sc.MustGetNumber();
						an.period = sc.Number;
						an.nexttic = 1 + (M_Random() % an.period);
						if (sc.GetString())
						{
							if (sc.Compare("ONCE"))
							{
								an.data = 1;
							}
							else
							{
								sc.UnGet();
							}
						}
						if (!sc.CheckString("{"))
						{
							sc.MustGetString();
							an.p[an.nanims++] = TexMan[sc.String];
						}
						else
						{
							while (!sc.CheckString("}"))
							{
								sc.MustGetString();
								if (an.nanims<MAX_ANIMATION_FRAMES)
									an.p[an.nanims++] = TexMan[sc.String];
							}
						}
						an.ctr = -1;
						anims.Push(an);
						break;

					case 13:		// Pic
						an.type |= ANIM_PIC;
						sc.MustGetNumber();
						an.loc.x = sc.Number;
						sc.MustGetNumber();
						an.loc.y = sc.Number;
						sc.MustGetString();
						an.p[0] = TexMan[sc.String];
						anims.Push(an);
						break;

					default:
						sc.ScriptError("Unknown token %s in intermission script", sc.String);
					}
				}
			}
		}
		else 
		{
			Printf("Intermission script %s not found!\n", lumpname+1);
			texture = TexMan.GetTexture("INTERPIC", FTexture::TEX_MiscPatch);
		}
	}
	background=TexMan[texture];
}

//====================================================================
// 
//	made this more generic and configurable through a script
//	Removed all the ugly special case handling for different game modes
//
//====================================================================

void WI_updateAnimatedBack()
{
	unsigned int i;

	for(i=0;i<anims.Size();i++)
	{
		in_anim_t * a = &anims[i];
		switch (a->type & ANIM_TYPE)
		{
		case ANIM_ALWAYS:
			if (bcnt >= a->nexttic)
			{
				if (++a->ctr >= a->nanims) 
				{
					if (a->data==0) a->ctr = 0;
					else a->ctr--;
				}
				a->nexttic = bcnt + a->period;
			}
			break;
			
		case ANIM_PIC:
			a->ctr = 0;
			break;
			
		}
	}
}

//====================================================================
// 
//	Draws the background including all animations
//
//====================================================================

void WI_drawBackground()
{
	unsigned int i;
	int animwidth=320;		// For a flat fill or clear background scale animations to 320x200
	int animheight=200;

	if (background)
	{
		// background
		if (background->UseType == FTexture::TEX_MiscPatch)
		{
			// scale all animations below to fit the size of the base pic
			// The base pic is always scaled to fit the screen so this allows
			// placing the animations precisely where they belong on the base pic
			animwidth = background->GetWidth();
			animheight = background->GetHeight();
			screen->FillBorder (NULL);
			screen->DrawTexture(background, 0, 0, DTA_VirtualWidth, animwidth,
				DTA_VirtualHeight, animheight, TAG_DONE);
		}
		else 
		{
			screen->FlatFill(0, 0, SCREENWIDTH, SCREENHEIGHT, background);
		}
	}
	else 
	{
		screen->Clear(0,0, SCREENWIDTH, SCREENHEIGHT, 0, 0);
	}

	for(i=0;i<anims.Size();i++)
	{
		in_anim_t * a = &anims[i];
		level_info_t * li;

		switch (a->type & ANIM_CONDITION)
		{
		case ANIM_IFVISITED:
			li = FindLevelInfo(a->levelname);
			if (li == NULL || !(li->flags & LEVEL_VISITED)) continue;
			break;

		case ANIM_IFNOTVISITED:
			li = FindLevelInfo(a->levelname);
			if (li == NULL || (li->flags & LEVEL_VISITED)) continue;
			break;

			// StatCount means 'leaving' - everything else means 'entering'!
		case ANIM_IFENTERING:
			if (state == StatCount || strnicmp(a->levelname, wbs->next, 8)) continue;
			break;

		case ANIM_IFNOTENTERING:
			if (state != StatCount && !strnicmp(a->levelname, wbs->next, 8)) continue;
			break;

		case ANIM_IFLEAVING:
			if (state != StatCount || strnicmp(a->levelname, wbs->current, 8)) continue;
			break;

		case ANIM_IFNOTLEAVING:
			if (state == StatCount && !strnicmp(a->levelname, wbs->current, 8)) continue;
			break;

		case ANIM_IFTRAVELLING:
			if (strnicmp(a->levelname2, wbs->current, 8) || strnicmp(a->levelname, wbs->next, 8)) continue;
			break;

		case ANIM_IFNOTTRAVELLING:
			if (!strnicmp(a->levelname2, wbs->current, 8) && !strnicmp(a->levelname, wbs->next, 8)) continue;
			break;
		}
		if (a->ctr >= 0)
			screen->DrawTexture(a->p[a->ctr], a->loc.x, a->loc.y, 
								DTA_VirtualWidth, animwidth, DTA_VirtualHeight, animheight, TAG_DONE);
	}
}


//====================================================================
//
// Draws a single character with a shadow
//
//====================================================================

static void WI_DrawCharPatch (FTexture *patch, int x, int y)
{
	if (patch->UseType != FTexture::TEX_FontChar)
	{
		screen->DrawTexture (patch, x, y,
			DTA_Clean, true,
			DTA_ShadowAlpha, (gameinfo.gametype == GAME_Doom) ? 0 : FRACUNIT/2,
			TAG_DONE);
	}
	else
	{
		screen->DrawTexture (patch, x, y,
			DTA_Clean, true,
			DTA_ShadowAlpha, (gameinfo.gametype == GAME_Doom) ? 0 : FRACUNIT/2,
			DTA_Translation, BigFont->GetColorTranslation (CR_UNTRANSLATED),	// otherwise it doesn't look good in Strife!
			TAG_DONE);
	}
}


//====================================================================
//
// Draws a level name with the big font
//
// x is no longer passed as a parameter because the text is now broken into several lines
// if it is too long
//
//====================================================================

int WI_DrawName(int y,const char * levelname, bool nomove=false)
{
	int i;
	size_t l;
	const char * p;
	int h=0;
	int lumph;

	lumph=BigFont->GetHeight();

	p=levelname;
	l=strlen(p);
	if (!l) return 0;

	screen->SetFont(BigFont);
	FBrokenLines *lines = V_BreakLines(BigFont, 320, p);

	if (lines)
	{
		for (i = 0; lines[i].Width >= 0; i++)
		{
			if (!nomove)
			{
				screen->DrawText(CR_UNTRANSLATED, 160 - lines[i].Width/2, y+h, lines[i].Text, DTA_Clean, true, TAG_DONE);
			}
			else
			{
				screen->DrawText(CR_UNTRANSLATED, (SCREENWIDTH - lines[i].Width * CleanXfac) / 2, (y+h) * CleanYfac, 
					lines[i].Text, DTA_CleanNoMove, true, TAG_DONE);
			}
			h+=lumph;
		}
		V_FreeBrokenLines(lines);
	}
	screen->SetFont(SmallFont);
	return h+lumph/4;
}


//====================================================================
//
// Draws "<Levelname> Finished!"
//
// Either uses the specified patch or the big font
// A level name patch can be specified for all games now, not just Doom.
//
//====================================================================
void WI_drawLF ()
{
	int y = WI_TITLEY;

	FTexture * tex = wbs->lname0[0]? TexMan[wbs->lname0] : NULL;
	
	// draw <LevelName> 
	if (tex)
	{
		screen->DrawTexture(tex, 160-tex->GetWidth()/2, y, DTA_Clean, true, TAG_DONE);
		y += tex->GetHeight() + BigFont->GetHeight()/4;
	}
	else 
	{
		y+=WI_DrawName(y, lnametexts[0]);
	}
	
	// draw "Finished!"
	if (y < NG_STATSY - screen->Font->GetHeight()*3/4)
	{
		// don't draw 'finished' if the level name is too high!
		if (gameinfo.gametype == GAME_Doom) 
		{
			screen->DrawTexture(finished, 160 - finished->GetWidth()/2, y, DTA_Clean, true, TAG_DONE);
		}
		else 
		{
			screen->SetFont(gameinfo.gametype&GAME_Raven? SmallFont : BigFont);
			screen->DrawText(CR_WHITE, 160 - screen->Font->StringWidth("finished")/2, y-4, "finished", 
				DTA_Clean, true, TAG_DONE);
			screen->SetFont(SmallFont);
		}
	}
}


//====================================================================
//
// Draws "Entering <LevelName>"
//
// Either uses the specified patch or the big font
// A level name patch can be specified for all games now, not just Doom.
//
//====================================================================
void WI_drawEL ()
{
	int y = WI_TITLEY;


	// draw "entering"
	// be careful with the added height so that it works for oversized 'entering' patches!
	if (gameinfo.gametype == GAME_Doom)
	{
		screen->DrawTexture(entering, (SCREENWIDTH - entering->GetWidth() * CleanXfac) / 2, y * CleanYfac, DTA_CleanNoMove, true, TAG_DONE);
		y += entering->GetHeight() + screen->Font->GetHeight()/4;
	}
	else
	{
		screen->SetFont(gameinfo.gametype&GAME_Raven? SmallFont : BigFont);
		screen->DrawText(CR_WHITE, (SCREENWIDTH - screen->Font->StringWidth("now entering:") * CleanXfac) / 2, y * CleanYfac, 
			"now entering:", DTA_CleanNoMove, true, TAG_DONE);
		y += screen->Font->GetHeight()*5/4;
		screen->SetFont(SmallFont);
	}

	// draw <LevelName>
	FTexture * tex = wbs->lname1[0]? TexMan[wbs->lname1] : NULL;
	if (tex)
	{
		screen->DrawTexture(tex, (SCREENWIDTH - tex->GetWidth() * CleanXfac) / 2, y * CleanYfac, DTA_CleanNoMove, true, TAG_DONE);
	}
	else
	{
		WI_DrawName(y, lnametexts[1], true);
	}
}


//====================================================================
//
// Draws the splats and the 'You are here' arrows
//
//====================================================================

int WI_MapToIndex (const char *map)
{
	unsigned int i;

	for (i = 0; i < lnodes.Size(); i++)
	{
		if (!strnicmp (lnodes[i].level, map, 8))
			break;
	}
	return i;
}


//====================================================================
//
// Draws the splats and the 'You are here' arrows
//
//====================================================================

void WI_drawOnLnode( int   n, FTexture * c[] ,int numc)
{
	int   i;
	for(i=0;i<numc;i++)
	{
		int            left;
		int            top;
		int            right;
		int            bottom;


		right = c[i]->GetWidth();
		bottom = c[i]->GetHeight();
		left = lnodes[n].x - c[i]->LeftOffset;
		top = lnodes[n].y - c[i]->TopOffset;
		right += left;
		bottom += top;
		
		if (left >= 0 && right < 320 && top >= 0 && bottom < 200) 
		{
			screen->DrawTexture (c[i], lnodes[n].x, lnodes[n].y, DTA_320x200, true, TAG_DONE);
			break;
		}
	} 
}

// ====================================================================
//
// Draws a number.
// If digits > 0, then use that many digits minimum,
//	otherwise only use as many as necessary.
// Returns new x position.
//
// ====================================================================
int WI_drawNum (int x, int y, int n, int digits, bool leadingzeros = true)
{
	int fontwidth = num[3]->GetWidth();
	int xofs;
	char text[8];
	char *text_p;

	if (leadingzeros)
	{
		mysnprintf (text, countof(text), "%07d", n);
	}
	else
	{
		mysnprintf (text, countof(text), "%7d", n);
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
			FTexture *p = num[*text_p - '0'];
			WI_DrawCharPatch (p, xofs + (fontwidth - p->GetWidth())/2, y);
		}
		text_p++;
		xofs += fontwidth;
	}
	return x;

}

// ====================================================================
//
//
//
// ====================================================================

void WI_drawPercent (int x, int y, int p, int b)
{
	if (p < 0)
		return;

	if (wi_percents)
	{
		WI_DrawCharPatch (percent, x, y);

		if (b == 0)
			WI_drawNum (x, y, 100, -1, false);
		else
			WI_drawNum(x, y, p * 100 / b, -1, false);
	}
	else
	{
		int y2 = y + percent->GetHeight() - screen->Font->GetHeight ();
		x = WI_drawNum (x, y, b, -1, false);
		x -= SmallFont->StringWidth (" OF ");
		screen->DrawText (CR_UNTRANSLATED, x, y2, " OF",
			DTA_Clean, true, TAG_DONE);
		WI_drawNum (x, y, p, -1, false);
	}
}

//====================================================================
//
// Display level completion time and par, or "sucks" message if overflow.
//
//====================================================================
void WI_drawTime (int x, int y, int t, bool no_sucks=false)
{
	bool sucky;

	if (t<0)
		return;

	sucky = !no_sucks && t >= wbs->sucktime * 60 * 60 && wbs->sucktime > 0;
	int hours = t / 3600;
	t -= hours * 3600;
	int minutes = t / 60;
	t -= minutes * 60;
	int seconds = t;

	// Why were these offsets hard coded? Half the WADs with custom patches
	// I tested screwed up miserably in this function!
	int num_spacing = num[3]->GetWidth();
	int colon_spacing = colon->GetWidth();

	x -= 2*num_spacing;
	WI_drawNum (x, y, seconds, 2);
	x -= colon_spacing;
	WI_DrawCharPatch (colon, x , y);
	x -= 2*num_spacing ;
	WI_drawNum (x, y, minutes, 2, hours!=0);
	if (hours)
	{
		x -= colon_spacing;
		WI_DrawCharPatch (colon, x , y);
		x -= 2*num_spacing ;
		WI_drawNum (x, y, hours, 2);
	}

	if (sucky)
	{ // "sucks"
		if (sucks != NULL)
		{
			screen->DrawTexture (sucks, x - sucks->GetWidth(), y - num[0]->GetHeight() - 2,
				DTA_Clean, true, TAG_DONE); 
		}
		else
		{
			screen->SetFont (BigFont);
			screen->DrawText (CR_UNTRANSLATED, x  - BigFont->StringWidth("SUCKS"), y - BigFont->GetHeight() - 2,
				"SUCKS", DTA_Clean, true, TAG_DONE);
			screen->SetFont (SmallFont);
		}
	}
}

void WI_End ()
{
	state = LeavingIntermission;
	WI_unloadData ();

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


	if (!wi_noautostartmap && !noautostartmap) cnt--;
	if (acceleratestage) cnt=0;

	if (cnt==0)
	{
		WI_End();
		G_WorldDone();
	}
}

static bool snl_pointeron = false;

void WI_initShowNextLoc ()
{
	if (wbs->next_ep == -1) 
	{
		// Last map in episode - there is no next location!
		WI_End();
		G_WorldDone();
		return;
	}

	state = ShowNextLoc;
	acceleratestage = 0;
	cnt = SHOWNEXTLOCDELAY * TICRATE;
	WI_LoadBackground(true);
}

void WI_updateShowNextLoc ()
{
	WI_updateAnimatedBack();

	if (!--cnt || acceleratestage)
		WI_initNoState();
	else
		snl_pointeron = (cnt & 31) < 20;
}

void WI_drawShowNextLoc(void)
{
	unsigned int i;
	
	WI_drawBackground();

	if (splat)
	{
		for (i=0 ; i<lnodes.Size() ; i++) 
		{
			level_info_t * li = FindLevelInfo (lnodes[i].level);
			if (li && li->flags & LEVEL_VISITED) WI_drawOnLnode(i, &splat,1);  // draw a splat on taken cities.
		}
	}
		
	// draw flashing ptr
	if (snl_pointeron && yah.Size())
	{
		unsigned int v = WI_MapToIndex (wbs->next);
		// Draw only if it points to a valid level on the current screen!
		if (v<lnodes.Size()) WI_drawOnLnode (v, &yah[0], yah.Size()); 
	}

	// draws which level you are entering..
	WI_drawEL ();  

}

void WI_drawNoState ()
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
}

void WI_updateDeathmatchStats ()
{
	/*
	int i, j;
	bool stillticking;
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
		
		S_Sound (CHAN_VOICE | CHAN_UI, NEXTSTAGE, 1, ATTN_NONE);
		*/
		dm_state = 4;
	}
	
    
	if (dm_state == 2)
	{
		/*
		if (!(bcnt&3))
			S_Sound (CHAN_VOICE | CHAN_UI, "weapons/pistol", 1, ATTN_NONE);
		
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
			S_Sound (CHAN_VOICE | CHAN_UI, NEXTSTAGE, 1, ATTN_NONE);
			dm_state++;
		}
		*/
		dm_state = 3;
	}
	else if (dm_state == 4)
	{
		if (acceleratestage)
		{
			S_Sound (CHAN_VOICE | CHAN_UI, "players/male/gibbed", 1, ATTN_NONE);

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
	WI_drawBackground(); 
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
	V_DrawPatchClean(DM_TOTALSX-LittleShort(total->width)/2,
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
			V_DrawPatchClean(x-LittleShort(p[i]->width)/2,
						DM_MATRIXY - WI_SPACINGY,
						&FB,
						p[i]);
			
			V_DrawPatchClean(DM_MATRIXX-LittleShort(p[i]->width)/2,
						y,
						&FB,
						p[i]);

			if (i == me)
			{
				V_DrawPatchClean(x-LittleShort(p[i]->width)/2,
							DM_MATRIXY - WI_SPACINGY,
							&FB,
							bstar);

				V_DrawPatchClean(DM_MATRIXX-LittleShort(p[i]->width)/2,
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
	w = LittleShort(num[0]->width);

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
static int    dofrags;
static int    ng_state;

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
}

void WI_updateNetgameStats ()
{

	int i;
	int fsum;
	bool stillticking;

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
		S_Sound (CHAN_VOICE | CHAN_UI, NEXTSTAGE, 1, ATTN_NONE);
		ng_state = 10;
	}

	if (ng_state == 2)
	{
		if (!(bcnt&3))
			S_Sound (CHAN_VOICE | CHAN_UI, "weapons/pistol", 1, ATTN_NONE);

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
			S_Sound (CHAN_VOICE | CHAN_UI, NEXTSTAGE, 1, ATTN_NONE);
			ng_state++;
		}
	}
	else if (ng_state == 4)
	{
		if (!(bcnt&3))
			S_Sound (CHAN_VOICE | CHAN_UI, "weapons/pistol", 1, ATTN_NONE);

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
			S_Sound (CHAN_VOICE | CHAN_UI, NEXTSTAGE, 1, ATTN_NONE);
			ng_state++;
		}
	}
	else if (ng_state == 6)
	{
		if (!(bcnt&3))
			S_Sound (CHAN_VOICE | CHAN_UI, "weapons/pistol", 1, ATTN_NONE);

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
			S_Sound (CHAN_VOICE | CHAN_UI, NEXTSTAGE, 1, ATTN_NONE);
			ng_state += 1 + 2*!dofrags;
		}
	}
	else if (ng_state == 8)
	{
		if (!(bcnt&3))
			S_Sound (CHAN_VOICE | CHAN_UI, "weapons/pistol", 1, ATTN_NONE);

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
			S_Sound (CHAN_VOICE | CHAN_UI, "player/male/death1", 1, ATTN_NONE);
			ng_state++;
		}
	}
	else if (ng_state == 10)
	{
		if (acceleratestage)
		{
			S_Sound (CHAN_VOICE | CHAN_UI, PASTSTATS, 1, ATTN_NONE);
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
	int pwidth = percent->GetWidth();

	// draw animated background
	WI_drawBackground(); 

	WI_drawLF();

	if (gameinfo.gametype == GAME_Doom)
	{
		// draw stat titles (top line)
		screen->DrawTexture (kills, NG_STATSX+NG_SPACINGX-kills->GetWidth(), NG_STATSY, DTA_Clean, true, TAG_DONE);
		screen->DrawTexture (items, NG_STATSX+2*NG_SPACINGX-items->GetWidth(), NG_STATSY, DTA_Clean, true, TAG_DONE);
		screen->DrawTexture (secret, NG_STATSX+3*NG_SPACINGX-secret->GetWidth(), NG_STATSY, DTA_Clean, true, TAG_DONE);

		if (dofrags)
			screen->DrawTexture (frags, NG_STATSX+4*NG_SPACINGX-frags->GetWidth(), NG_STATSY, DTA_Clean, true, TAG_DONE);

		// draw stats
		y = NG_STATSY + kills->GetHeight();

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (y >= 200-WI_SPACINGY)
				break;

			if (!playeringame[i])
				continue;

			x = NG_STATSX;
			// [RH] Only use one graphic for the face backgrounds
			screen->DrawTexture (p, x - p->GetWidth(), y,
				DTA_Translation, translationtables[TRANSLATION_Players](i),
				DTA_Clean, true,
				TAG_DONE);

			if (i == me)
				screen->DrawTexture (star, x - p->GetWidth(), y,
					DTA_Translation, translationtables[TRANSLATION_Players](i),
					DTA_Clean, true,
					TAG_DONE);

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
		if (gameinfo.gametype & GAME_Raven)
		{
			screen->SetFont (BigFont);
			screen->DrawText (CR_UNTRANSLATED, 95, 35, "KILLS", DTA_Clean, true, DTA_Shadow, true, TAG_DONE);
			screen->DrawText (CR_UNTRANSLATED, 155, 35, "BONUS", DTA_Clean, true, DTA_Shadow, true, TAG_DONE);
			screen->DrawText (CR_UNTRANSLATED, 232, 35, "SECRET", DTA_Clean, true, DTA_Shadow, true, TAG_DONE);
			y = 50;
		}
		else
		{
			screen->SetFont (SmallFont);
			screen->DrawText (CR_UNTRANSLATED, 95, 50, "KILLS", DTA_Clean, true, DTA_Shadow, true, TAG_DONE);
			screen->DrawText (CR_UNTRANSLATED, 155, 50, "BONUS", DTA_Clean, true, DTA_Shadow, true, TAG_DONE);
			screen->DrawText (CR_UNTRANSLATED, 232, 50, "SECRET", DTA_Clean, true, DTA_Shadow, true, TAG_DONE);
			y = 62;
		}
		WI_drawLF ();	

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (y >= 200-WI_SPACINGY)
				break;
			if (!playeringame[i])
				continue;
			if (gameinfo.gametype == GAME_Heretic)
			{
				screen->DrawTexture (star, 25, y,
					DTA_Translation, translationtables[TRANSLATION_Players](i),
					DTA_Clean, true,
					TAG_DONE);
			}
			else	// Hexen and Strife don't have a face graphic for this.
			{
				char pstr[3]={'P', '1'+i, 0};
				screen->SetFont (BigFont);
				screen->DrawText(CR_UNTRANSLATED, 25, y+10, pstr, DTA_Clean, true, TAG_DONE);
			}

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
		screen->SetFont (SmallFont);
	}
}

static int  sp_state;

void WI_initStats ()
{
	state = StatCount;
	acceleratestage = 0;
	sp_state = 1;
	cnt_kills[0] = cnt_items[0] = cnt_secret[0] = -1;
	cnt_time = cnt_par = -1;
	cnt_pause = TICRATE;
	
	cnt_total_time = -1;
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
			S_Sound (CHAN_VOICE | CHAN_UI, NEXTSTAGE, 1, ATTN_NONE);
		}
		cnt_kills[0] = plrs[me].skills;
		cnt_items[0] = plrs[me].sitems;
		cnt_secret[0] = plrs[me].ssecret;
		cnt_time = plrs[me].stime / TICRATE;
		cnt_par = wbs->partime / TICRATE;
	    cnt_total_time = wbs->totaltime / TICRATE;
	}

	if (sp_state == 2)
	{
		if (gameinfo.gametype == GAME_Doom)
		{
			cnt_kills[0] += 2;

			if (!(bcnt&3))
				S_Sound (CHAN_VOICE | CHAN_UI, "weapons/pistol", 1, ATTN_NONE);
		}
		if (cnt_kills[0] >= plrs[me].skills)
		{
			cnt_kills[0] = plrs[me].skills;
			S_Sound (CHAN_VOICE | CHAN_UI, NEXTSTAGE, 1, ATTN_NONE);
			sp_state++;
		}
	}
	else if (sp_state == 4)
	{
		if (gameinfo.gametype == GAME_Doom)
		{
			cnt_items[0] += 2;

			if (!(bcnt&3))
				S_Sound (CHAN_VOICE | CHAN_UI, "weapons/pistol", 1, ATTN_NONE);
		}
		if (cnt_items[0] >= plrs[me].sitems)
		{
			cnt_items[0] = plrs[me].sitems;
			S_Sound (CHAN_VOICE | CHAN_UI, NEXTSTAGE, 1, ATTN_NONE);
			sp_state++;
		}
	}
	else if (sp_state == 6)
	{
		if (gameinfo.gametype == GAME_Doom)
		{
			cnt_secret[0] += 2;

			if (!(bcnt&3))
				S_Sound (CHAN_VOICE | CHAN_UI, "weapons/pistol", 1, ATTN_NONE);
		}
		if (cnt_secret[0] >= plrs[me].ssecret)
		{
			cnt_secret[0] = plrs[me].ssecret;
			S_Sound (CHAN_VOICE | CHAN_UI, NEXTSTAGE, 1, ATTN_NONE);
			sp_state++;
		}
	}
	else if (sp_state == 8)
	{
		if (gameinfo.gametype == GAME_Doom)
		{
			if (!(bcnt&3))
				S_Sound (CHAN_VOICE | CHAN_UI, "weapons/pistol", 1, ATTN_NONE);

			cnt_time += 3;
			cnt_par += 3;
			cnt_total_time += 3;
		}

		if (cnt_time >= plrs[me].stime / TICRATE)
			cnt_time = plrs[me].stime / TICRATE;

		if (cnt_total_time >= wbs->totaltime / TICRATE)
			cnt_total_time = wbs->totaltime / TICRATE;

		if (cnt_par >= wbs->partime / TICRATE)
		{
			cnt_par = wbs->partime / TICRATE;

			if (cnt_time >= plrs[me].stime / TICRATE)
			{
				cnt_total_time = wbs->totaltime / TICRATE;
				S_Sound (CHAN_VOICE | CHAN_UI, NEXTSTAGE, 1, ATTN_NONE);
				sp_state++;
			}
		}
	}
	else if (sp_state == 10)
	{
		if (acceleratestage)
		{
			S_Sound (CHAN_VOICE | CHAN_UI, PASTSTATS, 1, ATTN_NONE);
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

	lh = (3*num[0]->GetHeight())/2;

	// draw animated background
	WI_drawBackground(); 
	
	WI_drawLF();
	
	if (gameinfo.gametype == GAME_Doom)
	{
		screen->DrawTexture (kills, SP_STATSX, SP_STATSY, DTA_Clean, true, TAG_DONE);
		WI_drawPercent (320 - SP_STATSX, SP_STATSY, cnt_kills[0], wbs->maxkills);

		screen->DrawTexture (items, SP_STATSX, SP_STATSY+lh, DTA_Clean, true, TAG_DONE);
		WI_drawPercent (320 - SP_STATSX, SP_STATSY+lh, cnt_items[0], wbs->maxitems);

		screen->DrawTexture (sp_secret, SP_STATSX, SP_STATSY+2*lh, DTA_Clean, true, TAG_DONE);
		WI_drawPercent(320 - SP_STATSX, SP_STATSY+2*lh, cnt_secret[0], wbs->maxsecret);

		screen->DrawTexture (timepic, SP_TIMEX, SP_TIMEY, DTA_Clean, true, TAG_DONE);
		WI_drawTime (160 - SP_TIMEX, SP_TIMEY, cnt_time);
		if (wi_showtotaltime)
		{
			WI_drawTime (160 - SP_TIMEX, SP_TIMEY + lh, cnt_total_time, true);	// no 'sucks' for total time ever!
		}

		if (wbs->partime)
		{
			screen->DrawTexture (par, 160 + SP_TIMEX, SP_TIMEY, DTA_Clean, true, TAG_DONE);
			WI_drawTime (320 - SP_TIMEX, SP_TIMEY, cnt_par);
		}

	}
	else
	{
		screen->SetFont (BigFont);
		screen->DrawText (CR_UNTRANSLATED, 50, 65, "KILLS", DTA_Clean, true, DTA_Shadow, true, TAG_DONE);
		screen->DrawText (CR_UNTRANSLATED, 50, 90, "ITEMS", DTA_Clean, true, DTA_Shadow, true, TAG_DONE);
		screen->DrawText (CR_UNTRANSLATED, 50, 115, "SECRETS", DTA_Clean, true, DTA_Shadow, true, TAG_DONE);

		int slashpos = gameinfo.gametype==GAME_Strife? 235:237;
		int countpos = gameinfo.gametype==GAME_Strife? 185:200;
		if (sp_state >= 2)
		{
			WI_drawNum (countpos, 65, cnt_kills[0], 3, false);
			WI_DrawCharPatch (slash, slashpos, 65);
			WI_drawNum (248, 65, wbs->maxkills, 3, false);
		}
		if (sp_state >= 4)
		{
			WI_drawNum (countpos, 90, cnt_items[0], 3, false);
			WI_DrawCharPatch (slash, slashpos, 90);
			WI_drawNum (248, 90, wbs->maxitems, 3, false);
		}
		if (sp_state >= 6)
		{
			WI_drawNum (countpos, 115, cnt_secret[0], 3, false);
			WI_DrawCharPatch (slash, slashpos, 115);
			WI_drawNum (248, 115, wbs->maxsecret, 3, false);
		}
		if (sp_state >= 8)
		{
			screen->DrawText (CR_UNTRANSLATED, 85, 160, "TIME",
				DTA_Clean, true, DTA_Shadow, true, TAG_DONE);
			WI_drawTime (249, 160, cnt_time);
			if (wi_showtotaltime)
			{
				WI_drawTime (249, 180, cnt_total_time);
			}
		}
		screen->SetFont (SmallFont);
	}
}

// ====================================================================
// WI_checkForAccelerate
// Purpose: See if the player has hit either the attack or use key
//          or mouse button.  If so we set acceleratestage to 1 and
//          all those display routines above jump right to the end.
// Args:    none
// Returns: void
//
// ====================================================================
void WI_checkForAccelerate(void)
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
					== players[i].oldbuttons) && !player->isbot)
			{
				acceleratestage = 1;
			}
			player->oldbuttons = player->cmd.ucmd.buttons;
		}
	}
}

// ====================================================================
// WI_Ticker
// Purpose: Do various updates every gametic, for stats, animation,
//          checking that intermission music is running, etc.
// Args:    none
// Returns: void
//
// ====================================================================
void WI_Ticker(void)
{
	// counter for general background animation
	bcnt++;  
	
	if (bcnt == 1)
	{
		// intermission music - use the defaults if none specified
		if (level.info->intermusic != NULL) 
			S_ChangeMusic(level.info->intermusic, level.info->intermusicorder);
		else if (gameinfo.gametype == GAME_Heretic)
			S_ChangeMusic ("mus_intr");
		else if (gameinfo.gametype == GAME_Hexen)
			S_ChangeMusic ("hub");
		else if (gameinfo.gametype == GAME_Strife)	// Strife also needs a default
			S_ChangeMusic ("d_slide");
		else if (gamemode == commercial)
			S_ChangeMusic ("d_dm2int");
		else
			S_ChangeMusic ("d_inter"); 

	}
	
	WI_checkForAccelerate();
	
	switch (state)
	{
    case StatCount:
		if (deathmatch) WI_updateDeathmatchStats();
		else if (multiplayer) WI_updateNetgameStats();
		else WI_updateStats();
		break;
		
    case ShowNextLoc:
		WI_updateShowNextLoc();
		break;
		
    case NoState:
		WI_updateNoState();
		break;

	case LeavingIntermission:
		// Hush, GCC.
		break;
	}
}

void WI_loadData(void)
{
	int i;
	char name[9];

	if (gameinfo.gametype == GAME_Doom)
	{
		wiminus = TexMan["WIMINUS"];		// minus sign
		percent = TexMan["WIPCNT"];		// percent sign
		finished = TexMan["WIF"];		// "finished"
		entering = TexMan["WIENTER"];	// "entering"
		kills = TexMan["WIOSTK"];		// "kills"
		secret = TexMan["WIOSTS"];		// "scrt"
		sp_secret = TexMan["WISCRT2"];	// "secret"
		items = TexMan["WIOSTI"];		// "items"
		frags = TexMan["WIFRGS"];		// "frgs"
		colon = TexMan["WICOLON"];		// ":"
		timepic = TexMan["WITIME"];		// "time"
		sucks = TexMan["WISUCKS"];		// "sucks"
		par = TexMan["WIPAR"];			// "par"
		killers = TexMan["WIKILRS"];		// "killers" (vertical]
		victims = TexMan["WIVCTMS"];		// "victims" (horiz]
		total = TexMan["WIMSTT"];		// "total"
		star = TexMan["STFST01"];		// your face
		bstar = TexMan["STFDEAD0"];		// dead face
 		p = TexMan["STPBANY"];

		for (i = 0; i < 10; i++)
		{ // numbers 0-9
			mysnprintf (name, countof(name), "WINUM%d", i);	 
			num[i] = TexMan[name];
		}
	}
	else if (gameinfo.gametype & GAME_Raven)
	{
		wiminus = TexMan["FONTB13"];
		percent = TexMan["FONTB05"];
		colon = TexMan["FONTB26"];
		slash = TexMan["FONTB15"];
		if (gameinfo.gametype==GAME_Heretic)
		{
			star = TexMan["FACEA0"];
			bstar = TexMan["FACEB0"];
		}
		else
		{
			int dummywidth;
			star = BigFont->GetChar('*', &dummywidth);	// just a dummy to avoid an error if it is being used
			bstar = star;
		}

		for (i = 0; i < 10; i++)
		{
			mysnprintf (name, countof(name), "FONTB%d", 16 + i);
			num[i] = TexMan[name];
		}
	}
	else // Strife needs some handling, too!
	{
		int dummywidth;
		wiminus = BigFont->GetChar('-', &dummywidth);
		percent = BigFont->GetChar('%', &dummywidth);
		colon = BigFont->GetChar(':', &dummywidth);
		slash = BigFont->GetChar('/', &dummywidth);
		star = BigFont->GetChar('*', &dummywidth);	// just a dummy to avoid an error if it is being used
		bstar = star;
		for (i = 0; i < 10; i++)
		{
			num[i] = BigFont->GetChar('0'+i, &dummywidth);
		}
	}

	// Use the local level structure which can be overridden by hubs if they eventually get names!
	lnametexts[0] = level.level_name;		

	level_info_t * li = FindLevelInfo(wbs->next);
	if (li) lnametexts[1] = G_MaybeLookupLevelName(li);
	else lnametexts[1]=NULL;

	WI_LoadBackground(false);
}

void WI_unloadData ()
{
	// [RH] The texture data gets unloaded at pre-map time, so there's nothing to do here
	return;
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
}

void WI_Start (wbstartstruct_t *wbstartstruct)
{
	noautostartmap = false;
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

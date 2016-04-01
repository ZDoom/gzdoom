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
#include "d_player.h"
#include "d_netinf.h"
#include "b_bot.h"
#include "textures/textures.h"
#include "r_data/r_translate.h"
#include "templates.h"
#include "gstrings.h"

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
CVAR (Bool, wi_noautostartmap, false, CVAR_USERINFO|CVAR_ARCHIVE)
CVAR (Int, wi_autoadvance, 0, CVAR_SERVERINFO)


void WI_loadData ();
void WI_unloadData ();

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
#define NG_STATSX				(32 + star->GetScaledWidth()/2 + 32*!dofrags)

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

struct yahpt_t
{
	int x, y;
};

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
struct in_anim_t
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
};

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
static bool				playerready[MAXPLAYERS];
static int				me;					// wbs->pnum
static stateenum_t		state;				// specifies current state
static wbstartstruct_t *wbs;				// contains information passed into intermission
static wbplayerstruct_t*plrs;				// wbs->plyr[]
static int				cnt;				// used for general timing
static int				bcnt;				// used for timing of background animation
static int				cnt_kills[MAXPLAYERS];
static int				cnt_items[MAXPLAYERS];
static int				cnt_secret[MAXPLAYERS];
static int				cnt_frags[MAXPLAYERS];
static int				cnt_deaths[MAXPLAYERS];
static int				cnt_time;
static int				cnt_total_time;
static int				cnt_par;
static int				cnt_pause;
static int				total_frags;
static int				total_deaths;
static bool				noautostartmap;
static int				dofrags;
static int				ng_state;

//
//		GRAPHICS
//

struct FPatchInfo
{
	FFont *mFont;
	FTexture *mPatch;
	EColorRange mColor;

	void Init(FGIFont &gifont)
	{
		if (gifont.color == NAME_Null)
		{
			mPatch = TexMan[gifont.fontname];	// "entering"
			mColor = mPatch == NULL? CR_UNTRANSLATED : CR_UNDEFINED;
			mFont = NULL;
		}
		else
		{
			mFont = V_GetFont(gifont.fontname);
			mColor = V_FindFontColor(gifont.color);
			mPatch = NULL;
		}
		if (mFont == NULL)
		{
			mFont = BigFont;
		}
	}
};

static FPatchInfo mapname;
static FPatchInfo finished;
static FPatchInfo entering;

static TArray<FTexture *> yah; 		// You Are Here graphic
static FTexture* 		splat;		// splat
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
//static FTexture* 		star;
//static FTexture* 		bstar;
static FTexture* 		p;			// Player graphic
static FTexture*		lnames[2];	// Name graphics of each level (centered)

// [RH] Info to dynamically generate the level name graphics
static FString			lnametexts[2];

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
	const char *lumpname = NULL;
	char buffer[10];
	in_anim_t an;
	lnode_t pt;
	FTextureID texture;

	bcnt=0;

	texture.SetInvalid();
	if (isenterpic)
	{
		level_info_t * li = FindLevelInfo(wbs->next);
		if (li != NULL) lumpname = li->EnterPic;
	}
	else
	{
		lumpname = level.info->ExitPic;
	}

	// Try to get a default if nothing specified
	if (lumpname == NULL || lumpname[0]==0) 
	{
		lumpname = NULL;
		switch(gameinfo.gametype)
		{
		case GAME_Chex:
		case GAME_Doom:
			if (!(gameinfo.flags & GI_MAPxx))
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
					if (level.info->ExitPic.IsNotEmpty()) return;

					// E1-E3 need special treatment when playing Doom 1.
					if (!(gameinfo.flags & GI_MAPxx))
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
			lumpname = gameinfo.BorderFlat;
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
					texture = TexMan.CheckForTexture(sc.String, FTexture::TEX_MiscPatch, FTextureManager::TEXMAN_TryAny);
					break;

				case 1:		// Splat
					sc.MustGetString();
					splat = TexMan[sc.String];
					break;

				case 2:		// Pointers
					while (sc.GetString() && !sc.Crossed)
					{
						yah.Push(TexMan[sc.String]);
					}
					if (sc.Crossed)
						sc.UnGet();
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
	double animwidth=320;		// For a flat fill or clear background scale animations to 320x200
	double animheight=200;

	if (background)
	{
		// background
		if (background->UseType == FTexture::TEX_MiscPatch)
		{
			// scale all animations below to fit the size of the base pic
			// The base pic is always scaled to fit the screen so this allows
			// placing the animations precisely where they belong on the base pic
			animwidth = background->GetScaledWidthDouble();
			animheight = background->GetScaledHeightDouble();
			screen->FillBorder (NULL);
			screen->DrawTexture(background, 0, 0, DTA_Fullscreen, true, TAG_DONE);
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
								DTA_VirtualWidthF, animwidth, DTA_VirtualHeightF, animheight, TAG_DONE);
	}
}


//====================================================================
//
// Draws a single character with a shadow
//
//====================================================================

static int WI_DrawCharPatch (FFont *font, int charcode, int x, int y, EColorRange translation=CR_UNTRANSLATED, bool nomove=false)
{
	int width;
	screen->DrawTexture(font->GetChar(charcode, &width), x, y,
		nomove ? DTA_CleanNoMove : DTA_Clean, true,
		DTA_ShadowAlpha, (gameinfo.gametype & GAME_DoomChex) ? 0 : OPAQUE/2,
		DTA_Translation, font->GetColorTranslation(translation),
		TAG_DONE);
	return x - width;
}

//====================================================================
//
// CheckRealHeight
//
// Checks the posts in a texture and returns the lowest row (plus one)
// of the texture that is actually used.
//
//====================================================================

int CheckRealHeight(FTexture *tex)
{
	const FTexture::Span *span;
	int maxy = 0, miny = tex->GetHeight();

	for (int i = 0; i < tex->GetWidth(); ++i)
	{
		tex->GetColumn(i, &span);
		while (span->Length != 0)
		{
			if (span->TopOffset < miny)
			{
				miny = span->TopOffset;
			}
			if (span->TopOffset + span->Length > maxy)
			{
				maxy = span->TopOffset + span->Length;
			}
			span++;
		}
	}
	// Scale maxy before returning it
	maxy = int((maxy *2) / tex->Scale.Y);
	maxy = (maxy >> 1) + (maxy & 1);
	return maxy;
}

//====================================================================
//
// Draws a level name with the big font
//
// x is no longer passed as a parameter because the text is now broken into several lines
// if it is too long
//
//====================================================================

int WI_DrawName(int y, FTexture *tex, const char *levelname)
{
	// draw <LevelName> 
	if (tex)
	{
		screen->DrawTexture(tex, (screen->GetWidth() - tex->GetScaledWidth()*CleanXfac) /2, y, DTA_CleanNoMove, true, TAG_DONE);
		int h = tex->GetScaledHeight();
		if (h > 50)
		{ // Fix for Deus Vult II and similar wads that decide to make these hugely tall
		  // patches with vast amounts of empty space at the bottom.
			h = CheckRealHeight(tex);
		}
		return y + (h + BigFont->GetHeight()/4) * CleanYfac;
	}
	else 
	{
		int i;
		size_t l;
		const char *p;
		int h = 0;
		int lumph;

		lumph = mapname.mFont->GetHeight() * CleanYfac;

		p = levelname;
		if (!p) return 0;
		l = strlen(p);
		if (!l) return 0;

		FBrokenLines *lines = V_BreakLines(mapname.mFont, screen->GetWidth() / CleanXfac, p);

		if (lines)
		{
			for (i = 0; lines[i].Width >= 0; i++)
			{
				screen->DrawText(mapname.mFont, mapname.mColor, (SCREENWIDTH - lines[i].Width * CleanXfac) / 2, y + h, 
					lines[i].Text, DTA_CleanNoMove, true, TAG_DONE);
				h += lumph;
			}
			V_FreeBrokenLines(lines);
		}
		return y + h + lumph/4;
	}
}

//====================================================================
//
// Draws a text, either as patch or as string from the string table
//
//====================================================================

int WI_DrawPatchText(int y, FPatchInfo *pinfo, const char *stringname)
{
	const char *string = GStrings(stringname);
	int midx = screen->GetWidth() / 2;

	if (pinfo->mPatch != NULL)
	{
		screen->DrawTexture(pinfo->mPatch, midx - pinfo->mPatch->GetScaledWidth()*CleanXfac/2, y, DTA_CleanNoMove, true, TAG_DONE);
		return y + (pinfo->mPatch->GetScaledHeight() * CleanYfac);
	}
	else 
	{
		screen->DrawText(pinfo->mFont, pinfo->mColor, midx - pinfo->mFont->StringWidth(string)*CleanXfac/2,
			y, string, DTA_CleanNoMove, true, TAG_DONE);
		return y + pinfo->mFont->GetHeight() * CleanYfac;
	}
}


//====================================================================
//
// Draws "<Levelname> Finished!"
//
// Either uses the specified patch or the big font
// A level name patch can be specified for all games now, not just Doom.
//
//====================================================================

int WI_drawLF ()
{
	int y = WI_TITLEY * CleanYfac;

	y = WI_DrawName(y, wbs->LName0, lnametexts[0]);
	
	// Adjustment for different font sizes for map name and 'finished'.
	y -= ((mapname.mFont->GetHeight() - finished.mFont->GetHeight()) * CleanYfac) / 4;

	// draw "Finished!"
	if (y < (NG_STATSY - finished.mFont->GetHeight()*3/4) * CleanYfac)
	{
		// don't draw 'finished' if the level name is too tall
		y = WI_DrawPatchText(y, &finished, "WI_FINISHED");
	}
	return y;
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
	int y = WI_TITLEY * CleanYfac;

	y = WI_DrawPatchText(y, &entering, "WI_ENTERING");
	y += entering.mFont->GetHeight() * CleanYfac / 4;
	WI_DrawName(y, wbs->LName1, lnametexts[1]);
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


		right = c[i]->GetScaledWidth();
		bottom = c[i]->GetScaledHeight();
		left = lnodes[n].x - c[i]->GetScaledLeftOffset();
		top = lnodes[n].y - c[i]->GetScaledTopOffset();
		right += left;
		bottom += top;
		
		if (left >= 0 && right < 320 && top >= 0 && bottom < 200) 
		{
			screen->DrawTexture (c[i], lnodes[n].x, lnodes[n].y, DTA_320x200, true, TAG_DONE);
			break;
		}
	} 
}

//====================================================================
//
// Draws a number.
// If digits > 0, then use that many digits minimum,
//	otherwise only use as many as necessary.
// x is the right edge of the number.
// Returns new x position, that is, the left edge of the number.
//
//====================================================================
int WI_drawNum (FFont *font, int x, int y, int n, int digits, bool leadingzeros=true, EColorRange translation=CR_UNTRANSLATED)
{
	int fontwidth = font->GetCharWidth('3');
	char text[8];
	int len;
	char *text_p;
	bool nomove = font != IntermissionFont;

	if (nomove)
	{
		fontwidth *= CleanXfac;
	}
	if (leadingzeros)
	{
		len = mysnprintf (text, countof(text), "%0*d", digits, n);
	}
	else
	{
		len = mysnprintf (text, countof(text), "%d", n);
	}
	text_p = text + MIN<int>(len, countof(text)-1);

	while (--text_p >= text)
	{
		// Digits are centered in a box the width of the '3' character.
		// Other characters (specifically, '-') are right-aligned in their cell.
		if (*text_p >= '0' && *text_p <= '9')
		{
			x -= fontwidth;
			WI_DrawCharPatch(font, *text_p, x + (fontwidth - font->GetCharWidth(*text_p)) / 2, y, translation, nomove);
		}
		else
		{
			WI_DrawCharPatch(font, *text_p, x - font->GetCharWidth(*text_p), y, translation, nomove);
			x -= fontwidth;
		}
	}
	if (len < digits)
	{
		x -= fontwidth * (digits - len);
	}
	return x;
}

//====================================================================
//
//
//
//====================================================================

void WI_drawPercent (FFont *font, int x, int y, int p, int b, bool show_total=true, EColorRange color=CR_UNTRANSLATED)
{
	if (p < 0)
		return;

	if (wi_percents)
	{
		if (font != IntermissionFont)
		{
			x -= font->GetCharWidth('%') * CleanXfac;
		}
		else
		{
			x -= font->GetCharWidth('%');
		}
		screen->DrawText(font, color, x, y, "%", font != IntermissionFont ? DTA_CleanNoMove : DTA_Clean, true, TAG_DONE);
		if (font != IntermissionFont)
		{
			x -= 2*CleanXfac;
		}
		WI_drawNum(font, x, y, b == 0 ? 100 : p * 100 / b, -1, false, color);
	}
	else
	{
		if (show_total)
		{
			x = WI_drawNum(font, x, y, b, 2, false);
			x -= font->GetCharWidth('/');
			screen->DrawText (IntermissionFont, color, x, y, "/",
				DTA_Clean, true, TAG_DONE);
		}
		WI_drawNum (font, x, y, p, -1, false, color);
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

	if (sucky)
	{ // "sucks"
		if (sucks != NULL)
		{
			screen->DrawTexture (sucks, x - sucks->GetScaledWidth(), y - IntermissionFont->GetHeight() - 2,
				DTA_Clean, true, TAG_DONE); 
		}
		else
		{
			screen->DrawText (BigFont, CR_UNTRANSLATED, x  - BigFont->StringWidth("SUCKS"), y - IntermissionFont->GetHeight() - 2,
				"SUCKS", DTA_Clean, true, TAG_DONE);
		}
	}

	int hours = t / 3600;
	t -= hours * 3600;
	int minutes = t / 60;
	t -= minutes * 60;
	int seconds = t;

	// Why were these offsets hard coded? Half the WADs with custom patches
	// I tested screwed up miserably in this function!
	int num_spacing = IntermissionFont->GetCharWidth('3');
	int colon_spacing = IntermissionFont->GetCharWidth(':');

	x = WI_drawNum (IntermissionFont, x, y, seconds, 2) - 1;
	WI_DrawCharPatch (IntermissionFont, ':', x -= colon_spacing, y);
	x = WI_drawNum (IntermissionFont, x, y, minutes, 2, hours!=0);
	if (hours)
	{
		WI_DrawCharPatch (IntermissionFont, ':', x -= colon_spacing, y);
		WI_drawNum (IntermissionFont, x, y, hours, 2);
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

bool WI_autoSkip()
{
	return wi_autoadvance > 0 && bcnt > (wi_autoadvance * TICRATE);
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

	if (acceleratestage)
	{
		cnt = 0;
	}
	else
	{
		bool noauto = noautostartmap;
		bool autoskip = WI_autoSkip();

		for (int i = 0; !noauto && i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
			{
				noauto |= players[i].userinfo.GetNoAutostartMap();
			}
		}
		if (!noauto || autoskip)
		{
			cnt--;
		}
	}

	if (cnt == 0)
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

static int player_deaths[MAXPLAYERS];

void WI_initDeathmatchStats (void)
{
	int i, j;

	state = StatCount;
	acceleratestage = 0;
	memset(playerready, 0, sizeof(playerready));
	memset(cnt_frags, 0, sizeof(cnt_frags));
	memset(cnt_deaths, 0, sizeof(cnt_deaths));
	memset(player_deaths, 0, sizeof(player_deaths));
	total_frags = 0;
	total_deaths = 0;

	ng_state = 1;
	cnt_pause = TICRATE;

	for (i=0 ; i<MAXPLAYERS ; i++)
	{
		if (playeringame[i])
		{
			for (j = 0; j < MAXPLAYERS; j++)
				if (playeringame[j])
					player_deaths[i] += plrs[j].frags[i];
			total_deaths += player_deaths[i];
			total_frags += plrs[i].fragcount;
		}
	}
}

void WI_updateDeathmatchStats ()
{

	int i;
	bool stillticking;
	bool autoskip = WI_autoSkip();

	WI_updateAnimatedBack();

	if ((acceleratestage || autoskip) && ng_state != 6)
	{
		acceleratestage = 0;

		for (i = 0; i<MAXPLAYERS; i++)
		{
			if (!playeringame[i])
				continue;

			cnt_frags[i] = plrs[i].fragcount;
			cnt_deaths[i] = player_deaths[i];
		}
		S_Sound(CHAN_VOICE | CHAN_UI, "intermission/nextstage", 1, ATTN_NONE);
		ng_state = 6;
	}

	if (ng_state == 2)
	{
		if (!(bcnt & 3))
			S_Sound(CHAN_VOICE | CHAN_UI, "intermission/tick", 1, ATTN_NONE);

		stillticking = false;

		for (i = 0; i<MAXPLAYERS; i++)
		{
			if (!playeringame[i])
				continue;

			cnt_frags[i] += 2;

			if (cnt_frags[i] > plrs[i].fragcount)
				cnt_frags[i] = plrs[i].fragcount;
			else
				stillticking = true;
		}

		if (!stillticking)
		{
			S_Sound(CHAN_VOICE | CHAN_UI, "intermission/nextstage", 1, ATTN_NONE);
			ng_state++;
		}
	}
	else if (ng_state == 4)
	{
		if (!(bcnt & 3))
			S_Sound(CHAN_VOICE | CHAN_UI, "intermission/tick", 1, ATTN_NONE);

		stillticking = false;

		for (i = 0; i<MAXPLAYERS; i++)
		{
			if (!playeringame[i])
				continue;

			cnt_deaths[i] += 2;
			if (cnt_deaths[i] > player_deaths[i])
				cnt_deaths[i] = player_deaths[i];
			else
				stillticking = true;
		}
		if (!stillticking)
		{
			S_Sound(CHAN_VOICE | CHAN_UI, "intermission/nextstage", 1, ATTN_NONE);
			ng_state++;
		}
	}
	else if (ng_state == 6)
	{
		int i;
		for (i = 0; i < MAXPLAYERS; i++)
		{
			// If the player is in the game and not ready, stop checking
			if (playeringame[i] && players[i].Bot == NULL && !playerready[i])
				break;
		}

		// All players are ready; proceed.
		if ((i == MAXPLAYERS && acceleratestage) || autoskip)
		{
			S_Sound(CHAN_VOICE | CHAN_UI, "intermission/pastdmstats", 1, ATTN_NONE);
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



void WI_drawDeathmatchStats ()
{
	int i, pnum, x, y, ypadding, height, lineheight;
	int maxnamewidth, maxscorewidth, maxiconheight;
	int pwidth = IntermissionFont->GetCharWidth('%');
	int icon_x, name_x, frags_x, deaths_x;
	int deaths_len;
	float h, s, v, r, g, b;
	EColorRange color;
	const char *text_deaths, *text_frags;
	FTexture *readyico = TexMan.FindTexture("READYICO");
	player_t *sortedplayers[MAXPLAYERS];

	// draw animated background
	WI_drawBackground();

	y = WI_drawLF();

	HU_GetPlayerWidths(maxnamewidth, maxscorewidth, maxiconheight);
	// Use the readyico height if it's bigger.
	height = readyico->GetScaledHeight() - readyico->GetScaledTopOffset();
	maxiconheight = MAX(height, maxiconheight);
	height = SmallFont->GetHeight() * CleanYfac;
	lineheight = MAX(height, maxiconheight * CleanYfac);
	ypadding = (lineheight - height + 1) / 2;
	y += CleanYfac;

	text_deaths = GStrings("SCORE_DEATHS");
	//text_color = GStrings("SCORE_COLOR");
	text_frags = GStrings("SCORE_FRAGS");

	icon_x = 8 * CleanXfac;
	name_x = icon_x + maxscorewidth * CleanXfac;
	frags_x = name_x + (maxnamewidth + MAX(SmallFont->StringWidth("XXXXX"), SmallFont->StringWidth(text_frags)) + 8) * CleanXfac;
	deaths_x = frags_x + ((deaths_len = SmallFont->StringWidth(text_deaths)) + 8) * CleanXfac;

	x = (SCREENWIDTH - deaths_x) >> 1;
	icon_x += x;
	name_x += x;
	frags_x += x;
	deaths_x += x;

	color = (gameinfo.gametype & GAME_Raven) ? CR_GREEN : CR_UNTRANSLATED;

	screen->DrawText(SmallFont, color, name_x, y, GStrings("SCORE_NAME"), DTA_CleanNoMove, true, TAG_DONE);
	screen->DrawText(SmallFont, color, frags_x - SmallFont->StringWidth(text_frags)*CleanXfac, y, text_frags, DTA_CleanNoMove, true, TAG_DONE);
	screen->DrawText(SmallFont, color, deaths_x - deaths_len*CleanXfac, y, text_deaths, DTA_CleanNoMove, true, TAG_DONE);
	y += height + 6 * CleanYfac;

	// Sort all players
	for (i = 0; i < MAXPLAYERS; i++)
	{
		sortedplayers[i] = &players[i];
	}

	if (teamplay)
		qsort(sortedplayers, MAXPLAYERS, sizeof(player_t *), compareteams);
	else
		qsort(sortedplayers, MAXPLAYERS, sizeof(player_t *), comparepoints);

	// Draw lines for each player
	for (i = 0; i < MAXPLAYERS; i++)
	{
		player_t *player = sortedplayers[i];
		pnum = int(player - players);

		if (!playeringame[pnum])
			continue;

		D_GetPlayerColor(pnum, &h, &s, &v, NULL);
		HSVtoRGB(&r, &g, &b, h, s, v);

		screen->Dim(MAKERGB(clamp(int(r*255.f), 0, 255), 
			clamp(int(g*255.f), 0, 255), 
			clamp(int(b*255.f), 0, 255)), 0.8f, x, y - ypadding, (deaths_x - x) + (8 * CleanXfac), lineheight);

		if (playerready[pnum] || player->Bot != NULL) // Bots are automatically assumed ready, to prevent confusion
			screen->DrawTexture(readyico, x - (readyico->GetWidth() * CleanXfac), y, DTA_CleanNoMove, true, TAG_DONE);

		color = (EColorRange)HU_GetRowColor(player, pnum == consoleplayer);
		if (player->mo->ScoreIcon.isValid())
		{
			FTexture *pic = TexMan[player->mo->ScoreIcon];
			screen->DrawTexture(pic, icon_x, y, DTA_CleanNoMove, true, TAG_DONE);
		}
		screen->DrawText(SmallFont, color, name_x, y + ypadding, player->userinfo.GetName(), DTA_CleanNoMove, true, TAG_DONE);
		WI_drawNum(SmallFont, frags_x, y + ypadding, cnt_frags[pnum], 0, false, color);
		if (ng_state >= 2)
		{
			WI_drawNum(SmallFont, deaths_x, y + ypadding, cnt_deaths[pnum], 0, false, color);
		}
		y += lineheight + CleanYfac;
	}

	// Draw "TOTAL" line
	y += height + 3 * CleanYfac;
	color = (gameinfo.gametype & GAME_Raven) ? CR_GREEN : CR_UNTRANSLATED;
	screen->DrawText(SmallFont, color, name_x, y, GStrings("SCORE_TOTAL"), DTA_CleanNoMove, true, TAG_DONE);
	WI_drawNum(SmallFont, frags_x, y, total_frags, 0, false, color);
	if (ng_state >= 4)
	{
		WI_drawNum(SmallFont, deaths_x, y, total_deaths, 0, false, color);
	}

	// Draw game time
	y += height + CleanYfac;

	int seconds = Tics2Seconds(plrs[me].stime);
	int hours = seconds / 3600;
	int minutes = (seconds % 3600) / 60;
	seconds = seconds % 60;

	FString leveltime = GStrings("SCORE_LVLTIME");
	leveltime += ": ";

	char timer[sizeof "HH:MM:SS"];
	mysnprintf(timer, sizeof(timer), "%02i:%02i:%02i", hours, minutes, seconds);
	leveltime += timer;

	screen->DrawText(SmallFont, color, x, y, leveltime, DTA_CleanNoMove, true, TAG_DONE);
}

void WI_initNetgameStats ()
{

	int i;

	state = StatCount;
	acceleratestage = 0;
	memset(playerready, 0, sizeof(playerready));
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
	bool autoskip = WI_autoSkip();

	WI_updateAnimatedBack ();

	if ((acceleratestage || autoskip) && ng_state != 10)
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
		S_Sound (CHAN_VOICE | CHAN_UI, "intermission/nextstage", 1, ATTN_NONE);
		ng_state = 10;
	}

	if (ng_state == 2)
	{
		if (!(bcnt&3))
			S_Sound (CHAN_VOICE | CHAN_UI, "intermission/tick", 1, ATTN_NONE);

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
			S_Sound (CHAN_VOICE | CHAN_UI, "intermission/nextstage", 1, ATTN_NONE);
			ng_state++;
		}
	}
	else if (ng_state == 4)
	{
		if (!(bcnt&3))
			S_Sound (CHAN_VOICE | CHAN_UI, "intermission/tick", 1, ATTN_NONE);

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
			S_Sound (CHAN_VOICE | CHAN_UI, "intermission/nextstage", 1, ATTN_NONE);
			ng_state++;
		}
	}
	else if (ng_state == 6)
	{
		if (!(bcnt&3))
			S_Sound (CHAN_VOICE | CHAN_UI, "intermission/tick", 1, ATTN_NONE);

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
			S_Sound (CHAN_VOICE | CHAN_UI, "intermission/nextstage", 1, ATTN_NONE);
			ng_state += 1 + 2*!dofrags;
		}
	}
	else if (ng_state == 8)
	{
		if (!(bcnt&3))
			S_Sound (CHAN_VOICE | CHAN_UI, "intermission/tick", 1, ATTN_NONE);

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
			S_Sound (CHAN_VOICE | CHAN_UI, "intermission/cooptotal", 1, ATTN_NONE);
			ng_state++;
		}
	}
	else if (ng_state == 10)
	{
		int i;
		for (i = 0; i < MAXPLAYERS; i++)
		{
			// If the player is in the game and not ready, stop checking
			if (playeringame[i] && players[i].Bot == NULL && !playerready[i])
				break;
		}

		// All players are ready; proceed.
		if ((i == MAXPLAYERS && acceleratestage) || autoskip)
		{
			S_Sound (CHAN_VOICE | CHAN_UI, "intermission/pastcoopstats", 1, ATTN_NONE);
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
	int i, x, y, ypadding, height, lineheight;
	int maxnamewidth, maxscorewidth, maxiconheight;
	int pwidth = IntermissionFont->GetCharWidth('%');
	int icon_x, name_x, kills_x, bonus_x, secret_x;
	int bonus_len, secret_len;
	int missed_kills, missed_items, missed_secrets;
	float h, s, v, r, g, b;
	EColorRange color;
	const char *text_bonus, *text_secret, *text_kills;
	FTexture *readyico = TexMan.FindTexture("READYICO");

	// draw animated background
	WI_drawBackground(); 

	y = WI_drawLF();

	HU_GetPlayerWidths(maxnamewidth, maxscorewidth, maxiconheight);
	// Use the readyico height if it's bigger.
	height = readyico->GetScaledHeight() - readyico->GetScaledTopOffset();
	if (height > maxiconheight)
	{
		maxiconheight = height;
	}
	height = SmallFont->GetHeight() * CleanYfac;
	lineheight = MAX(height, maxiconheight * CleanYfac);
	ypadding = (lineheight - height + 1) / 2;
	y += CleanYfac;

	text_bonus = GStrings((gameinfo.gametype & GAME_Raven) ? "SCORE_BONUS" : "SCORE_ITEMS");
	text_secret = GStrings("SCORE_SECRET");
	text_kills = GStrings("SCORE_KILLS");

	icon_x = 8 * CleanXfac;
	name_x = icon_x + maxscorewidth * CleanXfac;
	kills_x = name_x + (maxnamewidth + MAX(SmallFont->StringWidth("XXXXX"), SmallFont->StringWidth(text_kills)) + 8) * CleanXfac;
	bonus_x = kills_x + ((bonus_len = SmallFont->StringWidth(text_bonus)) + 8) * CleanXfac;
	secret_x = bonus_x + ((secret_len = SmallFont->StringWidth(text_secret)) + 8) * CleanXfac;

	x = (SCREENWIDTH - secret_x) >> 1;
	icon_x += x;
	name_x += x;
	kills_x += x;
	bonus_x += x;
	secret_x += x;

	color = (gameinfo.gametype & GAME_Raven) ? CR_GREEN : CR_UNTRANSLATED;

	screen->DrawText(SmallFont, color, name_x, y, GStrings("SCORE_NAME"), DTA_CleanNoMove, true, TAG_DONE);
	screen->DrawText(SmallFont, color, kills_x - SmallFont->StringWidth(text_kills)*CleanXfac, y, text_kills, DTA_CleanNoMove, true, TAG_DONE);
	screen->DrawText(SmallFont, color, bonus_x - bonus_len*CleanXfac, y, text_bonus, DTA_CleanNoMove, true, TAG_DONE);
	screen->DrawText(SmallFont, color, secret_x - secret_len*CleanXfac, y, text_secret, DTA_CleanNoMove, true, TAG_DONE);
	y += height + 6 * CleanYfac;

	missed_kills = wbs->maxkills;
	missed_items = wbs->maxitems;
	missed_secrets = wbs->maxsecret;

	// Draw lines for each player
	for (i = 0; i < MAXPLAYERS; ++i)
	{
		player_t *player;

		if (!playeringame[i])
			continue;

		player = &players[i];

		D_GetPlayerColor(i, &h, &s, &v, NULL);
		HSVtoRGB(&r, &g, &b, h, s, v);

		screen->Dim(MAKERGB(clamp(int(r*255.f), 0, 255),
			clamp(int(g*255.f), 0, 255),
			clamp(int(b*255.f), 0, 255)), 0.8f, x, y - ypadding, (secret_x - x) + (8 * CleanXfac), lineheight);

		if (playerready[i] || player->Bot != NULL) // Bots are automatically assumed ready, to prevent confusion
			screen->DrawTexture(readyico, x - (readyico->GetWidth() * CleanXfac), y, DTA_CleanNoMove, true, TAG_DONE);

		color = (EColorRange)HU_GetRowColor(player, i == consoleplayer);
		if (player->mo->ScoreIcon.isValid())
		{
			FTexture *pic = TexMan[player->mo->ScoreIcon];
			screen->DrawTexture(pic, icon_x, y, DTA_CleanNoMove, true, TAG_DONE);
		}
		screen->DrawText(SmallFont, color, name_x, y + ypadding, player->userinfo.GetName(), DTA_CleanNoMove, true, TAG_DONE);
		WI_drawPercent(SmallFont, kills_x, y + ypadding, cnt_kills[i], wbs->maxkills, false, color);
		missed_kills -= cnt_kills[i];
		if (ng_state >= 4)
		{
			WI_drawPercent(SmallFont, bonus_x, y + ypadding, cnt_items[i], wbs->maxitems, false, color);
			missed_items -= cnt_items[i];
			if (ng_state >= 6)
			{
				WI_drawPercent(SmallFont, secret_x, y + ypadding, cnt_secret[i], wbs->maxsecret, false, color);
				missed_secrets -= cnt_secret[i];
			}
		}
		y += lineheight + CleanYfac;
	}

	// Draw "MISSED" line
	y += 3 * CleanYfac;
	screen->DrawText(SmallFont, CR_DARKGRAY, name_x, y, GStrings("SCORE_MISSED"), DTA_CleanNoMove, true, TAG_DONE);
	WI_drawPercent(SmallFont, kills_x, y, missed_kills, wbs->maxkills, false, CR_DARKGRAY);
	if (ng_state >= 4)
	{
		WI_drawPercent(SmallFont, bonus_x, y, missed_items, wbs->maxitems, false, CR_DARKGRAY);
		if (ng_state >= 6)
		{
			WI_drawPercent(SmallFont, secret_x, y, missed_secrets, wbs->maxsecret, false, CR_DARKGRAY);
		}
	}

	// Draw "TOTAL" line
	y += height + 3 * CleanYfac;
	color = (gameinfo.gametype & GAME_Raven) ? CR_GREEN : CR_UNTRANSLATED;
	screen->DrawText(SmallFont, color, name_x, y, GStrings("SCORE_TOTAL"), DTA_CleanNoMove, true, TAG_DONE);
	WI_drawNum(SmallFont, kills_x, y, wbs->maxkills, 0, false, color);
	if (ng_state >= 4)
	{
		WI_drawNum(SmallFont, bonus_x, y, wbs->maxitems, 0, false, color);
		if (ng_state >= 6)
		{
			WI_drawNum(SmallFont, secret_x, y, wbs->maxsecret, 0, false, color);
		}
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

	if (acceleratestage && sp_state != 10)
	{
		acceleratestage = 0;
		sp_state = 10;
		S_Sound (CHAN_VOICE | CHAN_UI, "intermission/nextstage", 1, ATTN_NONE);

		cnt_kills[0] = plrs[me].skills;
		cnt_items[0] = plrs[me].sitems;
		cnt_secret[0] = plrs[me].ssecret;
		cnt_time = Tics2Seconds(plrs[me].stime);
		cnt_par = wbs->partime / TICRATE;
		cnt_total_time = Tics2Seconds(wbs->totaltime);
	}

	if (sp_state == 2)
	{
		if (gameinfo.intermissioncounter)
		{
			cnt_kills[0] += 2;

			if (!(bcnt&3))
				S_Sound (CHAN_VOICE | CHAN_UI, "intermission/tick", 1, ATTN_NONE);
		}
		if (!gameinfo.intermissioncounter || cnt_kills[0] >= plrs[me].skills)
		{
			cnt_kills[0] = plrs[me].skills;
			S_Sound (CHAN_VOICE | CHAN_UI, "intermission/nextstage", 1, ATTN_NONE);
			sp_state++;
		}
	}
	else if (sp_state == 4)
	{
		if (gameinfo.intermissioncounter)
		{
			cnt_items[0] += 2;

			if (!(bcnt&3))
				S_Sound (CHAN_VOICE | CHAN_UI, "intermission/tick", 1, ATTN_NONE);
		}
		if (!gameinfo.intermissioncounter || cnt_items[0] >= plrs[me].sitems)
		{
			cnt_items[0] = plrs[me].sitems;
			S_Sound (CHAN_VOICE | CHAN_UI, "intermission/nextstage", 1, ATTN_NONE);
			sp_state++;
		}
	}
	else if (sp_state == 6)
	{
		if (gameinfo.intermissioncounter)
		{
			cnt_secret[0] += 2;

			if (!(bcnt&3))
				S_Sound (CHAN_VOICE | CHAN_UI, "intermission/tick", 1, ATTN_NONE);
		}
		if (!gameinfo.intermissioncounter || cnt_secret[0] >= plrs[me].ssecret)
		{
			cnt_secret[0] = plrs[me].ssecret;
			S_Sound (CHAN_VOICE | CHAN_UI, "intermission/nextstage", 1, ATTN_NONE);
			sp_state++;
		}
	}
	else if (sp_state == 8)
	{
		if (gameinfo.intermissioncounter)
		{
			if (!(bcnt&3))
				S_Sound (CHAN_VOICE | CHAN_UI, "intermission/tick", 1, ATTN_NONE);

			cnt_time += 3;
			cnt_par += 3;
			cnt_total_time += 3;
		}

		int sec = Tics2Seconds(plrs[me].stime);
		if (!gameinfo.intermissioncounter || cnt_time >= sec)
			cnt_time = sec;

		int tsec = Tics2Seconds(wbs->totaltime);
		if (!gameinfo.intermissioncounter || cnt_total_time >= tsec)
			cnt_total_time = tsec;

		if (!gameinfo.intermissioncounter || cnt_par >= wbs->partime / TICRATE)
		{
			cnt_par = wbs->partime / TICRATE;

			if (cnt_time >= sec)
			{
				cnt_total_time = tsec;
				S_Sound (CHAN_VOICE | CHAN_UI, "intermission/nextstage", 1, ATTN_NONE);
				sp_state++;
			}
		}
	}
	else if (sp_state == 10)
	{
		if (acceleratestage)
		{
			S_Sound (CHAN_VOICE | CHAN_UI, "intermission/paststats", 1, ATTN_NONE);
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

	lh = IntermissionFont->GetHeight() * 3 / 2;

	// draw animated background
	WI_drawBackground(); 
	
	WI_drawLF();
	
	if (gameinfo.gametype & GAME_DoomChex)
	{
		screen->DrawTexture (kills, SP_STATSX, SP_STATSY, DTA_Clean, true, TAG_DONE);
		WI_drawPercent (IntermissionFont, 320 - SP_STATSX, SP_STATSY, cnt_kills[0], wbs->maxkills);

		screen->DrawTexture (items, SP_STATSX, SP_STATSY+lh, DTA_Clean, true, TAG_DONE);
		WI_drawPercent (IntermissionFont, 320 - SP_STATSX, SP_STATSY+lh, cnt_items[0], wbs->maxitems);

		screen->DrawTexture (sp_secret, SP_STATSX, SP_STATSY+2*lh, DTA_Clean, true, TAG_DONE);
		WI_drawPercent (IntermissionFont, 320 - SP_STATSX, SP_STATSY+2*lh, cnt_secret[0], wbs->maxsecret);

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
		screen->DrawText (BigFont, CR_UNTRANSLATED, 50, 65, GStrings("TXT_IMKILLS"), DTA_Clean, true, DTA_Shadow, true, TAG_DONE);
		screen->DrawText (BigFont, CR_UNTRANSLATED, 50, 90, GStrings("TXT_IMITEMS"), DTA_Clean, true, DTA_Shadow, true, TAG_DONE);
		screen->DrawText (BigFont, CR_UNTRANSLATED, 50, 115, GStrings("TXT_IMSECRETS"), DTA_Clean, true, DTA_Shadow, true, TAG_DONE);

		int countpos = gameinfo.gametype==GAME_Strife? 285:270;
		if (sp_state >= 2)
		{
			WI_drawPercent (IntermissionFont, countpos, 65, cnt_kills[0], wbs->maxkills);
		}
		if (sp_state >= 4)
		{
			WI_drawPercent (IntermissionFont, countpos, 90, cnt_items[0], wbs->maxitems);
		}
		if (sp_state >= 6)
		{
			WI_drawPercent (IntermissionFont, countpos, 115, cnt_secret[0], wbs->maxsecret);
		}
		if (sp_state >= 8)
		{
			screen->DrawText (BigFont, CR_UNTRANSLATED, 85, 160, GStrings("TXT_IMTIME"),
				DTA_Clean, true, DTA_Shadow, true, TAG_DONE);
			WI_drawTime (249, 160, cnt_time);
			if (wi_showtotaltime)
			{
				WI_drawTime (249, 180, cnt_total_time);
			}
		}
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
					== players[i].oldbuttons) && player->Bot == NULL)
			{
				acceleratestage = 1;
				playerready[i] = true;
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
		if (level.info->InterMusic.IsNotEmpty()) 
			S_ChangeMusic(level.info->InterMusic, level.info->intermusicorder);
		else
			S_ChangeMusic (gameinfo.intermissionMusic.GetChars(), gameinfo.intermissionOrder); 

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
	entering.Init(gameinfo.mStatscreenEnteringFont);
	finished.Init(gameinfo.mStatscreenFinishedFont);
	mapname.Init(gameinfo.mStatscreenMapNameFont);

	if (gameinfo.gametype & GAME_DoomChex)
	{
		kills = TexMan["WIOSTK"];		// "kills"
		secret = TexMan["WIOSTS"];		// "scrt"
		sp_secret = TexMan["WISCRT2"];	// "secret"
		items = TexMan["WIOSTI"];		// "items"
		frags = TexMan["WIFRGS"];		// "frgs"
		timepic = TexMan["WITIME"];		// "time"
		sucks = TexMan["WISUCKS"];		// "sucks"
		par = TexMan["WIPAR"];			// "par"
		killers = TexMan["WIKILRS"];	// "killers" (vertical]
		victims = TexMan["WIVCTMS"];	// "victims" (horiz]
		total = TexMan["WIMSTT"];		// "total"
//		star = TexMan["STFST01"];		// your face
//		bstar = TexMan["STFDEAD0"];		// dead face
 		p = TexMan["STPBANY"];
	}
#if 0
	else if (gameinfo.gametype & GAME_Raven)
	{
		if (gameinfo.gametype == GAME_Heretic)
		{
			star = TexMan["FACEA0"];
			bstar = TexMan["FACEB0"];
		}
		else
		{
			star = BigFont->GetChar('*', NULL);
			bstar = star;
		}
	}
	else // Strife needs some handling, too!
	{
		star = BigFont->GetChar('*', NULL);
		bstar = star;
	}
#endif

	// Use the local level structure which can be overridden by hubs
	lnametexts[0] = level.LevelName;		

	level_info_t *li = FindLevelInfo(wbs->next);
	if (li) lnametexts[1] = li->LookupLevelName();
	else lnametexts[1] = "";

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

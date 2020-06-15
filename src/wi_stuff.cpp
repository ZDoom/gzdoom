/*
** wi_stuff.cpp
** Support code for intermission status screens
**
**---------------------------------------------------------------------------
** Copyright 2003-2017 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include <ctype.h>
#include <stdio.h>

#include "m_random.h"
#include "m_swap.h"

#include "filesystem.h"
#include "g_level.h"
#include "s_sound.h"
#include "doomstat.h"
#include "v_video.h"
#include "i_video.h"
#include "wi_stuff.h"
#include "hu_stuff.h"
#include "s_sndseq.h"
#include "v_text.h"
#include "gi.h"
#include "d_player.h"
#include "d_netinf.h"
#include "cmdlib.h"
#include "g_levellocals.h"
#include "vm.h"
#include "texturemanager.h"
#include "v_draw.h"

CVAR(Bool, wi_percents, true, CVAR_ARCHIVE)
CVAR(Bool, wi_showtotaltime, true, CVAR_ARCHIVE)
CVAR(Bool, wi_noautostartmap, false, CVAR_USERINFO | CVAR_ARCHIVE)
CVAR(Int, wi_autoadvance, 0, CVAR_SERVERINFO)

// States for the intermission
enum EState
{
	NoState = -1,
	StatCount,
	ShowNextLoc,
	LeavingIntermission
};

static const char *WI_Cmd[] = {
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

class DInterBackground : public DObject
{
	DECLARE_ABSTRACT_CLASS(DInterBackground, DObject)
	
	// These animation variables, structures, etc. are used for the
	// DOOM/Ultimate DOOM intermission screen animations.  This is
	// totally different from any sprite or texture/flat animations
	enum EAnim
	{
		ANIM_ALWAYS,	// determined by patch entry
		ANIM_PIC,		// continuous

		// condition bitflags
		ANIM_IFVISITED = 8,
		ANIM_IFNOTVISITED = 16,
		ANIM_IFENTERING = 32,
		ANIM_IFNOTENTERING = 64,
		ANIM_IFLEAVING = 128,
		ANIM_IFNOTLEAVING = 256,
		ANIM_IFTRAVELLING = 512,
		ANIM_IFNOTTRAVELLING = 1024,

		ANIM_TYPE = 7,
		ANIM_CONDITION = ~7,

	};

	struct yahpt_t
	{
		int x, y;
	};

	struct lnode_t
	{
		int   x;       // x/y coordinate pair structure
		int   y;
		FString Level;
	};

	struct in_anim_t
	{
		int			type;	// Made an int so I can use '|'
		int 		period;	// period in tics between animations
		yahpt_t 	loc;	// location of animation
		int 		data;	// ALWAYS: n/a, RANDOM: period deviation (<256)
		TArray<FGameTexture*>	frames;	// actual graphics for frames of animations

									// following must be initialized to zero before use!
		int 		nexttic;	// next value of bcnt (used in conjunction with period)
		int 		ctr;		// next frame number to animate
		int 		state;		// used by RANDOM and LEVEL when animating

		FString		LevelName;
		FString		LevelName2;

		void Reset()
		{
			type = period = loc.x = loc.y = data = nexttic = ctr = state = 0;
			LevelName = "";
			LevelName2 = "";
			frames.Clear();
		}
	};

private:
	TArray<lnode_t> lnodes;
	TArray<in_anim_t> anims;
	int				bcnt = 0;				// used for timing of background animation
	TArray<FGameTexture *> yah; 		// You Are Here graphic
	FGameTexture* 	splat = nullptr;		// splat
	FGameTexture	*background = nullptr;
	wbstartstruct_t *wbs;
	level_info_t	*exitlevel;
	
public:

	DInterBackground(wbstartstruct_t *wbst);
	bool LoadBackground(bool isenterpic);
	void updateAnimatedBack();
	void drawBackground(int state, bool drawsplat, bool snl_pointeron);

private:

	bool IsExMy(const char * name)
	{
		// Only check for the first 3 episodes. They are the only ones with default intermission scripts.
		// Level names can be upper- and lower case so use tolower to check.
		return (tolower(name[0]) == 'e' && name[1] >= '1' && name[1] <= '3' && tolower(name[2]) == 'm');
	}
	
	//====================================================================
	//
	// Draws the splats and the 'You are here' arrows
	//
	//====================================================================

	int MapToIndex(const char *map)
	{
		unsigned int i;

		for (i = 0; i < lnodes.Size(); i++)
		{
			if (!lnodes[i].Level.CompareNoCase(map))
				break;
		}
		return i;
	}


	//====================================================================
	//
	// Draws the splats and the 'You are here' arrows
	//
	//====================================================================

	void drawOnLnode(int   n, FGameTexture * c[], int numc)
	{
		int   i;
		for (i = 0; i<numc; i++)
		{
			double            left;
			double            top;
			double            right;
			double            bottom;


			right = c[i]->GetDisplayWidth();
			bottom = c[i]->GetDisplayHeight();
			left = lnodes[n].x - c[i]->GetDisplayLeftOffset();
			top = lnodes[n].y - c[i]->GetDisplayTopOffset();
			right += left;
			bottom += top;

			if (left >= 0 && right < 320 && top >= 0 && bottom < 200)
			{
				DrawTexture(twod, c[i], lnodes[n].x, lnodes[n].y, DTA_320x200, true, TAG_DONE);
				break;
			}
		}
	}
};

DInterBackground:: DInterBackground(wbstartstruct_t *wbst)
{
	wbs = wbst;
}

DEFINE_ACTION_FUNCTION(DInterBackground, Create)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(wbst, wbstartstruct_t);
	ACTION_RETURN_POINTER(Create<DInterBackground>(wbst));
}

//====================================================================
// 
//	Loads the background - either from a single texture
//	or an intermission lump.
//	Unfortunately the texture manager is incapable of recognizing text
//	files so if you use a script you have to prefix its name by '$' in 
//  MAPINFO.
//
//====================================================================

bool DInterBackground::LoadBackground(bool isenterpic)
{
	const char *lumpname = nullptr;
	const char *exitpic = nullptr;
	char buffer[10];
	in_anim_t an;
	lnode_t pt;
	FTextureID texture;
	bool noautostartmap = false;

	bcnt = 0;

	texture.SetInvalid();

	level_info_t * li = FindLevelInfo(wbs->current);
	if (li != nullptr) exitpic = li->ExitPic;
	lumpname = exitpic;

	if (isenterpic)
	{
		level_info_t * li = FindLevelInfo(wbs->next);
		if (li != NULL) lumpname = li->EnterPic;
	}

	// Try to get a default if nothing specified
	if (lumpname == NULL || lumpname[0] == 0)
	{
		lumpname = NULL;
		switch (gameinfo.gametype)
		{
		case GAME_Chex:
		case GAME_Doom:
			if (!(gameinfo.flags & GI_MAPxx))
			{
				const char *levelname = isenterpic ? wbs->next : wbs->current;
				if (IsExMy(levelname))
				{
					mysnprintf(buffer, countof(buffer), "$IN_EPI%c", levelname[1]);
					lumpname = buffer;
				}
			}
			if (!lumpname)
			{
				if (isenterpic)
				{
					// One special case needs to be handled here:
					// If going from E1-E3 to E4 the default should be used, not the exit pic.

					// Not if the exit pic is user defined.
					if (exitpic && exitpic[0]) return false;

					// E1-E3 need special treatment when playing Doom 1.
					if (!(gameinfo.flags & GI_MAPxx))
					{
						// not if the last level is not from the first 3 episodes
						if (!IsExMy(wbs->current)) return false;

						// not if the next level is one of the first 3 episodes
						if (IsExMy(wbs->next)) return false;
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
				if (isenterpic) return false;
				lumpname = "FLOOR16";
			}
			break;

		case GAME_Hexen:
			if (isenterpic) return false;
			lumpname = "INTERPIC";
			break;

		case GAME_Strife:
		default:
			// Strife doesn't have an intermission pic so choose something neutral.
			if (isenterpic) return false;
			lumpname = gameinfo.BorderFlat;
			break;
		}
	}
	if (lumpname == NULL)
	{
		// shouldn't happen!
		background = NULL;
		return false;
	}

	lnodes.Clear();
	anims.Clear();
	yah.Clear();
	splat = NULL;

	// a name with a starting '$' indicates an intermission script
	if (*lumpname != '$')
	{
		texture = TexMan.CheckForTexture(lumpname, ETextureType::MiscPatch, FTextureManager::TEXMAN_TryAny);
	}
	else
	{
		int lumpnum = fileSystem.CheckNumForFullName(lumpname + 1, true);
		if (lumpnum >= 0)
		{
			FScanner sc(lumpnum);
			while (sc.GetString())
			{
				an.Reset();
				int caseval = sc.MustMatchString(WI_Cmd);
				switch (caseval)
				{
				case 0:		// Background
					sc.MustGetString();
					texture = TexMan.CheckForTexture(sc.String, ETextureType::MiscPatch, FTextureManager::TEXMAN_TryAny);
					break;

				case 1:		// Splat
					sc.MustGetString();
					splat = TexMan.GetGameTextureByName(sc.String);
					break;

				case 2:		// Pointers
					while (sc.GetString() && !sc.Crossed)
					{
						yah.Push(TexMan.GetGameTextureByName(sc.String));
					}
					if (sc.Crossed)
						sc.UnGet();
					break;

				case 3:		// Spots
					sc.MustGetStringName("{");
					while (!sc.CheckString("}"))
					{
						sc.MustGetString();
						pt.Level = sc.String;
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
					an.LevelName2 = sc.String;
					goto readanimation;

				case 11:	// IfNotTravelling
					an.type = ANIM_IFTRAVELLING;
					sc.MustGetString();
					an.LevelName2 = sc.String;
					goto readanimation;

				case 14:	// NoAutostartMap
					noautostartmap = true;
					break;

				readanimation:
					sc.MustGetString();
					an.LevelName = sc.String;
					sc.MustGetString();
					caseval = sc.MustMatchString(WI_Cmd);

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
							an.frames.Push(TexMan.GetGameTextureByName(sc.String));
						}
						else
						{
							while (!sc.CheckString("}"))
							{
								sc.MustGetString();
								an.frames.Push(TexMan.GetGameTextureByName(sc.String));
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
						an.frames.Reserve(1);	// allocate exactly one element
						an.frames[0] = TexMan.GetGameTextureByName(sc.String);
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
			Printf("Intermission script %s not found!\n", lumpname + 1);
			texture = TexMan.GetTextureID("INTERPIC", ETextureType::MiscPatch);
		}
	}
	background = TexMan.GetGameTexture(texture);
	return noautostartmap;
}

DEFINE_ACTION_FUNCTION(DInterBackground, LoadBackground)
{
	PARAM_SELF_PROLOGUE(DInterBackground);
	PARAM_BOOL(isenterpic);
	ACTION_RETURN_BOOL(self->LoadBackground(isenterpic));
}

//====================================================================
// 
//	made this more generic and configurable through a script
//	Removed all the ugly special case handling for different game modes
//
//====================================================================

void DInterBackground::updateAnimatedBack()
{
	unsigned int i;

	bcnt++;
	for (i = 0; i<anims.Size(); i++)
	{
		in_anim_t * a = &anims[i];
		switch (a->type & ANIM_TYPE)
		{
		case ANIM_ALWAYS:
			if (bcnt >= a->nexttic)
			{
				if (++a->ctr >= (int)a->frames.Size())
				{
					if (a->data == 0) a->ctr = 0;
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

DEFINE_ACTION_FUNCTION(DInterBackground, updateAnimatedBack)
{
	PARAM_SELF_PROLOGUE(DInterBackground);
	self->updateAnimatedBack();
	return 0;
}

//====================================================================
// 
//	Draws the background including all animations
//
//====================================================================

void DInterBackground::drawBackground(int state, bool drawsplat, bool snl_pointeron)
{
	unsigned int i;
	double animwidth = 320;		// For a flat fill or clear background scale animations to 320x200
	double animheight = 200;

	if (background)
	{
		// background
		if (background->isMiscPatch())
		{
			// scale all animations below to fit the size of the base pic
			// The base pic is always scaled to fit the screen so this allows
			// placing the animations precisely where they belong on the base pic
			animwidth = background->GetDisplayWidth();
			animheight = background->GetDisplayHeight();
			if (gameinfo.fullscreenautoaspect > 0) animwidth = 320;	// deal with widescreen replacements that keep the original coordinates.
			DrawTexture(twod, background, 0, 0, DTA_Fullscreen, true, TAG_DONE);
		}
		else
		{
			twod->AddFlatFill(0, 0, twod->GetWidth(), twod->GetHeight(), background);
		}
	}
	else
	{
		ClearRect(twod, 0, 0, twod->GetWidth(), twod->GetHeight(), 0, 0);
	}

	for (i = 0; i<anims.Size(); i++)
	{
		in_anim_t * a = &anims[i];
		level_info_t *li;

		switch (a->type & ANIM_CONDITION)
		{
		case ANIM_IFVISITED:
			li = FindLevelInfo(a->LevelName);
			if (li == NULL || !(li->flags & LEVEL_VISITED)) continue;
			break;

		case ANIM_IFNOTVISITED:
			li = FindLevelInfo(a->LevelName);
			if (li == NULL || (li->flags & LEVEL_VISITED)) continue;
			break;

			// StatCount means 'leaving' - everything else means 'entering'!
		case ANIM_IFENTERING:
			if (state == StatCount || strnicmp(a->LevelName, wbs->next, 8)) continue;
			break;

		case ANIM_IFNOTENTERING:
			if (state != StatCount && !strnicmp(a->LevelName, wbs->next, 8)) continue;
			break;

		case ANIM_IFLEAVING:
			if (state != StatCount || strnicmp(a->LevelName, wbs->current, 8)) continue;
			break;

		case ANIM_IFNOTLEAVING:
			if (state == StatCount && !strnicmp(a->LevelName, wbs->current, 8)) continue;
			break;

		case ANIM_IFTRAVELLING:
			if (strnicmp(a->LevelName2, wbs->current, 8) || strnicmp(a->LevelName, wbs->next, 8)) continue;
			break;

		case ANIM_IFNOTTRAVELLING:
			if (!strnicmp(a->LevelName2, wbs->current, 8) && !strnicmp(a->LevelName, wbs->next, 8)) continue;
			break;
		}
		if (a->ctr >= 0)
			DrawTexture(twod, a->frames[a->ctr], a->loc.x, a->loc.y,
				DTA_VirtualWidthF, animwidth, DTA_VirtualHeightF, animheight, DTA_FullscreenScale, gameinfo.fullscreenautoaspect, TAG_DONE);
	}

	if (drawsplat)
	{
		for (i = 0; i<lnodes.Size(); i++)
		{
			level_info_t * li = FindLevelInfo(lnodes[i].Level);
			if (li && li->flags & LEVEL_VISITED) drawOnLnode(i, &splat, 1);  // draw a splat on taken cities.
		}
	}

	// draw flashing ptr
	if (snl_pointeron && yah.Size())
	{
		unsigned int v = MapToIndex(wbs->next);
		// Draw only if it points to a valid level on the current screen!
		if (v<lnodes.Size()) drawOnLnode(v, &yah[0], yah.Size());
	}
}

DEFINE_ACTION_FUNCTION(DInterBackground, drawBackground)
{
	PARAM_SELF_PROLOGUE(DInterBackground);
	PARAM_INT(state);
	PARAM_BOOL(splat);
	PARAM_BOOL(pointer);
	self->drawBackground(state, splat, pointer);
	return 0;
}

IMPLEMENT_CLASS(DInterBackground, true, false)

DObject *WI_Screen;

//====================================================================
// 
//
//
//====================================================================

void WI_Ticker()
{
	if (WI_Screen)
	{
		ScaleOverrider s(twod);
		IFVIRTUALPTRNAME(WI_Screen, "StatusScreen", Ticker)
		{
			VMValue self = WI_Screen;
			VMCall(func, &self, 1, nullptr, 0);
		}
	}
}

//====================================================================
// 
// Called by main loop,
// draws the intermission directly into the screen buffer.
//
//====================================================================

void WI_Drawer()
{
	if (WI_Screen)
	{
		ScaleOverrider s(twod);
		IFVIRTUALPTRNAME(WI_Screen, "StatusScreen", Drawer)
		{
			twod->ClearClipRect();
			twod->ClearScreen();
			VMValue self = WI_Screen;
			VMCall(func, &self, 1, nullptr, 0);
			twod->ClearClipRect();	// make sure the scripts don't leave a valid clipping rect behind.

			// The internal handling here is somewhat poor. After being set to 'LeavingIntermission'
			// the screen is needed for one more draw operation so we cannot delete it right away but only here.
			if (WI_Screen->IntVar("CurState") == LeavingIntermission)
			{
				WI_Screen->Destroy();
				GC::DelSoftRoot(WI_Screen);
				WI_Screen = nullptr;
			}
		}
	}
}

//====================================================================
// 
// Setup for an intermission screen.
//
//====================================================================

void WI_Start(wbstartstruct_t *wbstartstruct)
{
	FName screenclass = deathmatch ? gameinfo.statusscreen_dm : multiplayer ? gameinfo.statusscreen_coop : gameinfo.statusscreen_single;
	auto cls = PClass::FindClass(screenclass);

	if (cls == nullptr || !cls->IsDescendantOf("StatusScreen"))
	{
		// Name was invalid - pick some working default.
		Printf("Status screen class %s not found - reverting to default", screenclass.GetChars());
		screenclass = deathmatch ? NAME_DeathmatchStatusScreen : multiplayer ? NAME_CoopStatusScreen : NAME_RavenStatusScreen;
		cls = PClass::FindClass(screenclass);
		if (cls == nullptr)
		{
			I_FatalError("Cannot create status screen");
		}
	}
	
	WI_Screen = cls->CreateNew();
	ScaleOverrider s(twod);
	IFVIRTUALPTRNAME(WI_Screen, "StatusScreen", Start)
	{
		VMValue val[2] = { WI_Screen, wbstartstruct };
		VMCall(func, val, 2, nullptr, 0);
	}
	GC::AddSoftRoot(WI_Screen);
}

//====================================================================
// 
//
//
//====================================================================

DEFINE_ACTION_FUNCTION(DStatusScreen, GetPlayerWidths)
{
	PARAM_PROLOGUE;
	int maxnamewidth, maxscorewidth, maxiconheight;
	HU_GetPlayerWidths(maxnamewidth, maxscorewidth, maxiconheight);
	if (numret > 0) ret[0].SetInt(maxnamewidth);
	if (numret > 1) ret[1].SetInt(maxscorewidth);
	if (numret > 2) ret[2].SetInt(maxiconheight);
	return MIN(numret, 3);
}

//====================================================================
// 
//
//
//====================================================================

DEFINE_ACTION_FUNCTION(DStatusScreen, GetRowColor)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(p, player_t);
	PARAM_BOOL(highlight);
	ACTION_RETURN_INT(HU_GetRowColor(p, highlight));
}

//====================================================================
// 
//
//
//====================================================================

DEFINE_ACTION_FUNCTION(DStatusScreen, GetSortedPlayers)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(array, TArray<int>);
	PARAM_BOOL(teamplay);

	player_t *sortedplayers[MAXPLAYERS];
	// Sort all players
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		sortedplayers[i] = &players[i];
	}

	if (teamplay)
		qsort(sortedplayers, MAXPLAYERS, sizeof(player_t *), compareteams);
	else
		qsort(sortedplayers, MAXPLAYERS, sizeof(player_t *), comparepoints);

	array->Resize(MAXPLAYERS);
	for (unsigned i = 0; i < MAXPLAYERS; i++)
	{
		(*array)[i] = int(sortedplayers[i] - players);
	}
	return 0;
}

//====================================================================
// 
//
//
//====================================================================

DEFINE_FIELD_X(WBPlayerStruct, wbplayerstruct_t, skills);
DEFINE_FIELD_X(WBPlayerStruct, wbplayerstruct_t, sitems);
DEFINE_FIELD_X(WBPlayerStruct, wbplayerstruct_t, ssecret);
DEFINE_FIELD_X(WBPlayerStruct, wbplayerstruct_t, stime);
DEFINE_FIELD_X(WBPlayerStruct, wbplayerstruct_t, frags);
DEFINE_FIELD_X(WBPlayerStruct, wbplayerstruct_t, fragcount);

DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, finished_ep);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, next_ep);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, current);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, next);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, nextname);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, thisname);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, nextauthor);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, thisauthor);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, LName0);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, LName1);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, maxkills);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, maxitems);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, maxsecret);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, maxfrags);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, partime);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, sucktime);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, totaltime);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, pnum);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, plyr);


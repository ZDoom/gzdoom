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

#include "serializer.h"
#include "s_music.h"

CVAR(Bool, wi_percents, true, CVAR_ARCHIVE)
CVAR(Bool, wi_showtotaltime, true, CVAR_ARCHIVE)
CVAR(Bool, wi_noautostartmap, false, CVAR_USERINFO | CVAR_ARCHIVE)
CVAR(Int, wi_autoadvance, 0, CVAR_SERVERINFO)
CVAR(Bool, wi_cleantextscale, false, CVAR_ARCHIVE)
EXTERN_CVAR(Bool, inter_classic_scaling)

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
	"Screensize",
	"TileBackground",

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
		ANIM_FRAME,     // determined by frame properties
		ANIM_NONE,      // frozen (used for infinite duration frames)

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

	struct frame_t
	{
		FTextureID	image;
		uint32_t	type;
		// In tics!
		int         duration;
		int         max_duration;
	};

	enum ECondition
	{
		COND_NONE = 0, // None.
		COND_GREATER = 1, // Parameter is greater than current map number.
		COND_EQUAL = 2, // Parameter is equal to current map number.
		COND_VISITED = 3, // Map number corresponding to parameter is visited.
		COND_NOTSECRET = 4, // Current map is not a secret one.
		COND_SECRETVISTED = 5, // Any secret map was visited.
		COND_TALLY = 6, // Tally screen.
		COND_ENTERING = 7, // "Entering" screen.
	};

	struct condition_t
	{
		ECondition condition;
		int param;
	};

	struct in_anim_t
	{
		int			type;	// Made an int so I can use '|'
		int 		period;	// period in tics between animations
		yahpt_t 	loc;	// location of animation
		int 		data;	// ALWAYS: n/a, RANDOM: period deviation (<256)
		TArray<frame_t>	frames;	// actual graphics for frames of animations
		TArray<condition_t> conditions; // conditions to display the animation

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

	struct in_layer_t
	{
		TArray<in_anim_t> anims;
		TArray<condition_t> conditions; // conditions to display the animations.
	};

private:
	TArray<lnode_t> lnodes;
	TArray<in_layer_t> layers;
	int				bcnt = 0;				// used for timing of background animation
	TArray<FTextureID> yah; 		// You Are Here graphic
	FTextureID		splat{};		// splat
	FTextureID		background{};
	wbstartstruct_t *wbs;
	level_info_t	*exitlevel;
	int			bgwidth = -1;
	int			bgheight = -1;
	bool		tilebackground = false;
	FString		muslump;


public:

	DInterBackground(wbstartstruct_t *wbst);
	bool LoadBackground(bool isenterpic);
	bool IsUsingMusic();
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

	void drawOnLnode(int   n, FTextureID c[], int numc, double backwidth, double backheight)
	{
		int   i;
		for (i = 0; i<numc; i++)
		{
			double            left;
			double            top;
			double            right;
			double            bottom;

			auto tex = TexMan.GetGameTexture(c[i]);
			right = tex->GetDisplayWidth();
			bottom = tex->GetDisplayHeight();
			left = lnodes[n].x - tex->GetDisplayLeftOffset();
			top = lnodes[n].y - tex->GetDisplayTopOffset();
			right += left;
			bottom += top;

			if (left >= 0 && right < 320 && top >= 0 && bottom < 200)
			{
				DrawTexture(twod, tex, lnodes[n].x, lnodes[n].y, DTA_FullscreenScale, FSMode_ScaleToFit43, DTA_VirtualWidthF, backwidth, DTA_VirtualHeightF, backheight, TAG_DONE);
				break;
			}
		}
	}

	//====================================================================
	//
	// Draws the splats and the 'You are here' arrows
	//
	//====================================================================
	bool ConditionsMet(int state, TArray<condition_t>& conditions)
	{
		for (auto& condition : conditions)
		{
			switch (condition.condition)
			{
				case ECondition::COND_NONE:
					break;
				case ECondition::COND_EQUAL:
				{
					auto* li = FindLevelInfo(state != StatCount ? wbs->next.GetChars() : wbs->current.GetChars());
					if (!li)
						return false;
					if (li->levelnum != condition.param)
						return false;
					break;
				}
				case ECondition::COND_GREATER:
				{
					auto* li = FindLevelInfo(state != StatCount ? wbs->next.GetChars() : wbs->current.GetChars());
					if (!li)
						return false;
					if (li->levelnum <= condition.param)
						return false;
					break;
				}
				case ECondition::COND_VISITED:
				{
					auto* li = FindLevelByNum(condition.param);
					if (li == NULL || !(li->flags & LEVEL_VISITED))
						return false;
					break;
				}
				case ECondition::COND_NOTSECRET:
				{
					auto* li = FindLevelInfo(state != StatCount ? wbs->next.GetChars() : wbs->current.GetChars());
					if (!li)
						return false;
					if (li->flags3 & LEVEL3_SECRET)
						return false;
					break;
				}
				case ECondition::COND_SECRETVISTED:
					if (!SecretLevelVisited())
						return false;
					break;
				case ECondition::COND_TALLY:
					if (state != StatCount)
						return false;
					break;
				case ECondition::COND_ENTERING:
					if (state == StatCount)
						return false;
					break;
			}
		}
		return true;
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
	const char* lumpname = nullptr;
	const char* exitpic = nullptr;
	const char* exitanim = nullptr;
	char buffer[10];
	in_anim_t an;
	lnode_t pt;
	FTextureID texture;
	bool noautostartmap = false;
	bool id24anim = false;

	bcnt = 0;

	if (!isenterpic) tilebackground = false;
	texture.SetInvalid();

	level_info_t* li = FindLevelInfo(wbs->current.GetChars());
	if (li != nullptr)
	{
		if (li->ExitAnim.IsNotEmpty())
		{
			id24anim = true;
			exitpic = li->ExitAnim.GetChars();
			tilebackground = false;
		}
		else
		{
			exitpic = li->ExitPic.GetChars();
			if (li->ExitPic.IsNotEmpty()) tilebackground = false;
		}
	}
	lumpname = exitpic;

	if (isenterpic)
	{
		level_info_t* li = FindLevelInfo(wbs->next.GetChars());
		if (li != NULL)
		{
			if (li->EnterAnim.IsNotEmpty())
			{
				id24anim = true;
				lumpname = li->EnterAnim.GetChars();
				tilebackground = false;
			}
			else
			{
				lumpname = li->EnterPic.GetChars();
				if (li->EnterPic.IsNotEmpty()) tilebackground = false;
			}
		}
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
				const char* levelname = isenterpic ? wbs->next.GetChars() : wbs->current.GetChars();
				if (IsExMy(levelname))
				{
					mysnprintf(buffer, countof(buffer), "$IN_EPI%c", levelname[1]);
					lumpname = buffer;
					tilebackground = false;
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
						if (!IsExMy(wbs->current.GetChars())) return false;

						// not if the next level is one of the first 3 episodes
						if (IsExMy(wbs->next.GetChars())) return false;
					}
				}
				lumpname = "INTERPIC";
				tilebackground = false;
			}
			break;

		case GAME_Heretic:
			if (isenterpic)
			{
				if (IsExMy(wbs->next.GetChars()))
				{
					mysnprintf(buffer, countof(buffer), "$IN_HTC%c", wbs->next[1]);
					lumpname = buffer;
					tilebackground = false;
				}
			}
			if (!lumpname)
			{
				if (isenterpic) return false;
				lumpname = "FLOOR16";
				tilebackground = true;
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
			lumpname = gameinfo.BorderFlat.GetChars();
			tilebackground = true;
			break;
		}
	}
	if (lumpname == NULL)
	{
		// shouldn't happen!
		background.SetInvalid();
		return false;
	}

	lnodes.Clear();
	layers.Clear();
	yah.Clear();
	splat.SetInvalid();
	if (!id24anim)
		layers.Resize(1);

	if (id24anim)
	{
		try
		{
			int lumpnum = fileSystem.CheckNumForFullName(lumpname, true);
			if (lumpnum == -1)
			{
				I_Error("Intermission animation lump %s not found!", lumpname);
			}
			auto data = fileSystem.ReadFile(lumpnum);
			FSerializer jsonReader;
			jsonReader.mLumpName = fileSystem.GetFileFullPath(lumpnum);
			lumpname = jsonReader.mLumpName.GetChars();
			if (jsonReader.OpenReader(data.string(), data.size()))
			{
				FString type = jsonReader.GetString("type");
				if (type.Compare("interlevel"))
				{
					I_Error("Interlevel lump %s is not interlevel!", lumpname);
				}
				if (jsonReader.BeginObject("data"))
				{
					FString music = jsonReader.GetString("music");
					if (music.IsEmpty())
					{
						I_Error("No music lump specified for intermission animation %s!", lumpname);
					}
					muslump = music;
					if (!MusicExists(muslump.GetChars()))
					{
						I_Error("Music lump %s not found!", muslump.GetChars());
					}
				
					// Check for background lump.
					FString backgroundimage = jsonReader.GetString("backgroundimage");
					texture = TexMan.CheckForTexture(backgroundimage.GetChars(), ETextureType::MiscPatch, FTextureManager::TEXMAN_TryAny);
					if (!texture.isValid())
					{
						if (backgroundimage.IsEmpty())
						{
							I_Error("No background image specified for intermission animation %s!", lumpname);
						}
						else
						{
							I_Error("Texture %s not found!", backgroundimage.GetChars());
						}
					}

					if (!jsonReader.IsKeyNull("layers") && jsonReader.BeginArray("layers"))
					{
						int size = jsonReader.ArraySize();

						if (size == 0)
							I_Error("Zero-length 'layers' array in %s", lumpname);

						for (int i = 0; i < size; i++)
						{
							if (jsonReader.BeginObject(nullptr))
							{
								layers.Reserve(1);
								auto& layer = layers.back();
								if (!jsonReader.IsKeyNull("conditions") && jsonReader.BeginArray("conditions"))
								{
									int condition_size = jsonReader.ArraySize();
									if (condition_size == 0)
									{
										I_Error("Condition array empty and not null for layer %d in lump %s", layers.Size(), lumpname);
									}
									for (int j = 0; j < condition_size; j++)
									{
										if (jsonReader.BeginObject(nullptr))
										{
											ECondition cond = COND_NONE;
											int param = 0;
											::Serialize(jsonReader, "condition", (int&)cond, nullptr);
											::Serialize(jsonReader, "param", param, nullptr);
											layer.conditions.Push(condition_t{ cond, param });
											jsonReader.EndObject();
										}
									}
									jsonReader.EndArray();
								}

								// Read anims.
								if (jsonReader.BeginArray("anims"))
								{
									int anim_size = jsonReader.ArraySize();
									if (anim_size == 0)
									{
										I_Error("No animations defined for layer %d in lump %s", layers.Size(), lumpname);
									}

									for (int j = 0; j < anim_size; j++)
									{
										if (jsonReader.BeginObject(nullptr))
										{
											layer.anims.Reserve(1);
											auto& anim = layer.anims.back();

											anim.Reset();
											anim.type = ANIM_FRAME;
											anim.ctr = 0;
											anim.data = 0;
										
											::Serialize(jsonReader, "x", anim.loc.x, nullptr);
											::Serialize(jsonReader, "y", anim.loc.y, nullptr);

											if (!jsonReader.IsKeyNull("conditions") && jsonReader.BeginArray("conditions"))
											{
												int condition_size = jsonReader.ArraySize();
												if (condition_size == 0)
												{
													I_Error("Condition array empty and not null for anim %d, layer %d in lump %s", layer.anims.Size(), layers.Size(), lumpname);
												}
												for (int k = 0; k < condition_size; k++)
												{
													if (jsonReader.BeginObject(nullptr))
													{
														ECondition cond = COND_NONE;
														int param = 0;
														::Serialize(jsonReader, "condition", (int&)cond, nullptr);
														::Serialize(jsonReader, "param", param, nullptr);
														anim.conditions.Push(condition_t{ cond, param });
														jsonReader.EndObject();
													}
												}
												jsonReader.EndArray();
											}

											if (jsonReader.BeginArray("frames"))
											{
												int frames = jsonReader.ArraySize();

												if (frames == 0)
												{
													I_Error("No frames defined for anim %d, layer %d in lump %s", layer.anims.Size(), layers.Size(), lumpname);
												}
												for (int k = 0; k < frames; k++)
												{
													if (jsonReader.BeginObject(nullptr))
													{
														double duration = 0.0;
														double maxduration = 0.0;
														anim.frames.Reserve(1);
														auto& frame = anim.frames.back();

														::Serialize(jsonReader, "duration", duration, nullptr);
														::Serialize(jsonReader, "maxduration", maxduration, nullptr);
														::Serialize(jsonReader, "type", frame.type, nullptr);

														FString image = jsonReader.GetString("image");
														if (image.IsEmpty())
														{
															I_Error("No image defined for frame %d, anim %d, layer %d in lump %s", anim.frames.Size(), layer.anims.Size(), layers.Size(), lumpname);
														}

														frame.image = TexMan.CheckForTexture(image.GetChars(), ETextureType::MiscPatch, FTextureManager::TEXMAN_TryAny);
														if (!frame.image.isValid())
														{
															I_Error("Texture '%s' not found!", image.GetChars());
														}

														frame.duration = round(duration * (double)TICRATE);
														frame.max_duration = round(maxduration * (double)TICRATE);

														if (frame.duration <= 0 && duration > 0.0)
														{
															frame.duration = 1;
														}

														if (frame.max_duration <= 0 && maxduration > 0.0)
														{
															frame.max_duration = 1;
														}

														if (anim.frames.Size() == 1)
														{
															if (frame.type & 0x1000)
															{
																frame.type &= ~0x1000;
																anim.nexttic = 1 + (M_Random() % frame.duration);
															}
															else
															{

																switch (frame.type)
																{
																case 0x1:
																	anim.type = ANIM_NONE;
																	break;
																case 0x4:
																	anim.nexttic = 1 + M_Random(frame.max_duration - frame.duration + 1) + frame.duration;
																	break;
																case 0x2:
																	anim.nexttic = 1 + frame.duration;
																	break;
																}
															}
														}

														jsonReader.EndObject();
													}
												}
												jsonReader.EndArray();
											}

											jsonReader.EndObject();
										}
									}

									jsonReader.EndArray();
								}

								jsonReader.EndObject();
							}
						}
						jsonReader.EndArray();
					}

					jsonReader.EndObject();
				}
				jsonReader.Close();
			}
		}
		// This is deliberate. Errors coming from here shouldn't cause a console abort.
		catch (CRecoverableError& error)
		{
			Printf(TEXTCOLOR_RED "Failed to parse intermission anim definition %s: %s.\nFalling back to non-anim definitions.\n", lumpname, error.GetMessage());

			if (isenterpic)
				li->EnterAnim = "";
			else
				li->ExitAnim = "";

			return LoadBackground(isenterpic);
		}
	}
	// a name with a starting '$' indicates an intermission script
	else if (*lumpname != '$')
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
					splat = TexMan.CheckForTexture(sc.String, ETextureType::MiscPatch, FTextureManager::TEXMAN_TryAny);
					break;

				case 2:		// Pointers
					while (sc.GetString() && !sc.Crossed)
					{
						yah.Push(TexMan.CheckForTexture(sc.String, ETextureType::MiscPatch, FTextureManager::TEXMAN_TryAny));
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

				case 15:	// screensize
					sc.MustGetNumber();
					bgwidth = sc.Number;
					sc.MustGetNumber();
					bgheight = sc.Number;
					break;

				case 16:	// tilebackground
					tilebackground = true;
					break;

				readanimation:
					sc.MustGetString();
					an.LevelName = sc.String;
					sc.MustGetString();
					caseval = sc.MustMatchString(WI_Cmd);
					[[fallthrough]];

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
							auto textureId = TexMan.CheckForTexture(sc.String, ETextureType::MiscPatch, FTextureManager::TEXMAN_TryAny);
							an.frames.Push(frame_t{ textureId, 0, 0, 0 });
						}
						else
						{
							while (!sc.CheckString("}"))
							{
								sc.MustGetString();
								auto textureId = TexMan.CheckForTexture(sc.String, ETextureType::MiscPatch, FTextureManager::TEXMAN_TryAny);
								an.frames.Push(frame_t{ textureId, 0, 0, 0 });
							}
						}
						an.ctr = -1;
						layers[0].anims.Push(an);
						break;

					case 13:		// Pic
						an.type |= ANIM_PIC;
						sc.MustGetNumber();
						an.loc.x = sc.Number;
						sc.MustGetNumber();
						an.loc.y = sc.Number;
						sc.MustGetString();
						an.frames.Reserve(1);	// allocate exactly one element
						an.frames[0].image = TexMan.CheckForTexture(sc.String, ETextureType::MiscPatch, FTextureManager::TEXMAN_TryAny);
						layers[0].anims.Push(an);
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
	background = texture;
	auto tex = TexMan.GetGameTexture(texture);
	// extremely small textures will always be tiled.
	if (tex && tex->GetDisplayWidth() < 128 && tex->GetDisplayHeight() < 128)
		tilebackground = true;
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
	if (bcnt == 1 && muslump.IsNotEmpty())
	{
		S_ChangeMusic(muslump.GetChars());
	}

	for (auto& layer : layers)
	{
		auto& anims = layer.anims;
		for (i = 0; i < anims.Size(); i++)
		{
			in_anim_t* a = &anims[i];
			switch (a->type & ANIM_TYPE)
			{
			case ANIM_NONE:
				break;

			case ANIM_FRAME:
				if (bcnt >= a->nexttic)
				{
					if (++a->ctr >= (int)a->frames.Size())
						a->ctr = 0;

					switch (a->frames[a->ctr].type & 0x7) {
					case 0x1:
						a->type &= ANIM_CONDITION;
						a->type = ANIM_NONE;
						break;
					case 0x4:
						a->nexttic = bcnt + M_Random(a->frames[a->ctr].max_duration - a->frames[a->ctr].duration + 1) + a->frames[a->ctr].duration;
						break;
					case 0x2:
						a->nexttic = bcnt + a->frames[a->ctr].duration;
						break;
					}
				}
				break;
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
	double animwidth = bgwidth;		// For a flat fill or clear background scale animations to 320x200
	double animheight = bgheight;

	if (background.isValid())
	{
		auto bgtex = TexMan.GetGameTexture(background);
		// background
		if (!tilebackground)
		{
			// if no explicit size was set scale all animations below to fit the size of the base pic
			// The base pic is always scaled to fit the screen so this allows
			// placing the animations precisely where they belong on the base pic
			if (bgwidth < 0 || bgheight < 0)
			{
				animwidth = bgtex->GetDisplayWidth();
				animheight = bgtex->GetDisplayHeight();
				if (animheight == 200) animwidth = 320;	// deal with widescreen replacements that keep the original coordinates.
			}
			DrawTexture(twod, bgtex, 0, 0, DTA_FullscreenEx, FSMode_ScaleToFit43, TAG_DONE);
		}
		else
		{
			twod->AddFlatFill(0, 0, twod->GetWidth(), twod->GetHeight(), bgtex, (inter_classic_scaling ? -1 : 0));
		}
	}
	else
	{
		ClearRect(twod, 0, 0, twod->GetWidth(), twod->GetHeight(), 0, 0);
	}

	for (auto& layer : layers)
	{
		auto& anims = layer.anims;

		if (!ConditionsMet(state, layer.conditions))
			continue;

		for (i = 0; i < anims.Size(); i++)
		{
			in_anim_t* a = &anims[i];
			level_info_t* li;

			switch (a->type & ANIM_CONDITION)
			{
			case ANIM_IFVISITED:
				li = FindLevelInfo(a->LevelName.GetChars());
				if (li == NULL || !(li->flags & LEVEL_VISITED)) continue;
				break;

			case ANIM_IFNOTVISITED:
				li = FindLevelInfo(a->LevelName.GetChars());
				if (li == NULL || (li->flags & LEVEL_VISITED)) continue;
				break;

				// StatCount means 'leaving' - everything else means 'entering'!
			case ANIM_IFENTERING:
				if (state == StatCount || a->LevelName.CompareNoCase(wbs->next, 8)) continue;
				break;

			case ANIM_IFNOTENTERING:
				if (state != StatCount && !a->LevelName.CompareNoCase(wbs->next, 8)) continue;
				break;

			case ANIM_IFLEAVING:
				if (state != StatCount || a->LevelName.CompareNoCase(wbs->current, 8)) continue;
				break;

			case ANIM_IFNOTLEAVING:
				if (state == StatCount && !a->LevelName.CompareNoCase(wbs->current, 8)) continue;
				break;

			case ANIM_IFTRAVELLING:
				if (a->LevelName2.CompareNoCase(wbs->current, 8) || a->LevelName.CompareNoCase(wbs->next, 8)) continue;
				break;

			case ANIM_IFNOTTRAVELLING:
				if (!a->LevelName2.CompareNoCase(wbs->current, 8) && !a->LevelName.CompareNoCase(wbs->next, 8)) continue;
				break;
			}
			if (a->ctr >= 0 && ConditionsMet(state, a->conditions))
				DrawTexture(twod, a->frames[a->ctr].image, false, a->loc.x, a->loc.y,
					DTA_VirtualWidthF, animwidth, DTA_VirtualHeightF, animheight, DTA_FullscreenScale, FSMode_ScaleToFit43, TAG_DONE);
		}
	}

	if (drawsplat)
	{
		for (i = 0; i<lnodes.Size(); i++)
		{
			level_info_t * li = FindLevelInfo(lnodes[i].Level.GetChars());
			if (li && li->flags & LEVEL_VISITED) drawOnLnode(i, &splat, 1, animwidth, animheight);  // draw a splat on taken cities.
		}
	}

	// draw flashing ptr
	if (snl_pointeron && yah.Size())
	{
		unsigned int v = MapToIndex(wbs->next.GetChars());
		// Draw only if it points to a valid level on the current screen!
		if (v<lnodes.Size()) drawOnLnode(v, &yah[0], yah.Size(), animwidth, animheight);
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

//====================================================================
//
//
//
//====================================================================

bool DInterBackground::IsUsingMusic()
{
	return muslump.IsNotEmpty();
}

DEFINE_ACTION_FUNCTION(DInterBackground, IsUsingMusic)
{
	PARAM_SELF_PROLOGUE(DInterBackground);
	ACTION_RETURN_BOOL(self->IsUsingMusic());
}

IMPLEMENT_CLASS(DInterBackground, true, false)

//====================================================================
// 
// Setup for an intermission screen.
//
//====================================================================

DObject* WI_Start(wbstartstruct_t *wbstartstruct)
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
	
	auto WI_Screen = cls->CreateNew();


	ScaleOverrider s(twod);
	IFVIRTUALPTRNAME(WI_Screen, "StatusScreen", Start)
	{
		VMValue val[2] = { WI_Screen, wbstartstruct };
		VMCall(func, val, 2, nullptr, 0);
	}

	if (!wi_cleantextscale)
	{
		// Only modify the original single player screens. Everything else must set itself up as intended
		if (cls->TypeName == NAME_DoomStatusScreen || cls->TypeName == NAME_RavenStatusScreen)
		{
			int w = screen->GetWidth();
			int h = screen->GetHeight();
			float ratio = ActiveRatio(w, h);
			int pixw = int(320 * (ratio * 0.75));
			if (pixw > 336) pixw -= 16;	// leave a bit of space at the sides.

			WI_Screen->IntVar(NAME_cwidth) = 320;
			WI_Screen->IntVar(NAME_cheight) = 200;
			WI_Screen->IntVar(NAME_scalemode) = FSMode_ScaleToFit43;
			WI_Screen->IntVar(NAME_scalefactorx) = 1;
			WI_Screen->IntVar(NAME_scalefactory) = 1;
			WI_Screen->IntVar(NAME_wrapwidth) = pixw;
		}
	}

	return WI_Screen;
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
	return min(numret, 3);
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
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, totalkills);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, maxkills);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, maxitems);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, maxsecret);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, maxfrags);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, partime);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, sucktime);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, totaltime);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, pnum);
DEFINE_FIELD_X(WBStartStruct, wbstartstruct_t, plyr);


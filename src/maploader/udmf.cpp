/*
** p_udmf.cpp
**
** UDMF text map parser
**
**---------------------------------------------------------------------------
** Copyright 2008 Christoph Oelckers
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

#include "doomstat.h"
#include "p_setup.h"
#include "p_lnspec.h"

#include "gi.h"
#include "r_sky.h"
#include "g_level.h"
#include "udmf.h"
#include "r_state.h"
#include "filesystem.h"
#include "p_tags.h"
#include "p_terrain.h"
#include "p_spec.h"
#include "g_levellocals.h"
#include "info.h"
#include "vm.h"
#include "xlat/xlat.h"
#include "maploader.h"
#include "texturemanager.h"
#include "a_scroll.h"
#include "p_spec_thinkers.h"

//===========================================================================
//
// Maps for Hexen namespace need to filter out all extended line and sector
// specials to comply with the spec.
//
//===========================================================================

static char HexenLineSpecialOk[]={
	1,1,1,1,1,1,1,1,1,0,	// 0-9
	1,1,1,1,0,0,0,0,0,0,	// 10-19
	1,1,1,1,1,1,1,1,1,1,	// 20-29
	1,1,1,0,0,1,1,0,0,0,	// 30-39
	1,1,1,1,1,1,1,0,0,0,	// 40-49
	0,0,0,0,0,0,0,0,0,0,	// 50-59
	1,1,1,1,1,1,1,1,1,1,	// 60-69
	1,1,1,1,1,1,0,0,0,0,	// 70-79
	1,1,1,1,0,0,0,0,0,0,	// 80-89
	1,1,1,1,1,1,1,0,0,0,	// 90-99
	1,1,1,1,0,0,0,0,0,1,	// 100-109
	1,1,1,1,1,1,1,0,0,0,	// 110-119
	1,0,0,0,0,0,0,0,0,1,	// 120-129
	1,1,1,1,1,1,1,1,1,0,	// 130-139
	1
	// 140 is the highest valid special in Hexen.

};

static char HexenSectorSpecialOk[256]={
	1,1,1,1,1,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,1,1,0,0,
	0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,
	1,1,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,1,1,
	1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,
};

#if 0
static const char* udmfsolidskewtypes[] =
{
   "none",
   "front",
   "back",
   nullptr
};

static const char* udmfmaskedskewtypes[] =
{
   "none",
   "front_floor",
   "front_ceiling",
   "back_floor",
   "back_ceiling",
   nullptr
};
#endif

static inline bool P_IsThingSpecial(int specnum)
{
	return (specnum >= Thing_Projectile && specnum <= Thing_SpawnNoFog) ||
			specnum == Thing_SpawnFacing || specnum == Thing_ProjectileIntercept || specnum == Thing_ProjectileAimed;
}
namespace
{
	enum
	{
		Dm=1,		// Doom
		Ht=2,		// Heretic
		Hx=4,		// Hexen
		St=8,		// Strife
		Zd=16,		// ZDoom
		Zdt=32,		// ZDoom Translated
		Va=64,		// Vavoom

		// will be extended later. Unknown namespaces will always be treated like the base
		// namespace for each game
	};
}
#define CHECK_N(f) if (!(namespace_bits&(f))) break;

//===========================================================================
//
// Common parsing routines
//
//===========================================================================

//===========================================================================
//
// Skip a key or block
//
//===========================================================================

void UDMFParserBase::Skip()
{
	if (developer >= DMSG_WARNING) sc.ScriptMessage("Ignoring unknown UDMF key \"%s\".", sc.String);
	if(sc.CheckToken('{'))
	{
		int level = 1;
		while(sc.GetToken())
		{
			if (sc.TokenType == '}')
			{
				level--;
				if(level == 0)
				{
					sc.UnGet();
					break;
				}
			}
			else if (sc.TokenType == '{')
			{
				level++;
			}
		}
	}
	else
	{
		sc.MustGetToken('=');
		do
		{
			sc.MustGetAnyToken();
		}
		while(sc.TokenType != ';');
	}
}

//===========================================================================
//
// Parses a 'key = value' line of the map
//
//===========================================================================

FName UDMFParserBase::ParseKey(bool checkblock, bool *isblock)
{
	sc.MustGetString();
	FName key = sc.String;
	if (checkblock)
	{
		if (sc.CheckToken('{'))
		{
			if (isblock) *isblock = true;
			return key;
		}
		else if (isblock) *isblock = false;
	}
	sc.MustGetToken('=');

	sc.Number = 0;
	sc.Float = 0;
	sc.MustGetAnyToken();

	if (sc.TokenType == '+' || sc.TokenType == '-')
	{
		bool neg = (sc.TokenType == '-');
		sc.MustGetAnyToken();
		if (sc.TokenType != TK_IntConst && sc.TokenType != TK_FloatConst)
		{
			sc.ScriptMessage("Numeric constant expected");
		}
		if (neg)
		{
			sc.Number = -sc.Number;
			sc.Float = -sc.Float;
		}
	}
	if (sc.TokenType == TK_StringConst)
	{
		parsedString = sc.String;
	}
	int savedtoken = sc.TokenType;
	sc.MustGetToken(';');
	sc.TokenType = savedtoken;
	return key;
}

//===========================================================================
//
// Syntax checks
//
//===========================================================================

int UDMFParserBase::CheckInt(FName key)
{
	if (sc.TokenType != TK_IntConst)
	{
		sc.ScriptMessage("Integer value expected for key '%s'", key.GetChars());
	}
	return sc.Number;
}

double UDMFParserBase::CheckFloat(FName key)
{
	if (sc.TokenType != TK_IntConst && sc.TokenType != TK_FloatConst)
	{
		sc.ScriptMessage("Floating point value expected for key '%s'", key.GetChars());
	}
	return sc.Float;
}

double UDMFParserBase::CheckCoordinate(FName key)
{
	if (sc.TokenType != TK_IntConst && sc.TokenType != TK_FloatConst)
	{
		sc.ScriptMessage("Floating point value expected for key '%s'", key.GetChars());
	}
	if (sc.Float < -32768 || sc.Float > 32768)
	{
		sc.ScriptMessage("Value %f out of range for a coordinate '%s'. Valid range is [-32768 .. 32768]", sc.Float, key.GetChars());
		BadCoordinates = true;	// If this happens the map must not allowed to be started.
	}
	return sc.Float;
}

DAngle UDMFParserBase::CheckAngle(FName key)
{
	return DAngle::fromDeg(CheckFloat(key)).Normalized360();
}

bool UDMFParserBase::CheckBool(FName key)
{
	if (sc.TokenType == TK_True) return true;
	if (sc.TokenType == TK_False) return false;
	sc.ScriptMessage("Boolean value expected for key '%s'", key.GetChars());
	return false;
}

const char *UDMFParserBase::CheckString(FName key)
{
	if (sc.TokenType != TK_StringConst)
	{
		sc.ScriptMessage("String value expected for key '%s'", key.GetChars());
	}
	return parsedString.GetChars();
}

int UDMFParserBase::MatchString(FName key, const char* const* strings, int defval)
{
	const char* string = CheckString(key);
	for (int i = 0; *strings != nullptr; i++, strings++)
	{
		if (!stricmp(string, *strings))
		{
			return i;
		}
	}
	sc.ScriptMessage("Unknown value %s for key '%s'", string, key.GetChars());
	return defval;
}

//===========================================================================
//
// Storage of UDMF user properties
//
//===========================================================================

static int udmfcmp(const void *a, const void *b)
{
	FUDMFKey *A = (FUDMFKey*)a;
	FUDMFKey *B = (FUDMFKey*)b;

	return int(A->Key.GetIndex()) - int(B->Key.GetIndex());
}

void FUDMFKeys::Sort()
{
	qsort(&(*this)[0], Size(), sizeof(FUDMFKey), udmfcmp);
}

FUDMFKey *FUDMFKeys::Find(FName key)
{
	if (!mSorted)
	{
		mSorted = true;
		Sort();
	}
	int min = 0, max = Size()-1;

	while (min <= max)
	{
		int mid = (min + max) / 2;
		if ((*this)[mid].Key == key)
		{
			return &(*this)[mid];
		}
		else if ((*this)[mid].Key <= key)
		{
			min = mid + 1;
		}
		else
		{
			max = mid - 1;
		}
	}
	return NULL;
}

//===========================================================================
//
// Retrieves UDMF user properties
//
//===========================================================================

int GetUDMFInt(FLevelLocals *Level, int type, int index, FName key)
{
	assert(type >=0 && type <=3);

	FUDMFKeys *pKeys = Level->UDMFKeys[type].CheckKey(index);

	if (pKeys != NULL)
	{
		FUDMFKey *pKey = pKeys->Find(key);
		if (pKey != NULL)
		{
			return pKey->IntVal;
		}
	}
	return 0;
}

double GetUDMFFloat(FLevelLocals *Level, int type, int index, FName key)
{
	assert(type >=0 && type <=3);

	FUDMFKeys *pKeys = Level->UDMFKeys[type].CheckKey(index);

	if (pKeys != NULL)
	{
		FUDMFKey *pKey = pKeys->Find(key);
		if (pKey != NULL)
		{
			return pKey->FloatVal;
		}
	}
	return 0;
}

FString GetUDMFString(FLevelLocals *Level, int type, int index, FName key)
{
	assert(type >= 0 && type <= 3);

	FUDMFKeys *pKeys = Level->UDMFKeys[type].CheckKey(index);

	if (pKeys != NULL)
	{
		FUDMFKey *pKey = pKeys->Find(key);
		if (pKey != NULL)
		{
			return pKey->StringVal;
		}
	}
	return "";
}

//===========================================================================
//
// UDMF parser
//
//===========================================================================

struct UDMFScroll
{
	int where;
	int index;
	double x, y;
	int scrolltype;
};

class UDMFParser : public UDMFParserBase
{
	bool isTranslated;
	bool isExtended;
	bool floordrop;
	MapLoader *loader;
	FLevelLocals *Level;

	TArray<line_t> ParsedLines;
	TArray<side_t> ParsedSides;
	TArray<intmapsidedef_t> ParsedSideTextures;
	TArray<sector_t> ParsedSectors;
	TArray<vertex_t> ParsedVertices;
	TArray<UDMFScroll> UDMFSectorScrollers;
	TArray<UDMFScroll> UDMFWallScrollers;
	TArray<UDMFScroll> UDMFThrusters;

	FDynamicColormap	*fogMap = nullptr, *normMap = nullptr;
	FMissingTextureTracker &missingTex;

public:
	UDMFParser(MapLoader *ld, FMissingTextureTracker &missing)
		: loader(ld), Level(ld->Level), missingTex(missing)
	{
		loader->linemap.Clear();
	}

  void ReadUserKey(FUDMFKey &ukey) {
		switch (sc.TokenType)
		{
		case TK_IntConst:
			ukey = sc.Number;
			break;
		case TK_FloatConst:
			ukey = sc.Float;
			break;
		default:
		case TK_StringConst:
			ukey = parsedString;
			break;
		case TK_True:
			ukey = 1;
			break;
		case TK_False:
			ukey = 0;
			break;
		}
  }
	void AddUserKey(FName key, int kind, int index)
	{
		FUDMFKeys &keyarray = Level->UDMFKeys[kind][index];

		for(unsigned i=0; i < keyarray.Size(); i++)
		{
			if (keyarray[i].Key == key)
			{
				ReadUserKey(keyarray[i]);
				return;
			}
		}
		FUDMFKey ukey;
		ukey.Key = key;
		ReadUserKey(ukey);
		keyarray.Push(ukey);
	}

	//===========================================================================
	//
	// Parse a thing block
	//
	//===========================================================================

	void ParseThing(FMapThing *th)
	{
		FString arg0str, arg1str;

		memset(th, 0, sizeof(*th));
		th->Gravity = 1;
		th->RenderStyle = STYLE_Count;
		th->Alpha = -1;
		th->Health = 1;
		th->FloatbobPhase = -1;
		sc.MustGetToken('{');
		while (!sc.CheckToken('}'))
		{
			FName key = ParseKey();
			switch(key.GetIndex())
			{
			case NAME_Id:
				th->thingid = CheckInt(key);
				break;

			case NAME_X:
				th->pos.X = CheckCoordinate(key);
				break;

			case NAME_Y:
				th->pos.Y = CheckCoordinate(key);
				break;

			case NAME_Height:
				th->pos.Z = CheckCoordinate(key);
				break;

			case NAME_Angle:
				th->angle = (short)CheckInt(key);
				break;

			case NAME_Type:
				th->EdNum = (short)CheckInt(key);
				th->info = DoomEdMap.CheckKey(th->EdNum);
				break;

			case NAME_Conversation:
				CHECK_N(Zd | Zdt)
				th->Conversation = CheckInt(key);
				break;

			case NAME_Special:
				CHECK_N(Hx | Zd | Zdt | Va)
				th->special = CheckInt(key);
				break;

			case NAME_Gravity:
				CHECK_N(Zd | Zdt)
				th->Gravity = CheckFloat(key);
				break;

			case NAME_Arg0:
			case NAME_Arg1:
			case NAME_Arg2:
			case NAME_Arg3:
			case NAME_Arg4:
				CHECK_N(Hx | Zd | Zdt | Va)
				th->args[key.GetIndex() - int(NAME_Arg0)] = CheckInt(key);
				break;

			case NAME_Arg0Str:
				CHECK_N(Zd);
				arg0str = CheckString(key);
				th->arg0str = arg0str;
				break;

			case NAME_Arg1Str:
				CHECK_N(Zd);
				arg1str = CheckString(key);
				break;

			case NAME_Skill1:
			case NAME_Skill2:
			case NAME_Skill3:
			case NAME_Skill4:
			case NAME_Skill5:
			case NAME_Skill6:
			case NAME_Skill7:
			case NAME_Skill8:
			case NAME_Skill9:
			case NAME_Skill10:
			case NAME_Skill11:
			case NAME_Skill12:
			case NAME_Skill13:
			case NAME_Skill14:
			case NAME_Skill15:
			case NAME_Skill16:
				if (CheckBool(key)) th->SkillFilter |= (1<<(key.GetIndex()-NAME_Skill1));
				else th->SkillFilter &= ~(1<<(key.GetIndex()-NAME_Skill1));
				break;

			case NAME_Class1:
			case NAME_Class2:
			case NAME_Class3:
			case NAME_Class4:
			case NAME_Class5:
			case NAME_Class6:
			case NAME_Class7:
			case NAME_Class8:
			case NAME_Class9:
			case NAME_Class10:
			case NAME_Class11:
			case NAME_Class12:
			case NAME_Class13:
			case NAME_Class14:
			case NAME_Class15:
			case NAME_Class16:
				CHECK_N(Hx | Zd | Zdt | Va)
				if (CheckBool(key)) th->ClassFilter |= (1<<(key.GetIndex()-NAME_Class1));
				else th->ClassFilter &= ~(1<<(key.GetIndex()-NAME_Class1));
				break;

			case NAME_Ambush:
				Flag(th->flags, MTF_AMBUSH, key); 
				break;

			case NAME_Dormant:
				CHECK_N(Hx | Zd | Zdt | Va)
				Flag(th->flags, MTF_DORMANT, key); 
				break;

			case NAME_Single:
				Flag(th->flags, MTF_SINGLE, key); 
				break;

			case NAME_Coop:
				Flag(th->flags, MTF_COOPERATIVE, key); 
				break;

			case NAME_Dm:
				Flag(th->flags, MTF_DEATHMATCH, key); 
				break;

			case NAME_Translucent:
				CHECK_N(St | Zd | Zdt | Va)
				Flag(th->flags, MTF_SHADOW, key); 
				break;

			case NAME_Invisible:
				CHECK_N(St | Zd | Zdt | Va)
				Flag(th->flags, MTF_ALTSHADOW, key); 
				break;

			case NAME_Friend:	// This maps to Strife's friendly flag
				CHECK_N(Dm | Zd | Zdt | Va)
				Flag(th->flags, MTF_FRIENDLY, key); 
				break;

			case NAME_Strifeally:
				CHECK_N(St | Zd | Zdt | Va)
				Flag(th->flags, MTF_FRIENDLY, key); 
				break;

			case NAME_Standing:
				CHECK_N(St | Zd | Zdt | Va)
				Flag(th->flags, MTF_STANDSTILL, key); 
				break;

			case NAME_Countsecret:
				CHECK_N(Zd | Zdt | Va)
				Flag(th->flags, MTF_SECRET, key); 
				break;

			case NAME_NoCount:
				CHECK_N(Zd | Zdt)
				Flag(th->flags, MTF_NOCOUNT, key);
				break;

			case NAME_Floatbobphase:
				CHECK_N(Zd | Zdt)
				th->FloatbobPhase = CheckInt(key);
				break;

			case NAME_Renderstyle:
				{
				FName style = CheckString(key);
				switch (style.GetIndex())
				{
				case NAME_None:
					th->RenderStyle = STYLE_None;
					break;
				case NAME_Normal:
					th->RenderStyle = STYLE_Normal;
					break;
				case NAME_Fuzzy:
					th->RenderStyle = STYLE_Fuzzy;
					break;
				case NAME_SoulTrans:
					th->RenderStyle = STYLE_SoulTrans;
					break;
				case NAME_OptFuzzy:
					th->RenderStyle = STYLE_OptFuzzy;
					break;
				case NAME_Stencil:
					th->RenderStyle = STYLE_Stencil;
					break;
				case NAME_AddStencil:
					th->RenderStyle = STYLE_AddStencil;
					break;
				case NAME_Translucent:
					th->RenderStyle = STYLE_Translucent;
					break;
				case NAME_Add:
				case NAME_Additive:
					th->RenderStyle = STYLE_Add;
					break;
				case NAME_Shaded:
					th->RenderStyle = STYLE_Shaded;
					break;
				case NAME_AddShaded:
					th->RenderStyle = STYLE_AddShaded;
					break;
				case NAME_TranslucentStencil:
					th->RenderStyle = STYLE_TranslucentStencil;
					break;
				case NAME_Shadow:
					th->RenderStyle = STYLE_Shadow;
					break;
				case NAME_Subtract:
				case NAME_Subtractive:
					th->RenderStyle = STYLE_Subtract;
					break;
				case NAME_ColorBlend:
					th->RenderStyle = STYLE_ColorBlend;
					break;
				case NAME_ColorAdd:
					th->RenderStyle = STYLE_ColorAdd;
					break;
				case NAME_Multiply:
					th->RenderStyle = STYLE_Multiply;
					break;
				default:
					break;
				}
				}
				break;

			case NAME_Alpha:
				th->Alpha = CheckFloat(key);
				break;

			case NAME_FillColor:
				th->fillcolor = CheckInt(key);
				break;

			case NAME_Health:
				th->Health = CheckFloat(key);
				break;

			case NAME_Score:
				th->score = CheckInt(key);
				break;

			case NAME_Pitch:
				th->pitch = (short)CheckInt(key);
				break;

			case NAME_Roll:
				th->roll = (short)CheckInt(key);
				break;

			case NAME_ScaleX:
				th->Scale.X = (float)CheckFloat(key);
				break;

			case NAME_ScaleY:
				th->Scale.Y = (float)CheckFloat(key);
				break;

			case NAME_Scale:
				th->Scale.X = th->Scale.Y = (float)CheckFloat(key);
				break;

			case NAME_FriendlySeeBlocks:
				CHECK_N(Zd | Zdt)
				th->friendlyseeblocks = CheckInt(key);
				break;

			case NAME_light_softshadowradius:
			case NAME_lm_suncolor:
			case NAME_lm_sampledist:
				CHECK_N(Zd | Zdt)
					break;

			default:
				CHECK_N(Zd | Zdt)
				if (0 == strnicmp("user_", key.GetChars(), 5))
				{ // Custom user key - Sets an actor's user variable directly
					FUDMFKey ukey;
					ukey.Key = key;
					ReadUserKey(ukey);
					loader->MapThingsUserData.Push(ukey);
				}
				else if (stricmp("comment", key.GetChars()))
				{
					DPrintf(DMSG_WARNING, "Unknown UDMF thing key %s\n", key.GetChars());
				}
				break;
			}
		}
		if (arg0str.IsNotEmpty() && (P_IsACSSpecial(th->special) || th->special == 0))
		{
			th->args[0] = -FName(arg0str).GetIndex();
		}
		if (arg1str.IsNotEmpty() && (P_IsThingSpecial(th->special) || th->special == 0))
		{
			th->args[1] = -FName(arg1str).GetIndex();
		}
		// Thing specials are only valid in namespaces with Hexen-type specials
		// and in ZDoomTranslated - which will use the translator on them.
		if (namespc == NAME_ZDoomTranslated)
		{
			maplinedef_t mld;
			line_t ld;

			if (th->special != 0)	// if special is 0, keep the args (e.g. for bridge things)
			{
				// The trigger type is ignored here.
				mld.flags = 0;
				mld.special = th->special;
				mld.tag = th->args[0];
				Level->TranslateLineDef(&ld, &mld);
				th->special = ld.special;
				memcpy(th->args, ld.args, sizeof (ld.args));
			}
		}
		else if (isTranslated)
		{
			th->special = 0;
			memset(th->args, 0, sizeof (th->args));
		}
	}

	//===========================================================================
	//
	// Parse a linedef block
	//
	//===========================================================================

	void ParseLinedef(line_t *ld, int index)
	{
		bool passuse = false;
		bool strifetrans = false;
		bool strifetrans2 = false;
		FString arg0str, arg1str;
		int lineid = -1;	// forZDoomTranslated namespace
		FString tagstring;

		memset(ld, 0, sizeof(*ld));
		ld->alpha = 1.;
		ld->portalindex = UINT_MAX;
		ld->portaltransferred = UINT_MAX;
		ld->sidedef[0] = ld->sidedef[1] = NULL;
		if (Level->flags2 & LEVEL2_CLIPMIDTEX) ld->flags |= ML_CLIP_MIDTEX;
		if (Level->flags2 & LEVEL2_WRAPMIDTEX) ld->flags |= ML_WRAP_MIDTEX;
		if (Level->flags2 & LEVEL2_CHECKSWITCHRANGE) ld->flags |= ML_CHECKSWITCHRANGE;

		sc.MustGetToken('{');
		while (!sc.CheckToken('}'))
		{
			FName key = ParseKey();

			// This switch contains all keys of the UDMF base spec
			switch(key.GetIndex())
			{
			case NAME_V1:
				ld->v1 = (vertex_t*)(intptr_t)CheckInt(key);	// must be relocated later
				continue;

			case NAME_V2:
				ld->v2 = (vertex_t*)(intptr_t)CheckInt(key);	// must be relocated later
				continue;

			case NAME_Special:
				ld->special = CheckInt(key);
				if (namespc == NAME_Hexen)
				{
					if (ld->special < 0 || ld->special > 140 || !HexenLineSpecialOk[ld->special])
						ld->special = 0;	// NULL all specials which don't exist in Hexen
				}

				continue;

			case NAME_Id:
				lineid = CheckInt(key);
				Level->tagManager.AddLineID(index, lineid);
				continue;

			case NAME_Sidefront:
				ld->sidedef[0] = (side_t*)(intptr_t)(1 + CheckInt(key));
				continue;

			case NAME_Sideback:
				ld->sidedef[1] = (side_t*)(intptr_t)(1 + CheckInt(key));
				continue;

			case NAME_Arg0:
			case NAME_Arg1:
			case NAME_Arg2:
			case NAME_Arg3:
			case NAME_Arg4:
				ld->args[key.GetIndex()-int(NAME_Arg0)] = CheckInt(key);
				continue;

			case NAME_Arg0Str:
				CHECK_N(Zd);
				arg0str = CheckString(key);
				continue;

			case NAME_Arg1Str:
				CHECK_N(Zd);
				arg1str = CheckString(key);
				continue;

			case NAME_Blocking:
				Flag(ld->flags, ML_BLOCKING, key); 
				continue;

			case NAME_Blockmonsters:
				Flag(ld->flags, ML_BLOCKMONSTERS, key); 
				continue;

			case NAME_Twosided:
				Flag(ld->flags, ML_TWOSIDED, key); 
				continue;

			case NAME_Dontpegtop:
				Flag(ld->flags, ML_DONTPEGTOP, key); 
				continue;

			case NAME_Dontpegbottom:
				Flag(ld->flags, ML_DONTPEGBOTTOM, key); 
				continue;

			case NAME_Secret:
				Flag(ld->flags, ML_SECRET, key); 
				continue;

			case NAME_Blocksound:
				Flag(ld->flags, ML_SOUNDBLOCK, key); 
				continue;

			case NAME_Dontdraw:
				Flag(ld->flags, ML_DONTDRAW, key); 
				continue;

			case NAME_Mapped:
				Flag(ld->flags, ML_MAPPED, key); 
				continue;

			case NAME_Jumpover:
				CHECK_N(St | Zd | Zdt | Va)
				Flag(ld->flags, ML_RAILING, key); 
				continue;

			case NAME_Blockfloaters:
				CHECK_N(St | Zd | Zdt | Va)
				Flag(ld->flags, ML_BLOCK_FLOATERS, key); 
				continue;

			case NAME_Blocklandmonsters:
				// This is from MBF21 so it may later be needed for a lower level namespace.
				CHECK_N(St | Zd | Zdt | Va)
				Flag(ld->flags2, ML2_BLOCKLANDMONSTERS, key);
				continue;

			case NAME_Translucent:
				CHECK_N(St | Zd | Zdt | Va)
				strifetrans = CheckBool(key); 
				continue;

			case NAME_Transparent:
				CHECK_N(St | Zd | Zdt | Va)
				strifetrans2 = CheckBool(key); 
				continue;

			case NAME_Passuse:
				CHECK_N(Dm | Zd | Zdt | Va)
				passuse = CheckBool(key); 
				continue;

			default:
				if (!stricmp("comment", key.GetChars()))
					continue;
				break;
			}

			// This switch contains all keys of the UDMF base spec which only apply to Hexen format specials
			if (!isTranslated) switch (key.GetIndex())
			{
			case NAME_Walking:
				Flag(ld->activation, SPAC_Walking, key);
				continue;

			case NAME_Playercross:
				Flag(ld->activation, SPAC_Cross, key); 
				continue;

			case NAME_Playeruse:
				Flag(ld->activation, SPAC_Use, key); 
				continue;

			case NAME_Playeruseback:
				Flag(ld->activation, SPAC_UseBack, key); 
				continue;

			case NAME_Monstercross:
				Flag(ld->activation, SPAC_MCross, key); 
				continue;

			case NAME_Impact:
				Flag(ld->activation, SPAC_Impact, key); 
				continue;

			case NAME_Playerpush:
				Flag(ld->activation, SPAC_Push, key); 
				continue;

			case NAME_Missilecross:
				Flag(ld->activation, SPAC_PCross, key); 
				continue;

			case NAME_Monsteruse:
				Flag(ld->activation, SPAC_MUse, key); 
				continue;

			case NAME_Monsterpush:
				Flag(ld->activation, SPAC_MPush, key); 
				continue;

			case NAME_Repeatspecial:
				Flag(ld->flags, ML_REPEAT_SPECIAL, key); 
				continue;

			default:
				break;
			}

			// This switch contains all keys which are ZDoom specific
			if (namespace_bits & (Zd|Zdt|Va)) switch(key.GetIndex())
			{
			case NAME_Alpha:
				ld->setAlpha(CheckFloat(key));
				continue;

			case NAME_Renderstyle:
			{
				const char *str = CheckString(key);
				if (!stricmp(str, "translucent")) ld->flags &= ~ML_ADDTRANS;
				else if (!stricmp(str, "add")) ld->flags |= ML_ADDTRANS;
				else sc.ScriptMessage("Unknown value \"%s\" for 'renderstyle'\n", str);
				continue;
			}

			case NAME_Anycross:
				Flag(ld->activation, SPAC_AnyCross, key); 
				continue;

			case NAME_Monsteractivate:
				Flag(ld->flags, ML_MONSTERSCANACTIVATE, key); 
				continue;

			case NAME_Blockplayers:
				Flag(ld->flags, ML_BLOCK_PLAYERS, key); 
				continue;

			case NAME_Blockeverything:
				Flag(ld->flags, ML_BLOCKEVERYTHING, key); 
				continue;

			case NAME_Zoneboundary:
				Flag(ld->flags, ML_ZONEBOUNDARY, key); 
				continue;

			case NAME_Clipmidtex:
				Flag(ld->flags, ML_CLIP_MIDTEX, key); 
				continue;

			case NAME_Wrapmidtex:
				Flag(ld->flags, ML_WRAP_MIDTEX, key); 
				continue;

			case NAME_Midtex3d:
				Flag(ld->flags, ML_3DMIDTEX, key); 
				continue;

			case NAME_Checkswitchrange:
				Flag(ld->flags, ML_CHECKSWITCHRANGE, key); 
				continue;

			case NAME_Firstsideonly:
				Flag(ld->flags, ML_FIRSTSIDEONLY, key); 
				continue;

			case NAME_blockprojectiles:
				Flag(ld->flags, ML_BLOCKPROJECTILE, key); 
				continue;

			case NAME_blockuse:
				Flag(ld->flags, ML_BLOCKUSE, key); 
				continue;

			case NAME_blocksight:
				Flag(ld->flags, ML_BLOCKSIGHT, key); 
				continue;
			
			case NAME_blockhitscan:
				Flag(ld->flags, ML_BLOCKHITSCAN, key); 
				continue;
			
			// [TP] Locks the special with a key
			case NAME_Locknumber:
				ld->locknumber = CheckInt(key);
				continue;

			// [TP] Causes a 3d midtex to behave like an impassible line
			case NAME_Midtex3dimpassible:
				Flag(ld->flags, ML_3DMIDTEX_IMPASS, key);
				continue;

			case NAME_Revealed:
				Flag(ld->flags, ML_REVEALED, key);
				continue;

			case NAME_AutomapStyle:
				ld->automapstyle = AutomapLineStyle(CheckInt(key));
				continue;

			case NAME_NoSkyWalls:
				Flag(ld->flags, ML_NOSKYWALLS, key);
				continue;

			case NAME_DrawFullHeight:
				Flag(ld->flags, ML_DRAWFULLHEIGHT, key);
				continue;

			case NAME_MoreIds:
				// delay parsing of the tag string until parsing of the sector is complete
				// This ensures that the ID is always the first tag in the list.
				tagstring = CheckString(key);
				break;

			case NAME_Health:
				ld->health = CheckInt(key);
				break;

			case NAME_DamageSpecial:
				Flag(ld->activation, SPAC_Damage, key);
				break;

			case NAME_DeathSpecial:
				Flag(ld->activation, SPAC_Death, key);
				break;

			case NAME_HealthGroup:
				ld->healthgroup = CheckInt(key);
				break;

			case NAME_lm_sampledist:
			case NAME_lm_sampledist_top:
			case NAME_lm_sampledist_mid:
			case NAME_lm_sampledist_bot:
				CHECK_N(Zd | Zdt)
				break;

			default:
				if (strnicmp("user_", key.GetChars(), 5))
					DPrintf(DMSG_WARNING, "Unknown UDMF linedef key %s\n", key.GetChars());
				break;
			}


			if ((namespace_bits & (Zd | Zdt)) && !strnicmp("user_", key.GetChars(), 5))
			{
				AddUserKey(key, UDMF_Line, index);
			}
		}

		if (tagstring.IsNotEmpty())
		{
			FScanner sc;
			sc.OpenString("tagstring", tagstring);
			// scan the string as long as valid numbers can be found
			while (sc.CheckNumber())
			{
				if (sc.Number != 0)	Level->tagManager.AddLineID(index, sc.Number);
			}
		}

		if (isTranslated)
		{
			int saved = ld->flags;

			maplinedef_t mld;
			memset(&mld, 0, sizeof(mld));
			mld.special = ld->special;
			mld.tag = ld->args[0];
			Level->TranslateLineDef(ld, &mld);
			ld->flags = saved | (ld->flags&(ML_MONSTERSCANACTIVATE|ML_REPEAT_SPECIAL|ML_FIRSTSIDEONLY));
		}
		if (passuse && (ld->activation & SPAC_Use)) 
		{
			ld->activation = (ld->activation & ~SPAC_Use) | SPAC_UseThrough;
		}
		if (strifetrans && ld->alpha == 1.)
		{
			ld->alpha = 0.75;
		}
		if (strifetrans2 && ld->alpha == 1.)
		{
			ld->alpha = 0.25;
		}
		if (ld->sidedef[0] == NULL)
		{
			ld->sidedef[0] = (side_t*)(intptr_t)(1);
			Printf("Line %d has no first side.\n", index);
		}
		if (arg0str.IsNotEmpty() && (P_IsACSSpecial(ld->special) || ld->special == 0))
		{
			ld->args[0] = -FName(arg0str).GetIndex();
		}
		if (arg1str.IsNotEmpty() && (P_IsThingSpecial(ld->special) || ld->special == 0))
		{
			ld->args[1] = -FName(arg1str).GetIndex();
		}
		if ((ld->flags & ML_3DMIDTEX_IMPASS) && !(ld->flags & ML_3DMIDTEX)) // [TP]
		{
			Printf ("Line %d has midtex3dimpassible without midtex3d.\n", index);
		}
	}

	//===========================================================================
	//
	// Parse a sidedef block
	//
	//===========================================================================

	void ParseSidedef(side_t *sd, intmapsidedef_t *sdt, int index)
	{
		double texOfs[2]={0,0};
		DVector2 scrolls[4] = {};

		memset(sd, 0, sizeof(*sd));
		sdt->bottomtexture = "-";
		sdt->toptexture = "-";
		sdt->midtexture = "-";
		sd->SetTextureXScale(1.);
		sd->SetTextureYScale(1.);
		sd->UDMFIndex = index;

		sc.MustGetToken('{');
		while (!sc.CheckToken('}'))
		{
			FName key = ParseKey();
			switch(key.GetIndex())

			{
			case NAME_Offsetx:
				texOfs[0] = CheckInt(key);
				continue;

			case NAME_Offsety:
				texOfs[1] = CheckInt(key);
				continue;

			case NAME_Texturetop:
				sdt->toptexture = CheckString(key);
				continue;

			case NAME_Texturebottom:
				sdt->bottomtexture = CheckString(key);
				continue;

			case NAME_Texturemiddle:
				sdt->midtexture = CheckString(key);
				continue;

			case NAME_Sector:
				sd->sector = (sector_t*)(intptr_t)CheckInt(key);
				continue;

			default:
				if (!stricmp("comment", key.GetChars()))
					continue;
				break;
			}

			if (namespace_bits & (Zd|Zdt|Va)) switch(key.GetIndex())
			{
			case NAME_offsetx_top:
				sd->SetTextureXOffset(side_t::top, CheckFloat(key));
				continue;

			case NAME_offsety_top:
				sd->SetTextureYOffset(side_t::top, CheckFloat(key));
				continue;

			case NAME_offsetx_mid:
				sd->SetTextureXOffset(side_t::mid, CheckFloat(key));
				continue;

			case NAME_offsety_mid:
				sd->SetTextureYOffset(side_t::mid, CheckFloat(key));
				continue;

			case NAME_offsetx_bottom:
				sd->SetTextureXOffset(side_t::bottom, CheckFloat(key));
				continue;

			case NAME_offsety_bottom:
				sd->SetTextureYOffset(side_t::bottom, CheckFloat(key));
				continue;

			case NAME_scalex_top:
				sd->SetTextureXScale(side_t::top, CheckFloat(key));
				continue;

			case NAME_scaley_top:
				sd->SetTextureYScale(side_t::top, CheckFloat(key));
				continue;

			case NAME_scalex_mid:
				sd->SetTextureXScale(side_t::mid, CheckFloat(key));
				continue;

			case NAME_scaley_mid:
				sd->SetTextureYScale(side_t::mid, CheckFloat(key));
				continue;

			case NAME_scalex_bottom:
				sd->SetTextureXScale(side_t::bottom, CheckFloat(key));
				continue;

			case NAME_scaley_bottom:
				sd->SetTextureYScale(side_t::bottom, CheckFloat(key));
				continue;

			case NAME_light:
				sd->SetLight(CheckInt(key));
				continue;

			case NAME_lightabsolute:
				Flag(sd->Flags, WALLF_ABSLIGHTING, key);
				continue;

			case NAME_light_top:
				sd->SetLight(CheckInt(key), side_t::top);
				continue;
				
			case NAME_lightabsolute_top:
				Flag(sd->Flags, WALLF_ABSLIGHTING_TOP, key);
				continue;

			case NAME_light_mid:
				sd->SetLight(CheckInt(key), side_t::mid);
				continue;

			case NAME_lightabsolute_mid:
				Flag(sd->Flags, WALLF_ABSLIGHTING_MID, key);
				continue;

			case NAME_light_bottom:
				sd->SetLight(CheckInt(key), side_t::bottom);
				continue;

			case NAME_lightabsolute_bottom:
				Flag(sd->Flags, WALLF_ABSLIGHTING_BOTTOM, key);
				continue;

			case NAME_lightfog:
				Flag(sd->Flags, WALLF_LIGHT_FOG, key);
				continue;

			case NAME_nofakecontrast:
				Flag(sd->Flags, WALLF_NOFAKECONTRAST, key);
				continue;

			case NAME_smoothlighting:
				Flag(sd->Flags, WALLF_SMOOTHLIGHTING, key);
				continue;

			case NAME_Wrapmidtex:
				Flag(sd->Flags, WALLF_WRAP_MIDTEX, key);
				continue;

			case NAME_Clipmidtex:
				Flag(sd->Flags, WALLF_CLIP_MIDTEX, key);
				continue;

			case NAME_Nodecals:
				Flag(sd->Flags, WALLF_NOAUTODECALS, key);
				continue;

			case NAME_colorization_top:
				sd->SetTextureFx(side_t::top, TexMan.GetTextureManipulation(CheckString(key)));
				break;

			case NAME_colorization_mid:
				sd->SetTextureFx(side_t::mid, TexMan.GetTextureManipulation(CheckString(key)));
				break;

			case NAME_colorization_bottom:
				sd->SetTextureFx(side_t::bottom, TexMan.GetTextureManipulation(CheckString(key)));
				break;

			case NAME_nogradient_top:
				Flag(sd->textures[side_t::top].flags, side_t::part::NoGradient, key);
				break;

			case NAME_flipgradient_top:
				Flag(sd->textures[side_t::top].flags, side_t::part::FlipGradient, key);
				break;

			case NAME_clampgradient_top:
				Flag(sd->textures[side_t::top].flags, side_t::part::ClampGradient, key);
				break;

			case NAME_useowncolors_top:
				if (Flag(sd->textures[side_t::top].flags, side_t::part::UseOwnSpecialColors, key))
					sd->Flags |= WALLF_EXTCOLOR;
				break;

			case NAME_uppercolor_top:
				sd->SetSpecialColor(side_t::top, 0, CheckInt(key));
				break;

			case NAME_lowercolor_top:
				sd->SetSpecialColor(side_t::top, 1, CheckInt(key));
				break;

			case NAME_nogradient_mid:
				Flag(sd->textures[side_t::mid].flags, side_t::part::NoGradient, key);
				break;

			case NAME_flipgradient_mid:
				Flag(sd->textures[side_t::mid].flags, side_t::part::FlipGradient, key);
				break;

			case NAME_clampgradient_mid:
				Flag(sd->textures[side_t::mid].flags, side_t::part::ClampGradient, key);
				break;

			case NAME_useowncolors_mid:
				if (Flag(sd->textures[side_t::mid].flags, side_t::part::UseOwnSpecialColors, key))
					sd->Flags |= WALLF_EXTCOLOR;

				break;

			case NAME_uppercolor_mid:
				sd->SetSpecialColor(side_t::mid, 0, CheckInt(key));
				break;

			case NAME_lowercolor_mid:
				sd->SetSpecialColor(side_t::mid, 1, CheckInt(key));
				break;

			case NAME_nogradient_bottom:
				Flag(sd->textures[side_t::bottom].flags, side_t::part::NoGradient, key);
				break;

			case NAME_flipgradient_bottom:
				Flag(sd->textures[side_t::bottom].flags, side_t::part::FlipGradient, key);
				break;

			case NAME_clampgradient_bottom:
				Flag(sd->textures[side_t::bottom].flags, side_t::part::ClampGradient, key);
				break;

			case NAME_useowncolors_bottom:
				if (Flag(sd->textures[side_t::bottom].flags, side_t::part::UseOwnSpecialColors, key))
					sd->Flags |= WALLF_EXTCOLOR;
				break;

			case NAME_uppercolor_bottom:
				sd->SetSpecialColor(side_t::bottom, 0, CheckInt(key));
				break;

			case NAME_lowercolor_bottom:
				sd->SetSpecialColor(side_t::bottom, 1, CheckInt(key));
				break;

			case NAME_coloradd_top:
				sd->SetAdditiveColor(side_t::top, CheckInt(key));
				break;

			case NAME_coloradd_mid:
				sd->SetAdditiveColor(side_t::mid, CheckInt(key));
				break;

			case NAME_coloradd_bottom:
				sd->SetAdditiveColor(side_t::bottom, CheckInt(key));
				break;

			case NAME_useowncoloradd_top:
				if (Flag(sd->textures[side_t::top].flags, side_t::part::UseOwnAdditiveColor, key))
					sd->Flags |= WALLF_EXTCOLOR;
				break;

			case NAME_useowncoloradd_mid:
				if (Flag(sd->textures[side_t::mid].flags, side_t::part::UseOwnAdditiveColor, key))
					sd->Flags |= WALLF_EXTCOLOR;
				break;

			case NAME_useowncoloradd_bottom:
				if (Flag(sd->textures[side_t::bottom].flags, side_t::part::UseOwnAdditiveColor, key))
					sd->Flags |= WALLF_EXTCOLOR;
				break;

			case NAME_lm_sampledist:
			case NAME_lm_sampledist_top:
			case NAME_lm_sampledist_mid:
			case NAME_lm_sampledist_bot:
				CHECK_N(Zd | Zdt)
					break;

#if 0		// specs are to rough and too vague - needs to be cleared first how this works.
			case NAME_skew_top_type:
				CHECK_N(Zd | Zdt)
				sd->textures[side_t::top].skew = MatchString(key, udmfsolidskewtypes, 0);
				break;

			case NAME_skew_middle_type:
				CHECK_N(Zd | Zdt)
					sd->textures[side_t::mid].skew = MatchString(key, udmfmaskedskewtypes, 0);
				break;

			case NAME_skew_bottom_type:
				CHECK_N(Zd | Zdt)
					sd->textures[side_t::bottom].skew = MatchString(key, udmfsolidskewtypes, 0);
				break;
#endif

			case NAME_skew_top:
				CHECK_N(Zd | Zdt)
					sd->textures[side_t::top].skew = CheckInt(key);
				break;

			case NAME_skew_middle:
				CHECK_N(Zd | Zdt)
					sd->textures[side_t::mid].skew = CheckInt(key);
				break;

			case NAME_skew_bottom:
				CHECK_N(Zd | Zdt)
					sd->textures[side_t::bottom].skew = CheckInt(key);
				break;

			case NAME_xscroll:
				scrolls[0].X = CheckFloat(key);
				break;
			case NAME_yscroll:
				scrolls[0].Y = CheckFloat(key);
				break;
			case NAME_xscrolltop:
				scrolls[1].X = CheckFloat(key);
				break;
			case NAME_yscrolltop:
				scrolls[1].Y = CheckFloat(key);
				break;
			case NAME_xscrollmid:
				scrolls[2].X = CheckFloat(key);
				break;
			case NAME_yscrollmid:
				scrolls[2].Y = CheckFloat(key);
				break;
			case NAME_xscrollbottom:
				scrolls[3].X = CheckFloat(key);
				break;
			case NAME_yscrollbottom:
				scrolls[3].Y = CheckFloat(key);
				break;
			default:
				if (strnicmp("user_", key.GetChars(), 5))
					DPrintf(DMSG_WARNING, "Unknown UDMF sidedef key %s\n", key.GetChars());
				break;

			}
			if ((namespace_bits & (Zd | Zdt)) && !strnicmp("user_", key.GetChars(), 5))
			{
				AddUserKey(key, UDMF_Side, index);
			}
		}
		// initialization of these is delayed to allow separate offsets and add them with the global ones.
		sd->AddTextureXOffset(side_t::top, texOfs[0]);
		sd->AddTextureXOffset(side_t::mid, texOfs[0]);
		sd->AddTextureXOffset(side_t::bottom, texOfs[0]);
		sd->AddTextureYOffset(side_t::top, texOfs[1]);
		sd->AddTextureYOffset(side_t::mid, texOfs[1]);
		sd->AddTextureYOffset(side_t::bottom, texOfs[1]);
		int scroll = scw_all;
		for (int i = 1; i < 4; i++)
		{
			auto& scrl = scrolls[i];
			if (!scrl.isZero())
			{
				int where = 1 << (i - 1);
				scroll &= ~where;
				UDMFWallScrollers.Push({ where, index, scrl.X, scrl.Y, 0 });
			}
		}
		if (!scrolls[0].isZero() && scroll)
		{
			UDMFWallScrollers.Push({ scroll, index, scrolls[0].X, scrolls[0].Y, 0});
		}
	}

	//===========================================================================
	//
	// parse a sector block
	//
	//===========================================================================

	void ParseSector(sector_t *sec, int index)
	{
		PalEntry lightcolor = -1;
		PalEntry fadecolor = -1;
		int fogdensity = -1;
		int desaturation = -1;
		int fplaneflags = 0, cplaneflags = 0;
		double fp[4] = { 0 }, cp[4] = { 0 };
		FString tagstring;

		// Brand new UDMF scroller properties
		double scroll_ceil_x = 0;
		double scroll_ceil_y = 0;
		int scroll_ceil_type = 0;

		double scroll_floor_x = 0;
		double scroll_floor_y = 0;
		int scroll_floor_type = 0;

		double friction = -FLT_MAX, movefactor = -FLT_MAX;

		DVector2 thrust = { 0,0 };
		int thrustgroup = 0;
		int thrustlocation = 0;

		const double scrollfactor = 1 / 3.2;	// I hope this is correct, it's just a guess taken from Eternity's code.

		memset(sec, 0, sizeof(*sec));
		sec->Level = Level;
		sec->lightlevel = 160;
		sec->SetXScale(sector_t::floor, 1.);	// [RH] floor and ceiling scaling
		sec->SetYScale(sector_t::floor, 1.);
		sec->SetXScale(sector_t::ceiling, 1.);
		sec->SetYScale(sector_t::ceiling, 1.);
		sec->SetAlpha(sector_t::floor, 1.);
		sec->SetAlpha(sector_t::ceiling, 1.);
		sec->thinglist = nullptr;
		sec->touching_thinglist = nullptr;		// phares 3/14/98
		sec->sectorportal_thinglist = nullptr;
		sec->touching_renderthings = nullptr;
		sec->seqType = (Level->flags & LEVEL_SNDSEQTOTALCTRL) ? 0 : -1;
		sec->nextsec = -1;	//jff 2/26/98 add fields to support locking out
		sec->prevsec = -1;	// stair retriggering until build completes
		sec->heightsec = NULL;	// sector used to get floor and ceiling height
		sec->sectornum = index;
		sec->damageinterval = 32;
		sec->terrainnum[sector_t::ceiling] = sec->terrainnum[sector_t::floor] = -1;
		sec->ibocount = -1;
		memset(sec->SpecialColors, -1, sizeof(sec->SpecialColors));
		memset(sec->AdditiveColors, 0, sizeof(sec->AdditiveColors));
		if (floordrop) sec->Flags = SECF_FLOORDROP;
		// killough 3/7/98: end changes

		sec->gravity = 1.;	// [RH] Default sector gravity of 1.0
		sec->ZoneNumber = 0xFFFF;

		// killough 8/28/98: initialize all sectors to normal friction
		sec->friction = ORIG_FRICTION;
		sec->movefactor = ORIG_FRICTION_FACTOR;

		sc.MustGetToken('{');
		while (!sc.CheckToken('}'))
		{
			FName key = ParseKey();
			switch(key.GetIndex())
			{
			case NAME_Heightfloor:
				sec->SetPlaneTexZ(sector_t::floor, CheckCoordinate(key));
				continue;

			case NAME_Heightceiling:
				sec->SetPlaneTexZ(sector_t::ceiling, CheckCoordinate(key));
				continue;

			case NAME_Texturefloor:
				loader->SetTexture(sec, index, sector_t::floor, CheckString(key), missingTex, false);
				continue;

			case NAME_Textureceiling:
				loader->SetTexture(sec, index, sector_t::ceiling, CheckString(key), missingTex, false);
				continue;

			case NAME_Lightlevel:
				sec->lightlevel = sector_t::ClampLight(CheckInt(key));
				continue;

			case NAME_Special:
				sec->special = (short)CheckInt(key);
				if (isTranslated) sec->special = Level->TranslateSectorSpecial(sec->special);
				else if (namespc == NAME_Hexen)
				{
					if (sec->special < 0 || sec->special > 140 || !HexenSectorSpecialOk[sec->special])
						sec->special = 0;	// NULL all unknown specials
				}
				continue;

			case NAME_Id:
				Level->tagManager.AddSectorTag(index, CheckInt(key));
				continue;

			default:
				if (!stricmp("comment", key.GetChars()))
					continue;
				break;
			}

			if (namespace_bits & (Zd|Zdt|Va)) switch(key.GetIndex())
			{
				case NAME_Xpanningfloor:
					sec->SetXOffset(sector_t::floor, CheckFloat(key));
					continue;

				case NAME_Ypanningfloor:
					sec->SetYOffset(sector_t::floor, CheckFloat(key));
					continue;

				case NAME_Xpanningceiling:
					sec->SetXOffset(sector_t::ceiling, CheckFloat(key));
					continue;

				case NAME_Ypanningceiling:
					sec->SetYOffset(sector_t::ceiling, CheckFloat(key));
					continue;

				case NAME_Xscalefloor:
					sec->SetXScale(sector_t::floor, CheckFloat(key));
					continue;

				case NAME_Yscalefloor:
					sec->SetYScale(sector_t::floor, CheckFloat(key));
					continue;

				case NAME_Xscaleceiling:
					sec->SetXScale(sector_t::ceiling, CheckFloat(key));
					continue;

				case NAME_Yscaleceiling:
					sec->SetYScale(sector_t::ceiling, CheckFloat(key));
					continue;

				case NAME_Rotationfloor:
					sec->SetAngle(sector_t::floor, CheckAngle(key));
					continue;

				case NAME_Rotationceiling:
					sec->SetAngle(sector_t::ceiling, CheckAngle(key));
					continue;

				case NAME_Lightfloor:
					sec->SetPlaneLight(sector_t::floor, CheckInt(key));
					continue;

				case NAME_Lightceiling:
					sec->SetPlaneLight(sector_t::ceiling, CheckInt(key));
					continue;

				case NAME_Alphafloor:
					sec->SetAlpha(sector_t::floor, CheckFloat(key));
					continue;

				case NAME_Alphaceiling:
					sec->SetAlpha(sector_t::ceiling, CheckFloat(key));
					continue;

				case NAME_Renderstylefloor:
				{
					const char *str = CheckString(key);
					if (!stricmp(str, "translucent")) sec->ChangeFlags(sector_t::floor, PLANEF_ADDITIVE, 0);
					else if (!stricmp(str, "add")) sec->ChangeFlags(sector_t::floor, 0, PLANEF_ADDITIVE);
					else sc.ScriptMessage("Unknown value \"%s\" for 'renderstylefloor'\n", str);
					continue;
				}

				case NAME_Renderstyleceiling:
				{
					const char *str = CheckString(key);
					if (!stricmp(str, "translucent")) sec->ChangeFlags(sector_t::ceiling, PLANEF_ADDITIVE, 0);
					else if (!stricmp(str, "add")) sec->ChangeFlags(sector_t::ceiling, 0, PLANEF_ADDITIVE);
					else sc.ScriptMessage("Unknown value \"%s\" for 'renderstyleceiling'\n", str);
					continue;
				}

				case NAME_Lightfloorabsolute:
					if (CheckBool(key)) sec->ChangeFlags(sector_t::floor, 0, PLANEF_ABSLIGHTING);
					else sec->ChangeFlags(sector_t::floor, PLANEF_ABSLIGHTING, 0);
					continue;

				case NAME_Lightceilingabsolute:
					if (CheckBool(key)) sec->ChangeFlags(sector_t::ceiling, 0, PLANEF_ABSLIGHTING);
					else sec->ChangeFlags(sector_t::ceiling, PLANEF_ABSLIGHTING, 0);
					continue;

				case NAME_Gravity:
					sec->gravity = CheckFloat(key);
					continue;

				case NAME_Lightcolor:
					lightcolor = CheckInt(key);
					continue;

				case NAME_Fadecolor:
					fadecolor = CheckInt(key);
					continue;

				case NAME_Color_Floor:
					sec->SpecialColors[sector_t::floor] = CheckInt(key) | 0xff000000;
					break;

				case NAME_Color_Ceiling:
					sec->SpecialColors[sector_t::ceiling] = CheckInt(key) | 0xff000000;
					break;

				case NAME_Color_Walltop:
					sec->SpecialColors[sector_t::walltop] = CheckInt(key) | 0xff000000;
					break;

				case NAME_Color_Wallbottom:
					sec->SpecialColors[sector_t::wallbottom] = CheckInt(key) | 0xff000000;
					break;

				case NAME_Color_Sprites:
					sec->SpecialColors[sector_t::sprites] = CheckInt(key) | 0xff000000;
					break;

				case NAME_ColorAdd_Floor:
					sec->AdditiveColors[sector_t::floor] = CheckInt(key) | 0xff000000; // Alpha is used to decide whether or not to use the color
					break;

				case NAME_ColorAdd_Ceiling:
					sec->AdditiveColors[sector_t::ceiling] = CheckInt(key) | 0xff000000;
					break;

				case NAME_ColorAdd_Walls:
					sec->AdditiveColors[sector_t::walltop] = CheckInt(key) | 0xff000000;
					break;

				case NAME_ColorAdd_Sprites:
					sec->AdditiveColors[sector_t::sprites] = CheckInt(key) | 0xff000000;
					break;

				case NAME_NoSkyWalls:
					Flag(sec->MoreFlags, SECMF_NOSKYWALLS, key);
					break;

				case NAME_colorization_floor:
					sec->SetTextureFx(sector_t::floor, TexMan.GetTextureManipulation(CheckString(key)));
					break;

				case NAME_colorization_ceiling:
					sec->SetTextureFx(sector_t::ceiling, TexMan.GetTextureManipulation(CheckString(key)));
					break;

				case NAME_Desaturation:
					desaturation = int(255*CheckFloat(key) + FLT_EPSILON);	// FLT_EPSILON to avoid rounding errors with numbers slightly below a full integer.
					continue;

				case NAME_fogdensity:
					fogdensity = CheckInt(key);
					break;

				case NAME_Silent:
					Flag(sec->Flags, SECF_SILENT, key);
					continue;

				case NAME_NoRespawn:
					Flag(sec->Flags, SECF_NORESPAWN, key);
					continue;

				case NAME_Nofallingdamage:
					Flag(sec->Flags, SECF_NOFALLINGDAMAGE, key);
					continue;

				case NAME_Dropactors:
					Flag(sec->Flags, SECF_FLOORDROP, key);
					continue;

				case NAME_SoundSequence:
					sec->SeqName = CheckString(key);
					sec->seqType = -1;
					continue;

				case NAME_hidden:
					Flag(sec->MoreFlags, SECMF_HIDDEN, key);
					break;

				case NAME_Waterzone:
					Flag(sec->MoreFlags, SECMF_UNDERWATER, key);
					break;

				case NAME_floorplane_a:
					fplaneflags |= 1;
					fp[0] = CheckFloat(key);
					break;

				case NAME_floorplane_b:
					fplaneflags |= 2;
					fp[1] = CheckFloat(key);
					break;

				case NAME_floorplane_c:
					fplaneflags |= 4;
					fp[2] = CheckFloat(key);
					break;

				case NAME_floorplane_d:
					fplaneflags |= 8;
					fp[3] = CheckFloat(key);
					break;

				case NAME_ceilingplane_a:
					cplaneflags |= 1;
					cp[0] = CheckFloat(key);
					break;

				case NAME_ceilingplane_b:
					cplaneflags |= 2;
					cp[1] = CheckFloat(key);
					break;

				case NAME_ceilingplane_c:
					cplaneflags |= 4;
					cp[2] = CheckFloat(key);
					break;

				case NAME_ceilingplane_d:
					cplaneflags |= 8;
					cp[3] = CheckFloat(key);
					break;

				case NAME_damageamount:
					sec->damageamount = CheckInt(key);
					break;

				case NAME_damagetype:
					sec->damagetype = CheckString(key);
					break;

				case NAME_damageinterval:
					sec->damageinterval = CheckInt(key);
					if (sec->damageinterval < 1) sec->damageinterval = 1;
					break;

				case NAME_leakiness:
					sec->leakydamage = CheckInt(key);
					break;

				case NAME_damageterraineffect:
					Flag(sec->Flags, SECF_DMGTERRAINFX, key);
					break;

				case NAME_damagehazard:
					Flag(sec->Flags, SECF_HAZARD, key);
					break;

				case NAME_hurtmonsters:
					Flag(sec->MoreFlags, SECMF_HURTMONSTERS, key);
					break;

				case NAME_harminair:
					Flag(sec->MoreFlags, SECMF_HARMINAIR, key);
					break;

				case NAME_floorterrain:
					sec->terrainnum[sector_t::floor] = P_FindTerrain(CheckString(key));
					break;

				case NAME_ceilingterrain:
					sec->terrainnum[sector_t::ceiling] = P_FindTerrain(CheckString(key));
					break;

				case NAME_floor_reflect:
					sec->reflect[sector_t::floor] = (float)CheckFloat(key);
					break;

				case NAME_ceiling_reflect:
					sec->reflect[sector_t::ceiling] = (float)CheckFloat(key);
					break;

				case NAME_floorglowcolor:
					sec->SetGlowColor(sector_t::floor, CheckInt(key));
					break;

				case NAME_floorglowheight:
					sec->SetGlowHeight(sector_t::floor, (float)CheckFloat(key));
					break;

				case NAME_ceilingglowcolor:
					sec->SetGlowColor(sector_t::ceiling, CheckInt(key));
					break;

				case NAME_ceilingglowheight:
					sec->SetGlowHeight(sector_t::ceiling, (float)CheckFloat(key));
					break;

				case NAME_Noattack:
					Flag(sec->Flags, SECF_NOATTACK, key);
					break;

				case NAME_MoreIds:
					// delay parsing of the tag string until parsing of the sector is complete
					// This ensures that the ID is always the first tag in the list.
					tagstring = CheckString(key);
					break;

				case NAME_portal_ceil_blocksound:
					Flag(sec->planes[sector_t::ceiling].Flags, PLANEF_BLOCKSOUND, key);
					break;

				case NAME_portal_ceil_disabled:
					Flag(sec->planes[sector_t::ceiling].Flags, PLANEF_DISABLED, key);
					break;

				case NAME_portal_ceil_nopass:
					Flag(sec->planes[sector_t::ceiling].Flags, PLANEF_NOPASS, key);
					break;

				case NAME_portal_ceil_norender:
					Flag(sec->planes[sector_t::ceiling].Flags, PLANEF_NORENDER, key);
					break;

				case NAME_portal_ceil_overlaytype:
					if (!stricmp(CheckString(key), "translucent")) sec->planes[sector_t::ceiling].Flags &= ~PLANEF_ADDITIVE;
					else if (!stricmp(CheckString(key), "additive")) sec->planes[sector_t::ceiling].Flags |= PLANEF_ADDITIVE;
					break;
				
				case NAME_portal_floor_blocksound:
					Flag(sec->planes[sector_t::floor].Flags, PLANEF_BLOCKSOUND, key);
					break;

				case NAME_portal_floor_disabled:
					Flag(sec->planes[sector_t::floor].Flags, PLANEF_DISABLED, key);
					break;

				case NAME_portal_floor_nopass:
					Flag(sec->planes[sector_t::floor].Flags, PLANEF_NOPASS, key);
					break;

				case NAME_portal_floor_norender:
					Flag(sec->planes[sector_t::floor].Flags, PLANEF_NORENDER, key);
					break;

				case NAME_portal_floor_overlaytype:
					if (!stricmp(CheckString(key), "translucent")) sec->planes[sector_t::floor].Flags &= ~PLANEF_ADDITIVE;
					else if (!stricmp(CheckString(key), "additive")) sec->planes[sector_t::floor].Flags |= PLANEF_ADDITIVE;
					break;

				case NAME_scroll_ceil_x:
					scroll_ceil_x = CheckFloat(key) * scrollfactor;
					break;

				case NAME_xscrollceiling:
					scroll_ceil_x = CheckFloat(key);
					break;

				case NAME_scroll_ceil_y:
					scroll_ceil_y = CheckFloat(key) * scrollfactor;
					break;

				case NAME_yscrollceiling:
					scroll_ceil_y = CheckFloat(key);
					break;

				case NAME_scrollceilingmode:
					scroll_ceil_type = CheckInt(key);
					break;

				case NAME_scroll_ceil_type:
				{
					const char* val = CheckString(key);
					if (!stricmp(val, "both")) scroll_ceil_type = SCROLL_All;
					else if (!stricmp(val, "visual")) scroll_ceil_type = SCROLL_Textures;
					if (!stricmp(val, "physical")) scroll_ceil_type = SCROLL_All & ~SCROLL_Textures;
					else scroll_ceil_type = 0;
					break;
				}

				case NAME_scroll_floor_x:
					scroll_floor_x = CheckFloat(key) * scrollfactor;
					break;

				case NAME_xscrollfloor:
					scroll_floor_x = CheckFloat(key);
					break;

				case NAME_scroll_floor_y:
					scroll_floor_y = CheckFloat(key) * scrollfactor;
					break;

				case NAME_yscrollfloor:
					scroll_floor_y = CheckFloat(key);
					break;

				case NAME_scrollfloormode:
					scroll_floor_type = CheckInt(key);
					break;

				case NAME_scroll_floor_type:
				{
					const char* val = CheckString(key);
					if (!stricmp(val, "both")) scroll_floor_type = SCROLL_All;
					else if (!stricmp(val, "visual")) scroll_floor_type = SCROLL_Textures;
					if (!stricmp(val, "physical")) scroll_floor_type = SCROLL_All & ~SCROLL_Textures;
					else scroll_floor_type = 0;
					break;
				}

				case NAME_colormap:
					sec->selfmap = R_ColormapNumForName(CheckString(key));
					break;

				case NAME_frictionfactor:
					friction = CheckFloat(key);
					break;

				case NAME_movefactor:
					movefactor = CheckFloat(key);
					break;

				case NAME_skyfloor:
					sec->planes[sector_t::floor].skytexture[0] = TexMan.CheckForTexture(CheckString(key), ETextureType::Wall, FTextureManager::TEXMAN_TryAny | FTextureManager::TEXMAN_ReturnFirst);
					break;

				case NAME_skyfloor2:
					sec->planes[sector_t::floor].skytexture[1] = TexMan.CheckForTexture(CheckString(key), ETextureType::Wall, FTextureManager::TEXMAN_TryAny | FTextureManager::TEXMAN_ReturnFirst);
					break;

				case NAME_skyceiling:
					sec->planes[sector_t::ceiling].skytexture[0] = TexMan.CheckForTexture(CheckString(key), ETextureType::Wall, FTextureManager::TEXMAN_TryAny | FTextureManager::TEXMAN_ReturnFirst);
					break;

				case NAME_skyceiling2:
					sec->planes[sector_t::ceiling].skytexture[1] = TexMan.CheckForTexture(CheckString(key), ETextureType::Wall, FTextureManager::TEXMAN_TryAny | FTextureManager::TEXMAN_ReturnFirst);
					break;

				case NAME_xthrust:
					thrust.X = CheckFloat(key);
					break;

				case NAME_ythrust:
					thrust.Y = CheckFloat(key);
					break;

				case NAME_thrustgroup:
					thrustgroup = CheckInt(key);
					break;

				case NAME_thrustlocation:
					thrustlocation = CheckInt(key);
					break;

					// These two are used by Eternity for something I do not understand.
				//case NAME_portal_ceil_useglobaltex:
				//case NAME_portal_floor_useglobaltex:

				case NAME_HealthFloor:
					sec->healthfloor = CheckInt(key);
					break;

				case NAME_HealthCeiling:
					sec->healthceiling = CheckInt(key);
					break;

				case NAME_Health3D:
					sec->health3d = CheckInt(key);
					break;

				case NAME_HealthFloorGroup:
					sec->healthfloorgroup = CheckInt(key);
					break;

				case NAME_HealthCeilingGroup:
					sec->healthceilinggroup = CheckInt(key);
					break;

				case NAME_Health3DGroup:
					sec->health3dgroup = CheckInt(key);
					break;

				case NAME_lm_sampledist_floor:
				case NAME_lm_sampledist_ceiling:
					CHECK_N(Zd | Zdt)
					break;

				default:
					if (strnicmp("user_", key.GetChars(), 5))
						DPrintf(DMSG_WARNING, "Unknown UDMF sector key %s\n", key.GetChars());
					break;
			}
			if ((namespace_bits & (Zd | Zdt)) && !strnicmp("user_", key.GetChars(), 5))
			{
				AddUserKey(key, UDMF_Sector, index);
			}
		}

		if (tagstring.IsNotEmpty())
		{
			FScanner sc;
			sc.OpenString("tagstring", tagstring);
			// scan the string as long as valid numbers can be found
			while (sc.CheckNumber())
			{
				if (sc.Number != 0)	Level->tagManager.AddSectorTag(index, sc.Number);
			}
		}

		if (sec->damageamount == 0)
		{
			// If no damage is set, clear all other related properties so that they do not interfere
			// with other means of setting them.
			sec->damagetype = NAME_None;
			sec->damageinterval = 0;
			sec->leakydamage = 0;
			sec->Flags &= ~SECF_DAMAGEFLAGS;
		}

		// These cannot be initialized yet because they need the final sector array.
		if (scroll_ceil_type != NAME_None)
		{
			UDMFSectorScrollers.Push({ true, index, scroll_ceil_x, scroll_ceil_y, scroll_ceil_type });
		}
		if (scroll_floor_type != NAME_None)
		{
			UDMFSectorScrollers.Push({ false, index, scroll_floor_x, scroll_floor_y, scroll_floor_type });
		}
		if (!thrust.isZero())
		{
			UDMFThrusters.Push({ thrustlocation, index, thrust.X, thrust.Y, thrustgroup });
		}

		
		// Reset the planes to their defaults if not all of the plane equation's parameters were found.
		if (fplaneflags != 15)
		{
			sec->floorplane.SetAtHeight(sec->GetPlaneTexZ(sector_t::floor), sector_t::floor);
		}
		else
		{
			// normalize the vector, it must have a length of 1
			DVector3 n = DVector3(fp[0], fp[1], fp[2]).Unit();
			sec->floorplane.set(n.X, n.Y, n.Z, fp[3]);
		}
		if (cplaneflags != 15)
		{
			sec->ceilingplane.SetAtHeight(sec->GetPlaneTexZ(sector_t::ceiling), sector_t::ceiling);
		}
		else
		{
			DVector3 n = DVector3(cp[0], cp[1], cp[2]).Unit();
			sec->ceilingplane.set(n.X, n.Y, n.Z, cp[3]);
		}
		sec->CheckOverlap();

		if (lightcolor == ~0u && fadecolor == ~0u && desaturation == -1 && fogdensity == -1)
		{
			// [RH] Sectors default to white light with the default fade.
			//		If they are outside (have a sky ceiling), they use the outside fog.
			sec->Colormap.LightColor = PalEntry(255, 255, 255);
			if (Level->outsidefog != 0xff000000 && (sec->GetTexture(sector_t::ceiling) == skyflatnum || (sec->special & 0xff) == Sector_Outside))
			{
				sec->Colormap.FadeColor.SetRGB(Level->outsidefog);
			}
			else
			{
				sec->Colormap.FadeColor.SetRGB(Level->fadeto);
			}
		}
		else
		{
			sec->Colormap.LightColor.SetRGB(lightcolor);
			if (fadecolor == ~0u)
			{
				if (Level->outsidefog != 0xff000000 && (sec->GetTexture(sector_t::ceiling) == skyflatnum || (sec->special & 0xff) == Sector_Outside))
					fadecolor = Level->outsidefog;
				else
					fadecolor = Level->fadeto;
			}
			sec->Colormap.FadeColor.SetRGB(fadecolor);
			sec->Colormap.Desaturation = clamp(desaturation, 0, 255);
			sec->Colormap.FogDensity = clamp(fogdensity, 0, 512) / 2;
		}
		if (friction > -FLT_MAX)
		{
			sec->friction = clamp(friction, 0., 1.);
			sec->movefactor = FrictionToMoveFactor(sec->friction);
			sec->Flags |= SECF_FRICTION;
		}
		if (movefactor > -FLT_MAX)
		{
			sec->movefactor = max(movefactor, 1 / 2048.);
		}
	}

	//===========================================================================
	//
	// parse a vertex block
	//
	//===========================================================================

	void ParseVertex(vertex_t *vt, vertexdata_t *vd)
	{
		vt->set(0, 0);
		vd->zCeiling = vd->zFloor = vd->flags = 0;

		sc.MustGetToken('{');
		double x = 0, y = 0;
		while (!sc.CheckToken('}'))
		{
			FName key = ParseKey();
			switch (key.GetIndex())
			{
			case NAME_X:
				x = CheckCoordinate(key);
				break;

			case NAME_Y:
				y = CheckCoordinate(key);
				break;

			case NAME_ZCeiling:
				vd->zCeiling = CheckCoordinate(key);
				vd->flags |= VERTEXFLAG_ZCeilingEnabled;
				break;

			case NAME_ZFloor:
				vd->zFloor = CheckCoordinate(key);
				vd->flags |= VERTEXFLAG_ZFloorEnabled;
				break;

			default:
				break;
			}
		}
		vt->set(x, y);
	}

	//===========================================================================
	//
	// Processes the linedefs after the map has been loaded
	//
	//===========================================================================

	void ProcessLineDefs(TArray<TArray<int>>& siderefs)
	{
		int sidecount = 0;
		for(unsigned i = 0, skipped = 0; i < ParsedLines.Size();)
		{
			// Relocate the vertices
			intptr_t v1i = intptr_t(ParsedLines[i].v1);
			intptr_t v2i = intptr_t(ParsedLines[i].v2);

			if ((unsigned)v1i >= Level->vertexes.Size() || (unsigned)v2i >= Level->vertexes.Size())
			{
				I_Error ("Line %d has invalid vertices: %zd and/or %zd.\nThe map only contains %u vertices.", i+skipped, v1i, v2i, Level->vertexes.Size());
			}
			else if (v1i == v2i ||
				(Level->vertexes[v1i].fX() == Level->vertexes[v2i].fX() && Level->vertexes[v1i].fY() == Level->vertexes[v2i].fY()))
			{
				Printf ("Removing 0-length line %d\n", i+skipped);
				ParsedLines.Delete(i);
				loader->ForceNodeBuild = true;
				skipped++;
			}
			else
			{
				ParsedLines[i].v1 = &Level->vertexes[v1i];
				ParsedLines[i].v2 = &Level->vertexes[v2i];

				if (ParsedLines[i].sidedef[0] != NULL)
					sidecount++;
				if (ParsedLines[i].sidedef[1] != NULL)
					sidecount++;
				loader->linemap.Push(i+skipped);
				i++;
			}
		}
		unsigned numlines = ParsedLines.Size();
		siderefs.Resize(ParsedSides.Size());
		Level->sides.Alloc(sidecount);
		Level->lines.Alloc(numlines);
		int line, side;
		auto lines = &Level->lines[0];
		auto sides = &Level->sides[0];

		for(line = 0, side = 0; line < (int)numlines; line++)
		{
			short tempalpha[2] = { SHRT_MIN, SHRT_MIN };

			lines[line] = ParsedLines[line];

			for(int sd = 0; sd < 2; sd++)
			{
				if (lines[line].sidedef[sd] != NULL)
				{
					int mapside = int(intptr_t(lines[line].sidedef[sd]))-1;
					if (mapside < sidecount)
					{
						siderefs[mapside].Push(side);
						sides[side] = ParsedSides[mapside];
						sides[side].linedef = &lines[line];
						sides[side].sector = &Level->sectors[intptr_t(sides[side].sector)];
						lines[line].sidedef[sd] = &sides[side];

#if 0
						if (sd == 1)
						{
							// fix flags for backside. The definition is linedef relative, not sidedef relative.
							static const uint8_t swaps[] = { 0, side_t::skew_back, side_t::skew_front };
							static const uint8_t swapsm[] = {0, side_t::skew_back_floor, side_t::skew_back_ceiling, side_t::skew_front_floor, side_t::skew_front_ceiling};

							sides[side].textures[side_t::top].skew = swaps[sides[side].textures[side_t::top].skew];
							sides[side].textures[side_t::bottom].skew = swaps[sides[side].textures[side_t::bottom].skew];
							sides[side].textures[side_t::mid].skew = swapsm[sides[side].textures[side_t::mid].skew];
						}
#endif

						loader->ProcessSideTextures(!isExtended, &sides[side], sides[side].sector, &ParsedSideTextures[mapside],
							lines[line].special, lines[line].args[0], &tempalpha[sd], missingTex);

						side++;
					}
					else
					{
						lines[line].sidedef[sd] = NULL;
					}
				}
			}

			lines[line].AdjustLine();
			loader->FinishLoadingLineDef(&lines[line], tempalpha[0]);
		}

		const int sideDelta = Level->sides.Size() - side;
		assert(sideDelta >= 0);

		if (sideDelta < 0)
		{
			Printf("Map had %d invalid side references\n", abs(sideDelta));
		}
		else if (sideDelta > 0)
		{
			Level->sides.Resize(side);
		}
	}

	//===========================================================================
	//
	// Main parsing function
	//
	//===========================================================================

	void ParseTextMap(MapData *map)
	{
		isTranslated = true;
		isExtended = false;
		floordrop = false;

		sc.OpenMem(fileSystem.GetFileFullName(map->lumpnum), map->Read(ML_TEXTMAP));
		sc.SetCMode(true);
		if (sc.CheckString("namespace"))
		{
			sc.MustGetStringName("=");
			sc.MustGetString();
			namespc = sc.String;
			switch(namespc.GetIndex())
			{
			case NAME_Dsda:
			case NAME_ZDoom:
			case NAME_Eternity:
				namespace_bits = Zd;
				isTranslated = false;
				break;
			case NAME_ZDoomTranslated:
				Level->flags2 |= LEVEL2_DUMMYSWITCHES;
				namespace_bits = Zdt;
				break;
			case NAME_Vavoom:
				namespace_bits = Va;
				isTranslated = false;
				break;
			case NAME_Hexen:
				namespace_bits = Hx;
				isTranslated = false;
				break;
			case NAME_Doom:
				namespace_bits = Dm;
				Level->Translator = P_LoadTranslator("xlat/doom_base.txt");
				Level->flags2 |= LEVEL2_DUMMYSWITCHES;
				floordrop = true;
				break;
			case NAME_Heretic:
				namespace_bits = Ht;
				Level->Translator = P_LoadTranslator("xlat/heretic_base.txt");
				Level->flags2 |= LEVEL2_DUMMYSWITCHES;
				floordrop = true;
				break;
			case NAME_Strife:
				namespace_bits = St;
				Level->Translator = P_LoadTranslator("xlat/strife_base.txt");
				Level->flags2 |= LEVEL2_DUMMYSWITCHES;
				floordrop = true;
				break;
			default:
				Printf("Unknown namespace %s. Using defaults for %s\n", sc.String, GameTypeName());
				switch (gameinfo.gametype)
				{
				default:			// Shh, GCC
				case GAME_Doom:
				case GAME_Chex:
					namespace_bits = Dm;
					Level->Translator = P_LoadTranslator("xlat/doom_base.txt");
					break;
				case GAME_Heretic:
					namespace_bits = Ht;
					Level->Translator = P_LoadTranslator("xlat/heretic_base.txt");
					break;
				case GAME_Strife:
					namespace_bits = St;
					Level->Translator = P_LoadTranslator("xlat/strife_base.txt");
					break;
				case GAME_Hexen:
					namespace_bits = Hx;
					isTranslated = false;
					break;
				}
			}
			sc.MustGetStringName(";");
		}
		else
		{
			Printf("Map does not define a namespace.\n");
		}

		while (sc.GetString())
		{
			if (sc.Compare("thing"))
			{
				FMapThing th;
				unsigned userdatastart = loader->MapThingsUserData.Size();
				ParseThing(&th);
				loader->MapThingsConverted.Push(th);
				if (userdatastart < loader->MapThingsUserData.Size())
				{ // User data added
					loader->MapThingsUserDataIndex[loader->MapThingsConverted.Size()-1] = userdatastart;
					// Mark end of the user data for this map thing
					FUDMFKey ukey;
					ukey.Key = NAME_None;
					ukey = 0;
					loader->MapThingsUserData.Push(ukey);
				}
			}
			else if (sc.Compare("linedef"))
			{
				line_t li;
				ParseLinedef(&li, ParsedLines.Size());
				ParsedLines.Push(li);
			}
			else if (sc.Compare("sidedef"))
			{
				side_t si;
				intmapsidedef_t st;
				ParseSidedef(&si, &st, ParsedSides.Size());
				ParsedSides.Push(si);
				ParsedSideTextures.Push(st);
			}
			else if (sc.Compare("sector"))
			{
				sector_t sec;
				memset(&sec, 0, sizeof(sector_t));
				ParseSector(&sec, ParsedSectors.Size());
				ParsedSectors.Push(sec);
			}
			else if (sc.Compare("vertex"))
			{
				vertex_t vt;
				vertexdata_t vd;
				ParseVertex(&vt, &vd);
				ParsedVertices.Push(vt);
				loader->vertexdatas.Push(vd);
			}
			else
			{
				Skip();
			}
		}

		// Catch bogus maps here rather than during nodebuilding
		if (ParsedVertices.Size() == 0)	I_Error("Map has no vertices.");
		if (ParsedSectors.Size() == 0)	I_Error("Map has no sectors. ");
		if (ParsedLines.Size() == 0)	I_Error("Map has no linedefs.");
		if (ParsedSides.Size() == 0)	I_Error("Map has no sidedefs.");
		if (BadCoordinates)				I_Error("Map has out of range coordinates");

		// Create the real vertices
		Level->vertexes.Alloc(ParsedVertices.Size());
		memcpy(&Level->vertexes[0], &ParsedVertices[0], Level->vertexes.Size() * sizeof(vertex_t));

		// Create the real sectors
		Level->sectors.Alloc(ParsedSectors.Size());
		memcpy(&Level->sectors[0], &ParsedSectors[0], Level->sectors.Size() * sizeof(sector_t));
		Level->extsectors.Alloc(Level->sectors.Size());
		for(unsigned i = 0; i < Level->sectors.Size(); i++)
		{
			Level->sectors[i].e = &Level->extsectors[i];
		}

		// Create the real linedefs and decompress the sidedefs. Must be done before
		TArray<TArray<int>> siderefs;
		ProcessLineDefs(siderefs);

		// Now create the scrollers.
		for (auto &scroll : UDMFSectorScrollers)
		{
			if (scroll.scrolltype & SCROLL_Textures)
			{
				loader->CreateScroller(scroll.where == 1 ? EScroll::sc_ceiling : EScroll::sc_floor, -scroll.x, scroll.y, &Level->sectors[scroll.index], nullptr, 0);
			}
			if (scroll.scrolltype & (SCROLL_StaticObjects | SCROLL_Players | SCROLL_Monsters))
			{
				loader->CreateScroller(scroll.where == 1 ? EScroll::sc_carry_ceiling : EScroll::sc_carry, scroll.x, scroll.y, &Level->sectors[scroll.index], nullptr, 0, scw_all, scroll.scrolltype);
			}
		}
		for (auto& scroll : UDMFWallScrollers)
		{
			for(auto sd : siderefs[scroll.index])
			{
				loader->CreateScroller(EScroll::sc_side, scroll.x, scroll.y, nullptr, &Level->sides[sd], 0);
			}
		}
		for (auto& scroll : UDMFThrusters)
		{
			Level->CreateThinker<DThruster>(&Level->sectors[scroll.index], scroll.x, scroll.y, scroll.scrolltype, scroll.where);
		}
	}
};

void MapLoader::ParseTextMap(MapData *map, FMissingTextureTracker &missingtex)
{
	UDMFParser parse(this, missingtex);

	parse.ParseTextMap(map);
}

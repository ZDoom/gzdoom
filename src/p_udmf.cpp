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
#include "templates.h"
#include "i_system.h"
#include "gi.h"
#include "r_sky.h"
#include "g_level.h"
#include "v_palette.h"
#include "p_udmf.h"
#include "r_state.h"
#include "r_data/colormaps.h"
#include "w_wad.h"

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

static inline bool P_IsThingSpecial(int specnum)
{
	return (specnum >= Thing_Projectile && specnum <= Thing_SpawnNoFog) ||
			specnum == Thing_SpawnFacing || Thing_ProjectileIntercept || Thing_ProjectileAimed;
}

enum
{
	Dm=1,
	Ht=2,
	Hx=4,
	St=8,
	Zd=16,
	Zdt=32,
	Va=64,

	// will be extended later. Unknown namespaces will always be treated like the base
	// namespace for each game
};

void SetTexture (sector_t *sector, int index, int position, const char *name8, FMissingTextureTracker &);
void P_ProcessSideTextures(bool checktranmap, side_t *sd, sector_t *sec, mapsidedef_t *msd, int special, int tag, short *alpha, FMissingTextureTracker &);
void P_AdjustLine (line_t *ld);
void P_FinishLoadingLineDef(line_t *ld, int alpha);
void SpawnMapThing(int index, FMapThing *mt, int position);
extern bool		ForceNodeBuild;
extern TArray<FMapThing> MapThingsConverted;
extern TArray<int>		linemap;


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
	if (developer) sc.ScriptMessage("Ignoring unknown key \"%s\".", sc.String);
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

int UDMFParserBase::CheckInt(const char *key)
{
	if (sc.TokenType != TK_IntConst)
	{
		sc.ScriptMessage("Integer value expected for key '%s'", key);
	}
	return sc.Number;
}

double UDMFParserBase::CheckFloat(const char *key)
{
	if (sc.TokenType != TK_IntConst && sc.TokenType != TK_FloatConst)
	{
		sc.ScriptMessage("Floating point value expected for key '%s'", key);
	}
	return sc.Float;
}

fixed_t UDMFParserBase::CheckFixed(const char *key)
{
	return FLOAT2FIXED(CheckFloat(key));
}

angle_t UDMFParserBase::CheckAngle(const char *key)
{
	return angle_t(CheckFloat(key) * ANGLE_90 / 90.);
}

bool UDMFParserBase::CheckBool(const char *key)
{
	if (sc.TokenType == TK_True) return true;
	if (sc.TokenType == TK_False) return false;
	sc.ScriptMessage("Boolean value expected for key '%s'", key);
	return false;
}

const char *UDMFParserBase::CheckString(const char *key)
{
	if (sc.TokenType != TK_StringConst)
	{
		sc.ScriptMessage("String value expected for key '%s'", key);
	}
	return parsedString;
}

//===========================================================================
//
// Storage of UDMF user properties
//
//===========================================================================

typedef TMap<int, FUDMFKeys> FUDMFKeyMap;


static FUDMFKeyMap UDMFKeys[4];
// Things must be handled differently

void P_ClearUDMFKeys()
{
	for(int i=0;i<4;i++)
	{
		UDMFKeys[i].Clear();
	}
}

static int STACK_ARGS udmfcmp(const void *a, const void *b)
{
	FUDMFKey *A = (FUDMFKey*)a;
	FUDMFKey *B = (FUDMFKey*)b;

	return int(A->Key) - int(B->Key);
}

void FUDMFKeys::Sort()
{
	qsort(&(*this)[0], Size(), sizeof(FUDMFKey), udmfcmp);
}

FUDMFKey *FUDMFKeys::Find(FName key)
{
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

int GetUDMFInt(int type, int index, const char *key)
{
	assert(type >=0 && type <=3);

	if (index > 0)
	{
		FUDMFKeys *pKeys = UDMFKeys[type].CheckKey(index);

		if (pKeys != NULL)
		{
			FUDMFKey *pKey = pKeys->Find(key);
			if (pKey != NULL)
			{
				return pKey->IntVal;
			}
		}
	}
	return 0;
}

fixed_t GetUDMFFixed(int type, int index, const char *key)
{
	assert(type >=0 && type <=3);

	if (index > 0)
	{
		FUDMFKeys *pKeys = UDMFKeys[type].CheckKey(index);

		if (pKeys != NULL)
		{
			FUDMFKey *pKey = pKeys->Find(key);
			if (pKey != NULL)
			{
				return FLOAT2FIXED(pKey->FloatVal);
			}
		}
	}
	return 0;
}


//===========================================================================
//
// UDMF parser
//
//===========================================================================

class UDMFParser : public UDMFParserBase
{
	bool isTranslated;
	bool isExtended;
	bool floordrop;

	TArray<line_t> ParsedLines;
	TArray<side_t> ParsedSides;
	TArray<mapsidedef_t> ParsedSideTextures;
	TArray<sector_t> ParsedSectors;
	TArray<vertex_t> ParsedVertices;
	TArray<vertexdata_t> ParsedVertexDatas;

	FDynamicColormap	*fogMap, *normMap;
	FMissingTextureTracker &missingTex;

public:
	UDMFParser(FMissingTextureTracker &missing)
		: missingTex(missing)
	{
		linemap.Clear();
		fogMap = normMap = NULL;
	}

	void AddUserKey(FName key, int kind, int index)
	{
		FUDMFKeys &keyarray = UDMFKeys[kind][index];

		for(unsigned i=0; i < keyarray.Size(); i++)
		{
			if (keyarray[i].Key == key)
			{
				switch (sc.TokenType)
				{
				case TK_IntConst:
					keyarray[i] = sc.Number;
					break;
				case TK_FloatConst:
					keyarray[i] = sc.Float;
					break;
				default:
				case TK_StringConst:
					keyarray[i] = parsedString;
					break;
				case TK_True:
					keyarray[i] = 1;
					break;
				case TK_False:
					keyarray[i] = 0;
					break;
				}
				return;
			}
		}
		FUDMFKey ukey;
		ukey.Key = key;
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
		th->gravity = FRACUNIT;
		th->RenderStyle = STYLE_Count;
		th->alpha = -1;
		th->health = 1;
		sc.MustGetToken('{');
		while (!sc.CheckToken('}'))
		{
			FName key = ParseKey();
			switch(key)
			{
			case NAME_Id:
				th->thingid = CheckInt(key);
				break;

			case NAME_X:
				th->x = CheckFixed(key);
				break;

			case NAME_Y:
				th->y = CheckFixed(key);
				break;

			case NAME_Height:
				th->z = CheckFixed(key);
				break;

			case NAME_Angle:
				th->angle = (short)CheckInt(key);
				break;

			case NAME_Type:
				th->type = (short)CheckInt(key);
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
				th->gravity = CheckFixed(key);
				break;

			case NAME_Arg0:
			case NAME_Arg1:
			case NAME_Arg2:
			case NAME_Arg3:
			case NAME_Arg4:
				CHECK_N(Hx | Zd | Zdt | Va)
				th->args[int(key)-int(NAME_Arg0)] = CheckInt(key);
				break;

			case NAME_Arg0Str:
				CHECK_N(Zd);
				arg0str = CheckString(key);
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
				if (CheckBool(key)) th->SkillFilter |= (1<<(int(key)-NAME_Skill1));
				else th->SkillFilter &= ~(1<<(int(key)-NAME_Skill1));
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
				if (CheckBool(key)) th->ClassFilter |= (1<<(int(key)-NAME_Class1));
				else th->ClassFilter &= ~(1<<(int(key)-NAME_Class1));
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

			case NAME_Renderstyle:
				{
				FName style = CheckString(key);
				switch (style)
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
				default:
					break;
				}
				}
				break;

			case NAME_Alpha:
				th->alpha = CheckFixed(key);
				break;

			case NAME_FillColor:
				th->fillcolor = CheckInt(key);

			case NAME_Health:
				th->health = CheckInt(key);
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
				th->scaleX = CheckFixed(key);
				break;

			case NAME_ScaleY:
				th->scaleY = CheckFixed(key);
				break;

			case NAME_Scale:
				th->scaleX = th->scaleY = CheckFixed(key);
				break;

			default:
				if (0 == strnicmp("user_", key.GetChars(), 5))
				{ // Custom user key - Sets an actor's user variable directly
					FMapThingUserData ud;
					ud.Property = key;
					ud.Value = CheckInt(key);
					MapThingsUserData.Push(ud);
				}
				break;
			}
		}
		if (arg0str.IsNotEmpty() && (P_IsACSSpecial(th->special) || th->special == 0))
		{
			th->args[0] = -FName(arg0str);
		}
		if (arg1str.IsNotEmpty() && (P_IsThingSpecial(th->special) || th->special == 0))
		{
			th->args[1] = -FName(arg1str);
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
				P_TranslateLineDef(&ld, &mld);
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

		memset(ld, 0, sizeof(*ld));
		ld->Alpha = FRACUNIT;
		ld->id = -1;
		ld->sidedef[0] = ld->sidedef[1] = NULL;
		if (level.flags2 & LEVEL2_CLIPMIDTEX) ld->flags |= ML_CLIP_MIDTEX;
		if (level.flags2 & LEVEL2_WRAPMIDTEX) ld->flags |= ML_WRAP_MIDTEX;
		if (level.flags2 & LEVEL2_CHECKSWITCHRANGE) ld->flags |= ML_CHECKSWITCHRANGE;

		sc.MustGetToken('{');
		while (!sc.CheckToken('}'))
		{
			FName key = ParseKey();

			// This switch contains all keys of the UDMF base spec
			switch(key)
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
				ld->id = CheckInt(key);
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
				ld->args[int(key)-int(NAME_Arg0)] = CheckInt(key);
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
				break;
			}

			// This switch contains all keys of the UDMF base spec which only apply to Hexen format specials
			if (!isTranslated) switch (key)
			{
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
			if (namespace_bits & (Zd|Zdt|Va)) switch(key)
			{
			case NAME_Alpha:
				ld->Alpha = CheckFixed(key);
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
			
			// [Dusk] lock number
			case NAME_Locknumber:
				ld->locknumber = CheckInt(key);
				continue;

			default:
				break;
			}

			if (!strnicmp("user_", key.GetChars(), 5))
			{
				AddUserKey(key, UDMF_Line, index);
			}
		}

		if (isTranslated)
		{
			int saved = ld->flags;

			maplinedef_t mld;
			memset(&mld, 0, sizeof(mld));
			mld.special = ld->special;
			mld.tag = ld->id;
			P_TranslateLineDef(ld, &mld);
			ld->flags = saved | (ld->flags&(ML_MONSTERSCANACTIVATE|ML_REPEAT_SPECIAL|ML_FIRSTSIDEONLY));
		}
		if (passuse && (ld->activation & SPAC_Use)) 
		{
			ld->activation = (ld->activation & ~SPAC_Use) | SPAC_UseThrough;
		}
		if (strifetrans && ld->Alpha == FRACUNIT)
		{
			ld->Alpha = FRACUNIT * 3/4;
		}
		if (strifetrans2 && ld->Alpha == FRACUNIT)
		{
			ld->Alpha = FRACUNIT * 1/4;
		}
		if (ld->sidedef[0] == NULL)
		{
			ld->sidedef[0] = (side_t*)(intptr_t)(1);
			Printf("Line %d has no first side.\n", index);
		}
		if (arg0str.IsNotEmpty() && (P_IsACSSpecial(ld->special) || ld->special == 0))
		{
			ld->args[0] = -FName(arg0str);
		}
		if (arg1str.IsNotEmpty() && (P_IsThingSpecial(ld->special) || ld->special == 0))
		{
			ld->args[1] = -FName(arg1str);
		}
	}

	//===========================================================================
	//
	// Parse a sidedef block
	//
	//===========================================================================

	void ParseSidedef(side_t *sd, mapsidedef_t *sdt, int index)
	{
		fixed_t texofs[2]={0,0};

		memset(sd, 0, sizeof(*sd));
		strncpy(sdt->bottomtexture, "-", 8);
		strncpy(sdt->toptexture, "-", 8);
		strncpy(sdt->midtexture, "-", 8);
		sd->SetTextureXScale(FRACUNIT);
		sd->SetTextureYScale(FRACUNIT);

		sc.MustGetToken('{');
		while (!sc.CheckToken('}'))
		{
			FName key = ParseKey();
			switch(key)
			{
			case NAME_Offsetx:
				texofs[0] = CheckInt(key) << FRACBITS;
				continue;

			case NAME_Offsety:
				texofs[1] = CheckInt(key) << FRACBITS;
				continue;

			case NAME_Texturetop:
				strncpy(sdt->toptexture, CheckString(key), 8);
				continue;

			case NAME_Texturebottom:
				strncpy(sdt->bottomtexture, CheckString(key), 8);
				continue;

			case NAME_Texturemiddle:
				strncpy(sdt->midtexture, CheckString(key), 8);
				continue;

			case NAME_Sector:
				sd->sector = (sector_t*)(intptr_t)CheckInt(key);
				continue;

			default:
				break;
			}

			if (namespace_bits & (Zd|Zdt|Va)) switch(key)
			{
			case NAME_offsetx_top:
				sd->SetTextureXOffset(side_t::top, CheckFixed(key));
				continue;

			case NAME_offsety_top:
				sd->SetTextureYOffset(side_t::top, CheckFixed(key));
				continue;

			case NAME_offsetx_mid:
				sd->SetTextureXOffset(side_t::mid, CheckFixed(key));
				continue;

			case NAME_offsety_mid:
				sd->SetTextureYOffset(side_t::mid, CheckFixed(key));
				continue;

			case NAME_offsetx_bottom:
				sd->SetTextureXOffset(side_t::bottom, CheckFixed(key));
				continue;

			case NAME_offsety_bottom:
				sd->SetTextureYOffset(side_t::bottom, CheckFixed(key));
				continue;

			case NAME_scalex_top:
				sd->SetTextureXScale(side_t::top, CheckFixed(key));
				continue;

			case NAME_scaley_top:
				sd->SetTextureYScale(side_t::top, CheckFixed(key));
				continue;

			case NAME_scalex_mid:
				sd->SetTextureXScale(side_t::mid, CheckFixed(key));
				continue;

			case NAME_scaley_mid:
				sd->SetTextureYScale(side_t::mid, CheckFixed(key));
				continue;

			case NAME_scalex_bottom:
				sd->SetTextureXScale(side_t::bottom, CheckFixed(key));
				continue;

			case NAME_scaley_bottom:
				sd->SetTextureYScale(side_t::bottom, CheckFixed(key));
				continue;

			case NAME_light:
				sd->SetLight(CheckInt(key));
				continue;

			case NAME_lightabsolute:
				Flag(sd->Flags, WALLF_ABSLIGHTING, key);
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

			default:
				break;

			}
			if (!strnicmp("user_", key.GetChars(), 5))
			{
				AddUserKey(key, UDMF_Side, index);
			}
		}
		// initialization of these is delayed to allow separate offsets and add them with the global ones.
		sd->AddTextureXOffset(side_t::top, texofs[0]);
		sd->AddTextureXOffset(side_t::mid, texofs[0]);
		sd->AddTextureXOffset(side_t::bottom, texofs[0]);
		sd->AddTextureYOffset(side_t::top, texofs[1]);
		sd->AddTextureYOffset(side_t::mid, texofs[1]);
		sd->AddTextureYOffset(side_t::bottom, texofs[1]);
	}

	//===========================================================================
	//
	// parse a sector block
	//
	//===========================================================================

	void ParseSector(sector_t *sec, int index)
	{
		int lightcolor = -1;
		int fadecolor = -1;
		int desaturation = -1;

		memset(sec, 0, sizeof(*sec));
		sec->lightlevel = 160;
		sec->SetXScale(sector_t::floor, FRACUNIT);	// [RH] floor and ceiling scaling
		sec->SetYScale(sector_t::floor, FRACUNIT);
		sec->SetXScale(sector_t::ceiling, FRACUNIT);
		sec->SetYScale(sector_t::ceiling, FRACUNIT);
		sec->SetAlpha(sector_t::floor, FRACUNIT);
		sec->SetAlpha(sector_t::ceiling, FRACUNIT);
		sec->thinglist = NULL;
		sec->touching_thinglist = NULL;		// phares 3/14/98
		sec->seqType = (level.flags & LEVEL_SNDSEQTOTALCTRL) ? 0 : -1;
		sec->nextsec = -1;	//jff 2/26/98 add fields to support locking out
		sec->prevsec = -1;	// stair retriggering until build completes
		sec->heightsec = NULL;	// sector used to get floor and ceiling height
		sec->sectornum = index;
		if (floordrop) sec->Flags = SECF_FLOORDROP;
		// killough 3/7/98: end changes

		sec->gravity = 1.f;	// [RH] Default sector gravity of 1.0
		sec->ZoneNumber = 0xFFFF;

		// killough 8/28/98: initialize all sectors to normal friction
		sec->friction = ORIG_FRICTION;
		sec->movefactor = ORIG_FRICTION_FACTOR;

		sc.MustGetToken('{');
		while (!sc.CheckToken('}'))
		{
			FName key = ParseKey();
			switch(key)
			{
			case NAME_Heightfloor:
				sec->SetPlaneTexZ(sector_t::floor, CheckInt(key) << FRACBITS);
				continue;

			case NAME_Heightceiling:
				sec->SetPlaneTexZ(sector_t::ceiling, CheckInt(key) << FRACBITS);
				continue;

			case NAME_Texturefloor:
				SetTexture(sec, index, sector_t::floor, CheckString(key), missingTex);
				continue;

			case NAME_Textureceiling:
				SetTexture(sec, index, sector_t::ceiling, CheckString(key), missingTex);
				continue;

			case NAME_Lightlevel:
				sec->lightlevel = sector_t::ClampLight(CheckInt(key));
				continue;

			case NAME_Special:
				sec->special = (short)CheckInt(key);
				if (isTranslated) sec->special = P_TranslateSectorSpecial(sec->special);
				else if (namespc == NAME_Hexen)
				{
					if (sec->special < 0 || sec->special > 255 || !HexenSectorSpecialOk[sec->special])
						sec->special = 0;	// NULL all unknown specials
				}
				continue;

			case NAME_Id:
				sec->tag = (short)CheckInt(key);
				continue;

			default:
				break;
			}

			if (namespace_bits & (Zd|Zdt|Va)) switch(key)
			{
				case NAME_Xpanningfloor:
					sec->SetXOffset(sector_t::floor, CheckFixed(key));
					continue;

				case NAME_Ypanningfloor:
					sec->SetYOffset(sector_t::floor, CheckFixed(key));
					continue;

				case NAME_Xpanningceiling:
					sec->SetXOffset(sector_t::ceiling, CheckFixed(key));
					continue;

				case NAME_Ypanningceiling:
					sec->SetYOffset(sector_t::ceiling, CheckFixed(key));
					continue;

				case NAME_Xscalefloor:
					sec->SetXScale(sector_t::floor, CheckFixed(key));
					continue;

				case NAME_Yscalefloor:
					sec->SetYScale(sector_t::floor, CheckFixed(key));
					continue;

				case NAME_Xscaleceiling:
					sec->SetXScale(sector_t::ceiling, CheckFixed(key));
					continue;

				case NAME_Yscaleceiling:
					sec->SetYScale(sector_t::ceiling, CheckFixed(key));
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
					sec->SetAlpha(sector_t::floor, CheckFixed(key));
					continue;

				case NAME_Alphaceiling:
					sec->SetAlpha(sector_t::ceiling, CheckFixed(key));
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
					sec->gravity = float(CheckFloat(key));
					continue;

				case NAME_Lightcolor:
					lightcolor = CheckInt(key);
					continue;

				case NAME_Fadecolor:
					fadecolor = CheckInt(key);
					continue;

				case NAME_Desaturation:
					desaturation = int(255*CheckFloat(key));
					continue;

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
					Flag(sec->MoreFlags, SECF_HIDDEN, key);
					break;

				case NAME_Waterzone:
					Flag(sec->MoreFlags, SECF_UNDERWATER, key);
					break;

				default:
					break;
			}
				
			if (!strnicmp("user_", key.GetChars(), 5))
			{
				AddUserKey(key, UDMF_Sector, index);
			}
		}

		sec->secretsector = !!(sec->special&SECRET_MASK);
		sec->floorplane.d = -sec->GetPlaneTexZ(sector_t::floor);
		sec->floorplane.c = FRACUNIT;
		sec->floorplane.ic = FRACUNIT;
		sec->ceilingplane.d = sec->GetPlaneTexZ(sector_t::ceiling);
		sec->ceilingplane.c = -FRACUNIT;
		sec->ceilingplane.ic = -FRACUNIT;

		if (lightcolor == -1 && fadecolor == -1 && desaturation == -1)
		{
			// [RH] Sectors default to white light with the default fade.
			//		If they are outside (have a sky ceiling), they use the outside fog.
			if (level.outsidefog != 0xff000000 && (sec->GetTexture(sector_t::ceiling) == skyflatnum || (sec->special&0xff) == Sector_Outside))
			{
				if (fogMap == NULL)
					fogMap = GetSpecialLights (PalEntry (255,255,255), level.outsidefog, 0);
				sec->ColorMap = fogMap;
			}
			else
			{
				if (normMap == NULL)
					normMap = GetSpecialLights (PalEntry (255,255,255), level.fadeto, NormalLight.Desaturate);
				sec->ColorMap = normMap;
			}
		}
		else
		{
			if (lightcolor == -1) lightcolor = PalEntry(255,255,255);
			if (fadecolor == -1) 
			{
				if (level.outsidefog != 0xff000000 && (sec->GetTexture(sector_t::ceiling) == skyflatnum || (sec->special&0xff) == Sector_Outside))
					fadecolor = level.outsidefog;
				else
					fadecolor = level.fadeto;
			}
			if (desaturation == -1) desaturation = NormalLight.Desaturate;

			sec->ColorMap = GetSpecialLights (lightcolor, fadecolor, desaturation);
		}
	}

	//===========================================================================
	//
	// parse a vertex block
	//
	//===========================================================================

	void ParseVertex(vertex_t *vt, vertexdata_t *vd)
	{
		vt->x = vt->y = 0;
		vd->zCeiling = vd->zFloor = vd->flags = 0;
		sc.MustGetStringName("{");
		while (!sc.CheckString("}"))
		{
			sc.MustGetString();
			FName key = sc.String;
			sc.MustGetStringName("=");
			sc.MustGetString();
			FString value = sc.String;
			sc.MustGetStringName(";");
			switch(key)
			{
			case NAME_X:
				vt->x = FLOAT2FIXED(strtod(value, NULL));
				break;

			case NAME_Y:
				vt->y = FLOAT2FIXED(strtod(value, NULL));
				break;

			case NAME_ZCeiling:
				vd->zCeiling = FLOAT2FIXED(strtod(value, NULL));
				vd->flags |= VERTEXFLAG_ZCeilingEnabled;
				break;

			case NAME_ZFloor:
				vd->zFloor = FLOAT2FIXED(strtod(value, NULL));
				vd->flags |= VERTEXFLAG_ZFloorEnabled;
				break;

			default:
				break;
			}
		}
	}

	//===========================================================================
	//
	// Processes the linedefs after the map has been loaded
	//
	//===========================================================================

	void ProcessLineDefs()
	{
		int sidecount = 0;
		for(unsigned i = 0, skipped = 0; i < ParsedLines.Size();)
		{
			// Relocate the vertices
			intptr_t v1i = intptr_t(ParsedLines[i].v1);
			intptr_t v2i = intptr_t(ParsedLines[i].v2);

			if (v1i >= numvertexes || v2i >= numvertexes || v1i < 0 || v2i < 0)
			{
				I_Error ("Line %d has invalid vertices: %zd and/or %zd.\nThe map only contains %d vertices.", i+skipped, v1i, v2i, numvertexes);
			}
			else if (v1i == v2i ||
				(vertexes[v1i].x == vertexes[v2i].x && vertexes[v1i].y == vertexes[v2i].y))
			{
				Printf ("Removing 0-length line %d\n", i+skipped);
				ParsedLines.Delete(i);
				ForceNodeBuild = true;
				skipped++;
			}
			else
			{
				ParsedLines[i].v1 = &vertexes[v1i];
				ParsedLines[i].v2 = &vertexes[v2i];

				if (ParsedLines[i].sidedef[0] != NULL)
					sidecount++;
				if (ParsedLines[i].sidedef[1] != NULL)
					sidecount++;
				linemap.Push(i+skipped);
				i++;
			}
		}
		numlines = ParsedLines.Size();
		numsides = sidecount;
		lines = new line_t[numlines];
		sides = new side_t[numsides];
		int line, side;

		for(line = 0, side = 0; line < numlines; line++)
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
						sides[side] = ParsedSides[mapside];
						sides[side].linedef = &lines[line];
						sides[side].sector = &sectors[intptr_t(sides[side].sector)];
						lines[line].sidedef[sd] = &sides[side];

						P_ProcessSideTextures(!isExtended, &sides[side], sides[side].sector, &ParsedSideTextures[mapside],
							lines[line].special, lines[line].args[0], &tempalpha[sd], missingTex);

						side++;
					}
					else
					{
						lines[line].sidedef[sd] = NULL;
					}
				}
			}

			P_AdjustLine(&lines[line]);
			P_FinishLoadingLineDef(&lines[line], tempalpha[0]);
		}
		assert(side <= numsides);
		if (side < numsides)
		{
			Printf("Map had %d invalid side references\n", numsides - side);
			numsides = side;
		}
	}

	//===========================================================================
	//
	// Main parsing function
	//
	//===========================================================================

	void ParseTextMap(MapData *map)
	{
		char *buffer = new char[map->Size(ML_TEXTMAP)];

		isTranslated = true;
		isExtended = false;
		floordrop = false;

		map->Read(ML_TEXTMAP, buffer);
		sc.OpenMem(Wads.GetLumpFullName(map->lumpnum), buffer, map->Size(ML_TEXTMAP));
		delete [] buffer;
		sc.SetCMode(true);
		if (sc.CheckString("namespace"))
		{
			sc.MustGetStringName("=");
			sc.MustGetString();
			namespc = sc.String;
			switch(namespc)
			{
			case NAME_ZDoom:
				namespace_bits = Zd;
				isTranslated = false;
				break;
			case NAME_ZDoomTranslated:
				level.flags2 |= LEVEL2_DUMMYSWITCHES;
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
				P_LoadTranslator("xlat/doom_base.txt");
				level.flags2 |= LEVEL2_DUMMYSWITCHES;
				floordrop = true;
				break;
			case NAME_Heretic:
				namespace_bits = Ht;
				P_LoadTranslator("xlat/heretic_base.txt");
				level.flags2 |= LEVEL2_DUMMYSWITCHES;
				floordrop = true;
				break;
			case NAME_Strife:
				namespace_bits = St;
				P_LoadTranslator("xlat/strife_base.txt");
				level.flags2 |= LEVEL2_DUMMYSWITCHES|LEVEL2_RAILINGHACK;
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
					P_LoadTranslator("xlat/doom_base.txt");
					break;
				case GAME_Heretic:
					namespace_bits = Ht;
					P_LoadTranslator("xlat/heretic_base.txt");
					break;
				case GAME_Strife:
					namespace_bits = St;
					P_LoadTranslator("xlat/strife_base.txt");
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
				unsigned userdatastart = MapThingsUserData.Size();
				ParseThing(&th);
				MapThingsConverted.Push(th);
				if (userdatastart < MapThingsUserData.Size())
				{ // User data added
					MapThingsUserDataIndex[MapThingsConverted.Size()-1] = userdatastart;
					// Mark end of the user data for this map thing
					FMapThingUserData ud;
					ud.Property = NAME_None;
					ud.Value = 0;
					MapThingsUserData.Push(ud);
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
				mapsidedef_t st;
				ParseSidedef(&si, &st, ParsedSides.Size());
				ParsedSides.Push(si);
				ParsedSideTextures.Push(st);
			}
			else if (sc.Compare("sector"))
			{
				sector_t sec;
				ParseSector(&sec, ParsedSectors.Size());
				ParsedSectors.Push(sec);
			}
			else if (sc.Compare("vertex"))
			{
				vertex_t vt;
				vertexdata_t vd;
				ParseVertex(&vt, &vd);
				ParsedVertices.Push(vt);
				ParsedVertexDatas.Push(vd);
			}
			else
			{
				Skip();
			}
		}

		// Catch bogus maps here rather than during nodebuilding
		if (ParsedVertices.Size() == 0)	I_Error("Map has no vertices.\n");
		if (ParsedSectors.Size() == 0)	I_Error("Map has no sectors. \n");
		if (ParsedLines.Size() == 0)	I_Error("Map has no linedefs.\n");
		if (ParsedSides.Size() == 0)	I_Error("Map has no sidedefs.\n");

		// Create the real vertices
		numvertexes = ParsedVertices.Size();
		vertexes = new vertex_t[numvertexes];
		memcpy(vertexes, &ParsedVertices[0], numvertexes * sizeof(*vertexes));

		// Create the real vertex datas
		numvertexdatas = ParsedVertexDatas.Size();
		vertexdatas = new vertexdata_t[numvertexdatas];
		memcpy(vertexdatas, &ParsedVertexDatas[0], numvertexdatas * sizeof(*vertexdatas));

		// Create the real sectors
		numsectors = ParsedSectors.Size();
		sectors = new sector_t[numsectors];
		memcpy(sectors, &ParsedSectors[0], numsectors * sizeof(*sectors));
		sectors[0].e = new extsector_t[numsectors];
		for(int i = 0; i < numsectors; i++)
		{
			sectors[i].e = &sectors[0].e[i];
		}

		// Create the real linedefs and decompress the sidedefs
		ProcessLineDefs();
	}
};

void P_ParseTextMap(MapData *map, FMissingTextureTracker &missingtex)
{
	UDMFParser parse(missingtex);

	parse.ParseTextMap(map);
}

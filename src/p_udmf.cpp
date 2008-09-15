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

#include "r_data.h"
#include "p_setup.h"
#include "sc_man.h"
#include "p_lnspec.h"
#include "templates.h"
#include "i_system.h"
#include "gi.h"
#include "r_sky.h"
#include "g_level.h"
#include "v_palette.h"

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


enum
{
	Dm=1,
	Ht=2,
	Hx=4,
	St=8,
	Zd=16,
	Zdt=32,

	// will be extended later. Unknown namespaces will always be treated like the base
	// namespace for each game
};


void P_ProcessSideTextures(bool checktranmap, side_t *sd, sector_t *sec, mapsidedef_t *msd, int special, int tag, short *alpha);
void P_AdjustLine (line_t *ld);
void P_FinishLoadingLineDef(line_t *ld, int alpha);
void SpawnMapThing(int index, FMapThing *mt, int position);
extern bool		ForceNodeBuild;
extern TArray<FMapThing> MapThingsConverted;
extern TArray<int>		linemap;


#define CHECK_N(f) if (!(namespace_bits&(f))) break;

struct UDMFParser
{
	FScanner sc;
	FName namespc;
	int namespace_bits;
	bool isTranslated;
	bool isExtended;
	bool floordrop;

	TArray<line_t> ParsedLines;
	TArray<side_t> ParsedSides;
	TArray<mapsidedef_t> ParsedSideTextures;
	TArray<sector_t> ParsedSectors;
	TArray<vertex_t> ParsedVertices;

	FDynamicColormap	*fogMap, *normMap;

	UDMFParser()
	{
		linemap.Clear();
		fogMap = normMap = NULL;
	}

	//===========================================================================
	//
	// Parses a 'key = value' line of the map but doesn't read the semicolon
	//
	//===========================================================================

	FName ParseKey()
	{
		sc.MustGetString();
		FName key = sc.String;
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
		return key;
	}

	//===========================================================================
	//
	// Syntax checks
	//
	//===========================================================================

	int CheckInt(const char *key)
	{
		if (sc.TokenType != TK_IntConst)
		{
			sc.ScriptMessage("Integer value expected for key '%s'", key);
		}
		return sc.Number;
	}

	double CheckFloat(const char *key)
	{
		if (sc.TokenType != TK_IntConst && sc.TokenType != TK_FloatConst)
		{
			sc.ScriptMessage("Floating point value expected for key '%s'", key);
		}
		return sc.Float;
	}

	fixed_t CheckFixed(const char *key)
	{
		return FLOAT2FIXED(CheckFloat(key));
	}

	angle_t CheckAngle(const char *key)
	{
		return angle_t(CheckFloat(key) * ANGLE_90 / 90.);
	}

	bool CheckBool(const char *key)
	{
		if (sc.TokenType == TK_True) return true;
		if (sc.TokenType == TK_False) return false;
		sc.ScriptMessage("Boolean value expected for key '%s'", key);
		return false;
	}

	const char *CheckString(const char *key)
	{
		if (sc.TokenType != TK_StringConst)
		{
			sc.ScriptMessage("String value expected for key '%s'", key);
		}
		return sc.String;
	}

	void Flag(DWORD &value, int mask, const char *key)
	{
		if (CheckBool(key))	value |= mask;
		else value &= ~mask;
	}

	//===========================================================================
	//
	// Parse a thing block
	//
	//===========================================================================

	void ParseThing(FMapThing *th)
	{
		memset(th, 0, sizeof(*th));
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

			case NAME_Special:
				CHECK_N(Hx | Zd | Zdt)
				th->special = CheckInt(key);
				break;

			case NAME_Arg0:
			case NAME_Arg1:
			case NAME_Arg2:
			case NAME_Arg3:
			case NAME_Arg4:
				CHECK_N(Hx | Zd | Zdt)
				th->args[int(key)-int(NAME_Arg0)] = CheckInt(key);
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
				CHECK_N(Hx | Zd | Zdt)
				if (CheckBool(key)) th->ClassFilter |= (1<<(int(key)-NAME_Class1));
				else th->ClassFilter &= ~(1<<(int(key)-NAME_Class1));
				break;

			case NAME_Ambush:
				Flag(th->flags, MTF_AMBUSH, key); 
				break;

			case NAME_Dormant:
				CHECK_N(Hx | Zd | Zdt)
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
				CHECK_N(St | Zd | Zdt)
				Flag(th->flags, MTF_SHADOW, key); 
				break;

			case NAME_Invisible:
				CHECK_N(St | Zd | Zdt)
				Flag(th->flags, MTF_ALTSHADOW, key); 
				break;

			case NAME_Friend:	// This maps to Strife's friendly flag
				CHECK_N(Dm | Zd | Zdt)
				Flag(th->flags, MTF_FRIENDLY, key); 
				break;

			case NAME_Strifeally:
				CHECK_N(St | Zd | Zdt)
				Flag(th->flags, MTF_FRIENDLY, key); 
				break;

			case NAME_Standing:
				CHECK_N(St | Zd | Zdt)
				Flag(th->flags, MTF_STANDSTILL, key); 
				break;

			default:
				break;
			}
			sc.MustGetToken(';');
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

	void ParseLinedef(line_t *ld)
	{
		bool passuse = false;
		bool strifetrans = false;

		memset(ld, 0, sizeof(*ld));
		ld->Alpha = FRACUNIT;
		ld->id = -1;
		ld->sidenum[0] = ld->sidenum[1] = NO_SIDE;
		if (level.flags & LEVEL_CLIPMIDTEX) ld->flags |= ML_CLIP_MIDTEX;
		if (level.flags & LEVEL_WRAPMIDTEX) ld->flags |= ML_WRAP_MIDTEX;
		if (level.flags & LEVEL_CHECKSWITCHRANGE) ld->flags |= ML_CHECKSWITCHRANGE;

		sc.MustGetToken('{');
		while (!sc.CheckToken('}'))
		{
			FName key = ParseKey();

			// This switch contains all keys of the UDMF base spec
			switch(key)
			{
			case NAME_V1:
				ld->v1 = (vertex_t*)(intptr_t)CheckInt(key);	// must be relocated later
				break;

			case NAME_V2:
				ld->v2 = (vertex_t*)(intptr_t)CheckInt(key);	// must be relocated later
				break;

			case NAME_Special:
				ld->special = CheckInt(key);
				if (namespc == NAME_Hexen)
				{
					if (ld->special < 0 || ld->special > 140 || !HexenLineSpecialOk[ld->special])
						ld->special = 0;	// NULL all specials which don't exist in Hexen
				}

				break;

			case NAME_Id:
				ld->id = CheckInt(key);
				break;

			case NAME_Sidefront:
				ld->sidenum[0] = CheckInt(key);
				break;

			case NAME_Sideback:
				ld->sidenum[1] = CheckInt(key);
				break;

			case NAME_Arg0:
			case NAME_Arg1:
			case NAME_Arg2:
			case NAME_Arg3:
			case NAME_Arg4:
				ld->args[int(key)-int(NAME_Arg0)] = CheckInt(key);
				break;

			case NAME_Blocking:
				Flag(ld->flags, ML_BLOCKING, key); 
				break;

			case NAME_Blockmonsters:
				Flag(ld->flags, ML_BLOCKMONSTERS, key); 
				break;

			case NAME_Twosided:
				Flag(ld->flags, ML_TWOSIDED, key); 
				break;

			case NAME_Dontpegtop:
				Flag(ld->flags, ML_DONTPEGTOP, key); 
				break;

			case NAME_Dontpegbottom:
				Flag(ld->flags, ML_DONTPEGBOTTOM, key); 
				break;

			case NAME_Secret:
				Flag(ld->flags, ML_SECRET, key); 
				break;

			case NAME_Blocksound:
				Flag(ld->flags, ML_SOUNDBLOCK, key); 
				break;

			case NAME_Dontdraw:
				Flag(ld->flags, ML_DONTDRAW, key); 
				break;

			case NAME_Mapped:
				Flag(ld->flags, ML_MAPPED, key); 
				break;

			case NAME_Jumpover:
				CHECK_N(St | Zd | Zdt)
				Flag(ld->flags, ML_RAILING, key); 
				break;

			case NAME_Blockfloating:
				CHECK_N(St | Zd | Zdt)
				Flag(ld->flags, ML_BLOCK_FLOATERS, key); 
				break;

			case NAME_Transparent:	
				CHECK_N(St | Zd | Zdt)
				strifetrans = CheckBool(key); 
				break;

			case NAME_Passuse:
				CHECK_N(Dm | Zd | Zdt)
				passuse = CheckBool(key); 
				break;

			default:
				break;
			}

			// This switch contains all keys of the UDMF base spec which only apply to Hexen format specials
			if (!isTranslated) switch (key)
			{
			case NAME_Playercross:
				Flag(ld->activation, SPAC_Cross, key); 
				break;

			case NAME_Playeruse:
				Flag(ld->activation, SPAC_Use, key); 
				break;

			case NAME_Monstercross:
				Flag(ld->activation, SPAC_MCross, key); 
				break;

			case NAME_Impact:
				Flag(ld->activation, SPAC_Impact, key); 
				break;

			case NAME_Playerpush:
				Flag(ld->activation, SPAC_Push, key); 
				break;

			case NAME_Missilecross:
				Flag(ld->activation, SPAC_PCross, key); 
				break;

			case NAME_Monsteruse:
				Flag(ld->activation, SPAC_MUse, key); 
				break;

			case NAME_Monsterpush:
				Flag(ld->activation, SPAC_MPush, key); 
				break;

			case NAME_Repeatspecial:
				Flag(ld->flags, ML_REPEAT_SPECIAL, key); 
				break;

			default:
				break;
			}

			// This switch contains all keys which are ZDoom specific
			if (namespace_bits & (Zd|Zdt)) switch(key)
			{
			case NAME_Alpha:
				ld->Alpha = CheckFixed(key);
				break;

			case NAME_Renderstyle:
			{
				const char *str = CheckString(key);
				if (!stricmp(str, "translucent")) ld->flags &= ~ML_ADDTRANS;
				else if (!stricmp(str, "add")) ld->flags |= ML_ADDTRANS;
				else sc.ScriptMessage("Unknown value \"%s\" for 'renderstyle'\n", str);
				break;
			}

			case NAME_Anycross:
				Flag(ld->activation, SPAC_AnyCross, key); 
				break;

			case NAME_Monsteractivate:
				Flag(ld->flags, ML_MONSTERSCANACTIVATE, key); 
				break;

			case NAME_Blockplayers:
				Flag(ld->flags, ML_BLOCK_PLAYERS, key); 
				break;

			case NAME_Blockeverything:
				Flag(ld->flags, ML_BLOCKEVERYTHING, key); 
				break;

			case NAME_Zoneboundary:
				Flag(ld->flags, ML_ZONEBOUNDARY, key); 
				break;

			case NAME_Clipmidtex:
				Flag(ld->flags, ML_CLIP_MIDTEX, key); 
				break;

			case NAME_Wrapmidtex:
				Flag(ld->flags, ML_WRAP_MIDTEX, key); 
				break;

			case NAME_Midtex3d:
				Flag(ld->flags, ML_3DMIDTEX, key); 
				break;

			case NAME_Checkswitchrange:
				Flag(ld->flags, ML_CHECKSWITCHRANGE, key); 
				break;

			case NAME_Firstsideonly:
				Flag(ld->flags, ML_FIRSTSIDEONLY, key); 
				break;

			default:
				break;
			}
			sc.MustGetToken(';');
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
	}

	//===========================================================================
	//
	// Parse a sidedef block
	//
	//===========================================================================

	void ParseSidedef(side_t *sd, mapsidedef_t *sdt)
	{
		fixed_t texofs[2]={0,0};

		memset(sd, 0, sizeof(*sd));
		strncpy(sdt->bottomtexture, "-", 8);
		strncpy(sdt->toptexture, "-", 8);
		strncpy(sdt->midtexture, "-", 8);

		sc.MustGetToken('{');
		while (!sc.CheckToken('}'))
		{
			FName key = ParseKey();
			switch(key)
			{
			case NAME_Offsetx:
				texofs[0] = CheckInt(key) << FRACBITS;
				break;

			case NAME_Offsety:
				texofs[1] = CheckInt(key) << FRACBITS;
				break;

			case NAME_Texturetop:
				strncpy(sdt->toptexture, CheckString(key), 8);
				break;

			case NAME_Texturebottom:
				strncpy(sdt->bottomtexture, CheckString(key), 8);
				break;

			case NAME_Texturemiddle:
				strncpy(sdt->midtexture, CheckString(key), 8);
				break;

			case NAME_Sector:
				sd->sector = (sector_t*)(intptr_t)CheckInt(key);
				break;

			default:
				break;
			}

			if (namespace_bits & (Zd|Zdt)) switch(key)
			{
			case NAME_offsetx_top:
				sd->SetTextureXOffset(side_t::top, CheckFixed(key));
				break;

			case NAME_offsety_top:
				sd->SetTextureYOffset(side_t::top, CheckFixed(key));
				break;

			case NAME_offsetx_mid:
				sd->SetTextureXOffset(side_t::mid, CheckFixed(key));
				break;

			case NAME_offsety_mid:
				sd->SetTextureYOffset(side_t::mid, CheckFixed(key));
				break;

			case NAME_offsetx_bottom:
				sd->SetTextureXOffset(side_t::bottom, CheckFixed(key));
				break;

			case NAME_offsety_bottom:
				sd->SetTextureYOffset(side_t::bottom, CheckFixed(key));
				break;

			case NAME_light:
				sd->SetLight(CheckInt(key));
				break;

			case NAME_lightabsolute:
				if (CheckBool(key)) sd->Flags |= WALLF_ABSLIGHTING;
				else sd->Flags &= ~WALLF_ABSLIGHTING;
				break;

			case NAME_nofakecontrast:
				if (CheckBool(key)) sd->Flags |= WALLF_NOFAKECONTRAST;
				else sd->Flags &= WALLF_NOFAKECONTRAST;
				break;

			case NAME_smoothlighting:
				if (CheckBool(key)) sd->Flags |= WALLF_SMOOTHLIGHTING;
				else sd->Flags &= ~WALLF_SMOOTHLIGHTING;
				break;

			default:
				break;

			}

			sc.MustGetToken(';');
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

	void ParseSector(sector_t *sec)
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
		sec->oldspecial = !!(sec->special&SECRET_MASK);
		sec->thinglist = NULL;
		sec->touching_thinglist = NULL;		// phares 3/14/98
		sec->seqType = (level.flags & LEVEL_SNDSEQTOTALCTRL)? 0:-1;
		sec->nextsec = -1;	//jff 2/26/98 add fields to support locking out
		sec->prevsec = -1;	// stair retriggering until build completes
		sec->heightsec = NULL;	// sector used to get floor and ceiling height
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
				break;

			case NAME_Heightceiling:
				sec->SetPlaneTexZ(sector_t::ceiling, CheckInt(key) << FRACBITS);
				break;

			case NAME_Texturefloor:
				sec->SetTexture(sector_t::floor, TexMan.GetTexture (CheckString(key), FTexture::TEX_Flat, FTextureManager::TEXMAN_Overridable));
				break;

			case NAME_Textureceiling:
				sec->SetTexture(sector_t::ceiling, TexMan.GetTexture (CheckString(key), FTexture::TEX_Flat, FTextureManager::TEXMAN_Overridable));
				break;

			case NAME_Lightlevel:
				sec->lightlevel = (BYTE)clamp<int>(CheckInt(key), 0, 255);
				break;

			case NAME_Special:
				sec->special = (short)CheckInt(key);
				if (isTranslated) sec->special = P_TranslateSectorSpecial(sec->special);
				else if (namespc == NAME_Hexen)
				{
					if (sec->special < 0 || sec->special > 255 || !HexenSectorSpecialOk[sec->special])
						sec->special = 0;	// NULL all unknown specials
				}
				break;

			case NAME_Id:
				sec->tag = (short)CheckInt(key);
				break;

			default:
				break;
			}

			if (namespace_bits & (Zd|Zdt)) switch(key)
			{
				case NAME_Xpanningfloor:
					sec->SetXOffset(sector_t::floor, CheckFixed(key));
					break;

				case NAME_Ypanningfloor:
					sec->SetYOffset(sector_t::floor, CheckFixed(key));
					break;

				case NAME_Xpanningceiling:
					sec->SetXOffset(sector_t::ceiling, CheckFixed(key));
					break;

				case NAME_Ypanningceiling:
					sec->SetYOffset(sector_t::ceiling, CheckFixed(key));
					break;

				case NAME_Xscalefloor:
					sec->SetXScale(sector_t::floor, CheckFixed(key));
					break;

				case NAME_Yscalefloor:
					sec->SetYScale(sector_t::floor, CheckFixed(key));
					break;

				case NAME_Xscaleceiling:
					sec->SetXScale(sector_t::ceiling, CheckFixed(key));
					break;

				case NAME_Yscaleceiling:
					sec->SetYScale(sector_t::ceiling, CheckFixed(key));
					break;

				case NAME_Rotationfloor:
					sec->SetAngle(sector_t::floor, CheckAngle(key));
					break;

				case NAME_Rotationceiling:
					sec->SetAngle(sector_t::ceiling, CheckAngle(key));
					break;

				case NAME_Lightfloor:
					sec->SetPlaneLight(sector_t::floor, CheckInt(key));
					break;

				case NAME_Lightceiling:
					sec->SetPlaneLight(sector_t::ceiling, CheckInt(key));
					break;

				case NAME_Lightfloorabsolute:
					if (CheckBool(key)) sec->ChangeFlags(sector_t::floor, 0, SECF_ABSLIGHTING);
					else sec->ChangeFlags(sector_t::floor, SECF_ABSLIGHTING, 0);
					break;

				case NAME_Lightceilingabsolute:
					if (CheckBool(key)) sec->ChangeFlags(sector_t::ceiling, 0, SECF_ABSLIGHTING);
					else sec->ChangeFlags(sector_t::ceiling, SECF_ABSLIGHTING, 0);
					break;

				case NAME_Gravity:
					sec->gravity = float(CheckFloat(key));
					break;

				case NAME_Lightcolor:
					lightcolor = CheckInt(key);
					break;

				case NAME_Fadecolor:
					fadecolor = CheckInt(key);
					break;

				case NAME_Desaturation:
					desaturation = int(255*CheckFloat(key));
					break;

				case NAME_Silent:
					Flag(sec->Flags, SECF_SILENT, key);
					break;

				case NAME_Nofallingdamage:
					Flag(sec->Flags, SECF_NOFALLINGDAMAGE, key);
					break;

				case NAME_Dropactors:
					Flag(sec->Flags, SECF_FLOORDROP, key);
					break;

				default:
					break;
			}
				
			sc.MustGetToken(';');
		}

		sec->floorplane.d = -sec->GetPlaneTexZ(sector_t::floor);
		sec->floorplane.c = FRACUNIT;
		sec->floorplane.ic = FRACUNIT;
		sec->ceilingplane.d = sec->GetPlaneTexZ(sector_t::ceiling);
		sec->ceilingplane.c = -FRACUNIT;
		sec->ceilingplane.ic = -FRACUNIT;

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

	//===========================================================================
	//
	// parse a vertex block
	//
	//===========================================================================

	void ParseVertex(vertex_t *vt)
	{
		vt->x = vt->y = 0;
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

				if (ParsedLines[i].sidenum[0] != NO_SIDE)
					sidecount++;
				if (ParsedLines[i].sidenum[1] != NO_SIDE)
					sidecount++;
				linemap.Push(i+skipped);
				i++;
			}
		}
		numlines = ParsedLines.Size();
		numsides = sidecount;
		lines = new line_t[numlines];
		sides = new side_t[numsides];

		for(int line = 0, side = 0; line < numlines; line++)
		{
			short tempalpha[2] = {-1,-1};

			lines[line] = ParsedLines[line];

			for(int sd = 0; sd < 2; sd++)
			{
				if (lines[line].sidenum[sd] != NO_SIDE)
				{
					int mapside = lines[line].sidenum[sd];
					sides[side] = ParsedSides[mapside];
					sides[side].linenum = line;
					sides[side].sector = &sectors[intptr_t(sides[side].sector)];
					lines[line].sidenum[sd] = side;

					P_ProcessSideTextures(!isExtended, &sides[side], sides[side].sector, &ParsedSideTextures[mapside],
						lines[line].special, lines[line].args[0], &tempalpha[sd]);

					side++;
				}
			}

			P_AdjustLine(&lines[line]);
			P_FinishLoadingLineDef(&lines[line], tempalpha[0]);
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

		map->Read(ML_TEXTMAP, buffer);
		sc.OpenMem(Wads.GetLumpFullName(map->lumpnum), buffer, map->Size(ML_TEXTMAP));
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
				namespace_bits = Zdt;
				break;
			case NAME_Hexen:
				namespace_bits = Hx;
				isTranslated = false;
				break;
			case NAME_Doom:
				namespace_bits = Dm;
				P_LoadTranslator("xlat/doom_base.txt");
				level.flags |= LEVEL_DUMMYSWITCHES;
				floordrop = true;
				break;
			case NAME_Heretic:
				namespace_bits = Ht;
				P_LoadTranslator("xlat/heretic_base.txt");
				level.flags |= LEVEL_DUMMYSWITCHES;
				floordrop = true;
				break;
			case NAME_Strife:
				namespace_bits = St;
				P_LoadTranslator("xlat/strife_base.txt");
				level.flags |= LEVEL_DUMMYSWITCHES|LEVEL_RAILINGHACK;
				floordrop = true;
				break;
			default:
				Printf("Unknown namespace %s. Using defaults for %s\n", sc.String, GameNames[gameinfo.gametype]);
				switch (gameinfo.gametype)
				{
				default:			// Shh, GCC
				case GAME_Doom:
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
				ParseThing(&th);
				MapThingsConverted.Push(th);
			}
			else if (sc.Compare("linedef"))
			{
				line_t li;
				ParseLinedef(&li);
				ParsedLines.Push(li);
			}
			else if (sc.Compare("sidedef"))
			{
				side_t si;
				mapsidedef_t st;
				ParseSidedef(&si, &st);
				ParsedSides.Push(si);
				ParsedSideTextures.Push(st);
			}
			else if (sc.Compare("sector"))
			{
				sector_t sec;
				ParseSector(&sec);
				ParsedSectors.Push(sec);
			}
			else if (sc.Compare("vertex"))
			{
				vertex_t vt;
				ParseVertex(&vt);
				ParsedVertices.Push(vt);
			}
		}

		// Create the real vertices
		numvertexes = ParsedVertices.Size();
		vertexes = new vertex_t[numvertexes];
		memcpy(vertexes, &ParsedVertices[0], numvertexes * sizeof(*vertexes));

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

void P_ParseTextMap(MapData *map)
{
	UDMFParser parse;

	parse.ParseTextMap(map);
}

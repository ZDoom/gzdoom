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
#include "vectors.h"
#include "p_lnspec.h"
#include "templates.h"
#include "i_system.h"

extern void P_TranslateLineDef (line_t *ld, maplinedef_t *mld);
extern int	P_TranslateSectorSpecial (int);
void P_ProcessSideTextures(bool checktranmap, side_t *sd, sector_t *sec, mapsidedef_t *msd, int special, int tag, short *alpha);
void P_AdjustLine (line_t *ld);
void P_FinishLoadingLineDef(line_t *ld, int alpha);
void SpawnMapThing(int index, FMapThing *mt, int position);
extern bool		ForceNodeBuild;
extern TArray<FMapThing> MapThingsConverted;
extern TArray<int>		linemap;

struct UDMFParser
{
	FScanner sc;
	FName namespc;
	bool isTranslated;
	bool isExtended;

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

	void Flag(DWORD &value, int mask, const FString &svalue)
	{
		if (!svalue.CompareNoCase("true"))
		{
			value |= mask;
		}
		else
		{
			value &= ~mask;
		}
	}

	void ParseThing(FMapThing *th)
	{
		memset(th, 0, sizeof(*th));
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
			case NAME_TID:
				th->thingid = (WORD)strtol(value, NULL, 0);
				break;
			case NAME_X:
				th->x = FLOAT2FIXED(strtod(value, NULL));
				break;
			case NAME_Y:
				th->y = FLOAT2FIXED(strtod(value, NULL));
				break;
			case NAME_Height:
				th->z = FLOAT2FIXED(strtod(value, NULL));
				break;
			case NAME_Angle:
				th->angle = (short)strtol(value, NULL, 0);
				break;
			case NAME_Type:
				th->type = (short)strtol(value, NULL, 0);
				break;
			case NAME_Special:
				th->special = (short)strtol(value, NULL, 0);
				break;
			case NAME_Arg0:
			case NAME_Arg1:
			case NAME_Arg2:
			case NAME_Arg3:
			case NAME_Arg4:
				th->args[int(key)-int(NAME_Arg0)] = strtol(value, NULL, 0);
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
				if (!value.CompareNoCase("true")) th->SkillFilter |= (1<<(int(key)-NAME_Skill1));
				else th->SkillFilter &= ~(1<<(int(key)-NAME_Skill1));
				break;

			case NAME_Class0:
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
				if (!value.CompareNoCase("true")) th->ClassFilter |= (1<<(int(key)-NAME_Class1));
				else th->SkillFilter &= ~(1<<(int(key)-NAME_Class1));
				break;

			case NAME_Ambush:
				Flag(th->flags, MTF_AMBUSH, value); break;
			case NAME_Dormant:
				Flag(th->flags, MTF_DORMANT, value); break;
			case NAME_Single:
				Flag(th->flags, MTF_SINGLE, value); break;
			case NAME_Coop:
				Flag(th->flags, MTF_COOPERATIVE, value); break;
			case NAME_Dm:
				Flag(th->flags, MTF_DEATHMATCH, value); break;
			case NAME_Translucent:
				Flag(th->flags, MTF_SHADOW, value); break;
			case NAME_Invisible:
				Flag(th->flags, MTF_ALTSHADOW, value); break;
			case NAME_Friend:
			case NAME_Strifeally:
				Flag(th->flags, MTF_FRIENDLY, value); break;
			case NAME_Standing:
				Flag(th->flags, MTF_STANDSTILL, value); break;

			default:
				break;
			}
		}
		if (isTranslated)
		{
			// NOTE: Handling of this is undefined in the UDMF spec yet!
			maplinedef_t mld;
			line_t ld;

			mld.flags = 0;
			mld.special = th->special;
			mld.tag = th->args[0];
			P_TranslateLineDef(&ld, &mld);
			th->special = ld.special;
			memcpy(th->args, ld.args, sizeof (ld.args));
		}
	}

	void ParseLinedef(line_t *ld)
	{
		bool passuse = false;

		memset(ld, 0, sizeof(*ld));
		ld->Alpha = FRACUNIT;
		ld->id = -1;
		ld->sidenum[0] = ld->sidenum[1] = NO_SIDE;
		if (level.flags & LEVEL_CLIPMIDTEX) ld->flags |= ML_CLIP_MIDTEX;
		if (level.flags & LEVEL_WRAPMIDTEX) ld->flags |= ML_WRAP_MIDTEX;
		if (level.flags & LEVEL_CHECKSWITCHRANGE) ld->flags |= ML_CHECKSWITCHRANGE;
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
			case NAME_V1:
				ld->v1 = (vertex_t*)(intptr_t)strtol(value, NULL, 0);	// must be relocated later
				break;
			case NAME_V2:
				ld->v2 = (vertex_t*)(intptr_t)strtol(value, NULL, 0);	// must be relocated later
				break;
			case NAME_Special:
				ld->special = strtol(value, NULL, 0);
				break;
			case NAME_Id:
				ld->id = strtol(value, NULL, 0);
				break;
			case NAME_Sidefront:
				ld->sidenum[0] = strtol(value, NULL, 0);
				break;
			case NAME_Sideback:
				ld->sidenum[1] = strtol(value, NULL, 0);
				break;
			case NAME_Arg0:
			case NAME_Arg1:
			case NAME_Arg2:
			case NAME_Arg3:
			case NAME_Arg4:
				ld->args[int(key)-int(NAME_Arg0)] = strtol(value, NULL, 0);
				break;
			case NAME_Blocking:
				Flag(ld->flags, ML_BLOCKING, value); break;
			case NAME_Blockmonsters:
				Flag(ld->flags, ML_BLOCKMONSTERS, value); break;
			case NAME_Twosided:
				Flag(ld->flags, ML_TWOSIDED, value); break;
			case NAME_Dontpegtop:
				Flag(ld->flags, ML_DONTPEGTOP, value); break;
			case NAME_Dontpegbottom:
				Flag(ld->flags, ML_DONTPEGBOTTOM, value); break;
			case NAME_Secret:
				Flag(ld->flags, ML_SECRET, value); break;
			case NAME_Soundblock:
				Flag(ld->flags, ML_SOUNDBLOCK, value); break;
			case NAME_Dontdraw:
				Flag(ld->flags, ML_DONTDRAW, value); break;
			case NAME_Mapped:
				Flag(ld->flags, ML_MAPPED, value); break;
			case NAME_Monsteractivate:
				Flag(ld->flags, ML_MONSTERSCANACTIVATE, value); break;
			case NAME_Blockplayers:
				if (isExtended) Flag(ld->flags, ML_BLOCK_PLAYERS, value); break;
			case NAME_Blockeverything:
				if (isExtended) Flag(ld->flags, ML_BLOCKEVERYTHING, value); break;
			case NAME_Zoneboundary:
				if (isExtended) Flag(ld->flags, ML_ZONEBOUNDARY, value); break;
			case NAME_Jumpover:
				if (isExtended || namespc == NAME_Strife) Flag(ld->flags, ML_RAILING, value); break;
			case NAME_Blockfloating:
				if (isExtended || namespc == NAME_Strife) Flag(ld->flags, ML_BLOCK_FLOATERS, value); break;
			case NAME_Clipmidtex:
				if (isExtended) Flag(ld->flags, ML_CLIP_MIDTEX, value); break;
			case NAME_Wrapmidtex:
				if (isExtended) Flag(ld->flags, ML_WRAP_MIDTEX, value); break;
			case NAME_Midtex3d:
				if (isExtended) Flag(ld->flags, ML_3DMIDTEX, value); break;
			case NAME_Checkswitchrange:
				if (isExtended) Flag(ld->flags, ML_CHECKSWITCHRANGE, value); break;
			case NAME_Firstsideonly:
				if (isExtended) Flag(ld->flags, ML_FIRSTSIDEONLY, value); break;
			case NAME_Transparent:	
				ld->Alpha = !value.CompareNoCase("true")? FRACUNIT*3/4 : FRACUNIT; break;
			case NAME_Passuse:
				passuse = !value.CompareNoCase("true");
			case NAME_Playercross:
				if (isExtended) Flag(ld->activation, SPAC_Cross, value); break;
			case NAME_Playeruse:
				if (isExtended) Flag(ld->activation, SPAC_Use, value); break;
			case NAME_Monstercross:
				if (isExtended) Flag(ld->activation, SPAC_MCross, value); break;
			case NAME_Impact:
				if (isExtended) Flag(ld->activation, SPAC_Impact, value); break;
			case NAME_Playerpush:
				if (isExtended) Flag(ld->activation, SPAC_Push, value); break;
			case NAME_Missilecross:
				if (isExtended) Flag(ld->activation, SPAC_PCross, value); break;
			case NAME_Monsteruse:
				if (isExtended) Flag(ld->activation, SPAC_MUse, value); break;
			case NAME_Monsterpush:
				if (isExtended) Flag(ld->activation, SPAC_MPush, value); break;

			default:
				break;
			}
		}
		if (isTranslated)
		{
		}
		else
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
	}

	void ParseSidedef(side_t *sd, mapsidedef_t *sdt)
	{
		fixed_t texofs[2]={0,0};

		memset(sd, 0, sizeof(*sd));
		strncpy(sdt->bottomtexture, "-", 8);
		strncpy(sdt->toptexture, "-", 8);
		strncpy(sdt->midtexture, "-", 8);
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
			case NAME_Offsetx:
				texofs[0] = strtol(value, NULL, 0) << FRACBITS;
				break;

			case NAME_Offsety:
				texofs[1] = strtol(value, NULL, 0) << FRACBITS;
				break;

			case NAME_Texturetop:
				strncpy(sdt->toptexture, value, 8);
				break;

			case NAME_Texturebottom:
				strncpy(sdt->bottomtexture, value, 8);
				break;

			case NAME_Texturemiddle:
				strncpy(sdt->midtexture, value, 8);
				break;

			case NAME_Sector:
				sd->sector = (sector_t*)(intptr_t)strtol(value, NULL, 0);
				break;

			default:
				break;
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

	void ParseSector(sector_t *sec)
	{
		memset(sec, 0, sizeof(*sec));
		sec->lightlevel = 255;
		sec->floor_xscale = FRACUNIT;	// [RH] floor and ceiling scaling
		sec->floor_yscale = FRACUNIT;
		sec->ceiling_xscale = FRACUNIT;
		sec->ceiling_yscale = FRACUNIT;
		sec->oldspecial = !!(sec->special&SECRET_MASK);
		sec->thinglist = NULL;
		sec->touching_thinglist = NULL;		// phares 3/14/98
		sec->seqType = (level.flags & LEVEL_SNDSEQTOTALCTRL)? 0:-1;
		sec->nextsec = -1;	//jff 2/26/98 add fields to support locking out
		sec->prevsec = -1;	// stair retriggering until build completes
		sec->heightsec = NULL;	// sector used to get floor and ceiling height
		// killough 3/7/98: end changes

		sec->gravity = 1.f;	// [RH] Default sector gravity of 1.0
		sec->ZoneNumber = 0xFFFF;

		// killough 8/28/98: initialize all sectors to normal friction
		sec->friction = ORIG_FRICTION;
		sec->movefactor = ORIG_FRICTION_FACTOR;

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
			case NAME_Heightfloor:
				sec->floortexz = strtol(value, NULL, 0) << FRACBITS;
				break;

			case NAME_Heightceiling:
				sec->ceilingtexz = strtol(value, NULL, 0) << FRACBITS;
				break;

			case NAME_Texturefloor:
				sec->floorpic = TexMan.GetTexture (value, FTexture::TEX_Flat, FTextureManager::TEXMAN_Overridable);
				break;

			case NAME_Textureceiling:
				sec->ceilingpic = TexMan.GetTexture (value, FTexture::TEX_Flat, FTextureManager::TEXMAN_Overridable);
				break;

			case NAME_Lightlevel:
				sec->lightlevel = (BYTE)clamp<int>(strtol(value, NULL, 0), 0, 255);
				break;

			case NAME_Special:
				sec->special = (short)strtol(value, NULL, 0);
				if (isTranslated) sec->special = P_TranslateSectorSpecial(sec->special);
				break;

			case NAME_Tag:
				sec->tag = (short)strtol(value, NULL, 0);
				break;

			default:
				break;
			}
		}

		sec->floorplane.d = -sec->floortexz;
		sec->floorplane.c = FRACUNIT;
		sec->floorplane.ic = FRACUNIT;
		sec->ceilingplane.d = sec->ceilingtexz;
		sec->ceilingplane.c = -FRACUNIT;
		sec->ceilingplane.ic = -FRACUNIT;

		// [RH] Sectors default to white light with the default fade.
		//		If they are outside (have a sky ceiling), they use the outside fog.
		if (level.outsidefog != 0xff000000 && (sec->ceilingpic == skyflatnum || (sec->special&0xff) == Sector_Outside))
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
				I_Error ("Line %d has invalid vertices: %d and/or %d.\nThe map only contains %d vertices.", i+skipped, v1i, v2i, numvertexes);
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

	void ParseTextMap(MapData *map)
	{
		char *buffer = new char[map->Size(ML_TEXTMAP)];

		isTranslated = true;
		isExtended = false;

		map->Read(ML_TEXTMAP, buffer);
		sc.OpenMem(Wads.GetLumpFullName(map->lumpnum), buffer, map->Size(ML_TEXTMAP));
		sc.SetCMode(true);
		while (sc.GetString())
		{
			if (sc.Compare("namespace"))
			{
				sc.MustGetStringName("=");
				sc.MustGetString();
				namespc = sc.String;
				if (namespc == NAME_ZDoom)
				{
					isTranslated = false;
					isExtended = true;
				}
				else if (namespc == NAME_Hexen)
				{
					isTranslated = false;
				}
				else if (namespc == NAME_ZDoomTranslated)
				{
					isExtended = true;
				}
				else if (namespc == NAME_Doom)
				{
				}
				else if (namespc == NAME_Heretic)
				{
				}
				else if (namespc == NAME_Strife)
				{
				}
				else 
				{
					sc.ScriptError("Unknown namespace %s", sc.String);
				}
				sc.MustGetStringName(";");
			}
			else if (sc.Compare("thing"))
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
		sectors[0].e = new extsector_t[numsectors];
		memcpy(sectors, &ParsedSectors[0], numsectors * sizeof(*sectors));
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

void P_SpawnTextThings(int position)
{
	for(unsigned i=0; i<MapThingsConverted.Size(); i++)
	{
		SpawnMapThing (i, &MapThingsConverted[i], position);
	}
}

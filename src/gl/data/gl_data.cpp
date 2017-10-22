// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2005-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** gl_data.cpp
** Maintenance data for GL renderer (mostly to handle rendering hacks)
**
**/

#include "gl/system/gl_system.h"

#include "doomtype.h"
#include "colormatcher.h"
#include "i_system.h"
#include "p_local.h"
#include "p_tags.h"
#include "p_lnspec.h"
#include "c_dispatch.h"
#include "r_sky.h"
#include "sc_man.h"
#include "w_wad.h"
#include "gi.h"
#include "g_level.h"
#include "doomstat.h"
#include "d_player.h"
#include "g_levellocals.h"

#include "gl/system/gl_interface.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/data/gl_data.h"
#include "gl/dynlights/gl_dynlight.h"
#include "gl/dynlights/gl_glow.h"
#include "gl/models/gl_models.h"
#include "gl/utility/gl_clock.h"
#include "gl/shaders/gl_shader.h"
#include "gl/gl_functions.h"

GLRenderSettings glset;
long gl_frameMS;

EXTERN_CVAR(Int, gl_lightmode)
EXTERN_CVAR(Bool, gl_brightfog)
EXTERN_CVAR(Bool, gl_lightadditivesurfaces)


CUSTOM_CVAR(Bool, gl_notexturefill, false, 0)
{
	glset.notexturefill = self;
}


void gl_CreateSections();
void AddAutoBrightmaps();

//-----------------------------------------------------------------------------
//
// Adjust sprite offsets for GL rendering (IWAD resources only)
//
//-----------------------------------------------------------------------------

void AdjustSpriteOffsets()
{
	int lump, lastlump = 0;
	int sprid;
	TMap<int, bool> donotprocess;

	int numtex = Wads.GetNumLumps();

	for (int i = 0; i < numtex; i++)
	{
		if (Wads.GetLumpFile(i) > Wads.GetIwadNum()) break; // we are past the IWAD
		if (Wads.GetLumpNamespace(i) == ns_sprites && Wads.GetLumpFile(i) == Wads.GetIwadNum())
		{
			char str[9];
			Wads.GetLumpName(str, i);
			str[8] = 0;
			FTextureID texid = TexMan.CheckForTexture(str, FTexture::TEX_Sprite, 0);
			if (texid.isValid() && Wads.GetLumpFile(TexMan[texid]->SourceLump) > Wads.GetIwadNum())
			{
				// This texture has been replaced by some PWAD.
				memcpy(&sprid, str, 4);
				donotprocess[sprid] = true;
			}
		}
	}

	while ((lump = Wads.FindLump("SPROFS", &lastlump, false)) != -1)
	{
		FScanner sc;
		sc.OpenLumpNum(lump);
		sc.SetCMode(true);
		GLRenderer->FlushTextures();
		int ofslumpno = Wads.GetLumpFile(lump);
		while (sc.GetString())
		{
			int x,y;
			bool iwadonly = false;
			bool forced = false;
			FTextureID texno = TexMan.CheckForTexture(sc.String, FTexture::TEX_Sprite);
			sc.MustGetStringName(",");
			sc.MustGetNumber();
			x=sc.Number;
			sc.MustGetStringName(",");
			sc.MustGetNumber();
			y=sc.Number;
			if (sc.CheckString(","))
			{
				sc.MustGetString();
				if (sc.Compare("iwad")) iwadonly = true;
				if (sc.Compare("iwadforced")) forced = iwadonly = true;
			}
			if (texno.isValid())
			{
				FTexture * tex = TexMan[texno];

				int lumpnum = tex->GetSourceLump();
				// We only want to change texture offsets for sprites in the IWAD or the file this lump originated from.
				if (lumpnum >= 0 && lumpnum < Wads.GetNumLumps())
				{
					int wadno = Wads.GetLumpFile(lumpnum);
					if ((iwadonly && wadno==Wads.GetIwadNum()) || (!iwadonly && wadno == ofslumpno))
					{
						if (wadno == Wads.GetIwadNum() && !forced && iwadonly)
						{
							memcpy(&sprid, &tex->Name[0], 4);
							if (donotprocess.CheckKey(sprid)) continue;	// do not alter sprites that only get partially replaced.
						}
						tex->LeftOffset=x;
						tex->TopOffset=y;
						tex->KillNative();
					}
				}
			}
		}
	}
}



//==========================================================================
//
// Portal identifier lists
//
//==========================================================================


//==========================================================================
//
// MAPINFO stuff
//
//==========================================================================

struct FGLROptions : public FOptionalMapinfoData
{
	FGLROptions()
	{
		identifier = "gl_renderer";
		brightfog = false;
		lightmode = -1;
		notexturefill = -1;
		skyrotatevector = FVector3(0,0,1);
		skyrotatevector2 = FVector3(0,0,1);
		lightadditivesurfaces = false;
	}
	virtual FOptionalMapinfoData *Clone() const
	{
		FGLROptions *newopt = new FGLROptions;
		newopt->identifier = identifier;
		newopt->lightmode = lightmode;
		newopt->notexturefill = notexturefill;
		newopt->skyrotatevector = skyrotatevector;
		newopt->skyrotatevector2 = skyrotatevector2;
		newopt->lightadditivesurfaces = lightadditivesurfaces;
		return newopt;
	}
	int			lightmode;
	int			brightfog;
	int8_t		lightadditivesurfaces;
	int8_t		notexturefill;
	FVector3	skyrotatevector;
	FVector3	skyrotatevector2;
};

DEFINE_MAP_OPTION(brightfog, false)
{
	FGLROptions *opt = info->GetOptData<FGLROptions>("gl_renderer");
	parse.ParseAssign();
	parse.sc.MustGetNumber();
	opt->brightfog = parse.sc.Number;
}

DEFINE_MAP_OPTION(lightmode, false)
{
	FGLROptions *opt = info->GetOptData<FGLROptions>("gl_renderer");
	parse.ParseAssign();
	parse.sc.MustGetNumber();
	opt->lightmode = uint8_t(parse.sc.Number);
}

DEFINE_MAP_OPTION(notexturefill, false)
{
	FGLROptions *opt = info->GetOptData<FGLROptions>("gl_renderer");
	if (parse.CheckAssign())
	{
		parse.sc.MustGetNumber();
		opt->notexturefill = !!parse.sc.Number;
	}
	else
	{
		opt->notexturefill = true;
	}
}

DEFINE_MAP_OPTION(lightadditivesurfaces, false)
{
	FGLROptions *opt = info->GetOptData<FGLROptions>("gl_renderer");
	if (parse.CheckAssign())
	{
		parse.sc.MustGetNumber();
		opt->lightadditivesurfaces = !!parse.sc.Number;
	}
	else
	{
		opt->lightadditivesurfaces = true;
	}
}

DEFINE_MAP_OPTION(skyrotate, false)
{
	FGLROptions *opt = info->GetOptData<FGLROptions>("gl_renderer");

	parse.ParseAssign();
	parse.sc.MustGetFloat();
	opt->skyrotatevector.X = (float)parse.sc.Float;
	if (parse.format_type == FMapInfoParser::FMT_New) parse.sc.MustGetStringName(","); 
	parse.sc.MustGetFloat();
	opt->skyrotatevector.Y = (float)parse.sc.Float;
	if (parse.format_type == FMapInfoParser::FMT_New) parse.sc.MustGetStringName(","); 
	parse.sc.MustGetFloat();
	opt->skyrotatevector.Z = (float)parse.sc.Float;
	opt->skyrotatevector.MakeUnit();
}

DEFINE_MAP_OPTION(skyrotate2, false)
{
	FGLROptions *opt = info->GetOptData<FGLROptions>("gl_renderer");

	parse.ParseAssign();
	parse.sc.MustGetFloat();
	opt->skyrotatevector2.X = (float)parse.sc.Float;
	if (parse.format_type == FMapInfoParser::FMT_New) parse.sc.MustGetStringName(","); 
	parse.sc.MustGetFloat();
	opt->skyrotatevector2.Y = (float)parse.sc.Float;
	if (parse.format_type == FMapInfoParser::FMT_New) parse.sc.MustGetStringName(","); 
	parse.sc.MustGetFloat();
	opt->skyrotatevector2.Z = (float)parse.sc.Float;
	opt->skyrotatevector2.MakeUnit();
}

bool IsLightmodeValid()
{
	return (glset.map_lightmode >= 0 && glset.map_lightmode <= 4) || glset.map_lightmode == 8;
}

static void ResetOpts()
{
	if (!IsLightmodeValid()) glset.lightmode = gl_lightmode;
	else glset.lightmode = glset.map_lightmode;
	if (glset.map_notexturefill == -1) glset.notexturefill = gl_notexturefill;
	else glset.notexturefill = !!glset.map_notexturefill;
	if (glset.map_brightfog == -1) glset.brightfog = gl_brightfog;
	else glset.brightfog = !!glset.map_brightfog;
	if (glset.map_lightadditivesurfaces == -1) glset.lightadditivesurfaces = gl_lightadditivesurfaces;
	else glset.lightadditivesurfaces = !!glset.map_lightadditivesurfaces;
}

void InitGLRMapinfoData()
{
	FGLROptions *opt = level.info->GetOptData<FGLROptions>("gl_renderer", false);

	if (opt != NULL)
	{
		glset.map_lightmode = opt->lightmode;
		glset.map_lightadditivesurfaces = opt->lightadditivesurfaces;
		glset.map_brightfog = opt->brightfog;
		glset.map_notexturefill = opt->notexturefill;
		glset.skyrotatevector = opt->skyrotatevector;
		glset.skyrotatevector2 = opt->skyrotatevector2;
	}
	else
	{
		glset.map_lightmode = -1;
		glset.map_lightadditivesurfaces = -1;
		glset.map_brightfog = -1;
		glset.map_notexturefill = -1;
		glset.skyrotatevector = FVector3(0, 0, 1);
		glset.skyrotatevector2 = FVector3(0, 0, 1);
	}
	ResetOpts();
}
CCMD(gl_resetmap)
{
	ResetOpts();
}


//==========================================================================
//
// Recalculate all heights affecting this vertex.
//
//==========================================================================
void gl_RecalcVertexHeights(vertex_t * v)
{
	int i,j,k;
	float height;

	v->numheights=0;
	for(i=0;i<v->numsectors;i++)
	{
		for(j=0;j<2;j++)
		{
			if (j==0) height=v->sectors[i]->ceilingplane.ZatPoint(v);
			else height=v->sectors[i]->floorplane.ZatPoint(v);

			for(k=0;k<v->numheights;k++)
			{
				if (height == v->heightlist[k]) break;
				if (height < v->heightlist[k])
				{
					memmove(&v->heightlist[k+1], &v->heightlist[k], sizeof(float) * (v->numheights-k));
					v->heightlist[k]=height;
					v->numheights++;
					break;
				}
			}
			if (k==v->numheights) v->heightlist[v->numheights++]=height;
		}
	}
	if (v->numheights<=2) v->numheights=0;	// is not in need of any special attention
	v->dirty = false;
}




void gl_InitData()
{
	AdjustSpriteOffsets();
	AddAutoBrightmaps();
}

//==========================================================================
//
// dumpgeometry
//
//==========================================================================

CCMD(dumpgeometry)
{
	for(auto &sector : level.sectors)
	{
		Printf(PRINT_LOG, "Sector %d\n", sector.sectornum);
		for(int j=0;j<sector.subsectorcount;j++)
		{
			subsector_t * sub = sector.subsectors[j];

			Printf(PRINT_LOG, "    Subsector %d - real sector = %d - %s\n", int(sub->Index()), sub->sector->sectornum, sub->hacked&1? "hacked":"");
			for(uint32_t k=0;k<sub->numlines;k++)
			{
				seg_t * seg = sub->firstline + k;
				if (seg->linedef)
				{
				Printf(PRINT_LOG, "      (%4.4f, %4.4f), (%4.4f, %4.4f) - seg %d, linedef %d, side %d", 
					seg->v1->fX(), seg->v1->fY(), seg->v2->fX(), seg->v2->fY(),
					seg->Index(), seg->linedef->Index(), seg->sidedef != seg->linedef->sidedef[0]);
				}
				else
				{
					Printf(PRINT_LOG, "      (%4.4f, %4.4f), (%4.4f, %4.4f) - seg %d, miniseg", 
						seg->v1->fX(), seg->v1->fY(), seg->v2->fX(), seg->v2->fY(), seg->Index());
				}
				if (seg->PartnerSeg) 
				{
					subsector_t * sub2 = seg->PartnerSeg->Subsector;
					Printf(PRINT_LOG, ", back sector = %d, real back sector = %d", sub2->render_sector->sectornum, seg->PartnerSeg->frontsector->sectornum);
				}
				else if (seg->backsector)
			{
					Printf(PRINT_LOG, ", back sector = %d (no partnerseg)", seg->backsector->sectornum);
				}

				Printf(PRINT_LOG, "\n");
			}
		}
	}
}

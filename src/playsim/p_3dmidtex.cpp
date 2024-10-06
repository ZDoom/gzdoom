/*
** p_3dmidtex.cpp
**
** Eternity-style 3D-midtex handling
** (No original Eternity code here!)
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



#include "p_3dmidtex.h"
#include "p_local.h"
#include "p_terrain.h"
#include "p_maputl.h"
#include "p_spec.h"
#include "g_levellocals.h"
#include "actor.h"
#include "texturemanager.h"
#include "vm.h"


//============================================================================
//
// P_Scroll3dMidtex
//
// Scrolls all sidedefs belonging to 3dMidtex lines attached to this sector
//
//============================================================================

bool P_Scroll3dMidtex(sector_t *sector, int crush, double move, bool ceiling, bool instant)
{
	extsector_t::midtex::plane &scrollplane = ceiling? sector->e->Midtex.Ceiling : sector->e->Midtex.Floor;

	// First step: Change all lines' texture offsets
	for(unsigned i = 0; i < scrollplane.AttachedLines.Size(); i++)
	{
		line_t *l = scrollplane.AttachedLines[i];

		l->sidedef[0]->AddTextureYOffset(side_t::mid, move);
		l->sidedef[1]->AddTextureYOffset(side_t::mid, move);
	}

	// Second step: Check all sectors whether the move is ok.
	bool res = false;

	for(unsigned i = 0; i < scrollplane.AttachedSectors.Size(); i++)
	{
		res |= P_ChangeSector(scrollplane.AttachedSectors[i], crush, move, 2, true, instant);
	}
	return !res;
}

//============================================================================
//
// P_Start3DMidtexInterpolations
//
// Starts interpolators for every sidedef that is being changed by moving
// this sector
//
//============================================================================

void P_Start3dMidtexInterpolations(TArray<DInterpolation *> &list, sector_t *sector, bool ceiling)
{
	extsector_t::midtex::plane &scrollplane = ceiling? sector->e->Midtex.Ceiling : sector->e->Midtex.Floor;

	for(unsigned i = 0; i < scrollplane.AttachedLines.Size(); i++)
	{
		line_t *l = scrollplane.AttachedLines[i];

		list.Push(l->sidedef[0]->SetInterpolation(side_t::mid));
		list.Push(l->sidedef[1]->SetInterpolation(side_t::mid));
	}
}

//============================================================================
//
// P_Attach3dMidtexLinesToSector
//
// Attaches 3dMidtex lines to a sector.
// If lineid is != 0, all lines with the matching line id will be added
// If tag is != 0, all lines touching a sector with the matching tag will be added
// If both are != 0, all lines with the matching line id that touch a sector with
// the matching tag will be added.
//
//============================================================================

void P_Attach3dMidtexLinesToSector(sector_t *sector, int lineid, int tag, bool ceiling)
{
	int v;

	if (lineid == 0 && tag == 0)
	{
		// invalid set of parameters
		return;
	}
	auto Level = sector->Level;

	extsector_t::midtex::plane &scrollplane = ceiling? sector->e->Midtex.Ceiling : sector->e->Midtex.Floor;

	// Bit arrays that mark whether a line or sector is to be attached.
	uint8_t *found_lines = new uint8_t[(Level->lines.Size()+7)/8];
	uint8_t *found_sectors = new uint8_t[(Level->sectors.Size()+7)/8];

	memset(found_lines, 0, sizeof (uint8_t) * ((Level->lines.Size()+7)/8));
	memset(found_sectors, 0, sizeof (uint8_t) * ((Level->sectors.Size()+7)/8));

	// mark all lines and sectors that are already attached to this one
	// and clear the arrays. The old data will be re-added automatically
	// from the marker arrays.
	for (unsigned i=0; i < scrollplane.AttachedLines.Size(); i++)
	{
		int line = scrollplane.AttachedLines[i]->Index();
		found_lines[line>>3] |= 1 << (line&7);
	}

	for (unsigned i=0; i < scrollplane.AttachedSectors.Size(); i++)
	{
		int sec = scrollplane.AttachedSectors[i]->Index();
		found_sectors[sec>>3] |= 1 << (sec&7);
	}

	scrollplane.AttachedLines.Clear();
	scrollplane.AttachedSectors.Clear();

	if (tag == 0)
	{
		auto itr = Level->GetLineIdIterator(lineid);
		int line;
		while ((line = itr.Next()) >= 0)
		{
			line_t *ln = &Level->lines[line];

			if (ln->frontsector == NULL || ln->backsector == NULL || !(ln->flags & ML_3DMIDTEX))
			{
				// Only consider two-sided lines with the 3DMIDTEX flag
				continue;
			}
			found_lines[line>>3] |= 1 << (line&7);
		}
	}
	else
	{
		auto it = Level->GetSectorTagIterator(tag);
		int sec;
		while ((sec = it.Next()) >= 0)
		{
			for (auto ln : Level->sectors[sec].Lines)
			{
				if (lineid != 0 && !Level->LineHasId(ln, lineid)) continue;

				if (ln->frontsector == NULL || ln->backsector == NULL || !(ln->flags & ML_3DMIDTEX))
				{
					// Only consider two-sided lines with the 3DMIDTEX flag
					continue;
				}
				int lineno = ln->Index();
				found_lines[lineno >> 3] |= 1 << (lineno & 7);
			}
		}
	}


	for(unsigned i=0; i < Level->lines.Size(); i++)
	{
		if (found_lines[i>>3] & (1 << (i&7)))
		{
			auto &line = Level->lines[i];
			scrollplane.AttachedLines.Push(&line);

			v = line.frontsector->Index();
			assert(v < (int)Level->sectors.Size());
			found_sectors[v>>3] |= 1 << (v&7);

			v = line.backsector->Index();
			assert(v < (int)Level->sectors.Size());
			found_sectors[v>>3] |= 1 << (v&7);
		}
	}

	for (unsigned i=0; i < Level->sectors.Size(); i++)
	{
		if (found_sectors[i>>3] & (1 << (i&7)))
		{
			scrollplane.AttachedSectors.Push(&Level->sectors[i]);
		}
	}

	delete[] found_lines;
	delete[] found_sectors;
}


//============================================================================
//
// P_GetMidTexturePosition
//
// Retrieves top and bottom of the current line's mid texture.
//
//============================================================================
bool P_GetMidTexturePosition(const line_t *line, int sideno, double *ptextop, double *ptexbot)
{
	if (line->sidedef[0]==NULL || line->sidedef[1]==NULL) return false;
	
	assert(sideno >= 0 && sideno <= 1);

	side_t *side = line->sidedef[sideno];
	FTextureID texnum = side->GetTexture(side_t::mid);
	if (!texnum.isValid()) return false;
	FGameTexture * tex= TexMan.GetGameTexture(texnum, true);
	if (!tex) return false;

	FTexCoordInfo tci;

	// We only need the vertical positioning info here.
	tci.GetFromTexture(tex, 1., (float)side->GetTextureYScale(side_t::mid), !!(line->GetLevel()->flags3 & LEVEL3_FORCEWORLDPANNING));
	double y_offset = tci.RowOffset((float)side->GetTextureYOffset(side_t::mid));
	double textureheight = tci.mRenderHeight;

	if(line->flags & ML_DONTPEGBOTTOM)
	{
		*ptexbot = y_offset +
			max(line->frontsector->GetPlaneTexZ(sector_t::floor), line->backsector->GetPlaneTexZ(sector_t::floor));

		*ptextop = *ptexbot + textureheight;
	}
	else
	{
		*ptextop = y_offset +
		   min(line->frontsector->GetPlaneTexZ(sector_t::ceiling), line->backsector->GetPlaneTexZ(sector_t::ceiling));
		
		*ptexbot = *ptextop - textureheight;
	}
	return true;
}

DEFINE_ACTION_FUNCTION(_Line, GetMidTexturePosition)
{
	PARAM_SELF_STRUCT_PROLOGUE(line_t);
	PARAM_INT(side);
	double top = 0.0;
	double bottom = 0.0;

	if(side < 0) ThrowAbortException(X_ARRAY_OUT_OF_BOUNDS, "side is negative");
	else if(side > 1) ThrowAbortException(X_ARRAY_OUT_OF_BOUNDS, "side is greater than one");

	bool res = P_GetMidTexturePosition(self,side,&top,&bottom);
	if (numret > 2) ret[2].SetFloat(bottom);
	if (numret > 1) ret[1].SetFloat(top);
	if (numret > 0) ret[0].SetInt(int(res));
	return numret;
}

//============================================================================
//
// P_LineOpening_3dMidtex
//
// 3dMidtex part of P_LineOpening
//
//============================================================================

bool P_LineOpening_3dMidtex(AActor *thing, const line_t *linedef, FLineOpening &open, bool restrict)
{
	// [TP] Impassible-like 3dmidtextures do not block missiles
	if ((linedef->flags & ML_3DMIDTEX_IMPASS)
		&& (thing->flags & MF_MISSILE || thing->BounceFlags & BOUNCE_MBF))
	{
		return false;
	}

	double tt, tb;

	open.abovemidtex = false;
	if (P_GetMidTexturePosition(linedef, 0, &tt, &tb))
	{
		if (thing->Center() < (tt + tb)/2)
		{
			if (tb < open.top)
			{
				open.top = tb;
				open.ceilingpic = linedef->sidedef[0]->GetTexture(side_t::mid);
			}
		}
		else
		{
			if (tt > open.bottom && (!restrict || thing->Z() >= tt))
			{
				open.bottom = tt;
				open.abovemidtex = true;
				open.floorpic = linedef->sidedef[0]->GetTexture(side_t::mid);
				open.floorterrain = TerrainTypes[open.floorpic];
				open.frontfloorplane.SetAtHeight(tt, sector_t::floor);
				open.backfloorplane.SetAtHeight(tt, sector_t::floor);

			}
			// returns true if it touches the midtexture
			return (fabs(thing->Z() - tt) <= thing->MaxStepHeight);
		}
	}
	return false;

	/* still have to figure out what this code from Eternity means...
	if((linedef->flags & ML_BLOCKMONSTERS) && 
		!(mo->flags & (MF_FLOAT | MF_DROPOFF)) &&
		fabs(mo->Z() - tt) <= 24)
	{
		opentop = openbottom;
		openrange = 0;
		return;
	}
	*/
}

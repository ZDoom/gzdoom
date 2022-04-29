/*
** p_slopes.cpp
** Slope creation
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
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

#include "doomtype.h"
#include "p_local.h"
#include "cmdlib.h"
#include "p_lnspec.h"
#include "p_maputl.h"
#include "p_spec.h"
#include "g_levellocals.h"
#include "actor.h"
#include "p_setup.h"
#include "maploader.h"

//===========================================================================
//
// P_SpawnSlopeMakers
//
//===========================================================================

void MapLoader::SlopeLineToPoint (int lineid, const DVector3 &pos, bool slopeCeil)
{
	int linenum;

	auto itr = Level->GetLineIdIterator(lineid);
	while ((linenum = itr.Next()) >= 0)
	{
		const line_t *line = &Level->lines[linenum];
		sector_t *sec;
		secplane_t *plane;
		
		if (P_PointOnLineSidePrecise (pos, line) == 0)
		{
			sec = line->frontsector;
		}
		else
		{
			sec = line->backsector;
		}
		if (sec == NULL)
		{
			continue;
		}
		if (slopeCeil)
		{
			plane = &sec->ceilingplane;
		}
		else
		{
			plane = &sec->floorplane;
		}

		DVector3 p, v1, v2, cross;

		p[0] = line->v1->fX();
		p[1] = line->v1->fY();
		p[2] = plane->ZatPoint (line->v1);
		v1[0] = line->Delta().X;
		v1[1] = line->Delta().Y;
		v1[2] = plane->ZatPoint (line->v2) - p[2];
		v2[0] = pos.X - p[0];
		v2[1] = pos.Y - p[1];
		v2[2] = pos.Z - p[2];

		cross = v1 ^ v2;
		double len = cross.Length();
		if (len == 0)
		{
			Printf ("Slope thing at (%f,%f) lies directly on its target line.\n", pos.X, pos.Y);
			return;
		}
		cross /= len;
		// Fix backward normals
		if ((cross.Z < 0 && !slopeCeil) || (cross.Z > 0 && slopeCeil))
		{
			cross = -cross;
		}

		double dist = -cross[0] * pos.X - cross[1] * pos.Y - cross[2] * pos.Z;
		plane->set(cross[0], cross[1], cross[2], dist);
	}
}

//===========================================================================
//
// P_CopyPlane
//
//===========================================================================

void MapLoader::CopyPlane (int tag, sector_t *dest, bool copyCeil)
{
	sector_t *source;
	int secnum;

	secnum = Level->FindFirstSectorFromTag (tag);
	if (secnum == -1)
	{
		return;
	}

	source = &Level->sectors[secnum];

	if (copyCeil)
	{
		dest->ceilingplane = source->ceilingplane;
	}
	else
	{
		dest->floorplane = source->floorplane;
	}
}

void MapLoader::CopyPlane (int tag, const DVector2 &pos, bool copyCeil)
{
	sector_t *dest = Level->PointInSector (pos);
	CopyPlane(tag, dest, copyCeil);
}

//===========================================================================
//
// P_SetSlope
//
//===========================================================================

void MapLoader::SetSlope (secplane_t *plane, bool setCeil, int xyangi, int zangi, const DVector3 &pos)
{
	DAngle xyang;
	DAngle zang;

	if (zangi >= 180)
	{
		zang = 179.;
	}
	else if (zangi <= 0)
	{
		zang = 1.;
	}
	else
	{
		zang = (double)zangi;
	}
	if (setCeil)
	{
		zang += 180.;
	}

	xyang = (double)xyangi;

	DVector3 norm;

	if (Level->ib_compatflags & BCOMPATF_SETSLOPEOVERFLOW)
	{
		// We have to consider an integer multiplication overflow here.
		norm[0] = FixedToFloat(FloatToFixed(zang.Cos()) * FloatToFixed(xyang.Cos())) / 65536.;
		norm[1] = FixedToFloat(FloatToFixed(zang.Cos()) * FloatToFixed(xyang.Sin())) / 65536.;
	}
	else
	{
		norm[0] = zang.Cos() * xyang.Cos();
		norm[1] = zang.Cos() * xyang.Sin();
	}
	norm[2] = zang.Sin();
	norm.MakeUnit();
	double dist = -norm[0] * pos.X - norm[1] * pos.Y - norm[2] * pos.Z;
	plane->set(norm[0], norm[1], norm[2], dist);
}


//===========================================================================
//
// P_VavoomSlope
//
//===========================================================================

void MapLoader::VavoomSlope(sector_t * sec, int id, const DVector3 &pos, int which)
{
	for(auto l : sec->Lines)
	{
		if (l->args[0]==id)
		{
			DVector3 v1, v2, cross;
			secplane_t *srcplane = (which == 0) ? &sec->floorplane : &sec->ceilingplane;
			double srcheight = (which == 0) ? sec->GetPlaneTexZ(sector_t::floor) : sec->GetPlaneTexZ(sector_t::ceiling);

			v1[0] = pos.X - l->v2->fX();
			v1[1] = pos.Y - l->v2->fY();
			v1[2] = pos.Z - srcheight;
			
			v2[0] = pos.X - l->v1->fX();
			v2[1] = pos.Y - l->v1->fY();
			v2[2] = pos.Z - srcheight;

			cross = v1 ^ v2;
			double len = cross.Length();
			if (len == 0)
			{
				Printf ("Slope thing at (%f,%f) lies directly on its target line.\n", pos.X, pos.Y);
				return;
			}
			cross /= len;

			// Fix backward normals
			if ((cross.Z < 0 && which == 0) || (cross.Z > 0 && which == 1))
			{
				cross = -cross;
			}

			double dist = -cross[0] * pos.X - cross[1] * pos.Y - cross[2] * pos.Z;
			srcplane->set(cross[0], cross[1], cross[2], dist);
			return;
		}
	}
}
				   
//==========================================================================
//
//	P_SetSlopesFromVertexHeights
//
//==========================================================================

void MapLoader::SetSlopesFromVertexHeights(FMapThing *firstmt, FMapThing *lastmt, const int *oldvertextable)
{
	TMap<int, double> vt_heights[2];
	FMapThing *mt;
	bool vt_found = false;

	for (mt = firstmt; mt < lastmt; ++mt)
	{
		if (mt->info != NULL && mt->info->Type == NULL)
		{
			if (mt->info->Special == SMT_VertexFloorZ || mt->info->Special == SMT_VertexCeilingZ)
			{
				for (unsigned i = 0; i < Level->vertexes.Size(); i++)
				{
					if (Level->vertexes[i].fX() == mt->pos.X && Level->vertexes[i].fY() == mt->pos.Y)
					{
						if (mt->info->Special == SMT_VertexFloorZ)
						{
							vt_heights[0][i] = mt->pos.Z;
						}
						else
						{
							vt_heights[1][i] = mt->pos.Z;
						}
						vt_found = true;
					}
				}
				mt->EdNum = 0;
			}
		}
	}

	for(unsigned i = 0; i < vertexdatas.Size(); i++)
	{
		int ii = oldvertextable == NULL ? i : oldvertextable[i];

		if (vertexdatas[i].flags & VERTEXFLAG_ZCeilingEnabled)
		{
			vt_heights[1][ii] = vertexdatas[i].zCeiling;
			vt_found = true;
		}

		if (vertexdatas[i].flags & VERTEXFLAG_ZFloorEnabled)
		{
			vt_heights[0][ii] = vertexdatas[i].zFloor;
			vt_found = true;
		}
	}

	// If vertexdata_t is ever extended for non-slope usage, this will obviously have to be deferred or removed.
	vertexdatas.Clear();
	vertexdatas.ShrinkToFit();

	if (vt_found)
	{
		for (auto &sec : Level->sectors)
		{
			if (sec.Lines.Size() != 3) continue;	// only works with triangular sectors

			DVector3 vt1, vt2, vt3;
			DVector3 vec1, vec2;
			int vi1, vi2, vi3;

			vi1 = Index(sec.Lines[0]->v1);
			vi2 = Index(sec.Lines[0]->v2);
			vi3 = (sec.Lines[1]->v1 == sec.Lines[0]->v1 || sec.Lines[1]->v1 == sec.Lines[0]->v2)?
				Index(sec.Lines[1]->v2) : Index(sec.Lines[1]->v1);

			vt1 = DVector3(Level->vertexes[vi1].fPos(), 0);
			vt2 = DVector3(Level->vertexes[vi2].fPos(), 0);
			vt3 = DVector3(Level->vertexes[vi3].fPos(), 0);

			for(int j=0; j<2; j++)
			{
				double *h1 = vt_heights[j].CheckKey(vi1);
				double *h2 = vt_heights[j].CheckKey(vi2);
				double *h3 = vt_heights[j].CheckKey(vi3);
				if (h1 == NULL && h2 == NULL && h3 == NULL) continue;

				vt1.Z = h1? *h1 : j==0? sec.GetPlaneTexZ(sector_t::floor) : sec.GetPlaneTexZ(sector_t::ceiling);
				vt2.Z = h2? *h2 : j==0? sec.GetPlaneTexZ(sector_t::floor) : sec.GetPlaneTexZ(sector_t::ceiling);
				vt3.Z = h3? *h3 : j==0? sec.GetPlaneTexZ(sector_t::floor) : sec.GetPlaneTexZ(sector_t::ceiling);

				if (P_PointOnLineSidePrecise(Level->vertexes[vi3].fX(), Level->vertexes[vi3].fY(), sec.Lines[0]) == 0)
				{
					vec1 = vt2 - vt3;
					vec2 = vt1 - vt3;
				}
				else
				{
					vec1 = vt1 - vt3;
					vec2 = vt2 - vt3;
				}

				DVector3 cross = vec1 ^ vec2;

				double len = cross.Length();
				if (len == 0)
				{
					// Only happens when all vertices in this sector are on the same line.
					// Let's just ignore this case.
					continue;
				}
				cross /= len;

				// Fix backward normals
				if ((cross.Z < 0 && j == 0) || (cross.Z > 0 && j == 1))
				{
					cross = -cross;
				}

				secplane_t *plane = j==0? &sec.floorplane : &sec.ceilingplane;

				double dist = -cross[0] * Level->vertexes[vi3].fX() - cross[1] * Level->vertexes[vi3].fY() - cross[2] * vt3.Z;
				plane->set(cross[0], cross[1], cross[2], dist);
			}
		}
	}
}

//===========================================================================
//
// P_SpawnSlopeMakers
//
//===========================================================================

void MapLoader::SpawnSlopeMakers (FMapThing *firstmt, FMapThing *lastmt, const int *oldvertextable)
{
	FMapThing *mt;

	for (mt = firstmt; mt < lastmt; ++mt)
	{
		if (mt->info != NULL && mt->info->Type == NULL &&
		   (mt->info->Special >= SMT_SlopeFloorPointLine && mt->info->Special <= SMT_VavoomCeiling))
		{
			DVector3 pos = mt->pos;
			secplane_t *refplane;
			sector_t *sec;
			bool ceiling;

			sec = Level->PointInSector (mt->pos);
			if (mt->info->Special == SMT_SlopeCeilingPointLine || mt->info->Special == SMT_VavoomCeiling || mt->info->Special == SMT_SetCeilingSlope)
			{
				refplane = &sec->ceilingplane;
				ceiling = true;
			}
			else
			{
				refplane = &sec->floorplane;
				ceiling = false;
			}
			pos.Z = refplane->ZatPoint (mt->pos) + mt->pos.Z;

			if (mt->info->Special <= SMT_SlopeCeilingPointLine)
			{ // SlopeFloorPointLine and SlopCeilingPointLine
				SlopeLineToPoint (mt->args[0], pos, ceiling);
			}
			else if (mt->info->Special <= SMT_SetCeilingSlope)
			{ // SetFloorSlope and SetCeilingSlope
				SetSlope (refplane, ceiling, mt->angle, mt->args[0], pos);
			}
			else 
			{ // VavoomFloor and VavoomCeiling (these do not perform any sector height adjustment - z is absolute)
				VavoomSlope(sec, mt->thingid, mt->pos, ceiling); 
			}
			mt->EdNum = 0;
		}
	}
	SetSlopesFromVertexHeights(firstmt, lastmt, oldvertextable);

	for (mt = firstmt; mt < lastmt; ++mt)
	{
		if (mt->info != NULL && mt->info->Type == NULL &&
			(mt->info->Special == SMT_CopyFloorPlane || mt->info->Special == SMT_CopyCeilingPlane))
		{
			CopyPlane (mt->args[0], mt->pos, mt->info->Special == SMT_CopyCeilingPlane);
			mt->EdNum = 0;
		}
	}

}


//===========================================================================
//
// [RH] Set slopes for sectors, based on line specials
//
// P_AlignPlane
//
// Aligns the floor or ceiling of a sector to the corresponding plane
// on the other side of the reference line. (By definition, line must be
// two-sided.)
//
// If (which & 1), sets floor.
// If (which & 2), sets ceiling.
//
//===========================================================================

void MapLoader::AlignPlane(sector_t *sec, line_t *line, int which)
{
	sector_t *refsec;
	double bestdist;
	vertex_t *refvert = sec->Lines[0]->v1;	// Shut up, GCC

	if (line->backsector == NULL)
		return;

	// Find furthest vertex from the reference line. It, along with the two ends
	// of the line, will define the plane.
	bestdist = 0;
	for (auto ln : sec->Lines)
	{
		for (int i = 0; i < 2; i++)
		{
			double dist;
			vertex_t *vert;

			vert = i == 0 ? ln->v1 : ln->v2;
			dist = fabs((line->v1->fY() - vert->fY()) * line->Delta().X -
				(line->v1->fX() - vert->fX()) * line->Delta().Y);

			if (dist > bestdist)
			{
				bestdist = dist;
				refvert = vert;
			}
		}
	}

	refsec = line->frontsector == sec ? line->backsector : line->frontsector;

	DVector3 p, v1, v2, cross;

	secplane_t *srcplane;
	double srcheight, destheight;

	srcplane = (which == 0) ? &sec->floorplane : &sec->ceilingplane;
	srcheight = (which == 0) ? sec->GetPlaneTexZ(sector_t::floor) : sec->GetPlaneTexZ(sector_t::ceiling);
	destheight = (which == 0) ? refsec->GetPlaneTexZ(sector_t::floor) : refsec->GetPlaneTexZ(sector_t::ceiling);

	p[0] = line->v1->fX();
	p[1] = line->v1->fY();
	p[2] = destheight;
	v1[0] = line->Delta().X;
	v1[1] = line->Delta().Y;
	v1[2] = 0;
	v2[0] = refvert->fX() - line->v1->fX();
	v2[1] = refvert->fY() - line->v1->fY();
	v2[2] = srcheight - destheight;

	cross = (v1 ^ v2).Unit();

	// Fix backward normals
	if ((cross.Z < 0 && which == 0) || (cross.Z > 0 && which == 1))
	{
		cross = -cross;
	}

	double dist = -cross[0] * line->v1->fX() - cross[1] * line->v1->fY() - cross[2] * destheight;
	srcplane->set(cross[0], cross[1], cross[2], dist);
}

//===========================================================================
//
// P_SetSlopes
//
//===========================================================================

void MapLoader::SetSlopes ()
{
	int s;

	for (auto &line : Level->lines)
	{
		if (line.special == Plane_Align)
		{
			line.special = 0;
			if (line.backsector != nullptr)
			{
				// args[0] is for floor, args[1] is for ceiling
				//
				// As a special case, if args[1] is 0,
				// then args[0], bits 2-3 are for ceiling.
				for (s = 0; s < 2; s++)
				{
					int bits = line.args[s] & 3;

					if (s == 1 && bits == 0)
						bits = (line.args[0] >> 2) & 3;

					if (bits == 1)			// align front side to back
						AlignPlane (line.frontsector, &line, s);
					else if (bits == 2)		// align back side to front
						AlignPlane (line.backsector, &line, s);
				}
			}
		}
	}
}

//===========================================================================
//
// P_CopySlopes
//
//===========================================================================

void MapLoader::CopySlopes()
{
	for (auto &line : Level->lines)
	{
		if (line.special == Plane_Copy)
		{
			// The args are used for the tags of sectors to copy:
			// args[0]: front floor
			// args[1]: front ceiling
			// args[2]: back floor
			// args[3]: back ceiling
			// args[4]: copy slopes from one side of the line to the other.
			line.special = 0;
			for (int s = 0; s < (line.backsector ? 4 : 2); s++)
			{
				if (line.args[s])
					CopyPlane(line.args[s], 
					(s & 2 ? line.backsector : line.frontsector), s & 1);
			}

			if (line.backsector != NULL)
			{
				if ((line.args[4] & 3) == 1)
				{
					line.backsector->floorplane = line.frontsector->floorplane;
				}
				else if ((line.args[4] & 3) == 2)
				{
					line.frontsector->floorplane = line.backsector->floorplane;
				}
				if ((line.args[4] & 12) == 4)
				{
					line.backsector->ceilingplane = line.frontsector->ceilingplane;
				}
				else if ((line.args[4] & 12) == 8)
				{
					line.frontsector->ceilingplane = line.backsector->ceilingplane;
				}
			}
		}
	}
}

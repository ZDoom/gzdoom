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

//===========================================================================
//
// P_SpawnSlopeMakers
//
//===========================================================================

static void P_SlopeLineToPoint (int lineid, fixed_t x, fixed_t y, fixed_t z, bool slopeCeil)
{
	int linenum = -1;

	while ((linenum = P_FindLineFromID (lineid, linenum)) != -1)
	{
		const line_t *line = &lines[linenum];
		sector_t *sec;
		secplane_t *plane;
		
		if (P_PointOnLineSide (x, y, line) == 0)
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

		FVector3 p, v1, v2, cross;

		p[0] = FIXED2FLOAT (line->v1->x);
		p[1] = FIXED2FLOAT (line->v1->y);
		p[2] = FIXED2FLOAT (plane->ZatPoint (line->v1->x, line->v1->y));
		v1[0] = FIXED2FLOAT (line->dx);
		v1[1] = FIXED2FLOAT (line->dy);
		v1[2] = FIXED2FLOAT (plane->ZatPoint (line->v2->x, line->v2->y)) - p[2];
		v2[0] = FIXED2FLOAT (x - line->v1->x);
		v2[1] = FIXED2FLOAT (y - line->v1->y);
		v2[2] = FIXED2FLOAT (z) - p[2];

		cross = v1 ^ v2;
		double len = cross.Length();
		if (len == 0)
		{
			Printf ("Slope thing at (%d,%d) lies directly on its target line.\n", int(x>>16), int(y>>16));
			return;
		}
		cross /= len;
		// Fix backward normals
		if ((cross.Z < 0 && !slopeCeil) || (cross.Z > 0 && slopeCeil))
		{
			cross = -cross;
		}

		plane->a = FLOAT2FIXED (cross[0]);
		plane->b = FLOAT2FIXED (cross[1]);
		plane->c = FLOAT2FIXED (cross[2]);
		//plane->ic = FLOAT2FIXED (1.f/cross[2]);
		plane->ic = DivScale32 (1, plane->c);
		plane->d = -TMulScale16 (plane->a, x,
								 plane->b, y,
								 plane->c, z);
	}
}

//===========================================================================
//
// P_CopyPlane
//
//===========================================================================

static void P_CopyPlane (int tag, sector_t *dest, bool copyCeil)
{
	sector_t *source;
	int secnum;
	size_t planeofs;

	secnum = P_FindSectorFromTag (tag, -1);
	if (secnum == -1)
	{
		return;
	}

	source = &sectors[secnum];

	if (copyCeil)
	{
		planeofs = myoffsetof(sector_t, ceilingplane);
	}
	else
	{
		planeofs = myoffsetof(sector_t, floorplane);
	}
	*(secplane_t *)((BYTE *)dest + planeofs) = *(secplane_t *)((BYTE *)source + planeofs);
}

static void P_CopyPlane (int tag, fixed_t x, fixed_t y, bool copyCeil)
{
	sector_t *dest = P_PointInSector (x, y);
	P_CopyPlane(tag, dest, copyCeil);
}

//===========================================================================
//
// P_SetSlope
//
//===========================================================================

void P_SetSlope (secplane_t *plane, bool setCeil, int xyangi, int zangi,
	fixed_t x, fixed_t y, fixed_t z)
{
	angle_t xyang;
	angle_t zang;

	if (zangi >= 180)
	{
		zang = ANGLE_180-ANGLE_1;
	}
	else if (zangi <= 0)
	{
		zang = ANGLE_1;
	}
	else
	{
		zang = Scale (zangi, ANGLE_90, 90);
	}
	if (setCeil)
	{
		zang += ANGLE_180;
	}
	zang >>= ANGLETOFINESHIFT;

	// Sanitize xyangi to [0,360) range
	xyangi = xyangi % 360;
	if (xyangi < 0)
	{
		xyangi = 360 + xyangi;
	}
	xyang = (angle_t)Scale (xyangi, ANGLE_90, 90 << ANGLETOFINESHIFT);

	FVector3 norm;

	if (ib_compatflags & BCOMPATF_SETSLOPEOVERFLOW)
	{
		norm[0] = float(finecosine[zang] * finecosine[xyang]);
		norm[1] = float(finecosine[zang] * finesine[xyang]);
	}
	else
	{
		norm[0] = float(finecosine[zang]) * float(finecosine[xyang]);
		norm[1] = float(finecosine[zang]) * float(finesine[xyang]);
	}
	norm[2] = float(finesine[zang]) * 65536.f;
	norm.MakeUnit();
	plane->a = (int)(norm[0] * 65536.f);
	plane->b = (int)(norm[1] * 65536.f);
	plane->c = (int)(norm[2] * 65536.f);
	//plane->ic = (int)(65536.f / norm[2]);
	plane->ic = DivScale32 (1, plane->c);
	plane->d = -TMulScale16 (plane->a, x,
							 plane->b, y,
							 plane->c, z);
}


//===========================================================================
//
// P_VavoomSlope
//
//===========================================================================

void P_VavoomSlope(sector_t * sec, int id, fixed_t x, fixed_t y, fixed_t z, int which)
{
	for (int i=0;i<sec->linecount;i++)
	{
		line_t * l=sec->lines[i];

		if (l->args[0]==id)
		{
			FVector3 v1, v2, cross;
			secplane_t *srcplane = (which == 0) ? &sec->floorplane : &sec->ceilingplane;
			fixed_t srcheight = (which == 0) ? sec->GetPlaneTexZ(sector_t::floor) : sec->GetPlaneTexZ(sector_t::ceiling);

			v1[0] = FIXED2FLOAT (x - l->v2->x);
			v1[1] = FIXED2FLOAT (y - l->v2->y);
			v1[2] = FIXED2FLOAT (z - srcheight);
			
			v2[0] = FIXED2FLOAT (x - l->v1->x);
			v2[1] = FIXED2FLOAT (y - l->v1->y);
			v2[2] = FIXED2FLOAT (z - srcheight);

			cross = v1 ^ v2;
			double len = cross.Length();
			if (len == 0)
			{
				Printf ("Slope thing at (%d,%d) lies directly on its target line.\n", int(x>>16), int(y>>16));
				return;
			}
			cross /= len;

			// Fix backward normals
			if ((cross.Z < 0 && which == 0) || (cross.Z > 0 && which == 1))
			{
				cross = -cross;
			}


			srcplane->a = FLOAT2FIXED (cross[0]);
			srcplane->b = FLOAT2FIXED (cross[1]);
			srcplane->c = FLOAT2FIXED (cross[2]);
			//plane->ic = FLOAT2FIXED (1.f/cross[2]);
			srcplane->ic = DivScale32 (1, srcplane->c);
			srcplane->d = -TMulScale16 (srcplane->a, x,
										srcplane->b, y,
										srcplane->c, z);
			return;
		}
	}
}
				   
enum
{
	THING_SlopeFloorPointLine = 9500,
	THING_SlopeCeilingPointLine = 9501,
	THING_SetFloorSlope = 9502,
	THING_SetCeilingSlope = 9503,
	THING_CopyFloorPlane = 9510,
	THING_CopyCeilingPlane = 9511,
	THING_VavoomFloor=1500,
	THING_VavoomCeiling=1501,
	THING_VertexFloorZ=1504,
	THING_VertexCeilingZ=1505,
};

//==========================================================================
//
//	P_SetSlopesFromVertexHeights
//
//==========================================================================

static void P_SetSlopesFromVertexHeights(FMapThing *firstmt, FMapThing *lastmt, const int *oldvertextable)
{
	TMap<int, fixed_t> vt_heights[2];
	FMapThing *mt;
	bool vt_found = false;

	for (mt = firstmt; mt < lastmt; ++mt)
	{
		if (mt->type == THING_VertexFloorZ || mt->type == THING_VertexCeilingZ)
		{
			for(int i=0; i<numvertexes; i++)
			{
				if (vertexes[i].x == mt->x && vertexes[i].y == mt->y)
				{
					if (mt->type == THING_VertexFloorZ) 
					{
						vt_heights[0][i] = mt->z;
					}
					else 
					{
						vt_heights[1][i] = mt->z;
					}
					vt_found = true;
				}
			}
			mt->type = 0;
		}
	}

	for(int i = 0; i < numvertexdatas; i++)
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
	delete[] vertexdatas;
	vertexdatas = NULL;
	numvertexdatas = 0;

	if (vt_found)
	{
		for (int i = 0; i < numsectors; i++)
		{
			sector_t *sec = &sectors[i];
			if (sec->linecount != 3) continue;	// only works with triangular sectors

			FVector3 vt1, vt2, vt3, cross;
			FVector3 vec1, vec2;
			int vi1, vi2, vi3;

			vi1 = int(sec->lines[0]->v1 - vertexes);
			vi2 = int(sec->lines[0]->v2 - vertexes);
			vi3 = (sec->lines[1]->v1 == sec->lines[0]->v1 || sec->lines[1]->v1 == sec->lines[0]->v2)?
				int(sec->lines[1]->v2 - vertexes) : int(sec->lines[1]->v1 - vertexes);

			vt1.X = FIXED2FLOAT(vertexes[vi1].x);
			vt1.Y = FIXED2FLOAT(vertexes[vi1].y);
			vt2.X = FIXED2FLOAT(vertexes[vi2].x);
			vt2.Y = FIXED2FLOAT(vertexes[vi2].y);
			vt3.X = FIXED2FLOAT(vertexes[vi3].x);
			vt3.Y = FIXED2FLOAT(vertexes[vi3].y);

			for(int j=0; j<2; j++)
			{
				fixed_t *h1 = vt_heights[j].CheckKey(vi1);
				fixed_t *h2 = vt_heights[j].CheckKey(vi2);
				fixed_t *h3 = vt_heights[j].CheckKey(vi3);
				fixed_t z3;
				if (h1==NULL && h2==NULL && h3==NULL) continue;

				vt1.Z = FIXED2FLOAT(h1? *h1 : j==0? sec->GetPlaneTexZ(sector_t::floor) : sec->GetPlaneTexZ(sector_t::ceiling));
				vt2.Z = FIXED2FLOAT(h2? *h2 : j==0? sec->GetPlaneTexZ(sector_t::floor) : sec->GetPlaneTexZ(sector_t::ceiling));
				z3 = h3? *h3 : j==0? sec->GetPlaneTexZ(sector_t::floor) : sec->GetPlaneTexZ(sector_t::ceiling);
				vt3.Z = FIXED2FLOAT(z3);

				if (P_PointOnLineSide(vertexes[vi3].x, vertexes[vi3].y, sec->lines[0]) == 0)
				{
					vec1 = vt2 - vt3;
					vec2 = vt1 - vt3;
				}
				else
				{
					vec1 = vt1 - vt3;
					vec2 = vt2 - vt3;
				}

				FVector3 cross = vec1 ^ vec2;

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

				secplane_t *srcplane = j==0? &sec->floorplane : &sec->ceilingplane;

				srcplane->a = FLOAT2FIXED (cross[0]);
				srcplane->b = FLOAT2FIXED (cross[1]);
				srcplane->c = FLOAT2FIXED (cross[2]);
				srcplane->ic = DivScale32 (1, srcplane->c);
				srcplane->d = -TMulScale16 (srcplane->a, vertexes[vi3].x,
											srcplane->b, vertexes[vi3].y,
											srcplane->c, z3);
			}
		}
	}
}

//===========================================================================
//
// P_SpawnSlopeMakers
//
//===========================================================================

void P_SpawnSlopeMakers (FMapThing *firstmt, FMapThing *lastmt, const int *oldvertextable)
{
	FMapThing *mt;

	for (mt = firstmt; mt < lastmt; ++mt)
	{
		if ((mt->type >= THING_SlopeFloorPointLine &&
			 mt->type <= THING_SetCeilingSlope) ||
			mt->type == THING_VavoomFloor || mt->type == THING_VavoomCeiling)
		{
			fixed_t x, y, z;
			secplane_t *refplane;
			sector_t *sec;

			x = mt->x;
			y = mt->y;
			sec = P_PointInSector (x, y);
			if (mt->type & 1)
			{
				refplane = &sec->ceilingplane;
			}
			else
			{
				refplane = &sec->floorplane;
			}
			z = refplane->ZatPoint (x, y) + (mt->z);
			if (mt->type == THING_VavoomFloor || mt->type == THING_VavoomCeiling)
			{
				P_VavoomSlope(sec, mt->thingid, x, y, mt->z, mt->type & 1); 
			}
			else if (mt->type <= THING_SlopeCeilingPointLine)
			{ // THING_SlopeFloorPointLine and THING_SlopCeilingPointLine
				P_SlopeLineToPoint (mt->args[0], x, y, z, mt->type & 1);
			}
			else
			{ // THING_SetFloorSlope and THING_SetCeilingSlope
				P_SetSlope (refplane, mt->type & 1, mt->angle, mt->args[0], x, y, z);
			}
			mt->type = 0;
		}
	}

	for (mt = firstmt; mt < lastmt; ++mt)
	{
		if (mt->type == THING_CopyFloorPlane ||
			mt->type == THING_CopyCeilingPlane)
		{
			P_CopyPlane (mt->args[0], mt->x, mt->y, mt->type & 1);
			mt->type = 0;
		}
	}

	P_SetSlopesFromVertexHeights(firstmt, lastmt, oldvertextable);
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

static void P_AlignPlane (sector_t *sec, line_t *line, int which)
{
	sector_t *refsec;
	double bestdist;
	vertex_t *refvert = (*sec->lines)->v1;	// Shut up, GCC
	int i;
	line_t **probe;

	if (line->backsector == NULL)
		return;

	// Find furthest vertex from the reference line. It, along with the two ends
	// of the line, will define the plane.
	bestdist = 0;
	for (i = sec->linecount*2, probe = sec->lines; i > 0; i--)
	{
		double dist;
		vertex_t *vert;

		if (i & 1)
			vert = (*probe++)->v2;
		else
			vert = (*probe)->v1;
		dist = fabs((double(line->v1->y) - vert->y) * line->dx -
					(double(line->v1->x) - vert->x) * line->dy);

		if (dist > bestdist)
		{
			bestdist = dist;
			refvert = vert;
		}
	}

	refsec = line->frontsector == sec ? line->backsector : line->frontsector;

	FVector3 p, v1, v2, cross;

	const secplane_t *refplane;
	secplane_t *srcplane;
	fixed_t srcheight, destheight;

	refplane = (which == 0) ? &refsec->floorplane : &refsec->ceilingplane;
	srcplane = (which == 0) ? &sec->floorplane : &sec->ceilingplane;
	srcheight = (which == 0) ? sec->GetPlaneTexZ(sector_t::floor) : sec->GetPlaneTexZ(sector_t::ceiling);
	destheight = (which == 0) ? refsec->GetPlaneTexZ(sector_t::floor) : refsec->GetPlaneTexZ(sector_t::ceiling);

	p[0] = FIXED2FLOAT (line->v1->x);
	p[1] = FIXED2FLOAT (line->v1->y);
	p[2] = FIXED2FLOAT (destheight);
	v1[0] = FIXED2FLOAT (line->dx);
	v1[1] = FIXED2FLOAT (line->dy);
	v1[2] = 0;
	v2[0] = FIXED2FLOAT (refvert->x - line->v1->x);
	v2[1] = FIXED2FLOAT (refvert->y - line->v1->y);
	v2[2] = FIXED2FLOAT (srcheight - destheight);

	cross = (v1 ^ v2).Unit();

	// Fix backward normals
	if ((cross.Z < 0 && which == 0) || (cross.Z > 0 && which == 1))
	{
		cross = -cross;
	}

	srcplane->a = FLOAT2FIXED (cross[0]);
	srcplane->b = FLOAT2FIXED (cross[1]);
	srcplane->c = FLOAT2FIXED (cross[2]);
	//srcplane->ic = FLOAT2FIXED (1.f/cross[2]);
	srcplane->ic = DivScale32 (1, srcplane->c);
	srcplane->d = -TMulScale16 (srcplane->a, line->v1->x,
								srcplane->b, line->v1->y,
								srcplane->c, destheight);
}

//===========================================================================
//
// P_SetSlopes
//
//===========================================================================

void P_SetSlopes ()
{
	int i, s;

	for (i = 0; i < numlines; i++)
	{
		if (lines[i].special == Plane_Align)
		{
			lines[i].special = 0;
			if (lines[i].backsector != NULL)
			{
				// args[0] is for floor, args[1] is for ceiling
				//
				// As a special case, if args[1] is 0,
				// then args[0], bits 2-3 are for ceiling.
				for (s = 0; s < 2; s++)
				{
					int bits = lines[i].args[s] & 3;

					if (s == 1 && bits == 0)
						bits = (lines[i].args[0] >> 2) & 3;

					if (bits == 1)			// align front side to back
						P_AlignPlane (lines[i].frontsector, lines + i, s);
					else if (bits == 2)		// align back side to front
						P_AlignPlane (lines[i].backsector, lines + i, s);
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

void P_CopySlopes()
{
	for (int i = 0; i < numlines; i++)
	{
		if (lines[i].special == Plane_Copy)
		{
			// The args are used for the tags of sectors to copy:
			// args[0]: front floor
			// args[1]: front ceiling
			// args[2]: back floor
			// args[3]: back ceiling
			// args[4]: copy slopes from one side of the line to the other.
			lines[i].special = 0;
			for (int s = 0; s < (lines[i].backsector ? 4 : 2); s++)
			{
				if (lines[i].args[s])
					P_CopyPlane(lines[i].args[s], 
					(s & 2 ? lines[i].backsector : lines[i].frontsector), s & 1);
			}

			if (lines[i].backsector != NULL)
			{
				if ((lines[i].args[4] & 3) == 1)
				{
					lines[i].backsector->floorplane = lines[i].frontsector->floorplane;
				}
				else if ((lines[i].args[4] & 3) == 2)
				{
					lines[i].frontsector->floorplane = lines[i].backsector->floorplane;
				}
				if ((lines[i].args[4] & 12) == 4)
				{
					lines[i].backsector->ceilingplane = lines[i].frontsector->ceilingplane;
				}
				else if ((lines[i].args[4] & 12) == 8)
				{
					lines[i].frontsector->ceilingplane = lines[i].backsector->ceilingplane;
				}
			}
		}
	}
}


/*
** portals.cpp
** Everything that has to do with portals (both of the line and sector variety)
**
**---------------------------------------------------------------------------
** Copyright 2016 ZZYZX
** Copyright 2016 Christoph Oelckers
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
** There is no code here that is directly taken from Eternity
** although some similarities may be inevitable because it has to
** implement the same concepts.
*/


#include "p_local.h"
#include "p_blockmap.h"
#include "p_lnspec.h"
#include "r_bsp.h"
#include "r_segs.h"
#include "c_cvars.h"
#include "m_bbox.h"
#include "p_tags.h"
#include "farchive.h"
#include "v_text.h"
#include "a_sharedglobal.h"
#include "i_system.h"
#include "c_dispatch.h"
#include "p_maputl.h"
#include "p_spec.h"
#include "p_checkposition.h"

// simulation recurions maximum
CVAR(Int, sv_portal_recursions, 4, CVAR_ARCHIVE|CVAR_SERVERINFO)

FDisplacementTable Displacements;
FPortalBlockmap PortalBlockmap;

TArray<FLinePortal> linePortals;
TArray<FLinePortal*> linkedPortals;	// only the linked portals, this is used to speed up looking for them in P_CollectConnectedGroups.

//============================================================================
//
// This is used to mark processed portals for some collection functions.
//
//============================================================================

struct FPortalBits
{
	TArray<DWORD> data;

	void setSize(int num)
	{
		data.Resize((num + 31) / 32);
		clear();
	}

	void clear()
	{
		memset(&data[0], 0, data.Size()*sizeof(DWORD));
	}

	void setBit(int group)
	{
		data[group >> 5] |= (1 << (group & 31));
	}

	int getBit(int group)
	{
		return data[group >> 5] & (1 << (group & 31));
	}
};

//============================================================================
//
// BuildBlockmap
//
//============================================================================

static void BuildBlockmap()
{
	PortalBlockmap.Clear();
	PortalBlockmap.Create(bmapwidth, bmapheight);
	for (int y = 0; y < bmapheight; y++)
	{
		for (int x = 0; x < bmapwidth; x++)
		{
			int offset = y*bmapwidth + x;
			int *list = blockmaplump + *(blockmap + offset) + 1;
			FPortalBlock &block = PortalBlockmap(x, y);

			while (*list != -1)
			{
				line_t *ld = &lines[*list++];

				if (ld->isLinePortal())
				{
					PortalBlockmap.containsLines = true;
					block.portallines.Push(ld);
					block.neighborContainsLines = true;
					if (ld->getPortal()->mType == PORTT_LINKED) block.containsLinkedPortals = true;
					if (x > 0) PortalBlockmap(x - 1, y).neighborContainsLines = true;
					if (y > 0) PortalBlockmap(x, y - 1).neighborContainsLines = true;
					if (x < PortalBlockmap.dx - 1) PortalBlockmap(x + 1, y).neighborContainsLines = true;
					if (y < PortalBlockmap.dy - 1) PortalBlockmap(x, y + 1).neighborContainsLines = true;
				}
				else
				{

					bool yes = ld->frontsector->PortalIsLinked(sector_t::ceiling) || ld->frontsector->PortalIsLinked(sector_t::floor);
					if (ld->backsector)
					{
						yes |= ld->backsector->PortalIsLinked(sector_t::ceiling) || ld->backsector->PortalIsLinked(sector_t::floor);
					}
					block.containsLinkedPortals |= yes;
					PortalBlockmap.hasLinkedSectorPortals |= yes;

				}
			}
		}
	}
}

//===========================================================================
//
// FLinePortalTraverse :: AddLineIntercepts.
//
// Similar to AddLineIntercepts but checks the portal blockmap for line-to-line portals
//
//===========================================================================

void FLinePortalTraverse::AddLineIntercepts(int bx, int by)
{
	FPortalBlock &block = PortalBlockmap(bx, by);

	for (unsigned i = 0; i<block.portallines.Size(); i++)
	{
		line_t *ld = block.portallines[i];
		fixed_t frac;
		divline_t dl;

		if (ld->validcount == validcount) continue;	// already processed

		if (P_PointOnDivlineSidePrecise (ld->v1->x, ld->v1->y, &trace) ==
			P_PointOnDivlineSidePrecise (ld->v2->x, ld->v2->y, &trace))
		{
			continue;		// line isn't crossed
		}
		P_MakeDivline (ld, &dl);
		if (P_PointOnDivlineSidePrecise (trace.x, trace.y, &dl) != 0 ||
			P_PointOnDivlineSidePrecise (trace.x+trace.dx, trace.y+trace.dy, &dl) != 1)
		{
			continue;		// line isn't crossed from the front side
		}

		// hit the line
		P_MakeDivline(ld, &dl);
		frac = P_InterceptVector(&trace, &dl);
		if (frac < 0 || frac > FRACUNIT) continue;	// behind source

		intercept_t newintercept;

		newintercept.frac = frac;
		newintercept.isaline = true;
		newintercept.done = false;
		newintercept.d.line = ld;
		intercepts.Push(newintercept);
	}
}

//============================================================================
//
// Save a line portal for savegames.
//
//============================================================================

FArchive &operator<< (FArchive &arc, FLinePortal &port)
{
	arc << port.mOrigin
		<< port.mDestination
		<< port.mXDisplacement
		<< port.mYDisplacement
		<< port.mType
		<< port.mFlags
		<< port.mDefFlags
		<< port.mAlign;
	return arc;
}


//============================================================================
//
// finds the destination for a line portal for spawning
//
//============================================================================

static line_t *FindDestination(line_t *src, int tag)
{
	if (tag)
	{
		int lineno = -1;
		FLineIdIterator it(tag);

		while ((lineno = it.Next()) >= 0)
		{
			if (&lines[lineno] != src)
			{
				return &lines[lineno];
			}
		}
	}
	return NULL;
}


//============================================================================
//
//
//
//============================================================================

static void SetRotation(FLinePortal *port)
{
	line_t *dst = port->mDestination;
	line_t *line = port->mOrigin;
	double angle = atan2(dst->dy, dst->dx) - atan2(line->dy, line->dx) + M_PI;
	port->mSinRot = FLOAT2FIXED(sin(angle));
	port->mCosRot = FLOAT2FIXED(cos(angle));
	port->mAngleDiff = RAD2ANGLE(angle);
}

//============================================================================
//
// Spawns a single line portal
//
//============================================================================

void P_SpawnLinePortal(line_t* line)
{
	// portal destination is special argument #0
	line_t* dst = NULL;

	if (line->args[2] >= PORTT_VISUAL && line->args[2] <= PORTT_LINKED)
	{
		dst = FindDestination(line, line->args[0]);

		line->portalindex = linePortals.Reserve(1);
		FLinePortal *port = &linePortals.Last();

		memset(port, 0, sizeof(FLinePortal));
		port->mOrigin = line;
		port->mDestination = dst;
		port->mType = BYTE(line->args[2]);	// range check is done above.

		if (port->mType == PORTT_LINKED)
		{
			// Linked portals have no z-offset ever.
			port->mAlign = PORG_ABSOLUTE;
		}
		else
		{
			port->mAlign = BYTE(line->args[3] >= PORG_ABSOLUTE && line->args[3] <= PORG_CEILING ? line->args[3] : PORG_ABSOLUTE);
			if (port->mType == PORTT_INTERACTIVE)
			{
				// Due to the way z is often handled, these pose a major issue for parts of the code that needs to transparently handle interactive portals.
				Printf(TEXTCOLOR_RED "Warning: z-offsetting not allowed for interactive portals. Changing line %d to teleport-portal!\n", int(line - lines));
				port->mType = PORTT_TELEPORT;
			}
		}
		if (port->mDestination != NULL)
		{
			port->mDefFlags = port->mType == PORTT_VISUAL ? PORTF_VISIBLE : port->mType == PORTT_TELEPORT ? PORTF_TYPETELEPORT : PORTF_TYPEINTERACTIVE;
		}

		// Get the angle between the two linedefs, for rotating
		// orientation and velocity. Rotate 180 degrees, and flip
		// the position across the exit linedef, if reversed.
		SetRotation(port);
	}
	else if (line->args[2] == PORTT_LINKEDEE && line->args[0] == 0)
	{
		// EE-style portals require that the first line ID is identical and the first arg of the two linked linedefs are 0 and 1 respectively.

		int mytag = tagManager.GetFirstLineID(line);

		for (int i = 0; i < numlines; i++)
		{
			if (tagManager.GetFirstLineID(&lines[i]) == mytag && lines[i].args[0] == 1 && lines[i].special == Line_SetPortal)
			{
				line->portalindex = linePortals.Reserve(1);
				FLinePortal *port = &linePortals.Last();

				memset(port, 0, sizeof(FLinePortal));
				port->mOrigin = line;
				port->mDestination = &lines[i];
				port->mType = PORTT_LINKED;
				port->mAlign = PORG_ABSOLUTE;
				port->mDefFlags = PORTF_TYPEINTERACTIVE;
				SetRotation(port);

				// we need to create the backlink here, too.
				lines[i].portalindex = linePortals.Reserve(1);
				port = &linePortals.Last();

				memset(port, 0, sizeof(FLinePortal));
				port->mOrigin = &lines[i];
				port->mDestination = line;
				port->mType = PORTT_LINKED;
				port->mAlign = PORG_ABSOLUTE;
				port->mDefFlags = PORTF_TYPEINTERACTIVE;

				SetRotation(port);
			}
		}
	}
	else
	{
		// undefined type
		return;
	}
}

//============================================================================
//
// Update a line portal's state after all have been spawned
//
//============================================================================

void P_UpdatePortal(FLinePortal *port)
{
	if (port->mDestination == NULL)
	{
		// Portal has no destination: switch it off
		port->mFlags = 0;
	}
	else if (port->mDestination->getPortalDestination() != port->mOrigin)
	{
		//portal doesn't link back. This will be a simple teleporter portal.
		port->mFlags = port->mDefFlags & ~PORTF_INTERACTIVE;
		if (port->mType == PORTT_LINKED)
		{
			// this is illegal. Demote the type to TELEPORT
			port->mType = PORTT_TELEPORT;
			port->mDefFlags &= ~PORTF_INTERACTIVE;
		}
	}
	else
	{
		port->mFlags = port->mDefFlags;
		if (port->mType == PORTT_LINKED)
		{
			if (linePortals[port->mDestination->portalindex].mType != PORTT_LINKED)
			{
				port->mType = PORTT_INTERACTIVE;	// linked portals must be two-way.
			}
			else
			{
				port->mXDisplacement = port->mDestination->v2->x - port->mOrigin->v1->x;
				port->mYDisplacement = port->mDestination->v2->y - port->mOrigin->v1->y;
			}
		}
 	}
}

//============================================================================
//
// Collect a separate list of linked portals so that these can be
// processed faster without the simpler types interfering.
//
//============================================================================

void P_CollectLinkedPortals()
{
	linkedPortals.Clear();
	for (unsigned i = 0; i < linePortals.Size(); i++)
	{
		FLinePortal * port = &linePortals[i];
		if (port->mType == PORTT_LINKED)
		{
			linkedPortals.Push(port);
		}
	}
}

//============================================================================
//
// Post-process all line portals
//
//============================================================================

void P_FinalizePortals()
{
	for (unsigned i = 0; i < linePortals.Size(); i++)
	{
		FLinePortal * port = &linePortals[i];
		P_UpdatePortal(port);
	}
	P_CollectLinkedPortals();
	BuildBlockmap();
	P_CreateLinkedPortals();
}

//============================================================================
//
// Change the destination of a portal
//
//============================================================================

static bool ChangePortalLine(line_t *line, int destid)
{
	if (line->portalindex >= linePortals.Size()) return false;
	FLinePortal *port = &linePortals[line->portalindex];
	if (port->mType == PORTT_LINKED) return false;	// linked portals cannot be changed.
	if (destid == 0) port->mDestination = NULL;
	port->mDestination = FindDestination(line, destid);
	if (port->mDestination == NULL)
	{
		port->mFlags = 0;
	}
	else if (port->mType == PORTT_INTERACTIVE)
	{
		FLinePortal *portd = &linePortals[port->mDestination->portalindex];
		if (portd != NULL && portd->mType == PORTT_INTERACTIVE && portd->mDestination == line)
		{
			// this is a 2-way interactive portal
			port->mFlags = port->mDefFlags | PORTF_INTERACTIVE;
			portd->mFlags = portd->mDefFlags | PORTF_INTERACTIVE;
		}
		else
		{
			port->mFlags = port->mDefFlags;
		}
		SetRotation(portd);
	}
	SetRotation(port);
	return true;
}


//============================================================================
//
// Change the destination of a group of portals
//
//============================================================================

bool P_ChangePortal(line_t *ln, int thisid, int destid)
{
	int lineno;

	if (thisid == 0) return ChangePortalLine(ln, destid);
	FLineIdIterator it(thisid);
	bool res = false;
	while ((lineno = it.Next()) >= 0)
	{
		res |= ChangePortalLine(&lines[lineno], destid);
	}
	return res;
}

//============================================================================
//
// clears all portal dat for a new level start
//
//============================================================================

void P_ClearPortals()
{
	Displacements.Create(1);
	linePortals.Clear();
	linkedPortals.Clear();
}


inline int P_PointOnLineSideExplicit (fixed_t x, fixed_t y, fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{
	return DMulScale32 (y-y1, x2-x1, x1-x, y2-y1) > 0;
}

//============================================================================
//
// check if this line is between portal and the viewer. clip away if it is.
//
//============================================================================

inline int P_GetLineSide(fixed_t x, fixed_t y, const line_t *line)
{
	return DMulScale32(y - line->v1->y, line->dx, line->v1->x - x, line->dy);
}

bool P_ClipLineToPortal(line_t* line, line_t* portal, fixed_t viewx, fixed_t viewy, bool partial, bool samebehind)
{
	int behind1 = P_GetLineSide(line->v1->x, line->v1->y, portal);
	int behind2 = P_GetLineSide(line->v2->x, line->v2->y, portal);

	if (behind1 == 0 && behind2 == 0)
	{
		// The line is parallel to the portal and cannot possibly be visible.
		return true;
	}
	// If one point lies on the same straight line than the portal, the other vertex will determine sidedness alone.
	else if (behind2 == 0) behind2 = behind1;
	else if (behind1 == 0) behind1 = behind2;

	if (behind1 > 0 && behind2 > 0)
	{
		// The line is behind the portal, i.e. between viewer and portal line, and must be rejected
		return true;
	}
	else if (behind1 < 0 && behind2 < 0)
	{
		// The line is in front of the portal, i.e. the portal is between viewer and line. This line must not be rejected
		return false;
	}
	else
	{
		// The line intersects with the portal straight, so we need to do another check to see how both ends of the portal lie in relation to the viewer.
		int viewside = P_PointOnLineSidePrecise(viewx, viewy, line); 
		int p1side = P_GetLineSide(portal->v1->x, portal->v1->y, line);
		int p2side = P_GetLineSide(portal->v2->x, portal->v2->y, line);
		// Do the same handling of points on the portal straight than above.
		if (p1side == 0) p1side = p2side;
		else if (p2side == 0) p2side = p1side;
		p1side = p1side > 0;
		p2side = p2side > 0;
		// If the portal is on the other side of the line than the viewpoint, there is no possibility to see this line inside the portal.
		return (p1side == p2side && viewside != p1side);
	}
}

//============================================================================
//
// Translates a coordinate by a portal's displacement
//
//============================================================================

void P_TranslatePortalXY(line_t* src, fixed_t& x, fixed_t& y)
{
	if (!src) return;
	FLinePortal *port = src->getPortal();
	if (!port) return;

	// offsets from line
	fixed_t nposx = x - src->v1->x;
	fixed_t nposy = y - src->v1->y;

	// Rotate position along normal to match exit linedef
	fixed_t tx = FixedMul(nposx, port->mCosRot) - FixedMul(nposy, port->mSinRot);
	fixed_t ty = FixedMul(nposy, port->mCosRot) + FixedMul(nposx, port->mSinRot);

	tx += port->mDestination->v2->x;
	ty += port->mDestination->v2->y;

	x = tx;
	y = ty;
}

//============================================================================
//
// Translates a velocity vector by a portal's displacement
//
//============================================================================

void P_TranslatePortalVXVY(line_t* src, fixed_t& vx, fixed_t& vy)
{
	if (!src) return;
	FLinePortal *port = src->getPortal();
	if (!port) return;

	fixed_t orig_velx = vx;
	fixed_t orig_vely = vy;
	vx = FixedMul(orig_velx, port->mCosRot) - FixedMul(orig_vely, port->mSinRot);
	vy = FixedMul(orig_vely, port->mCosRot) + FixedMul(orig_velx, port->mSinRot);
}

//============================================================================
//
// Translates an angle by a portal's displacement
//
//============================================================================

void P_TranslatePortalAngle(line_t* src, angle_t& angle)
{
	if (!src) return;
	FLinePortal *port = src->getPortal();
	if (!port) return;
	angle += port->mAngleDiff;
}

//============================================================================
//
// Translates a z-coordinate by a portal's displacement
//
//============================================================================

void P_TranslatePortalZ(line_t* src, fixed_t& z)
{
	// args[2] = 0 - no adjustment
	// args[2] = 1 - adjust by floor difference
	// args[2] = 2 - adjust by ceiling difference

	// This cannot be precalculated because heights may change.
	line_t *dst = src->getPortalDestination();
	switch (src->getPortalAlignment())
	{
	case PORG_FLOOR:
		z = z - src->frontsector->floorplane.ZatPoint(src->v1->x, src->v1->y) + dst->frontsector->floorplane.ZatPoint(dst->v2->x, dst->v2->y);
		return;

	case PORG_CEILING:
		z = z - src->frontsector->ceilingplane.ZatPoint(src->v1->x, src->v1->y) + dst->frontsector->ceilingplane.ZatPoint(dst->v2->x, dst->v2->y);
		return;

	default:
		return;
	}
}

//============================================================================
//
// calculate shortest distance from a point (x,y) to a linedef
//
//============================================================================

fixed_t P_PointLineDistance(line_t* line, fixed_t x, fixed_t y)
{
	angle_t angle = R_PointToAngle2(0, 0, line->dx, line->dy);
	angle += ANGLE_180;

	fixed_t dx = line->v1->x - x;
	fixed_t dy = line->v1->y - y;

	fixed_t s = finesine[angle>>ANGLETOFINESHIFT];
	fixed_t c = finecosine[angle>>ANGLETOFINESHIFT];

	fixed_t d2x = FixedMul(dx, c) - FixedMul(dy, s);

	return abs(d2x);
}

void P_NormalizeVXVY(fixed_t& vx, fixed_t& vy)
{
	double _vx = FIXED2DBL(vx);
	double _vy = FIXED2DBL(vy);
	double len = sqrt(_vx*_vx+_vy*_vy);
	vx = FLOAT2FIXED(_vx/len);
	vy = FLOAT2FIXED(_vy/len);
}

//============================================================================
//
// P_GetOffsetPosition
//
// Offsets a given coordinate if the trace from the origin crosses an 
// interactive line-to-line portal.
//
//============================================================================

fixedvec2 P_GetOffsetPosition(fixed_t x, fixed_t y, fixed_t dx, fixed_t dy)
{
	fixedvec2 dest = { x + dx, y + dy };
	if (PortalBlockmap.containsLines)
	{
		fixed_t actx = x, acty = y;
		// Try some easily discoverable early-out first. If we know that the trace cannot possibly find a portal, this saves us from calling the traverser completely for vast parts of the map.
		if (dx < 128 * FRACUNIT && dy < 128 * FRACUNIT)
		{
			fixed_t blockx = GetSafeBlockX(actx - bmaporgx);
			fixed_t blocky = GetSafeBlockY(acty - bmaporgy);
			if (blockx < 0 || blocky < 0 || blockx >= bmapwidth || blocky >= bmapheight || !PortalBlockmap(blockx, blocky).neighborContainsLines) return dest;
		}

		FLinePortalTraverse it;
		bool repeat;
		do
		{
			it.init(actx, acty, dx, dy, PT_ADDLINES|PT_DELTA);
			intercept_t *in;

			repeat = false;
			while ((in = it.Next()))
			{
				// hit a portal line.
				line_t *line = in->d.line;
				FLinePortal *port = line->getPortal();

				// Teleport portals are intentionally ignored since skipping this stuff is their entire reason for existence.
				if (port->mFlags & PORTF_INTERACTIVE)
				{
					fixedvec2 hit = it.InterceptPoint(in);

					if (port->mType == PORTT_LINKED)
					{
						// optimized handling for linked portals where we only need to add an offset.
						hit.x += port->mXDisplacement;
						hit.y += port->mYDisplacement;
						dest.x += port->mXDisplacement;
						dest.y += port->mYDisplacement;
					}
					else
					{
						// interactive ones are more complex because the vector may be rotated.
						// Note: There is no z-translation here, there's just too much code in the engine that wouldn't be able to handle interactive portals with a height difference.
						P_TranslatePortalXY(line, hit.x, hit.y);
						P_TranslatePortalXY(line, dest.x, dest.y);
					}
					// update the fields, end this trace and restart from the new position
					dx = dest.x - hit.x;
					dy = dest.y - hit.y;
					repeat = true;
				}

				break;
			}
		} while (repeat);
	}
	return dest;
}


//============================================================================
//
// CollectSectors
//
// Collects all sectors that are connected to any sector belonging to a portal
// because they all will need the same displacement values
//
//============================================================================

static bool CollectSectors(int groupid, sector_t *origin)
{
	if (origin->PortalGroup != 0) return false;	// already processed
	origin->PortalGroup = groupid;

	TArray<sector_t *> list(16);
	list.Push(origin);

	for (unsigned i = 0; i < list.Size(); i++)
	{
		sector_t *sec = list[i];

		for (int j = 0; j < sec->linecount; j++)
		{
			line_t *line = sec->lines[j];
			sector_t *other = line->frontsector == sec ? line->backsector : line->frontsector;
			if (other != NULL && other != sec && other->PortalGroup != groupid)
			{
				other->PortalGroup = groupid;
				list.Push(other);
			}
		}
	}
	return true;
}


//============================================================================
//
// AddDisplacementForPortal
//
// Adds the displacement for one portal to the displacement array
// (one version for sector to sector plane, one for line to line portals)
//
// Note: Despite the similarities to Eternity's equivalent this is
// original code!
//
//============================================================================

static void AddDisplacementForPortal(AStackPoint *portal)
{
	int thisgroup = portal->Mate->Sector->PortalGroup;
	int othergroup = portal->Sector->PortalGroup;
	if (thisgroup == othergroup)
	{
		Printf("Portal between sectors %d and %d has both sides in same group and will be disabled\n", portal->Sector->sectornum, portal->Mate->Sector->sectornum);
		portal->special1 = portal->Mate->special1 = SKYBOX_PORTAL;
		return;
	}
	if (thisgroup <= 0 || thisgroup >= Displacements.size || othergroup <= 0 || othergroup >= Displacements.size)
	{
		Printf("Portal between sectors %d and %d has invalid group and will be disabled\n", portal->Sector->sectornum, portal->Mate->Sector->sectornum);
		portal->special1 = portal->Mate->special1 = SKYBOX_PORTAL;
		return;
	}

	FDisplacement & disp = Displacements(thisgroup, othergroup);
	if (!disp.isSet)
	{
		disp.pos.x = portal->scaleX;
		disp.pos.y = portal->scaleY;
		disp.isSet = true;
	}
	else
	{
		if (disp.pos.x != portal->scaleX || disp.pos.y != portal->scaleY)
		{
			Printf("Portal between sectors %d and %d has displacement mismatch and will be disabled\n", portal->Sector->sectornum, portal->Mate->Sector->sectornum);
			portal->special1 = portal->Mate->special1 = SKYBOX_PORTAL;
			return;
		}
	}
}


static void AddDisplacementForPortal(FLinePortal *portal)
{
	int thisgroup = portal->mOrigin->frontsector->PortalGroup;
	int othergroup = portal->mDestination->frontsector->PortalGroup;
	if (thisgroup == othergroup)
	{
		Printf("Portal between lines %d and %d has both sides in same group\n", int(portal->mOrigin-lines), int(portal->mDestination-lines));
		portal->mType = linePortals[portal->mDestination->portalindex].mType = PORTT_TELEPORT;
		return;
	}
	if (thisgroup <= 0 || thisgroup >= Displacements.size || othergroup <= 0 || othergroup >= Displacements.size)
	{
		Printf("Portal between lines %d and %d has invalid group\n", int(portal->mOrigin - lines), int(portal->mDestination - lines));
		portal->mType = linePortals[portal->mDestination->portalindex].mType = PORTT_TELEPORT;
		return;
	}

	FDisplacement & disp = Displacements(thisgroup, othergroup);
	if (!disp.isSet)
	{
		disp.pos.x = portal->mXDisplacement;
		disp.pos.y = portal->mYDisplacement;
		disp.isSet = true;
	}
	else
	{
		if (disp.pos.x != portal->mXDisplacement || disp.pos.y != portal->mYDisplacement)
		{
			Printf("Portal between lines %d and %d has displacement mismatch\n", int(portal->mOrigin - lines), int(portal->mDestination - lines));
			portal->mType = linePortals[portal->mDestination->portalindex].mType = PORTT_TELEPORT;
			return;
		}
	}
}

//============================================================================
//
// ConnectGroups
//
// Do the indirect connections. This loop will run until it cannot find any new connections
//
//============================================================================

static bool ConnectGroups()
{
	// Now 
	BYTE indirect = 1;
	bool bogus = false;
	bool changed;
	do
	{
		changed = false;
		for (int x = 1; x < Displacements.size; x++)
		{
			for (int y = 1; y < Displacements.size; y++)
			{
				FDisplacement &dispxy = Displacements(x, y);
				if (dispxy.isSet)
				{
					for (int z = 1; z < Displacements.size; z++)
					{
						FDisplacement &dispyz = Displacements(y, z);
						if (dispyz.isSet)
						{
							FDisplacement &dispxz = Displacements(x, z);
							if (dispxz.isSet)
							{
								if (dispxy.pos.x + dispyz.pos.x != dispxz.pos.x || dispxy.pos.y + dispyz.pos.y != dispxz.pos.y)
								{
									bogus = true;
								}
							}
							else
							{
								dispxz.pos = dispxy.pos + dispyz.pos;
								dispxz.isSet = true;
								dispxz.indirect = indirect;
								changed = true;
							}
						}
					}
				}
			}
		}
		indirect++;
	} while (changed);
	return bogus;
}


//============================================================================
//
// P_CreateLinkedPortals
//
// Creates the data structures needed for linked portals
// Removes portals from sloped sectors (as they cannot work on them)
// Group all sectors connected to one side of the portal
// Caclculate displacements between all created groups.
//
// Portals with the same offset but different anchors will not be merged.
//
//============================================================================

void P_CreateLinkedPortals()
{
	TThinkerIterator<AStackPoint> it;
	AStackPoint *mo;
	TArray<AStackPoint *> orgs;
	int id = 0;
	bool bogus = false;

	while ((mo = it.Next()))
	{
		if (mo->special1 == SKYBOX_LINKEDPORTAL)
		{
			if (mo->Mate != NULL)
			{
				orgs.Push(mo);
				mo->reactiontime = ++id;
			}
			else
			{
				// this should never happen, but if it does, the portal needs to be removed
				mo->Destroy();
			}
		}
	}
	if (orgs.Size() == 0)
	{
		// Create the 0->0 translation which is always needed.
		Displacements.Create(1);
		return;
	}
	for (int i = 0; i < numsectors; i++)
	{
		for (int j = 0; j < 2; j++)
		{
			AActor *box = sectors[i].SkyBoxes[j];
			if (box != NULL && box->special1 == SKYBOX_LINKEDPORTAL)
			{
				secplane_t &plane = j == 0 ? sectors[i].floorplane : sectors[i].ceilingplane;
				if (plane.a || plane.b)
				{
					// The engine cannot deal with portals on a sloped plane.
					sectors[i].SkyBoxes[j] = NULL;
					Printf("Portal on %s of sector %d is sloped and will be disabled\n", j==0? "floor":"ceiling", i);
				}
			}
		}
	}

	// Group all sectors, starting at each portal origin.
	id = 1;
	for (unsigned i = 0; i < orgs.Size(); i++)
	{
		if (CollectSectors(id, orgs[i]->Sector)) id++;
		if (CollectSectors(id, orgs[i]->Mate->Sector)) id++;
	}
	for (unsigned i = 0; i < linePortals.Size(); i++)
	{
		if (linePortals[i].mType == PORTT_LINKED)
		{
			if (CollectSectors(id, linePortals[i].mOrigin->frontsector)) id++;
			if (CollectSectors(id, linePortals[i].mDestination->frontsector)) id++;
		}
	}

	Displacements.Create(id);
	// Check for leftover sectors that connect to a portal
	for (int i = 0; i<numsectors; i++)
	{
		for (int j = 0; j < 2; j++)
		{
			ASkyViewpoint *box = barrier_cast<ASkyViewpoint*>(sectors[i].SkyBoxes[j]);
			if (box != NULL)
			{
				if (box->special1 == SKYBOX_LINKEDPORTAL && sectors[i].PortalGroup == 0)
				{
					// Note: the linked actor will be on the other side of the portal.
					// To get this side's group we will have to look at the mate object.
					CollectSectors(box->Mate->Sector->PortalGroup, &sectors[i]);
					// We cannot process the backlink here because all we can access is the anchor object
					// If necessary that will have to be done for the other side's portal.
				}
			}
		}
	}
	for (unsigned i = 0; i < orgs.Size(); i++)
	{
		AddDisplacementForPortal(orgs[i]);
	}
	for (unsigned i = 0; i < linePortals.Size(); i++)
	{
		if (linePortals[i].mType == PORTT_LINKED)
		{
			AddDisplacementForPortal(&linePortals[i]);
		}
	}

	for (int x = 1; x < Displacements.size; x++)
	{
		for (int y = x + 1; y < Displacements.size; y++)
		{
			FDisplacement &dispxy = Displacements(x, y);
			FDisplacement &dispyx = Displacements(y, x);
			if (dispxy.isSet && dispyx.isSet &&
				(dispxy.pos.x != -dispyx.pos.x || dispxy.pos.y != -dispyx.pos.y))
			{
				int sec1 = -1, sec2 = -1;
				for (int i = 0; i < numsectors && (sec1 == -1 || sec2 == -1); i++)
				{
					if (sec1 == -1 && sectors[i].PortalGroup == x)  sec1 = i;
					if (sec2 == -1 && sectors[i].PortalGroup == y)  sec2 = i;
				}
				Printf("Link offset mismatch between sectors %d and %d\n", sec1, sec2);
				bogus = true;
			}
			// mark everything that connects to a one-sided line
			for (int i = 0; i < numlines; i++)
			{
				if (lines[i].backsector == NULL && lines[i].frontsector->PortalGroup == 0)
				{
					CollectSectors(-1, lines[i].frontsector);
				}
			}
			// and now print a message for everything that still wasn't processed.
			for (int i = 0; i < numsectors; i++)
			{
				if (sectors[i].PortalGroup == 0)
				{
					Printf("Unable to assign sector %d to any group. Possibly self-referencing\n", i);
				}
				else if (sectors[i].PortalGroup == -1) sectors[i].PortalGroup = 0;
			}
		}
	}
	bogus |= ConnectGroups();
	if (bogus)
	{
		// todo: disable all portals whose offsets do not match the associated groups
	}

	// reject would just get in the way when checking sight through portals.
	if (rejectmatrix != NULL)
	{
		delete[] rejectmatrix;
		rejectmatrix = NULL;
	}
	// finally we must flag all planes which are obstructed by the sector's own ceiling or floor.
	for (int i = 0; i < numsectors; i++)
	{
		sectors[i].CheckPortalPlane(sector_t::floor);
		sectors[i].CheckPortalPlane(sector_t::ceiling);
		// set a flag on each line connecting to a plane portal sector. This is used to reduce the amount of checks in P_CheckSight.
		if (sectors[i].PortalIsLinked(sector_t::floor) || sectors[i].PortalIsLinked(sector_t::ceiling))
		{
			for (int j = 0; j < sectors[i].linecount; j++)
			{
				sectors[i].lines[j]->flags |= ML_PORTALCONNECT;
			}
		}
		if (sectors[i].PortalIsLinked(sector_t::ceiling) && sectors[i].PortalIsLinked(sector_t::floor))
		{
			fixed_t cz = sectors[i].SkyBoxes[sector_t::ceiling]->threshold;
			fixed_t fz = sectors[i].SkyBoxes[sector_t::floor]->threshold;
			if (cz < fz)
			{
				// This is a fatal condition. We have to remove one of the two portals. Choose the one that doesn't match the current plane
				Printf("Error in sector %d: Ceiling portal at z=%d is below floor portal at z=%d\n", i, cz, fz);
				fixed_t cp = sectors[i].ceilingplane.Zat0();
				fixed_t fp = sectors[i].ceilingplane.Zat0();
				if (cp < fp || fz == fp)
				{
					sectors[i].SkyBoxes[sector_t::ceiling] = NULL;
				}
				else
				{
					sectors[i].SkyBoxes[sector_t::floor] = NULL;
				}
			}
		}
	}
	if (linkedPortals.Size() > 0)
	{
		// We need to relink all actors that may touch a linked line portal
		TThinkerIterator<AActor> it;
		AActor *actor;
		while ((actor = it.Next()))
		{
			if (!(actor->flags & MF_NOBLOCKMAP))
			{
				FPortalGroupArray check(FPortalGroupArray::PGA_NoSectorPortals);
				P_CollectConnectedGroups(actor->Sector->PortalGroup, actor->Pos(), actor->Top(), actor->radius, check);
				if (check.Size() > 0)
				{
					actor->UnlinkFromWorld();
					actor->LinkToWorld();
				}
			}
		}
	}
}


//============================================================================
//
// Collect all portal groups this actor would occupy at the given position
// This is used to determine which parts of the map need to be checked.
//
//============================================================================

bool P_CollectConnectedGroups(int startgroup, const fixedvec3 &position, fixed_t upperz, fixed_t checkradius, FPortalGroupArray &out)
{
	// Keep this temporary work stuff static. This function can never be called recursively
	// and this would have to be reallocated for each call otherwise.
	static FPortalBits processMask;
	static TArray<FLinePortal*> foundPortals;
	static TArray<int> groupsToCheck;

	bool retval = false;
	out.inited = true;

	processMask.setSize(Displacements.size);
	if (Displacements.size == 1)
	{
		processMask.setBit(startgroup);
		return false;
	}

	if (linkedPortals.Size() != 0)
	{
		processMask.clear();
		foundPortals.Clear();

		int thisgroup = startgroup;
		processMask.setBit(thisgroup);
		//out.Add(thisgroup);

		for (unsigned i = 0; i < linkedPortals.Size(); i++)
		{
			line_t *ld = linkedPortals[i]->mOrigin;
			int othergroup = ld->frontsector->PortalGroup;
			FDisplacement &disp = Displacements(thisgroup, othergroup);
			if (!disp.isSet) continue;	// no connection.

			FBoundingBox box(position.x + disp.pos.x, position.y + disp.pos.y, checkradius);

			if (box.Right() <= ld->bbox[BOXLEFT]
				|| box.Left() >= ld->bbox[BOXRIGHT]
				|| box.Top() <= ld->bbox[BOXBOTTOM]
				|| box.Bottom() >= ld->bbox[BOXTOP])
				continue;	// not touched

			if (box.BoxOnLineSide(linkedPortals[i]->mOrigin) != -1) continue;	// not touched
			foundPortals.Push(linkedPortals[i]);
		}
		bool foundone = true;
		while (foundone)
		{
			foundone = false;
			for (int i = foundPortals.Size() - 1; i >= 0; i--)
			{
				if (processMask.getBit(foundPortals[i]->mOrigin->frontsector->PortalGroup) &&
					!processMask.getBit(foundPortals[i]->mDestination->frontsector->PortalGroup))
				{
					processMask.setBit(foundPortals[i]->mDestination->frontsector->PortalGroup);
					out.Add(foundPortals[i]->mDestination->frontsector->PortalGroup);
					foundone = true;
					retval = true;
					foundPortals.Delete(i);
				}
			}
		}
	}
	if (out.method != FPortalGroupArray::PGA_NoSectorPortals)
	{
		sector_t *sec = P_PointInSector(position.x, position.y);
		sector_t *wsec = sec;
		while (!wsec->PortalBlocksMovement(sector_t::ceiling) && upperz > wsec->SkyBoxes[sector_t::ceiling]->threshold)
		{
			sector_t *othersec = wsec->SkyBoxes[sector_t::ceiling]->Sector;
			fixedvec2 pos = Displacements.getOffset(startgroup, othersec->PortalGroup);
			fixed_t dx = position.x + pos.x;
			fixed_t dy = position.y + pos.y;
			processMask.setBit(othersec->PortalGroup);
			out.Add(othersec->PortalGroup | FPortalGroupArray::UPPER);
			wsec = P_PointInSector(dx, dy);	// get upper sector at the exact spot we want to check and repeat
			retval = true;
		}
		wsec = sec;
		while (!wsec->PortalBlocksMovement(sector_t::floor) && position.z < wsec->SkyBoxes[sector_t::floor]->threshold)
		{
			sector_t *othersec = wsec->SkyBoxes[sector_t::floor]->Sector;
			fixedvec2 pos = Displacements.getOffset(startgroup, othersec->PortalGroup);
			fixed_t dx = position.x + pos.x;
			fixed_t dy = position.y + pos.y;
			processMask.setBit(othersec->PortalGroup | FPortalGroupArray::LOWER);
			out.Add(othersec->PortalGroup);
			wsec = P_PointInSector(dx, dy);	// get lower sector at the exact spot we want to check and repeat
			retval = true;
		}
		if (out.method == FPortalGroupArray::PGA_Full3d && PortalBlockmap.hasLinkedSectorPortals)
		{
			groupsToCheck.Clear();
			groupsToCheck.Push(startgroup);
			int thisgroup = startgroup;
			for (unsigned i = 0; i < groupsToCheck.Size();i++)
			{
				fixedvec2 disp = Displacements.getOffset(startgroup, thisgroup & ~FPortalGroupArray::FLAT);
				FBoundingBox box(position.x + disp.x, position.y + disp.y, checkradius);
				FBlockLinesIterator it(box);
				line_t *ld;
				while ((ld = it.Next()))
				{
					if (box.Right() <= ld->bbox[BOXLEFT]
						|| box.Left() >= ld->bbox[BOXRIGHT]
						|| box.Top() <= ld->bbox[BOXBOTTOM]
						|| box.Bottom() >= ld->bbox[BOXTOP])
						continue;

					if (box.BoxOnLineSide(ld) != -1)
						continue;

					if (!(thisgroup & FPortalGroupArray::LOWER))
					{
						for (int s = 0; s < 2; s++)
						{
							sector_t *sec = s ? ld->backsector : ld->frontsector;
							if (sec && !(sec->PortalBlocksMovement(sector_t::ceiling)))
							{
								if (sec->SkyBoxes[sector_t::ceiling]->threshold < upperz)
								{
									int grp = sec->SkyBoxes[sector_t::ceiling]->Sector->PortalGroup;
									if (!(processMask.getBit(grp)))
									{
										processMask.setBit(grp);
										groupsToCheck.Push(grp | FPortalGroupArray::UPPER);
									}
								}
							}
						}
					}
					if (!(thisgroup & FPortalGroupArray::UPPER))
					{
						for (int s = 0; s < 2; s++)
						{
							sector_t *sec = s ? ld->backsector : ld->frontsector;
							if (sec && !(sec->PortalBlocksMovement(sector_t::floor)))
							{
								if (sec->SkyBoxes[sector_t::floor]->threshold > position.z)
								{
									int grp = sec->SkyBoxes[sector_t::floor]->Sector->PortalGroup;
									if (!(processMask.getBit(grp)))
									{
										processMask.setBit(grp);
										groupsToCheck.Push(grp | FPortalGroupArray::LOWER);
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return retval;
}


//============================================================================
//
// print the group link table to the console
//
//============================================================================

CCMD(dumplinktable)
{
	for (int x = 1; x < Displacements.size; x++)
	{
		for (int y = 1; y < Displacements.size; y++)
		{
			FDisplacement &disp = Displacements(x, y);
			Printf("%c%c(%6d, %6d)", TEXTCOLOR_ESCAPE, 'C' + disp.indirect, disp.pos.x >> FRACBITS, disp.pos.y >> FRACBITS);
		}
		Printf("\n");
	}
}





// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//		Movement, collision handling.
//		Shooting and aiming.
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <math.h>

#include "templates.h"

#include "m_bbox.h"
#include "m_random.h"
#include "i_system.h"
#include "c_dispatch.h"

#include "doomdef.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "p_effect.h"
#include "p_terrain.h"
#include "p_trace.h"

#include "s_sound.h"
#include "decallib.h"

// State.
#include "doomstat.h"
#include "r_state.h"

#include "gi.h"

#include "a_sharedglobal.h"
#include "p_conversation.h"
#include "r_data/r_translate.h"
#include "g_level.h"
#include "r_sky.h"

CVAR(Bool, cl_bloodsplats, true, CVAR_ARCHIVE)
CVAR(Int, sv_smartaim, 0, CVAR_ARCHIVE | CVAR_SERVERINFO)
CVAR(Bool, cl_doautoaim, false, CVAR_ARCHIVE)

static void CheckForPushSpecial(line_t *line, int side, AActor *mobj, bool windowcheck);
static void SpawnShootDecal(AActor *t1, const FTraceResults &trace);
static void SpawnDeepSplash(AActor *t1, const FTraceResults &trace, AActor *puff,
	fixed_t vx, fixed_t vy, fixed_t vz, fixed_t shootz, bool ffloor = false);

static FRandom pr_tracebleed("TraceBleed");
static FRandom pr_checkthing("CheckThing");
static FRandom pr_lineattack("LineAttack");
static FRandom pr_crunch("DoCrunch");

// keep track of special lines as they are hit,
// but don't process them until the move is proven valid
TArray<line_t *> spechit;

// Temporary holder for thing_sectorlist threads
msecnode_t* sector_list = NULL;		// phares 3/16/98

//==========================================================================
//
// GetCoefficientClosestPointInLine24
//
// Formula: (dotProduct(ldv1 - tm, ld) << 24) / dotProduct(ld, ld)
// with: ldv1 = (ld->v1->x, ld->v1->y), tm = (tm.x, tm.y)
// and ld = (ld->dx, ld->dy)
// Returns truncated to range [0, 1 << 24].
//
//==========================================================================

static fixed_t GetCoefficientClosestPointInLine24(line_t *ld, FCheckPosition &tm)
{
	// [EP] Use 64 bit integers in order to keep the exact result of the
	// multiplication, because in the case the vertexes have both the
	// distance coordinates equal to the map limit (32767 units, which is
	// 2147418112 in fixed_t notation), the product result would occupy
	// 62 bits and the sum of two products would occupy 63 bits
	// in the worst case. If instead the vertexes are very close (1 in
	// fixed_t notation, which is 1.52587890625e-05 in float notation), the
	// product and the sum can be 1 in the worst case, which is very tiny.

	SQWORD r_num = ((SQWORD(tm.x - ld->v1->x)*ld->dx) +
					(SQWORD(tm.y - ld->v1->y)*ld->dy));

	// The denominator is always positive. Use this to avoid useless
	// calculations.
	SQWORD r_den = (SQWORD(ld->dx)*ld->dx + SQWORD(ld->dy)*ld->dy);

	if (r_num <= 0) {
		// [EP] The numerator is less or equal to zero, hence the closest
		// point on the line is the first vertex. Truncate the result to 0.
		return 0;
	}

	if (r_num >= r_den) {
		// [EP] The division is greater or equal to 1, hence the closest
		// point on the line is the second vertex. Truncate the result to
		// 1 << 24.
		return (1 << 24);
	}

	// [EP] Deal with the limited bits. The original formula is:
	// r = (r_num << 24) / r_den,
	// but r_num might be big enough to make the shift overflow.
	// Since the numerator can't be saved in a 128bit integer,
	// the denominator must be right shifted. If the denominator is
	// less than (1 << 24), there would be a division by zero.
	// Thanks to the fact that in this code path the denominator is greater
	// than the numerator, it's possible to avoid this bad situation by
	// just checking the last 24 bits of the numerator.
	if ((r_num >> (63-24)) != 0) {
		// [EP] In fact, if the numerator is greater than
		// (1 << (63-24)), the denominator must be greater than
		// (1 << (63-24)), hence the denominator won't be zero after
		// the right shift by 24 places.
		return (fixed_t)(r_num/(r_den >> 24));
	}
	// [EP] Having the last 24 bits all zero allows left shifting
	// the numerator by 24 bits without overflow.
	return (fixed_t)((r_num << 24)/r_den);
}

//==========================================================================
//
// PIT_FindFloorCeiling
//
// only3d set means to only check against 3D floors and midtexes.
//
//==========================================================================

static bool PIT_FindFloorCeiling(line_t *ld, const FBoundingBox &box, FCheckPosition &tmf, int flags)
{
	if (box.Right() <= ld->bbox[BOXLEFT]
		|| box.Left() >= ld->bbox[BOXRIGHT]
		|| box.Top() <= ld->bbox[BOXBOTTOM]
		|| box.Bottom() >= ld->bbox[BOXTOP])
		return true;

	if (box.BoxOnLineSide(ld) != -1)
		return true;

	// A line has been hit

	if (!ld->backsector)
	{ // One sided line
		return true;
	}

	fixed_t sx, sy;
	FLineOpening open;

	// set openrange, opentop, openbottom
	if ((((ld->frontsector->floorplane.a | ld->frontsector->floorplane.b) |
		(ld->backsector->floorplane.a | ld->backsector->floorplane.b) |
		(ld->frontsector->ceilingplane.a | ld->frontsector->ceilingplane.b) |
		(ld->backsector->ceilingplane.a | ld->backsector->ceilingplane.b)) == 0)
		&& ld->backsector->e->XFloor.ffloors.Size() == 0 && ld->frontsector->e->XFloor.ffloors.Size() == 0)
	{
		P_LineOpening(open, tmf.thing, ld, sx = tmf.x, sy = tmf.y, tmf.x, tmf.y, flags);
	}
	else
	{ // Find the point on the line closest to the actor's center, and use
		// that to calculate openings
		double dx = ld->dx;
		double dy = ld->dy;
		fixed_t r = xs_CRoundToInt(((double)(tmf.x - ld->v1->x) * dx +
			(double)(tmf.y - ld->v1->y) * dy) /
			(dx*dx + dy*dy) * 16777216.f);
		if (r <= 0)
		{
			P_LineOpening(open, tmf.thing, ld, sx = ld->v1->x, sy = ld->v1->y, tmf.x, tmf.y, flags);
		}
		else if (r >= (1 << 24))
		{
			P_LineOpening(open, tmf.thing, ld, sx = ld->v2->x, sy = ld->v2->y, tmf.thing->X(), tmf.thing->Y(), flags);
		}
		else
		{
			P_LineOpening(open, tmf.thing, ld, sx = ld->v1->x + MulScale24(r, ld->dx),
				sy = ld->v1->y + MulScale24(r, ld->dy), tmf.x, tmf.y, flags);
		}
	}

	// adjust floor / ceiling heights
	if (open.top < tmf.ceilingz)
	{
		tmf.ceilingz = open.top;
	}

	if (open.bottom > tmf.floorz)
	{
		tmf.floorz = open.bottom;
		if (open.bottomsec != NULL) tmf.floorsector = open.bottomsec;
		tmf.touchmidtex = open.touchmidtex;
		tmf.abovemidtex = open.abovemidtex;
	}
	else if (open.bottom == tmf.floorz)
	{
		tmf.touchmidtex |= open.touchmidtex;
		tmf.abovemidtex |= open.abovemidtex;
	}

	if (open.lowfloor < tmf.dropoffz)
		tmf.dropoffz = open.lowfloor;

	return true;
}


//==========================================================================
//
//
//
//==========================================================================

void P_GetFloorCeilingZ(FCheckPosition &tmf, int flags)
{
	sector_t *sec;
	if (!(flags & FFCF_ONLYSPAWNPOS))
	{
		sec = !(flags & FFCF_SAMESECTOR) ? P_PointInSector(tmf.x, tmf.y) : tmf.thing->Sector;
		tmf.floorsector = sec;
		tmf.ceilingsector = sec;

		tmf.floorz = tmf.dropoffz = sec->floorplane.ZatPoint(tmf.x, tmf.y);
		tmf.ceilingz = sec->ceilingplane.ZatPoint(tmf.x, tmf.y);
		tmf.floorpic = sec->GetTexture(sector_t::floor);
		tmf.floorterrain = sec->GetTerrain(sector_t::floor);
		tmf.ceilingpic = sec->GetTexture(sector_t::ceiling);
	}
	else
	{
		sec = tmf.thing->Sector;
	}

	for (unsigned int i = 0; i<sec->e->XFloor.ffloors.Size(); i++)
	{
		F3DFloor*  rover = sec->e->XFloor.ffloors[i];

		if (!(rover->flags & FF_SOLID) || !(rover->flags & FF_EXISTS)) continue;

		fixed_t ff_bottom = rover->bottom.plane->ZatPoint(tmf.x, tmf.y);
		fixed_t ff_top = rover->top.plane->ZatPoint(tmf.x, tmf.y);

		if (ff_top > tmf.floorz)
		{
			if (ff_top <= tmf.z || (!(flags & FFCF_3DRESTRICT) && (tmf.thing != NULL && ff_bottom < tmf.z && ff_top < tmf.z + tmf.thing->MaxStepHeight)))
			{
				tmf.dropoffz = tmf.floorz = ff_top;
				tmf.floorpic = *rover->top.texture;
				tmf.floorterrain = rover->model->GetTerrain(rover->top.isceiling);
			}
		}
		if (ff_bottom <= tmf.ceilingz && ff_bottom > tmf.z + tmf.thing->height)
		{
			tmf.ceilingz = ff_bottom;
			tmf.ceilingpic = *rover->bottom.texture;
		}
	}
}

//==========================================================================
//
// P_FindFloorCeiling
//
//==========================================================================

void P_FindFloorCeiling(AActor *actor, int flags)
{
	FCheckPosition tmf;

	tmf.thing = actor;
	tmf.x = actor->X();
	tmf.y = actor->Y();
	tmf.z = actor->Z();

	if (flags & FFCF_ONLYSPAWNPOS)
	{
		flags |= FFCF_3DRESTRICT;
	}
	if (!(flags & FFCF_ONLYSPAWNPOS))
	{
		P_GetFloorCeilingZ(tmf, flags);
	}
	else
	{
		tmf.ceilingsector = tmf.floorsector = actor->Sector;

		tmf.floorz = tmf.dropoffz = actor->floorz;
		tmf.ceilingz = actor->ceilingz;
		tmf.floorpic = actor->floorpic;
		tmf.floorterrain = actor->floorterrain;
		tmf.ceilingpic = actor->ceilingpic;
		P_GetFloorCeilingZ(tmf, flags);
	}
	actor->floorz = tmf.floorz;
	actor->dropoffz = tmf.dropoffz;
	actor->ceilingz = tmf.ceilingz;
	actor->floorpic = tmf.floorpic;
	actor->floorterrain = tmf.floorterrain;
	actor->floorsector = tmf.floorsector;
	actor->ceilingpic = tmf.ceilingpic;
	actor->ceilingsector = tmf.ceilingsector;

	FBoundingBox box(tmf.x, tmf.y, actor->radius);

	tmf.touchmidtex = false;
	tmf.abovemidtex = false;
	validcount++;

	FBlockLinesIterator it(box);
	line_t *ld;

	while ((ld = it.Next()))
	{
		PIT_FindFloorCeiling(ld, box, tmf, flags);
	}

	if (tmf.touchmidtex) tmf.dropoffz = tmf.floorz;

	if (!(flags & FFCF_ONLYSPAWNPOS) || (tmf.abovemidtex && (tmf.floorz <= actor->Z())))
	{
		actor->floorz = tmf.floorz;
		actor->dropoffz = tmf.dropoffz;
		actor->ceilingz = tmf.ceilingz;
		actor->floorpic = tmf.floorpic;
		actor->floorterrain = tmf.floorterrain;
		actor->floorsector = tmf.floorsector;
		actor->ceilingpic = tmf.ceilingpic;
		actor->ceilingsector = tmf.ceilingsector;
	}
	else
	{
		actor->floorsector = actor->ceilingsector = actor->Sector;
		// [BB] Don't forget to update floorpic and ceilingpic.
		if (actor->Sector != NULL)
		{
			actor->floorpic = actor->Sector->GetTexture(sector_t::floor);
			actor->floorterrain = actor->Sector->GetTerrain(sector_t::floor);
			actor->ceilingpic = actor->Sector->GetTexture(sector_t::ceiling);
		}
	}
}

//==========================================================================
//
// TELEPORT MOVE
// 

//
// P_TeleportMove
//
// [RH] Added telefrag parameter: When true, anything in the spawn spot
//		will always be telefragged, and the move will be successful.
//		Added z parameter. Originally, the thing's z was set *after* the
//		move was made, so the height checking I added for 1.13 could
//		potentially erroneously indicate the move was okay if the thing
//		was being teleported between two non-overlapping height ranges.
//
//==========================================================================

bool P_TeleportMove(AActor *thing, fixed_t x, fixed_t y, fixed_t z, bool telefrag, bool modifyactor)
{
	FCheckPosition tmf;
	sector_t *oldsec = thing->Sector;

	// kill anything occupying the position


	// The base floor/ceiling is from the subsector that contains the point.
	// Any contacted lines the step closer together will adjust them.
	tmf.thing = thing;
	tmf.x = x;
	tmf.y = y;
	tmf.z = z;
	tmf.touchmidtex = false;
	tmf.abovemidtex = false;
	P_GetFloorCeilingZ(tmf, 0);

	spechit.Clear();

	bool StompAlwaysFrags = ((thing->flags2 & MF2_TELESTOMP) || (level.flags & LEVEL_MONSTERSTELEFRAG) || telefrag) && !(thing->flags7 & MF7_NOTELESTOMP);

	FBoundingBox box(x, y, thing->radius);
	FBlockLinesIterator it(box);
	line_t *ld;

	// P_LineOpening requires the thing's z to be the destination z in order to work.
	fixed_t savedz = thing->Z();
	thing->SetZ(z);
	while ((ld = it.Next()))
	{
		PIT_FindFloorCeiling(ld, box, tmf, 0);
	}
	thing->SetZ(savedz);

	if (tmf.touchmidtex) tmf.dropoffz = tmf.floorz;

	FBlockThingsIterator it2(FBoundingBox(x, y, thing->radius));
	AActor *th;

	while ((th = it2.Next()))
	{
		if (!(th->flags & MF_SHOOTABLE))
			continue;

		// don't clip against self
		if (th == thing)
			continue;

		fixed_t blockdist = th->radius + tmf.thing->radius;
		if (abs(th->X() - tmf.x) >= blockdist || abs(th->Y() - tmf.y) >= blockdist)
			continue;

		if ((th->flags2 | tmf.thing->flags2) & MF2_THRUACTORS)
			continue;

		if (tmf.thing->flags6 & MF6_THRUSPECIES && tmf.thing->GetSpecies() == th->GetSpecies())
			continue;

		// [RH] Z-Check
		// But not if not MF2_PASSMOBJ or MF3_DONTOVERLAP are set!
		// Otherwise those things would get stuck inside each other.
		if ((thing->flags2 & MF2_PASSMOBJ || th->flags4 & MF4_ACTLIKEBRIDGE) && !(i_compatflags & COMPATF_NO_PASSMOBJ))
		{
			if (!(th->flags3 & thing->flags3 & MF3_DONTOVERLAP))
			{
				if (z > th->Top() ||	// overhead
					z + thing->height < th->Z())	// underneath
					continue;
			}
		}

		// monsters don't stomp things except on boss level
		// [RH] Some Heretic/Hexen monsters can telestomp
		// ... and some items can never be telefragged while others will be telefragged by everything that teleports upon them.
		if ((StompAlwaysFrags && !(th->flags6 & MF6_NOTELEFRAG)) || (th->flags7 & MF7_ALWAYSTELEFRAG))
		{
			// Don't actually damage if predicting a teleport
			if (thing->player == NULL || !(thing->player->cheats & CF_PREDICTING))
				P_DamageMobj(th, thing, thing, TELEFRAG_DAMAGE, NAME_Telefrag, DMG_THRUSTLESS);
			continue;
		}
		return false;
	}

	if (modifyactor)
	{
		// the move is ok, so link the thing into its new position
		thing->SetOrigin(x, y, z, false);
		thing->floorz = tmf.floorz;
		thing->ceilingz = tmf.ceilingz;
		thing->floorsector = tmf.floorsector;
		thing->floorpic = tmf.floorpic;
		thing->floorterrain = tmf.floorterrain;
		thing->ceilingsector = tmf.ceilingsector;
		thing->ceilingpic = tmf.ceilingpic;
		thing->dropoffz = tmf.dropoffz;        // killough 11/98
		thing->BlockingLine = NULL;

		if (thing->flags2 & MF2_FLOORCLIP)
		{
			thing->AdjustFloorClip();
		}

		if (thing == players[consoleplayer].camera)
		{
			R_ResetViewInterpolation();
		}

		thing->PrevX = x;
		thing->PrevY = y;
		thing->PrevZ = z;

		// If this teleport was caused by a move, P_TryMove() will handle the
		// sector transition messages better than we can here.
		if (!(thing->flags6 & MF6_INTRYMOVE))
		{
			thing->CheckSectorTransition(oldsec);
		}
	}

	return true;
}

//==========================================================================
//
// [RH] P_PlayerStartStomp
//
// Like P_TeleportMove, but it doesn't move anything, and only monsters and other
// players get telefragged.
//
//==========================================================================

void P_PlayerStartStomp(AActor *actor, bool mononly)
{
	AActor *th;
	FBlockThingsIterator it(FBoundingBox(actor->X(), actor->Y(), actor->radius));

	while ((th = it.Next()))
	{
		if (!(th->flags & MF_SHOOTABLE))
			continue;

		// don't clip against self, and don't kill your own voodoo dolls
		if (th == actor || (th->player == actor->player && th->player != NULL))
			continue;

		if (!th->intersects(actor))
			continue;

		// only kill monsters and other players
		if (th->player == NULL && !(th->flags3 & MF3_ISMONSTER))
			continue;

		if (th->player != NULL && mononly)
			continue;

		if (actor->Z() > th->Top())
			continue;        // overhead
		if (actor->Top() < th->Z())
			continue;        // underneath

		P_DamageMobj(th, actor, actor, TELEFRAG_DAMAGE, NAME_Telefrag);
	}
}

//==========================================================================
//
//
//
//==========================================================================

inline fixed_t secfriction(const sector_t *sec, int plane = sector_t::floor)
{
	if (sec->Flags & SECF_FRICTION) return sec->friction;
	fixed_t friction = Terrains[sec->GetTerrain(plane)].Friction;
	return friction != 0 ? friction : ORIG_FRICTION;
}

inline fixed_t secmovefac(const sector_t *sec, int plane = sector_t::floor)
{
	if (sec->Flags & SECF_FRICTION) return sec->movefactor;
	fixed_t movefactor = Terrains[sec->GetTerrain(plane)].MoveFactor;
	return movefactor != 0 ? movefactor : ORIG_FRICTION_FACTOR;
}

//==========================================================================
//
// killough 8/28/98:
//
// P_GetFriction()
//
// Returns the friction associated with a particular mobj.
//
//==========================================================================

int P_GetFriction(const AActor *mo, int *frictionfactor)
{
	int friction = ORIG_FRICTION;
	int movefactor = ORIG_FRICTION_FACTOR;
	fixed_t newfriction;
	const msecnode_t *m;
	const sector_t *sec;

	if (mo->IsNoClip2())
	{
		// The default values are fine for noclip2 mode
	}
	else if (mo->flags2 & MF2_FLY && mo->flags & MF_NOGRAVITY)
	{
		friction = FRICTION_FLY;
	}
	else if ((!(mo->flags & MF_NOGRAVITY) && mo->waterlevel > 1) ||
		(mo->waterlevel == 1 && mo->Z() > mo->floorz + 6 * FRACUNIT))
	{
		friction = secfriction(mo->Sector);
		movefactor = secmovefac(mo->Sector) >> 1;

			// Check 3D floors -- might be the source of the waterlevel
			for (unsigned i = 0; i < mo->Sector->e->XFloor.ffloors.Size(); i++)
			{
				F3DFloor *rover = mo->Sector->e->XFloor.ffloors[i];
				if (!(rover->flags & FF_EXISTS)) continue;
				if (!(rover->flags & FF_SWIMMABLE)) continue;

				if (mo->Z() > rover->top.plane->ZatPoint(mo) ||
					mo->Z() < rover->bottom.plane->ZatPoint(mo))
					continue;

				newfriction = secfriction(rover->model, rover->top.isceiling);
				if (newfriction < friction || friction == ORIG_FRICTION)
				{
					friction = newfriction;
					movefactor = secmovefac(rover->model, rover->top.isceiling) >> 1;
				}
			}
	}
	else if (var_friction && !(mo->flags & (MF_NOCLIP | MF_NOGRAVITY)))
	{	// When the object is straddling sectors with the same
		// floor height that have different frictions, use the lowest
		// friction value (muddy has precedence over icy).

		for (m = mo->touching_sectorlist; m; m = m->m_tnext)
		{
			sec = m->m_sector;

			// 3D floors must be checked, too
			for (unsigned i = 0; i < sec->e->XFloor.ffloors.Size(); i++)
			{
				F3DFloor *rover = sec->e->XFloor.ffloors[i];
				if (!(rover->flags & FF_EXISTS)) continue;

				if (rover->flags & FF_SOLID)
				{
					// Must be standing on a solid floor
					if (mo->Z() != rover->top.plane->ZatPoint(mo)) continue;
				}
				else if (rover->flags & FF_SWIMMABLE)
				{
					// Or on or inside a swimmable floor (e.g. in shallow water)
					if (mo->Z() > rover->top.plane->ZatPoint(mo) ||
						(mo->Top()) < rover->bottom.plane->ZatPoint(mo))
						continue;
				}
				else
					continue;

				newfriction = secfriction(rover->model, rover->top.isceiling);
				if (newfriction < friction || friction == ORIG_FRICTION)
				{
					friction = newfriction;
					movefactor = secmovefac(rover->model, rover->top.isceiling);
				}
			}

			if (!(sec->Flags & SECF_FRICTION) &&
				Terrains[sec->GetTerrain(sector_t::floor)].Friction == 0)
			{
				continue;
			}
			newfriction = secfriction(sec);
			if ((newfriction < friction || friction == ORIG_FRICTION) &&
				(mo->Z() <= sec->floorplane.ZatPoint(mo) ||
				(sec->GetHeightSec() != NULL &&
				mo->Z() <= sec->heightsec->floorplane.ZatPoint(mo))))
			{
				friction = newfriction;
				movefactor = secmovefac(sec);
			}
		}
	}

	if (mo->Friction != FRACUNIT)
	{
		friction = clamp(FixedMul(friction, mo->Friction), 0, FRACUNIT);
		movefactor = FrictionToMoveFactor(friction);
	}

	if (frictionfactor)
		*frictionfactor = movefactor;

	return friction;
}

//==========================================================================
//
// phares 3/19/98
// P_GetMoveFactor() returns the value by which the x,y
// movements are multiplied to add to player movement.
//
// killough 8/28/98: rewritten
//
//==========================================================================

int P_GetMoveFactor(const AActor *mo, int *frictionp)
{
	int movefactor, friction;

	// If the floor is icy or muddy, it's harder to get moving. This is where
	// the different friction factors are applied to 'trying to move'. In
	// p_mobj.c, the friction factors are applied as you coast and slow down.

	if ((friction = P_GetFriction(mo, &movefactor)) < ORIG_FRICTION)
	{
		// phares 3/11/98: you start off slowly, then increase as
		// you get better footing

		int velocity = P_AproxDistance(mo->velx, mo->vely);

		if (velocity > MORE_FRICTION_VELOCITY << 2)
			movefactor <<= 3;
		else if (velocity > MORE_FRICTION_VELOCITY << 1)
			movefactor <<= 2;
		else if (velocity > MORE_FRICTION_VELOCITY)
			movefactor <<= 1;
	}

	if (frictionp)
		*frictionp = friction;

	return movefactor;
}

//
// MOVEMENT ITERATOR FUNCTIONS
//

//==========================================================================
//
//
// PIT_CheckLine
// Adjusts tmfloorz and tmceilingz as lines are contacted
//
//
//==========================================================================

static // killough 3/26/98: make static
bool PIT_CheckLine(line_t *ld, const FBoundingBox &box, FCheckPosition &tm)
{
	bool rail = false;

	if (box.Right() <= ld->bbox[BOXLEFT]
		|| box.Left() >= ld->bbox[BOXRIGHT]
		|| box.Top() <= ld->bbox[BOXBOTTOM]
		|| box.Bottom() >= ld->bbox[BOXTOP])
		return true;

	if (box.BoxOnLineSide(ld) != -1)
		return true;

	// A line has been hit
	/*
	=
	= The moving thing's destination position will cross the given line.
	= If this should not be allowed, return false.
	= If the line is special, keep track of it to process later if the move
	=       is proven ok.  NOTE: specials are NOT sorted by order, so two special lines
	=       that are only 8 pixels apart could be crossed in either order.
	*/

	if (!ld->backsector)
	{ // One sided line
		if (tm.thing->flags2 & MF2_BLASTED)
		{
			P_DamageMobj(tm.thing, NULL, NULL, tm.thing->Mass >> 5, NAME_Melee);
		}
		tm.thing->BlockingLine = ld;
		CheckForPushSpecial(ld, 0, tm.thing, false);
		return false;
	}

	// MBF bouncers are treated as missiles here.
	bool Projectile = (tm.thing->flags & MF_MISSILE || tm.thing->BounceFlags & BOUNCE_MBF);
	// MBF considers that friendly monsters are not blocked by monster-blocking lines.
	// This is added here as a compatibility option. Note that monsters that are dehacked
	// into being friendly with the MBF flag automatically gain MF3_NOBLOCKMONST, so this
	// just optionally generalizes the behavior to other friendly monsters.
	bool NotBlocked = ((tm.thing->flags3 & MF3_NOBLOCKMONST)
		|| ((i_compatflags & COMPATF_NOBLOCKFRIENDS) && (tm.thing->flags & MF_FRIENDLY)));

	fixedvec3 pos = tm.thing->PosRelative(ld);
	if (!(Projectile) || (ld->flags & (ML_BLOCKEVERYTHING | ML_BLOCKPROJECTILE)))
	{
		if (ld->flags & ML_RAILING)
		{
			rail = true;
		}
		else if ((ld->flags & (ML_BLOCKING | ML_BLOCKEVERYTHING)) || 				// explicitly blocking everything
			(!(NotBlocked) && (ld->flags & ML_BLOCKMONSTERS)) || 				// block monsters only
			(tm.thing->player != NULL && (ld->flags & ML_BLOCK_PLAYERS)) ||		// block players
			((Projectile) && (ld->flags & ML_BLOCKPROJECTILE)) ||				// block projectiles
			((tm.thing->flags & MF_FLOAT) && (ld->flags & ML_BLOCK_FLOATERS)))	// block floaters
		{
			if (tm.thing->flags2 & MF2_BLASTED)
			{
				P_DamageMobj(tm.thing, NULL, NULL, tm.thing->Mass >> 5, NAME_Melee);
			}
			tm.thing->BlockingLine = ld;
			// Calculate line side based on the actor's original position, not the new one.
			CheckForPushSpecial(ld, P_PointOnLineSide(pos.x, pos.y, ld), tm.thing, false);
			return false;
		}
	}

	// [RH] Steep sectors count as dropoffs (unless already in one)
	if (!(tm.thing->flags & MF_DROPOFF) &&
		!(tm.thing->flags & (MF_NOGRAVITY | MF_NOCLIP)))
	{
		secplane_t frontplane, backplane;
		// Check 3D floors as well
		frontplane = P_FindFloorPlane(ld->frontsector, pos.x, pos.y, tm.thing->floorz);
		backplane = P_FindFloorPlane(ld->backsector, pos.x, pos.y, tm.thing->floorz);
		if (frontplane.c < STEEPSLOPE || backplane.c < STEEPSLOPE)
		{
			const msecnode_t *node = tm.thing->touching_sectorlist;
			bool allow = false;
			int count = 0;
			while (node != NULL)
			{
				count++;
				if (node->m_sector->floorplane.c < STEEPSLOPE)
				{
					allow = true;
					break;
				}
				node = node->m_tnext;
			}
			if (!allow)
			{
				return false;
			}
		}
	}

	fixed_t sx = 0, sy = 0;
	FLineOpening open;

	// set openrange, opentop, openbottom
	if ((((ld->frontsector->floorplane.a | ld->frontsector->floorplane.b) |
		(ld->backsector->floorplane.a | ld->backsector->floorplane.b) |
		(ld->frontsector->ceilingplane.a | ld->frontsector->ceilingplane.b) |
		(ld->backsector->ceilingplane.a | ld->backsector->ceilingplane.b)) == 0)
		&& ld->backsector->e->XFloor.ffloors.Size() == 0 && ld->frontsector->e->XFloor.ffloors.Size() == 0)
	{
		P_LineOpening(open, tm.thing, ld, sx = tm.x, sy = tm.y, tm.x, tm.y);
	}
	else
	{ // Find the point on the line closest to the actor's center, and use
		// that to calculate openings
		fixed_t r = GetCoefficientClosestPointInLine24(ld, tm);

		/*		Printf ("%d:%d: %d  (%d %d %d %d)  (%d %d %d %d)\n", level.time, ld-lines, r,
		ld->frontsector->floorplane.a,
		ld->frontsector->floorplane.b,
		ld->frontsector->floorplane.c,
		ld->frontsector->floorplane.ic,
		ld->backsector->floorplane.a,
		ld->backsector->floorplane.b,
		ld->backsector->floorplane.c,
		ld->backsector->floorplane.ic);*/
		if (r <= 0)
		{
			P_LineOpening(open, tm.thing, ld, sx = ld->v1->x, sy = ld->v1->y, tm.x, tm.y);
		}
		else if (r >= (1 << 24))
		{
			P_LineOpening(open, tm.thing, ld, sx = ld->v2->x, sy = ld->v2->y, pos.x, pos.y);
		}
		else
		{
			P_LineOpening(open, tm.thing, ld, sx = ld->v1->x + MulScale24(r, ld->dx),
				sy = ld->v1->y + MulScale24(r, ld->dy), tm.x, tm.y);
		}

		// the floorplane on both sides is identical with the current one
		// so don't mess around with the z-position
		if (ld->frontsector->floorplane == ld->backsector->floorplane &&
			ld->frontsector->floorplane == tm.thing->Sector->floorplane &&
			!ld->frontsector->e->XFloor.ffloors.Size() && !ld->backsector->e->XFloor.ffloors.Size() &&
			!open.abovemidtex)
		{
			open.bottom = INT_MIN;
		}
		/*	Printf ("    %d %d %d\n", sx, sy, openbottom);*/
	}

	if (rail &&
		// Eww! Gross! This check means the rail only exists if you stand on the
		// high side of the rail. So if you're walking on the low side of the rail,
		// it's possible to get stuck in the rail until you jump out. Unfortunately,
		// there is an area on Strife MAP04 that requires this behavior. Still, it's
		// better than Strife's handling of rails, which lets you jump into rails
		// from either side. How long until somebody reports this as a bug and I'm
		// forced to say, "It's not a bug. It's a feature?" Ugh.
		(!(level.flags2 & LEVEL2_RAILINGHACK) ||
		open.bottom == tm.thing->Sector->floorplane.ZatPoint(sx, sy)))
	{
		open.bottom += 32 * FRACUNIT;
	}

	// adjust floor / ceiling heights
	if (open.top < tm.ceilingz)
	{
		tm.ceilingz = open.top;
		tm.ceilingsector = open.topsec;
		tm.ceilingpic = open.ceilingpic;
		tm.ceilingline = ld;
		tm.thing->BlockingLine = ld;
	}

	if (open.bottom > tm.floorz)
	{
		tm.floorz = open.bottom;
		tm.floorsector = open.bottomsec;
		tm.floorpic = open.floorpic;
		tm.floorterrain = open.floorterrain;
		tm.touchmidtex = open.touchmidtex;
		tm.abovemidtex = open.abovemidtex;
		tm.thing->BlockingLine = ld;
	}
	else if (open.bottom == tm.floorz)
	{
		tm.touchmidtex |= open.touchmidtex;
		tm.abovemidtex |= open.abovemidtex;
	}

	if (open.lowfloor < tm.dropoffz)
		tm.dropoffz = open.lowfloor;

	// if contacted a special line, add it to the list
	if (ld->special)
	{
		spechit.Push(ld);
	}

	return true;
}


//==========================================================================
//
// Isolated to keep the code readable and fix the logic
//
//==========================================================================

static bool CheckRipLevel(AActor *victim, AActor *projectile)
{
	if (victim->RipLevelMin > 0 && projectile->RipperLevel < victim->RipLevelMin) return false;
	if (victim->RipLevelMax > 0 && projectile->RipperLevel > victim->RipLevelMax) return false;
	return true;
}


//==========================================================================
//
// Isolated to keep the code readable and allow reuse in other attacks
//
//==========================================================================

static bool CanAttackHurt(AActor *victim, AActor *shooter)
{
	// players are never subject to infighting settings and are always allowed
	// to harm / be harmed by anything.
	if (!victim->player && !shooter->player)
	{
		int infight = G_SkillProperty(SKILLP_Infight);

		if (infight < 0)
		{
			// -1: Monsters cannot hurt each other, but make exceptions for
			//     friendliness and hate status.
			if (shooter->flags & MF_SHOOTABLE)
			{
				// Question: Should monsters be allowed to shoot barrels in this mode?
				// The old code does not.
				if (victim->flags3 & MF3_ISMONSTER)
				{
					// Monsters that are clearly hostile can always hurt each other
					if (!victim->IsHostile(shooter))
					{
						// The same if the shooter hates the target
						if (victim->tid == 0 || shooter->TIDtoHate != victim->tid)
						{
							return false;
						}
					}
				}
			}
		}
		else if (infight == 0)
		{
			//  0: Monsters cannot hurt same species except 
			//     cases where they are clearly supposed to do that
			if (victim->IsFriend(shooter))
			{
				// Friends never harm each other, unless the shooter has the HARMFRIENDS set.
				if (!(shooter->flags7 & MF7_HARMFRIENDS)) return false;
			}
			else
			{
				if (victim->TIDtoHate != 0 && victim->TIDtoHate == shooter->TIDtoHate)
				{
					// [RH] Don't hurt monsters that hate the same victim as you do
					return false;
				}
				if (victim->GetSpecies() == shooter->GetSpecies() && !(victim->flags6 & MF6_DOHARMSPECIES))
				{
					// Don't hurt same species or any relative -
					// but only if the target isn't one's hostile.
					if (!victim->IsHostile(shooter))
					{
						// Allow hurting monsters the shooter hates.
						if (victim->tid == 0 || shooter->TIDtoHate != victim->tid)
						{
							return false;
						}
					}
				}
			}
		}
		// else if (infight==1) every shot hurts anything - no further tests needed
	}
	return true;
}

//==========================================================================
//
// PIT_CheckThing
//
//==========================================================================

bool PIT_CheckThing(AActor *thing, FCheckPosition &tm)
{
	fixed_t topz;
	bool 	solid;
	int 	damage;

	// don't clip against self
	if (thing == tm.thing)
		return true;

	if (!((thing->flags & (MF_SOLID | MF_SPECIAL | MF_SHOOTABLE)) || thing->flags6 & MF6_TOUCHY))
		return true;	// can't hit thing

	fixedvec3 thingpos = thing->PosRelative(tm.thing);
	fixed_t blockdist = thing->radius + tm.thing->radius;
	if (abs(thingpos.x - tm.x) >= blockdist || abs(thingpos.y - tm.y) >= blockdist)
		return true;

	if ((thing->flags2 | tm.thing->flags2) & MF2_THRUACTORS)
		return true;

	if ((tm.thing->flags6 & MF6_THRUSPECIES) && (tm.thing->GetSpecies() == thing->GetSpecies()))
		return true;

	tm.thing->BlockingMobj = thing;
	topz = thing->Top();
	if (!(i_compatflags & COMPATF_NO_PASSMOBJ) && !(tm.thing->flags & (MF_FLOAT | MF_MISSILE | MF_SKULLFLY | MF_NOGRAVITY)) &&
		(thing->flags & MF_SOLID) && (thing->flags4 & MF4_ACTLIKEBRIDGE))
	{
		// [RH] Let monsters walk on actors as well as floors
		if ((tm.thing->flags3 & MF3_ISMONSTER) &&
			topz >= tm.floorz && topz <= tm.thing->Z() + tm.thing->MaxStepHeight)
		{
			// The commented-out if is an attempt to prevent monsters from walking off a
			// thing further than they would walk off a ledge. I can't think of an easy
			// way to do this, so I restrict them to only walking on bridges instead.
			// Uncommenting the if here makes it almost impossible for them to walk on
			// anything, bridge or otherwise.
			//			if (abs(thing->x - tmx) <= thing->radius &&
			//				abs(thing->y - tmy) <= thing->radius)
			{
				tm.stepthing = thing;
				tm.floorz = topz;
			}
		}
	}

	// Both things overlap in x or y direction
	bool unblocking = false;

	if ((tm.FromPMove || tm.thing->player != NULL) && thing->flags&MF_SOLID)
	{
		// Both actors already overlap. To prevent them from remaining stuck allow the move if it
		// takes them further apart or the move does not change the position (when called from P_ChangeSector.)
		if (tm.x == tm.thing->X() && tm.y == tm.thing->Y())
		{
			unblocking = true;
		}
		else if (abs(thingpos.x - tm.thing->X()) < (thing->radius+tm.thing->radius) &&
				 abs(thingpos.y - tm.thing->Y()) < (thing->radius+tm.thing->radius))

		{
			fixed_t newdist = thing->AproxDistance(tm.x, tm.y, tm.thing);
			fixed_t olddist = thing->AproxDistance(tm.thing);

			if (newdist > olddist)
			{
				// ... but not if they did not overlap in z-direction before but would after the move.
				unblocking = !((tm.thing->Z() >= topz && tm.z < topz) ||
					(tm.thing->Top() <= thingpos.z && tm.thing->Top() > thingpos.z));
			}
		}
	}

	// [RH] If the other thing is a bridge, then treat the moving thing as if it had MF2_PASSMOBJ, so
	// you can use a scrolling floor to move scenery items underneath a bridge.
	if ((tm.thing->flags2 & MF2_PASSMOBJ || thing->flags4 & MF4_ACTLIKEBRIDGE) && !(i_compatflags & COMPATF_NO_PASSMOBJ))
	{ // check if a mobj passed over/under another object
		if (!(tm.thing->flags & MF_MISSILE) ||
			!(tm.thing->flags2 & MF2_RIP) ||
			(thing->flags5 & MF5_DONTRIP) ||
			((tm.thing->flags6 & MF6_NOBOSSRIP) && (thing->flags2 & MF2_BOSS)))
		{
			if (tm.thing->flags3 & thing->flags3 & MF3_DONTOVERLAP)
			{ // Some things prefer not to overlap each other, if possible
				return unblocking;
			}
			if ((tm.thing->Z() >= topz) || (tm.thing->Top() <= thing->Z()))
				return true;
		}
	}

	if (tm.thing->player == NULL || !(tm.thing->player->cheats & CF_PREDICTING))
	{
		// touchy object is alive, toucher is solid
		if (thing->flags6 & MF6_TOUCHY && tm.thing->flags & MF_SOLID && thing->health > 0 &&
			// Thing is an armed mine or a sentient thing
			(thing->flags6 & MF6_ARMED || thing->IsSentient()) &&
			// either different classes or players
			(thing->player || thing->GetClass() != tm.thing->GetClass()) &&
			// or different species if DONTHARMSPECIES
			(!(thing->flags6 & MF6_DONTHARMSPECIES) || thing->GetSpecies() != tm.thing->GetSpecies()) &&
			// touches vertically
			topz >= tm.thing->Z() && tm.thing->Z() + tm.thing->height >= thingpos.z &&
			// prevents lost souls from exploding when fired by pain elementals
			(thing->master != tm.thing && tm.thing->master != thing))
			// Difference with MBF: MBF hardcodes the LS/PE check and lets actors of the same species
			// but different classes trigger the touchiness, but that seems less straightforwards.
		{
			thing->flags6 &= ~MF6_ARMED; // Disarm
			P_DamageMobj(thing, NULL, NULL, thing->health, NAME_None, DMG_FORCED);  // kill object
			return true;
		}

		// Check for MF6_BUMPSPECIAL
		// By default, only players can activate things by bumping into them
		if ((thing->flags6 & MF6_BUMPSPECIAL) && ((tm.thing->player != NULL)
			|| ((thing->activationtype & THINGSPEC_MonsterTrigger) && (tm.thing->flags3 & MF3_ISMONSTER))
			|| ((thing->activationtype & THINGSPEC_MissileTrigger) && (tm.thing->flags & MF_MISSILE))
			) && (level.maptime > thing->lastbump)) // Leave the bumper enough time to go away
		{
			if (P_ActivateThingSpecial(thing, tm.thing))
				thing->lastbump = level.maptime + TICRATE;
		}
	}

	// Check for skulls slamming into things
	if (tm.thing->flags & MF_SKULLFLY)
	{
		bool res = tm.thing->Slam(tm.thing->BlockingMobj);
		tm.thing->BlockingMobj = NULL;
		return res;
	}

	// [ED850] Player Prediction ends here. There is nothing else they could/should do.
	if (tm.thing->player != NULL && (tm.thing->player->cheats & CF_PREDICTING))
	{
		solid = (thing->flags & MF_SOLID) &&
			!(thing->flags & MF_NOCLIP) &&
			((tm.thing->flags & MF_SOLID) || (tm.thing->flags6 & MF6_BLOCKEDBYSOLIDACTORS));

		return !solid || unblocking;
	}

	// Check for blasted thing running into another
	if ((tm.thing->flags2 & MF2_BLASTED) && (thing->flags & MF_SHOOTABLE))
	{
		if (!(thing->flags2 & MF2_BOSS) && (thing->flags3 & MF3_ISMONSTER) && !(thing->flags3 & MF3_DONTBLAST))
		{
			// ideally this should take the mass factor into account
			thing->velx += tm.thing->velx;
			thing->vely += tm.thing->vely;
			if ((thing->velx + thing->vely) > 3 * FRACUNIT)
			{
				int newdam;
				damage = (tm.thing->Mass / 100) + 1;
				newdam = P_DamageMobj(thing, tm.thing, tm.thing, damage, tm.thing->DamageType);
				P_TraceBleed(newdam > 0 ? newdam : damage, thing, tm.thing);
				damage = (thing->Mass / 100) + 1;
				newdam = P_DamageMobj(tm.thing, thing, thing, damage >> 2, tm.thing->DamageType);
				P_TraceBleed(newdam > 0 ? newdam : damage, tm.thing, thing);
			}
			return false;
		}
	}
	// Check for missile or non-solid MBF bouncer
	if (tm.thing->flags & MF_MISSILE || ((tm.thing->BounceFlags & BOUNCE_MBF) && !(tm.thing->flags & MF_SOLID)))
	{
		// Check for a non-shootable mobj
		if (thing->flags2 & MF2_NONSHOOTABLE)
		{
			return true;
		}
		// Check for passing through a ghost
		if ((thing->flags3 & MF3_GHOST) && (tm.thing->flags2 & MF2_THRUGHOST))
		{
			return true;
		}

		if ((tm.thing->flags6 & MF6_MTHRUSPECIES)
			&& tm.thing->target // NULL pointer check
			&& (tm.thing->target->GetSpecies() == thing->GetSpecies()))
			return true;

		// Check for rippers passing through corpses
		if ((thing->flags & MF_CORPSE) && (tm.thing->flags2 & MF2_RIP) && !(thing->flags & MF_SHOOTABLE))
		{
			return true;
		}

		int clipheight;

		if (thing->projectilepassheight > 0)
		{
			clipheight = thing->projectilepassheight;
		}
		else if (thing->projectilepassheight < 0 && (i_compatflags & COMPATF_MISSILECLIP))
		{
			clipheight = -thing->projectilepassheight;
		}
		else
		{
			clipheight = thing->height;
		}

		// Check if it went over / under
		if (tm.thing->Z() > thingpos.z + clipheight)
		{ // Over thing
			return true;
		}
		if (tm.thing->Top() < thingpos.z)
		{ // Under thing
			return true;
		}

		// [RH] What is the point of this check, again? In Hexen, it is unconditional,
		// but here we only do it if the missile's damage is 0.
		// MBF bouncer might have a non-0 damage value, but they must not deal damage on impact either.
		if ((tm.thing->BounceFlags & BOUNCE_Actors) && (tm.thing->Damage == 0 || !(tm.thing->flags & MF_MISSILE)))
		{
			return (tm.thing->target == thing || !(thing->flags & MF_SOLID));
		}

		switch (tm.thing->SpecialMissileHit(thing))
		{
		case 0:		return false;
		case 1:		return true;
		default:	break;
		}

		// [RH] Extend DeHacked infighting to allow for monsters
		// to never fight each other

		if (tm.thing->target != NULL)
		{
			if (thing == tm.thing->target)
			{ // Don't missile self
				return true;
			}

			if (!CanAttackHurt(thing, tm.thing->target))
			{
				return false;
			}
		}
		if (!(thing->flags & MF_SHOOTABLE))
		{ // Didn't do any damage
			return !(thing->flags & MF_SOLID);
		}
		if ((thing->flags4 & MF4_SPECTRAL) && !(tm.thing->flags4 & MF4_SPECTRAL))
		{
			return true;
		}

		if ((tm.DoRipping && !(thing->flags5 & MF5_DONTRIP)) && CheckRipLevel(thing, tm.thing))
		{
			if (!(tm.thing->flags6 & MF6_NOBOSSRIP) || !(thing->flags2 & MF2_BOSS))
			{
				bool *check = tm.LastRipped.CheckKey(thing);
				if (check == NULL || !*check)
				{
					tm.LastRipped[thing] = true;
					if (!(thing->flags & MF_NOBLOOD) &&
						!(thing->flags2 & MF2_REFLECTIVE) &&
						!(tm.thing->flags3 & MF3_BLOODLESSIMPACT) &&
						!(thing->flags2 & (MF2_INVULNERABLE | MF2_DORMANT)))
					{ // Ok to spawn blood
						P_RipperBlood(tm.thing, thing);
					}
					S_Sound(tm.thing, CHAN_BODY, "misc/ripslop", 1, ATTN_IDLE);

					// Do poisoning (if using new style poison)
					if (tm.thing->PoisonDamage > 0 && tm.thing->PoisonDuration != INT_MIN)
					{
						P_PoisonMobj(thing, tm.thing, tm.thing->target, tm.thing->PoisonDamage, tm.thing->PoisonDuration, tm.thing->PoisonPeriod, tm.thing->PoisonDamageType);
					}

					damage = tm.thing->GetMissileDamage(3, 2);
					int newdam = P_DamageMobj(thing, tm.thing, tm.thing->target, damage, tm.thing->DamageType);
					if (!(tm.thing->flags3 & MF3_BLOODLESSIMPACT))
					{
						P_TraceBleed(newdam > 0 ? newdam : damage, thing, tm.thing);
					}
					if (thing->flags2 & MF2_PUSHABLE
						&& !(tm.thing->flags2 & MF2_CANNOTPUSH))
					{ // Push thing
						if (thing->lastpush != tm.PushTime)
						{
							thing->velx += FixedMul(tm.thing->velx, thing->pushfactor);
							thing->vely += FixedMul(tm.thing->vely, thing->pushfactor);
							thing->lastpush = tm.PushTime;
						}
					}
				}
				spechit.Clear();
				return true;
			}
		}

		// Do poisoning (if using new style poison)
		if (tm.thing->PoisonDamage > 0 && tm.thing->PoisonDuration != INT_MIN)
		{
			P_PoisonMobj(thing, tm.thing, tm.thing->target, tm.thing->PoisonDamage, tm.thing->PoisonDuration, tm.thing->PoisonPeriod, tm.thing->PoisonDamageType);
		}

		// Do damage
		damage = tm.thing->GetMissileDamage((tm.thing->flags4 & MF4_STRIFEDAMAGE) ? 3 : 7, 1);
		if ((damage > 0) || (tm.thing->flags6 & MF6_FORCEPAIN) || (tm.thing->flags7 & MF7_CAUSEPAIN))
		{
			int newdam = P_DamageMobj(thing, tm.thing, tm.thing->target, damage, tm.thing->DamageType);
			if (damage > 0)
			{
				if ((tm.thing->flags5 & MF5_BLOODSPLATTER) &&
					!(thing->flags & MF_NOBLOOD) &&
					!(thing->flags2 & MF2_REFLECTIVE) &&
					!(thing->flags2 & (MF2_INVULNERABLE | MF2_DORMANT)) &&
					!(tm.thing->flags3 & MF3_BLOODLESSIMPACT) &&
					(pr_checkthing() < 192))
				{
					P_BloodSplatter(tm.thing->X(), tm.thing->Y(), tm.thing->Z(), thing);
				}
				if (!(tm.thing->flags3 & MF3_BLOODLESSIMPACT))
				{
					P_TraceBleed(newdam > 0 ? newdam : damage, thing, tm.thing);
				}
			}
		}
		else
		{
			P_GiveBody(thing, -damage);
		}

		if ((thing->flags7 & MF7_THRUREFLECT) && (thing->flags2 & MF2_REFLECTIVE) && (tm.thing->flags & MF_MISSILE))
		{
			if (tm.thing->flags2 & MF2_SEEKERMISSILE)
			{
				tm.thing->tracer = tm.thing->target;
			}
			tm.thing->target = thing;
			return true;
		}
		return false;		// don't traverse any more
	}
	if (thing->flags2 & MF2_PUSHABLE && !(tm.thing->flags2 & MF2_CANNOTPUSH))
	{ // Push thing
		if (thing->lastpush != tm.PushTime)
		{
			thing->velx += FixedMul(tm.thing->velx, thing->pushfactor);
			thing->vely += FixedMul(tm.thing->vely, thing->pushfactor);
			thing->lastpush = tm.PushTime;
		}
	}
	solid = (thing->flags & MF_SOLID) &&
		!(thing->flags & MF_NOCLIP) &&
		((tm.thing->flags & MF_SOLID) || (tm.thing->flags6 & MF6_BLOCKEDBYSOLIDACTORS));

	// Check for special pickup
	if ((thing->flags & MF_SPECIAL) && (tm.thing->flags & MF_PICKUP)
		// [RH] The next condition is to compensate for the extra height
		// that gets added by P_CheckPosition() so that you cannot pick
		// up things that are above your true height.
		&& thingpos.z < tm.thing->Top() - tm.thing->MaxStepHeight)
	{ // Can be picked up by tmthing
		P_TouchSpecialThing(thing, tm.thing);	// can remove thing
	}

	// killough 3/16/98: Allow non-solid moving objects to move through solid
	// ones, by allowing the moving thing (tmthing) to move if it's non-solid,
	// despite another solid thing being in the way.
	// killough 4/11/98: Treat no-clipping things as not blocking

	return !solid || unblocking;

	// return !(thing->flags & MF_SOLID);	// old code -- killough
}



/*
===============================================================================

MOVEMENT CLIPPING

===============================================================================
*/

//==========================================================================
//
// P_CheckPosition
// This is purely informative, nothing is modified
// (except things picked up and missile damage applied).
// 
// in:
//	a AActor (can be valid or invalid)
//	a position to be checked
//	 (doesn't need to be related to the AActor->x,y)
//
// during:
//	special things are touched if MF_PICKUP
//	early out on solid lines?
//
// out:
//	newsubsec
//	floorz
//	ceilingz
//	tmdropoffz = the lowest point contacted (monsters won't move to a dropoff)
//	speciallines[]
//	numspeciallines
//  AActor *BlockingMobj = pointer to thing that blocked position (NULL if not
//   blocked, or blocked by a line).
//
//==========================================================================

bool P_CheckPosition(AActor *thing, fixed_t x, fixed_t y, FCheckPosition &tm, bool actorsonly)
{
	sector_t *newsec;
	AActor *thingblocker;
	fixed_t realheight = thing->height;

	tm.thing = thing;

	tm.x = x;
	tm.y = y;

	newsec = P_PointInSector(x, y);
	tm.ceilingline = thing->BlockingLine = NULL;

	// The base floor / ceiling is from the subsector that contains the point.
	// Any contacted lines the step closer together will adjust them.
	tm.floorz = tm.dropoffz = newsec->floorplane.ZatPoint(x, y);
	tm.ceilingz = newsec->ceilingplane.ZatPoint(x, y);
	tm.floorpic = newsec->GetTexture(sector_t::floor);
	tm.floorterrain = newsec->GetTerrain(sector_t::floor);
	tm.floorsector = newsec;
	tm.ceilingpic = newsec->GetTexture(sector_t::ceiling);
	tm.ceilingsector = newsec;
	tm.touchmidtex = false;
	tm.abovemidtex = false;

	//Added by MC: Fill the tmsector.
	tm.sector = newsec;

	//Check 3D floors
	if (!thing->IsNoClip2() && newsec->e->XFloor.ffloors.Size())
	{
		F3DFloor*  rover;
		fixed_t    delta1;
		fixed_t    delta2;
		int        thingtop = thing->Z() + (thing->height == 0 ? 1 : thing->height);

		for (unsigned i = 0; i<newsec->e->XFloor.ffloors.Size(); i++)
		{
			rover = newsec->e->XFloor.ffloors[i];
			if (!(rover->flags & FF_SOLID) || !(rover->flags & FF_EXISTS)) continue;

			fixed_t ff_bottom = rover->bottom.plane->ZatPoint(x, y);
			fixed_t ff_top = rover->top.plane->ZatPoint(x, y);

			delta1 = thing->Z() - (ff_bottom + ((ff_top - ff_bottom) / 2));
			delta2 = thingtop - (ff_bottom + ((ff_top - ff_bottom) / 2));

			if (ff_top > tm.floorz && abs(delta1) < abs(delta2))
			{
				tm.floorz = tm.dropoffz = ff_top;
				tm.floorpic = *rover->top.texture;
				tm.floorterrain = rover->model->GetTerrain(rover->top.isceiling);
			}
			if (ff_bottom < tm.ceilingz && abs(delta1) >= abs(delta2))
			{
				tm.ceilingz = ff_bottom;
				tm.ceilingpic = *rover->bottom.texture;
			}
		}
	}

	validcount++;
	spechit.Clear();

	if ((thing->flags & MF_NOCLIP) && !(thing->flags & MF_SKULLFLY))
		return true;

	// Check things first, possibly picking things up.
	thing->BlockingMobj = NULL;
	thingblocker = NULL;
	if (thing->player)
	{ // [RH] Fake taller height to catch stepping up into things.
		thing->height = realheight + thing->MaxStepHeight;
	}

	tm.stepthing = NULL;
	FBoundingBox box(x, y, thing->radius);

	{
		FBlockThingsIterator it2(box);
		AActor *th;
		while ((th = it2.Next()))
		{
			if (!PIT_CheckThing(th, tm))
			{ // [RH] If a thing can be stepped up on, we need to continue checking
				// other things in the blocks and see if we hit something that is
				// definitely blocking. Otherwise, we need to check the lines, or we
				// could end up stuck inside a wall.
				AActor *BlockingMobj = thing->BlockingMobj;

				if (BlockingMobj == NULL || (i_compatflags & COMPATF_NO_PASSMOBJ))
				{ // Thing slammed into something; don't let it move now.
					thing->height = realheight;
					return false;
				}
				else if (!BlockingMobj->player && !(thing->flags & (MF_FLOAT | MF_MISSILE | MF_SKULLFLY)) &&
					BlockingMobj->Top() - thing->Z() <= thing->MaxStepHeight)
				{
					if (thingblocker == NULL ||
						BlockingMobj->Z() > thingblocker->Z())
					{
						thingblocker = BlockingMobj;
					}
					thing->BlockingMobj = NULL;
				}
				else if (thing->player &&
					thing->Top() - BlockingMobj->Z() <= thing->MaxStepHeight)
				{
					if (thingblocker)
					{ // There is something to step up on. Return this thing as
						// the blocker so that we don't step up.
						thing->height = realheight;
						return false;
					}
					// Nothing is blocking us, but this actor potentially could
					// if there is something else to step on.
					thing->BlockingMobj = NULL;
				}
				else
				{ // Definitely blocking
					thing->height = realheight;
					return false;
				}
			}
		}
	}

	// check lines

	// [RH] We need to increment validcount again, because a function above may
	// have already set some lines to equal the current validcount.
	//
	// Specifically, when DehackedPickup spawns a new item in its TryPickup()
	// function, that new actor will set the lines around it to match validcount
	// when it links itself into the world. If we just leave validcount alone,
	// that will give the player the freedom to walk through walls at will near
	// a pickup they cannot get, because their validcount will prevent them from
	// being considered for collision with the player.
	validcount++;

	thing->BlockingMobj = NULL;
	thing->height = realheight;
	if (actorsonly || (thing->flags & MF_NOCLIP))
		return (thing->BlockingMobj = thingblocker) == NULL;

	FBlockLinesIterator it(box);
	line_t *ld;

	fixed_t thingdropoffz = tm.floorz;
	//bool onthing = (thingdropoffz != tmdropoffz);
	tm.floorz = tm.dropoffz;

	bool good = true;

	while ((ld = it.Next()))
	{
		good &= PIT_CheckLine(ld, box, tm);
	}
	if (!good)
	{
		return false;
	}
	if (tm.ceilingz - tm.floorz < thing->height)
	{
		return false;
	}
	if (tm.touchmidtex)
	{
		tm.dropoffz = tm.floorz;
	}
	else if (tm.stepthing != NULL)
	{
		tm.dropoffz = thingdropoffz;
	}

	return (thing->BlockingMobj = thingblocker) == NULL;
}

bool P_CheckPosition(AActor *thing, fixed_t x, fixed_t y, bool actorsonly)
{
	FCheckPosition tm;
	return P_CheckPosition(thing, x, y, tm, actorsonly);
}

//----------------------------------------------------------------------------
//
// FUNC P_TestMobjLocation
//
// Returns true if the mobj is not blocked by anything at its current
// location, otherwise returns false.
//
//----------------------------------------------------------------------------

bool P_TestMobjLocation(AActor *mobj)
{
	ActorFlags flags;

	flags = mobj->flags;
	mobj->flags &= ~MF_PICKUP;
	if (P_CheckPosition(mobj, mobj->X(), mobj->Y()))
	{ // XY is ok, now check Z
		mobj->flags = flags;
		if ((mobj->Z() < mobj->floorz) || (mobj->Top() > mobj->ceilingz))
		{ // Bad Z
			return false;
		}
		return true;
	}
	mobj->flags = flags;
	return false;
}


//=============================================================================
//
// P_CheckOnmobj(AActor *thing)
//
//				Checks if the new Z position is legal
//=============================================================================

AActor *P_CheckOnmobj(AActor *thing)
{
	fixed_t oldz;
	bool good;
	AActor *onmobj;

	oldz = thing->Z();
	P_FakeZMovement(thing);
	good = P_TestMobjZ(thing, false, &onmobj);
	thing->SetZ(oldz);

	return good ? NULL : onmobj;
}

//=============================================================================
//
// P_TestMobjZ
//
//=============================================================================

bool P_TestMobjZ(AActor *actor, bool quick, AActor **pOnmobj)
{
	AActor *onmobj = NULL;
	if (actor->flags & MF_NOCLIP)
	{
		if (pOnmobj) *pOnmobj = NULL;
		return true;
	}

	FBlockThingsIterator it(FBoundingBox(actor->X(), actor->Y(), actor->radius));
	AActor *thing;

	while ((thing = it.Next()))
	{
		if (!thing->intersects(actor))
		{
			continue;
		}
		if ((actor->flags2 | thing->flags2) & MF2_THRUACTORS)
		{
			continue;
		}
		if ((actor->flags6 & MF6_THRUSPECIES) && (thing->GetSpecies() == actor->GetSpecies()))
		{
			continue;
		}
		if (!(thing->flags & MF_SOLID))
		{ // Can't hit thing
			continue;
		}
		if (thing->flags & (MF_SPECIAL | MF_NOCLIP))
		{ // [RH] Specials and noclippers don't block moves
			continue;
		}
		if (thing->flags & (MF_CORPSE))
		{ // Corpses need a few more checks
			if (!(actor->flags & MF_ICECORPSE))
				continue;
		}
		if (!(thing->flags4 & MF4_ACTLIKEBRIDGE) && (actor->flags & MF_SPECIAL))
		{ // [RH] Only bridges block pickup items
			continue;
		}
		if (thing == actor)
		{ // Don't clip against self
			continue;
		}
		if ((actor->flags & MF_MISSILE) && (thing == actor->target))
		{ // Don't clip against whoever shot the missile.
			continue;
		}
		if (actor->Z() > thing->Top())
		{ // over thing
			continue;
		}
		else if (actor->Top() <= thing->Z())
		{ // under thing
			continue;
		}
		else if (!quick && onmobj != NULL && thing->Top() < onmobj->Top())
		{ // something higher is in the way
			continue;
		}
		onmobj = thing;
		if (quick) break;
	}

	if (pOnmobj) *pOnmobj = onmobj;
	return onmobj == NULL;
}

//=============================================================================
//
// P_FakeZMovement
//
//				Fake the zmovement so that we can check if a move is legal
//=============================================================================

void P_FakeZMovement(AActor *mo)
{
	//
	// adjust height
	//
	mo->AddZ(mo->velz);
	if ((mo->flags&MF_FLOAT) && mo->target)
	{ // float down towards target if too close
		if (!(mo->flags & MF_SKULLFLY) && !(mo->flags & MF_INFLOAT))
		{
			fixed_t dist = mo->AproxDistance(mo->target);
			fixed_t delta = (mo->target->Z() + (mo->height >> 1)) - mo->Z();
			if (delta < 0 && dist < -(delta * 3))
				mo->AddZ(-mo->FloatSpeed);
			else if (delta > 0 && dist < (delta * 3))
				mo->AddZ(mo->FloatSpeed);
		}
	}
	if (mo->player && mo->flags&MF_NOGRAVITY && (mo->Z() > mo->floorz) && !mo->IsNoClip2())
	{
		mo->AddZ(finesine[(FINEANGLES / 80 * level.maptime)&FINEMASK] / 8);
	}

	//
	// clip movement
	//
	if (mo->Z() <= mo->floorz)
	{ // hit the floor
		mo->SetZ(mo->floorz);
	}

	if (mo->Top() > mo->ceilingz)
	{ // hit the ceiling
		mo->SetZ(mo->ceilingz - mo->height);
	}
}

//===========================================================================
//
// CheckForPushSpecial
//
//===========================================================================

static void CheckForPushSpecial(line_t *line, int side, AActor *mobj, bool windowcheck)
{
	if (line->special && !(mobj->flags6 & MF6_NOTRIGGER))
	{
		if (windowcheck && !(ib_compatflags & BCOMPATF_NOWINDOWCHECK) && line->backsector != NULL)
		{ // Make sure this line actually blocks us and is not a window
			// or similar construct we are standing inside of.
			fixed_t fzt = line->frontsector->ceilingplane.ZatPoint(mobj);
			fixed_t fzb = line->frontsector->floorplane.ZatPoint(mobj);
			fixed_t bzt = line->backsector->ceilingplane.ZatPoint(mobj);
			fixed_t bzb = line->backsector->floorplane.ZatPoint(mobj);
			if (fzt >= mobj->Top() && bzt >= mobj->Top() &&
				fzb <= mobj->Z() && bzb <= mobj->Z())
			{
				// we must also check if some 3D floor in the backsector may be blocking
				for (unsigned int i = 0; i<line->backsector->e->XFloor.ffloors.Size(); i++)
				{
					F3DFloor* rover = line->backsector->e->XFloor.ffloors[i];

					if (!(rover->flags & FF_SOLID) || !(rover->flags & FF_EXISTS)) continue;

					fixed_t ff_bottom = rover->bottom.plane->ZatPoint(mobj);
					fixed_t ff_top = rover->top.plane->ZatPoint(mobj);

					if (ff_bottom < mobj->Top() && ff_top > mobj->Z())
					{
						goto isblocking;
					}
				}
				return;
			}
		}
	isblocking:
		if (mobj->flags2 & MF2_PUSHWALL)
		{
			P_ActivateLine(line, mobj, side, SPAC_Push);
		}
		else if (mobj->flags2 & MF2_IMPACT)
		{
			if ((level.flags2 & LEVEL2_MISSILESACTIVATEIMPACT) ||
				!(mobj->flags & MF_MISSILE) ||
				(mobj->target == NULL))
			{
				P_ActivateLine(line, mobj, side, SPAC_Impact);
			}
			else
			{
				P_ActivateLine(line, mobj->target, side, SPAC_Impact);
			}
		}
	}
}

//==========================================================================
//
// P_TryMove
// Attempt to move to a new position,
// crossing special lines unless MF_TELEPORT is set.
//
//==========================================================================

bool P_TryMove(AActor *thing, fixed_t x, fixed_t y,
	int dropoff, // killough 3/15/98: allow dropoff as option
	const secplane_t *onfloor, // [RH] Let P_TryMove keep the thing on the floor
	FCheckPosition &tm,
	bool missileCheck)	// [GZ] Fired missiles ignore the drop-off test
{
	fixedvec3	oldpos;
	sector_t	*oldsector;
	fixed_t		oldz;
	int 		side;
	int 		oldside;
	line_t* 	ld;
	sector_t*	oldsec = thing->Sector;	// [RH] for sector actions
	sector_t*	newsec;

	tm.floatok = false;
	oldz = thing->Z();
	if (onfloor)
	{
		thing->SetZ(onfloor->ZatPoint(x, y));
	}
	thing->flags6 |= MF6_INTRYMOVE;
	if (!P_CheckPosition(thing, x, y, tm))
	{
		AActor *BlockingMobj = thing->BlockingMobj;
		// Solid wall or thing
		if (!BlockingMobj || BlockingMobj->player || !thing->player)
		{
			goto pushline;
		}
		else
		{
			if (BlockingMobj->player || !thing->player)
			{
				goto pushline;
			}
			else if (BlockingMobj->Top() - thing->Z() > thing->MaxStepHeight
				|| (BlockingMobj->Sector->ceilingplane.ZatPoint(x, y) - (BlockingMobj->Top()) < thing->height)
				|| (tm.ceilingz - (BlockingMobj->Top()) < thing->height))
			{
				goto pushline;
			}
		}
		if (!(tm.thing->flags2 & MF2_PASSMOBJ) || (i_compatflags & COMPATF_NO_PASSMOBJ))
		{
			thing->SetZ(oldz);
			thing->flags6 &= ~MF6_INTRYMOVE;
			return false;
		}
	}

	if (thing->flags3 & MF3_FLOORHUGGER)
	{
		thing->SetZ(tm.floorz);
	}
	else if (thing->flags3 & MF3_CEILINGHUGGER)
	{
		thing->SetZ(tm.ceilingz - thing->height);
	}

	if (onfloor && tm.floorsector == thing->floorsector)
	{
		thing->SetZ(tm.floorz);
	}
	if (!(thing->flags & MF_NOCLIP))
	{
		if (tm.ceilingz - tm.floorz < thing->height)
		{
			goto pushline;		// doesn't fit
		}

		tm.floatok = true;

		if (!(thing->flags & MF_TELEPORT)
			&& tm.ceilingz - thing->Z() < thing->height
			&& !(thing->flags3 & MF3_CEILINGHUGGER)
			&& (!(thing->flags2 & MF2_FLY) || !(thing->flags & MF_NOGRAVITY)))
		{
			goto pushline;		// mobj must lower itself to fit
		}
		if (thing->flags2 & MF2_FLY && thing->flags & MF_NOGRAVITY)
		{
#if 1
			if (thing->Top() > tm.ceilingz)
				goto pushline;
#else
			// When flying, slide up or down blocking lines until the actor
			// is not blocked.
			if (thing->Top() > tm.ceilingz)
			{
				thing->velz = -8 * FRACUNIT;
				goto pushline;
			}
			else if (thing->Z() < tm.floorz && tm.floorz - tm.dropoffz > thing->MaxDropOffHeight)
			{
				thing->velz = 8 * FRACUNIT;
				goto pushline;
			}
#endif
		}
		if (!(thing->flags & MF_TELEPORT) && !(thing->flags3 & MF3_FLOORHUGGER))
		{
			if ((thing->flags & MF_MISSILE) && !(thing->flags6 & MF6_STEPMISSILE) && tm.floorz > thing->Z())
			{ // [RH] Don't let normal missiles climb steps
				goto pushline;
			}
			if (tm.floorz - thing->Z() > thing->MaxStepHeight)
			{ // too big a step up
				goto pushline;
			}
			else if (thing->Z() < tm.floorz)
			{ // [RH] Check to make sure there's nothing in the way for the step up
				fixed_t savedz = thing->Z();
				bool good;
				thing->SetZ(tm.floorz);
				good = P_TestMobjZ(thing);
				thing->SetZ(savedz);
				if (!good)
				{
					goto pushline;
				}
				if (thing->flags6 & MF6_STEPMISSILE)
				{
					thing->SetZ(tm.floorz);
					// If moving down, cancel vertical component of the velocity
					if (thing->velz < 0)
					{
						// If it's a bouncer, let it bounce off its new floor, too.
						if (thing->BounceFlags & BOUNCE_Floors)
						{
							thing->FloorBounceMissile(tm.floorsector->floorplane);
						}
						else
						{
							thing->velz = 0;
						}
					}
				}
			}
		}

		// compatibility check: Doom originally did not allow monsters to cross dropoffs at all.
		// If the compatibility flag is on, only allow this when the velocity comes from a scroller
		if ((i_compatflags & COMPATF_CROSSDROPOFF) && !(thing->flags4 & MF4_SCROLLMOVE))
		{
			dropoff = false;
		}

		if (dropoff == 2 &&  // large jump down (e.g. dogs)
			(tm.floorz - tm.dropoffz > 128 * FRACUNIT || thing->target == NULL || thing->target->Z() >tm.dropoffz))
		{
			dropoff = false;
		}


		// killough 3/15/98: Allow certain objects to drop off
		if ((!dropoff && !(thing->flags & (MF_DROPOFF | MF_FLOAT | MF_MISSILE))) || (thing->flags5&MF5_NODROPOFF))
		{
			if (!(thing->flags5&MF5_AVOIDINGDROPOFF))
			{
				fixed_t floorz = tm.floorz;
				// [RH] If the thing is standing on something, use its current z as the floorz.
				// This is so that it does not walk off of things onto a drop off.
				if (thing->flags2 & MF2_ONMOBJ)
				{
					floorz = MAX(thing->Z(), tm.floorz);
				}

				if (floorz - tm.dropoffz > thing->MaxDropOffHeight &&
					!(thing->flags2 & MF2_BLASTED) && !missileCheck)
				{ // Can't move over a dropoff unless it's been blasted
					// [GZ] Or missile-spawned
					thing->SetZ(oldz);
					thing->flags6 &= ~MF6_INTRYMOVE;
					return false;
				}
			}
			else
			{
				// special logic to move a monster off a dropoff
				// this intentionally does not check for standing on things.
				if (thing->floorz - tm.floorz > thing->MaxDropOffHeight ||
					thing->dropoffz - tm.dropoffz > thing->MaxDropOffHeight)
				{
					thing->flags6 &= ~MF6_INTRYMOVE;
					return false;
				}
			}
		}
		if (thing->flags2 & MF2_CANTLEAVEFLOORPIC
			&& (tm.floorpic != thing->floorpic
			|| tm.floorz - thing->Z() != 0))
		{ // must stay within a sector of a certain floor type
			thing->SetZ(oldz);
			thing->flags6 &= ~MF6_INTRYMOVE;
			return false;
		}

		//Added by MC: To prevent bot from getting into dangerous sectors.
		if (thing->player && thing->player->Bot != NULL && thing->flags & MF_SHOOTABLE)
		{
			if (tm.sector != thing->Sector
				&& bglobal.IsDangerous(tm.sector))
			{
				thing->player->Bot->prev = thing->player->Bot->dest;
				thing->player->Bot->dest = NULL;
				thing->velx = 0;
				thing->vely = 0;
				thing->SetZ(oldz);
				thing->flags6 &= ~MF6_INTRYMOVE;
				return false;
			}
		}
	}

	// [RH] Check status of eyes against fake floor/ceiling in case
	// it slopes or the player's eyes are bobbing in and out.

	bool oldAboveFakeFloor, oldAboveFakeCeiling;
	fixed_t viewheight;

	viewheight = thing->player ? thing->player->viewheight : thing->height / 2;
	oldAboveFakeFloor = oldAboveFakeCeiling = false;	// pacify GCC

	if (oldsec->heightsec)
	{
		fixed_t eyez = oldz + viewheight;

		oldAboveFakeFloor = eyez > oldsec->heightsec->floorplane.ZatPoint(thing);
		oldAboveFakeCeiling = eyez > oldsec->heightsec->ceilingplane.ZatPoint(thing);
	}

	// Borrowed from MBF: 
	if (thing->BounceFlags & BOUNCE_MBF &&  // killough 8/13/98
		!(thing->flags & (MF_MISSILE | MF_NOGRAVITY)) &&
		!thing->IsSentient() && tm.floorz - thing->Z() > 16 * FRACUNIT)
	{ // too big a step up for MBF bouncers under gravity
		thing->flags6 &= ~MF6_INTRYMOVE;
		return false;
	}

	// the move is ok, so link the thing into its new position
	thing->UnlinkFromWorld();

	oldpos = thing->Pos();
	oldsector = thing->Sector;
	thing->floorz = tm.floorz;
	thing->ceilingz = tm.ceilingz;
	thing->dropoffz = tm.dropoffz;		// killough 11/98: keep track of dropoffs
	thing->floorpic = tm.floorpic;
	thing->floorterrain = tm.floorterrain;
	thing->floorsector = tm.floorsector;
	thing->ceilingpic = tm.ceilingpic;
	thing->ceilingsector = tm.ceilingsector;
	thing->SetXY(x, y);

	thing->LinkToWorld();

	if (thing->flags2 & MF2_FLOORCLIP)
	{
		thing->AdjustFloorClip();
	}

	// if any special lines were hit, do the effect
	if (!(thing->flags & (MF_TELEPORT | MF_NOCLIP)))
	{
		while (spechit.Pop(ld))
		{
			fixedvec3 thingpos = thing->PosRelative(ld);
			fixedvec3 oldrelpos = PosRelative(oldpos, ld, oldsector);
			// see if the line was crossed
			side = P_PointOnLineSide(thingpos.x, thingpos.y, ld);
			oldside = P_PointOnLineSide(oldrelpos.x, oldrelpos.y, ld);
			if (side != oldside && ld->special && !(thing->flags6 & MF6_NOTRIGGER))
			{
				if (thing->player && (thing->player->cheats & CF_PREDICTING))
				{
					P_PredictLine(ld, thing, oldside, SPAC_Cross);
				}
				else if (thing->player)
				{
					P_ActivateLine(ld, thing, oldside, SPAC_Cross);
				}
				else if (thing->flags2 & MF2_MCROSS)
				{
					P_ActivateLine(ld, thing, oldside, SPAC_MCross);
				}
				else if (thing->flags2 & MF2_PCROSS)
				{
					P_ActivateLine(ld, thing, oldside, SPAC_PCross);
				}
				else if ((ld->special == Teleport ||
					ld->special == Teleport_NoFog ||
					ld->special == Teleport_Line))
				{	// [RH] Just a little hack for BOOM compatibility
					P_ActivateLine(ld, thing, oldside, SPAC_MCross);
				}
				else
				{
					P_ActivateLine(ld, thing, oldside, SPAC_AnyCross);
				}
			}
		}
	}

	// [RH] Don't activate anything if just predicting
	if (thing->player && (thing->player->cheats & CF_PREDICTING))
	{
		thing->flags6 &= ~MF6_INTRYMOVE;
		return true;
	}

	// [RH] Check for crossing fake floor/ceiling
	newsec = thing->Sector;
	if (newsec->heightsec && oldsec->heightsec && newsec->SecActTarget)
	{
		const sector_t *hs = newsec->heightsec;
		fixed_t eyez = thing->Z() + viewheight;
		fixed_t fakez = hs->floorplane.ZatPoint(x, y);

		if (!oldAboveFakeFloor && eyez > fakez)
		{ // View went above fake floor
			newsec->SecActTarget->TriggerAction(thing, SECSPAC_EyesSurface);
		}
		else if (oldAboveFakeFloor && eyez <= fakez)
		{ // View went below fake floor
			newsec->SecActTarget->TriggerAction(thing, SECSPAC_EyesDive);
		}

		if (!(hs->MoreFlags & SECF_FAKEFLOORONLY))
		{
			fakez = hs->ceilingplane.ZatPoint(x, y);
			if (!oldAboveFakeCeiling && eyez > fakez)
			{ // View went above fake ceiling
				newsec->SecActTarget->TriggerAction(thing, SECSPAC_EyesAboveC);
			}
			else if (oldAboveFakeCeiling && eyez <= fakez)
			{ // View went below fake ceiling
				newsec->SecActTarget->TriggerAction(thing, SECSPAC_EyesBelowC);
			}
		}
	}

	// [RH] If changing sectors, trigger transitions
	thing->CheckSectorTransition(oldsec);
	thing->flags6 &= ~MF6_INTRYMOVE;
	return true;

pushline:
	thing->flags6 &= ~MF6_INTRYMOVE;

	// [RH] Don't activate anything if just predicting
	if (thing->player && (thing->player->cheats & CF_PREDICTING))
	{
		return false;
	}

	thing->SetZ(oldz);
	if (!(thing->flags&(MF_TELEPORT | MF_NOCLIP)))
	{
		int numSpecHitTemp;

		if (tm.thing->flags2 & MF2_BLASTED)
		{
			P_DamageMobj(tm.thing, NULL, NULL, tm.thing->Mass >> 5, NAME_Melee);
		}
		numSpecHitTemp = (int)spechit.Size();
		while (numSpecHitTemp > 0)
		{
			// see which lines were pushed
			ld = spechit[--numSpecHitTemp];
			fixedvec3 pos = thing->PosRelative(ld);
			side = P_PointOnLineSide(pos.x, pos.y, ld);
			CheckForPushSpecial(ld, side, thing, true);
		}
	}
	return false;
}

bool P_TryMove(AActor *thing, fixed_t x, fixed_t y,
	int dropoff, // killough 3/15/98: allow dropoff as option
	const secplane_t *onfloor) // [RH] Let P_TryMove keep the thing on the floor
{
	FCheckPosition tm;
	return P_TryMove(thing, x, y, dropoff, onfloor, tm);
}



//==========================================================================
//
// P_CheckMove
// Similar to P_TryMove but doesn't actually move the actor. Used for polyobject crushing
//
//==========================================================================

bool P_CheckMove(AActor *thing, fixed_t x, fixed_t y)
{
	FCheckPosition tm;
	fixed_t		newz = thing->Z();

	if (!P_CheckPosition(thing, x, y, tm))
	{
		return false;
	}

	if (thing->flags3 & MF3_FLOORHUGGER)
	{
		newz = tm.floorz;
	}
	else if (thing->flags3 & MF3_CEILINGHUGGER)
	{
		newz = tm.ceilingz - thing->height;
	}

	if (!(thing->flags & MF_NOCLIP))
	{
		if (tm.ceilingz - tm.floorz < thing->height)
		{
			return false;
		}

		if (!(thing->flags & MF_TELEPORT)
			&& tm.ceilingz - newz < thing->height
			&& !(thing->flags3 & MF3_CEILINGHUGGER)
			&& (!(thing->flags2 & MF2_FLY) || !(thing->flags & MF_NOGRAVITY)))
		{
			return false;
		}
		if (thing->flags2 & MF2_FLY && thing->flags & MF_NOGRAVITY)
		{
			if (thing->Top() > tm.ceilingz)
				return false;
		}
		if (!(thing->flags & MF_TELEPORT) && !(thing->flags3 & MF3_FLOORHUGGER))
		{
			if (tm.floorz - newz > thing->MaxStepHeight)
			{ // too big a step up
				return false;
			}
			else if ((thing->flags & MF_MISSILE) && !(thing->flags6 & MF6_STEPMISSILE) && tm.floorz > newz)
			{ // [RH] Don't let normal missiles climb steps
				return false;
			}
			else if (newz < tm.floorz)
			{ // [RH] Check to make sure there's nothing in the way for the step up
				fixed_t savedz = thing->Z();
				thing->SetZ(newz = tm.floorz);
				bool good = P_TestMobjZ(thing);
				thing->SetZ(savedz);
				if (!good)
				{
					return false;
				}
			}
		}

		if (thing->flags2 & MF2_CANTLEAVEFLOORPIC
			&& (tm.floorpic != thing->floorpic
			|| tm.floorz - newz != 0))
		{ // must stay within a sector of a certain floor type
			return false;
		}
	}

	return true;
}



//==========================================================================
//
// SLIDE MOVE
// Allows the player to slide along any angled walls.
//
//==========================================================================

struct FSlide
{
	fixed_t 		bestslidefrac;
	fixed_t 		secondslidefrac;

	line_t* 		bestslideline;
	line_t* 		secondslideline;

	AActor* 		slidemo;

	fixed_t 		tmxmove;
	fixed_t 		tmymove;

	void HitSlideLine(line_t *ld);
	void SlideTraverse(fixed_t startx, fixed_t starty, fixed_t endx, fixed_t endy);
	void SlideMove(AActor *mo, fixed_t tryx, fixed_t tryy, int numsteps);

	// The bouncing code uses the same data structure
	bool BounceTraverse(fixed_t startx, fixed_t starty, fixed_t endx, fixed_t endy);
	bool BounceWall(AActor *mo);
};

//==========================================================================
//
// P_HitSlideLine
// Adjusts the xmove / ymove
// so that the next move will slide along the wall.
// If the floor is icy, then you can bounce off a wall.				// phares
//
//==========================================================================

void FSlide::HitSlideLine(line_t* ld)
{
	int 	side;

	angle_t lineangle;
	angle_t moveangle;
	angle_t deltaangle;

	fixed_t movelen;
	bool	icyfloor;	// is floor icy?							// phares
	//   |
	// Under icy conditions, if the angle of approach to the wall	//   V
	// is more than 45 degrees, then you'll bounce and lose half
	// your velocity. If less than 45 degrees, you'll slide along
	// the wall. 45 is arbitrary and is believable.

	// Check for the special cases of horz or vert walls.

	// killough 10/98: only bounce if hit hard (prevents wobbling)
	icyfloor =
		(P_AproxDistance(tmxmove, tmymove) > 4 * FRACUNIT) &&
		var_friction &&  // killough 8/28/98: calc friction on demand
		slidemo->Z() <= slidemo->floorz &&
		P_GetFriction(slidemo, NULL) > ORIG_FRICTION;

	if (ld->dx == 0)
	{ // ST_VERTICAL
		if (icyfloor && (abs(tmxmove) > abs(tmymove)))
		{
			tmxmove = -tmxmove / 2; // absorb half the velocity
			tmymove /= 2;
			if (slidemo->player && slidemo->health > 0 && !(slidemo->player->cheats & CF_PREDICTING))
			{
				S_Sound(slidemo, CHAN_VOICE, "*grunt", 1, ATTN_IDLE); // oooff!//   ^
			}
		}																		//   |
		else																	// phares
			tmxmove = 0; // no more movement in the X direction
		return;
	}

	if (ld->dy == 0)
	{ // ST_HORIZONTAL
		if (icyfloor && (abs(tmymove) > abs(tmxmove)))
		{
			tmxmove /= 2; // absorb half the velocity
			tmymove = -tmymove / 2;
			if (slidemo->player && slidemo->health > 0 && !(slidemo->player->cheats & CF_PREDICTING))
			{
				S_Sound(slidemo, CHAN_VOICE, "*grunt", 1, ATTN_IDLE); // oooff!
			}
		}
		else
			tmymove = 0; // no more movement in the Y direction
		return;
	}

	// The wall is angled. Bounce if the angle of approach is		// phares
	// less than 45 degrees.										// phares

	fixedvec3 pos = slidemo->PosRelative(ld);
	side = P_PointOnLineSide(pos.x, pos.y, ld);

	lineangle = R_PointToAngle2(0, 0, ld->dx, ld->dy);

	if (side == 1)
		lineangle += ANG180;

	moveangle = R_PointToAngle2(0, 0, tmxmove, tmymove);

	moveangle += 10;		// prevents sudden path reversal due to	// phares
	// rounding error						//   |
	deltaangle = moveangle - lineangle;								//   V
	movelen = P_AproxDistance(tmxmove, tmymove);
	if (icyfloor && (deltaangle > ANG45) && (deltaangle < ANG90 + ANG45))
	{
		moveangle = lineangle - deltaangle;
		movelen /= 2; // absorb
		if (slidemo->player && slidemo->health > 0 && !(slidemo->player->cheats & CF_PREDICTING))
		{
			S_Sound(slidemo, CHAN_VOICE, "*grunt", 1, ATTN_IDLE); // oooff!
		}
		moveangle >>= ANGLETOFINESHIFT;
		tmxmove = FixedMul(movelen, finecosine[moveangle]);
		tmymove = FixedMul(movelen, finesine[moveangle]);
	}																//   ^
	else															//   |
	{																// phares
		// Doom's original algorithm here does not work well due to imprecisions of the sine table.
		// However, keep it active if the wallrunning compatibility flag is on
		if (i_compatflags & COMPATF_WALLRUN)
		{
			fixed_t newlen;

			if (deltaangle > ANG180)
				deltaangle += ANG180;
			//	I_Error ("SlideLine: ang>ANG180");

			lineangle >>= ANGLETOFINESHIFT;
			deltaangle >>= ANGLETOFINESHIFT;

			newlen = FixedMul(movelen, finecosine[deltaangle]);

			tmxmove = FixedMul(newlen, finecosine[lineangle]);
			tmymove = FixedMul(newlen, finesine[lineangle]);
		}
		else
		{
			divline_t dll, dlv;
			fixed_t inter1, inter2, inter3;

			P_MakeDivline(ld, &dll);

			dlv.x = pos.x;
			dlv.y = pos.y;
			dlv.dx = dll.dy;
			dlv.dy = -dll.dx;

			inter1 = P_InterceptVector(&dll, &dlv);

			dlv.dx = tmxmove;
			dlv.dy = tmymove;
			inter2 = P_InterceptVector(&dll, &dlv);
			inter3 = P_InterceptVector(&dlv, &dll);

			if (inter3 != 0)
			{
				tmxmove = Scale(inter2 - inter1, dll.dx, inter3);
				tmymove = Scale(inter2 - inter1, dll.dy, inter3);
			}
			else
			{
				tmxmove = tmymove = 0;
			}
		}
	}																// phares
}


//==========================================================================
//
// PTR_SlideTraverse
//
//==========================================================================

void FSlide::SlideTraverse(fixed_t startx, fixed_t starty, fixed_t endx, fixed_t endy)
{
	FLineOpening open;
	FPathTraverse it(startx, starty, endx, endy, PT_ADDLINES);
	intercept_t *in;

	while ((in = it.Next()))
	{
		line_t* 	li;

		if (!in->isaline)
		{
			// should never happen
			Printf("PTR_SlideTraverse: not a line?");
			continue;
		}

		li = in->d.line;

		if (!(li->flags & ML_TWOSIDED) || !li->backsector)
		{
			fixedvec3 pos = slidemo->PosRelative(li);
			if (P_PointOnLineSide(pos.x, pos.y, li))
			{
				// don't hit the back side
				continue;
			}
			goto isblocking;
		}
		if (li->flags & (ML_BLOCKING | ML_BLOCKEVERYTHING))
		{
			goto isblocking;
		}
		if (li->flags & ML_BLOCK_PLAYERS && slidemo->player != NULL)
		{
			goto isblocking;
		}
		if (li->flags & ML_BLOCKMONSTERS && !((slidemo->flags3 & MF3_NOBLOCKMONST)
			|| ((i_compatflags & COMPATF_NOBLOCKFRIENDS) && (slidemo->flags & MF_FRIENDLY))))
		{
			goto isblocking;
		}

		// set openrange, opentop, openbottom
		P_LineOpening(open, slidemo, li, it.Trace().x + FixedMul(it.Trace().dx, in->frac),
			it.Trace().y + FixedMul(it.Trace().dy, in->frac));

		if (open.range < slidemo->height)
			goto isblocking;				// doesn't fit

		if (open.top - slidemo->Z() < slidemo->height)
			goto isblocking;				// mobj is too high

		if (open.bottom - slidemo->Z() > slidemo->MaxStepHeight)
		{
			goto isblocking;				// too big a step up
		}
		else if (slidemo->Z() < open.bottom)
		{ // [RH] Check to make sure there's nothing in the way for the step up
			fixed_t savedz = slidemo->Z();
			slidemo->SetZ(open.bottom);
			bool good = P_TestMobjZ(slidemo);
			slidemo->SetZ(savedz);
			if (!good)
			{
				goto isblocking;
			}
		}

		// this line doesn't block movement
		continue;

		// the line does block movement,
		// see if it is closer than best so far
	isblocking:
		if (in->frac < bestslidefrac)
		{
			secondslidefrac = bestslidefrac;
			secondslideline = bestslideline;
			bestslidefrac = in->frac;
			bestslideline = li;
		}

		return;		// stop
	}
}



//==========================================================================
//
// P_SlideMove
//
// The velx / vely move is bad, so try to slide along a wall.
//
// Find the first line hit, move flush to it, and slide along it
//
// This is a kludgy mess.
//
//==========================================================================

void FSlide::SlideMove(AActor *mo, fixed_t tryx, fixed_t tryy, int numsteps)
{
	fixed_t leadx, leady;
	fixed_t trailx, traily;
	fixed_t newx, newy;
	fixed_t xmove, ymove;
	const secplane_t * walkplane;
	int hitcount;

	hitcount = 3;
	slidemo = mo;

	if (mo->player && mo->player->mo == mo && mo->reactiontime > 0)
		return;	// player coming right out of a teleporter.

retry:
	if (!--hitcount)
		goto stairstep; 		// don't loop forever

	// trace along the three leading corners
	if (tryx > 0)
	{
		leadx = mo->X() + mo->radius;
		trailx = mo->X() - mo->radius;
	}
	else
	{
		leadx = mo->X() - mo->radius;
		trailx = mo->X() + mo->radius;
	}

	if (tryy > 0)
	{
		leady = mo->Y() + mo->radius;
		traily = mo->Y() - mo->radius;
	}
	else
	{
		leady = mo->Y() - mo->radius;
		traily = mo->Y() + mo->radius;
	}

	bestslidefrac = FRACUNIT + 1;

	SlideTraverse(leadx, leady, leadx + tryx, leady + tryy);
	SlideTraverse(trailx, leady, trailx + tryx, leady + tryy);
	SlideTraverse(leadx, traily, leadx + tryx, traily + tryy);

	// move up to the wall
	if (bestslidefrac == FRACUNIT + 1)
	{
		// the move must have hit the middle, so stairstep
	stairstep:
		// killough 3/15/98: Allow objects to drop off ledges
		xmove = 0, ymove = tryy;
		walkplane = P_CheckSlopeWalk(mo, xmove, ymove);
		if (!P_TryMove(mo, mo->X() + xmove, mo->Y() + ymove, true, walkplane))
		{
			xmove = tryx, ymove = 0;
			walkplane = P_CheckSlopeWalk(mo, xmove, ymove);
			P_TryMove(mo, mo->X() + xmove, mo->Y() + ymove, true, walkplane);
		}
		return;
	}

	// fudge a bit to make sure it doesn't hit
	bestslidefrac -= FRACUNIT / 32;
	if (bestslidefrac > 0)
	{
		newx = FixedMul(tryx, bestslidefrac);
		newy = FixedMul(tryy, bestslidefrac);

		// [BL] We need to abandon this function if we end up going through a teleporter
		const fixed_t startvelx = mo->velx;
		const fixed_t startvely = mo->vely;

		// killough 3/15/98: Allow objects to drop off ledges
		if (!P_TryMove(mo, mo->X() + newx, mo->Y() + newy, true))
			goto stairstep;

		if (mo->velx != startvelx || mo->vely != startvely)
			return;
	}

	// Now continue along the wall.
	bestslidefrac = FRACUNIT - (bestslidefrac + FRACUNIT / 32);	// remainder
	if (bestslidefrac > FRACUNIT)
		bestslidefrac = FRACUNIT;
	else if (bestslidefrac <= 0)
		return;

	tryx = tmxmove = FixedMul(tryx, bestslidefrac);
	tryy = tmymove = FixedMul(tryy, bestslidefrac);

	HitSlideLine(bestslideline); 	// clip the moves

	mo->velx = tmxmove * numsteps;
	mo->vely = tmymove * numsteps;

	// killough 10/98: affect the bobbing the same way (but not voodoo dolls)
	if (mo->player && mo->player->mo == mo)
	{
		if (abs(mo->player->velx) > abs(mo->velx))
			mo->player->velx = mo->velx;
		if (abs(mo->player->vely) > abs(mo->vely))
			mo->player->vely = mo->vely;
	}

	walkplane = P_CheckSlopeWalk(mo, tmxmove, tmymove);

	// killough 3/15/98: Allow objects to drop off ledges
	if (!P_TryMove(mo, mo->X() + tmxmove, mo->Y() + tmymove, true, walkplane))
	{
		goto retry;
	}
}

void P_SlideMove(AActor *mo, fixed_t tryx, fixed_t tryy, int numsteps)
{
	FSlide slide;
	slide.SlideMove(mo, tryx, tryy, numsteps);
}

//============================================================================
//
// P_CheckSlopeWalk
//
//============================================================================

const secplane_t * P_CheckSlopeWalk(AActor *actor, fixed_t &xmove, fixed_t &ymove)
{
	static secplane_t copyplane;
	if (actor->flags & MF_NOGRAVITY)
	{
		return NULL;
	}

	const secplane_t *plane = &actor->floorsector->floorplane;
	fixed_t planezhere = plane->ZatPoint(actor);

	for (unsigned int i = 0; i<actor->floorsector->e->XFloor.ffloors.Size(); i++)
	{
		F3DFloor * rover = actor->floorsector->e->XFloor.ffloors[i];
		if (!(rover->flags & FF_SOLID) || !(rover->flags & FF_EXISTS)) continue;

		fixed_t thisplanez = rover->top.plane->ZatPoint(actor);

		if (thisplanez>planezhere && thisplanez <= actor->Z() + actor->MaxStepHeight)
		{
			copyplane = *rover->top.plane;
			if (copyplane.c<0) copyplane.FlipVert();
			plane = &copyplane;
			planezhere = thisplanez;
		}
	}

	if (actor->floorsector != actor->Sector)
	{
		for (unsigned int i = 0; i<actor->Sector->e->XFloor.ffloors.Size(); i++)
		{
			F3DFloor * rover = actor->Sector->e->XFloor.ffloors[i];
			if (!(rover->flags & FF_SOLID) || !(rover->flags & FF_EXISTS)) continue;

			fixed_t thisplanez = rover->top.plane->ZatPoint(actor);

			if (thisplanez>planezhere && thisplanez <= actor->Z() + actor->MaxStepHeight)
			{
				copyplane = *rover->top.plane;
				if (copyplane.c<0) copyplane.FlipVert();
				plane = &copyplane;
				planezhere = thisplanez;
			}
		}
	}

	if (actor->floorsector != actor->Sector)
	{
		// this additional check prevents sliding on sloped dropoffs
		if (planezhere>actor->floorz + 4 * FRACUNIT)
			return NULL;
	}

	if (actor->Z() - planezhere > FRACUNIT)
	{ // not on floor
		return NULL;
	}

	if ((plane->a | plane->b) != 0)
	{
		fixed_t destx, desty;
		fixed_t t;

		destx = actor->X() + xmove;
		desty = actor->Y() + ymove;
		t = TMulScale16(plane->a, destx, plane->b, desty, plane->c, actor->Z()) + plane->d;
		if (t < 0)
		{ // Desired location is behind (below) the plane
			// (i.e. Walking up the plane)
			if (plane->c < STEEPSLOPE)
			{ // Can't climb up slopes of ~45 degrees or more
				if (actor->flags & MF_NOCLIP)
				{
					return (actor->floorsector == actor->Sector) ? plane : NULL;
				}
				else
				{
					const msecnode_t *node;
					bool dopush = true;

					if (plane->c > STEEPSLOPE * 2 / 3)
					{
						for (node = actor->touching_sectorlist; node; node = node->m_tnext)
						{
							const sector_t *sec = node->m_sector;
							if (sec->floorplane.c >= STEEPSLOPE)
							{
								if (sec->floorplane.ZatPoint(destx, desty) >= actor->Z() - actor->MaxStepHeight)
								{
									dopush = false;
									break;
								}
							}
						}
					}
					if (dopush)
					{
						xmove = actor->velx = plane->a * 2;
						ymove = actor->vely = plane->b * 2;
					}
					return (actor->floorsector == actor->Sector) ? plane : NULL;
				}
			}
			// Slide the desired location along the plane's normal
			// so that it lies on the plane's surface
			destx -= FixedMul(plane->a, t);
			desty -= FixedMul(plane->b, t);
			xmove = destx - actor->X();
			ymove = desty - actor->Y();
			return (actor->floorsector == actor->Sector) ? plane : NULL;
		}
		else if (t > 0)
		{ // Desired location is in front of (above) the plane
			if (planezhere == actor->Z())
			{ // Actor's current spot is on/in the plane, so walk down it
				// Same principle as walking up, except reversed
				destx += FixedMul(plane->a, t);
				desty += FixedMul(plane->b, t);
				xmove = destx - actor->X();
				ymove = desty - actor->Y();
				return (actor->floorsector == actor->Sector) ? plane : NULL;
			}
		}
	}
	return NULL;
}

//============================================================================
//
// PTR_BounceTraverse
//
//============================================================================

bool FSlide::BounceTraverse(fixed_t startx, fixed_t starty, fixed_t endx, fixed_t endy)
{
	FLineOpening open;
	FPathTraverse it(startx, starty, endx, endy, PT_ADDLINES);
	intercept_t *in;

	while ((in = it.Next()))
	{

		line_t  *li;

		if (!in->isaline)
		{
			Printf("PTR_BounceTraverse: not a line?");
			continue;
		}

		li = in->d.line;
		assert(((size_t)li - (size_t)lines) % sizeof(line_t) == 0);
		if (li->flags & ML_BLOCKEVERYTHING)
		{
			goto bounceblocking;
		}
		if (!(li->flags&ML_TWOSIDED) || !li->backsector)
		{
			if (P_PointOnLineSide(slidemo->X(), slidemo->Y(), li))
				continue;			// don't hit the back side
			goto bounceblocking;
		}


		P_LineOpening(open, slidemo, li, it.Trace().x + FixedMul(it.Trace().dx, in->frac),
			it.Trace().y + FixedMul(it.Trace().dy, in->frac));	// set openrange, opentop, openbottom
		if (open.range < slidemo->height)
			goto bounceblocking;				// doesn't fit

		if (open.top - slidemo->Z() < slidemo->height)
			goto bounceblocking;				// mobj is too high

		if (open.bottom > slidemo->Z())
			goto bounceblocking;				// mobj is too low

		continue;			// this line doesn't block movement

		// the line does block movement, see if it is closer than best so far
	bounceblocking:
		if (in->frac < bestslidefrac)
		{
			secondslidefrac = bestslidefrac;
			secondslideline = bestslideline;
			bestslidefrac = in->frac;
			bestslideline = li;
		}
		return false;   // stop
	}
	return true;
}

//============================================================================
//
// P_BounceWall
//
//============================================================================

bool FSlide::BounceWall(AActor *mo)
{
	fixed_t         leadx, leady;
	int             side;
	angle_t         lineangle, moveangle, deltaangle;
	fixed_t         movelen;
	line_t			*line;

	if (!(mo->BounceFlags & BOUNCE_Walls))
	{
		return false;
	}

	slidemo = mo;
	//
	// trace along the three leading corners
	//
	if (mo->velx > 0)
	{
		leadx = mo->X() + mo->radius;
	}
	else
	{
		leadx = mo->X() - mo->radius;
	}
	if (mo->vely > 0)
	{
		leady = mo->Y() + mo->radius;
	}
	else
	{
		leady = mo->Y() - mo->radius;
	}
	bestslidefrac = FRACUNIT + 1;
	bestslideline = mo->BlockingLine;
	if (BounceTraverse(leadx, leady, leadx + mo->velx, leady + mo->vely) && mo->BlockingLine == NULL)
	{ // Could not find a wall, so bounce off the floor/ceiling instead.
		fixed_t floordist = mo->Z() - mo->floorz;
		fixed_t ceildist = mo->ceilingz - mo->Z();
		if (floordist <= ceildist)
		{
			mo->FloorBounceMissile(mo->Sector->floorplane);
			return true;
		}
		else
		{
			mo->FloorBounceMissile(mo->Sector->ceilingplane);
			return true;
		}
	}
	line = bestslideline;

	if (line->special == Line_Horizon)
	{
		mo->SeeSound = 0;	// it might make a sound otherwise
		mo->Destroy();
		return true;
	}

	// The amount of bounces is limited
	if (mo->bouncecount>0 && --mo->bouncecount == 0)
	{
		if (mo->flags & MF_MISSILE)
			P_ExplodeMissile(mo, line, NULL);
		else
			mo->Die(NULL, NULL);
		return true;
	}

	side = P_PointOnLineSide(mo->X(), mo->Y(), line);
	lineangle = R_PointToAngle2(0, 0, line->dx, line->dy);
	if (side == 1)
	{
		lineangle += ANG180;
	}
	moveangle = R_PointToAngle2(0, 0, mo->velx, mo->vely);
	deltaangle = (2 * lineangle) - moveangle;
	mo->angle = deltaangle;

	deltaangle >>= ANGLETOFINESHIFT;

	movelen = fixed_t(sqrt(double(mo->velx)*mo->velx + double(mo->vely)*mo->vely));
	movelen = FixedMul(movelen, mo->wallbouncefactor);

	FBoundingBox box(mo->X(), mo->Y(), mo->radius);
	if (box.BoxOnLineSide(line) == -1)
	{
		fixedvec3 pos = mo->Vec3Offset(
			FixedMul(mo->radius, finecosine[deltaangle]),
			FixedMul(mo->radius, finesine[deltaangle]), 0);
		mo->SetOrigin(pos, true);

	}
	if (movelen < FRACUNIT)
	{
		movelen = 2 * FRACUNIT;
	}
	mo->velx = FixedMul(movelen, finecosine[deltaangle]);
	mo->vely = FixedMul(movelen, finesine[deltaangle]);
	if (mo->BounceFlags & BOUNCE_UseBounceState)
	{
		FState *bouncestate = mo->FindState(NAME_Bounce, NAME_Wall);
		if (bouncestate != NULL)
		{
			mo->SetState(bouncestate);
		}
	}
	return true;
}

bool P_BounceWall(AActor *mo)
{
	FSlide slide;
	return slide.BounceWall(mo);
}

//==========================================================================
//
//
//
//==========================================================================

extern FRandom pr_bounce;
bool P_BounceActor(AActor *mo, AActor *BlockingMobj, bool ontop)
{
	//Don't go through all of this if the actor is reflective and wants things to pass through them.
	if (BlockingMobj && ((BlockingMobj->flags2 & MF2_REFLECTIVE) && (BlockingMobj->flags7 & MF7_THRUREFLECT))) return true;
	if (mo && BlockingMobj && ((mo->BounceFlags & BOUNCE_AllActors)
		|| ((mo->flags & MF_MISSILE) && (!(mo->flags2 & MF2_RIP) 
		|| (BlockingMobj->flags5 & MF5_DONTRIP) 
		|| ((mo->flags6 & MF6_NOBOSSRIP) && (BlockingMobj->flags2 & MF2_BOSS))) && (BlockingMobj->flags2 & MF2_REFLECTIVE))
		|| ((BlockingMobj->player == NULL) && (!(BlockingMobj->flags3 & MF3_ISMONSTER)))))
	{
		if (mo->bouncecount > 0 && --mo->bouncecount == 0) return false;

		if (mo->flags7 & MF7_HITTARGET)	mo->target = BlockingMobj;
		if (mo->flags7 & MF7_HITMASTER)	mo->master = BlockingMobj;
		if (mo->flags7 & MF7_HITTRACER)	mo->tracer = BlockingMobj;

		if (!ontop)
		{
			fixed_t speed;
			angle_t angle = BlockingMobj->AngleTo(mo) + ANGLE_1*((pr_bounce() % 16) - 8);
			speed = P_AproxDistance(mo->velx, mo->vely);
			speed = FixedMul(speed, mo->wallbouncefactor); // [GZ] was 0.75, using wallbouncefactor seems more consistent
			mo->angle = angle;
			angle >>= ANGLETOFINESHIFT;
			mo->velx = FixedMul(speed, finecosine[angle]);
			mo->vely = FixedMul(speed, finesine[angle]);
			mo->PlayBounceSound(true);
			if (mo->BounceFlags & BOUNCE_UseBounceState)
			{
				FName names[] = { NAME_Bounce, NAME_Actor, NAME_Creature };
				FState *bouncestate;
				int count = 2;

				if ((BlockingMobj->flags & MF_SHOOTABLE) && !(BlockingMobj->flags & MF_NOBLOOD))
				{
					count = 3;
				}
				bouncestate = mo->FindState(count, names);
				if (bouncestate != NULL)
				{
					mo->SetState(bouncestate);
				}
			}
		}
		else
		{
			fixed_t dot = mo->velz;

			if (mo->BounceFlags & (BOUNCE_HereticType | BOUNCE_MBF))
			{
				mo->velz -= MulScale15(FRACUNIT, dot);
				if (!(mo->BounceFlags & BOUNCE_MBF)) // Heretic projectiles die, MBF projectiles don't.
				{
					mo->flags |= MF_INBOUNCE;
					mo->SetState(mo->FindState(NAME_Death));
					mo->flags &= ~MF_INBOUNCE;
					return false;
				}
				else
				{
					mo->velz = FixedMul(mo->velz, mo->bouncefactor);
				}
			}
			else // Don't run through this for MBF-style bounces
			{
				// The reflected velocity keeps only about 70% of its original speed
				mo->velz = FixedMul(mo->velz - MulScale15(FRACUNIT, dot), mo->bouncefactor);
			}

			mo->PlayBounceSound(true);
			if (mo->BounceFlags & BOUNCE_MBF) // Bring it to rest below a certain speed
			{
				if (abs(mo->velz) < (fixed_t)(mo->Mass * mo->GetGravity() / 64))
					mo->velz = 0;
			}
			else if (mo->BounceFlags & (BOUNCE_AutoOff | BOUNCE_AutoOffFloorOnly))
			{
				if (!(mo->flags & MF_NOGRAVITY) && (mo->velz < 3 * FRACUNIT))
					mo->BounceFlags &= ~BOUNCE_TypeMask;
			}
		}
		return true;
	}
	return false;
}

//============================================================================
//
// Aiming
//
//============================================================================

struct aim_t
{
	fixed_t			aimpitch;
	fixed_t			attackrange;
	fixed_t			shootz;			// Height if not aiming up or down
	AActor*			shootthing;
	AActor*			friender;		// actor to check friendliness again

	fixed_t			toppitch, bottompitch;
	AActor *		linetarget;
	AActor *		thing_friend, *thing_other;
	angle_t			pitch_friend, pitch_other;
	int				flags;
	sector_t *		lastsector;
	secplane_t *	lastfloorplane;
	secplane_t *	lastceilingplane;

	bool			crossedffloors;

	bool AimTraverse3DFloors(const divline_t &trace, intercept_t * in);

	void AimTraverse(fixed_t startx, fixed_t starty, fixed_t endx, fixed_t endy, AActor *target = NULL);

};

//============================================================================
//
// AimTraverse3DFloors
//
//============================================================================
bool aim_t::AimTraverse3DFloors(const divline_t &trace, intercept_t * in)
{
	sector_t * nextsector;
	secplane_t * nexttopplane, *nextbottomplane;
	line_t * li = in->d.line;

	nextsector = NULL;
	nexttopplane = nextbottomplane = NULL;

	if (li->backsector == NULL) return true;	// shouldn't really happen but crashed once for me...
	if (li->frontsector->e->XFloor.ffloors.Size() || li->backsector->e->XFloor.ffloors.Size())
	{
		int  frontflag;
		F3DFloor* rover;
		int    highpitch, lowpitch;

		fixed_t trX = trace.x + FixedMul(trace.dx, in->frac);
		fixed_t trY = trace.y + FixedMul(trace.dy, in->frac);
		fixed_t dist = FixedMul(attackrange, in->frac);

		frontflag = P_PointOnLineSide(shootthing->X(), shootthing->Y(), li);

		// 3D floor check. This is not 100% accurate but normally sufficient when
		// combined with a final sight check
		for (int i = 1; i <= 2; i++)
		{
			sector_t * s = i == 1 ? li->frontsector : li->backsector;

			for (unsigned k = 0; k<s->e->XFloor.ffloors.Size(); k++)
			{
				crossedffloors = true;
				rover = s->e->XFloor.ffloors[k];

				if ((rover->flags & FF_SHOOTTHROUGH) || !(rover->flags & FF_EXISTS)) continue;

				fixed_t ff_bottom = rover->bottom.plane->ZatPoint(trX, trY);
				fixed_t ff_top = rover->top.plane->ZatPoint(trX, trY);


				highpitch = -(int)R_PointToAngle2(0, shootz, dist, ff_top);
				lowpitch = -(int)R_PointToAngle2(0, shootz, dist, ff_bottom);

				if (highpitch <= toppitch)
				{
					// blocks completely
					if (lowpitch >= bottompitch) return false;
					// blocks upper edge of view
					if (lowpitch>toppitch)
					{
						toppitch = lowpitch;
						if (frontflag != i - 1)
						{
							nexttopplane = rover->bottom.plane;
						}
					}
				}
				else if (lowpitch >= bottompitch)
				{
					// blocks lower edge of view
					if (highpitch<bottompitch)
					{
						bottompitch = highpitch;
						if (frontflag != i - 1)
						{
							nextbottomplane = rover->top.plane;
						}
					}
				}
				// trace is leaving a sector with a 3d-floor

				if (frontflag == i - 1)
				{
					if (s == lastsector)
					{
						// upper slope intersects with this 3d-floor
						if (rover->bottom.plane == lastceilingplane && lowpitch > toppitch)
						{
							toppitch = lowpitch;
						}
						// lower slope intersects with this 3d-floor
						if (rover->top.plane == lastfloorplane && highpitch < bottompitch)
						{
							bottompitch = highpitch;
						}
					}
				}
				if (toppitch >= bottompitch) return false;		// stop
			}
		}
	}

	lastsector = nextsector;
	lastceilingplane = nexttopplane;
	lastfloorplane = nextbottomplane;
	return true;
}

//============================================================================
//
// PTR_AimTraverse
// Sets linetaget and aimpitch when a target is aimed at.
//
//============================================================================

void aim_t::AimTraverse(fixed_t startx, fixed_t starty, fixed_t endx, fixed_t endy, AActor *target)
{
	FPathTraverse it(startx, starty, endx, endy, PT_ADDLINES | PT_ADDTHINGS | PT_COMPATIBLE);
	intercept_t *in;

	while ((in = it.Next()))
	{
		line_t* 			li;
		AActor* 			th;
		fixed_t 			pitch;
		fixed_t 			thingtoppitch;
		fixed_t 			thingbottompitch;
		fixed_t 			dist;
		int					thingpitch;

		if (in->isaline)
		{
			li = in->d.line;

			if (!(li->flags & ML_TWOSIDED) || (li->flags & ML_BLOCKEVERYTHING))
				return;				// stop

			// Crosses a two sided line.
			// A two sided line will restrict the possible target ranges.
			FLineOpening open;
			P_LineOpening(open, NULL, li, it.Trace().x + FixedMul(it.Trace().dx, in->frac),
				it.Trace().y + FixedMul(it.Trace().dy, in->frac));

			if (open.bottom >= open.top)
				return;				// stop

			dist = FixedMul(attackrange, in->frac);

			pitch = -(int)R_PointToAngle2(0, shootz, dist, open.bottom);
			if (pitch < bottompitch)
				bottompitch = pitch;

			pitch = -(int)R_PointToAngle2(0, shootz, dist, open.top);
			if (pitch > toppitch)
				toppitch = pitch;

			if (toppitch >= bottompitch)
				return;				// stop

			if (!AimTraverse3DFloors(it.Trace(), in)) return;
			continue;					// shot continues
		}

		// shoot a thing
		th = in->d.thing;
		if (th == shootthing)
			continue;					// can't shoot self

		if (target != NULL && th != target)
			continue;					// only care about target, and you're not it

		// If we want to start a conversation anything that has one should be
		// found, regardless of other settings.
		if (!(flags & ALF_CHECKCONVERSATION) || th->Conversation == NULL)
		{
			if (!(flags & ALF_CHECKNONSHOOTABLE))			// For info CCMD, ignore stuff about GHOST and SHOOTABLE flags
			{
				if (!(th->flags&MF_SHOOTABLE))
					continue;					// corpse or something

				// check for physical attacks on a ghost
				if ((th->flags3 & MF3_GHOST) &&
					shootthing->player &&	// [RH] Be sure shootthing is a player
					shootthing->player->ReadyWeapon &&
					(shootthing->player->ReadyWeapon->flags2 & MF2_THRUGHOST))
				{
					continue;
				}
			}
		}
		dist = FixedMul(attackrange, in->frac);

		// Don't autoaim certain special actors
		if (!cl_doautoaim && th->flags6 & MF6_NOTAUTOAIMED)
		{
			continue;
		}

		// we must do one last check whether the trace has crossed a 3D floor
		if (lastsector == th->Sector && th->Sector->e->XFloor.ffloors.Size())
		{
			if (lastceilingplane)
			{
				fixed_t ff_top = lastceilingplane->ZatPoint(th);
				fixed_t pitch = -(int)R_PointToAngle2(0, shootz, dist, ff_top);
				// upper slope intersects with this 3d-floor
				if (pitch > toppitch)
				{
					toppitch = pitch;
				}
			}
			if (lastfloorplane)
			{
				fixed_t ff_bottom = lastfloorplane->ZatPoint(th);
				fixed_t pitch = -(int)R_PointToAngle2(0, shootz, dist, ff_bottom);
				// lower slope intersects with this 3d-floor
				if (pitch < bottompitch)
				{
					bottompitch = pitch;
				}
			}
		}

		// check angles to see if the thing can be aimed at

		thingtoppitch = -(int)R_PointToAngle2(0, shootz, dist, th->Z() + th->height);

		if (thingtoppitch > bottompitch)
			continue;					// shot over the thing

		thingbottompitch = -(int)R_PointToAngle2(0, shootz, dist, th->Z());

		if (thingbottompitch < toppitch)
			continue;					// shot under the thing

		if (crossedffloors)
		{
			// if 3D floors were in the way do an extra visibility check for safety
			if (!P_CheckSight(shootthing, th, SF_IGNOREVISIBILITY | SF_IGNOREWATERBOUNDARY))
			{
				// the thing can't be seen so we can safely exclude its range from our aiming field
				if (thingtoppitch<toppitch)
				{
					if (thingbottompitch>toppitch) toppitch = thingbottompitch;
				}
				else if (thingbottompitch>bottompitch)
				{
					if (thingtoppitch<bottompitch) bottompitch = thingtoppitch;
				}
				if (toppitch < bottompitch) continue;
				else return;
			}
		}

		// this thing can be hit!
		if (thingtoppitch < toppitch)
			thingtoppitch = toppitch;

		if (thingbottompitch > bottompitch)
			thingbottompitch = bottompitch;

		thingpitch = thingtoppitch / 2 + thingbottompitch / 2;

		if (flags & ALF_CHECK3D)
		{
			// We need to do a 3D distance check here because this is nearly always used in
			// combination with P_LineAttack. P_LineAttack uses 3D distance but FPathTraverse
			// only 2D. This causes some problems with Hexen's weapons that use different
			// attack modes based on distance to target
			fixed_t cosine = finecosine[thingpitch >> ANGLETOFINESHIFT];
			if (cosine != 0)
			{
				fixed_t d3 = FixedDiv(FixedMul(P_AproxDistance(it.Trace().dx, it.Trace().dy), in->frac), cosine);
				if (d3 > attackrange)
				{
					return;
				}
			}
		}

		if ((flags & ALF_NOFRIENDS) && th->IsFriend(friender))
		{
			continue;
		}
		else if (sv_smartaim != 0 && !(flags & ALF_FORCENOSMART))
		{
			// try to be a little smarter about what to aim at!
			// In particular avoid autoaiming at friends and barrels.
			if (th->IsFriend(friender))
			{
				if (sv_smartaim < 2)
				{
					// friends don't aim at friends (except players), at least not first
					thing_friend = th;
					pitch_friend = thingpitch;
				}
			}
			else if (!(th->flags3 & MF3_ISMONSTER) && th->player == NULL)
			{
				if (sv_smartaim < 3)
				{
					// don't autoaim at barrels and other shootable stuff unless no monsters have been found
					thing_other = th;
					pitch_other = thingpitch;
				}
			}
			else
			{
				linetarget = th;
				aimpitch = thingpitch;
				return;
			}
		}
		else
		{
			linetarget = th;
			aimpitch = thingpitch;
			return;
		}
	}
}

//============================================================================
//
// P_AimLineAttack
//
//============================================================================

fixed_t P_AimLineAttack(AActor *t1, angle_t angle, fixed_t distance, AActor **pLineTarget, fixed_t vrange,
	int flags, AActor *target, AActor *friender)
{
	fixed_t x2;
	fixed_t y2;
	aim_t aim;

	angle >>= ANGLETOFINESHIFT;
	aim.flags = flags;
	aim.shootthing = t1;
	aim.friender = (friender == NULL) ? t1 : friender;

	x2 = t1->X() + (distance >> FRACBITS)*finecosine[angle];
	y2 = t1->Y() + (distance >> FRACBITS)*finesine[angle];
	aim.shootz = t1->Z() + (t1->height >> 1) - t1->floorclip;
	if (t1->player != NULL)
	{
		aim.shootz += FixedMul(t1->player->mo->AttackZOffset, t1->player->crouchfactor);
	}
	else
	{
		aim.shootz += 8 * FRACUNIT;
	}

	// can't shoot outside view angles
	if (vrange == 0)
	{
		if (t1->player == NULL || !level.IsFreelookAllowed())
		{
			vrange = ANGLE_1 * 35;
		}
		else
		{
			// [BB] Disable autoaim on weapons with WIF_NOAUTOAIM.
			AWeapon *weapon = t1->player->ReadyWeapon;
			if (weapon && (weapon->WeaponFlags & WIF_NOAUTOAIM))
			{
				vrange = ANGLE_1 / 2;
			}
			else
			{
				// 35 degrees is approximately what Doom used. You cannot have a
				// vrange of 0 degrees, because then toppitch and bottompitch will
				// be equal, and PTR_AimTraverse will never find anything to shoot at
				// if it crosses a line.
				vrange = clamp(t1->player->userinfo.GetAimDist(), ANGLE_1 / 2, ANGLE_1 * 35);
			}
		}
	}
	aim.toppitch = t1->pitch - vrange;
	aim.bottompitch = t1->pitch + vrange;

	aim.attackrange = distance;
	aim.linetarget = NULL;

	// for smart aiming
	aim.thing_friend = aim.thing_other = NULL;

	// Information for tracking crossed 3D floors
	aim.aimpitch = t1->pitch;

	aim.crossedffloors = t1->Sector->e->XFloor.ffloors.Size() != 0;
	aim.lastsector = t1->Sector;
	aim.lastfloorplane = aim.lastceilingplane = NULL;

	// set initial 3d-floor info
	for (unsigned i = 0; i<t1->Sector->e->XFloor.ffloors.Size(); i++)
	{
		F3DFloor * rover = t1->Sector->e->XFloor.ffloors[i];
		fixed_t bottomz = rover->bottom.plane->ZatPoint(t1);

		if (bottomz >= t1->Top()) aim.lastceilingplane = rover->bottom.plane;

		bottomz = rover->top.plane->ZatPoint(t1);
		if (bottomz <= t1->Z()) aim.lastfloorplane = rover->top.plane;
	}

	aim.AimTraverse(t1->X(), t1->Y(), x2, y2, target);

	if (!aim.linetarget)
	{
		if (aim.thing_other)
		{
			aim.linetarget = aim.thing_other;
			aim.aimpitch = aim.pitch_other;
		}
		else if (aim.thing_friend)
		{
			aim.linetarget = aim.thing_friend;
			aim.aimpitch = aim.pitch_friend;
		}
	}
	if (pLineTarget)
	{
		*pLineTarget = aim.linetarget;
	}
	return aim.linetarget ? aim.aimpitch : t1->pitch;
}


//==========================================================================
//
//
//
//==========================================================================

struct Origin
{
	AActor *Caller;
	bool hitGhosts;
	bool hitSameSpecies;
};

static ETraceStatus CheckForActor(FTraceResults &res, void *userdata)
{
	if (res.HitType != TRACE_HitActor)
	{
		return TRACE_Stop;
	}

	Origin *data = (Origin *)userdata;

	// check for physical attacks on spectrals
	if (res.Actor->flags4 & MF4_SPECTRAL)
	{
		return TRACE_Skip;
	}

	if (data->hitSameSpecies && res.Actor->GetSpecies() == data->Caller->GetSpecies()) 
	{
		return TRACE_Skip;
	}
	if (data->hitGhosts && res.Actor->flags3 & MF3_GHOST)
	{
		return TRACE_Skip;
	}

	return TRACE_Stop;
}

//==========================================================================
//
// P_LineAttack
//
// if damage == 0, it is just a test trace that will leave linetarget set
//
//==========================================================================

AActor *P_LineAttack(AActor *t1, angle_t angle, fixed_t distance,
	int pitch, int damage, FName damageType, const PClass *pufftype, int flags, AActor **victim, int *actualdamage)
{
	fixed_t vx, vy, vz, shootz;
	FTraceResults trace;
	Origin TData;
	TData.Caller = t1;
	angle_t srcangle = angle;
	int srcpitch = pitch;
	bool killPuff = false;
	AActor *puff = NULL;
	int pflag = 0;
	int puffFlags = (flags & LAF_ISMELEEATTACK) ? PF_MELEERANGE : 0;
	if (flags & LAF_NORANDOMPUFFZ)
		puffFlags |= PF_NORANDOMZ;

	if (victim != NULL)
	{
		*victim = NULL;
	}
	if (actualdamage != NULL)
	{
		*actualdamage = 0;
	}

	angle >>= ANGLETOFINESHIFT;
	pitch = (angle_t)(pitch) >> ANGLETOFINESHIFT;

	vx = FixedMul(finecosine[pitch], finecosine[angle]);
	vy = FixedMul(finecosine[pitch], finesine[angle]);
	vz = -finesine[pitch];

	shootz = t1->Z() - t1->floorclip + (t1->height >> 1);
	if (t1->player != NULL)
	{
		shootz += FixedMul(t1->player->mo->AttackZOffset, t1->player->crouchfactor);
		if (damageType == NAME_Melee || damageType == NAME_Hitscan)
		{
			// this is coming from a weapon attack function which needs to transfer information to the obituary code,
			// We need to preserve this info from the damage type because the actual damage type can get overridden by the puff
			pflag = DMG_PLAYERATTACK;
		}
	}
	else
	{
		shootz += 8 * FRACUNIT;
	}

	// We need to check the defaults of the replacement here
	AActor *puffDefaults = GetDefaultByType(pufftype->GetReplacement());

	TData.hitGhosts = (t1->player != NULL &&
		t1->player->ReadyWeapon != NULL &&
		(t1->player->ReadyWeapon->flags2 & MF2_THRUGHOST)) ||
		(puffDefaults && (puffDefaults->flags2 & MF2_THRUGHOST));

	TData.hitSameSpecies = (puffDefaults && (puffDefaults->flags6 & MF6_MTHRUSPECIES));

	// if the puff uses a non-standard damage type, this will override default, hitscan and melee damage type.
	// All other explicitly passed damage types (currenty only MDK) will be preserved.
	if ((damageType == NAME_None || damageType == NAME_Melee || damageType == NAME_Hitscan) &&
		puffDefaults != NULL && puffDefaults->DamageType != NAME_None)
	{
		damageType = puffDefaults->DamageType;
	}

	int tflags;
	if (puffDefaults != NULL && puffDefaults->flags6 & MF6_NOTRIGGER) tflags = TRACE_NoSky;
	else tflags = TRACE_NoSky | TRACE_Impact;

	if (!Trace(t1->X(), t1->Y(), shootz, t1->Sector, vx, vy, vz, distance,
		MF_SHOOTABLE, ML_BLOCKEVERYTHING | ML_BLOCKHITSCAN, t1, trace,
		tflags, CheckForActor, &TData))
	{ // hit nothing
		if (puffDefaults == NULL)
		{
		}
		else if (puffDefaults->ActiveSound)
		{ // Play miss sound
			S_Sound(t1, CHAN_WEAPON, puffDefaults->ActiveSound, 1, ATTN_NORM);
		}
		if (puffDefaults != NULL && puffDefaults->flags3 & MF3_ALWAYSPUFF)
		{ // Spawn the puff anyway
			puff = P_SpawnPuff(t1, pufftype, trace.X, trace.Y, trace.Z, angle - ANG180, 2, puffFlags);
		}
		else
		{
			return NULL;
		}
	}
	else
	{
		fixed_t hitx = 0, hity = 0, hitz = 0;

		if (trace.HitType != TRACE_HitActor)
		{
			// position a bit closer for puffs
			if (trace.HitType != TRACE_HitWall || trace.Line->special != Line_Horizon)
			{
				fixed_t closer = trace.Distance - 4 * FRACUNIT;
				fixedvec2 pos = t1->Vec2Offset(FixedMul(vx, closer), FixedMul(vy, closer));
				puff = P_SpawnPuff(t1, pufftype, pos.x, pos.y,
					shootz + FixedMul(vz, closer), angle - ANG90, 0, puffFlags);
			}

			// [RH] Spawn a decal
			if (trace.HitType == TRACE_HitWall && trace.Line->special != Line_Horizon && !(flags & LAF_NOIMPACTDECAL) && !(puffDefaults->flags7 & MF7_NODECAL))
			{
				// [TN] If the actor or weapon has a decal defined, use that one.
				if (t1->DecalGenerator != NULL ||
					(t1->player != NULL && t1->player->ReadyWeapon != NULL && t1->player->ReadyWeapon->DecalGenerator != NULL))
				{
					// [ZK] If puff has FORCEDECAL set, do not use the weapon's decal
					if (puffDefaults->flags7 & MF7_FORCEDECAL && puff != NULL && puff->DecalGenerator)
						SpawnShootDecal(puff, trace);
					else
						SpawnShootDecal(t1, trace);
				}

				// Else, look if the bulletpuff has a decal defined.
				else if (puff != NULL && puff->DecalGenerator)
				{
					SpawnShootDecal(puff, trace);
				}

				else
				{
					SpawnShootDecal(t1, trace);
				}
			}
			else if (puff != NULL &&
				trace.CrossedWater == NULL &&
				trace.Sector->heightsec == NULL &&
				trace.HitType == TRACE_HitFloor)
			{
				// Using the puff's position is not accurate enough.
				// Instead make it splash at the actual hit position
				hitx = t1->X() + FixedMul(vx, trace.Distance);
				hity = t1->Y() + FixedMul(vy, trace.Distance);
				hitz = shootz + FixedMul(vz, trace.Distance);
				P_HitWater(puff, P_PointInSector(hitx, hity), hitx, hity, hitz);
			}
		}
		else
		{
			bool bloodsplatter = (t1->flags5 & MF5_BLOODSPLATTER) ||
				(t1->player != NULL &&	t1->player->ReadyWeapon != NULL &&
				(t1->player->ReadyWeapon->WeaponFlags & WIF_AXEBLOOD));

			bool axeBlood = (t1->player != NULL &&
				t1->player->ReadyWeapon != NULL &&
				(t1->player->ReadyWeapon->WeaponFlags & WIF_AXEBLOOD));

			// Hit a thing, so it could be either a puff or blood
			fixed_t dist = trace.Distance;
			// position a bit closer for puffs/blood if using compatibility mode.
			if (i_compatflags & COMPATF_HITSCAN) dist -= 10 * FRACUNIT;
			hitx = t1->X() + FixedMul(vx, dist);
			hity = t1->Y() + FixedMul(vy, dist);
			hitz = shootz + FixedMul(vz, dist);

			

			// Spawn bullet puffs or blood spots, depending on target type.
			if ((puffDefaults != NULL && puffDefaults->flags3 & MF3_PUFFONACTORS) ||
				(trace.Actor->flags & MF_NOBLOOD) ||
				(trace.Actor->flags2 & (MF2_INVULNERABLE | MF2_DORMANT)))
			{
				if (!(trace.Actor->flags & MF_NOBLOOD))
					puffFlags |= PF_HITTHINGBLEED;

				// We must pass the unreplaced puff type here 
				puff = P_SpawnPuff(t1, pufftype, hitx, hity, hitz, angle - ANG180, 2, puffFlags | PF_HITTHING, trace.Actor);
			}

			// Allow puffs to inflict poison damage, so that hitscans can poison, too.
			if (puffDefaults != NULL && puffDefaults->PoisonDamage > 0 && puffDefaults->PoisonDuration != INT_MIN)
			{
				P_PoisonMobj(trace.Actor, puff ? puff : t1, t1, puffDefaults->PoisonDamage, puffDefaults->PoisonDuration, puffDefaults->PoisonPeriod, puffDefaults->PoisonDamageType);
			}

			// [GZ] If MF6_FORCEPAIN is set, we need to call P_DamageMobj even if damage is 0!
			// Note: The puff may not yet be spawned here so we must check the class defaults, not the actor.
			int newdam = damage;
			if (damage || (puffDefaults != NULL && ((puffDefaults->flags6 & MF6_FORCEPAIN) || (puffDefaults->flags7 & MF7_CAUSEPAIN))))
			{
				int dmgflags = DMG_INFLICTOR_IS_PUFF | pflag;
				// Allow MF5_PIERCEARMOR on a weapon as well.
				if (t1->player != NULL && (dmgflags & DMG_PLAYERATTACK) && t1->player->ReadyWeapon != NULL &&
					t1->player->ReadyWeapon->flags5 & MF5_PIERCEARMOR)
				{
					dmgflags |= DMG_NO_ARMOR;
				}
				
				if (puff == NULL)
				{
					// Since the puff is the damage inflictor we need it here 
					// regardless of whether it is displayed or not.
					puff = P_SpawnPuff(t1, pufftype, hitx, hity, hitz, angle - ANG180, 2, puffFlags | PF_HITTHING | PF_TEMPORARY);
					killPuff = true;
				}
				newdam = P_DamageMobj(trace.Actor, puff ? puff : t1, t1, damage, damageType, dmgflags);
				if (actualdamage != NULL)
				{
					*actualdamage = newdam;
				}
			}
			if (!(puffDefaults != NULL && puffDefaults->flags3&MF3_BLOODLESSIMPACT))
			{
				if (!bloodsplatter && !axeBlood &&
					!(trace.Actor->flags & MF_NOBLOOD) &&
					!(trace.Actor->flags2 & (MF2_INVULNERABLE | MF2_DORMANT)))
				{
					P_SpawnBlood(hitx, hity, hitz, angle - ANG180, newdam > 0 ? newdam : damage, trace.Actor);
				}

				if (damage)
				{
					if (bloodsplatter || axeBlood)
					{
						if (!(trace.Actor->flags&MF_NOBLOOD) &&
							!(trace.Actor->flags2&(MF2_INVULNERABLE | MF2_DORMANT)))
						{
							if (axeBlood)
							{
								P_BloodSplatter2(hitx, hity, hitz, trace.Actor);
							}
							if (pr_lineattack() < 192)
							{
								P_BloodSplatter(hitx, hity, hitz, trace.Actor);
							}
						}
					}
					// [RH] Stick blood to walls
					P_TraceBleed(newdam > 0 ? newdam : damage, trace.X, trace.Y, trace.Z,
						trace.Actor, srcangle, srcpitch);
				}
			}
			if (victim != NULL)
			{
				*victim = trace.Actor;
			}
		}
		if (trace.Crossed3DWater || trace.CrossedWater)
		{

			if (puff == NULL)
			{ // Spawn puff just to get a mass for the splash
				puff = P_SpawnPuff(t1, pufftype, hitx, hity, hitz, angle - ANG180, 2, puffFlags | PF_HITTHING | PF_TEMPORARY);
				killPuff = true;
			}
			SpawnDeepSplash(t1, trace, puff, vx, vy, vz, shootz, trace.Crossed3DWater != NULL);
		}
	}
	if (killPuff && puff != NULL)
	{
		puff->Destroy();
		puff = NULL;
	}
	return puff;
}

AActor *P_LineAttack(AActor *t1, angle_t angle, fixed_t distance,
	int pitch, int damage, FName damageType, FName pufftype, int flags, AActor **victim, int *actualdamage)
{
	const PClass * type = PClass::FindClass(pufftype);
	if (victim != NULL)
	{
		*victim = NULL;
	}
	if (type == NULL)
	{
		Printf("Attempt to spawn unknown actor type '%s'\n", pufftype.GetChars());
	}
	else
	{
		return P_LineAttack(t1, angle, distance, pitch, damage, damageType, type, flags, victim, actualdamage);
	}
	return NULL;
}

//==========================================================================
//
// P_LinePickActor
//
//==========================================================================

AActor *P_LinePickActor(AActor *t1, angle_t angle, fixed_t distance, int pitch,
						ActorFlags actorMask, DWORD wallMask)
{
	fixed_t vx, vy, vz, shootz;
	
	angle >>= ANGLETOFINESHIFT;
	pitch = (angle_t)(pitch) >> ANGLETOFINESHIFT;

	vx = FixedMul(finecosine[pitch], finecosine[angle]);
	vy = FixedMul(finecosine[pitch], finesine[angle]);
	vz = -finesine[pitch];

	shootz = t1->Z() - t1->floorclip + (t1->height >> 1);
	if (t1->player != NULL)
	{
		shootz += FixedMul(t1->player->mo->AttackZOffset, t1->player->crouchfactor);
	}
	else
	{
		shootz += 8 * FRACUNIT;
	}

	FTraceResults trace;
	Origin TData;
	
	TData.Caller = t1;
	TData.hitGhosts = true;
	
	if (Trace(t1->X(), t1->Y(), shootz, t1->Sector, vx, vy, vz, distance,
		actorMask, wallMask, t1, trace, TRACE_NoSky, CheckForActor, &TData))
	{
		if (trace.HitType == TRACE_HitActor)
		{
			return trace.Actor;
		}
	}

	return NULL;
}

//==========================================================================
//
//
//
//==========================================================================

void P_TraceBleed(int damage, fixed_t x, fixed_t y, fixed_t z, AActor *actor, angle_t angle, int pitch)
{
	if (!cl_bloodsplats)
		return;

	const char *bloodType = "BloodSplat";
	int count;
	int noise;


	if ((actor->flags & MF_NOBLOOD) ||
		(actor->flags5 & MF5_NOBLOODDECALS) ||
		(actor->flags2 & (MF2_INVULNERABLE | MF2_DORMANT)) ||
		(actor->player && actor->player->cheats & CF_GODMODE))
	{
		return;
	}
	if (damage < 15)
	{	// For low damages, there is a chance to not spray blood at all
		if (damage <= 10)
		{
			if (pr_tracebleed() < 160)
			{
				return;
			}
		}
		count = 1;
		noise = 18;
	}
	else if (damage < 25)
	{
		count = 2;
		noise = 19;
	}
	else
	{	// For high damages, there is a chance to spray just one big glob of blood
		if (pr_tracebleed() < 24)
		{
			bloodType = "BloodSmear";
			count = 1;
			noise = 20;
		}
		else
		{
			count = 3;
			noise = 20;
		}
	}

	for (; count; --count)
	{
		FTraceResults bleedtrace;

		angle_t bleedang = (angle + ((pr_tracebleed() - 128) << noise)) >> ANGLETOFINESHIFT;
		angle_t bleedpitch = (angle_t)(pitch + ((pr_tracebleed() - 128) << noise)) >> ANGLETOFINESHIFT;
		fixed_t vx = FixedMul(finecosine[bleedpitch], finecosine[bleedang]);
		fixed_t vy = FixedMul(finecosine[bleedpitch], finesine[bleedang]);
		fixed_t vz = -finesine[bleedpitch];

		if (Trace(x, y, z, actor->Sector,
			vx, vy, vz, 172 * FRACUNIT, 0, ML_BLOCKEVERYTHING, actor,
			bleedtrace, TRACE_NoSky))
		{
			if (bleedtrace.HitType == TRACE_HitWall)
			{
				PalEntry bloodcolor = actor->GetBloodColor();
				if (bloodcolor != 0)
				{
					bloodcolor.r >>= 1;	// the full color is too bright for blood decals
					bloodcolor.g >>= 1;
					bloodcolor.b >>= 1;
					bloodcolor.a = 1;
				}

				DImpactDecal::StaticCreate(bloodType,
					bleedtrace.X, bleedtrace.Y, bleedtrace.Z,
					bleedtrace.Line->sidedef[bleedtrace.Side],
					bleedtrace.ffloor,
					bloodcolor);
			}
		}
	}
}

void P_TraceBleed(int damage, AActor *target, angle_t angle, int pitch)
{
	P_TraceBleed(damage, target->X(), target->Y(), target->Z() + target->height / 2,
		target, angle, pitch);
}

//==========================================================================
//
//
//
//==========================================================================

void P_TraceBleed(int damage, AActor *target, AActor *missile)
{
	int pitch;

	if (target == NULL || missile->flags3 & MF3_BLOODLESSIMPACT)
	{
		return;
	}

	if (missile->velz != 0)
	{
		double aim;

		aim = atan((double)missile->velz / (double)target->AproxDistance(missile));
		pitch = -(int)(aim * ANGLE_180 / PI);
	}
	else
	{
		pitch = 0;
	}
	P_TraceBleed(damage, target->X(), target->Y(), target->Z() + target->height / 2,
		target, missile->AngleTo(target),
		pitch);
}

//==========================================================================
//
//
//
//==========================================================================

void P_TraceBleed(int damage, AActor *target)
{
	if (target != NULL)
	{
		fixed_t one = pr_tracebleed() << 24;
		fixed_t two = (pr_tracebleed() - 128) << 16;

		P_TraceBleed(damage, target->X(), target->Y(), target->Z() + target->height / 2,
			target, one, two);
	}
}

//==========================================================================
//
// [RH] Rail gun stuffage
//
//==========================================================================

struct SRailHit
{
	AActor *HitActor;
	fixed_t Distance;
};
struct RailData
{
	AActor *Caller;
	TArray<SRailHit> RailHits;
	bool StopAtOne;
	bool StopAtInvul;
	bool ThruSpecies;
};

static ETraceStatus ProcessRailHit(FTraceResults &res, void *userdata)
{
	RailData *data = (RailData *)userdata;
	if (res.HitType != TRACE_HitActor)
	{
		return TRACE_Stop;
	}

	// Invulnerable things completely block the shot
	if (data->StopAtInvul && res.Actor->flags2 & MF2_INVULNERABLE)
	{
		return TRACE_Stop;
	}

	// Skip actors with the same species if the puff has MTHRUSPECIES.
	if (data->ThruSpecies && res.Actor->GetSpecies() == data->Caller->GetSpecies())
	{
		return TRACE_Skip;
	}

	// Save this thing for damaging later, and continue the trace
	SRailHit newhit;
	newhit.HitActor = res.Actor;
	newhit.Distance = res.Distance - 10 * FRACUNIT;	// put blood in front
	data->RailHits.Push(newhit);

	return data->StopAtOne ? TRACE_Stop : TRACE_Continue;
}

//==========================================================================
//
//
//
//==========================================================================
void P_RailAttack(AActor *source, int damage, int offset_xy, fixed_t offset_z, int color1, int color2, double maxdiff, int railflags, const PClass *puffclass, angle_t angleoffset, angle_t pitchoffset, fixed_t distance, int duration, double sparsity, double drift, const PClass *spawnclass, int SpiralOffset)
{
	fixed_t vx, vy, vz;
	angle_t angle, pitch;
	TVector3<double> start, end;
	FTraceResults trace;
	fixed_t shootz;

	if (puffclass == NULL) puffclass = PClass::FindClass(NAME_BulletPuff);

	pitch = ((angle_t)(-source->pitch) + pitchoffset) >> ANGLETOFINESHIFT;
	angle = (source->angle + angleoffset) >> ANGLETOFINESHIFT;

	vx = FixedMul(finecosine[pitch], finecosine[angle]);
	vy = FixedMul(finecosine[pitch], finesine[angle]);
	vz = finesine[pitch];

	shootz = source->Z() - source->floorclip + (source->height >> 1) + offset_z;

	if (!(railflags & RAF_CENTERZ))
	{
		if (source->player != NULL)
		{
			shootz += FixedMul(source->player->mo->AttackZOffset, source->player->crouchfactor);
		}
		else
		{
			shootz += 8 * FRACUNIT;
		}
	}

	angle = ((source->angle + angleoffset) - ANG90) >> ANGLETOFINESHIFT;

	fixedvec2 xy = source->Vec2Offset(offset_xy * finecosine[angle], offset_xy * finesine[angle]);

	RailData rail_data;
	rail_data.Caller = source;
	
	rail_data.StopAtOne = !!(railflags & RAF_NOPIERCE);
	start.X = FIXED2FLOAT(xy.x);
	start.Y = FIXED2FLOAT(xy.y);
	start.Z = FIXED2FLOAT(shootz);

	int flags;

	assert(puffclass != NULL);		// Because we set it to a default above
	AActor *puffDefaults = GetDefaultByType(puffclass->GetReplacement()); //Contains all the flags such as FOILINVUL, etc.

	flags = (puffDefaults->flags6 & MF6_NOTRIGGER) ? 0 : TRACE_PCross | TRACE_Impact;
	rail_data.StopAtInvul = (puffDefaults->flags3 & MF3_FOILINVUL) ? false : true;
	rail_data.ThruSpecies = (puffDefaults->flags6 & MF6_MTHRUSPECIES) ? true : false;
	Trace(xy.x, xy.y, shootz, source->Sector, vx, vy, vz,
		distance, MF_SHOOTABLE, ML_BLOCKEVERYTHING, source, trace,
		flags, ProcessRailHit, &rail_data);

	// Hurt anything the trace hit
	unsigned int i;
	FName damagetype = (puffDefaults == NULL || puffDefaults->DamageType == NAME_None) ? FName(NAME_Railgun) : puffDefaults->DamageType;

	// used as damage inflictor
	AActor *thepuff = NULL;

	if (puffclass != NULL) thepuff = Spawn(puffclass, source->Pos(), ALLOW_REPLACE);

	for (i = 0; i < rail_data.RailHits.Size(); i++)
	{
		

		fixed_t x, y, z;
		bool spawnpuff;
		bool bleed = false;

		int puffflags = PF_HITTHING;
		AActor *hitactor = rail_data.RailHits[i].HitActor;
		fixed_t hitdist = rail_data.RailHits[i].Distance;		

		x = xy.x + FixedMul(hitdist, vx);
		y = xy.y + FixedMul(hitdist, vy);
		z = shootz + FixedMul(hitdist, vz);

		if ((hitactor->flags & MF_NOBLOOD) ||
			(hitactor->flags2 & MF2_DORMANT || ((hitactor->flags2 & MF2_INVULNERABLE) && !(puffDefaults->flags3 & MF3_FOILINVUL))))
		{
			spawnpuff = (puffclass != NULL);
		}
		else
		{
			spawnpuff = (puffclass != NULL && puffDefaults->flags3 & MF3_ALWAYSPUFF);
			puffflags |= PF_HITTHINGBLEED; // [XA] Allow for puffs to jump to XDeath state.
			if (!(puffDefaults->flags3 & MF3_BLOODLESSIMPACT))
			{
				bleed = true;
			}
		}
		if (spawnpuff)
		{
			P_SpawnPuff(source, puffclass, x, y, z, (source->angle + angleoffset) - ANG90, 1, puffflags, hitactor);
		}
		
		int dmgFlagPass = DMG_INFLICTOR_IS_PUFF;
		if (puffDefaults != NULL)	// is this even possible?
		{
			if (puffDefaults->PoisonDamage > 0 && puffDefaults->PoisonDuration != INT_MIN)
			{
				P_PoisonMobj(hitactor, thepuff ? thepuff : source, source, puffDefaults->PoisonDamage, puffDefaults->PoisonDuration, puffDefaults->PoisonPeriod, puffDefaults->PoisonDamageType);
			}
			if (puffDefaults->flags3 & MF3_FOILINVUL) dmgFlagPass |= DMG_FOILINVUL;
			if (puffDefaults->flags7 & MF7_FOILBUDDHA) dmgFlagPass |= DMG_FOILBUDDHA;
		}
		int newdam = P_DamageMobj(hitactor, thepuff ? thepuff : source, source, damage, damagetype, dmgFlagPass);

		if (bleed)
		{
			P_SpawnBlood(x, y, z, (source->angle + angleoffset) - ANG180, newdam > 0 ? newdam : damage, hitactor);
			P_TraceBleed(newdam > 0 ? newdam : damage, x, y, z, hitactor, source->angle, pitch);
		}
	}

	// Spawn a decal or puff at the point where the trace ended.
	if (trace.HitType == TRACE_HitWall)
	{
		AActor* puff = NULL;

		if (puffclass != NULL && puffDefaults->flags3 & MF3_ALWAYSPUFF)
		{
			puff = P_SpawnPuff(source, puffclass, trace.X, trace.Y, trace.Z, (source->angle + angleoffset) - ANG90, 1, 0);
			if (puff && (trace.Line != NULL) && (trace.Line->special == Line_Horizon) && !(puff->flags3 & MF3_SKYEXPLODE))
				puff->Destroy();
		}
		if (puff != NULL && puffDefaults->flags7 & MF7_FORCEDECAL && puff->DecalGenerator)
			SpawnShootDecal(puff, trace);
		else
			SpawnShootDecal(source, trace);

	}
	if (trace.HitType == TRACE_HitFloor || trace.HitType == TRACE_HitCeiling)
	{
		AActor* puff = NULL;
		if (puffclass != NULL && puffDefaults->flags3 & MF3_ALWAYSPUFF)
		{
			puff = P_SpawnPuff(source, puffclass, trace.X, trace.Y, trace.Z, (source->angle + angleoffset) - ANG90, 1, 0);
			if (puff && !(puff->flags3 & MF3_SKYEXPLODE) &&
				(((trace.HitType == TRACE_HitFloor) && (puff->floorpic == skyflatnum)) ||
				((trace.HitType == TRACE_HitCeiling) && (puff->ceilingpic == skyflatnum))))
			{
				puff->Destroy();
			}
		}
	}
	if (thepuff != NULL)
	{
		if (trace.HitType == TRACE_HitFloor &&
			trace.CrossedWater == NULL &&
			trace.Sector->heightsec == NULL)
		{
			thepuff->SetOrigin(trace.X, trace.Y, trace.Z, false);
			P_HitWater(thepuff, trace.Sector);
		}
		if (trace.Crossed3DWater || trace.CrossedWater)
		{
			SpawnDeepSplash(source, trace, thepuff, vx, vy, vz, shootz, trace.Crossed3DWater != NULL);
		}
		thepuff->Destroy();
	}

	// Draw the slug's trail.
	end.X = FIXED2DBL(trace.X);
	end.Y = FIXED2DBL(trace.Y);
	end.Z = FIXED2DBL(trace.Z);
	P_DrawRailTrail(source, start, end, color1, color2, maxdiff, railflags, spawnclass, source->angle + angleoffset, duration, sparsity, drift, SpiralOffset);
}

//==========================================================================
//
// [RH] P_AimCamera
//
//==========================================================================

CVAR(Float, chase_height, -8.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, chase_dist, 90.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

void P_AimCamera(AActor *t1, fixed_t &CameraX, fixed_t &CameraY, fixed_t &CameraZ, sector_t *&CameraSector)
{
	fixed_t distance = (fixed_t)(clamp<double>(chase_dist, 0, 30000) * FRACUNIT);
	angle_t angle = (t1->angle - ANG180) >> ANGLETOFINESHIFT;
	angle_t pitch = (angle_t)(t1->pitch) >> ANGLETOFINESHIFT;
	FTraceResults trace;
	fixed_t vx, vy, vz, sz;

	vx = FixedMul(finecosine[pitch], finecosine[angle]);
	vy = FixedMul(finecosine[pitch], finesine[angle]);
	vz = finesine[pitch];

	sz = t1->Z() - t1->floorclip + t1->height + (fixed_t)(clamp<double>(chase_height, -1000, 1000) * FRACUNIT);

	if (Trace(t1->X(), t1->Y(), sz, t1->Sector,
		vx, vy, vz, distance, 0, 0, NULL, trace) &&
		trace.Distance > 10 * FRACUNIT)
	{
		// Position camera slightly in front of hit thing
		fixed_t dist = trace.Distance - 5 * FRACUNIT;
		CameraX = t1->X() + FixedMul(vx, dist);
		CameraY = t1->Y() + FixedMul(vy, dist);
		CameraZ = sz + FixedMul(vz, dist);
	}
	else
	{
		CameraX = trace.X;
		CameraY = trace.Y;
		CameraZ = trace.Z;
	}
	CameraSector = trace.Sector;
}


//==========================================================================
//
// P_TalkFacing
//
// Looks for something within 5.625 degrees left or right of the player
// to talk to.
//
//==========================================================================

bool P_TalkFacing(AActor *player)
{
	AActor *linetarget;

	P_AimLineAttack(player, player->angle, TALKRANGE, &linetarget, ANGLE_1 * 35, ALF_FORCENOSMART | ALF_CHECKCONVERSATION);
	if (linetarget == NULL)
	{
		P_AimLineAttack(player, player->angle + (ANGLE_90 >> 4), TALKRANGE, &linetarget, ANGLE_1 * 35, ALF_FORCENOSMART | ALF_CHECKCONVERSATION);
		if (linetarget == NULL)
		{
			P_AimLineAttack(player, player->angle - (ANGLE_90 >> 4), TALKRANGE, &linetarget, ANGLE_1 * 35, ALF_FORCENOSMART | ALF_CHECKCONVERSATION);
			if (linetarget == NULL)
			{
				return false;
			}
		}
	}
	// Dead things can't talk.
	if (linetarget->health <= 0)
	{
		return false;
	}
	// Fighting things don't talk either.
	if (linetarget->flags4 & MF4_INCOMBAT)
	{
		return false;
	}
	if (linetarget->Conversation != NULL)
	{
		// Give the NPC a chance to play a brief animation
		linetarget->ConversationAnimation(0);
		P_StartConversation(linetarget, player, true, true);
		return true;
	}
	return false;
}

//==========================================================================
//
// USE LINES
//
//==========================================================================

bool P_UseTraverse(AActor *usething, fixed_t endx, fixed_t endy, bool &foundline)
{
	FPathTraverse it(usething->X(), usething->Y(), endx, endy, PT_ADDLINES | PT_ADDTHINGS);
	intercept_t *in;

	while ((in = it.Next()))
	{
		// [RH] Check for things to talk with or use a puzzle item on
		if (!in->isaline)
		{
			if (usething == in->d.thing)
				continue;
			// Check thing

			// Check for puzzle item use or USESPECIAL flag
			// Extended to use the same activationtype mechanism as BUMPSPECIAL does
			if (in->d.thing->flags5 & MF5_USESPECIAL || in->d.thing->special == UsePuzzleItem)
			{
				if (P_ActivateThingSpecial(in->d.thing, usething))
					return true;
			}
			continue;
		}

		FLineOpening open;
		if (in->d.line->special == 0 || !(in->d.line->activation & (SPAC_Use | SPAC_UseThrough | SPAC_UseBack)))
		{
		blocked:
			if (in->d.line->flags & (ML_BLOCKEVERYTHING | ML_BLOCKUSE))
			{
				open.range = 0;
			}
			else
			{
				P_LineOpening(open, NULL, in->d.line, it.Trace().x + FixedMul(it.Trace().dx, in->frac),
					it.Trace().y + FixedMul(it.Trace().dy, in->frac));
			}
			if (open.range <= 0 ||
				(in->d.line->special != 0 && (i_compatflags & COMPATF_USEBLOCKING)))
			{
				// [RH] Give sector a chance to intercept the use

				sector_t * sec;

				sec = usething->Sector;

				if (sec->SecActTarget && sec->SecActTarget->TriggerAction(usething, SECSPAC_Use))
				{
					return true;
				}

				sec = P_PointOnLineSide(usething->X(), usething->Y(), in->d.line) == 0 ?
					in->d.line->frontsector : in->d.line->backsector;

				if (sec != NULL && sec->SecActTarget &&
					sec->SecActTarget->TriggerAction(usething, SECSPAC_UseWall))
				{
					return true;
				}

				if (usething->player)
				{
					S_Sound(usething, CHAN_VOICE, "*usefail", 1, ATTN_IDLE);
				}
				return true;		// can't use through a wall
			}
			foundline = true;
			continue;			// not a special line, but keep checking
		}

		if (P_PointOnLineSide(usething->X(), usething->Y(), in->d.line) == 1)
		{
			if (!(in->d.line->activation & SPAC_UseBack))
			{
				// [RH] continue traversal for two-sided lines
				//return in->d.line->backsector != NULL;		// don't use back side
				goto blocked;	// do a proper check for back sides of triggers
			}
			else
			{
				P_ActivateLine(in->d.line, usething, 1, SPAC_UseBack);
				return true;
			}
		}
		else
		{
			if ((in->d.line->activation & (SPAC_Use | SPAC_UseThrough | SPAC_UseBack)) == SPAC_UseBack)
			{
				goto blocked; // Line cannot be used from front side so treat it as a non-trigger line
			}

			P_ActivateLine(in->d.line, usething, 0, SPAC_Use);

			//WAS can't use more than one special line in a row
			//jff 3/21/98 NOW multiple use allowed with enabling line flag
			//[RH] And now I've changed it again. If the line is of type
			//	   SPAC_USE, then it eats the use. Everything else passes
			//	   it through, including SPAC_USETHROUGH.
			if (i_compatflags & COMPATF_USEBLOCKING)
			{
				if (in->d.line->activation & SPAC_UseThrough) continue;
				else return true;
			}
			else
			{
				if (!(in->d.line->activation & SPAC_Use)) continue;
				else return true;
			}

		}

	}
	return false;
}

//==========================================================================
//
// Returns false if a "oof" sound should be made because of a blocking
// linedef. Makes 2s middles which are impassable, as well as 2s uppers
// and lowers which block the player, cause the sound effect when the
// player tries to activate them. Specials are excluded, although it is
// assumed that all special linedefs within reach have been considered
// and rejected already (see P_UseLines).
//
// by Lee Killough
//
//==========================================================================

bool P_NoWayTraverse(AActor *usething, fixed_t endx, fixed_t endy)
{
	FPathTraverse it(usething->X(), usething->Y(), endx, endy, PT_ADDLINES);
	intercept_t *in;

	while ((in = it.Next()))
	{
		line_t *ld = in->d.line;
		FLineOpening open;

		// [GrafZahl] de-obfuscated. Was I the only one who was unable to make sense out of
		// this convoluted mess?
		if (ld->special) continue;
		if (ld->flags&(ML_BLOCKING | ML_BLOCKEVERYTHING | ML_BLOCK_PLAYERS)) return true;
		P_LineOpening(open, NULL, ld, it.Trace().x + FixedMul(it.Trace().dx, in->frac),
			it.Trace().y + FixedMul(it.Trace().dy, in->frac));
		if (open.range <= 0 ||
			open.bottom > usething->Z() + usething->MaxStepHeight ||
			open.top < usething->Top()) return true;
	}
	return false;
}

//==========================================================================
//
// P_UseLines
//
// Looks for special lines in front of the player to activate
//
//==========================================================================

void P_UseLines(player_t *player)
{
	bool foundline = false;

	// [NS] Now queries the Player's UseRange.
	fixedvec2 end = player->mo->Vec2Angle(player->mo->UseRange, player->mo->angle, true);

	// old code:
	//
	// P_PathTraverse ( x1, y1, x2, y2, PT_ADDLINES, PTR_UseTraverse );
	//
	// This added test makes the "oof" sound work on 2s lines -- killough:

	if (!P_UseTraverse(player->mo, end.x, end.y, foundline))
	{ // [RH] Give sector a chance to eat the use
		sector_t *sec = player->mo->Sector;
		int spac = SECSPAC_Use;
		if (foundline) spac |= SECSPAC_UseWall;
		if ((!sec->SecActTarget || !sec->SecActTarget->TriggerAction(player->mo, spac)) &&
			P_NoWayTraverse(player->mo, end.x, end.y))
		{
			S_Sound(player->mo, CHAN_VOICE, "*usefail", 1, ATTN_IDLE);
		}
	}
}

//==========================================================================
//
// P_UsePuzzleItem
//
// Returns true if the puzzle item was used on a line or a thing.
//
//==========================================================================

bool P_UsePuzzleItem(AActor *PuzzleItemUser, int PuzzleItemType)
{
	int angle;
	fixed_t x1, y1, x2, y2, usedist;

	angle = PuzzleItemUser->angle >> ANGLETOFINESHIFT;
	x1 = PuzzleItemUser->X();
	y1 = PuzzleItemUser->Y();

	// [NS] If it's a Player, get their UseRange.
	if (PuzzleItemUser->player)
		usedist = PuzzleItemUser->player->mo->UseRange;
	else
		usedist = USERANGE;

	x2 = x1 + FixedMul(usedist, finecosine[angle]);
	y2 = y1 + FixedMul(usedist, finesine[angle]);

	FPathTraverse it(x1, y1, x2, y2, PT_ADDLINES | PT_ADDTHINGS);
	intercept_t *in;

	while ((in = it.Next()))
	{
		AActor *mobj;
		FLineOpening open;

		if (in->isaline)
		{ // Check line
			if (in->d.line->special != UsePuzzleItem)
			{
				P_LineOpening(open, NULL, in->d.line, it.Trace().x + FixedMul(it.Trace().dx, in->frac),
					it.Trace().y + FixedMul(it.Trace().dy, in->frac));
				if (open.range <= 0)
				{
					return false; // can't use through a wall
				}
				continue;
			}
			if (P_PointOnLineSide(PuzzleItemUser->X(), PuzzleItemUser->Y(), in->d.line) == 1)
			{ // Don't use back sides
				return false;
			}
			if (PuzzleItemType != in->d.line->args[0])
			{ // Item type doesn't match
				return false;
			}
			int args[3] = { in->d.line->args[2], in->d.line->args[3], in->d.line->args[4] };
			P_StartScript(PuzzleItemUser, in->d.line, in->d.line->args[1], NULL, args, 3, ACS_ALWAYS);
			in->d.line->special = 0;
			return true;
		}
		// Check thing
		mobj = in->d.thing;
		if (mobj->special != UsePuzzleItem)
		{ // Wrong special
			continue;
		}
		if (PuzzleItemType != mobj->args[0])
		{ // Item type doesn't match
			continue;
		}
		int args[3] = { mobj->args[2], mobj->args[3], mobj->args[4] };
		P_StartScript(PuzzleItemUser, NULL, mobj->args[1], NULL, args, 3, ACS_ALWAYS);
		mobj->special = 0;
		return true;
	}
	return false;
}

//==========================================================================
//
// RADIUS ATTACK
//
//
//==========================================================================


// [RH] Damage scale to apply to thing that shot the missile.
static float selfthrustscale;

CUSTOM_CVAR(Float, splashfactor, 1.f, CVAR_SERVERINFO)
{
	if (self <= 0.f)
		self = 1.f;
	else
		selfthrustscale = 1.f / self;
}

//==========================================================================
//
// P_RadiusAttack
// Source is the creature that caused the explosion at spot.
//
//==========================================================================

void P_RadiusAttack(AActor *bombspot, AActor *bombsource, int bombdamage, int bombdistance, FName bombmod,
	int flags, int fulldamagedistance)
{
	if (bombdistance <= 0)
		return;
	fulldamagedistance = clamp<int>(fulldamagedistance, 0, bombdistance - 1);

	double bombdistancefloat = 1.f / (double)(bombdistance - fulldamagedistance);
	double bombdamagefloat = (double)bombdamage;

	FBlockThingsIterator it(FBoundingBox(bombspot->X(), bombspot->Y(), bombdistance << FRACBITS));
	AActor *thing;

	if (flags & RADF_SOURCEISSPOT)
	{ // The source is actually the same as the spot, even if that wasn't what we received.
		bombsource = bombspot;
	}

	while ((thing = it.Next()))
	{
		// Vulnerable actors can be damaged by radius attacks even if not shootable
		// Used to emulate MBF's vulnerability of non-missile bouncers to explosions.
		if (!((thing->flags & MF_SHOOTABLE) || (thing->flags6 & MF6_VULNERABLE)))
			continue;

		// Boss spider and cyborg and Heretic's ep >= 2 bosses
		// take no damage from concussion.
		if (thing->flags3 & MF3_NORADIUSDMG && !(bombspot->flags4 & MF4_FORCERADIUSDMG))
			continue;

		if (!(flags & RADF_HURTSOURCE) && (thing == bombsource || thing == bombspot))
		{ // don't damage the source of the explosion
			continue;
		}

		// a much needed option: monsters that fire explosive projectiles cannot 
		// be hurt by projectiles fired by a monster of the same type.
		// Controlled by the DONTHARMCLASS and DONTHARMSPECIES flags.
		if ((bombsource && !thing->player) // code common to both checks
			&& ( // Class check first
			((bombsource->flags4 & MF4_DONTHARMCLASS) && (thing->GetClass() == bombsource->GetClass()))
			|| // Nigh-identical species check second
			((bombsource->flags6 & MF6_DONTHARMSPECIES) && (thing->GetSpecies() == bombsource->GetSpecies()))
			)
			)	continue;

		// Barrels always use the original code, since this makes
		// them far too "active." BossBrains also use the old code
		// because some user levels require they have a height of 16,
		// which can make them near impossible to hit with the new code.
		if ((flags & RADF_NODAMAGE) || !((bombspot->flags5 | thing->flags5) & MF5_OLDRADIUSDMG))
		{
			// [RH] New code. The bounding box only covers the
			// height of the thing and not the height of the map.
			double points;
			double len;
			fixed_t dx, dy;
			double boxradius;

			fixedvec2 vec = bombspot->Vec2To(thing);
			dx = abs(vec.x);
			dy = abs(vec.y);
			boxradius = double(thing->radius);

			// The damage pattern is square, not circular.
			len = double(dx > dy ? dx : dy);

			if (bombspot->Z() < thing->Z() || bombspot->Z() >= thing->Top())
			{
				double dz;

				if (bombspot->Z() > thing->Z())
				{
					dz = double(bombspot->Z() - thing->Top());
				}
				else
				{
					dz = double(thing->Z() - bombspot->Z());
				}
				if (len <= boxradius)
				{
					len = dz;
				}
				else
				{
					len -= boxradius;
					len = sqrt(len*len + dz*dz);
				}
			}
			else
			{
				len -= boxradius;
				if (len < 0.f)
					len = 0.f;
			}
			len /= FRACUNIT;
			len = clamp<double>(len - (double)fulldamagedistance, 0, len);
			points = bombdamagefloat * (1.f - len * bombdistancefloat);
			if (thing == bombsource)
			{
				points = points * splashfactor;
			}
			points *= thing->GetClass()->Meta.GetMetaFixed(AMETA_RDFactor, FRACUNIT) / (double)FRACUNIT;

			// points and bombdamage should be the same sign
			if (((points * bombdamage) > 0) && P_CheckSight(thing, bombspot, SF_IGNOREVISIBILITY | SF_IGNOREWATERBOUNDARY))
			{ // OK to damage; target is in direct path
				double velz;
				double thrust;
				int damage = abs((int)points);
				int newdam = damage;

				if (!(flags & RADF_NODAMAGE))
					newdam = P_DamageMobj(thing, bombspot, bombsource, damage, bombmod);
				else if (thing->player == NULL && (!(flags & RADF_NOIMPACTDAMAGE) && !(thing->flags7 & MF7_DONTTHRUST)))
					thing->flags2 |= MF2_BLASTED;

				if (!(thing->flags & MF_ICECORPSE))
				{
					if (!(flags & RADF_NODAMAGE) && !(bombspot->flags3 & MF3_BLOODLESSIMPACT))
						P_TraceBleed(newdam > 0 ? newdam : damage, thing, bombspot);

					if ((flags & RADF_NODAMAGE) || !(bombspot->flags2 & MF2_NODMGTHRUST))
					{
						if (bombsource == NULL || !(bombsource->flags2 & MF2_NODMGTHRUST))
						{
							if (!(thing->flags7 & MF7_DONTTHRUST))
							{
							
								thrust = points * 0.5f / (double)thing->Mass;
								if (bombsource == thing)
								{
									thrust *= selfthrustscale;
								}
								velz = (double)(thing->Z() + (thing->height >> 1) - bombspot->Z()) * thrust;
								if (bombsource != thing)
								{
									velz *= 0.5f;
								}
								else
								{
									velz *= 0.8f;
								}
								angle_t ang = bombspot->AngleTo(thing) >> ANGLETOFINESHIFT;
								thing->velx += fixed_t(finecosine[ang] * thrust);
								thing->vely += fixed_t(finesine[ang] * thrust);
								if (!(flags & RADF_NODAMAGE))
									thing->velz += (fixed_t)velz;	// this really doesn't work well
							}
						}
					}
				}
			}
		}
		else
		{
			// [RH] Old code just for barrels
			fixed_t dx, dy, dist;

			fixedvec2 vec = bombspot->Vec2To(thing);
			dx = abs(vec.x);
			dy = abs(vec.y);

			dist = dx>dy ? dx : dy;
			dist = (dist - thing->radius) >> FRACBITS;

			if (dist < 0)
				dist = 0;

			if (dist >= bombdistance)
				continue;  // out of range

			if (P_CheckSight(thing, bombspot, SF_IGNOREVISIBILITY | SF_IGNOREWATERBOUNDARY))
			{ // OK to damage; target is in direct path
				dist = clamp<int>(dist - fulldamagedistance, 0, dist);
				int damage = Scale(bombdamage, bombdistance - dist, bombdistance);
				damage = (int)((double)damage * splashfactor);

				damage = Scale(damage, thing->GetClass()->Meta.GetMetaFixed(AMETA_RDFactor, FRACUNIT), FRACUNIT);
				if (damage > 0)
				{
					int newdam = P_DamageMobj(thing, bombspot, bombsource, damage, bombmod);
					P_TraceBleed(newdam > 0 ? newdam : damage, thing, bombspot);
				}
			}
		}
	}
}

//==========================================================================
//
// SECTOR HEIGHT CHANGING
// After modifying a sector's floor or ceiling height,
// call this routine to adjust the positions
// of all things that touch the sector.
//
// If anything doesn't fit anymore, true will be returned.
//
// [RH] If crushchange is non-negative, they will take the
//		specified amount of damage as they are being crushed.
//		If crushchange is negative, you should set the sector
//		height back the way it was and call P_ChangeSector()
//		again to undo the changes.
//		Note that this is very different from the original
//		true/false usage of crushchange! If you want regular
//		DOOM crushing behavior set crushchange to 10 or -1
//		if no crushing is desired.
//
//==========================================================================


struct FChangePosition
{
	sector_t *sector;
	int moveamt;
	int crushchange;
	bool nofit;
	bool movemidtex;
};

TArray<AActor *> intersectors;

EXTERN_CVAR(Int, cl_bloodtype)

//=============================================================================
//
// P_AdjustFloorCeil
//
//=============================================================================

bool P_AdjustFloorCeil(AActor *thing, FChangePosition *cpos)
{
	ActorFlags2 flags2 = thing->flags2 & MF2_PASSMOBJ;
	FCheckPosition tm;

	if ((thing->flags2 & MF2_PASSMOBJ) && (thing->flags3 & MF3_ISMONSTER))
	{
		tm.FromPMove = true;
	}

	if (cpos->movemidtex)
	{
		// From Eternity:
		// ALL things must be treated as PASSMOBJ when moving
		// 3DMidTex lines, otherwise you get stuck in them.
		thing->flags2 |= MF2_PASSMOBJ;
	}

	bool isgood = P_CheckPosition(thing, thing->X(), thing->Y(), tm);
	thing->floorz = tm.floorz;
	thing->ceilingz = tm.ceilingz;
	thing->dropoffz = tm.dropoffz;		// killough 11/98: remember dropoffs
	thing->floorpic = tm.floorpic;
	thing->floorterrain = tm.floorterrain;
	thing->floorsector = tm.floorsector;
	thing->ceilingpic = tm.ceilingpic;
	thing->ceilingsector = tm.ceilingsector;

	// restore the PASSMOBJ flag but leave the other flags alone.
	thing->flags2 = (thing->flags2 & ~MF2_PASSMOBJ) | flags2;

	return isgood;
}

//=============================================================================
//
// P_FindAboveIntersectors
//
//=============================================================================

void P_FindAboveIntersectors(AActor *actor)
{
	if (actor->flags & MF_NOCLIP)
		return;

	if (!(actor->flags & MF_SOLID))
		return;

	AActor *thing;
	FBlockThingsIterator it(FBoundingBox(actor->X(), actor->Y(), actor->radius));
	while ((thing = it.Next()))
	{
		if (!thing->intersects(actor))
		{
			continue;
		}
		if (!(thing->flags & MF_SOLID))
		{ // Can't hit thing
			continue;
		}
		if (thing->flags & (MF_SPECIAL))
		{ // [RH] Corpses and specials don't block moves
			continue;
		}
		if (thing->flags & (MF_CORPSE))
		{ // Corpses need a few more checks
			if (!(actor->flags & MF_ICECORPSE))
				continue;
		}
		if (thing == actor)
		{ // Don't clip against self
			continue;
		}
		if (!((thing->flags2 | actor->flags2) & MF2_PASSMOBJ) && !((thing->flags3 | actor->flags3) & MF3_ISMONSTER))
		{
			// Don't bother if both things don't have MF2_PASSMOBJ set and aren't monsters.
			// These things would always block each other which in nearly every situation is 
			// not what is wanted here.
			continue;
		}
		if (thing->Z() >= actor->Z() &&
			thing->Z() <= actor->Top())
		{ // Thing intersects above the base
			intersectors.Push(thing);
		}
	}
}

//=============================================================================
//
// P_FindBelowIntersectors
//
//=============================================================================

void P_FindBelowIntersectors(AActor *actor)
{
	if (actor->flags & MF_NOCLIP)
		return;

	if (!(actor->flags & MF_SOLID))
		return;

	AActor *thing;
	FBlockThingsIterator it(FBoundingBox(actor->X(), actor->Y(), actor->radius));
	while ((thing = it.Next()))
	{
		if (!thing->intersects(actor))
		{
			continue;
		}
		if (!(thing->flags & MF_SOLID))
		{ // Can't hit thing
			continue;
		}
		if (thing->flags & (MF_SPECIAL))
		{ // [RH] Corpses and specials don't block moves
			continue;
		}
		if (thing->flags & (MF_CORPSE))
		{ // Corpses need a few more checks
			if (!(actor->flags & MF_ICECORPSE))
				continue;
		}
		if (thing == actor)
		{ // Don't clip against self
			continue;
		}
		if (!((thing->flags2 | actor->flags2) & MF2_PASSMOBJ) && !((thing->flags3 | actor->flags3) & MF3_ISMONSTER))
		{
			// Don't bother if both things don't have MF2_PASSMOBJ set and aren't monsters.
			// These things would always block each other which in nearly every situation is 
			// not what is wanted here.
			continue;
		}
		if (thing->Top() <= actor->Top() &&
			thing->Top() > actor->Z())
		{ // Thing intersects below the base
			intersectors.Push(thing);
		}
	}
}

//=============================================================================
//
// P_DoCrunch
//
//=============================================================================

void P_DoCrunch(AActor *thing, FChangePosition *cpos)
{
	if (!(thing && thing->Grind(true) && cpos)) return;
	cpos->nofit = true;

	if ((cpos->crushchange > 0) && !(level.maptime & 3))
	{
		int newdam = P_DamageMobj(thing, NULL, NULL, cpos->crushchange, NAME_Crush);

		// spray blood in a random direction
		if (!(thing->flags2&(MF2_INVULNERABLE | MF2_DORMANT)))
		{
			if (!(thing->flags&MF_NOBLOOD))
			{
				PalEntry bloodcolor = thing->GetBloodColor();
				const PClass *bloodcls = thing->GetBloodType();

				P_TraceBleed(newdam > 0 ? newdam : cpos->crushchange, thing);

				if (bloodcls != NULL)
				{
					AActor *mo;

					mo = Spawn(bloodcls, thing->PosPlusZ(thing->height / 2), ALLOW_REPLACE);

					mo->velx = pr_crunch.Random2() << 12;
					mo->vely = pr_crunch.Random2() << 12;
					if (bloodcolor != 0 && !(mo->flags2 & MF2_DONTTRANSLATE))
					{
						mo->Translation = TRANSLATION(TRANSLATION_Blood, bloodcolor.a);
					}

					if (!(cl_bloodtype <= 1)) mo->renderflags |= RF_INVISIBLE;
				}

				angle_t an;
				an = (M_Random() - 128) << 24;
				if (cl_bloodtype >= 1)
				{
					P_DrawSplash2(32, thing->X(), thing->Y(), thing->Z() + thing->height / 2, an, 2, bloodcolor);
				}
			}
			if (thing->CrushPainSound != 0 && !S_GetSoundPlayingInfo(thing, thing->CrushPainSound))
			{
				S_Sound(thing, CHAN_VOICE, thing->CrushPainSound, 1.f, ATTN_NORM);
			}
		}
	}

	// keep checking (crush other things)
	return;
}

//=============================================================================
//
// P_PushUp
//
// Returns 0 if thing fits, 1 if ceiling got in the way, or 2 if something
// above it didn't fit.
//=============================================================================

int P_PushUp(AActor *thing, FChangePosition *cpos)
{
	unsigned int firstintersect = intersectors.Size();
	unsigned int lastintersect;
	int mymass = thing->Mass;

	if (thing->Top() > thing->ceilingz)
	{
		return 1;
	}
	// [GZ] Skip thing intersect test for THRUACTORS things.
	if (thing->flags2 & MF2_THRUACTORS)
		return 0;
	P_FindAboveIntersectors(thing);
	lastintersect = intersectors.Size();
	for (; firstintersect < lastintersect; firstintersect++)
	{
		AActor *intersect = intersectors[firstintersect];
		// [GZ] Skip this iteration for THRUSPECIES things
		// Should there be MF2_THRUGHOST / MF3_GHOST checks there too for consistency?
		// Or would that risk breaking established behavior? THRUGHOST, like MTHRUSPECIES,
		// is normally for projectiles which would have exploded by now anyway...
		if (thing->flags6 & MF6_THRUSPECIES && thing->GetSpecies() == intersect->GetSpecies())
			continue;
		if ((thing->flags & MF_MISSILE) && (intersect->flags2 & MF2_REFLECTIVE) && (intersect->flags7 & MF7_THRUREFLECT))
			continue;
		if (!(intersect->flags2 & MF2_PASSMOBJ) ||
			(!(intersect->flags3 & MF3_ISMONSTER) && intersect->Mass > mymass) ||
			(intersect->flags4 & MF4_ACTLIKEBRIDGE)
			)
		{
			// Can't push bridges or things more massive than ourself
			return 2;
		}
		fixed_t oldz;
		oldz = intersect->Z();
		P_AdjustFloorCeil(intersect, cpos);
		intersect->SetZ(thing->Top() + 1);
		if (P_PushUp(intersect, cpos))
		{ // Move blocked
			P_DoCrunch(intersect, cpos);
			intersect->SetZ(oldz);
			return 2;
		}
	}
	return 0;
}

//=============================================================================
//
// P_PushDown
//
// Returns 0 if thing fits, 1 if floor got in the way, or 2 if something
// below it didn't fit.
//=============================================================================

int P_PushDown(AActor *thing, FChangePosition *cpos)
{
	unsigned int firstintersect = intersectors.Size();
	unsigned int lastintersect;
	int mymass = thing->Mass;

	if (thing->Z() <= thing->floorz)
	{
		return 1;
	}
	P_FindBelowIntersectors(thing);
	lastintersect = intersectors.Size();
	for (; firstintersect < lastintersect; firstintersect++)
	{
		AActor *intersect = intersectors[firstintersect];
		if (!(intersect->flags2 & MF2_PASSMOBJ) ||
			(!(intersect->flags3 & MF3_ISMONSTER) && intersect->Mass > mymass) ||
			(intersect->flags4 & MF4_ACTLIKEBRIDGE)
			)
		{
			// Can't push bridges or things more massive than ourself
			return 2;
		}
		fixed_t oldz = intersect->Z();
		P_AdjustFloorCeil(intersect, cpos);
		if (oldz > thing->Z() - intersect->height)
		{ // Only push things down, not up.
			intersect->SetZ(thing->Z() - intersect->height);
			if (P_PushDown(intersect, cpos))
			{ // Move blocked
				P_DoCrunch(intersect, cpos);
				intersect->SetZ(oldz);
				return 2;
			}
		}
	}
	return 0;
}

//=============================================================================
//
// PIT_FloorDrop
//
//=============================================================================

void PIT_FloorDrop(AActor *thing, FChangePosition *cpos)
{
	fixed_t oldfloorz = thing->floorz;
	fixed_t oldz = thing->Z();

	P_AdjustFloorCeil(thing, cpos);

	if (oldfloorz == thing->floorz) return;
	if (thing->flags4 & MF4_ACTLIKEBRIDGE) return; // do not move bridge things

	if (thing->velz == 0 &&
		(!(thing->flags & MF_NOGRAVITY) ||
		(thing->Z() == oldfloorz && !(thing->flags & MF_NOLIFTDROP))))
	{
		fixed_t oldz = thing->Z();

		if ((thing->flags & MF_NOGRAVITY) || (thing->flags5 & MF5_MOVEWITHSECTOR) ||
			(((cpos->sector->Flags & SECF_FLOORDROP) || cpos->moveamt < 9 * FRACUNIT)
			&& thing->Z() - thing->floorz <= cpos->moveamt))
		{
			thing->SetZ(thing->floorz);
			P_CheckFakeFloorTriggers(thing, oldz);
		}
	}
	else if ((thing->Z() != oldfloorz && !(thing->flags & MF_NOLIFTDROP)))
	{
		fixed_t oldz = thing->Z();
		if ((thing->flags & MF_NOGRAVITY) && (thing->flags6 & MF6_RELATIVETOFLOOR))
		{
			thing->AddZ(-oldfloorz + thing->floorz);
			P_CheckFakeFloorTriggers(thing, oldz);
		}
	}
	if (thing->player && thing->player->mo == thing)
	{
		thing->player->viewz += thing->Z() - oldz;
	}
}

//=============================================================================
//
// PIT_FloorRaise
//
//=============================================================================

void PIT_FloorRaise(AActor *thing, FChangePosition *cpos)
{
	fixed_t oldfloorz = thing->floorz;
	fixed_t oldz = thing->Z();

	P_AdjustFloorCeil(thing, cpos);

	if (oldfloorz == thing->floorz) return;

	// Move things intersecting the floor up
	if (thing->Z() <= thing->floorz)
	{
		if (thing->flags4 & MF4_ACTLIKEBRIDGE)
		{
			cpos->nofit = true;
			return; // do not move bridge things
		}
		intersectors.Clear();
		thing->SetZ(thing->floorz);
	}
	else
	{
		if ((thing->flags & MF_NOGRAVITY) && (thing->flags6 & MF6_RELATIVETOFLOOR))
		{
			intersectors.Clear();
			thing->AddZ(-oldfloorz + thing->floorz);
		}
		else return;
	}
	switch (P_PushUp(thing, cpos))
	{
	default:
		P_CheckFakeFloorTriggers(thing, oldz);
		break;
	case 1:
		P_DoCrunch(thing, cpos);
		P_CheckFakeFloorTriggers(thing, oldz);
		break;
	case 2:
		P_DoCrunch(thing, cpos);
		thing->SetZ(oldz);
		break;
	}
	if (thing->player && thing->player->mo == thing)
	{
		thing->player->viewz += thing->Z() - oldz;
	}
}

//=============================================================================
//
// PIT_CeilingLower
//
//=============================================================================

void PIT_CeilingLower(AActor *thing, FChangePosition *cpos)
{
	bool onfloor;
	fixed_t oldz = thing->Z();

	onfloor = thing->Z() <= thing->floorz;
	P_AdjustFloorCeil(thing, cpos);

	if (thing->Top() > thing->ceilingz)
	{
		if (thing->flags4 & MF4_ACTLIKEBRIDGE)
		{
			cpos->nofit = true;
			return; // do not move bridge things
		}
		intersectors.Clear();
		fixed_t oldz = thing->Z();
		if (thing->ceilingz - thing->height >= thing->floorz)
		{
			thing->SetZ(thing->ceilingz - thing->height);
		}
		else
		{
			thing->SetZ(thing->floorz);
		}
		switch (P_PushDown(thing, cpos))
		{
		case 2:
			// intentional fall-through
		case 1:
			if (onfloor)
				thing->SetZ(thing->floorz);
			P_DoCrunch(thing, cpos);
			P_CheckFakeFloorTriggers(thing, oldz);
			break;
		default:
			P_CheckFakeFloorTriggers(thing, oldz);
			break;
		}
	}
	if (thing->player && thing->player->mo == thing)
	{
		thing->player->viewz += thing->Z() - oldz;
	}
}

//=============================================================================
//
// PIT_CeilingRaise
//
//=============================================================================

void PIT_CeilingRaise(AActor *thing, FChangePosition *cpos)
{
	bool isgood = P_AdjustFloorCeil(thing, cpos);
	fixed_t oldz = thing->Z();

	if (thing->flags4 & MF4_ACTLIKEBRIDGE) return; // do not move bridge things

	// For DOOM compatibility, only move things that are inside the floor.
	// (or something else?) Things marked as hanging from the ceiling will
	// stay where they are.
	if (thing->Z() < thing->floorz &&
		thing->Top() >= thing->ceilingz - cpos->moveamt &&
		!(thing->flags & MF_NOLIFTDROP))
	{
		fixed_t oldz = thing->Z();
		thing->SetZ(thing->floorz);
		if (thing->Top() > thing->ceilingz)
		{
			thing->SetZ(thing->ceilingz - thing->height);
		}
		P_CheckFakeFloorTriggers(thing, oldz);
	}
	else if ((thing->flags2 & MF2_PASSMOBJ) && !isgood && thing->Top() < thing->ceilingz)
	{
		AActor *onmobj;
		if (!P_TestMobjZ(thing, true, &onmobj) && onmobj->Z() <= thing->Z())
		{
			thing->SetZ( MIN(thing->ceilingz - thing->height, onmobj->Top()));
		}
	}
	if (thing->player && thing->player->mo == thing)
	{
		thing->player->viewz += thing->Z() - oldz;
	}
}

//=============================================================================
//
// P_ChangeSector	[RH] Was P_CheckSector in BOOM
//
// jff 3/19/98 added to just check monsters on the periphery
// of a moving sector instead of all in bounding box of the
// sector. Both more accurate and faster.
//
//=============================================================================

bool P_ChangeSector(sector_t *sector, int crunch, int amt, int floorOrCeil, bool isreset)
{
	FChangePosition cpos;
	void(*iterator)(AActor *, FChangePosition *);
	void(*iterator2)(AActor *, FChangePosition *) = NULL;
	msecnode_t *n;

	cpos.nofit = false;
	cpos.crushchange = crunch;
	cpos.moveamt = abs(amt);
	cpos.movemidtex = false;
	cpos.sector = sector;

	// Also process all sectors that have 3D floors transferred from the
	// changed sector.
	if (sector->e->XFloor.attached.Size())
	{
		unsigned       i;
		sector_t*      sec;


		// Use different functions for the four different types of sector movement.
		// for 3D-floors the meaning of floor and ceiling is inverted!!!
		if (floorOrCeil == 1)
		{
			iterator = (amt >= 0) ? PIT_FloorRaise : PIT_FloorDrop;
		}
		else
		{
			iterator = (amt >= 0) ? PIT_CeilingRaise : PIT_CeilingLower;
		}

		for (i = 0; i < sector->e->XFloor.attached.Size(); i++)
		{
			sec = sector->e->XFloor.attached[i];
			P_Recalculate3DFloors(sec);	// Must recalculate the 3d floor and light lists

			// no thing checks for attached sectors because of heightsec
			if (sec->heightsec == sector) continue;

			for (n = sec->touching_thinglist; n; n = n->m_snext) n->visited = false;
			do
			{
				for (n = sec->touching_thinglist; n; n = n->m_snext)
				{
					if (!n->visited)
					{
						n->visited = true;
						if (!(n->m_thing->flags & MF_NOBLOCKMAP) ||	//jff 4/7/98 don't do these
							(n->m_thing->flags5 & MF5_MOVEWITHSECTOR))
						{
							iterator(n->m_thing, &cpos);
						}
						break;
					}
				}
			} while (n);
		}
	}
	P_Recalculate3DFloors(sector);			// Must recalculate the 3d floor and light lists

	// [RH] Use different functions for the four different types of sector
	// movement.
	switch (floorOrCeil)
	{
	case 0:
		// floor
		iterator = (amt < 0) ? PIT_FloorDrop : PIT_FloorRaise;
		break;

	case 1:
		// ceiling
		iterator = (amt < 0) ? PIT_CeilingLower : PIT_CeilingRaise;
		break;

	case 2:
		// 3dmidtex
		// This must check both floor and ceiling 
		iterator = (amt < 0) ? PIT_FloorDrop : PIT_FloorRaise;
		iterator2 = (amt < 0) ? PIT_CeilingLower : PIT_CeilingRaise;
		cpos.movemidtex = true;
		break;

	default:
		// invalid
		assert(floorOrCeil > 0 && floorOrCeil < 2);
		return false;
	}

	// killough 4/4/98: scan list front-to-back until empty or exhausted,
	// restarting from beginning after each thing is processed. Avoids
	// crashes, and is sure to examine all things in the sector, and only
	// the things which are in the sector, until a steady-state is reached.
	// Things can arbitrarily be inserted and removed and it won't mess up.
	//
	// killough 4/7/98: simplified to avoid using complicated counter

	// Mark all things invalid

	for (n = sector->touching_thinglist; n; n = n->m_snext)
		n->visited = false;

	do
	{
		for (n = sector->touching_thinglist; n; n = n->m_snext)	// go through list
		{
			if (!n->visited)								// unprocessed thing found
			{
				n->visited = true; 							// mark thing as processed
				if (!(n->m_thing->flags & MF_NOBLOCKMAP) ||	//jff 4/7/98 don't do these
					(n->m_thing->flags5 & MF5_MOVEWITHSECTOR))
				{
					iterator(n->m_thing, &cpos);		 			// process it
					if (iterator2 != NULL) iterator2(n->m_thing, &cpos);
				}
				break;										// exit and start over
			}
		}
	} while (n);	// repeat from scratch until all things left are marked valid

	if (!cpos.nofit && !isreset /* && sector->MoreFlags & (SECF_UNDERWATERMASK)*/)
	{
		// If this is a control sector for a deep water transfer, all actors in affected
		// sectors need to have their waterlevel information updated and if applicable,
		// execute appropriate sector actions.
		// Only check if the sector move was successful.
		TArray<sector_t *> & secs = sector->e->FakeFloor.Sectors;
		for (unsigned i = 0; i < secs.Size(); i++)
		{
			sector_t * s = secs[i];

			for (n = s->touching_thinglist; n; n = n->m_snext)
				n->visited = false;

			do
			{
				for (n = s->touching_thinglist; n; n = n->m_snext)	// go through list
				{
					if (!n->visited && n->m_thing->Sector == s)		// unprocessed thing found
					{
						n->visited = true; 							// mark thing as processed

						n->m_thing->UpdateWaterLevel(n->m_thing->Z(), false);
						P_CheckFakeFloorTriggers(n->m_thing, n->m_thing->Z() - amt);
					}
				}
			} while (n);	// repeat from scratch until all things left are marked valid
		}

	}

	return cpos.nofit;
}

//=============================================================================
// phares 3/21/98
//
// Maintain a freelist of msecnode_t's to reduce memory allocs and frees.
//=============================================================================

msecnode_t *headsecnode = NULL;

//=============================================================================
//
// P_GetSecnode
//
// Retrieve a node from the freelist. The calling routine
// should make sure it sets all fields properly.
//
//=============================================================================

msecnode_t *P_GetSecnode()
{
	msecnode_t *node;

	if (headsecnode)
	{
		node = headsecnode;
		headsecnode = headsecnode->m_snext;
	}
	else
	{
		node = (msecnode_t *)M_Malloc(sizeof(*node));
	}
	return node;
}

//=============================================================================
//
// P_PutSecnode
//
// Returns a node to the freelist.
//
//=============================================================================

void P_PutSecnode(msecnode_t *node)
{
	node->m_snext = headsecnode;
	headsecnode = node;
}

//=============================================================================
// phares 3/16/98
//
// P_AddSecnode
//
// Searches the current list to see if this sector is
// already there. If not, it adds a sector node at the head of the list of
// sectors this object appears in. This is called when creating a list of
// nodes that will get linked in later. Returns a pointer to the new node.
//
//=============================================================================

msecnode_t *P_AddSecnode(sector_t *s, AActor *thing, msecnode_t *nextnode)
{
	msecnode_t *node;

	if (s == 0)
	{
		I_FatalError("AddSecnode of 0 for %s\n", thing->_StaticType.TypeName.GetChars());
	}

	node = nextnode;
	while (node)
	{
		if (node->m_sector == s)	// Already have a node for this sector?
		{
			node->m_thing = thing;	// Yes. Setting m_thing says 'keep it'.
			return nextnode;
		}
		node = node->m_tnext;
	}

	// Couldn't find an existing node for this sector. Add one at the head
	// of the list.

	node = P_GetSecnode();

	// killough 4/4/98, 4/7/98: mark new nodes unvisited.
	node->visited = 0;

	node->m_sector = s; 			// sector
	node->m_thing = thing; 		// mobj
	node->m_tprev = NULL;			// prev node on Thing thread
	node->m_tnext = nextnode;		// next node on Thing thread
	if (nextnode)
		nextnode->m_tprev = node;	// set back link on Thing

	// Add new node at head of sector thread starting at s->touching_thinglist

	node->m_sprev = NULL;			// prev node on sector thread
	node->m_snext = s->touching_thinglist; // next node on sector thread
	if (s->touching_thinglist)
		node->m_snext->m_sprev = node;
	s->touching_thinglist = node;
	return node;
}

//=============================================================================
//
// P_DelSecnode
//
// Deletes a sector node from the list of
// sectors this object appears in. Returns a pointer to the next node
// on the linked list, or NULL.
//
//=============================================================================

msecnode_t *P_DelSecnode(msecnode_t *node)
{
	msecnode_t* tp;  // prev node on thing thread
	msecnode_t* tn;  // next node on thing thread
	msecnode_t* sp;  // prev node on sector thread
	msecnode_t* sn;  // next node on sector thread

	if (node)
	{
		// Unlink from the Thing thread. The Thing thread begins at
		// sector_list and not from AActor->touching_sectorlist.

		tp = node->m_tprev;
		tn = node->m_tnext;
		if (tp)
			tp->m_tnext = tn;
		if (tn)
			tn->m_tprev = tp;

		// Unlink from the sector thread. This thread begins at
		// sector_t->touching_thinglist.

		sp = node->m_sprev;
		sn = node->m_snext;
		if (sp)
			sp->m_snext = sn;
		else
			node->m_sector->touching_thinglist = sn;
		if (sn)
			sn->m_sprev = sp;

		// Return this node to the freelist

		P_PutSecnode(node);
		return tn;
	}
	return NULL;
} 														// phares 3/13/98

//=============================================================================
//
// P_DelSector_List
//
// Deletes the sector_list and NULLs it.
//
//=============================================================================

void P_DelSector_List()
{
	if (sector_list != NULL)
	{
		P_DelSeclist(sector_list);
		sector_list = NULL;
	}
}

//=============================================================================
//
// P_DelSeclist
//
// Delete an entire sector list
//
//=============================================================================

void P_DelSeclist(msecnode_t *node)
{
	while (node)
		node = P_DelSecnode(node);
}

//=============================================================================
// phares 3/14/98
//
// P_CreateSecNodeList
//
// Alters/creates the sector_list that shows what sectors the object resides in
//
//=============================================================================

void P_CreateSecNodeList(AActor *thing, fixed_t x, fixed_t y)
{
	msecnode_t *node;

	// First, clear out the existing m_thing fields. As each node is
	// added or verified as needed, m_thing will be set properly. When
	// finished, delete all nodes where m_thing is still NULL. These
	// represent the sectors the Thing has vacated.

	node = sector_list;
	while (node)
	{
		node->m_thing = NULL;
		node = node->m_tnext;
	}

	FBoundingBox box(thing->X(), thing->Y(), thing->radius);
	FBlockLinesIterator it(box);
	line_t *ld;

	while ((ld = it.Next()))
	{
		if (box.Right() <= ld->bbox[BOXLEFT] ||
			box.Left() >= ld->bbox[BOXRIGHT] ||
			box.Top() <= ld->bbox[BOXBOTTOM] ||
			box.Bottom() >= ld->bbox[BOXTOP])
			continue;

		if (box.BoxOnLineSide(ld) != -1)
			continue;

		// This line crosses through the object.

		// Collect the sector(s) from the line and add to the
		// sector_list you're examining. If the Thing ends up being
		// allowed to move to this position, then the sector_list
		// will be attached to the Thing's AActor at touching_sectorlist.

		sector_list = P_AddSecnode(ld->frontsector, thing, sector_list);

		// Don't assume all lines are 2-sided, since some Things
		// like MT_TFOG are allowed regardless of whether their radius takes
		// them beyond an impassable linedef.

		// killough 3/27/98, 4/4/98:
		// Use sidedefs instead of 2s flag to determine two-sidedness.

		if (ld->backsector)
			sector_list = P_AddSecnode(ld->backsector, thing, sector_list);
	}

	// Add the sector of the (x,y) point to sector_list.

	sector_list = P_AddSecnode(thing->Sector, thing, sector_list);

	// Now delete any nodes that won't be used. These are the ones where
	// m_thing is still NULL.

	node = sector_list;
	while (node)
	{
		if (node->m_thing == NULL)
		{
			if (node == sector_list)
				sector_list = node->m_tnext;
			node = P_DelSecnode(node);
		}
		else
		{
			node = node->m_tnext;
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void SpawnShootDecal(AActor *t1, const FTraceResults &trace)
{
	FDecalBase *decalbase = NULL;

	if (t1->player != NULL && t1->player->ReadyWeapon != NULL)
	{
		decalbase = t1->player->ReadyWeapon->GetDefault()->DecalGenerator;
	}
	else
	{
		decalbase = t1->DecalGenerator;
	}
	if (decalbase != NULL)
	{
		DImpactDecal::StaticCreate(decalbase->GetDecal(),
			trace.X, trace.Y, trace.Z, trace.Line->sidedef[trace.Side], trace.ffloor);
	}
}

//==========================================================================
//
//
//
//==========================================================================

static void SpawnDeepSplash(AActor *t1, const FTraceResults &trace, AActor *puff,
	fixed_t vx, fixed_t vy, fixed_t vz, fixed_t shootz, bool ffloor)
{
	const secplane_t *plane;
	if (ffloor && trace.Crossed3DWater)
		plane = trace.Crossed3DWater->top.plane;
	else if (trace.CrossedWater && trace.CrossedWater->heightsec)
		plane = &trace.CrossedWater->heightsec->floorplane;
	else return;

	fixed_t num, den, hitdist;
	den = TMulScale16(plane->a, vx, plane->b, vy, plane->c, vz);
	if (den != 0)
	{
		num = TMulScale16(plane->a, t1->X(), plane->b, t1->Y(), plane->c, shootz) + plane->d;
		hitdist = FixedDiv(-num, den);

		if (hitdist >= 0 && hitdist <= trace.Distance)
		{
			fixed_t hitx = t1->X() + FixedMul(vx, hitdist);
			fixed_t hity = t1->Y() + FixedMul(vy, hitdist);
			fixed_t hitz = shootz + FixedMul(vz, hitdist);

			P_HitWater(puff != NULL ? puff : t1, P_PointInSector(hitx, hity), hitx, hity, hitz);
		}
	}
}

//=============================================================================
//
// P_ActivateThingSpecial
//
// Handles the code for things activated by death, USESPECIAL or BUMPSPECIAL
//
//=============================================================================

bool P_ActivateThingSpecial(AActor * thing, AActor * trigger, bool death)
{
	bool res = false;

	// Target switching mechanism
	if (thing->activationtype & THINGSPEC_ThingTargets)		thing->target = trigger;
	if (thing->activationtype & THINGSPEC_TriggerTargets)	trigger->target = thing;

	// State change mechanism. The thing needs to be not dead and to have at least one of the relevant flags
	if (!death && (thing->activationtype & (THINGSPEC_Activate | THINGSPEC_Deactivate | THINGSPEC_Switch)))
	{
		// If a switchable thing does not know whether it should be activated
		// or deactivated, the default is to activate it.
		if ((thing->activationtype & THINGSPEC_Switch)
			&& !(thing->activationtype & (THINGSPEC_Activate | THINGSPEC_Deactivate)))
		{
			thing->activationtype |= THINGSPEC_Activate;
		}
		// Can it be activated?
		if (thing->activationtype & THINGSPEC_Activate)
		{
			thing->activationtype &= ~THINGSPEC_Activate; // Clear flag
			if (thing->activationtype & THINGSPEC_Switch) // Set other flag if switching
				thing->activationtype |= THINGSPEC_Deactivate;
			thing->Activate(trigger);
			res = true;
		}
		// If not, can it be deactivated?
		else if (thing->activationtype & THINGSPEC_Deactivate)
		{
			thing->activationtype &= ~THINGSPEC_Deactivate; // Clear flag
			if (thing->activationtype & THINGSPEC_Switch)	// Set other flag if switching
				thing->activationtype |= THINGSPEC_Activate;
			thing->Deactivate(trigger);
			res = true;
		}
	}

	// Run the special, if any
	if (thing->special)
	{
		res = !!P_ExecuteSpecial(thing->special, NULL,
			// TriggerActs overrides the level flag, which only concerns thing activated by death
			(((death && level.flags & LEVEL_ACTOWNSPECIAL && !(thing->activationtype & THINGSPEC_TriggerActs))
			|| (thing->activationtype & THINGSPEC_ThingActs)) // Who triggers?
			? thing : trigger),
			false, thing->args[0], thing->args[1], thing->args[2], thing->args[3], thing->args[4]);

		// Clears the special if it was run on thing's death or if flag is set.
		if (death || (thing->activationtype & THINGSPEC_ClearSpecial && res)) thing->special = 0;
	}

	// Returns the result
	return res;
}

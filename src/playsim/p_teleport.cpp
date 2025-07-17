//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
// Copyright 1998-1998 Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//		Teleportation.
//
//-----------------------------------------------------------------------------

#include "d_player.h"
#include "doomdef.h"
#include "vm.h"
#include "g_levellocals.h"
#include "p_maputl.h"
#include "gi.h"
#include "r_utility.h"

#define FUDGEFACTOR		10

static FRandom pr_teleport ("Teleport");
static FRandom pr_playerteleport("PlayerTeleport");

CVAR (Bool, telezoom, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);

//==========================================================================
//
// P_SpawnTeleportFog
//
// The beginning of customizable teleport fog
// (not active yet)
//
//==========================================================================

void P_SpawnTeleportFog(AActor *mobj, const DVector3 &pos, bool beforeTele, bool setTarget)
{
	AActor *mo;
	if ((beforeTele ? mobj->TeleFogSourceType : mobj->TeleFogDestType) == NULL)
	{
		//Do nothing.
		mo = NULL;
	}
	else
	{
		double fogDelta = mobj->flags & MF_MISSILE ? 0 : TELEFOGHEIGHT;
		mo = Spawn(mobj->Level, (beforeTele ? mobj->TeleFogSourceType : mobj->TeleFogDestType), pos.plusZ(fogDelta), ALLOW_REPLACE);
	}

	if (mo != NULL && setTarget)
		mo->target = mobj;
}

DEFINE_ACTION_FUNCTION(AActor, SpawnTeleportFog)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	PARAM_BOOL(before);
	PARAM_BOOL(settarget);
	P_SpawnTeleportFog(self, DVector3(x, y, z), before, settarget);
	return 0;
}

//-----------------------------------------------------------------------------
//
// TELEPORTATION
//
//-----------------------------------------------------------------------------

bool P_Teleport (AActor *thing, DVector3 pos, DAngle angle, int flags)
{
	bool predicting = (thing->player && (thing->player->cheats & CF_PREDICTING));

	DVector3 old;
	double aboveFloor;
	player_t *player;
	sector_t *destsect;
	bool resetpitch = false;
	double floorheight, ceilingheight;
	double missilespeed = 0;

	old = thing->Pos();
	aboveFloor = thing->Z() - thing->floorz;
	destsect = thing->Level->PointInSector (pos);
	// killough 5/12/98: exclude voodoo dolls:
	player = thing->player;
	if (player && player->mo != thing)
		player = NULL;
	floorheight = destsect->floorplane.ZatPoint (pos);
	ceilingheight = destsect->ceilingplane.ZatPoint (pos);
	if (thing->flags & MF_MISSILE)
	{ // We don't measure z velocity, because it doesn't change.
		missilespeed = thing->VelXYToSpeed();
	}
	if (flags & TELF_KEEPHEIGHT)
	{
		pos.Z = floorheight + aboveFloor;
	}
	else if (pos.Z == ONFLOORZ)
	{
		if (player)
		{
			if (thing->flags & MF_NOGRAVITY && aboveFloor)
			{
				pos.Z = floorheight + aboveFloor;
				if (pos.Z + thing->Height > ceilingheight)
				{
					pos.Z = ceilingheight - thing->Height;
				}
			}
			else
			{
				if (!(thing->Level->i_compatflags2 & COMPATF2_FDTELEPORT) || !(flags & TELF_FDCOMPAT) || floorheight > thing->Z())
				{
					pos.Z = floorheight;
				}
				else
				{
					pos.Z = thing->Z();
				}

				if (!(flags & TELF_KEEPORIENTATION))
				{
					resetpitch = false;
				}
			}
		}
		else if (thing->flags & MF_MISSILE)
		{
			pos.Z = floorheight + aboveFloor;
			if (pos.Z + thing->Height > ceilingheight)
			{
				pos.Z = ceilingheight - thing->Height;
			}
		}
		else
		{
			// emulation of Final Doom's teleport glitch.
			// For walking monsters we still have to force them to the ground because the handling of off-ground monsters is different from vanilla.
			if (!(thing->Level->i_compatflags2 & COMPATF2_FDTELEPORT) || !(flags & TELF_FDCOMPAT) || !(thing->flags & MF_NOGRAVITY) || floorheight > pos.Z)
			{
				pos.Z = floorheight;
			}
			else
			{
				pos.Z = thing->Z();
			}
		}
	}
	// [MK] notify thing of incoming teleport, check for an early cancel
	// if it returns false
	{
		int nocancel = 1;
		IFVIRTUALPTR(thing, AActor, PreTeleport)
		{
			VMValue params[] = { thing, pos.X, pos.Y, pos.Z, angle.Degrees(), flags };
			VMReturn ret;
			ret.IntAt(&nocancel);
			VMCall(func, params, countof(params), &ret, 1);
		}
		if ( !nocancel ) return false;
	}
	if (!P_TeleportMove (thing, pos, false))
	{
		return false;
	}
	if (player)
	{
		player->viewz = thing->Z() + player->viewheight;
		if (resetpitch)
		{
			player->mo->Angles.Pitch = nullAngle;
		}
	}
	if (!(flags & TELF_KEEPORIENTATION))
	{
		thing->Angles.Yaw = angle;
	}
	else
	{
		angle = thing->Angles.Yaw;
	}
	// Spawn teleport fog at source and destination
	if ((flags & TELF_SOURCEFOG) && !predicting)
	{
		P_SpawnTeleportFog(thing, old, true, true); //Passes the actor through which then pulls the TeleFog metadata types based on properties.
	}
	if (flags & TELF_DESTFOG)
	{
		if (!predicting)
		{
			DVector2 vector = angle.ToVector(20);
			DVector2 fogpos = thing->Level->GetPortalOffsetPosition(pos.X, pos.Y, vector.X, vector.Y);
			P_SpawnTeleportFog(thing, DVector3(fogpos, thing->Z()), false, true);

		}
		if (thing->player)
		{
			// [RH] Zoom player's field of vision
			// [BC] && bHaltVelocity.
			if (telezoom && thing->player->mo == thing && !(flags & TELF_KEEPVELOCITY))
				thing->player->FOV = min (175.f, thing->player->DesiredFOV + 45.f);
		}
	}
	// [BC] && bHaltVelocity.
	if (thing->player && ((flags & TELF_DESTFOG) || !(flags & TELF_KEEPORIENTATION)) && !(flags & TELF_KEEPVELOCITY))
	{
		int time = 18;
		IFVIRTUALPTRNAME(thing, NAME_PlayerPawn, GetTeleportFreezeTime)
		{
			VMValue param = thing;
			VMReturn ret(&time);
			VMCall(func, &param, 1, &ret, 1);
		}
		thing->reactiontime = time;
	}
	if (thing->flags & MF_MISSILE)
	{
		thing->VelFromAngle(missilespeed);
	}
	// [BC] && bHaltVelocity.
	else if (!(flags & TELF_KEEPORIENTATION) && !(flags & TELF_KEEPVELOCITY))
	{ // no fog doesn't alter the player's momentum
		thing->Vel.Zero();
		// killough 10/98: kill all bobbing velocity too
		if (player)	player->Vel.Zero();
	}
	// [MK] notify thing of successful teleport
	{
		IFVIRTUALPTR(thing, AActor, PostTeleport)
		{
			VMValue params[] = { thing, pos.X, pos.Y, pos.Z, angle.Degrees(), flags };
			VMCall(func, params, countof(params), nullptr, 1);
		}
	}
	return true;
}

DEFINE_ACTION_FUNCTION(AActor, Teleport)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	PARAM_ANGLE(an);
	PARAM_INT(flags);
	ACTION_RETURN_BOOL(P_Teleport(self, DVector3(x, y, z), an, flags & ~TELF_FDCOMPAT));
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

AActor *FLevelLocals::SelectTeleDest (int tid, int tag, bool norandom, bool isPlayer)
{
	AActor *searcher;

	// If tid is non-zero, select a destination from a matching actor at random.
	// If tag is also non-zero, the selection is restricted to actors in sectors
	// with a matching tag. If tid is zero and tag is non-zero, then the old Doom
	// behavior is used instead (return the first teleport dest found in a tagged
	// sector).

	// Compatibility hack for some maps that fell victim to a bug in the teleport code in 2.0.9x
	if (ib_compatflags & BCOMPATF_BADTELEPORTERS) tag = 0;

	if (tid != 0)
	{
		auto iterator = GetActorIterator(NAME_TeleportDest, tid);
		int count = 0;
		while ( (searcher = iterator.Next ()) )
		{
			if (tag == 0 || SectorHasTag(searcher->Sector, tag))
			{
				count++;
			}
		}

		// If teleport dests were not found, the sector tag is ignored for the
		// following compatibility searches.
		// Do this only when tag is 0 because this is the only case that was defined in Hexen.
		if (count == 0)
		{
			if (tag == 0)
			{
				// Try to find a matching map spot (fixes Hexen MAP10)
				auto it2 = GetActorIterator(NAME_MapSpot, tid);
				searcher = it2.Next ();
				if (searcher == nullptr)
				{
					// Try to find a matching non-blocking spot of any type (fixes Caldera MAP13)
					auto it3 = GetActorIterator(tid);
					searcher = it3.Next ();
					while (searcher != NULL && (searcher->flags & MF_SOLID))
					{
						searcher = it3.Next ();
					}
					return searcher;
				}
			}
		}
		else
		{
			if (count != 1 && !norandom)
			{
				// Players get their own RNG seed to reduce likelihood of breaking prediction.
				count = 1 + ((isPlayer ? pr_playerteleport() : pr_teleport()) % count);
			}
			searcher = NULL;
			while (count > 0)
			{
				searcher = iterator.Next ();
				if (tag == 0 || SectorHasTag(searcher->Sector, tag))
				{
					count--;
				}
			}
		}
		return searcher;
	}

	if (tag != 0)
	{
		int secnum;

		auto itr = GetSectorTagIterator(tag);
		while ((secnum = itr.Next()) >= 0)
		{
			// Scanning the snext links of things in the sector will not work, because
			// TeleportDests have MF_NOSECTOR set. So you have to search *everything*.
			// If there is more than one sector with a matching tag, then the destination
			// in the lowest-numbered sector is returned, rather than the earliest placed
			// teleport destination. This means if 50 sectors have a matching tag and
			// only the last one has a destination, *every* actor is scanned at least 49
			// times. Yuck.
			auto it2 = GetThinkerIterator<AActor>(NAME_TeleportDest);
			while ((searcher = it2.Next()) != NULL)
			{
				if (searcher->Sector == &sectors[secnum])
				{
					return searcher;
				}
			}
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

bool FLevelLocals::EV_Teleport (int tid, int tag, line_t *line, int side, AActor *thing, int flags)
{
	AActor *searcher;
	double z;
	DAngle angle = nullAngle;
	double s = 0, c = 0;
	double vx = 0, vy = 0;
	DAngle badangle = nullAngle;

	if (thing == NULL)
	{ // Teleport function called with an invalid actor
		return false;
	}
	bool predicting = (thing->player && (thing->player->cheats & CF_PREDICTING));
	if (thing->flags2 & MF2_NOTELEPORT)
	{
		return false;
	}
	if (side != 0)
	{ // Don't teleport if hit back of line, so you can get out of teleporter.
		return 0;
	}
	searcher = SelectTeleDest(tid, tag, false, thing->player != nullptr && thing->player->mo == thing);
	if (searcher == NULL)
	{
		return false;
	}
	// [RH] Lee Killough's changes for silent teleporters from BOOM
	if ((flags & (TELF_ROTATEBOOM|TELF_ROTATEBOOMINVERSE)) && line)
	{
		// Get the angle between the exit thing and source linedef.
		// Rotate 90 degrees, so that walking perpendicularly across
		// teleporter linedef causes thing to exit in the direction
		// indicated by the exit thing.
		angle = line->Delta().Angle() - searcher->Angles.Yaw + DAngle::fromDeg(90.);
		if (flags & TELF_ROTATEBOOMINVERSE) angle = -angle;

		// Sine, cosine of angle adjustment
		s = angle.Sin();
		c = angle.Cos();

		// Velocity of thing crossing teleporter linedef
		vx = thing->Vel.X;
		vy = thing->Vel.Y;

		z = searcher->Z();
	}
	else if (searcher->IsKindOf (NAME_TeleportDest2))
	{
		z = searcher->Z();
	}
	else
	{
		z = ONFLOORZ;
	}
	if ((i_compatflags2 & COMPATF2_BADANGLES) && (thing->player != NULL))
	{
		badangle = DAngle::fromDeg(0.01);
	}
	if (P_Teleport (thing, DVector3(searcher->Pos().XY(), z), searcher->Angles.Yaw + badangle, flags))
	{
		// [RH] Lee Killough's changes for silent teleporters from BOOM
		if (line)
		{
			if (flags & (TELF_ROTATEBOOM| TELF_ROTATEBOOMINVERSE))
			{
				// Rotate thing according to difference in angles (or not - Boom got the direction wrong here.)
				thing->Angles.Yaw += angle;

				// Rotate thing's velocity to come out of exit just like it entered
				thing->Vel.X = vx*c - vy*s;
				thing->Vel.Y = vy*c + vx*s;
			}
		}
		if (vx == 0 && vy == 0 && thing->player != NULL && thing->player->mo == thing && !predicting)
		{
			PlayIdle (thing->player->mo);
		}
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
//
// Silent linedef-based TELEPORTATION, by Lee Killough
// Primarily for rooms-over-rooms etc.
// This is the complete player-preserving kind of teleporter.
// It has advantages over the teleporter with thing exits.
//
// [RH] Modified to support different source and destination ids.
// [RH] Modified some more to be accurate.
//
//-----------------------------------------------------------------------------

bool FLevelLocals::EV_SilentLineTeleport (line_t *line, int side, AActor *thing, int id, INTBOOL reverse)
{
	int i;
	line_t *l;

	if (side || thing->flags2 & MF2_NOTELEPORT || !line || line->sidedef[1] == NULL)
		return false;

	auto itr = GetLineIdIterator(id);
	while ((i = itr.Next()) >= 0)
	{
		if (line->Index() == i)
			continue;

		if ((l=&lines[i]) != line && l->backsector)
		{
			// Get the thing's position along the source linedef
			double pos;
			DVector2 npos;			// offsets from line
			double den;

			den = line->Delta().LengthSquared();
			if (den == 0)
			{
				pos = 0;
				npos.Zero();
			}
			else
			{
				double num = (thing->Pos().XY() - line->v1->fPos()) | line->Delta();
				if (num <= 0)
				{
					pos = 0;
				}
				else if (num >= den)
				{
					pos = 1;
				}
				else
				{
					pos = num / den;
				}
				npos = thing->Pos().XY() - line->v1->fPos() - line->Delta() * pos;
			}

			// Get the angle between the two linedefs, for rotating
			// orientation and velocity. Rotate 180 degrees, and flip
			// the position across the exit linedef, if reversed.
			DAngle angle = l->Delta().Angle() - line->Delta().Angle();

			if (!reverse)
			{
				angle += DAngle::fromDeg(180.);
				pos = 1 - pos;
			}

			// Sine, cosine of angle adjustment
			double s = angle.Sin();
			double c = angle.Cos();

			DVector2 p;

			// Rotate position along normal to match exit linedef
			p.X = npos.X*c - npos.Y*s;
			p.Y = npos.Y*c + npos.X*s;

			// Interpolate position across the exit linedef
			p += l->v1->fPos() + pos*l->Delta();

			// Whether this is a player, and if so, a pointer to its player_t.
			// Voodoo dolls are excluded by making sure thing->player->mo==thing.
			player_t *player = thing->player && thing->player->mo == thing ?
				thing->player : NULL;

			// Whether walking towards first side of exit linedef steps down
			bool stepdown = l->frontsector->floorplane.ZatPoint(p) < l->backsector->floorplane.ZatPoint(p);

			// Height of thing above ground
			double z = thing->Z() - thing->floorz;

			// Side to exit the linedef on positionally.
			//
			// Notes:
			//
			// This flag concerns exit position, not momentum. Due to
			// roundoff error, the thing can land on either the left or
			// the right side of the exit linedef, and steps must be
			// taken to make sure it does not end up on the wrong side.
			//
			// Exit momentum is always towards side 1 in a reversed
			// teleporter, and always towards side 0 otherwise.
			//
			// Exiting positionally on side 1 is always safe, as far
			// as avoiding oscillations and stuck-in-wall problems,
			// but may not be optimum for non-reversed teleporters.
			//
			// Exiting on side 0 can cause oscillations if momentum
			// is towards side 1, as it is with reversed teleporters.
			//
			// Exiting on side 1 slightly improves player viewing
			// when going down a step on a non-reversed teleporter.

			// Is this really still necessary with real math instead of imprecise trig tables?
#if 1
			const double fudgeamount = 1. / 65536.;

			int side = reverse || (player && stepdown);
			int fudge = FUDGEFACTOR;

			double dx = line->Delta().X;
			double dy = line->Delta().Y;

			// Make sure we are on correct side of exit linedef.
			while (P_PointOnLineSidePrecise(p, l) != side && --fudge >= 0)
			{
				if (fabs(dx) > fabs(dy))
					p.Y -= (dx < 0) != side ? -fudgeamount : fudgeamount;
				else
					p.X += (dy < 0) != side ? -fudgeamount : fudgeamount;
			}
#endif

			// Adjust z position to be same height above ground as before.
			// Ground level at the exit is measured as the higher of the
			// two floor heights at the exit linedef.
			z = z + l->sidedef[stepdown]->sector->floorplane.ZatPoint(p);

			// Attempt to teleport, aborting if blocked
			if (!P_TeleportMove (thing, DVector3(p, z), false))
			{
				return false;
			}

			thing->renderflags |= RF_NOINTERPOLATEVIEW;

			// Rotate thing's orientation according to difference in linedef angles
			thing->Angles.Yaw += angle;

			// Rotate thing's velocity to come out of exit just like it entered
			p = thing->Vel.XY();
			thing->Vel.X = p.X*c - p.Y*s;
			thing->Vel.Y = p.Y*c + p.X*s;

			// Adjust a player's view, in case there has been a height change
			if (player && player->mo == thing)
			{
				// Adjust player's local copy of velocity
				p = player->Vel;
				player->Vel.X = p.X*c - p.Y*s;
				player->Vel.Y = p.Y*c + p.X*s;

				// Save the current deltaviewheight, used in stepping
				double deltaviewheight = player->deltaviewheight;

				// Clear deltaviewheight, since we don't want any changes now
				player->deltaviewheight = 0;

				// Set player's view according to the newly set parameters
				IFVIRTUALPTRNAME(player->mo, NAME_PlayerPawn, CalcHeight)
				{
					VMValue param = player->mo;
					VMCall(func, &param, 1, nullptr, 0);
				}

				// Reset the delta to have the same dynamics as before
				player->deltaviewheight = deltaviewheight;
			}

			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
//
// [RH] Teleport anything matching other_tid to dest_tid
//
//-----------------------------------------------------------------------------

bool FLevelLocals::EV_TeleportOther (int other_tid, int dest_tid, bool fog)
{
	bool didSomething = false;

	if (other_tid != 0 && dest_tid != 0)
	{
		AActor *victim;
		auto iterator = GetActorIterator(other_tid);

		while ( (victim = iterator.Next ()) )
		{
			didSomething |= EV_Teleport (dest_tid, 0, NULL, 0, victim,
				fog ? (TELF_DESTFOG | TELF_SOURCEFOG) : TELF_KEEPORIENTATION);
		}
	}

	return didSomething;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

bool DoGroupForOne (AActor *victim, AActor *source, AActor *dest, bool floorz, bool fog)
{
	DAngle an = dest->Angles.Yaw - source->Angles.Yaw;
	DVector2 off = victim->Pos().XY() - source->Pos().XY();
	DAngle offAngle = victim->Angles.Yaw - source->Angles.Yaw;
	DVector2 newp = { off.X * an.Cos() - off.Y * an.Sin(), off.X * an.Sin() + off.Y * an.Cos() };
	double z = floorz ? ONFLOORZ : dest->Z() + victim->Z() - source->Z();
	int flags = fog ? (TELF_DESTFOG | TELF_SOURCEFOG | TELF_KEEPORIENTATION) : TELF_KEEPORIENTATION;

	bool res =
		P_Teleport (victim, DVector3(dest->Pos().XY() + newp, z),
							nullAngle, flags);
	// P_Teleport only changes angle if fog is true
	victim->Angles.Yaw = (dest->Angles.Yaw + victim->Angles.Yaw - source->Angles.Yaw).Normalized360();

	return res;
}

//-----------------------------------------------------------------------------
//
// [RH] Teleport a group of actors centered around source_tid so
// that they become centered around dest_tid instead.
//
//-----------------------------------------------------------------------------

bool FLevelLocals::EV_TeleportGroup (int group_tid, AActor *victim, int source_tid, int dest_tid, bool moveSource, bool fog)
{
	AActor *sourceOrigin, *destOrigin;
	{
		auto iterator = GetActorIterator(source_tid);
		sourceOrigin = iterator.Next ();
	}
	if (sourceOrigin == NULL)
	{ // If there is no source origin, behave like TeleportOther
		return EV_TeleportOther (group_tid, dest_tid, fog);
	}

	{
		auto iterator = GetActorIterator(NAME_TeleportDest, dest_tid);
		destOrigin = iterator.Next ();
	}
	if (destOrigin == NULL)
	{
		return false;
	}

	bool didSomething = false;
	bool floorz = !destOrigin->IsKindOf (PClass::FindClass("TeleportDest2"));

	// Use the passed victim if group_tid is 0
	if (group_tid == 0 && victim != NULL)
	{
		didSomething = DoGroupForOne (victim, sourceOrigin, destOrigin, floorz, fog);
	}
	else
	{
		auto iterator = GetActorIterator(group_tid);

		// For each actor with tid matching arg0, move it to the same
		// position relative to destOrigin as it is relative to sourceOrigin
		// before the teleport.
		while ( (victim = iterator.Next ()) )
		{
			didSomething |= DoGroupForOne (victim, sourceOrigin, destOrigin, floorz, fog);
		}
	}

	if (moveSource && didSomething)
	{
		didSomething |=
			P_Teleport (sourceOrigin, destOrigin->PosAtZ(floorz ? ONFLOORZ : destOrigin->Z()), nullAngle, TELF_KEEPORIENTATION);
		sourceOrigin->Angles.Yaw = destOrigin->Angles.Yaw;
	}

	return didSomething;
}

//-----------------------------------------------------------------------------
//
// [RH] Teleport a group of actors in a sector. Source_tid is used as a
// reference point so that they end up in the same position relative to
// dest_tid. Group_tid can be used to not teleport all actors in the sector.
//
//-----------------------------------------------------------------------------

bool FLevelLocals::EV_TeleportSector (int tag, int source_tid, int dest_tid, bool fog, int group_tid)
{
	AActor *sourceOrigin, *destOrigin;
	{
		auto iterator = GetActorIterator(source_tid);
		sourceOrigin = iterator.Next ();
	}
	if (sourceOrigin == NULL)
	{
		return false;
	}
	{
		auto iterator = GetActorIterator(NAME_TeleportDest, dest_tid);
		destOrigin = iterator.Next ();
	}
	if (destOrigin == NULL)
	{
		return false;
	}

	bool didSomething = false;
	bool floorz = !destOrigin->IsKindOf(NAME_TeleportDest2);
	int secnum;

	secnum = -1;
	auto itr = GetSectorTagIterator(tag);
	while ((secnum = itr.Next()) >= 0)
	{
		msecnode_t *node;
		const sector_t * const sec = &sectors[secnum];

		for (node = sec->touching_thinglist; node; )
		{
			AActor *actor = node->m_thing;
			msecnode_t *next = node->m_snext;

			// possibly limit actors by group
			if (actor != NULL && (group_tid == 0 || actor->tid == group_tid))
			{
				didSomething |= DoGroupForOne (actor, sourceOrigin, destOrigin, floorz, fog);
			}
			node = next;
		}
	}
	return didSomething;
}

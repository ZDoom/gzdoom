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
//		Teleportation.
//
//-----------------------------------------------------------------------------



#include "doomtype.h"
#include "doomdef.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_terrain.h"
#include "r_state.h"
#include "gi.h"
#include "a_sharedglobal.h"
#include "m_random.h"
#include "i_system.h"

extern void P_CalcHeight (player_t *player);

CVAR (Bool, telezoom, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);

FState ATeleportFog::States[] =
{
#define S_DTFOG 0
	S_BRIGHT (TFOG, 'A',	6, NULL 						, &States[S_DTFOG+1]),
	S_BRIGHT (TFOG, 'B',	6, NULL 						, &States[S_DTFOG+2]),
	S_BRIGHT (TFOG, 'A',	6, NULL 						, &States[S_DTFOG+3]),
	S_BRIGHT (TFOG, 'B',	6, NULL 						, &States[S_DTFOG+4]),
	S_BRIGHT (TFOG, 'C',	6, NULL 						, &States[S_DTFOG+5]),
	S_BRIGHT (TFOG, 'D',	6, NULL 						, &States[S_DTFOG+6]),
	S_BRIGHT (TFOG, 'E',	6, NULL 						, &States[S_DTFOG+7]),
	S_BRIGHT (TFOG, 'F',	6, NULL 						, &States[S_DTFOG+8]),
	S_BRIGHT (TFOG, 'G',	6, NULL 						, &States[S_DTFOG+9]),
	S_BRIGHT (TFOG, 'H',	6, NULL 						, &States[S_DTFOG+10]),
	S_BRIGHT (TFOG, 'I',	6, NULL 						, &States[S_DTFOG+11]),
	S_BRIGHT (TFOG, 'J',	6, NULL 						, NULL),

#define S_HTFOG (S_DTFOG+12)
	S_BRIGHT (TELE, 'A',    6, NULL                         , &States[S_HTFOG+1]),
	S_BRIGHT (TELE, 'B',    6, NULL                         , &States[S_HTFOG+2]),
	S_BRIGHT (TELE, 'C',    6, NULL                         , &States[S_HTFOG+3]),
	S_BRIGHT (TELE, 'D',    6, NULL                         , &States[S_HTFOG+4]),
	S_BRIGHT (TELE, 'E',    6, NULL                         , &States[S_HTFOG+5]),
	S_BRIGHT (TELE, 'F',    6, NULL                         , &States[S_HTFOG+6]),
	S_BRIGHT (TELE, 'G',    6, NULL                         , &States[S_HTFOG+7]),
	S_BRIGHT (TELE, 'H',    6, NULL                         , &States[S_HTFOG+8]),
	S_BRIGHT (TELE, 'G',    6, NULL                         , &States[S_HTFOG+9]),
	S_BRIGHT (TELE, 'F',    6, NULL                         , &States[S_HTFOG+10]),
	S_BRIGHT (TELE, 'E',    6, NULL                         , &States[S_HTFOG+11]),
	S_BRIGHT (TELE, 'D',    6, NULL                         , &States[S_HTFOG+12]),
	S_BRIGHT (TELE, 'C',    6, NULL                         , NULL),
};

IMPLEMENT_ACTOR (ATeleportFog, Any, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_Add)
END_DEFAULTS

AT_GAME_SET (TeleportFog)
{
	ATeleportFog *def = GetDefault<ATeleportFog>();

	def->SpawnState = &ATeleportFog::States[
		gameinfo.gametype == GAME_Doom ? S_DTFOG : S_HTFOG];
}

void ATeleportFog::PostBeginPlay ()
{
	Super::PostBeginPlay ();
	S_Sound (this, CHAN_BODY, "misc/teleport", 1, ATTN_NORM);
}

IMPLEMENT_STATELESS_ACTOR (ATeleportDest, Any, 14, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOSECTOR)
END_DEFAULTS

class ATeleportDest2 : public ATeleportDest
{
	DECLARE_STATELESS_ACTOR (ATeleportDest2, ATeleportDest)
};

IMPLEMENT_STATELESS_ACTOR (ATeleportDest2, Any, 9044, 0)
	PROP_FlagsSet (MF_NOGRAVITY)
END_DEFAULTS

//
// TELEPORTATION
//

bool P_Teleport (AActor *thing, fixed_t x, fixed_t y, fixed_t z, angle_t angle,
				 bool useFog)
{
	fixed_t oldx;
	fixed_t oldy;
	fixed_t oldz;
	fixed_t aboveFloor;
	player_t *player;
	angle_t an;
	sector_t *destsect;
	bool resetpitch = false;
	fixed_t floorheight, ceilingheight;

	oldx = thing->x;
	oldy = thing->y;
	oldz = thing->z;
	aboveFloor = thing->z - thing->floorz;
	destsect = R_PointInSubsector (x, y)->sector;
	// killough 5/12/98: exclude voodoo dolls:
	player = thing->player;
	if (player && player->mo != thing)
		player = NULL;
	floorheight = destsect->floorplane.ZatPoint (x, y);
	ceilingheight = destsect->ceilingplane.ZatPoint (x, y);
	if (z == ONFLOORZ)
	{
		if (player)
		{
			if (player->powers[pw_flight] && aboveFloor)
			{
				z = floorheight + aboveFloor;
				if (z + thing->height > ceilingheight)
				{
					z = ceilingheight - thing->height;
				}
			}
			else
			{
				z = floorheight;
				if (useFog)
				{
					resetpitch = 0;
				}
			}
		}
		else if (thing->flags & MF_MISSILE)
		{
			z = floorheight + aboveFloor;
			if (z + thing->height > ceilingheight)
			{
				z = ceilingheight - thing->height;
			}
		}
		else
		{
			z = floorheight;
		}
	}
	if (!P_TeleportMove (thing, x, y, z, false))
	{
		return false;
	}
	if (player)
	{
		player->viewz = thing->z + player->viewheight;
		if (resetpitch)
		{
			player->mo->pitch = 0;
		}
	}
	// Spawn teleport fog at source and destination
	if (useFog)
	{
		fixed_t fogDelta = thing->flags & MF_MISSILE ? 0 : TELEFOGHEIGHT;
		Spawn<ATeleportFog> (oldx, oldy, oldz + fogDelta);
		an = angle >> ANGLETOFINESHIFT;
		Spawn<ATeleportFog> (x + 20*finecosine[an],
			y + 20*finesine[an], thing->z + fogDelta);
		thing->angle = angle;
		if (thing->player)
		{
			// [RH] Zoom player's field of vision
			if (*telezoom)
				thing->player->FOV = MIN (175.f, thing->player->DesiredFOV + 45.f);
			// Freeze player for about .5 sec
			if (!thing->player->powers[pw_speed])
				thing->reactiontime = 18;
		}
	}
	if (thing->flags2 & MF2_FLOORCLIP)
	{
		thing->AdjustFloorClip ();
	}
	if (thing->flags & MF_MISSILE)
	{
		angle >>= ANGLETOFINESHIFT;
		thing->momx = FixedMul (thing->Speed, finecosine[angle]);
		thing->momy = FixedMul (thing->Speed, finesine[angle]);
	}
	else if (useFog) // no fog doesn't alter the player's momentum
	{
		thing->momx = thing->momy = thing->momz = 0;
		// killough 10/98: kill all bobbing momentum too
		if (player)
			player->momx = player->momy = 0;
	}
	return true;
}

bool EV_Teleport (int tid, line_t *line, int side, AActor *thing, bool fog)
{
	int count;
	AActor *searcher;
	fixed_t z;
	angle_t angle;
	fixed_t s, c;
	fixed_t momx, momy;
	TActorIterator<ATeleportDest> iterator (tid);

	if (!thing)
	{ // Teleport function called with an invalid actor
		return false;
	}
	if (thing->flags2 & MF2_NOTELEPORT)
	{
		return false;
	}
	if (side != 0)
	{ // Don't teleport if hit back of line, so you can get out of teleporter.
		return 0;
	}
	count = 0;
	while ( (searcher = iterator.Next ()) )
	{
		count++;
	}
	if (count == 0)
	{
		return false;
	}
	count = 1 + (P_Random() % count);
	searcher = NULL;
	while (count > 0)
	{
		searcher = iterator.Next ();
		count--;
	}
	if (!searcher)
		I_Error ("Can't find teleport mapspot %d\n", tid);
	// [RH] Lee Killough's changes for silent teleporters from BOOM
	if (!fog && line)
	{
		// Get the angle between the exit thing and source linedef.
		// Rotate 90 degrees, so that walking perpendicularly across
		// teleporter linedef causes thing to exit in the direction
		// indicated by the exit thing.
		angle =
			R_PointToAngle2 (0, 0, line->dx, line->dy) - thing->angle + ANG90;

		// Sine, cosine of angle adjustment
		s = finesine[angle>>ANGLETOFINESHIFT];
		c = finecosine[angle>>ANGLETOFINESHIFT];

		// Momentum of thing crossing teleporter linedef
		momx = thing->momx;
		momy = thing->momy;

		z = searcher->z;
	}
	else if (searcher->IsKindOf (RUNTIME_CLASS(ATeleportDest2)))
	{
		z = searcher->z;
	}
	else
	{
		z = ONFLOORZ;
	}
	if (P_Teleport (thing, searcher->x, searcher->y, z, searcher->angle, fog))
	{
		return true;
	}
	// [RH] Lee Killough's changes for silent teleporters from BOOM
	if (!fog && line)
	{
		// Rotate thing according to difference in angles
		thing->angle += angle;

		// Rotate thing's momentum to come out of exit just like it entered
		thing->momx = FixedMul(momx, c) - FixedMul(momy, s);
		thing->momy = FixedMul(momy, c) + FixedMul(momx, s);
	}
	return false;
}

//
// Silent linedef-based TELEPORTATION, by Lee Killough
// Primarily for rooms-over-rooms etc.
// This is the complete player-preserving kind of teleporter.
// It has advantages over the teleporter with thing exits.
//

// maximum fixed_t units to move object to avoid hiccups
#define FUDGEFACTOR 10

// [RH] Modified to support different source and destination ids.
bool EV_SilentLineTeleport (line_t *line, int side, AActor *thing, int id,
							BOOL reverse)
{
	int i;
	line_t *l;

	if (side || thing->flags2 & MF2_NOTELEPORT || !line)
		return false;

	for (i = -1; (i = P_FindLineFromID (id, i)) >= 0;)
		if ((l=lines+i) != line && l->backsector)
		{
			// Get the thing's position along the source linedef
			fixed_t pos = abs(line->dx) > abs(line->dy) ?
				FixedDiv(thing->x - line->v1->x, line->dx) :
				FixedDiv(thing->y - line->v1->y, line->dy) ;

			// Get the angle between the two linedefs, for rotating
			// orientation and momentum. Rotate 180 degrees, and flip
			// the position across the exit linedef, if reversed.
			angle_t angle = (reverse ? pos = FRACUNIT-pos, 0 : ANG180) +
				R_PointToAngle2(0, 0, l->dx, l->dy) -
				R_PointToAngle2(0, 0, line->dx, line->dy);

			// Interpolate position across the exit linedef
			fixed_t x = l->v2->x - FixedMul(pos, l->dx);
			fixed_t y = l->v2->y - FixedMul(pos, l->dy);

			// Sine, cosine of angle adjustment
			fixed_t s = finesine[angle>>ANGLETOFINESHIFT];
			fixed_t c = finecosine[angle>>ANGLETOFINESHIFT];

			// Maximum distance thing can be moved away from interpolated
			// exit, to ensure that it is on the correct side of exit linedef
			int fudge = FUDGEFACTOR;

			// Whether this is a player, and if so, a pointer to its player_t.
			// Voodoo dolls are excluded by making sure thing->player->mo==thing.
			player_t *player = thing->player && thing->player->mo == thing ?
				thing->player : NULL;

			// Whether walking towards first side of exit linedef steps down
			int stepdown =
				l->frontsector->floorplane.ZatPoint (x, y) <
				l->backsector->floorplane.ZatPoint (x, y);

			// Height of thing above ground
			fixed_t z = thing->z - thing->floorz;

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

			int side = reverse || (player && stepdown);

			// Make sure we are on correct side of exit linedef.
			while (P_PointOnLineSide(x, y, l) != side && --fudge>=0)
				if (abs(l->dx) > abs(l->dy))
					y -= l->dx < 0 != side ? -1 : 1;
				else
					x += l->dy < 0 != side ? -1 : 1;

			// Attempt to teleport, aborting if blocked
			// Adjust z position to be same height above ground as before.
			// Ground level at the exit is measured as the higher of the
			// two floor heights at the exit linedef.
			if (!P_TeleportMove (thing, x, y,
								 z + sides[l->sidenum[stepdown]].sector->floorplane.ZatPoint (x, y), false))
				return false;

			// Rotate thing's orientation according to difference in linedef angles
			thing->angle += angle;

			// Momentum of thing crossing teleporter linedef
			x = thing->momx;
			y = thing->momy;

			// Rotate thing's momentum to come out of exit just like it entered
			thing->momx = FixedMul(x, c) - FixedMul(y, s);
			thing->momy = FixedMul(y, c) + FixedMul(x, s);

			if (thing->flags2 & MF2_FLOORCLIP)
			{
				thing->AdjustFloorClip ();
			}

			// Adjust a player's view, in case there has been a height change
			if (player)
			{
				// Save the current deltaviewheight, used in stepping
				fixed_t deltaviewheight = player->deltaviewheight;

				// Clear deltaviewheight, since we don't want any changes now
				player->deltaviewheight = 0;

				// Set player's view according to the newly set parameters
				P_CalcHeight(player);

				// Reset the delta to have the same dynamics as before
				player->deltaviewheight = deltaviewheight;
			}

			return true;
		}
	return false;
}

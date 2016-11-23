/*
#include "actor.h"
#include "info.h"
#include "p_enemy.h"
#include "p_local.h"
#include "a_action.h"
#include "templates.h"
#include "m_bbox.h"
#include "vm.h"
#include "doomstat.h"
*/

enum PA_Flags
{
	PAF_NOSKULLATTACK = 1,
	PAF_AIMFACING = 2,
	PAF_NOTARGET = 4,
};

//
// A_PainShootSkull
// Spawn a lost soul and launch it at the target
//
void A_PainShootSkull (VMFrameStack *stack, AActor *self, DAngle Angle, PClassActor *spawntype, int flags = 0, int limit = -1)
{
	AActor *other;
	double prestep;

	if (spawntype == NULL) spawntype = PClass::FindActor("LostSoul");
	assert(spawntype != NULL);
	if (self->DamageType == NAME_Massacre) return;

	// [RH] check to make sure it's not too close to the ceiling
	if (self->Top() + 8 > self->ceilingz)
	{
		if (self->flags & MF_FLOAT)
		{
			self->Vel.Z -= 2;
			self->flags |= MF_INFLOAT;
			self->flags4 |= MF4_VFRICTION;
		}
		return;
	}

	// [RH] make this optional
	if (limit == -1 && (i_compatflags & COMPATF_LIMITPAIN))
		limit = 21;

	if (limit)
	{
		// count total number of skulls currently on the level
		// if there are already 21 skulls on the level, don't spit another one
		int count = limit;
		FThinkerIterator iterator (spawntype);
		DThinker *othink;

		while ( (othink = iterator.Next ()) )
		{
			if (--count == 0)
				return;
		}
	}

	// okay, there's room for another one
	double otherradius = GetDefaultByType(spawntype)->radius;
	prestep = 4 + (self->radius + otherradius) * 1.5;

	DVector2 move = Angle.ToVector(prestep);
	DVector3 spawnpos = self->PosPlusZ(8.0);
	DVector3 destpos = spawnpos + move;

	other = Spawn(spawntype, spawnpos, ALLOW_REPLACE);

	// Now check if the spawn is legal. Unlike Boom's hopeless attempt at fixing it, let's do it the same way
	// P_XYMovement solves the line skipping: Spawn the Lost Soul near the PE's center and then use multiple
	// smaller steps to get it to its intended position. This will also result in proper clipping, but
	// it will avoid all the problems of the Boom method, which checked too many lines and despite some
	// adjustments never worked with portals.

	if (other != nullptr)
	{
		double maxmove = other->radius - 1;

		if (maxmove <= 0) maxmove = MAXMOVE;

		const double xspeed = fabs(move.X);
		const double yspeed = fabs(move.Y);

		int steps = 1;

		if (xspeed > yspeed)
		{
			if (xspeed > maxmove)
			{
				steps = int(1 + xspeed / maxmove);
			}
		}
		else
		{
			if (yspeed > maxmove)
			{
				steps = int(1 + yspeed / maxmove);
			}
		}

		DVector2 stepmove = move / steps;
		self->flags &= ~MF_SOLID;	// make it solid again
		other->flags2 |= MF2_NOTELEPORT;	// we do not want the LS to teleport
		for (int i = 0; i < steps; i++)
		{
			DVector2 ptry = other->Pos().XY() + stepmove;
			DAngle oldangle = other->Angles.Yaw;
			if (!P_TryMove(other, ptry, 0, nullptr))
			{
				// kill it immediately
				other->ClearCounters();
				P_DamageMobj(other, self, self, TELEFRAG_DAMAGE, NAME_None);
				return;
			}

			if (other->Pos().XY() != ptry)
			{
				// If the new position does not match the desired position, the player
				// must have gone through a portal.
				// For that we need to adjust the movement vector for the following steps.
				DAngle anglediff = deltaangle(oldangle, other->Angles.Yaw);

				if (anglediff != 0)
				{
					stepmove = stepmove.Rotated(anglediff);
				}
			}

		}
		self->flags |= MF_SOLID;	// don't let the LS be stuck in the PE while checking the move

		// [RH] Lost souls hate the same things as their pain elementals
		other->CopyFriendliness (self, !(flags & PAF_NOTARGET));

		if (!(flags & PAF_NOSKULLATTACK))
		{
			DECLARE_VMFUNC(AActor, A_SkullAttack);
			CallAction(stack, A_SkullAttack, other);
		}
	}
}


DEFINE_ACTION_FUNCTION(AActor, A_PainShootSkull)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_CLASS(spawntype, AActor);
	PARAM_FLOAT(angle);
	PARAM_INT_DEF(flags);
	PARAM_INT_DEF(limit);
	A_PainShootSkull(stack, self, angle, spawntype, flags, limit);

	return 0;
}


/*
#include "actor.h"
#include "info.h"
#include "p_enemy.h"
#include "p_local.h"
#include "a_action.h"
#include "templates.h"
#include "m_bbox.h"
#include "thingdef/thingdef.h"
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
void A_PainShootSkull (AActor *self, DAngle Angle, PClassActor *spawntype, int flags = 0, int limit = -1)
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
	prestep = 4 + (self->radius + GetDefaultByType(spawntype)->radius) * 1.5;

	// NOTE: The following code contains some advance work for line-to-line portals which is currenty inactive.

	DVector2 dist = Angle.ToVector(prestep);
	DVector3 pos = self->Vec3Offset(dist.X, dist.Y, 8., true);
	DVector3 src = self->Pos();

	for (int i = 0; i < 2; i++)
	{
		// Check whether the Lost Soul is being fired through a 1-sided	// phares
		// wall or an impassible line, or a "monsters can't cross" line.//   |
		// If it is, then we don't allow the spawn.						//   V

		FBoundingBox box(MIN(src.X, pos.X), MIN(src.Y, pos.Y), MAX(src.X, pos.X), MAX(src.Y, pos.Y));
		FBlockLinesIterator it(box);
		line_t *ld;
		bool inportal = false;

		while ((ld = it.Next()))
		{
			if (ld->isLinePortal() && i == 0)
			{
				if (P_PointOnLineSidePrecise(src, ld) == 0 &&
					P_PointOnLineSidePrecise(pos, ld) == 1)
				{
					// crossed a portal line from front to back, we need to repeat the check on the other side as well.
					inportal = true;
				}
			}
			else if (!(ld->flags & ML_TWOSIDED) ||
				(ld->flags & (ML_BLOCKING | ML_BLOCKMONSTERS | ML_BLOCKEVERYTHING)))
			{
				if (box.inRange(ld))
				{
					if (P_PointOnLineSidePrecise(src, ld) != P_PointOnLineSidePrecise(pos, ld))
						return;  // line blocks trajectory				//   ^
				}
			}
		}
		if (!inportal) break;

		// recalculate position and redo the check on the other side of the portal
		pos = self->Vec3Offset(dist.X, dist.Y, 8.);
		src.X = pos.X - dist.X;
		src.Y = pos.Y - dist.Y;

	}

	other = Spawn (spawntype, pos, ALLOW_REPLACE);

	// Check to see if the new Lost Soul's z value is above the
	// ceiling of its new sector, or below the floor. If so, kill it.

	if (other->Top() > other->Sector->HighestCeilingAt(other) ||
        other->Z() < other->Sector->LowestFloorAt(other))
	{
		// kill it immediately
		P_DamageMobj (other, self, self, TELEFRAG_DAMAGE, NAME_None);//  ^
		return;														//   |
	}																// phares

	// Check for movements.

	if (!P_CheckPosition (other, other->Pos()))
	{
		// kill it immediately
		P_DamageMobj (other, self, self, TELEFRAG_DAMAGE, NAME_None);		
		return;
	}

	// [RH] Lost souls hate the same things as their pain elementals
	other->CopyFriendliness (self, !(flags & PAF_NOTARGET));

	if (!(flags & PAF_NOSKULLATTACK))
		A_SkullAttack(other, SKULLSPEED);
}


//
// A_PainAttack
// Spawn a lost soul and launch it at the target
// 
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_PainAttack)
{
	PARAM_ACTION_PROLOGUE;

	if (!self->target)
		return 0;

	PARAM_CLASS_OPT (spawntype, AActor)	{ spawntype = NULL; }
	PARAM_ANGLE_OPT (angle)				{ angle = 0.; }
	PARAM_INT_OPT   (flags)				{ flags = 0; }
	PARAM_INT_OPT   (limit)				{ limit = -1; }

	if (!(flags & PAF_AIMFACING))
		A_FaceTarget (self);
	A_PainShootSkull (self, self->Angles.Yaw + angle, spawntype, flags, limit);
	return 0;
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_DualPainAttack)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_CLASS_OPT(spawntype, AActor) { spawntype = NULL; }

	if (!self->target)
		return 0;

	A_FaceTarget (self);
	A_PainShootSkull (self, self->Angles.Yaw + 45., spawntype);
	A_PainShootSkull (self, self->Angles.Yaw - 45., spawntype);
	return 0;
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_PainDie)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_CLASS_OPT(spawntype, AActor) { spawntype = NULL; }

	if (self->target != NULL && self->IsFriend(self->target))
	{ // And I thought you were my friend!
		self->flags &= ~MF_FRIENDLY;
	}
	A_Unblock(self, true);
	A_PainShootSkull (self, self->Angles.Yaw + 90, spawntype);
	A_PainShootSkull (self, self->Angles.Yaw + 180, spawntype);
	A_PainShootSkull (self, self->Angles.Yaw + 270, spawntype);
	return 0;
}

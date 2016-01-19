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

DECLARE_ACTION(A_SkullAttack)

static const PClass *GetSpawnType(DECLARE_PARAMINFO)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_CLASS(spawntype, 0);

	if (spawntype == NULL) spawntype = PClass::FindClass("LostSoul");
	return spawntype;
}


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
void A_PainShootSkull (AActor *self, angle_t angle, const PClass *spawntype, int flags = 0, int limit = -1)
{
	AActor *other;
	int prestep;

	if (spawntype == NULL) return;
	if (self->DamageType==NAME_Massacre) return;

	// [RH] check to make sure it's not too close to the ceiling
	if (self->Top() + 8*FRACUNIT > self->ceilingz)
	{
		if (self->flags & MF_FLOAT)
		{
			self->velz -= 2*FRACUNIT;
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
	prestep = 4*FRACUNIT +
		3*(self->radius + GetDefaultByType(spawntype)->radius)/2;

	// NOTE: The following code contains some advance work for line-to-line portals which is currenty inactive.

	fixedvec2 dist = Vec2Angle(prestep, angle);
	fixedvec3 pos = self->Vec3Offset(dist.x, dist.y, 8 * FRACUNIT, true);
	fixedvec3 src = self->Pos();

	for (int i = 0; i < 2; i++)
	{
		// Check whether the Lost Soul is being fired through a 1-sided	// phares
		// wall or an impassible line, or a "monsters can't cross" line.//   |
		// If it is, then we don't allow the spawn.						//   V

		FBoundingBox box(MIN(src.x, pos.x), MIN(src.y, pos.y), MAX(src.x, pos.x), MAX(src.y, pos.y));
		FBlockLinesIterator it(box);
		line_t *ld;
		bool inportal = false;

		while ((ld = it.Next()))
		{
			if (ld->isLinePortal() && i == 0)
			{
				if (P_PointOnLineSidePrecise(src.x, src.y, ld) == 0 &&
					P_PointOnLineSidePrecise(pos.x, pos.y, ld) == 1)
				{
					// crossed a portal line from front to back, we need to repeat the check on the other side as well.
					inportal = true;
				}
			}
			else if (!(ld->flags & ML_TWOSIDED) ||
				(ld->flags & (ML_BLOCKING | ML_BLOCKMONSTERS | ML_BLOCKEVERYTHING)))
			{
				if (!(box.Left() > ld->bbox[BOXRIGHT] ||
					box.Right() < ld->bbox[BOXLEFT] ||
					box.Top() < ld->bbox[BOXBOTTOM] ||
					box.Bottom() > ld->bbox[BOXTOP]))
				{
					if (P_PointOnLineSidePrecise(src.x, src.y, ld) != P_PointOnLineSidePrecise(pos.x, pos.y, ld))
						return;  // line blocks trajectory				//   ^
				}
			}
		}
		if (!inportal) break;

		// recalculate position and redo the check on the other side of the portal
		pos = self->Vec3Offset(dist.x, dist.y, 8 * FRACUNIT, false);
		src.x = pos.x - dist.x;
		src.y = pos.y - dist.y;

	}

	other = Spawn (spawntype, pos.x, pos.y, pos.z, ALLOW_REPLACE);

	// Check to see if the new Lost Soul's z value is above the
	// ceiling of its new sector, or below the floor. If so, kill it.

	if ((other->Z() >
         (other->Sector->HighestCeiling(other) - other->height)) ||
        (other->Z() < other->Sector->LowestFloor(other)))
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
	if (!self->target)
		return;

	ACTION_PARAM_START(4);
	ACTION_PARAM_CLASS(spawntype, 0);
	ACTION_PARAM_ANGLE(angle, 1);
	ACTION_PARAM_INT(flags, 2);
	ACTION_PARAM_INT(limit, 3);

	if (spawntype == NULL) spawntype = PClass::FindClass("LostSoul");

	if (!(flags & PAF_AIMFACING))
		A_FaceTarget (self);
	A_PainShootSkull (self, self->angle+angle, spawntype, flags, limit);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_DualPainAttack)
{
	if (!self->target)
		return;

	const PClass *spawntype = GetSpawnType(PUSH_PARAMINFO);
	A_FaceTarget (self);
	A_PainShootSkull (self, self->angle + ANG45, spawntype);
	A_PainShootSkull (self, self->angle - ANG45, spawntype);
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_PainDie)
{
	if (self->target != NULL && self->IsFriend (self->target))
	{ // And I thought you were my friend!
		self->flags &= ~MF_FRIENDLY;
	}
	const PClass *spawntype = GetSpawnType(PUSH_PARAMINFO);
	A_Unblock(self, true);
	A_PainShootSkull (self, self->angle + ANG90, spawntype);
	A_PainShootSkull (self, self->angle + ANG180, spawntype);
	A_PainShootSkull (self, self->angle + ANG270, spawntype);
}

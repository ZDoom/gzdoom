#pragma once

#include "actor.h"
#include "r_defs.h"
#include "m_random.h"

//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//		Handling of MF_SHADOW related code for attack and aiming functions.
//
//-----------------------------------------------------------------------------


// RNG VARIABLES ------------------------------------------------
extern FRandom pr_spawnmissile;
extern FRandom pr_facetarget;
extern FRandom pr_railface;
extern FRandom pr_crailgun;

//==========================================================================
//
// Generic checks
//
//==========================================================================

inline bool CheckShadowFlags(AActor* self, AActor* other)
{
	return (other->flags & MF_SHADOW && !(self->flags6 & MF6_SEEINVISIBLE));
}

//==========================================================================
//
// Function-specific inlines.
//
//==========================================================================

inline void P_SpawnMissileXYZ_ShadowHandling(AActor* source, AActor* target, AActor* missile)
{
	// invisible target: rotate velocity vector in 2D
	// [RC] Now monsters can aim at invisible player as if they were fully visible.
	if (CheckShadowFlags(source,target))
	{
		DAngle an = DAngle::fromDeg(pr_spawnmissile.Random2() * (22.5 / 256));
		double c = an.Cos();
		double s = an.Sin();

		double newx = missile->Vel.X * c - missile->Vel.Y * s;
		double newy = missile->Vel.X * s + missile->Vel.Y * c;

		missile->Vel.X = newx;
		missile->Vel.Y = newy;
	}
	return;
}

//P_SpawnMissileZAimed uses a local variable for the angle it passes on.
inline DAngle P_SpawnMissileZAimed_ShadowHandling(AActor* source, AActor* target)
{
	if (CheckShadowFlags(source,target))
	{
		return DAngle::fromDeg(pr_spawnmissile.Random2() * (16. / 360.));
	}
	return nullAngle;
}

inline void A_Face_ShadowHandling(AActor* self, AActor* other, DAngle max_turn, DAngle other_angle)
{
	// This will never work well if the turn angle is limited.
	if (max_turn == nullAngle && (self->Angles.Yaw == other_angle) && CheckShadowFlags(self, other))
	{
		self->Angles.Yaw += DAngle::fromDeg(pr_facetarget.Random2() * (45 / 256.));
	}
	return;
}

inline void A_MonsterRail_ShadowHandling(AActor* self)
{
	if (CheckShadowFlags(self, self->target))
	{
		self->Angles.Yaw += DAngle::fromDeg(pr_railface.Random2() * 45. / 256);
	}
	return;
}

inline void A_CustomRailgun_ShadowHandling(AActor* self)
{
	if (CheckShadowFlags(self, self->target))
	{
		DAngle rnd = DAngle::fromDeg(pr_crailgun.Random2() * (45. / 256.));
		self->Angles.Yaw += rnd;
	}
	return;
}

//A_WolfAttack directly harms the target instead of firing a hitscan or projectile. So it handles shadows by lowering the chance of harming the target.
inline bool A_WolfAttack_ShadowHandling(AActor* self)
{
	return (CheckShadowFlags(self, self->target));
}
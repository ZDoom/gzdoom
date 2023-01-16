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

struct ShadowCheckData
{
	bool HitShadow;
};

static ETraceStatus CheckForShadowBlockers(FTraceResults& res, void* userdata)
{
	ShadowCheckData* output = (ShadowCheckData*)userdata;
	if (res.HitType == TRACE_HitActor && res.Actor && (res.Actor->flags9 & MF9_SHADOWBLOCK))
	{
		output->HitShadow = true;
		return TRACE_Stop;
	}

	if (res.HitType != TRACE_HitActor)
	{
		return TRACE_Stop;
	}

	return TRACE_Continue;
}

// [inkoalawetrust] Check if an MF9_SHADOWBLOCK actor is standing between t1 and t2.
inline bool P_CheckForShadowBlock(AActor* t1, AActor* t2, DVector3 pos)
{
	FTraceResults result;
	ShadowCheckData ShadowCheck;
	ShadowCheck.HitShadow = false;
	DVector3 dir = t1->Vec3To(t2);
	double dist = dir.Length();

	Trace(pos, t1->Sector, dir, dist, ActorFlags::FromInt(0xFFFFFFFF), ML_BLOCKEVERYTHING, t1, result, 0, CheckForShadowBlockers, &ShadowCheck);

	return ShadowCheck.HitShadow;
}

inline bool AffectedByShadows(AActor* self, AActor* other)
{
	return (!(self->flags6 & MF6_SEEINVISIBLE) || self->flags9 & MF9_SHADOWAIM);
}

inline bool CheckForShadows(AActor* self, AActor* other, DVector3 pos)
{
	return (other->flags & MF_SHADOW || self->flags9 & MF9_DOSHADOWBLOCK && P_CheckForShadowBlock(self, other, pos));
}

inline bool PerformShadowChecks(AActor* self, AActor* other, DVector3 pos)
{
	return (AffectedByShadows(self, other) && CheckForShadows(self, other, pos));
}

//==========================================================================
//
// Function-specific inlines.
//
//==========================================================================

inline void P_SpawnMissileXYZ_ShadowHandling(AActor* source, AActor* target, AActor* missile, DVector3 pos)
{
	// invisible target: rotate velocity vector in 2D
	// [RC] Now monsters can aim at invisible player as if they were fully visible.
	if (PerformShadowChecks(source,target,pos))
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
inline DAngle P_SpawnMissileZAimed_ShadowHandling(AActor* source, AActor* target, DVector3 pos)
{
	if (PerformShadowChecks(source,target,pos))
	{
		return DAngle::fromDeg(pr_spawnmissile.Random2() * (16. / 360.));
	}
	return nullAngle;
}

inline void A_Face_ShadowHandling(AActor* self, AActor* other, DAngle max_turn, DAngle other_angle)
{
	// This will never work well if the turn angle is limited.
	if (max_turn == nullAngle && (self->Angles.Yaw == other_angle) && PerformShadowChecks(self, other,self->PosAtZ(self->Center())))
	{
		self->Angles.Yaw += DAngle::fromDeg(pr_facetarget.Random2() * (45 / 256.));
	}
	return;
}

inline void A_MonsterRail_ShadowHandling(AActor* self)
{
	double shootZ = self->Center() - self->FloatSpeed - self->Floorclip; // The formula P_RailAttack uses, minus offset_z since A_MonsterRail doesn't use it.
	
	if (PerformShadowChecks(self, self->target,self->PosAtZ(shootZ)))
	{
		self->Angles.Yaw += DAngle::fromDeg(pr_railface.Random2() * 45. / 256);
	}
	return;
}

//Also passes parameters to determine a firing position for the SHADOWBLOCK check.
inline void A_CustomRailgun_ShadowHandling(AActor* self, double spawnofs_xy, double spawnofs_z, DAngle spread_xy, int flags)
{
	// [inkoalawetrust] The exact formula P_RailAttack uses to determine where the railgun trace should spawn from.
	DVector2 shootXY = (self->Vec2Angle(spawnofs_xy, (self->Angles.Yaw + spread_xy) - DAngle::fromDeg(90.)));
	double shootZ = self->Center() - self->FloatSpeed + spawnofs_z - self->Floorclip;
	if (flags & 16) shootZ += self->AttackOffset(); //16 is RGF_CENTERZ 
	DVector3 checkPos;
	checkPos.X = shootXY.X;
	checkPos.Y = shootXY.Y;
	checkPos.Z = shootZ;

	if (PerformShadowChecks(self, self->target, checkPos))
	{
		DAngle rnd = DAngle::fromDeg(pr_crailgun.Random2() * (45. / 256.));
		self->Angles.Yaw += rnd;
	}
	return;
}

//A_WolfAttack directly harms the target instead of firing a hitscan or projectile. So it handles shadows by lowering the chance of harming the target.
inline bool A_WolfAttack_ShadowHandling(AActor* self)
{
	return (PerformShadowChecks(self, self->target, self->PosAtZ(self->Center())));
}
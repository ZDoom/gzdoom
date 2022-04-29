#pragma once

#include "actor.h"
#include "r_defs.h"
#include "g_levellocals.h"
#include "d_player.h"
// These depend on both actor.h and r_defs.h so they cannot be in either file without creating a circular dependency.

inline DVector3 AActor::PosRelative(int portalgroup) const
{
	return Pos() + Level->Displacements.getOffset(Sector->PortalGroup, portalgroup);
}

inline DVector3 AActor::PosRelative(const AActor *other) const
{
	return Pos() + Level->Displacements.getOffset(Sector->PortalGroup, other->Sector->PortalGroup);
}

inline DVector3 AActor::PosRelative(sector_t *sec) const
{
	return Pos() + Level->Displacements.getOffset(Sector->PortalGroup, sec->PortalGroup);
}

inline DVector3 AActor::PosRelative(const line_t *line) const
{
	return Pos() + Level->Displacements.getOffset(Sector->PortalGroup, line->frontsector->PortalGroup);
}

inline void AActor::ClearInterpolation()
{
	Prev = Pos();
	PrevAngles = Angles;
	if (Sector) PrevPortalGroup = Sector->PortalGroup;
	else PrevPortalGroup = 0;
}

inline double secplane_t::ZatPoint(const AActor *ac) const
{
	return (D + normal.X*ac->X() + normal.Y*ac->Y()) * negiC;
}

inline double sector_t::HighestCeilingAt(AActor *a, sector_t **resultsec)
{
	return ::HighestCeilingAt(this, a->X(), a->Y(), resultsec);
}

inline double sector_t::LowestFloorAt(AActor *a, sector_t **resultsec)
{
	return ::LowestFloorAt(this, a->X(), a->Y(), resultsec);
}

inline double AActor::GetBobOffset(double ticfrac) const
{
	if (!(flags2 & MF2_FLOATBOB))
	{
		return 0;
	}
	return BobSin(FloatBobPhase + Level->maptime + ticfrac) * FloatBobStrength;
}

inline double AActor::GetCameraHeight() const
{
	return CameraHeight == INT_MIN ? Height / 2 : CameraHeight;
}


inline FDropItem *AActor::GetDropItems() const
{
	return GetInfo()->DropItems;
}

inline double AActor::GetGravity() const
{
	if (flags & MF_NOGRAVITY) return 0;
	return Level->gravity * Sector->gravity * Gravity * 0.00125;
}

inline double AActor::AttackOffset(double offset)
{
	if (player != NULL)
	{
		return (FloatVar(NAME_AttackZOffset) + offset) * player->crouchfactor;
	}
	else
	{
		return 8 + offset;
	}

}

inline DVector2 AActor::Vec2Offset(double dx, double dy, bool absolute)
{
	if (absolute)
	{
		return { X() + dx, Y() + dy };
	}
	else
	{
		return Level->GetPortalOffsetPosition(X(), Y(), dx, dy);
	}
}


inline DVector3 AActor::Vec2OffsetZ(double dx, double dy, double atz, bool absolute)
{
	if (absolute)
	{
		return{ X() + dx, Y() + dy, atz };
	}
		else
	{
		DVector2 v = Level->GetPortalOffsetPosition(X(), Y(), dx, dy);
		return DVector3(v, atz);
	}
}

inline DVector2 AActor::Vec2Angle(double length, DAngle angle, bool absolute)
{
	if (absolute)
	{
		return{ X() + length * angle.Cos(), Y() + length * angle.Sin() };
	}
	else
	{
		return Level->GetPortalOffsetPosition(X(), Y(), length*angle.Cos(), length*angle.Sin());
	}
}

inline DVector3 AActor::Vec3Offset(double dx, double dy, double dz, bool absolute)
{
	if (absolute)
	{
		return { X() + dx, Y() + dy, Z() + dz };
	}
	else
		{
		DVector2 v = Level->GetPortalOffsetPosition(X(), Y(), dx, dy);
		return DVector3(v, Z() + dz);
	}
}

inline DVector3 AActor::Vec3Offset(const DVector3 &ofs, bool absolute)
{
	return Vec3Offset(ofs.X, ofs.Y, ofs.Z, absolute);
}

inline DVector3 AActor::Vec3Angle(double length, DAngle angle, double dz, bool absolute)
{
	if (absolute)
	{
		return{ X() + length * angle.Cos(), Y() + length * angle.Sin(), Z() + dz };
	}
	else
		{
		DVector2 v = Level->GetPortalOffsetPosition(X(), Y(), length*angle.Cos(), length*angle.Sin());
		return DVector3(v, Z() + dz);
	}
}

inline bool AActor::isFrozen() const
{
	if (!(flags5 & MF5_NOTIMEFREEZE))
	{
		auto state = Level->isFrozen();
		if (state)
		{
			if (player == nullptr || player->Bot != nullptr) return true;

			// This is the only place in the entire game where the two freeze flags need different treatment.
			// The time freezer flag also freezes other players, the global setting does not.

			if ((state & 1) && player->timefreezer == 0)
			{
				return true;
			}
		}
	}
	return false;
}

// Consolidated from all (incomplete) variants that check if a line should block.
inline bool P_IsBlockedByLine(AActor* actor, line_t* line)
{
	// Keep this stuff readable - so no chained and nested 'if's!

	// Unconditional blockers.
	if (line->flags & (ML_BLOCKING | ML_BLOCKEVERYTHING)) return true;

	// MBF considers that friendly monsters are not blocked by monster-blocking lines.
	// This is added here as a compatibility option. Note that monsters that are dehacked
	// into being friendly with the MBF flag automatically gain MF3_NOBLOCKMONST, so this
	// just optionally generalizes the behavior to other friendly monsters.

	if (!((actor->flags3 & MF3_NOBLOCKMONST)
		|| ((actor->Level->i_compatflags & COMPATF_NOBLOCKFRIENDS) && (actor->flags & MF_FRIENDLY))))
	{
		// the regular 'blockmonsters' flag.
		if (line->flags & ML_BLOCKMONSTERS) return true;
		// MBF21's flag for walking monsters
		if ((line->flags2 & ML2_BLOCKLANDMONSTERS) && actor->Level->MBF21Enabled() && !(actor->flags & MF_FLOAT)) return true;
	}

	// Blocking players
	if ((((actor->player != nullptr) || (actor->flags8 & MF8_BLOCKASPLAYER)) && (line->flags & ML_BLOCK_PLAYERS)) && actor->Level->MBF21Enabled()) return true;

	// Blocking floaters.
	if ((actor->flags & MF_FLOAT) && (line->flags & ML_BLOCK_FLOATERS)) return true;

	return false;
}

// For Dehacked modified actors we need to dynamically check the bounce factors because MBF didn't bother to implement this properly and with other flags changing this must adjust.
inline double GetMBFBounceFactor(AActor* actor)
{
	if (actor->BounceFlags & BOUNCE_DEH) // only when modified through Dehacked. 
	{
		constexpr double MBF_BOUNCE_NOGRAVITY = 1;				// With NOGRAVITY: full momentum
		constexpr double MBF_BOUNCE_FLOATDROPOFF = 0.85;		// With FLOAT and DROPOFF: 85%
		constexpr double MBF_BOUNCE_FLOAT = 0.7;				// With FLOAT alone: 70%
		constexpr double MBF_BOUNCE_DEFAULT = 0.45;				// Without the above flags: 45%

		if (actor->flags & MF_NOGRAVITY) return MBF_BOUNCE_NOGRAVITY;
		if (actor->flags & MF_FLOAT) return (actor->flags & MF_DROPOFF) ? MBF_BOUNCE_FLOATDROPOFF : MBF_BOUNCE_FLOAT;
		return MBF_BOUNCE_DEFAULT;
	}
	return actor->bouncefactor;
}

inline double GetWallBounceFactor(AActor* actor)
{
	if (actor->BounceFlags & BOUNCE_DEH) // only when modified through Dehacked. 
	{
		constexpr double MBF_BOUNCE_NOGRAVITY = 1;				// With NOGRAVITY: full momentum
		constexpr double MBF_BOUNCE_WALL = 0.5;					// Bouncing off walls: 50%
		return ((actor->flags & MF_NOGRAVITY) ? MBF_BOUNCE_NOGRAVITY : MBF_BOUNCE_WALL);
	}
	return actor->wallbouncefactor;
}

// Yet another hack for MBF...
inline bool CanJump(AActor* actor)
{
	return (actor->flags6 & MF6_CANJUMP) || (
		(actor->BounceFlags & BOUNCE_MBF) && actor->IsSentient() && (actor->flags & MF_FLOAT));
}
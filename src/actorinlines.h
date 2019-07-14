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

inline bool AActor::isFrozen()
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


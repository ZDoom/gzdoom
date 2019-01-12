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

/*
inline DVector3 PosRelative(const DVector3 &pos, line_t *line, sector_t *refsec = NULL)
{
	return pos + Level->Displacements.getOffset(refsec->PortalGroup, line->frontsector->PortalGroup);
}
*/


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

inline bool AActor::isFrozen()
{
	if (!(flags5 & MF5_NOTIMEFREEZE))
	{
		auto state = currentSession->isFrozen();
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


class FActorIterator
{
public:
	FActorIterator (FLevelLocals *l, int i) : Level(l), base (nullptr), id (i)
	{
	}
	FActorIterator (int i, AActor *start) : Level(start->Level), base (start), id (i)
	{
	}
	AActor *Next ()
	{
		if (id == 0)
			return nullptr;
		if (!base)
			base = Level->TIDHash[id & 127];
		else
			base = base->inext;

		while (base && base->tid != id)
			base = base->inext;

		return base;
	}
	void Reinit()
	{
		base = nullptr;
	}

private:
	FLevelLocals *Level;
	AActor *base;
	int id;
};

template<class T>
class TActorIterator : public FActorIterator
{
public:
	TActorIterator (FLevelLocals *Level, int id) : FActorIterator (Level, id) {}
	T *Next ()
	{
		AActor *actor;
		do
		{
			actor = FActorIterator::Next ();
		} while (actor && !actor->IsKindOf (RUNTIME_CLASS(T)));
		return static_cast<T *>(actor);
	}
};

class NActorIterator : public FActorIterator
{
	const PClass *type;
public:
	NActorIterator (FLevelLocals *Level, const PClass *cls, int id) : FActorIterator (Level, id) { type = cls; }
	NActorIterator (FLevelLocals *Level, FName cls, int id) : FActorIterator (Level, id) { type = PClass::FindClass(cls); }
	NActorIterator (FLevelLocals *Level, const char *cls, int id) : FActorIterator (Level, id) { type = PClass::FindClass(cls); }
	AActor *Next ()
	{
		AActor *actor;
		if (type == NULL) return NULL;
		do
		{
			actor = FActorIterator::Next ();
		} while (actor && !actor->IsKindOf (type));
		return actor;
	}
};

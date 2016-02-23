#ifndef __DSECTOREFFECT_H__
#define __DSECTOREFFECT_H__

#include "dthinker.h"
#include "r_defs.h"

class DSectorEffect : public DThinker
{
	DECLARE_CLASS (DSectorEffect, DThinker)
public:
	DSectorEffect (sector_t *sector);

	void Serialize (FArchive &arc);
	void Destroy();

	sector_t *GetSector() const { return m_Sector; }

protected:
	DSectorEffect ();
	sector_t *m_Sector;
};

class DMover : public DSectorEffect
{
	DECLARE_CLASS (DMover, DSectorEffect)
	HAS_OBJECT_POINTERS
public:
	DMover (sector_t *sector);
protected:
	enum EResult { ok, crushed, pastdest };
	TObjPtr<DInterpolation> interpolation;
private:
	bool MoveAttached(int crush, fixed_t move, int floorOrCeiling, bool resetfailed);
	EResult MovePlane (fixed_t speed, fixed_t dest, int crush, int floorOrCeiling, int direction, bool hexencrush);
protected:
	DMover ();
	void Serialize (FArchive &arc);
	void Destroy();
	void StopInterpolation(bool force = false);
	inline EResult MoveFloor (fixed_t speed, fixed_t dest, int crush, int direction, bool hexencrush)
	{
		return MovePlane (speed, dest, crush, 0, direction, hexencrush);
	}
	inline EResult MoveFloor (fixed_t speed, fixed_t dest, int direction)
	{
		return MovePlane (speed, dest, -1, 0, direction, false);
	}
	inline EResult MoveCeiling (fixed_t speed, fixed_t dest, int crush, int direction, bool hexencrush)
	{
		return MovePlane (speed, dest, crush, 1, direction, hexencrush);
	}
	inline EResult MoveCeiling (fixed_t speed, fixed_t dest, int direction)
	{
		return MovePlane (speed, dest, -1, 1, direction, false);
	}
};

class DMovingFloor : public DMover
{
	DECLARE_CLASS (DMovingFloor, DMover)
public:
	DMovingFloor (sector_t *sector);
protected:
	DMovingFloor ();
};

class DMovingCeiling : public DMover
{
	DECLARE_CLASS (DMovingCeiling, DMover)
public:
	DMovingCeiling (sector_t *sector);
protected:
	DMovingCeiling ();
};

#endif //__DSECTOREFFECT_H__

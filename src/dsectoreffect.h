#ifndef __DSECTOREFFECT_H__
#define __DSECTOREFFECT_H__

#include "dthinker.h"
#include "r_defs.h"

class DSectorEffect : public DThinker
{
	DECLARE_CLASS (DSectorEffect, DThinker)
public:
	DSectorEffect (sector_t *sector);

	
	void Serialize(FSerializer &arc);
	void OnDestroy() override;

	sector_t *GetSector() const { return m_Sector; }

	sector_t *m_Sector;

protected:
	DSectorEffect();

};

class DMover : public DSectorEffect
{
	DECLARE_ABSTRACT_CLASS (DMover, DSectorEffect)
	HAS_OBJECT_POINTERS
protected:
	DMover (sector_t *sector);

	TObjPtr<DInterpolation*> interpolation;
public:
	void StopInterpolation(bool force = false);

protected:
	DMover () {}
	
	void Serialize(FSerializer &arc);
	void OnDestroy() override;
};

class DMovingFloor : public DMover
{
	DECLARE_ABSTRACT_CLASS (DMovingFloor, DMover)
protected:
	DMovingFloor (sector_t *sector);
	DMovingFloor() {}
};

class DMovingCeiling : public DMover
{
	DECLARE_ABSTRACT_CLASS (DMovingCeiling, DMover)
protected:
	DMovingCeiling (sector_t *sector, bool interpolate = true);
	DMovingCeiling () {}
};

#endif //__DSECTOREFFECT_H__

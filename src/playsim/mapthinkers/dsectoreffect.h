#ifndef __DSECTOREFFECT_H__
#define __DSECTOREFFECT_H__

#include "dthinker.h"
#include "r_defs.h"

class DSectorEffect : public DThinker
{
	DECLARE_CLASS (DSectorEffect, DThinker)
public:
	static const int DEFAULT_STAT = STAT_SECTOREFFECT;
	void Construct(sector_t *sector);

	
	void Serialize(FSerializer &arc);
	void OnDestroy() override;

	sector_t *GetSector() const { return m_Sector; }

	sector_t *m_Sector;
};

class DMover : public DSectorEffect
{
	DECLARE_ABSTRACT_CLASS (DMover, DSectorEffect)
	HAS_OBJECT_POINTERS
protected:
	void Construct(sector_t *sector);

	TObjPtr<DInterpolation*> interpolation;
public:
	void StopInterpolation(bool force = false);

protected:

	void Serialize(FSerializer &arc);
	void OnDestroy() override;
};

class DMovingFloor : public DMover
{
	DECLARE_ABSTRACT_CLASS (DMovingFloor, DMover)
protected:
	void Construct(sector_t *sector);
};

class DMovingCeiling : public DMover
{
	DECLARE_ABSTRACT_CLASS (DMovingCeiling, DMover)
protected:
	void Construct(sector_t *sector, bool interpolate = true);
};

#endif //__DSECTOREFFECT_H__

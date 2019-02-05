#ifndef R_INTERPOLATE_H
#define R_INTERPOLATE_H

#include "dobject.h"

struct FLevelLocals;
//==========================================================================
//
//
//
//==========================================================================

class DInterpolation : public DObject
{
	friend struct FInterpolator;

	DECLARE_ABSTRACT_CLASS(DInterpolation, DObject)
	HAS_OBJECT_POINTERS

	TObjPtr<DInterpolation*> Next = nullptr;
	TObjPtr<DInterpolation*> Prev = nullptr;

protected:
	FLevelLocals *Level;
	int refcount = 0;

	DInterpolation(FLevelLocals *l = nullptr) : Level(l) {}

public:
	int AddRef();
	int DelRef(bool force = false);

	virtual void UnlinkFromMap();
	virtual void UpdateInterpolation() = 0;
	virtual void Restore() = 0;
	virtual void Interpolate(double smoothratio) = 0;
	
	virtual void Serialize(FSerializer &arc);
};

//==========================================================================
//
//
//
//==========================================================================

struct FInterpolator
{
	TObjPtr<DInterpolation*> Head = nullptr;
	bool didInterp = false;
	int count = 0;

	int CountInterpolations ();

public:
	void UpdateInterpolations();
	void AddInterpolation(DInterpolation *);
	void RemoveInterpolation(DInterpolation *);
	void DoInterpolations(double smoothratio);
	void RestoreInterpolations();
	void ClearInterpolations();
};


#endif


#ifndef R_INTERPOLATE_H
#define R_INTERPOLATE_H

#include "dobject.h"
//==========================================================================
//
//
//
//==========================================================================

class DInterpolation : public DObject
{
	friend struct FInterpolator;

	DECLARE_ABSTRACT_CLASS(DInterpolation, DObject)

	DInterpolation *Next;
	DInterpolation **Prev;
	int refcount;

protected:
	DInterpolation();

public:
	int AddRef();
	int DelRef();

	virtual void Destroy();
	virtual void UpdateInterpolation() = 0;
	virtual void Restore() = 0;
	virtual void Interpolate(fixed_t smoothratio) = 0;
	virtual void Serialize(FArchive &arc);
};

//==========================================================================
//
//
//
//==========================================================================

struct FInterpolator
{
	DInterpolation *Head;
	bool didInterp;
	int count;

	int CountInterpolations ();

public:
	FInterpolator()
	{
		Head = NULL;
		didInterp = false;
		count = 0;
	}
	void UpdateInterpolations();
	void AddInterpolation(DInterpolation *);
	void RemoveInterpolation(DInterpolation *);
	void DoInterpolations(fixed_t smoothratio);
	void RestoreInterpolations();
	void ClearInterpolations();
};

extern FInterpolator interpolator;



#endif


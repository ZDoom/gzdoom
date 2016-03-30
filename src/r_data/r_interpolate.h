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
	HAS_OBJECT_POINTERS

	TObjPtr<DInterpolation> Next;
	TObjPtr<DInterpolation> Prev;

protected:
	int refcount;

	DInterpolation();

public:
	int AddRef();
	int DelRef(bool force = false);

	virtual void Destroy();
	virtual void UpdateInterpolation() = 0;
	virtual void Restore() = 0;
	virtual void Interpolate(double smoothratio) = 0;
	virtual void Serialize(FArchive &arc);
};

//==========================================================================
//
//
//
//==========================================================================

struct FInterpolator
{
	TObjPtr<DInterpolation> Head;
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
	void DoInterpolations(double smoothratio);
	void RestoreInterpolations();
	void ClearInterpolations();
};

extern FInterpolator interpolator;



#endif


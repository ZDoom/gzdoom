#pragma once
#include "dobject.h"
#include "tarray.h"
#include "TRS.h"
#include "matrix.h"


class DBoneComponents : public DObject
{
	DECLARE_CLASS(DBoneComponents, DObject);
public:
	TArray<TArray<TRS>>			trscomponents;
	TArray<TArray<VSMatrix>>	trsmatrix;

	DBoneComponents() = default;
};

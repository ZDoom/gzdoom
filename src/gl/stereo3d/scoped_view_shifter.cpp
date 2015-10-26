#include "scoped_view_shifter.h"
#include "r_utility.h"

namespace s3d {

ScopedViewShifter::ScopedViewShifter(float dxyz[3]) // in meters
{
	// save original values
	cachedViewx = viewx;
	cachedViewy = viewy;
	cachedViewz = viewz;
	// modify values
	float fViewx = FIXED2FLOAT(viewx) - dxyz[0];
	float fViewy = FIXED2FLOAT(viewy) + dxyz[1];
	float fViewz = FIXED2FLOAT(viewz) + dxyz[2];
	viewx = FLOAT2FIXED(fViewx);
	viewy = FLOAT2FIXED(fViewy);
	viewz = FLOAT2FIXED(fViewz);
}

ScopedViewShifter::~ScopedViewShifter()
{
	// restore original values
	viewx = cachedViewx;
	viewy = cachedViewy;
	viewz = cachedViewz;
}

}
#pragma once

#include "r_data/matrix.h"
#include "r_utility.h"

struct HWViewpointUniforms
{
	VSMatrix mProjectionMatrix;
	VSMatrix mViewMatrix;
	VSMatrix mNormalViewMatrix;
	FVector4 mCameraPos;
	FVector4 mClipLine;

	float mGlobVis;
	int mPalLightLevels;
	int mViewHeight;

	void CalcDependencies()
	{
		mNormalViewMatrix.computeNormalMatrix(mViewMatrix);
	}

	void SetDefaults();

};

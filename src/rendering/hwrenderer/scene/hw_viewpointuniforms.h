#pragma once

#include "matrix.h"
#include "r_utility.h"

struct HWDrawInfo;

struct HWViewpointUniforms
{
	VSMatrix mProjectionMatrix;
	VSMatrix mViewMatrix;
	VSMatrix mNormalViewMatrix;
	FVector4 mCameraPos;
	FVector4 mClipLine;

	float mGlobVis = 1.f;
	int mPalLightLevels = 0;
	int mViewHeight = 0;
	float mClipHeight = 0.f;
	float mClipHeightDirection = 0.f;
	int mShadowmapFilter = 1;

	void CalcDependencies()
	{
		mNormalViewMatrix.computeNormalMatrix(mViewMatrix);
	}

	void SetDefaults(HWDrawInfo *drawInfo);

};




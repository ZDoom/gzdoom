#pragma once

#include "r_data/matrix.h"

struct HWViewpointUniforms
{
  VSMatrix mProjectionMatrix;
  VSMatrix mViewMatrix;
  VSMatrix mNormalViewMatrix;

  void CalcDependencies()
  {
    mNormalViewMatrix.computeNormalMatrix(mViewMatrix);
  }

  void SetDefaults()
  {
	  mProjectionMatrix.loadIdentity();
	  mViewMatrix.loadIdentity();
	  mNormalViewMatrix.loadIdentity();
  }

};

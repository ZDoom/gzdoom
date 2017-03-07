
#pragma once

#include "gl/dynlights/gl_lightbsp.h"
#include "tarray.h"

class ADynamicLight;

class FShadowMap
{
public:
	FShadowMap() { }
	~FShadowMap() { Clear(); }

	void Clear();
	void Update();

	int ShadowMapIndex(ADynamicLight *light) { return mLightToShadowmap[light]; }

	bool ShadowTest(ADynamicLight *light, const DVector3 &pos);

private:
	void UploadLights();

	FLightBSP mLightBSP;
	int mLightList = 0;
	TArray<float> mLights;
	TMap<ADynamicLight*, int> mLightToShadowmap;

	FShadowMap(const FShadowMap &) = delete;
	FShadowMap &operator=(FShadowMap &) = delete;
};

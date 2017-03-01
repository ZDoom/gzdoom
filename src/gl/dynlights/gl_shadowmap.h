
#pragma once

#include "gl/dynlights/gl_lightbsp.h"

class FShadowMap
{
public:
	FShadowMap() { }
	~FShadowMap() { Clear(); }

	void Clear();
	void Update();

private:
	void UploadLights();

	FLightBSP mLightBSP;
	int mLightList = 0;
	TArray<float> lights;

	FShadowMap(const FShadowMap &) = delete;
	FShadowMap &operator=(FShadowMap &) = delete;
};

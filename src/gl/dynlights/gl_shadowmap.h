
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
	FLightBSP mLightBSP;

	FShadowMap(const FShadowMap &) = delete;
	FShadowMap &operator=(FShadowMap &) = delete;
};

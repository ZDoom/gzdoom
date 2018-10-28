
#pragma once

#include "hwrenderer/dynlights/hw_shadowmap.h"

class IDataBuffer;

class FShadowMap : public IShadowMap
{
public:
	// Update shadow map texture
	void Update() override;
};

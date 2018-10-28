
#pragma once

#include "hwrenderer/dynlights/hw_shadowmap.h"

class IDataBuffer;

class FShadowMap : public IShadowMap
{
public:
	~FShadowMap() { Clear(); }

	// Release resources
	void Clear() override;

	// Update shadow map texture
	void Update() override;

private:
	// Upload the AABB-tree to the GPU
	void UploadAABBTree();

	// Upload light list to the GPU
	void UploadLights();

	// OpenGL storage buffer with the list of lights in the shadow map texture
	IDataBuffer *mLightList = nullptr;

	// OpenGL storage buffers for the AABB tree
	IDataBuffer *mNodesBuffer = nullptr;
	IDataBuffer *mLinesBuffer = nullptr;
};

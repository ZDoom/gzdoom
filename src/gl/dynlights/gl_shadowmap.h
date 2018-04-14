
#pragma once

#include "hwrenderer/dynlights/hw_aabbtree.h"
#include "tarray.h"
#include <memory>

class ADynamicLight;
struct level_info_t;

class FShadowMap
{
public:
	FShadowMap() { }
	~FShadowMap() { Clear(); }

	// Release resources
	void Clear();

	// Update shadow map texture
	void Update();

	// Return the assigned shadow map index for a given light
	int ShadowMapIndex(ADynamicLight *light);

	// Test if a world position is in shadow relative to the specified light and returns false if it is
	bool ShadowTest(ADynamicLight *light, const DVector3 &pos);

	// Returns true if gl_light_shadowmap is enabled and supported by the hardware
	bool IsEnabled() const;

private:
	// Upload the AABB-tree to the GPU
	void UploadAABBTree();

	// Upload light list to the GPU
	void UploadLights();

	// OpenGL storage buffer with the list of lights in the shadow map texture
	int mLightList = 0;

	// Working buffer for creating the list of lights. Stored here to avoid allocating memory each frame
	TArray<float> mLights;

	// The assigned shadow map index for each light
	TMap<ADynamicLight*, int> mLightToShadowmap;

	// OpenGL storage buffers for the AABB tree
	int mNodesBuffer = 0;
	int mLinesBuffer = 0;

	// Used to detect when a level change requires the AABB tree to be regenerated
	level_info_t *mLastLevel = nullptr;
	unsigned mLastNumNodes = 0;
	unsigned mLastNumSegs = 0;

	// AABB-tree of the level, used for ray tests
	std::unique_ptr<hwrenderer::LevelAABBTree> mAABBTree;

	FShadowMap(const FShadowMap &) = delete;
	FShadowMap &operator=(FShadowMap &) = delete;
};


#pragma once

#include "hw_aabbtree.h"
#include "stats.h"
#include <memory>

class ADynamicLight;
struct level_info_t;

class IShadowMap
{
public:
	IShadowMap() { }
	virtual ~IShadowMap() { }

	// Release resources
	virtual void Clear() = 0;

	// Update shadow map texture
	virtual void Update() = 0;

	// Test if a world position is in shadow relative to the specified light and returns false if it is
	bool ShadowTest(ADynamicLight *light, const DVector3 &pos);

	// Returns true if gl_light_shadowmap is enabled and supported by the hardware
	bool IsEnabled() const;

	static cycle_t UpdateCycles;
	static int LightsProcessed;
	static int LightsShadowmapped;
	
protected:
	void CollectLights();
	bool ValidateAABBTree();

	// Working buffer for creating the list of lights. Stored here to avoid allocating memory each frame
	TArray<float> mLights;

	// Used to detect when a level change requires the AABB tree to be regenerated
	level_info_t *mLastLevel = nullptr;
	unsigned mLastNumNodes = 0;
	unsigned mLastNumSegs = 0;

	// AABB-tree of the level, used for ray tests
	std::unique_ptr<hwrenderer::LevelAABBTree> mAABBTree;

	IShadowMap(const IShadowMap &) = delete;
	IShadowMap &operator=(IShadowMap &) = delete;
};

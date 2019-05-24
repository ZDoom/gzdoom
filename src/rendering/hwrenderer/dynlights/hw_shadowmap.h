
#pragma once

#include "hw_aabbtree.h"
#include "stats.h"
#include <memory>

struct FDynamicLight;
struct level_info_t;
class IDataBuffer;
struct FLevelLocals;

class IShadowMap
{
public:
	IShadowMap() { }
	virtual ~IShadowMap();

	void Reset();

	// Test if a world position is in shadow relative to the specified light and returns false if it is
	bool ShadowTest(FDynamicLight *light, const DVector3 &pos);

	// Returns true if gl_light_shadowmap is enabled and supported by the hardware
	bool IsEnabled() const;

	static cycle_t UpdateCycles;
	static int LightsProcessed;
	static int LightsShadowmapped;

	bool PerformUpdate();
	void FinishUpdate()
	{
		UpdateCycles.Clock();
	}

	unsigned int NodesCount() const
	{
		assert(mAABBTree);
		return mAABBTree->NodesCount();
	}

protected:
	void CollectLights();
	bool ValidateAABBTree(FLevelLocals *lev);

	// Upload the AABB-tree to the GPU
	void UploadAABBTree();

	// Upload light list to the GPU
	void UploadLights();

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

	// OpenGL storage buffer with the list of lights in the shadow map texture
	IDataBuffer *mLightList = nullptr;

	// OpenGL storage buffers for the AABB tree
	IDataBuffer *mNodesBuffer = nullptr;
	IDataBuffer *mLinesBuffer = nullptr;

};

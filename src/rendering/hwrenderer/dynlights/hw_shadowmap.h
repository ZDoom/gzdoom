
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
	bool ShadowTest(const DVector3 &lpos, const DVector3 &pos);

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

	void SetAABBTree(hwrenderer::LevelAABBTree* tree)
	{
		mAABBTree = tree;
		mNewTree = true;
	}

protected:
	void CollectLights();

	// Upload the AABB-tree to the GPU
	void UploadAABBTree();

	// Upload light list to the GPU
	void UploadLights();

	// Working buffer for creating the list of lights. Stored here to avoid allocating memory each frame
	TArray<float> mLights;

	// AABB-tree of the level, used for ray tests, owned by the playsim, not the renderer.
	hwrenderer::LevelAABBTree* mAABBTree = nullptr;
	bool mNewTree = false;

	IShadowMap(const IShadowMap &) = delete;
	IShadowMap &operator=(IShadowMap &) = delete;

	// OpenGL storage buffer with the list of lights in the shadow map texture
	// These buffers need to be accessed by the OpenGL backend directly so that they can be bound.
public:
	IDataBuffer *mLightList = nullptr;

	// OpenGL storage buffers for the AABB tree
	IDataBuffer *mNodesBuffer = nullptr;
	IDataBuffer *mLinesBuffer = nullptr;

};

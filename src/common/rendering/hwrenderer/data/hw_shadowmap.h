
#pragma once

#include "hw_aabbtree.h"
#include "stats.h"
#include <memory>
#include <functional>

class IBuffer;
class DFrameBuffer;

class ShadowMap
{
	DFrameBuffer* fb = nullptr;

public:
	ShadowMap(DFrameBuffer* fb) : fb(fb) { }
	virtual ~ShadowMap();

	// Test if a world position is in shadow relative to the specified light and returns false if it is
	bool ShadowTest(const DVector3 &lpos, const DVector3 &pos);

	static cycle_t UpdateCycles;
	static int LightsProcessed;
	static int LightsShadowmapped;

	void PerformUpdate();

	unsigned int NodesCount() const
	{
		assert(mAABBTree);
		return mAABBTree->NodesCount();
	}

	void SetAABBTree(hwrenderer::LevelAABBTree* tree)
	{
		if (mAABBTree != tree)
		{
			mAABBTree = tree;
			mNewTree = true;
		}
	}

	void SetCollectLights(std::function<void()> func)
	{
		CollectLights = std::move(func);
	}

	void SetLight(int index, float x, float y, float z, float r)
	{
		index *= 4;
		mLights[index] = x;
		mLights[index + 1] = y;
		mLights[index + 2] = z;
		mLights[index + 3] = r;
	}

	bool Enabled() const
	{
		return mAABBTree != nullptr;
	}

protected:
	// Working buffer for creating the list of lights. Stored here to avoid allocating memory each frame
	TArray<float> mLights;

	// AABB-tree of the level, used for ray tests, owned by the playsim, not the renderer.
	hwrenderer::LevelAABBTree* mAABBTree = nullptr;
	bool mNewTree = false;

	ShadowMap(const ShadowMap &) = delete;
	ShadowMap &operator=(ShadowMap &) = delete;

public:
	std::function<void()> CollectLights = nullptr;
};

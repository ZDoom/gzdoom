
#pragma once

#include "gl/dynlights/gl_aabbtree.h"
#include "tarray.h"
#include <memory>

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
	void UploadAABBTree();
	void UploadLights();

	int mLightList = 0;
	TArray<float> mLights;
	TMap<ADynamicLight*, int> mLightToShadowmap;

	int mNodesBuffer = 0;
	int mLinesBuffer = 0;

	int mLastNumNodes = 0;
	int mLastNumSegs = 0;

	std::unique_ptr<LevelAABBTree> mAABBTree;

	FShadowMap(const FShadowMap &) = delete;
	FShadowMap &operator=(FShadowMap &) = delete;
};

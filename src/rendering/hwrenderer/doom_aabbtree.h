#pragma once
#include "hw_aabbtree.h"

struct FLevelLocals;

// Axis aligned bounding box tree used for ray testing treelines.
class DoomLevelAABBTree : public hwrenderer::LevelAABBTree
{
public:
	// Constructs a tree for the current level
	DoomLevelAABBTree(FLevelLocals *lev);
	bool Update() override;

private:
	bool GenerateTree(const FVector2 *centroids, bool dynamicsubtree);

	// Generate a tree node and its children recursively
	int GenerateTreeNode(int *treelines, int num_lines, const FVector2 *centroids, int *work_buffer);

	TArray<int> mapLines;
	FLevelLocals *Level;
};


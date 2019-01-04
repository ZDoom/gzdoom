
#pragma once

#include "tarray.h"
#include "vectors.h"

namespace hwrenderer
{

// Node in a binary AABB tree
struct AABBTreeNode
{
	AABBTreeNode(const FVector2 &aabb_min, const FVector2 &aabb_max, int line_index) : aabb_left(aabb_min.X), aabb_top(aabb_min.Y), aabb_right(aabb_max.X), aabb_bottom(aabb_max.Y), left_node(-1), right_node(-1), line_index(line_index) { }
	AABBTreeNode(const FVector2 &aabb_min, const FVector2 &aabb_max, int left, int right) : aabb_left(aabb_min.X), aabb_top(aabb_min.Y), aabb_right(aabb_max.X), aabb_bottom(aabb_max.Y), left_node(left), right_node(right), line_index(-1) { }

	// Axis aligned bounding box for the node
	float aabb_left, aabb_top;
	float aabb_right, aabb_bottom;

	// Child node indices
	int left_node;
	int right_node;

	// AABBTreeLine index if it is a leaf node. Index is -1 if it is not.
	int line_index;

	// Padding to keep 16-byte length (this structure is uploaded to the GPU)
	int padding;
};

// Line segment for leaf nodes in an AABB tree
struct AABBTreeLine
{
	float x, y;
	float dx, dy;
};

// Axis aligned bounding box tree used for ray testing lines.
class LevelAABBTree
{
public:
	// Constructs a tree for the current level
	LevelAABBTree();

	// Shoot a ray from ray_start to ray_end and return the closest hit as a fractional value between 0 and 1. Returns 1 if no line was hit.
	double RayTest(const DVector3 &ray_start, const DVector3 &ray_end);

	bool Update();

	const void *Nodes() const { return nodes.Data(); }
	const void *Lines() const { return lines.Data(); }
	size_t NodesSize() const { return nodes.Size() * sizeof(AABBTreeNode); }
	size_t LinesSize() const { return lines.Size() * sizeof(AABBTreeLine); }

	const void *DynamicNodes() const { return nodes.Data() + dynamicStartNode; }
	const void *DynamicLines() const { return lines.Data() + dynamicStartLine; }
	size_t DynamicNodesSize() const { return (nodes.Size() - dynamicStartNode) * sizeof(AABBTreeNode); }
	size_t DynamicLinesSize() const { return (lines.Size() - dynamicStartLine) * sizeof(AABBTreeLine); }
	size_t DynamicNodesOffset() const { return dynamicStartNode * sizeof(AABBTreeNode); }
	size_t DynamicLinesOffset() const { return dynamicStartLine * sizeof(AABBTreeLine); }

private:
	bool GenerateTree(const FVector2 *centroids, bool dynamicsubtree);

	// Test if a ray overlaps an AABB node or not
	bool OverlapRayAABB(const DVector2 &ray_start2d, const DVector2 &ray_end2d, const AABBTreeNode &node);

	// Intersection test between a ray and a line segment
	double IntersectRayLine(const DVector2 &ray_start, const DVector2 &ray_end, int line_index, const DVector2 &raydelta, double rayd, double raydist2);

	// Generate a tree node and its children recursively
	int GenerateTreeNode(int *lines, int num_lines, const FVector2 *centroids, int *work_buffer);

	TArray<int> FindNodePath(unsigned int line, unsigned int node);

	// Nodes in the AABB tree. Last node is the root node.
	TArray<AABBTreeNode> nodes;

	// Line segments for the leaf nodes in the tree.
	TArray<AABBTreeLine> lines;

	int dynamicStartNode = 0;
	int dynamicStartLine = 0;

	TArray<int> mapLines;
};

} // namespace

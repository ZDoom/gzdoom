
#pragma once

#include <memory>

struct GPUNode
{
	GPUNode(const FVector2 &aabb_min, const FVector2 &aabb_max, int line_index) : aabb_left(aabb_min.X), aabb_top(aabb_min.Y), aabb_right(aabb_max.X), aabb_bottom(aabb_max.Y), left(-1), right(-1), line_index(line_index) { }
	GPUNode(const FVector2 &aabb_min, const FVector2 &aabb_max, int left, int right) : aabb_left(aabb_min.X), aabb_top(aabb_min.Y), aabb_right(aabb_max.X), aabb_bottom(aabb_max.Y), left(left), right(right), line_index(-1) { }

	float aabb_left, aabb_top;
	float aabb_right, aabb_bottom;
	int left;
	int right;
	int line_index;
	int padding;
};

struct GPULine
{
	float x, y;
	float dx, dy;
};

class Level2DShape
{
public:
	Level2DShape();

	TArray<GPUNode> nodes;
	int root;

private:
	int subdivide(int *lines, int num_lines, const FVector2 *centroids, int *work_buffer);
};

class FLightBSP
{
public:
	FLightBSP() { }
	~FLightBSP() { Clear(); }

	int GetNodesBuffer();
	int GetLinesBuffer();
	void Clear();

private:
	void UpdateBuffers();
	void GenerateBuffers();
	void UploadNodes();
	void UploadSegs();

	FLightBSP(const FLightBSP &) = delete;
	FLightBSP &operator=(FLightBSP &) = delete;

	int NodesBuffer = 0;
	int LinesBuffer = 0;
	int NumNodes = 0;
	int NumSegs = 0;

	std::unique_ptr<Level2DShape> Shape;
};

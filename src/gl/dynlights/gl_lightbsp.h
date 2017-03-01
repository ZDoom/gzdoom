
#pragma once

struct GPUNode
{
	float plane[4];
	int children[2];
	int linecount[2];
};

struct GPUSeg
{
	float x, y;
	float dx, dy;
	float bSolid;
	float padding1, padding2, padding3;
};

class FLightBSP
{
public:
	FLightBSP() { }
	~FLightBSP() { Clear(); }

	int GetNodesBuffer();
	int GetSegsBuffer();
	void Clear();

private:
	void UpdateBuffers();
	void GenerateBuffers();
	void UploadNodes();
	void UploadSegs();

	FLightBSP(const FLightBSP &) = delete;
	FLightBSP &operator=(FLightBSP &) = delete;

	int NodesBuffer = 0;
	int SegsBuffer = 0;
	int NumNodes = 0;
	int NumSegs = 0;
};

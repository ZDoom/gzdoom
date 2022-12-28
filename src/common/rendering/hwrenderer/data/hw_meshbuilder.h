#pragma once

#include "hw_renderstate.h"
#include "hw_material.h"
#include "flatvertices.h"

class Mesh;

class MeshApplyState
{
public:
	FRenderStyle RenderStyle;
	int SpecialEffect;
	bool TextureEnabled;
	float AlphaThreshold;
	int DepthFunc;

	StreamData streamData;
	FMaterialState material;

	uint8_t FogEnabled;
	uint8_t BrightmapEnabled;
	int TextureClamp;
	int TextureMode;
	int TextureModeFlags;

	float uLightLevel;
	float uFogDensity;
	float uLightFactor;
	float uLightDist;

	FVector2 uClipSplit;
};

class MeshDrawCommand
{
public:
	int DrawType;
	int Start;
	int Count;
	int ApplyIndex;
};

class MeshBuilder : public FRenderState
{
public:
	MeshBuilder();

	// Vertices
	std::pair<FFlatVertex*, unsigned int> AllocVertices(unsigned int count) override;

	// Draw commands
	void Draw(int dt, int index, int count, bool apply = true) override;
	void DrawIndexed(int dt, int index, int count, bool apply = true) override;

	// Immediate render state change commands. These only change infrequently and should not clutter the render state.
	void SetDepthFunc(int func) override;

	// Commands not relevant for mesh building
	void Clear(int targets) override { }
	void ClearScreen() override { }
	void SetScissor(int x, int y, int w, int h) override { }
	void SetViewport(int x, int y, int w, int h) override { }
	void EnableMultisampling(bool on) override { }
	void EnableLineSmooth(bool on) override { }
	void EnableDrawBuffers(int count, bool apply) override { }
	void EnableClipDistance(int num, bool state) override { }
	void SetDepthRange(float min, float max) override { }
	bool SetDepthClamp(bool on) override { return false; }
	void SetDepthMask(bool on) override { }
	void SetColorMask(bool r, bool g, bool b, bool a) override { }
	void SetStencil(int offs, int op, int flags = -1) override { }
	void SetCulling(int mode) override { }
	void EnableStencil(bool on) override { }
	void EnableDepthTest(bool on) override { }

	std::unique_ptr<Mesh> Create();

private:
	void Apply();

	TArray<MeshApplyState> mApplys;
	TArray<MeshDrawCommand> mDraws;
	TArray<MeshDrawCommand> mIndexedDraws;
	TArray<FFlatVertex> mVertices;
	int mDepthFunc = 0;
};

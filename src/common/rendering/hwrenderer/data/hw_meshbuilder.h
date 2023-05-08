#pragma once

#include "hw_renderstate.h"
#include "hw_material.h"
#include "flatvertices.h"
#include <map>

class Mesh;

class MeshApplyState
{
public:
	struct ApplyData
	{
		FRenderStyle RenderStyle;
		int SpecialEffect;
		int TextureEnabled;
		float AlphaThreshold;
		int DepthFunc;
		int FogEnabled;
		int BrightmapEnabled;
		int TextureClamp;
		int TextureMode;
		int TextureModeFlags;
	};

	ApplyData applyData;
	StreamData streamData;
	FMaterialState material;
	VSMatrix textureMatrix;

	bool operator<(const MeshApplyState& other) const
	{
		if (material.mMaterial != other.material.mMaterial)
			return material.mMaterial < other.material.mMaterial;
		if (material.mClampMode != other.material.mClampMode)
			return material.mClampMode < other.material.mClampMode;
		if (material.mTranslation != other.material.mTranslation)
			return material.mTranslation < other.material.mTranslation;
		if (material.mOverrideShader != other.material.mOverrideShader)
			return material.mOverrideShader < other.material.mOverrideShader;

		int result = memcmp(&applyData, &other.applyData, sizeof(ApplyData));
		if (result != 0)
			return result < 0;

		result = memcmp(&textureMatrix, &other.textureMatrix, sizeof(VSMatrix));
		if (result != 0)
			return result < 0;

		result = memcmp(&streamData, &other.streamData, sizeof(StreamData));
		return result < 0;
	}
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
	void SetShadowData(const TArray<FFlatVertex>& vertices, const TArray<uint32_t>& indexes) override;
	void UpdateShadowData(unsigned int index, const FFlatVertex* vertices, unsigned int count) override { }
	void ResetVertices() override { }

	// Buffers
	int SetViewpoint(const HWViewpointUniforms& vp) override { return 0; }
	void SetViewpoint(int index) override { }
	void SetModelMatrix(const VSMatrix& matrix, const VSMatrix& normalMatrix) override { }
	void SetTextureMatrix(const VSMatrix& matrix) override { mTextureMatrix = matrix; }
	int UploadLights(const FDynLightData& lightdata) override { return -1; }
	int UploadBones(const TArray<VSMatrix>& bones) override { return -1; }

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
	void EnableLineSmooth(bool on) override { }
	void EnableDrawBuffers(int count, bool apply) override { }
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

	struct DrawLists
	{
		TArray<MeshDrawCommand> mDraws;
		TArray<MeshDrawCommand> mIndexedDraws;
	};
	std::map<MeshApplyState, DrawLists> mSortedLists;
	DrawLists* mDrawLists = nullptr;

	TArray<FFlatVertex> mVertices;
	TArray<uint32_t> mIndexes;
	int mDepthFunc = 0;

	VSMatrix mTextureMatrix = VSMatrix::identity();
};

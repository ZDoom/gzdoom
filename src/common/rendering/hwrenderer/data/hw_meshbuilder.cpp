
#include "hw_meshbuilder.h"
#include "hw_mesh.h"
#include "v_video.h"

MeshBuilder::MeshBuilder()
{
	Reset();

	// Initialize state same way as it begins in HWDrawInfo::RenderScene:
	SetTextureMode(TM_NORMAL);
	SetDepthMask(true);
	EnableFog(true);
	SetRenderStyle(STYLE_Source);
	SetDepthFunc(DF_Less);
	AlphaFunc(Alpha_GEqual, 0.f);
	ClearDepthBias();
	EnableTexture(1);
	EnableBrightmap(true);
}

void MeshBuilder::SetShadowData(const TArray<FFlatVertex>& vertices, const TArray<uint32_t>& indexes)
{
	mVertices = vertices;
	mIndexes = indexes;
}

void MeshBuilder::Draw(int dt, int index, int count, bool apply)
{
	if (apply)
		Apply();

	MeshDrawCommand command;
	command.DrawType = dt;
	command.Start = index;
	command.Count = count;
	command.ApplyIndex = -1;
	mDrawLists->mDraws.Push(command);
}

void MeshBuilder::DrawIndexed(int dt, int index, int count, bool apply)
{
	if (apply)
		Apply();

	MeshDrawCommand command;
	command.DrawType = dt;
	command.Start = index;
	command.Count = count;
	command.ApplyIndex = -1;
	mDrawLists->mIndexedDraws.Push(command);
}

void MeshBuilder::SetDepthFunc(int func)
{
	mDepthFunc = func;
}

void MeshBuilder::Apply()
{
	MeshApplyState state;

	state.applyData.RenderStyle = mRenderStyle;
	state.applyData.SpecialEffect = mSpecialEffect;
	state.applyData.TextureEnabled = mTextureEnabled;
	state.applyData.AlphaThreshold = mStreamData.uAlphaThreshold;
	state.applyData.DepthFunc = mDepthFunc;
	state.applyData.FogEnabled = mFogEnabled;
	state.applyData.BrightmapEnabled = mBrightmapEnabled;
	state.applyData.TextureClamp = mTextureClamp;
	state.applyData.TextureMode = mTextureMode;
	state.applyData.TextureModeFlags = mTextureModeFlags;
	state.streamData = mStreamData;
	state.material = mMaterial;
	state.textureMatrix = mTextureMatrix;

	state.streamData.uVertexNormal = FVector4(0.0f, 0.0f, 0.0f, 0.0f); // Grr, this should be part of the vertex!!

	mDrawLists = &mSortedLists[state];
}

std::unique_ptr<Mesh> MeshBuilder::Create()
{
	if (mSortedLists.empty())
		return {};

	auto mesh = std::make_unique<Mesh>();

	int applyIndex = 0;
	for (auto& it : mSortedLists)
	{
		mesh->mApplys.Push(it.first);
		for (MeshDrawCommand& command : it.second.mDraws)
		{
			command.ApplyIndex = applyIndex;
			mesh->mDraws.Push(command);
		}
		for (MeshDrawCommand& command : it.second.mIndexedDraws)
		{
			command.ApplyIndex = applyIndex;
			mesh->mIndexedDraws.Push(command);
		}
		applyIndex++;
	}

	// To do: the various mesh layers have to share the vertex buffer since some vertex allocations happen at the Process stage
	//mesh->mVertices = std::move(mVertices);
	//mVertices.Clear();
	mesh->mVertices = mVertices;

	return mesh;
}

std::pair<FFlatVertex*, unsigned int> MeshBuilder::AllocVertices(unsigned int count)
{
	unsigned int offset = mVertices.Reserve(count);
	return { &mVertices[offset], offset };
}

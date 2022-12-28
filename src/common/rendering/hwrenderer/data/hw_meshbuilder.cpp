
#include "hw_meshbuilder.h"
#include "hw_mesh.h"

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

void MeshBuilder::Draw(int dt, int index, int count, bool apply)
{
	if (apply)
		Apply();

	MeshDrawCommand command;
	command.DrawType = dt;
	command.Start = index;
	command.Count = count;
	command.ApplyIndex = mApplys.Size() - 1;
	mDraws.Push(command);
}

void MeshBuilder::DrawIndexed(int dt, int index, int count, bool apply)
{
	if (apply)
		Apply();

	MeshDrawCommand command;
	command.DrawType = dt;
	command.Start = index;
	command.Count = count;
	command.ApplyIndex = mApplys.Size() - 1;
	mIndexedDraws.Push(command);
}

void MeshBuilder::SetDepthFunc(int func)
{
	mDepthFunc = func;
}

void MeshBuilder::Apply()
{
	MeshApplyState state;

	state.RenderStyle = mRenderStyle;
	state.SpecialEffect = mSpecialEffect;
	state.TextureEnabled = mTextureEnabled;
	state.AlphaThreshold = mAlphaThreshold;
	state.DepthFunc = mDepthFunc;

	state.streamData = mStreamData;
	state.material = mMaterial;

	state.FogEnabled = mFogEnabled;
	state.BrightmapEnabled = mBrightmapEnabled;
	state.TextureClamp = mTextureClamp;
	state.TextureMode = mTextureMode;
	state.TextureModeFlags = mTextureModeFlags;

	state.uLightDist = mLightParms[0];
	state.uLightFactor = mLightParms[1];
	state.uFogDensity = mLightParms[2];
	state.uLightLevel = mLightParms[3];

	state.uClipSplit = { mClipSplit[0], mClipSplit[1] };

	mApplys.Push(state);
}

std::unique_ptr<Mesh> MeshBuilder::Create()
{
	if (mDraws.Size() == 0 && mIndexedDraws.Size() == 0)
		return {};

	auto mesh = std::make_unique<Mesh>();
	mesh->mApplys = std::move(mApplys);
	mesh->mDraws = std::move(mDraws);
	mesh->mIndexedDraws = std::move(mIndexedDraws);
	mesh->mVertices = std::move(mVertices);

	mApplys.Clear();
	mDraws.Clear();
	mIndexedDraws.Clear();
	mVertices.Clear();

	return mesh;
}

std::pair<FFlatVertex*, unsigned int> MeshBuilder::AllocVertices(unsigned int count)
{
	unsigned int offset = mVertices.Reserve(count);
	return { &mVertices[offset], offset };
}


#include "hw_meshbuilder.h"
#include "hw_mesh.h"

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

	state.uClipSplit = { mClipSplit[0], mClipSplit[1] };

	state.FogEnabled = mFogEnabled;
	state.BrightmapEnabled = mBrightmapEnabled;
	state.TextureClamp = mTextureClamp;
	state.TextureMode = mTextureMode;

	state.uLightDist = mLightParms[0];
	state.uLightFactor = mLightParms[1];
	state.uFogDensity = mLightParms[2];
	state.uLightLevel = mLightParms[3];

	mApplys.Push(state);
}

std::unique_ptr<Mesh> MeshBuilder::Create()
{
	auto mesh = std::make_unique<Mesh>();

	return mesh;
}

std::pair<FFlatVertex*, unsigned int> MeshBuilder::AllocVertices(unsigned int count)
{
	unsigned int offset = mVertices.Reserve(count);
	return { &mVertices[offset], offset };
}


#include "hw_mesh.h"
#include "v_video.h"

#define USE_MESH_VERTEX_BUFFER

void Mesh::Draw(FRenderState& renderstate)
{
#ifdef USE_MESH_VERTEX_BUFFER

	if (!mVertexBuffer && mVertices.Size() != 0)
	{
		mVertexBuffer.reset(screen->CreateVertexBuffer());
		mVertexBuffer->SetData(mVertices.Size() * sizeof(FFlatVertex), mVertices.Data(), BufferUsageType::Static);

		static const FVertexBufferAttribute format[] =
		{
			{ 0, VATTR_VERTEX, VFmt_Float3, (int)myoffsetof(FFlatVertex, x) },
			{ 0, VATTR_TEXCOORD, VFmt_Float2, (int)myoffsetof(FFlatVertex, u) },
			{ 0, VATTR_LIGHTMAP, VFmt_Float3, (int)myoffsetof(FFlatVertex, lu) },
		};
		mVertexBuffer->SetFormat(1, 3, sizeof(FFlatVertex), format);
	}

	if (mVertexBuffer)
		renderstate.SetVertexBuffer(mVertexBuffer.get(), 0, 0);

#else

	auto pair = renderstate.AllocVertices(mVertices.Size());
	memcpy(pair.first, mVertices.Data(), mVertices.Size() * sizeof(FFlatVertex));

#endif

	MeshApplyState origState;
	origState.applyData.RenderStyle = renderstate.mRenderStyle;
	origState.applyData.SpecialEffect = renderstate.mSpecialEffect;
	origState.applyData.TextureEnabled = renderstate.mTextureEnabled;
	origState.applyData.AlphaThreshold = renderstate.mAlphaThreshold;
	origState.applyData.FogEnabled = renderstate.mFogEnabled;
	origState.applyData.BrightmapEnabled = renderstate.mBrightmapEnabled;
	origState.applyData.TextureClamp = renderstate.mTextureClamp;
	origState.applyData.TextureMode = renderstate.mTextureMode;
	origState.applyData.TextureModeFlags = renderstate.mTextureModeFlags;
	origState.applyData.uLightDist = renderstate.mLightParms[0];
	origState.applyData.uLightFactor = renderstate.mLightParms[1];
	origState.applyData.uFogDensity = renderstate.mLightParms[2];
	origState.applyData.uClipSplit[0] = renderstate.mClipSplit[0];
	origState.applyData.uClipSplit[1] = renderstate.mClipSplit[1];
	origState.uLightLevel = renderstate.mLightParms[3];
	origState.streamData = renderstate.mStreamData;
	origState.material = renderstate.mMaterial;

	int applyIndex = -1;
	int depthFunc = -1;
	for (const MeshDrawCommand& cmd : mDraws)
	{
		bool apply = applyIndex != cmd.ApplyIndex;
		if (apply)
		{
			int newDepthFunc = mApplys[cmd.ApplyIndex].applyData.DepthFunc;
			if (depthFunc != newDepthFunc)
			{
				depthFunc = newDepthFunc;
				renderstate.SetDepthFunc(depthFunc);
			}

			applyIndex = cmd.ApplyIndex;
			Apply(renderstate, mApplys[cmd.ApplyIndex]);
		}

#ifdef USE_MESH_VERTEX_BUFFER
		renderstate.Draw(cmd.DrawType, cmd.Start, cmd.Count, apply);
#else
		renderstate.Draw(cmd.DrawType, pair.second + cmd.Start, cmd.Count, apply);
#endif
	}

#ifdef USE_MESH_VERTEX_BUFFER
	if (mVertexBuffer)
		renderstate.SetVertexBuffer(screen->mVertexData);
#endif

	for (const MeshDrawCommand& cmd : mIndexedDraws)
	{
		bool apply = applyIndex != cmd.ApplyIndex;
		if (apply)
		{
			int newDepthFunc = mApplys[cmd.ApplyIndex].applyData.DepthFunc;
			if (depthFunc != newDepthFunc)
			{
				depthFunc = newDepthFunc;
				renderstate.SetDepthFunc(depthFunc);
			}

			applyIndex = cmd.ApplyIndex;
			Apply(renderstate, mApplys[cmd.ApplyIndex]);
		}
		renderstate.DrawIndexed(cmd.DrawType, cmd.Start, cmd.Count, apply);
	}

	Apply(renderstate, origState);
}

void Mesh::Apply(FRenderState& renderstate, const MeshApplyState& state)
{
	renderstate.mRenderStyle = state.applyData.RenderStyle;
	renderstate.mSpecialEffect = state.applyData.SpecialEffect;
	renderstate.mTextureEnabled = state.applyData.TextureEnabled;
	renderstate.mAlphaThreshold = state.applyData.AlphaThreshold;
	renderstate.mFogEnabled = state.applyData.FogEnabled;
	renderstate.mBrightmapEnabled = state.applyData.BrightmapEnabled;
	renderstate.mTextureClamp = state.applyData.TextureClamp;
	renderstate.mTextureMode = state.applyData.TextureMode;
	renderstate.mTextureModeFlags = state.applyData.TextureModeFlags;
	renderstate.mLightParms[0] = state.applyData.uLightDist;
	renderstate.mLightParms[1] = state.applyData.uLightFactor;
	renderstate.mLightParms[2] = state.applyData.uFogDensity;
	renderstate.mClipSplit[0] = state.applyData.uClipSplit[0];
	renderstate.mClipSplit[1] = state.applyData.uClipSplit[1];
	renderstate.mLightParms[3] = state.uLightLevel;
	renderstate.mStreamData = state.streamData;
	renderstate.mMaterial = state.material;
}

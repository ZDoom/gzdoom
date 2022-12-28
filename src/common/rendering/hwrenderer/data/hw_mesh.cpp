
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
	origState.RenderStyle = renderstate.mRenderStyle;
	origState.SpecialEffect = renderstate.mSpecialEffect;
	origState.TextureEnabled = renderstate.mTextureEnabled;
	origState.AlphaThreshold = renderstate.mAlphaThreshold;
	origState.streamData = renderstate.mStreamData;
	origState.material = renderstate.mMaterial;
	origState.FogEnabled = renderstate.mFogEnabled;
	origState.BrightmapEnabled = renderstate.mBrightmapEnabled;
	origState.TextureClamp = renderstate.mTextureClamp;
	origState.TextureMode = renderstate.mTextureMode;
	origState.TextureModeFlags = renderstate.mTextureModeFlags;
	origState.uLightDist = renderstate.mLightParms[0];
	origState.uLightFactor = renderstate.mLightParms[1];
	origState.uFogDensity = renderstate.mLightParms[2];
	origState.uLightLevel = renderstate.mLightParms[3];
	origState.uClipSplit[0] = renderstate.mClipSplit[0];
	origState.uClipSplit[1] = renderstate.mClipSplit[1];

	int applyIndex = -1;
	int depthFunc = -1;
	for (const MeshDrawCommand& cmd : mDraws)
	{
		bool apply = applyIndex != cmd.ApplyIndex;
		if (apply)
		{
			int newDepthFunc = mApplys[cmd.ApplyIndex].DepthFunc;
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
			int newDepthFunc = mApplys[cmd.ApplyIndex].DepthFunc;
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
	renderstate.mRenderStyle = state.RenderStyle;
	renderstate.mSpecialEffect = state.SpecialEffect;
	renderstate.mTextureEnabled = state.TextureEnabled;
	renderstate.mAlphaThreshold = state.AlphaThreshold;
	renderstate.mStreamData = state.streamData;
	renderstate.mMaterial = state.material;
	renderstate.mFogEnabled = state.FogEnabled;
	renderstate.mBrightmapEnabled = state.BrightmapEnabled;
	renderstate.mTextureClamp = state.TextureClamp;
	renderstate.mTextureMode = state.TextureMode;
	renderstate.mTextureModeFlags = state.TextureModeFlags;
	renderstate.mLightParms[0] = state.uLightDist;
	renderstate.mLightParms[1] = state.uLightFactor;
	renderstate.mLightParms[2] = state.uFogDensity;
	renderstate.mLightParms[3] = state.uLightLevel;
	renderstate.mClipSplit[0] = state.uClipSplit[0];
	renderstate.mClipSplit[1] = state.uClipSplit[1];
}

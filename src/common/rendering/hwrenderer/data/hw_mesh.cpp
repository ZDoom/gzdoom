
#include "hw_mesh.h"
#include "v_video.h"
#include "cmdlib.h"

#define USE_MESH_VERTEX_BUFFER

void Mesh::Draw(FRenderState& renderstate)
{
#ifdef USE_MESH_VERTEX_BUFFER

	if (!mVertexBuffer && mVertices.Size() != 0)
	{
		static const FVertexBufferAttribute format[] =
		{
			{ 0, VATTR_VERTEX, VFmt_Float3, (int)myoffsetof(FFlatVertex, x) },
			{ 0, VATTR_TEXCOORD, VFmt_Float2, (int)myoffsetof(FFlatVertex, u) },
			{ 0, VATTR_LIGHTMAP, VFmt_Float3, (int)myoffsetof(FFlatVertex, lu) },
		};
		mVertexBuffer.reset(screen->CreateVertexBuffer(1, 3, sizeof(FFlatVertex), format));
		mVertexBuffer->SetData(mVertices.Size() * sizeof(FFlatVertex), mVertices.Data(), BufferUsageType::Static);
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
	origState.applyData.AlphaThreshold = renderstate.mStreamData.uAlphaThreshold;
	origState.applyData.FogEnabled = renderstate.mFogEnabled;
	origState.applyData.BrightmapEnabled = renderstate.mBrightmapEnabled;
	origState.applyData.TextureClamp = renderstate.mTextureClamp;
	origState.applyData.TextureMode = renderstate.mTextureMode;
	origState.applyData.TextureModeFlags = renderstate.mTextureModeFlags;
	origState.streamData = renderstate.mStreamData;
	origState.material = renderstate.mMaterial;
	origState.textureMatrix.loadIdentity();

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
		renderstate.SetFlatVertexBuffer();
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
	renderstate.mFogEnabled = state.applyData.FogEnabled;
	renderstate.mBrightmapEnabled = state.applyData.BrightmapEnabled;
	renderstate.mTextureClamp = state.applyData.TextureClamp;
	renderstate.mTextureMode = state.applyData.TextureMode;
	renderstate.mTextureModeFlags = state.applyData.TextureModeFlags;
	renderstate.mStreamData = state.streamData;
	renderstate.mMaterial = state.material;
	renderstate.SetTextureMatrix(state.textureMatrix);
}

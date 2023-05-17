/*
**  Vulkan backend
**  Copyright (c) 2016-2020 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include "vk_renderstate.h"
#include "vulkan/vk_renderdevice.h"
#include "vulkan/commands/vk_commandbuffer.h"
#include "vulkan/buffers/vk_buffer.h"
#include "vulkan/pipelines/vk_renderpass.h"
#include "vulkan/descriptorsets/vk_descriptorset.h"
#include "vulkan/textures/vk_renderbuffers.h"
#include "vulkan/textures/vk_hwtexture.h"
#include <zvulkan/vulkanbuilders.h>

#include "hw_skydome.h"
#include "hw_viewpointuniforms.h"
#include "hw_dynlightdata.h"
#include "hw_cvars.h"
#include "hw_clock.h"
#include "flatvertices.h"

CVAR(Int, vk_submit_size, 1000, 0);
EXTERN_CVAR(Bool, r_skipmats)

VkRenderState::VkRenderState(VulkanRenderDevice* fb, int threadIndex) : fb(fb), threadIndex(threadIndex), mRSBuffers(fb->GetBufferManager()->GetRSBuffers(threadIndex)), mStreamBufferWriter(mRSBuffers), mMatrixBufferWriter(mRSBuffers)
{
	mMatrices.ModelMatrix.loadIdentity();
	mMatrices.NormalModelMatrix.loadIdentity();
	mMatrices.TextureMatrix.loadIdentity();

	Reset();
}

void VkRenderState::ClearScreen()
{
	int width = fb->GetWidth();
	int height = fb->GetHeight();

	auto vertices = AllocVertices(4);
	FFlatVertex* v = vertices.first;
	v[0].Set(0, 0, 0, 0, 0);
	v[1].Set(0, (float)height, 0, 0, 1);
	v[2].Set((float)width, 0, 0, 1, 0);
	v[3].Set((float)width, (float)height, 0, 1, 1);

	Set2DViewpoint(width, height);
	SetColor(0, 0, 0);
	Apply(DT_TriangleStrip);

	mCommandBuffer->draw(4, 1, vertices.second, 0);
}

void VkRenderState::Draw(int dt, int index, int count, bool apply)
{
	if (apply || mNeedApply)
		Apply(dt);

	mCommandBuffer->draw(count, 1, index, 0);
}

void VkRenderState::DrawIndexed(int dt, int index, int count, bool apply)
{
	if (apply || mNeedApply)
		Apply(dt);

	mCommandBuffer->drawIndexed(count, 1, index, 0, 0);
}

bool VkRenderState::SetDepthClamp(bool on)
{
	bool lastValue = mDepthClamp;
	mDepthClamp = on;
	mNeedApply = true;
	return lastValue;
}

void VkRenderState::SetDepthMask(bool on)
{
	mDepthWrite = on;
	mNeedApply = true;
}

void VkRenderState::SetDepthFunc(int func)
{
	mDepthFunc = func;
	mNeedApply = true;
}

void VkRenderState::SetDepthRange(float min, float max)
{
	mViewportDepthMin = min;
	mViewportDepthMax = max;
	mViewportChanged = true;
	mNeedApply = true;
}

void VkRenderState::SetColorMask(bool r, bool g, bool b, bool a)
{
	int rr = r, gg = g, bb = b, aa = a;
	mColorMask = (aa << 3) | (bb << 2) | (gg << 1) | rr;
	mNeedApply = true;
}

void VkRenderState::SetStencil(int offs, int op, int flags)
{
	mStencilRef = screen->stencilValue + offs;
	mStencilRefChanged = true;
	mStencilOp = op;

	if (flags != -1)
	{
		bool cmon = !(flags & SF_ColorMaskOff);
		SetColorMask(cmon, cmon, cmon, cmon); // don't write to the graphics buffer
		mDepthWrite = !(flags & SF_DepthMaskOff);
	}

	mNeedApply = true;
}

void VkRenderState::SetCulling(int mode)
{
	mCullMode = mode;
	mNeedApply = true;
}

void VkRenderState::Clear(int targets)
{
	mClearTargets = targets;
	EndRenderPass();
}

void VkRenderState::EnableStencil(bool on)
{
	mStencilTest = on;
	mNeedApply = true;
}

void VkRenderState::SetScissor(int x, int y, int w, int h)
{
	mScissorX = x;
	mScissorY = y;
	mScissorWidth = w;
	mScissorHeight = h;
	mScissorChanged = true;
	mNeedApply = true;
}

void VkRenderState::SetViewport(int x, int y, int w, int h)
{
	mViewportX = x;
	mViewportY = y;
	mViewportWidth = w;
	mViewportHeight = h;
	mViewportChanged = true;
	mNeedApply = true;
}

void VkRenderState::EnableDepthTest(bool on)
{
	mDepthTest = on;
	mNeedApply = true;
}

void VkRenderState::EnableLineSmooth(bool on)
{
}

void VkRenderState::Apply(int dt)
{
	drawcalls.Clock();

	mApplyCount++;
	if (threadIndex == 0 && mApplyCount >= vk_submit_size)
	{
		fb->GetCommands()->FlushCommands(false);
		mApplyCount = 0;
	}

	ApplyStreamData();
	ApplyMatrices();
	ApplyRenderPass(dt);
	ApplyScissor();
	ApplyViewport();
	ApplyStencilRef();
	ApplyDepthBias();
	ApplyPushConstants();
	ApplyVertexBuffers();
	ApplyBufferSets();
	ApplyMaterial();
	mNeedApply = false;

	drawcalls.Unclock();
}

void VkRenderState::ApplyDepthBias()
{
	if (mBias.mChanged)
	{
		mCommandBuffer->setDepthBias(mBias.mUnits, 0.0f, mBias.mFactor);
		mBias.mChanged = false;
	}
}

void VkRenderState::ApplyRenderPass(int dt)
{
	// Find a pipeline that matches our state
	VkPipelineKey pipelineKey;
	pipelineKey.DrawType = dt;
	pipelineKey.VertexFormat = mVertexBuffer ? static_cast<VkHardwareVertexBuffer*>(mVertexBuffer)->VertexFormat : mRSBuffers->Flatbuffer.VertexFormat;
	pipelineKey.RenderStyle = mRenderStyle;
	pipelineKey.DepthTest = mDepthTest;
	pipelineKey.DepthWrite = mDepthTest && mDepthWrite;
	pipelineKey.DepthFunc = mDepthFunc;
	pipelineKey.DepthClamp = mDepthClamp;
	pipelineKey.DepthBias = !(mBias.mFactor == 0 && mBias.mUnits == 0);
	pipelineKey.StencilTest = mStencilTest;
	pipelineKey.StencilPassOp = mStencilOp;
	pipelineKey.ColorMask = mColorMask;
	pipelineKey.CullMode = mCullMode;
	pipelineKey.NumTextureLayers = mMaterial.mMaterial ? mMaterial.mMaterial->NumLayers() : 0;
	pipelineKey.NumTextureLayers = max(pipelineKey.NumTextureLayers, SHADER_MIN_REQUIRED_TEXTURE_LAYERS);// Always force minimum 8 textures as the shader requires it
	if (mSpecialEffect > EFF_NONE)
	{
		pipelineKey.ShaderKey.SpecialEffect = mSpecialEffect;
		pipelineKey.ShaderKey.EffectState = 0;
		pipelineKey.ShaderKey.AlphaTest = false;
	}
	else
	{
		int effectState = mMaterial.mOverrideShader >= 0 ? mMaterial.mOverrideShader : (mMaterial.mMaterial ? mMaterial.mMaterial->GetShaderIndex() : 0);
		pipelineKey.ShaderKey.SpecialEffect = EFF_NONE;
		pipelineKey.ShaderKey.EffectState = mTextureEnabled ? effectState : SHADER_NoTexture;
		if (r_skipmats && pipelineKey.ShaderKey.EffectState >= 3 && pipelineKey.ShaderKey.EffectState <= 4)
			pipelineKey.ShaderKey.EffectState = 0;
		pipelineKey.ShaderKey.AlphaTest = mStreamData.uAlphaThreshold >= 0.f;
	}

	int uTextureMode = GetTextureModeAndFlags((mMaterial.mMaterial && mMaterial.mMaterial->Source()->isHardwareCanvas()) ? TM_OPAQUE : TM_NORMAL);
	pipelineKey.ShaderKey.TextureMode = uTextureMode & 0xffff;
	pipelineKey.ShaderKey.ClampY = (uTextureMode & TEXF_ClampY) != 0;
	pipelineKey.ShaderKey.Brightmap = (uTextureMode & TEXF_Brightmap) != 0;
	pipelineKey.ShaderKey.Detailmap = (uTextureMode & TEXF_Detailmap) != 0;
	pipelineKey.ShaderKey.Glowmap = (uTextureMode & TEXF_Glowmap) != 0;

	// The way GZDoom handles state is just plain insanity!
	int fogset = 0;
	if (mFogEnabled)
	{
		if (mFogEnabled == 2)
		{
			fogset = -3;	// 2D rendering with 'foggy' overlay.
		}
		else if ((mFogColor & 0xffffff) == 0)
		{
			fogset = gl_fogmode;
		}
		else
		{
			fogset = -gl_fogmode;
		}
	}
	pipelineKey.ShaderKey.Simple2D = (fogset == -3);
	pipelineKey.ShaderKey.FogBeforeLights = (fogset > 0);
	pipelineKey.ShaderKey.FogAfterLights = (fogset < 0);
	pipelineKey.ShaderKey.FogRadial = (fogset < -1 || fogset > 1);
	pipelineKey.ShaderKey.SWLightRadial = (gl_fogmode == 2);
	pipelineKey.ShaderKey.SWLightBanded = false; // gl_bandedswlight;

	float lightlevel = mStreamData.uLightLevel;
	if (lightlevel < 0.0)
	{
		pipelineKey.ShaderKey.LightMode = 0; // Default
	}
	else
	{
		if (mLightMode == 5)
			pipelineKey.ShaderKey.LightMode = 3; // Build
		else if (mLightMode == 16)
			pipelineKey.ShaderKey.LightMode = 2; // Vanilla
		else
			pipelineKey.ShaderKey.LightMode = 1; // Software
	}

	pipelineKey.ShaderKey.UseShadowmap = gl_light_shadowmap;
	pipelineKey.ShaderKey.UseRaytrace = gl_light_raytrace;

	pipelineKey.ShaderKey.GBufferPass = mRenderTarget.DrawBuffers > 1;

	// Is this the one we already have?
	bool inRenderPass = mCommandBuffer;
	bool changingPipeline = (!inRenderPass) || (pipelineKey != mPipelineKey);

	if (!inRenderPass)
	{
		if (threadIndex == 0)
		{
			mCommandBuffer = fb->GetCommands()->GetDrawCommands();
		}
		else
		{
			mThreadCommandBuffer = fb->GetCommands()->BeginThreadCommands();
			mCommandBuffer = mThreadCommandBuffer.get();
		}

		mScissorChanged = true;
		mViewportChanged = true;
		mStencilRefChanged = true;
		mBias.mChanged = true;

		BeginRenderPass(mCommandBuffer);
	}

	if (changingPipeline)
	{
		mCommandBuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, mPassSetup->GetPipeline(pipelineKey));
		mPipelineKey = pipelineKey;
	}
}

void VkRenderState::ApplyStencilRef()
{
	if (mStencilRefChanged)
	{
		mCommandBuffer->setStencilReference(VK_STENCIL_FRONT_AND_BACK, mStencilRef);
		mStencilRefChanged = false;
	}
}

void VkRenderState::ApplyScissor()
{
	if (mScissorChanged)
	{
		VkRect2D scissor;
		if (mScissorWidth >= 0)
		{
			int x0 = clamp(mScissorX, 0, mRenderTarget.Width);
			int y0 = clamp(mScissorY, 0, mRenderTarget.Height);
			int x1 = clamp(mScissorX + mScissorWidth, 0, mRenderTarget.Width);
			int y1 = clamp(mScissorY + mScissorHeight, 0, mRenderTarget.Height);

			scissor.offset.x = x0;
			scissor.offset.y = y0;
			scissor.extent.width = x1 - x0;
			scissor.extent.height = y1 - y0;
		}
		else
		{
			scissor.offset.x = 0;
			scissor.offset.y = 0;
			scissor.extent.width = mRenderTarget.Width;
			scissor.extent.height = mRenderTarget.Height;
		}
		mCommandBuffer->setScissor(0, 1, &scissor);
		mScissorChanged = false;
	}
}

void VkRenderState::ApplyViewport()
{
	if (mViewportChanged)
	{
		VkViewport viewport;
		if (mViewportWidth >= 0)
		{
			viewport.x = (float)mViewportX;
			viewport.y = (float)mViewportY;
			viewport.width = (float)mViewportWidth;
			viewport.height = (float)mViewportHeight;
		}
		else
		{
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = (float)mRenderTarget.Width;
			viewport.height = (float)mRenderTarget.Height;
		}
		viewport.minDepth = mViewportDepthMin;
		viewport.maxDepth = mViewportDepthMax;
		mCommandBuffer->setViewport(0, 1, &viewport);
		mViewportChanged = false;
	}
}

void VkRenderState::ApplyStreamData()
{
	auto passManager = fb->GetRenderPassManager();

	mStreamData.useVertexData = mVertexBuffer ? passManager->GetVertexFormat(static_cast<VkHardwareVertexBuffer*>(mVertexBuffer)->VertexFormat)->UseVertexData : 0;

	if (mMaterial.mMaterial && mMaterial.mMaterial->Source())
		mStreamData.timer = static_cast<float>((double)(screen->FrameTime - firstFrame) * (double)mMaterial.mMaterial->Source()->GetShaderSpeed() / 1000.);
	else
		mStreamData.timer = 0.0f;

	if (mMaterial.mMaterial)
	{
		auto source = mMaterial.mMaterial->Source();
		mStreamData.uSpecularMaterial = { source->GetGlossiness(), source->GetSpecularLevel() };
	}

	if (!mStreamBufferWriter.Write(mStreamData))
	{
		WaitForStreamBuffers();
		mStreamBufferWriter.Write(mStreamData);
	}
}

void VkRenderState::ApplyPushConstants()
{
	mPushConstants.uDataIndex = mStreamBufferWriter.DataIndex();
	mPushConstants.uLightIndex = mLightIndex >= 0 ? (mLightIndex % MAX_LIGHT_DATA) : -1;
	mPushConstants.uBoneIndexBase = mBoneIndexBase;

	mCommandBuffer->pushConstants(GetPipelineLayout(mPipelineKey.NumTextureLayers), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, (uint32_t)sizeof(PushConstants), &mPushConstants);
}

void VkRenderState::ApplyMatrices()
{
	if (mMatricesChanged)
	{
		if (!mMatrixBufferWriter.Write(mMatrices))
		{
			WaitForStreamBuffers();
			mMatrixBufferWriter.Write(mMatrices);
		}
		mMatricesChanged = false;
	}
}

void VkRenderState::ApplyVertexBuffers()
{
	if ((mVertexBuffer != mLastVertexBuffer || mVertexOffsets[0] != mLastVertexOffsets[0] || mVertexOffsets[1] != mLastVertexOffsets[1]))
	{
		if (mVertexBuffer)
		{
			auto vkbuf = static_cast<VkHardwareVertexBuffer*>(mVertexBuffer);
			const VkVertexFormat* format = fb->GetRenderPassManager()->GetVertexFormat(vkbuf->VertexFormat);
			VkBuffer vertexBuffers[2] = { vkbuf->mBuffer->buffer, vkbuf->mBuffer->buffer };
			VkDeviceSize offsets[] = { mVertexOffsets[0] * format->Stride, mVertexOffsets[1] * format->Stride };
			mCommandBuffer->bindVertexBuffers(0, 2, vertexBuffers, offsets);
		}
		else
		{
			const VkVertexFormat* format = fb->GetRenderPassManager()->GetVertexFormat(mRSBuffers->Flatbuffer.VertexFormat);
			VkBuffer vertexBuffers[2] = { mRSBuffers->Flatbuffer.VertexBuffer->buffer, mRSBuffers->Flatbuffer.VertexBuffer->buffer };
			VkDeviceSize offsets[] = { mVertexOffsets[0] * format->Stride, mVertexOffsets[1] * format->Stride };
			mCommandBuffer->bindVertexBuffers(0, 2, vertexBuffers, offsets);
		}

		mLastVertexBuffer = mVertexBuffer;
		mLastVertexOffsets[0] = mVertexOffsets[0];
		mLastVertexOffsets[1] = mVertexOffsets[1];
	}

	if (mIndexBuffer != mLastIndexBuffer || mIndexBufferNeedsBind)
	{
		if (mIndexBuffer)
		{
			mCommandBuffer->bindIndexBuffer(static_cast<VkHardwareIndexBuffer*>(mIndexBuffer)->mBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
		}
		else
		{
			mCommandBuffer->bindIndexBuffer(mRSBuffers->Flatbuffer.IndexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
		}
		mLastIndexBuffer = mIndexBuffer;
		mIndexBufferNeedsBind = false;
	}
}

void VkRenderState::ApplyMaterial()
{
	if (mMaterial.mChanged)
	{
		auto descriptors = fb->GetDescriptorSetManager();
		VulkanPipelineLayout* layout = GetPipelineLayout(mPipelineKey.NumTextureLayers);

		if (mMaterial.mMaterial && mMaterial.mMaterial->Source()->isHardwareCanvas())
			static_cast<FCanvasTexture*>(mMaterial.mMaterial->Source()->GetTexture())->NeedUpdate();

		VulkanDescriptorSet* descriptorset = mMaterial.mMaterial ? static_cast<VkMaterial*>(mMaterial.mMaterial)->GetDescriptorSet(threadIndex, mMaterial) : descriptors->GetNullTextureDescriptorSet();

		mCommandBuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, descriptors->GetFixedDescriptorSet());
		mCommandBuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 2, descriptorset);
		mMaterial.mChanged = false;
	}
}

void VkRenderState::ApplyBufferSets()
{
	uint32_t matrixOffset = mMatrixBufferWriter.Offset();
	uint32_t streamDataOffset = mStreamBufferWriter.StreamDataOffset();
	uint32_t lightsOffset = mLightIndex >= 0 ? (uint32_t)(mLightIndex / MAX_LIGHT_DATA) * sizeof(LightBufferUBO) : mLastLightsOffset;
	if (mViewpointOffset != mLastViewpointOffset || matrixOffset != mLastMatricesOffset || streamDataOffset != mLastStreamDataOffset || lightsOffset != mLastLightsOffset)
	{
		auto descriptors = fb->GetDescriptorSetManager();
		VulkanPipelineLayout* layout = GetPipelineLayout(mPipelineKey.NumTextureLayers);

		uint32_t offsets[4] = { mViewpointOffset, matrixOffset, streamDataOffset, lightsOffset };
		mCommandBuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, descriptors->GetFixedDescriptorSet());
		mCommandBuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, descriptors->GetRSBufferDescriptorSet(threadIndex), 4, offsets);

		mLastViewpointOffset = mViewpointOffset;
		mLastMatricesOffset = matrixOffset;
		mLastStreamDataOffset = streamDataOffset;
		mLastLightsOffset = lightsOffset;
	}
}

void VkRenderState::WaitForStreamBuffers()
{
	fb->WaitForCommands(false);
	mApplyCount = 0;
	mStreamBufferWriter.Reset();
	mMatrixBufferWriter.Reset();
}

int VkRenderState::SetViewpoint(const HWViewpointUniforms& vp)
{
	if (mRSBuffers->Viewpoint.Count == mRSBuffers->Viewpoint.UploadIndex)
	{
		return mRSBuffers->Viewpoint.Count - 1;
	}
	memcpy(((char*)mRSBuffers->Viewpoint.Data) + mRSBuffers->Viewpoint.UploadIndex * mRSBuffers->Viewpoint.BlockAlign, &vp, sizeof(HWViewpointUniforms));
	int index = mRSBuffers->Viewpoint.UploadIndex++;
	mViewpointOffset = index * mRSBuffers->Viewpoint.BlockAlign;
	mNeedApply = true;
	return index;
}

void VkRenderState::SetViewpoint(int index)
{
	mViewpointOffset = index * mRSBuffers->Viewpoint.BlockAlign;
	mNeedApply = true;
}

void VkRenderState::SetModelMatrix(const VSMatrix& matrix, const VSMatrix& normalMatrix)
{
	mMatrices.ModelMatrix = matrix;
	mMatrices.NormalModelMatrix = normalMatrix;
	mMatricesChanged = true;
	mNeedApply = true;
}

void VkRenderState::SetTextureMatrix(const VSMatrix& matrix)
{
	mMatrices.TextureMatrix = matrix;
	mMatricesChanged = true;
	mNeedApply = true;
}

int VkRenderState::UploadLights(const FDynLightData& data)
{
	// All meaasurements here are in vec4's.
	int size0 = data.arrays[0].Size() / 4;
	int size1 = data.arrays[1].Size() / 4;
	int size2 = data.arrays[2].Size() / 4;
	int totalsize = size0 + size1 + size2 + 1;

	// Clamp lights so they aren't bigger than what fits into a single dynamic uniform buffer page
	if (totalsize > MAX_LIGHT_DATA)
	{
		int diff = totalsize - MAX_LIGHT_DATA;

		size2 -= diff;
		if (size2 < 0)
		{
			size1 += size2;
			size2 = 0;
		}
		if (size1 < 0)
		{
			size0 += size1;
			size1 = 0;
		}
		totalsize = size0 + size1 + size2 + 1;
	}

	// Check if we still have any lights
	if (totalsize <= 1)
		return -1;

	// Make sure the light list doesn't cross a page boundary
	if (mRSBuffers->Lightbuffer.UploadIndex % MAX_LIGHT_DATA + totalsize > MAX_LIGHT_DATA)
		mRSBuffers->Lightbuffer.UploadIndex = (mRSBuffers->Lightbuffer.UploadIndex / MAX_LIGHT_DATA + 1) * MAX_LIGHT_DATA;

	int thisindex = mRSBuffers->Lightbuffer.UploadIndex;
	if (thisindex + totalsize <= mRSBuffers->Lightbuffer.Count)
	{
		mRSBuffers->Lightbuffer.UploadIndex += totalsize;

		float parmcnt[] = { 0, float(size0), float(size0 + size1), float(size0 + size1 + size2) };

		float* copyptr = (float*)mRSBuffers->Lightbuffer.Data + thisindex * 4;
		memcpy(&copyptr[0], parmcnt, sizeof(FVector4));
		memcpy(&copyptr[4], &data.arrays[0][0], size0 * sizeof(FVector4));
		memcpy(&copyptr[4 + 4 * size0], &data.arrays[1][0], size1 * sizeof(FVector4));
		memcpy(&copyptr[4 + 4 * (size0 + size1)], &data.arrays[2][0], size2 * sizeof(FVector4));
		return thisindex;
	}
	else
	{
		return -1;	// Buffer is full. Since it is being used live at the point of the upload we cannot do much here but to abort.
	}
}

int VkRenderState::UploadBones(const TArray<VSMatrix>& bones)
{
	int totalsize = bones.Size();
	if (bones.Size() == 0)
	{
		return -1;
	}

	int thisindex = mRSBuffers->Bonebuffer.UploadIndex;
	mRSBuffers->Bonebuffer.UploadIndex += totalsize;

	if (thisindex + totalsize <= mRSBuffers->Bonebuffer.Count)
	{
		memcpy((VSMatrix*)mRSBuffers->Bonebuffer.Data + thisindex, bones.Data(), bones.Size() * sizeof(VSMatrix));
		return thisindex;
	}
	else
	{
		return -1;	// Buffer is full. Since it is being used live at the point of the upload we cannot do much here but to abort.
	}
}

std::pair<FFlatVertex*, unsigned int> VkRenderState::AllocVertices(unsigned int count)
{
	unsigned int index = mRSBuffers->Flatbuffer.CurIndex;
	if (index + count >= mRSBuffers->Flatbuffer.BUFFER_SIZE_TO_USE)
	{
		// If a single scene needs 2'000'000 vertices there must be something very wrong. 
		I_FatalError("Out of vertex memory. Tried to allocate more than %u vertices for a single frame", index + count);
	}
	mRSBuffers->Flatbuffer.CurIndex += count;
	return std::make_pair(mRSBuffers->Flatbuffer.Vertices + index, index);
}

void VkRenderState::SetShadowData(const TArray<FFlatVertex>& vertices, const TArray<uint32_t>& indexes)
{
	auto commands = fb->GetCommands();

	UpdateShadowData(0, vertices.Data(), vertices.Size());
	mRSBuffers->Flatbuffer.ShadowDataSize = vertices.Size();
	mRSBuffers->Flatbuffer.CurIndex = mRSBuffers->Flatbuffer.ShadowDataSize;

	if (indexes.Size() > 0)
	{
		size_t bufsize = indexes.Size() * sizeof(uint32_t);

		auto buffer = BufferBuilder()
			.Usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY)
			.Size(bufsize)
			.DebugName("Flatbuffer.IndexBuffer")
			.Create(fb->GetDevice());

		auto staging = BufferBuilder()
			.Usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY)
			.Size(bufsize)
			.DebugName("Flatbuffer.IndexBuffer.Staging")
			.Create(fb->GetDevice());

		void* dst = staging->Map(0, bufsize);
		memcpy(dst, indexes.Data(), bufsize);
		staging->Unmap();

		commands->GetTransferCommands()->copyBuffer(staging.get(), buffer.get());
		commands->TransferDeleteList->Add(std::move(staging));

		commands->DrawDeleteList->Add(std::move(mRSBuffers->Flatbuffer.IndexBuffer));
		mRSBuffers->Flatbuffer.IndexBuffer = std::move(buffer);

		mIndexBufferNeedsBind = true;
		mNeedApply = true;
	}
}

void VkRenderState::UpdateShadowData(unsigned int index, const FFlatVertex* vertices, unsigned int count)
{
	memcpy(mRSBuffers->Flatbuffer.Vertices + index, vertices, count * sizeof(FFlatVertex));
}

void VkRenderState::ResetVertices()
{
	mRSBuffers->Flatbuffer.CurIndex = mRSBuffers->Flatbuffer.ShadowDataSize;
}

void VkRenderState::BeginFrame()
{
	mMaterial.Reset();
	mApplyCount = 0;

	mRSBuffers->Viewpoint.UploadIndex = 0;
	mRSBuffers->Lightbuffer.UploadIndex = 0;
	mRSBuffers->Bonebuffer.UploadIndex = 0;
}

void VkRenderState::EndRenderPass()
{
	if (mCommandBuffer)
	{
		mCommandBuffer->endRenderPass();
		mCommandBuffer = nullptr;
	}

	if (mThreadCommandBuffer)
	{
		fb->GetCommands()->EndThreadCommands(std::move(mThreadCommandBuffer));
	}

	// Force rebind of everything on next draw
	mPipelineKey = {};
	mLastViewpointOffset = 0xffffffff;
	mLastVertexOffsets[0] = 0xffffffff;
	mIndexBufferNeedsBind = true;
}

void VkRenderState::EndFrame()
{
	mMatrixBufferWriter.Reset();
	mStreamBufferWriter.Reset();
}

void VkRenderState::EnableDrawBuffers(int count, bool apply)
{
	if (mRenderTarget.DrawBuffers != count)
	{
		EndRenderPass();
		mRenderTarget.DrawBuffers = count;
	}
}

void VkRenderState::SetRenderTarget(VkTextureImage *image, VulkanImageView *depthStencilView, int width, int height, VkFormat format, VkSampleCountFlagBits samples)
{
	EndRenderPass();

	mRenderTarget.Image = image;
	mRenderTarget.DepthStencil = depthStencilView;
	mRenderTarget.Width = width;
	mRenderTarget.Height = height;
	mRenderTarget.Format = format;
	mRenderTarget.Samples = samples;
}

void VkRenderState::BeginRenderPass(VulkanCommandBuffer *cmdbuffer)
{
	VkRenderPassKey key = {};
	key.DrawBufferFormat = mRenderTarget.Format;
	key.Samples = mRenderTarget.Samples;
	key.DrawBuffers = mRenderTarget.DrawBuffers;
	key.DepthStencil = !!mRenderTarget.DepthStencil;

	mPassSetup = GetRenderPass(key);

	auto &framebuffer = mRenderTarget.Image->RSFramebuffers[key];
	if (!framebuffer)
	{
		auto buffers = fb->GetBuffers();
		FramebufferBuilder builder;
		builder.RenderPass(mPassSetup->GetRenderPass(0));
		builder.Size(mRenderTarget.Width, mRenderTarget.Height);
		builder.AddAttachment(mRenderTarget.Image->View.get());
		if (key.DrawBuffers > 1)
			builder.AddAttachment(buffers->SceneFog.View.get());
		if (key.DrawBuffers > 2)
			builder.AddAttachment(buffers->SceneNormal.View.get());
		if (key.DepthStencil)
			builder.AddAttachment(mRenderTarget.DepthStencil);
		builder.DebugName("VkRenderPassSetup.Framebuffer");
		framebuffer = builder.Create(fb->GetDevice());
	}

	// Only clear depth+stencil if the render target actually has that
	if (!mRenderTarget.DepthStencil)
		mClearTargets &= ~(CT_Depth | CT_Stencil);

	RenderPassBegin beginInfo;
	beginInfo.RenderPass(mPassSetup->GetRenderPass(mClearTargets));
	beginInfo.RenderArea(0, 0, mRenderTarget.Width, mRenderTarget.Height);
	beginInfo.Framebuffer(framebuffer.get());
	beginInfo.AddClearColor(screen->mSceneClearColor[0], screen->mSceneClearColor[1], screen->mSceneClearColor[2], screen->mSceneClearColor[3]);
	if (key.DrawBuffers > 1)
		beginInfo.AddClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	if (key.DrawBuffers > 2)
		beginInfo.AddClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	beginInfo.AddClearDepthStencil(1.0f, 0);
	beginInfo.Execute(cmdbuffer);

	mMaterial.mChanged = true;
	mClearTargets = 0;
}

VkThreadRenderPassSetup* VkRenderState::GetRenderPass(const VkRenderPassKey& key)
{
	auto& item = mRenderPassSetups[key];
	if (!item)
		item.reset(new VkThreadRenderPassSetup(fb, key));
	return item.get();
}

VulkanPipelineLayout* VkRenderState::GetPipelineLayout(int numLayers)
{
	if (mPipelineLayouts.size() <= (size_t)numLayers)
		mPipelineLayouts.resize(numLayers + 1);

	auto& layout = mPipelineLayouts[numLayers];
	if (layout)
		return layout;

	std::unique_lock<std::mutex> lock(fb->ThreadMutex);
	layout = fb->GetRenderPassManager()->GetPipelineLayout(numLayers);
	return layout;
}

/////////////////////////////////////////////////////////////////////////////

VkThreadRenderPassSetup::VkThreadRenderPassSetup(VulkanRenderDevice* fb, const VkRenderPassKey& key) : PassKey(key), fb(fb)
{
}

VulkanRenderPass* VkThreadRenderPassSetup::GetRenderPass(int clearTargets)
{
	if (RenderPasses[clearTargets])
		return RenderPasses[clearTargets];

	std::unique_lock<std::mutex> lock(fb->ThreadMutex);
	RenderPasses[clearTargets] = fb->GetRenderPassManager()->GetRenderPass(PassKey)->GetRenderPass(clearTargets);
	return RenderPasses[clearTargets];
}

VulkanPipeline* VkThreadRenderPassSetup::GetPipeline(const VkPipelineKey& key)
{
	auto& item = Pipelines[key];
	if (item)
		return item;

	std::unique_lock<std::mutex> lock(fb->ThreadMutex);
	item = fb->GetRenderPassManager()->GetRenderPass(PassKey)->GetPipeline(key);
	return item;
}

/////////////////////////////////////////////////////////////////////////////

void VkRenderStateMolten::Draw(int dt, int index, int count, bool apply)
{
	if (dt == DT_TriangleFan)
	{
		IBuffer* oldIndexBuffer = mIndexBuffer;
		mIndexBuffer = fb->GetBufferManager()->FanToTrisIndexBuffer.get();

		if (apply || mNeedApply)
			Apply(DT_Triangles);
		else
			ApplyVertexBuffers();

		mCommandBuffer->drawIndexed((count - 2) * 3, 1, 0, index, 0);

		mIndexBuffer = oldIndexBuffer;
	}
	else
	{
		if (apply || mNeedApply)
			Apply(dt);

		mCommandBuffer->draw(count, 1, index, 0);
	}
}

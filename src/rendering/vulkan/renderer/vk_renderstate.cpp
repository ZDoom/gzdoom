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
#include "vulkan/system/vk_framebuffer.h"
#include "vulkan/system/vk_builders.h"
#include "vulkan/renderer/vk_renderpass.h"
#include "vulkan/renderer/vk_renderbuffers.h"
#include "vulkan/textures/vk_hwtexture.h"
#include "templates.h"
#include "hw_skydome.h"
#include "hw_viewpointuniforms.h"
#include "hw_lightbuffer.h"
#include "hw_cvars.h"
#include "hw_clock.h"
#include "flatvertices.h"
#include "hwrenderer/data/hw_viewpointbuffer.h"
#include "hwrenderer/data/shaderuniforms.h"

CVAR(Int, vk_submit_size, 1000, 0);

VkRenderState::VkRenderState()
{
	Reset();
}

void VkRenderState::ClearScreen()
{
	screen->mViewpoints->Set2D(*this, SCREENWIDTH, SCREENHEIGHT);
	SetColor(0, 0, 0);
	Apply(DT_TriangleStrip);
	mCommandBuffer->draw(4, 1, FFlatVertexBuffer::FULLSCREEN_INDEX, 0);
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

void VkRenderState::EnableClipDistance(int num, bool state)
{
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

void VkRenderState::EnableMultisampling(bool on)
{
}

void VkRenderState::EnableLineSmooth(bool on)
{
}

void VkRenderState::Apply(int dt)
{
	drawcalls.Clock();

	mApplyCount++;
	if (mApplyCount >= vk_submit_size)
	{
		GetVulkanFrameBuffer()->FlushCommands(false);
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
	ApplyDynamicSet();
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
	pipelineKey.VertexFormat = static_cast<VKVertexBuffer*>(mVertexBuffer)->VertexFormat;
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
	pipelineKey.NumTextureLayers = std::max(pipelineKey.NumTextureLayers, SHADER_MIN_REQUIRED_TEXTURE_LAYERS);// Always force minimum 8 textures as the shader requires it
	if (mSpecialEffect > EFF_NONE)
	{
		pipelineKey.SpecialEffect = mSpecialEffect;
		pipelineKey.EffectState = 0;
		pipelineKey.AlphaTest = false;
	}
	else
	{
		int effectState = mMaterial.mOverrideShader >= 0 ? mMaterial.mOverrideShader : (mMaterial.mMaterial ? mMaterial.mMaterial->GetShaderIndex() : 0);
		pipelineKey.SpecialEffect = EFF_NONE;
		pipelineKey.EffectState = mTextureEnabled ? effectState : SHADER_NoTexture;
		pipelineKey.AlphaTest = mAlphaThreshold >= 0.f;
	}

	// Is this the one we already have?
	bool inRenderPass = mCommandBuffer;
	bool changingPipeline = (!inRenderPass) || (pipelineKey != mPipelineKey);

	if (!inRenderPass)
	{
		mCommandBuffer = GetVulkanFrameBuffer()->GetDrawCommands();
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
	auto fb = GetVulkanFrameBuffer();
	auto passManager = fb->GetRenderPassManager();

	mStreamData.useVertexData = passManager->GetVertexFormat(static_cast<VKVertexBuffer*>(mVertexBuffer)->VertexFormat)->UseVertexData;

	if (mMaterial.mMaterial && mMaterial.mMaterial->Source())
		mStreamData.timer = static_cast<float>((double)(screen->FrameTime - firstFrame) * (double)mMaterial.mMaterial->Source()->GetShaderSpeed() / 1000.);
	else
		mStreamData.timer = 0.0f;

	if (!mStreamBufferWriter.Write(mStreamData))
	{
		WaitForStreamBuffers();
		mStreamBufferWriter.Write(mStreamData);
	}
}

void VkRenderState::ApplyPushConstants()
{
	int fogset = 0;
	if (mFogEnabled)
	{
		if (mFogEnabled == 2)
		{
			fogset = -3;	// 2D rendering with 'foggy' overlay.
		}
		else if ((GetFogColor() & 0xffffff) == 0)
		{
			fogset = gl_fogmode;
		}
		else
		{
			fogset = -gl_fogmode;
		}
	}

	int tempTM = TM_NORMAL;
	if (mMaterial.mMaterial && mMaterial.mMaterial->Source()->isHardwareCanvas())
		tempTM = TM_OPAQUE;

	mPushConstants.uFogEnabled = fogset;
	int f = mTextureModeFlags;
	if (!mBrightmapEnabled) f &= ~(TEXF_Brightmap|TEXF_Glowmap);
	mPushConstants.uTextureMode = (mTextureMode == TM_NORMAL && tempTM == TM_OPAQUE ? TM_OPAQUE : mTextureMode) | f;
	mPushConstants.uLightDist = mLightParms[0];
	mPushConstants.uLightFactor = mLightParms[1];
	mPushConstants.uFogDensity = mLightParms[2];
	mPushConstants.uLightLevel = mLightParms[3];
	mPushConstants.uAlphaThreshold = mAlphaThreshold;
	mPushConstants.uClipSplit = { mClipSplit[0], mClipSplit[1] };

	if (mMaterial.mMaterial)
	{
		auto source = mMaterial.mMaterial->Source();
		mPushConstants.uSpecularMaterial = { source->GetGlossiness(), source->GetSpecularLevel() };
	}

	mPushConstants.uLightIndex = mLightIndex;
	mPushConstants.uDataIndex = mStreamBufferWriter.DataIndex();

	auto fb = GetVulkanFrameBuffer();
	auto passManager = fb->GetRenderPassManager();
	mCommandBuffer->pushConstants(passManager->GetPipelineLayout(mPipelineKey.NumTextureLayers), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, (uint32_t)sizeof(PushConstants), &mPushConstants);
}

void VkRenderState::ApplyMatrices()
{
	if (!mMatrixBufferWriter.Write(mModelMatrix, mModelMatrixEnabled, mTextureMatrix, mTextureMatrixEnabled))
	{
		WaitForStreamBuffers();
		mMatrixBufferWriter.Write(mModelMatrix, mModelMatrixEnabled, mTextureMatrix, mTextureMatrixEnabled);
	}
}

void VkRenderState::ApplyVertexBuffers()
{
	if ((mVertexBuffer != mLastVertexBuffer || mVertexOffsets[0] != mLastVertexOffsets[0] || mVertexOffsets[1] != mLastVertexOffsets[1]) && mVertexBuffer)
	{
		auto vkbuf = static_cast<VKVertexBuffer*>(mVertexBuffer);
		const VkVertexFormat *format = GetVulkanFrameBuffer()->GetRenderPassManager()->GetVertexFormat(vkbuf->VertexFormat);
		VkBuffer vertexBuffers[2] = { vkbuf->mBuffer->buffer, vkbuf->mBuffer->buffer };
		VkDeviceSize offsets[] = { mVertexOffsets[0] * format->Stride, mVertexOffsets[1] * format->Stride };
		mCommandBuffer->bindVertexBuffers(0, 2, vertexBuffers, offsets);
		mLastVertexBuffer = mVertexBuffer;
		mLastVertexOffsets[0] = mVertexOffsets[0];
		mLastVertexOffsets[1] = mVertexOffsets[1];
	}

	if (mIndexBuffer != mLastIndexBuffer && mIndexBuffer)
	{
		mCommandBuffer->bindIndexBuffer(static_cast<VKIndexBuffer*>(mIndexBuffer)->mBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
		mLastIndexBuffer = mIndexBuffer;
	}
}

void VkRenderState::ApplyMaterial()
{
	if (mMaterial.mChanged)
	{
		auto fb = GetVulkanFrameBuffer();
		auto passManager = fb->GetRenderPassManager();

		if (mMaterial.mMaterial && mMaterial.mMaterial->Source()->isHardwareCanvas()) static_cast<FCanvasTexture*>(mMaterial.mMaterial->Source()->GetTexture())->NeedUpdate();

		VulkanDescriptorSet* descriptorset = mMaterial.mMaterial ? static_cast<VkMaterial*>(mMaterial.mMaterial)->GetDescriptorSet(mMaterial) : passManager->GetNullTextureDescriptorSet();

		mCommandBuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, passManager->GetPipelineLayout(mPipelineKey.NumTextureLayers), 1, descriptorset);
		mMaterial.mChanged = false;
	}
}

void VkRenderState::ApplyDynamicSet()
{
	auto fb = GetVulkanFrameBuffer();
	uint32_t matrixOffset = mMatrixBufferWriter.Offset();
	uint32_t streamDataOffset = mStreamBufferWriter.StreamDataOffset();
	if (mViewpointOffset != mLastViewpointOffset || matrixOffset != mLastMatricesOffset || streamDataOffset != mLastStreamDataOffset)
	{
		auto passManager = fb->GetRenderPassManager();

		uint32_t offsets[3] = { mViewpointOffset, matrixOffset, streamDataOffset };
		mCommandBuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, passManager->GetPipelineLayout(mPipelineKey.NumTextureLayers), 0, passManager->DynamicSet.get(), 3, offsets);

		mLastViewpointOffset = mViewpointOffset;
		mLastMatricesOffset = matrixOffset;
		mLastStreamDataOffset = streamDataOffset;
	}
}

void VkRenderState::WaitForStreamBuffers()
{
	GetVulkanFrameBuffer()->WaitForCommands(false);
	mApplyCount = 0;
	mStreamBufferWriter.Reset();
	mMatrixBufferWriter.Reset();
}

void VkRenderState::Bind(int bindingpoint, uint32_t offset)
{
	if (bindingpoint == VIEWPOINT_BINDINGPOINT)
	{
		mViewpointOffset = offset;
		mNeedApply = true;
	}
}

void VkRenderState::BeginFrame()
{
	mMaterial.Reset();
	mApplyCount = 0;
}

void VkRenderState::EndRenderPass()
{
	if (mCommandBuffer)
	{
		mCommandBuffer->endRenderPass();
		mCommandBuffer = nullptr;
		mPipelineKey = {};

		mLastViewpointOffset = 0xffffffff;
		mLastVertexBuffer = nullptr;
		mLastIndexBuffer = nullptr;
		mLastModelMatrixEnabled = true;
		mLastTextureMatrixEnabled = true;
	}
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
	auto fb = GetVulkanFrameBuffer();

	VkRenderPassKey key = {};
	key.DrawBufferFormat = mRenderTarget.Format;
	key.Samples = mRenderTarget.Samples;
	key.DrawBuffers = mRenderTarget.DrawBuffers;
	key.DepthStencil = !!mRenderTarget.DepthStencil;

	mPassSetup = fb->GetRenderPassManager()->GetRenderPass(key);

	auto &framebuffer = mRenderTarget.Image->RSFramebuffers[key];
	if (!framebuffer)
	{
		auto buffers = fb->GetBuffers();
		FramebufferBuilder builder;
		builder.setRenderPass(mPassSetup->GetRenderPass(0));
		builder.setSize(mRenderTarget.Width, mRenderTarget.Height);
		builder.addAttachment(mRenderTarget.Image->View.get());
		if (key.DrawBuffers > 1)
			builder.addAttachment(buffers->SceneFog.View.get());
		if (key.DrawBuffers > 2)
			builder.addAttachment(buffers->SceneNormal.View.get());
		if (key.DepthStencil)
			builder.addAttachment(mRenderTarget.DepthStencil);
		framebuffer = builder.create(GetVulkanFrameBuffer()->device);
		framebuffer->SetDebugName("VkRenderPassSetup.Framebuffer");
	}

	// Only clear depth+stencil if the render target actually has that
	if (!mRenderTarget.DepthStencil)
		mClearTargets &= ~(CT_Depth | CT_Stencil);

	RenderPassBegin beginInfo;
	beginInfo.setRenderPass(mPassSetup->GetRenderPass(mClearTargets));
	beginInfo.setRenderArea(0, 0, mRenderTarget.Width, mRenderTarget.Height);
	beginInfo.setFramebuffer(framebuffer.get());
	beginInfo.addClearColor(screen->mSceneClearColor[0], screen->mSceneClearColor[1], screen->mSceneClearColor[2], screen->mSceneClearColor[3]);
	if (key.DrawBuffers > 1)
		beginInfo.addClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	if (key.DrawBuffers > 2)
		beginInfo.addClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	beginInfo.addClearDepthStencil(1.0f, 0);
	cmdbuffer->beginRenderPass(beginInfo);

	mMaterial.mChanged = true;
	mClearTargets = 0;
}

/////////////////////////////////////////////////////////////////////////////

void VkRenderStateMolten::Draw(int dt, int index, int count, bool apply)
{
	if (dt == DT_TriangleFan)
	{
		IIndexBuffer *oldIndexBuffer = mIndexBuffer;
		mIndexBuffer = GetVulkanFrameBuffer()->FanToTrisIndexBuffer.get();

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

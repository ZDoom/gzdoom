
#include "vk_renderstate.h"
#include "vulkan/system/vk_framebuffer.h"
#include "vulkan/system/vk_builders.h"
#include "vulkan/renderer/vk_renderpass.h"
#include "vulkan/textures/vk_hwtexture.h"
#include "templates.h"
#include "doomstat.h"
#include "r_data/colormaps.h"
#include "hwrenderer/scene/hw_skydome.h"
#include "hwrenderer/scene/hw_viewpointuniforms.h"
#include "hwrenderer/dynlights/hw_lightbuffer.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "hwrenderer/utility/hw_clock.h"
#include "hwrenderer/data/flatvertices.h"
#include "hwrenderer/data/hw_viewpointbuffer.h"
#include "hwrenderer/data/shaderuniforms.h"

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
	if (apply)
		Apply(dt);
	else if (mDescriptorsChanged)
		BindDescriptorSets();

	drawcalls.Clock();
	mCommandBuffer->draw(count, 1, index, 0);
	drawcalls.Unclock();
}

void VkRenderState::DrawIndexed(int dt, int index, int count, bool apply)
{
	if (apply)
		Apply(dt);
	else if (mDescriptorsChanged)
		BindDescriptorSets();

	drawcalls.Clock();
	mCommandBuffer->drawIndexed(count, 1, index, 0, 0);
	drawcalls.Unclock();
}

bool VkRenderState::SetDepthClamp(bool on)
{
	bool lastValue = mLastDepthClamp;
	mLastDepthClamp = on;
	return lastValue;
}

void VkRenderState::SetDepthMask(bool on)
{
	mDepthWrite = on;
}

void VkRenderState::SetDepthFunc(int func)
{
}

void VkRenderState::SetDepthRange(float min, float max)
{
	mViewportDepthMin = min;
	mViewportDepthMax = max;
	mViewportChanged = true;
}

void VkRenderState::SetColorMask(bool r, bool g, bool b, bool a)
{
}

void VkRenderState::EnableDrawBufferAttachments(bool on)
{
}

void VkRenderState::SetStencil(int offs, int op, int flags)
{
}

void VkRenderState::SetCulling(int mode)
{
}

void VkRenderState::EnableClipDistance(int num, bool state)
{
}

void VkRenderState::Clear(int targets)
{
	// We need an active render pass, and it must have a depth attachment..
	bool lastDepthTest = mDepthTest;
	bool lastDepthWrite = mDepthWrite;
	if (targets & (CT_Depth | CT_Stencil))
	{
		mDepthTest = true;
		mDepthWrite = true;
	}
	Apply(DT_TriangleFan);
	mDepthTest = lastDepthTest;
	mDepthWrite = lastDepthWrite;

	VkClearAttachment attachments[2] = { };
	VkClearRect rects[2] = { };

	for (int i = 0; i < 2; i++)
	{
		rects[i].layerCount = 1;
		if (mScissorWidth >= 0)
		{
			rects[0].rect.offset.x = mScissorX;
			rects[0].rect.offset.y = mScissorY;
			rects[0].rect.extent.width = mScissorWidth;
			rects[0].rect.extent.height = mScissorHeight;
		}
		else
		{
			rects[0].rect.offset.x = 0;
			rects[0].rect.offset.y = 0;
			rects[0].rect.extent.width = SCREENWIDTH;
			rects[0].rect.extent.height = SCREENHEIGHT;
		}
	}

	if (targets & CT_Depth)
	{
		attachments[1].aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
		attachments[1].clearValue.depthStencil.depth = 1.0f;
	}
	if (targets & CT_Stencil)
	{
		attachments[1].aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		attachments[1].clearValue.depthStencil.stencil = 0;
	}
	if (targets & CT_Color)
	{
		attachments[0].aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
		for (int i = 0; i < 4; i++)
			attachments[0].clearValue.color.float32[i] = screen->mSceneClearColor[i];
	}

	if ((targets & CT_Color) && (targets & CT_Stencil) && (targets & CT_Depth))
		mCommandBuffer->clearAttachments(2, attachments, 2, rects);
	else if (targets & (CT_Stencil | CT_Depth))
		mCommandBuffer->clearAttachments(1, attachments + 1, 1, rects + 1);
	else if (targets & CT_Color)
		mCommandBuffer->clearAttachments(1, attachments, 1, rects);
}

void VkRenderState::EnableStencil(bool on)
{
}

void VkRenderState::SetScissor(int x, int y, int w, int h)
{
	mScissorX = x;
	mScissorY = y;
	mScissorWidth = w;
	mScissorHeight = h;
	mScissorChanged = true;
}

void VkRenderState::SetViewport(int x, int y, int w, int h)
{
	mViewportX = x;
	mViewportY = y;
	mViewportWidth = w;
	mViewportHeight = h;
	mViewportChanged = true;
}

void VkRenderState::EnableDepthTest(bool on)
{
	mDepthTest = on;
}

void VkRenderState::EnableMultisampling(bool on)
{
}

void VkRenderState::EnableLineSmooth(bool on)
{
}

template<typename T>
static void CopyToBuffer(uint32_t &offset, const T &data, VKDataBuffer *buffer)
{
	if (offset + (UniformBufferAlignment<T>() << 1) < buffer->Size())
	{
		offset += UniformBufferAlignment<T>();
		memcpy(static_cast<uint8_t*>(buffer->Memory()) + offset, &data, sizeof(T));
	}
}

void VkRenderState::Apply(int dt)
{
	auto fb = GetVulkanFrameBuffer();
	auto passManager = fb->GetRenderPassManager();

	// Find a render pass that matches our state
	VkRenderPassKey passKey;
	passKey.DrawType = dt;
	passKey.VertexFormat = static_cast<VKVertexBuffer*>(mVertexBuffer)->VertexFormat;
	passKey.RenderStyle = mRenderStyle;
	passKey.DepthTest = mDepthTest;
	passKey.DepthWrite = mDepthTest && mDepthWrite;
	if (mSpecialEffect > EFF_NONE)
	{
		passKey.SpecialEffect = mSpecialEffect;
		passKey.EffectState = 0;
		passKey.AlphaTest = false;
	}
	else
	{
		int effectState = mMaterial.mOverrideShader >= 0 ? mMaterial.mOverrideShader : (mMaterial.mMaterial ? mMaterial.mMaterial->GetShaderIndex() : 0);
		passKey.SpecialEffect = EFF_NONE;
		passKey.EffectState = mTextureEnabled ? effectState : SHADER_NoTexture;
		passKey.AlphaTest = mAlphaThreshold >= 0.f;
	}
	VkRenderPassSetup *passSetup = passManager->GetRenderPass(passKey);

	// Is this the one we already have or do we need to change render pass?
	bool changingRenderPass = (passSetup != mRenderPassSetup);

	if (!mCommandBuffer)
	{
		mCommandBuffer = fb->GetDrawCommands();
		changingRenderPass = true;
		mScissorChanged = true;
		mViewportChanged = true;
	}
	else if (changingRenderPass)
	{
		mCommandBuffer->endRenderPass();
	}

	if (changingRenderPass)
	{
		RenderPassBegin beginInfo;
		beginInfo.setRenderPass(passSetup->RenderPass.get());
		beginInfo.setRenderArea(0, 0, SCREENWIDTH, SCREENHEIGHT);
		beginInfo.setFramebuffer(passSetup->Framebuffer.get());
		mCommandBuffer->beginRenderPass(beginInfo);
		mCommandBuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, passSetup->Pipeline.get());
		mRenderPassSetup = passSetup;
	}

	if (mScissorChanged)
	{
		VkRect2D scissor;
		if (mScissorWidth >= 0)
		{
			scissor.offset.x = mScissorX;
			scissor.offset.y = mScissorY;
			scissor.extent.width = mScissorWidth;
			scissor.extent.height = mScissorHeight;
		}
		else
		{
			scissor.offset.x = 0;
			scissor.offset.y = 0;
			scissor.extent.width = SCREENWIDTH;
			scissor.extent.height = SCREENHEIGHT;
		}
		mCommandBuffer->setScissor(0, 1, &scissor);
		mScissorChanged = false;
	}

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
			viewport.width = (float)SCREENWIDTH;
			viewport.height = (float)SCREENHEIGHT;
		}
		viewport.minDepth = mViewportDepthMin;
		viewport.maxDepth = mViewportDepthMax;
		mCommandBuffer->setViewport(0, 1, &viewport);
		mViewportChanged = false;
	}

	const float normScale = 1.0f / 255.0f;

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

	mColors.uDesaturationFactor = mDesaturation * normScale;
	mColors.uFogColor = { mFogColor.r * normScale, mFogColor.g * normScale, mFogColor.b * normScale, mFogColor.a * normScale };
	mColors.uAddColor = { mAddColor.r * normScale, mAddColor.g * normScale, mAddColor.b * normScale, mAddColor.a * normScale };
	mColors.uObjectColor = { mObjectColor.r * normScale, mObjectColor.g * normScale, mObjectColor.b * normScale, mObjectColor.a * normScale };
	mColors.uDynLightColor = mDynColor.vec;
	mColors.uInterpolationFactor = mInterpolationFactor;

	mColors.useVertexData = passManager->VertexFormats[static_cast<VKVertexBuffer*>(mVertexBuffer)->VertexFormat].UseVertexData;
	mColors.uVertexColor = mColor.vec;
	mColors.uVertexNormal = mNormal.vec;

	mColors.timer = 0.0f; // static_cast<float>((double)(screen->FrameTime - firstFrame) * (double)mShaderTimer / 1000.);

	int tempTM = TM_NORMAL;
	if (mMaterial.mMaterial && mMaterial.mMaterial->tex->isHardwareCanvas())
		tempTM = TM_OPAQUE;

	mPushConstants.uFogEnabled = fogset;
	mPushConstants.uTextureMode = mTextureMode == TM_NORMAL && tempTM == TM_OPAQUE ? TM_OPAQUE : mTextureMode;
	mPushConstants.uLightDist = mLightParms[0];
	mPushConstants.uLightFactor = mLightParms[1];
	mPushConstants.uFogDensity = mLightParms[2];
	mPushConstants.uLightLevel = mLightParms[3];
	mPushConstants.uAlphaThreshold = mAlphaThreshold;
	mPushConstants.uClipSplit = { mClipSplit[0], mClipSplit[1] };

	/*if (mMaterial.mMaterial)
	{
		mPushConstants.uSpecularMaterial = { mMaterial.mMaterial->tex->Glossiness, mMaterial.mMaterial->tex->SpecularLevel };
	}*/

	if (mGlowEnabled)
	{
		mGlowingWalls.uGlowTopPlane = mGlowTopPlane.vec;
		mGlowingWalls.uGlowTopColor = mGlowTop.vec;
		mGlowingWalls.uGlowBottomPlane = mGlowBottomPlane.vec;
		mGlowingWalls.uGlowBottomColor = mGlowBottom.vec;
	}
	else
	{
		mGlowingWalls.uGlowTopColor = { 0.0f, 0.0f, 0.0f, 0.0f };
		mGlowingWalls.uGlowBottomColor = { 0.0f, 0.0f, 0.0f, 0.0f };
	}

	if (mGradientEnabled)
	{
		mColors.uObjectColor2 = { mObjectColor2.r * normScale, mObjectColor2.g * normScale, mObjectColor2.b * normScale, mObjectColor2.a * normScale };
		mGlowingWalls.uGradientTopPlane = mGradientTopPlane.vec;
		mGlowingWalls.uGradientBottomPlane = mGradientBottomPlane.vec;
	}
	else
	{
		mColors.uObjectColor2 = { 0.0f, 0.0f, 0.0f, 0.0f };
	}

	if (mSplitEnabled)
	{
		mGlowingWalls.uSplitTopPlane = mSplitTopPlane.vec;
		mGlowingWalls.uSplitBottomPlane = mSplitBottomPlane.vec;
	}
	else
	{
		mGlowingWalls.uSplitTopPlane = { 0.0f, 0.0f, 0.0f, 0.0f };
		mGlowingWalls.uSplitBottomPlane = { 0.0f, 0.0f, 0.0f, 0.0f };
	}

	if (mTextureMatrixEnabled)
	{
		mMatrices.TextureMatrix = mTextureMatrix;
	}
	else
	{
		mMatrices.TextureMatrix.loadIdentity();
	}

	if (mModelMatrixEnabled)
	{
		mMatrices.ModelMatrix = mModelMatrix;
		mMatrices.NormalModelMatrix.computeNormalMatrix(mModelMatrix);
	}
	else
	{
		mMatrices.ModelMatrix.loadIdentity();
		mMatrices.NormalModelMatrix.loadIdentity();
	}

	mPushConstants.uLightIndex = screen->mLights->BindUBO(mLightIndex);

	CopyToBuffer(mMatricesOffset, mMatrices, fb->MatricesUBO);
	CopyToBuffer(mColorsOffset, mColors, fb->ColorsUBO);
	CopyToBuffer(mGlowingWallsOffset, mGlowingWalls, fb->GlowingWallsUBO);

	mCommandBuffer->pushConstants(passManager->PipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, (uint32_t)sizeof(PushConstants), &mPushConstants);

	VkBuffer vertexBuffers[] = { static_cast<VKVertexBuffer*>(mVertexBuffer)->mBuffer->buffer };
	VkDeviceSize offsets[] = { 0 };
	mCommandBuffer->bindVertexBuffers(0, 1, vertexBuffers, offsets);

	if (mIndexBuffer)
		mCommandBuffer->bindIndexBuffer(static_cast<VKIndexBuffer*>(mIndexBuffer)->mBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);

	BindDescriptorSets();

	//if (mMaterial.mChanged)
	if (mMaterial.mMaterial)
	{
		auto base = static_cast<VkHardwareTexture*>(mMaterial.mMaterial->GetLayer(0, mMaterial.mTranslation));
		if (base)
			mCommandBuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, passManager->PipelineLayout.get(), 1, base->GetDescriptorSet(mMaterial));

		mMaterial.mChanged = false;
	}
}

void VkRenderState::Bind(int bindingpoint, uint32_t offset)
{
	if (bindingpoint == VIEWPOINT_BINDINGPOINT)
	{
		mViewpointOffset = offset;
	}
	else if (bindingpoint == LIGHTBUF_BINDINGPOINT)
	{
		mLightBufferOffset = offset;
	}

	mDescriptorsChanged = true;
}

void VkRenderState::BindDescriptorSets()
{
	auto fb = GetVulkanFrameBuffer();
	auto passManager = fb->GetRenderPassManager();

	uint32_t offsets[5] = { mViewpointOffset, mLightBufferOffset, mMatricesOffset, mColorsOffset, mGlowingWallsOffset };
	mCommandBuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, passManager->PipelineLayout.get(), 0, passManager->DynamicSet.get(), 5, offsets);
	mDescriptorsChanged = false;
}

void VkRenderState::EndRenderPass()
{
	if (mCommandBuffer)
	{
		mCommandBuffer->endRenderPass();
		mCommandBuffer = nullptr;
		mRenderPassSetup = nullptr;

		// To do: move this elsewhere or rename this function to make it clear this can only happen at the end of a frame
		mMatricesOffset = 0;
		mColorsOffset = 0;
		mGlowingWallsOffset = 0;
	}
}

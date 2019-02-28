
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

	/*
	glDisable(GL_MULTISAMPLE);
	glDisable(GL_DEPTH_TEST);
	mCommandBuffer->draw(4, 1, FFlatVertexBuffer::FULLSCREEN_INDEX, 0);
	glEnable(GL_DEPTH_TEST);
	*/
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
	if (mMaterial.mMaterial)
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
}

void VkRenderState::SetDepthFunc(int func)
{
}

void VkRenderState::SetDepthRange(float min, float max)
{
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
}

void VkRenderState::EnableStencil(bool on)
{
}

void VkRenderState::SetScissor(int x, int y, int w, int h)
{
}

void VkRenderState::SetViewport(int x, int y, int w, int h)
{
}

void VkRenderState::EnableDepthTest(bool on)
{
}

void VkRenderState::EnableMultisampling(bool on)
{
}

void VkRenderState::EnableLineSmooth(bool on)
{
}

void VkRenderState::Apply(int dt)
{
	auto fb = GetVulkanFrameBuffer();
	auto passManager = fb->GetRenderPassManager();
	auto passSetup = passManager->RenderPassSetup.get();

	bool changingRenderPass = false; // To do: decide if the state matches current bound renderpass

	if (!mCommandBuffer)
	{
		mCommandBuffer = fb->GetDrawCommands();
		changingRenderPass = true;
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
		beginInfo.addClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		beginInfo.addClearDepthStencil(1.0f, 0);
		mCommandBuffer->beginRenderPass(beginInfo);
		mCommandBuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, passSetup->Pipeline.get());
	}

	const float normScale = 1.0f / 255.0f;

	//glVertexAttrib4fv(VATTR_COLOR, mColor.vec);
	//glVertexAttrib4fv(VATTR_NORMAL, mNormal.vec);

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

	//activeShader->muTimer.Set((double)(screen->FrameTime - firstFrame) * (double)mShaderTimer / 1000.);

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

	mMatricesOffset += UniformBufferAlignment<MatricesUBO>();
	mColorsOffset += UniformBufferAlignment<ColorsUBO>();
	mGlowingWallsOffset += UniformBufferAlignment<GlowingWallsUBO>();

	memcpy(static_cast<uint8_t*>(fb->MatricesUBO->Memory()) + mMatricesOffset, &mMatrices, sizeof(MatricesUBO));
	memcpy(static_cast<uint8_t*>(fb->ColorsUBO->Memory()) + mColorsOffset, &mColors, sizeof(ColorsUBO));
	memcpy(static_cast<uint8_t*>(fb->GlowingWallsUBO->Memory()) + mGlowingWallsOffset, &mGlowingWalls, sizeof(GlowingWallsUBO));

	mCommandBuffer->pushConstants(passManager->PipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, (uint32_t)sizeof(PushConstants), &mPushConstants);

	VkBuffer vertexBuffers[] = { static_cast<VKVertexBuffer*>(mVertexBuffer)->mBuffer->buffer };
	VkDeviceSize offsets[] = { 0 };
	mCommandBuffer->bindVertexBuffers(0, 1, vertexBuffers, offsets);

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

		// To do: move this elsewhere or rename this function to make it clear this can only happen at the end of a frame
		mMatricesOffset = 0;
		mColorsOffset = 0;
		mGlowingWallsOffset = 0;
	}
}

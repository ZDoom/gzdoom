// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2010-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//

#include "volk/volk.h"

#include "v_video.h"
#include "m_png.h"
#include "templates.h"
#include "r_videoscale.h"
#include "actor.h"

#include "hwrenderer/utility/hw_clock.h"
#include "hwrenderer/utility/hw_vrmodes.h"
#include "hwrenderer/models/hw_models.h"
#include "hwrenderer/scene/hw_skydome.h"
#include "hwrenderer/data/hw_viewpointbuffer.h"
#include "hwrenderer/data/flatvertices.h"
#include "hwrenderer/data/shaderuniforms.h"
#include "hwrenderer/dynlights/hw_lightbuffer.h"

#include "swrenderer/r_swscene.h"

#include "vk_framebuffer.h"
#include "vk_buffers.h"
#include "vulkan/renderer/vk_renderstate.h"
#include "vulkan/renderer/vk_renderpass.h"
#include "vulkan/shaders/vk_shader.h"
#include "vulkan/textures/vk_samplers.h"
#include "vulkan/textures/vk_hwtexture.h"
#include "vulkan/system/vk_builders.h"
#include "vulkan/system/vk_swapchain.h"
#include "doomerrors.h"

void Draw2D(F2DDrawer *drawer, FRenderState &state);

EXTERN_CVAR(Bool, vid_vsync)
EXTERN_CVAR(Bool, r_drawvoxels)
EXTERN_CVAR(Int, gl_tonemap)

VulkanFrameBuffer::VulkanFrameBuffer(void *hMonitor, bool fullscreen, VulkanDevice *dev) : 
	Super(hMonitor, fullscreen) 
{
	device = dev;
	SetViewportRects(nullptr);
	InitPalette();
}

VulkanFrameBuffer::~VulkanFrameBuffer()
{
	delete MatricesUBO;
	delete ColorsUBO;
	delete GlowingWallsUBO;
	delete mVertexData;
	delete mSkyData;
	delete mViewpoints;
	delete mLights;
	for (auto tex : AllTextures)
		tex->Reset();
}

void VulkanFrameBuffer::InitializeState()
{
	gl_vendorstring = "Vulkan";
	hwcaps = RFL_SHADER_STORAGE_BUFFER | RFL_BUFFER_STORAGE;

	mUploadSemaphore.reset(new VulkanSemaphore(device));
	mGraphicsCommandPool.reset(new VulkanCommandPool(device, device->graphicsFamily));

	mVertexData = new FFlatVertexBuffer(GetWidth(), GetHeight());
	mSkyData = new FSkyVertexBuffer;
	mViewpoints = new GLViewpointBuffer;
	mLights = new FLightBuffer();

	// To do: move this to HW renderer interface maybe?
	MatricesUBO = (VKDataBuffer*)CreateDataBuffer(1234, false);
	ColorsUBO = (VKDataBuffer*)CreateDataBuffer(1234, false);
	GlowingWallsUBO = (VKDataBuffer*)CreateDataBuffer(1234, false);
	MatricesUBO->SetData(sizeof(MatricesUBO) * 128, nullptr, false);
	ColorsUBO->SetData(sizeof(ColorsUBO) * 128, nullptr, false);
	GlowingWallsUBO->SetData(sizeof(GlowingWallsUBO) * 128, nullptr, false);

	mShaderManager.reset(new VkShaderManager(device));
	mSamplerManager.reset(new VkSamplerManager(device));
	mRenderPassManager.reset(new VkRenderPassManager());
	mRenderState.reset(new VkRenderState());
}

void VulkanFrameBuffer::Update()
{
	twoD.Reset();
	Flush3D.Reset();

	Flush3D.Clock();

	int newWidth = GetClientWidth();
	int newHeight = GetClientHeight();
	if (lastSwapWidth != newWidth || lastSwapHeight != newHeight)
	{
		device->windowResized();
		lastSwapWidth = newWidth;
		lastSwapHeight = newHeight;
	}

	device->beginFrame();

	Draw2D();
	Clear2D();

	mRenderState->EndRenderPass();

	//DrawPresentTexture(mOutputLetterbox, true);
	{
		auto sceneColor = mRenderPassManager->SceneColor.get();

		PipelineBarrier barrier0;
		barrier0.addImage(sceneColor, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
		barrier0.execute(GetDrawCommands(), VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		VkImageBlit blit = {};
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { sceneColor->width, sceneColor->height, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = 0;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { (int32_t)device->swapChain->actualExtent.width, (int32_t)device->swapChain->actualExtent.height, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = 0;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;
		GetDrawCommands()->blitImage(
			sceneColor->image, VK_IMAGE_LAYOUT_GENERAL,
			device->swapChain->swapChainImages[device->presentImageIndex], VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR,
			1, &blit, VK_FILTER_NEAREST);
	}

	mDrawCommands->end();

	if (mUploadCommands)
	{
		mUploadCommands->end();

		// Submit upload commands immediately
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &mUploadCommands->buffer;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &mUploadSemaphore->semaphore;
		VkResult result = vkQueueSubmit(device->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		if (result != VK_SUCCESS)
			I_FatalError("Failed to submit command buffer!\n");

		// Wait for upload commands to finish, then submit render commands
		VkSemaphore waitSemaphores[] = { mUploadSemaphore->semaphore, device->imageAvailableSemaphore->semaphore };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 2;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &mDrawCommands->buffer;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &device->renderFinishedSemaphore->semaphore;
		result = vkQueueSubmit(device->graphicsQueue, 1, &submitInfo, device->renderFinishedFence->fence);
		if (result != VK_SUCCESS)
			I_FatalError("Failed to submit command buffer!\n");
	}
	else
	{
		VkSemaphore waitSemaphores[] = { device->imageAvailableSemaphore->semaphore };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &mDrawCommands->buffer;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &device->renderFinishedSemaphore->semaphore;
		VkResult result = vkQueueSubmit(device->graphicsQueue, 1, &submitInfo, device->renderFinishedFence->fence);
		if (result != VK_SUCCESS)
			I_FatalError("Failed to submit command buffer!\n");
	}

	Flush3D.Unclock();

	Finish.Reset();
	Finish.Clock();
	device->presentFrame();
	device->waitPresent();

	mDrawCommands.reset();
	mUploadCommands.reset();
	mFrameDeleteList.clear();

	Finish.Unclock();

	Super::Update();
}

void VulkanFrameBuffer::WriteSavePic(player_t *player, FileWriter *file, int width, int height)
{
	if (!V_IsHardwareRenderer())
		Super::WriteSavePic(player, file, width, height);
}

sector_t *VulkanFrameBuffer::RenderView(player_t *player)
{
	mRenderState->SetVertexBuffer(screen->mVertexData);
	screen->mVertexData->Reset();

	if (!V_IsHardwareRenderer())
	{
		if (!swdrawer) swdrawer.reset(new SWSceneDrawer);
		return swdrawer->RenderView(player);
	}
	else
	{
		return nullptr;
	}
}

uint32_t VulkanFrameBuffer::GetCaps()
{
	if (!V_IsHardwareRenderer())
		return Super::GetCaps();

	// describe our basic feature set
	ActorRenderFeatureFlags FlagSet = RFF_FLATSPRITES | RFF_MODELS | RFF_SLOPE3DFLOORS |
		RFF_TILTPITCH | RFF_ROLLSPRITES | RFF_POLYGONAL | RFF_MATSHADER | RFF_POSTSHADER | RFF_BRIGHTMAP;
	if (r_drawvoxels)
		FlagSet |= RFF_VOXELS;

	if (gl_tonemap != 5) // not running palette tonemap shader
		FlagSet |= RFF_TRUECOLOR;

	return (uint32_t)FlagSet;
}

void VulkanFrameBuffer::SetVSync(bool vsync)
{
}

void VulkanFrameBuffer::CleanForRestart()
{
	// force recreation of the SW scene drawer to ensure it gets a new set of resources.
	swdrawer.reset();
}

IHardwareTexture *VulkanFrameBuffer::CreateHardwareTexture()
{
	auto texture = new VkHardwareTexture();
	AllTextures.Push(texture);
	return texture;
}

FModelRenderer *VulkanFrameBuffer::CreateModelRenderer(int mli) 
{
	I_FatalError("VulkanFrameBuffer::CreateModelRenderer not implemented\n");
	return nullptr;
}

IShaderProgram *VulkanFrameBuffer::CreateShaderProgram()
{
	I_FatalError("VulkanFrameBuffer::CreateShaderProgram not implemented\n");
	return nullptr;
}

IVertexBuffer *VulkanFrameBuffer::CreateVertexBuffer()
{
	return new VKVertexBuffer();
}

IIndexBuffer *VulkanFrameBuffer::CreateIndexBuffer()
{
	return new VKIndexBuffer();
}

IDataBuffer *VulkanFrameBuffer::CreateDataBuffer(int bindingpoint, bool ssbo)
{
	auto buffer = new VKDataBuffer(bindingpoint, ssbo);
	if (bindingpoint == VIEWPOINT_BINDINGPOINT)
	{
		ViewpointUBO = buffer;
	}
	else if (bindingpoint == LIGHTBUF_BINDINGPOINT)
	{
		LightBufferSSO = buffer;
	}
	return buffer;
}

void VulkanFrameBuffer::UnbindTexUnit(int no)
{
}

void VulkanFrameBuffer::TextureFilterChanged()
{
}

void VulkanFrameBuffer::BlurScene(float amount)
{
}

void VulkanFrameBuffer::UpdatePalette()
{
}

void VulkanFrameBuffer::BeginFrame()
{
	mRenderPassManager->BeginFrame();
}

void VulkanFrameBuffer::Draw2D()
{
	::Draw2D(&m2DDrawer, *mRenderState);
}

VulkanCommandBuffer *VulkanFrameBuffer::GetUploadCommands()
{
	if (!mUploadCommands)
	{
		mUploadCommands = mGraphicsCommandPool->createBuffer();
		mUploadCommands->begin();
	}
	return mUploadCommands.get();
}

VulkanCommandBuffer *VulkanFrameBuffer::GetDrawCommands()
{
	if (!mDrawCommands)
	{
		mDrawCommands = mGraphicsCommandPool->createBuffer();
		mDrawCommands->begin();
	}
	return mDrawCommands.get();
}

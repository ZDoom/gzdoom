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
#include "hwrenderer/dynlights/hw_lightbuffer.h"

#include "vk_framebuffer.h"
#include "vk_buffers.h"
#include "vulkan/renderer/vk_renderstate.h"
#include "vulkan/shaders/vk_shader.h"
#include "vulkan/textures/vk_samplers.h"
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
}

VulkanFrameBuffer::~VulkanFrameBuffer()
{
}

void VulkanFrameBuffer::InitializeState()
{
	mGraphicsCommandPool.reset(new VulkanCommandPool(device, device->graphicsFamily));

	mVertexData = new FFlatVertexBuffer(GetWidth(), GetHeight());
	mSkyData = new FSkyVertexBuffer;
	mViewpoints = new GLViewpointBuffer;
	mLights = new FLightBuffer();

	mShaderManager.reset(new VkShaderManager(device));
	mSamplerManager.reset(new VkSamplerManager(device));
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

	mPresentCommands = mGraphicsCommandPool->createBuffer();
	mPresentCommands->begin();

	Draw2D();
	Clear2D();
	//DrawPresentTexture(mOutputLetterbox, true);

	mPresentCommands->end();

	VkSemaphore waitSemaphores[] = { device->imageAvailableSemaphore };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &mPresentCommands->buffer;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &device->renderFinishedSemaphore;
	VkResult result = vkQueueSubmit(device->graphicsQueue, 1, &submitInfo, device->renderFinishedFence);
	if (result != VK_SUCCESS)
		I_FatalError("Failed to submit command buffer!\n");

	Flush3D.Unclock();

	Finish.Reset();
	Finish.Clock();
	device->presentFrame();
	device->waitPresent();

	mPresentCommands.reset();
	mUploadCommands.reset();

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
	return nullptr;
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
	return new VKDataBuffer(bindingpoint, ssbo);
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
	return mPresentCommands.get();
}

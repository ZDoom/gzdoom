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
#include "i_time.h"
#include "g_game.h"
#include "gamedata/fonts/v_text.h"

#include "hwrenderer/utility/hw_clock.h"
#include "hwrenderer/utility/hw_vrmodes.h"
#include "hwrenderer/models/hw_models.h"
#include "hwrenderer/scene/hw_skydome.h"
#include "hwrenderer/scene/hw_fakeflat.h"
#include "hwrenderer/scene/hw_drawinfo.h"
#include "hwrenderer/scene/hw_portal.h"
#include "hwrenderer/data/hw_viewpointbuffer.h"
#include "hwrenderer/data/flatvertices.h"
#include "hwrenderer/data/shaderuniforms.h"
#include "hwrenderer/dynlights/hw_lightbuffer.h"

#include "swrenderer/r_swscene.h"

#include "vk_framebuffer.h"
#include "vk_buffers.h"
#include "vulkan/renderer/vk_renderstate.h"
#include "vulkan/renderer/vk_renderpass.h"
#include "vulkan/renderer/vk_postprocess.h"
#include "vulkan/renderer/vk_renderbuffers.h"
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
EXTERN_CVAR(Int, screenblocks)
EXTERN_CVAR(Bool, cl_capfps)
EXTERN_CVAR(Bool, gl_no_skyclear)

extern bool NoInterpolateView;

VulkanFrameBuffer::VulkanFrameBuffer(void *hMonitor, bool fullscreen, VulkanDevice *dev) : 
	Super(hMonitor, fullscreen) 
{
	device = dev;
	SetViewportRects(nullptr);
	InitPalette();
}

VulkanFrameBuffer::~VulkanFrameBuffer()
{
	// All descriptors must be destroyed before the descriptor pool in renderpass manager is destroyed
	for (VkHardwareTexture *cur = VkHardwareTexture::First; cur; cur = cur->Next)
		cur->Reset();

	delete MatricesUBO;
	delete StreamUBO;
	delete mVertexData;
	delete mSkyData;
	delete mViewpoints;
	delete mLights;
}

void VulkanFrameBuffer::InitializeState()
{
	static bool first = true;
	if (first)
	{
		PrintStartupLog();
		first = false;
	}

	gl_vendorstring = "Vulkan";
	hwcaps = RFL_SHADER_STORAGE_BUFFER | RFL_BUFFER_STORAGE;
	uniformblockalignment = (unsigned int)device->deviceProperties.limits.minUniformBufferOffsetAlignment;
	maxuniformblock = device->deviceProperties.limits.maxUniformBufferRange;

	mUploadSemaphore.reset(new VulkanSemaphore(device));
	mGraphicsCommandPool.reset(new VulkanCommandPool(device, device->graphicsFamily));

	mScreenBuffers.reset(new VkRenderBuffers());
	mSaveBuffers.reset(new VkRenderBuffers());
	mActiveRenderBuffers = mScreenBuffers.get();

	mPostprocess.reset(new VkPostprocess());
	mRenderPassManager.reset(new VkRenderPassManager());

	mVertexData = new FFlatVertexBuffer(GetWidth(), GetHeight());
	mSkyData = new FSkyVertexBuffer;
	mViewpoints = new GLViewpointBuffer;
	mLights = new FLightBuffer();

	// To do: move this to HW renderer interface maybe?
	MatricesUBO = (VKDataBuffer*)CreateDataBuffer(-1, false);
	StreamUBO = (VKDataBuffer*)CreateDataBuffer(-1, false);
	MatricesUBO->SetData(UniformBufferAlignedSize<::MatricesUBO>() * 50000, nullptr, false);
	StreamUBO->SetData(UniformBufferAlignedSize<::StreamUBO>() * 200, nullptr, false);

	mShaderManager.reset(new VkShaderManager(device));
	mSamplerManager.reset(new VkSamplerManager(device));
	mRenderPassManager->Init();
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
		auto sceneColor = mScreenBuffers->SceneColor.get();

		PipelineBarrier barrier0;
		barrier0.addImage(sceneColor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
		barrier0.addImage(device->swapChain->swapChainImages[device->presentImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
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
			sceneColor->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			device->swapChain->swapChainImages[device->presentImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit, VK_FILTER_NEAREST);

		PipelineBarrier barrier1;
		barrier1.addImage(sceneColor, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, 0);
		barrier1.addImage(device->swapChain->swapChainImages[device->presentImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_TRANSFER_WRITE_BIT, 0);
		barrier1.execute(GetDrawCommands(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
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
		if (result < VK_SUCCESS)
			I_FatalError("Failed to submit command buffer! Error %d\n", result);

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
		if (result < VK_SUCCESS)
			I_FatalError("Failed to submit command buffer! Error %d\n", result);
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
		if (result < VK_SUCCESS)
			I_FatalError("Failed to submit command buffer! Error %d\n", result);
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
	// To do: this is virtually identical to FGLRenderer::RenderView and should be merged.

	mRenderState->SetVertexBuffer(screen->mVertexData);
	screen->mVertexData->Reset();

	sector_t *retsec;
	if (!V_IsHardwareRenderer())
	{
		if (!swdrawer) swdrawer.reset(new SWSceneDrawer);
		retsec = swdrawer->RenderView(player);
	}
	else
	{
		hw_ClearFakeFlat();

		iter_dlightf = iter_dlight = draw_dlight = draw_dlightf = 0;

		checkBenchActive();

		// reset statistics counters
		ResetProfilingData();

		// Get this before everything else
		if (cl_capfps || r_NoInterpolate) r_viewpoint.TicFrac = 1.;
		else r_viewpoint.TicFrac = I_GetTimeFrac();

		screen->mLights->Clear();
		screen->mViewpoints->Clear();

#if 0
		// NoInterpolateView should have no bearing on camera textures, but needs to be preserved for the main view below.
		bool saved_niv = NoInterpolateView;
		NoInterpolateView = false;

		// Shader start time does not need to be handled per level. Just use the one from the camera to render from.
		GetRenderState()->CheckTimer(player->camera->Level->ShaderStartTime);
		// prepare all camera textures that have been used in the last frame.
		// This must be done for all levels, not just the primary one!
		for (auto Level : AllLevels())
		{
			Level->canvasTextureInfo.UpdateAll([&](AActor *camera, FCanvasTexture *camtex, double fov)
			{
				RenderTextureView(camtex, camera, fov);
			});
		}
		NoInterpolateView = saved_niv;
#endif

		// now render the main view
		float fovratio;
		float ratio = r_viewwindow.WidescreenRatio;
		if (r_viewwindow.WidescreenRatio >= 1.3f)
		{
			fovratio = 1.333333f;
		}
		else
		{
			fovratio = ratio;
		}

		retsec = RenderViewpoint(r_viewpoint, player->camera, NULL, r_viewpoint.FieldOfView.Degrees, ratio, fovratio, true, true);
	}
	All.Unclock();
	return retsec;
}

sector_t *VulkanFrameBuffer::RenderViewpoint(FRenderViewpoint &mainvp, AActor * camera, IntRect * bounds, float fov, float ratio, float fovratio, bool mainview, bool toscreen)
{
	// To do: this is virtually identical to FGLRenderer::RenderViewpoint and should be merged.

	R_SetupFrame(mainvp, r_viewwindow, camera);

#if 0
	if (mainview && toscreen)
		UpdateShadowMap();
#endif

	// Update the attenuation flag of all light defaults for each viewpoint.
	// This function will only do something if the setting differs.
	FLightDefaults::SetAttenuationForLevel(!!(camera->Level->flags3 & LEVEL3_ATTENUATE));

	// Render (potentially) multiple views for stereo 3d
	// Fixme. The view offsetting should be done with a static table and not require setup of the entire render state for the mode.
	auto vrmode = VRMode::GetVRMode(mainview && toscreen);
	for (int eye_ix = 0; eye_ix < vrmode->mEyeCount; ++eye_ix)
	{
		const auto &eye = vrmode->mEyes[eye_ix];
		screen->SetViewportRects(bounds);

#if 0
		if (mainview) // Bind the scene frame buffer and turn on draw buffers used by ssao
		{
			bool useSSAO = (gl_ssao != 0);
			mBuffers->BindSceneFB(useSSAO);
			GetRenderState()->SetPassType(useSSAO ? GBUFFER_PASS : NORMAL_PASS);
			GetRenderState()->EnableDrawBuffers(gl_RenderState.GetPassDrawBufferCount());
			GetRenderState()->Apply();
		}
#endif

		auto di = HWDrawInfo::StartDrawInfo(mainvp.ViewLevel, nullptr, mainvp, nullptr);
		auto &vp = di->Viewpoint;

		di->Set3DViewport(*GetRenderState());
		di->SetViewArea();
		auto cm = di->SetFullbrightFlags(mainview ? vp.camera->player : nullptr);
		di->Viewpoint.FieldOfView = fov;	// Set the real FOV for the current scene (it's not necessarily the same as the global setting in r_viewpoint)

		// Stereo mode specific perspective projection
		di->VPUniforms.mProjectionMatrix = eye.GetProjection(fov, ratio, fovratio);
		// Stereo mode specific viewpoint adjustment
		vp.Pos += eye.GetViewShift(vp.HWAngles.Yaw.Degrees);
		di->SetupView(*GetRenderState(), vp.Pos.X, vp.Pos.Y, vp.Pos.Z, false, false);

		// std::function until this can be done better in a cross-API fashion.
		di->ProcessScene(toscreen, [&](HWDrawInfo *di, int mode) {
			DrawScene(di, mode);
		});

		if (mainview)
		{
			PostProcess.Clock();
			if (toscreen) di->EndDrawScene(mainvp.sector, *GetRenderState()); // do not call this for camera textures.

#if 0
			if (GetRenderState()->GetPassType() == GBUFFER_PASS) // Turn off ssao draw buffers
			{
				GetRenderState()->SetPassType(NORMAL_PASS);
				GetRenderState()->EnableDrawBuffers(1);
			}

			mBuffers->BlitSceneToTexture(); // Copy the resulting scene to the current post process texture

			PostProcessScene(cm, [&]() { di->DrawEndScene2D(mainvp.sector, *GetRenderState()); });
#else
			di->DrawEndScene2D(mainvp.sector, *GetRenderState());
#endif

			PostProcess.Unclock();
		}
		di->EndDrawInfo();

#if 0
		if (vrmode->mEyeCount > 1)
			mBuffers->BlitToEyeTexture(eye_ix);
#endif
	}

	return mainvp.sector;
}

void VulkanFrameBuffer::DrawScene(HWDrawInfo *di, int drawmode)
{
	// To do: this is virtually identical to FGLRenderer::DrawScene and should be merged.

	static int recursion = 0;
	static int ssao_portals_available = 0;
	const auto &vp = di->Viewpoint;

#if 0
	bool applySSAO = false;
	if (drawmode == DM_MAINVIEW)
	{
		ssao_portals_available = gl_ssao_portals;
		applySSAO = true;
	}
	else if (drawmode == DM_OFFSCREEN)
	{
		ssao_portals_available = 0;
	}
	else if (drawmode == DM_PORTAL && ssao_portals_available > 0)
	{
		applySSAO = true;
		ssao_portals_available--;
	}
#endif

	if (vp.camera != nullptr)
	{
		ActorRenderFlags savedflags = vp.camera->renderflags;
		di->CreateScene();
		vp.camera->renderflags = savedflags;
	}
	else
	{
		di->CreateScene();
	}

	GetRenderState()->SetDepthMask(true);
	if (!gl_no_skyclear) screen->mPortalState->RenderFirstSkyPortal(recursion, di, *GetRenderState());

	di->RenderScene(*GetRenderState());

#if 0
	if (applySSAO && GetRenderState()->GetPassType() == GBUFFER_PASS)
	{
		GetRenderState()->EnableDrawBuffers(1);
		GLRenderer->AmbientOccludeScene(di->VPUniforms.mProjectionMatrix.get()[5]);
		glViewport(screen->mSceneViewport.left, screen->mSceneViewport.top, screen->mSceneViewport.width, screen->mSceneViewport.height);
		GLRenderer->mBuffers->BindSceneFB(true);
		GetRenderState()->EnableDrawBuffers(GetRenderState()->GetPassDrawBufferCount());
		GetRenderState()->Apply();
		screen->mViewpoints->Bind(*GetRenderState(), di->vpIndex);
	}
#endif

	// Handle all portals after rendering the opaque objects but before
	// doing all translucent stuff
	recursion++;
	screen->mPortalState->EndFrame(di, *GetRenderState());
	recursion--;
	di->RenderTranslucent(*GetRenderState());
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
	if (device->swapChain->vsync != vsync)
	{
		device->windowResized();
	}
}

void VulkanFrameBuffer::CleanForRestart()
{
	// force recreation of the SW scene drawer to ensure it gets a new set of resources.
	swdrawer.reset();
}

IHardwareTexture *VulkanFrameBuffer::CreateHardwareTexture()
{
	return new VkHardwareTexture();
}

FModelRenderer *VulkanFrameBuffer::CreateModelRenderer(int mli) 
{
	return new FGLModelRenderer(nullptr, *GetRenderState(), mli);
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
	mScreenBuffers->BeginFrame(screen->mScreenViewport.width, screen->mScreenViewport.height, screen->mSceneViewport.width, screen->mSceneViewport.height);
	mSaveBuffers->BeginFrame(SAVEPICWIDTH, SAVEPICHEIGHT, SAVEPICWIDTH, SAVEPICHEIGHT);
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

unsigned int VulkanFrameBuffer::GetLightBufferBlockSize() const
{
	return mLights->GetBlockSize();
}

void VulkanFrameBuffer::PrintStartupLog()
{
	FString deviceType;
	switch (device->deviceProperties.deviceType)
	{
	case VK_PHYSICAL_DEVICE_TYPE_OTHER: deviceType = "other"; break;
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: deviceType = "integrated gpu"; break;
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: deviceType = "discrete gpu"; break;
	case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: deviceType = "virtual gpu"; break;
	case VK_PHYSICAL_DEVICE_TYPE_CPU: deviceType = "cpu"; break;
	default: deviceType.Format("%d", (int)device->deviceProperties.deviceType); break;
	}

	FString apiVersion, driverVersion;
	apiVersion.Format("%d.%d.%d", VK_VERSION_MAJOR(device->deviceProperties.apiVersion), VK_VERSION_MINOR(device->deviceProperties.apiVersion), VK_VERSION_PATCH(device->deviceProperties.apiVersion));
	driverVersion.Format("%d.%d.%d", VK_VERSION_MAJOR(device->deviceProperties.driverVersion), VK_VERSION_MINOR(device->deviceProperties.driverVersion), VK_VERSION_PATCH(device->deviceProperties.driverVersion));

	Printf("Vulkan device: " TEXTCOLOR_ORANGE "%s\n", device->deviceProperties.deviceName);
	Printf("Vulkan device type: %s\n", deviceType.GetChars());
	Printf("Vulkan version: %s (api) %s (driver)\n", apiVersion.GetChars(), driverVersion.GetChars());

	Printf(PRINT_LOG, "Vulkan extensions:");
	for (const VkExtensionProperties &p : device->availableDeviceExtensions)
	{
		Printf(PRINT_LOG, " %s", p.extensionName);
	}
	Printf(PRINT_LOG, "\n");

	const auto &limits = device->deviceProperties.limits;
	Printf("Max. texture size: %d\n", limits.maxImageDimension2D);
	Printf("Max. uniform buffer range: %d\n", limits.maxUniformBufferRange);
	Printf("Min. uniform buffer offset alignment: %d\n", limits.minUniformBufferOffsetAlignment);
}

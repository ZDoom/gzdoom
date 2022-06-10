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

#include "volk/volk.h"

#include <inttypes.h>

#include "v_video.h"
#include "m_png.h"

#include "r_videoscale.h"
#include "i_time.h"
#include "v_text.h"
#include "version.h"
#include "v_draw.h"

#include "hw_clock.h"
#include "hw_vrmodes.h"
#include "hw_cvars.h"
#include "hw_skydome.h"
#include "hwrenderer/data/hw_viewpointbuffer.h"
#include "flatvertices.h"
#include "hwrenderer/data/shaderuniforms.h"
#include "hw_lightbuffer.h"

#include "vk_framebuffer.h"
#include "vk_hwbuffer.h"
#include "vulkan/renderer/vk_renderstate.h"
#include "vulkan/renderer/vk_renderpass.h"
#include "vulkan/renderer/vk_descriptorset.h"
#include "vulkan/renderer/vk_streambuffer.h"
#include "vulkan/renderer/vk_postprocess.h"
#include "vulkan/renderer/vk_raytrace.h"
#include "vulkan/shaders/vk_shader.h"
#include "vulkan/textures/vk_renderbuffers.h"
#include "vulkan/textures/vk_samplers.h"
#include "vulkan/textures/vk_hwtexture.h"
#include "vulkan/textures/vk_texture.h"
#include "vulkan/system/vk_builders.h"
#include "vulkan/system/vk_swapchain.h"
#include "vulkan/system/vk_commandbuffer.h"
#include "vulkan/system/vk_buffer.h"
#include "engineerrors.h"
#include "c_dispatch.h"

EXTERN_CVAR(Bool, r_drawvoxels)
EXTERN_CVAR(Int, gl_tonemap)
EXTERN_CVAR(Int, screenblocks)
EXTERN_CVAR(Bool, cl_capfps)

CCMD(vk_memstats)
{
	VmaStats stats = {};
	vmaCalculateStats(GetVulkanFrameBuffer()->device->allocator, &stats);
	Printf("Allocated objects: %d, used bytes: %d MB\n", (int)stats.total.allocationCount, (int)stats.total.usedBytes / (1024 * 1024));
	Printf("Unused range count: %d, unused bytes: %d MB\n", (int)stats.total.unusedRangeCount, (int)stats.total.unusedBytes / (1024 * 1024));
}

VulkanFrameBuffer::VulkanFrameBuffer(void *hMonitor, bool fullscreen, VulkanDevice *dev) : 
	Super(hMonitor, fullscreen) 
{
	device = dev;
}

VulkanFrameBuffer::~VulkanFrameBuffer()
{
	vkDeviceWaitIdle(device->device); // make sure the GPU is no longer using any objects before RAII tears them down

	// All descriptors must be destroyed before the descriptor pool in renderpass manager is destroyed
	VkHardwareBuffer::ResetAll();
	PPResource::ResetAll();

	delete MatrixBuffer;
	delete StreamBuffer;
	delete mVertexData;
	delete mSkyData;
	delete mViewpoints;
	delete mLights;
	mShadowMap.Reset();

	mCommands->DeleteFrameObjects();
}

void VulkanFrameBuffer::InitializeState()
{
	static bool first = true;
	if (first)
	{
		PrintStartupLog();
		first = false;
	}

	// Use the same names here as OpenGL returns.
	switch (device->PhysicalDevice.Properties.vendorID)
	{
	case 0x1002: vendorstring = "ATI Technologies Inc.";     break;
	case 0x10DE: vendorstring = "NVIDIA Corporation";  break;
	case 0x8086: vendorstring = "Intel";   break;
	default:     vendorstring = "Unknown"; break;
	}

	hwcaps = RFL_SHADER_STORAGE_BUFFER | RFL_BUFFER_STORAGE;
	glslversion = 4.50f;
	uniformblockalignment = (unsigned int)device->PhysicalDevice.Properties.limits.minUniformBufferOffsetAlignment;
	maxuniformblock = device->PhysicalDevice.Properties.limits.maxUniformBufferRange;

	mCommands.reset(new VkCommandBufferManager(this));

	mSamplerManager.reset(new VkSamplerManager(this));
	mTextureManager.reset(new VkTextureManager(this));
	mBufferManager.reset(new VkBufferManager(this));

	mScreenBuffers.reset(new VkRenderBuffers(this));
	mSaveBuffers.reset(new VkRenderBuffers(this));
	mActiveRenderBuffers = mScreenBuffers.get();

	mPostprocess.reset(new VkPostprocess(this));
	mDescriptorSetManager.reset(new VkDescriptorSetManager(this));
	mRenderPassManager.reset(new VkRenderPassManager(this));
	mRaytrace.reset(new VkRaytrace(this));

	mVertexData = new FFlatVertexBuffer(GetWidth(), GetHeight());
	mSkyData = new FSkyVertexBuffer;
	mViewpoints = new HWViewpointBuffer;
	mLights = new FLightBuffer();

	CreateFanToTrisIndexBuffer();

	// To do: move this to HW renderer interface maybe?
	MatrixBuffer = new VkStreamBuffer(this, sizeof(MatricesUBO), 50000);
	StreamBuffer = new VkStreamBuffer(this, sizeof(StreamUBO), 300);

	mShaderManager.reset(new VkShaderManager(device));
	mDescriptorSetManager->Init();
#ifdef __APPLE__
	mRenderState.reset(new VkRenderStateMolten(this));
#else
	mRenderState.reset(new VkRenderState(this));
#endif
}

void VulkanFrameBuffer::Update()
{
	twoD.Reset();
	Flush3D.Reset();

	Flush3D.Clock();

	GetPostprocess()->SetActiveRenderTarget();

	Draw2D();
	twod->Clear();

	mRenderState->EndRenderPass();
	mRenderState->EndFrame();

	Flush3D.Unclock();

	mCommands->WaitForCommands(true);
	mCommands->UpdateGpuStats();

	Super::Update();
}

bool VulkanFrameBuffer::CompileNextShader()
{
	return mShaderManager->CompileNextShader();
}

void VulkanFrameBuffer::RenderTextureView(FCanvasTexture* tex, std::function<void(IntRect &)> renderFunc)
{
	auto BaseLayer = static_cast<VkHardwareTexture*>(tex->GetHardwareTexture(0, 0));

	VkTextureImage *image = BaseLayer->GetImage(tex, 0, 0);
	VkTextureImage *depthStencil = BaseLayer->GetDepthStencil(tex);

	mRenderState->EndRenderPass();

	VkImageTransition barrier0;
	barrier0.addImage(image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, true);
	barrier0.execute(mCommands->GetDrawCommands());

	mRenderState->SetRenderTarget(image, depthStencil->View.get(), image->Image->width, image->Image->height, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT);

	IntRect bounds;
	bounds.left = bounds.top = 0;
	bounds.width = min(tex->GetWidth(), image->Image->width);
	bounds.height = min(tex->GetHeight(), image->Image->height);

	renderFunc(bounds);

	mRenderState->EndRenderPass();

	VkImageTransition barrier1;
	barrier1.addImage(image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false);
	barrier1.execute(mCommands->GetDrawCommands());

	mRenderState->SetRenderTarget(&GetBuffers()->SceneColor, GetBuffers()->SceneDepthStencil.View.get(), GetBuffers()->GetWidth(), GetBuffers()->GetHeight(), VK_FORMAT_R16G16B16A16_SFLOAT, GetBuffers()->GetSceneSamples());

	tex->SetUpdated(true);
}

void VulkanFrameBuffer::PostProcessScene(bool swscene, int fixedcm, float flash, const std::function<void()> &afterBloomDrawEndScene2D)
{
	if (!swscene) mPostprocess->BlitSceneToPostprocess(); // Copy the resulting scene to the current post process texture
	mPostprocess->PostProcessScene(fixedcm, flash, afterBloomDrawEndScene2D);
}

const char* VulkanFrameBuffer::DeviceName() const
{
	const auto &props = device->PhysicalDevice.Properties;
	return props.deviceName;
}


void VulkanFrameBuffer::SetVSync(bool vsync)
{
	// This is handled in VulkanSwapChain::AcquireImage.
	cur_vsync = vsync;
}

void VulkanFrameBuffer::PrecacheMaterial(FMaterial *mat, int translation)
{
	if (mat->Source()->GetUseType() == ETextureType::SWCanvas) return;

	MaterialLayerInfo* layer;

	auto systex = static_cast<VkHardwareTexture*>(mat->GetLayer(0, translation, &layer));
	systex->GetImage(layer->layerTexture, translation, layer->scaleFlags);

	int numLayers = mat->NumLayers();
	for (int i = 1; i < numLayers; i++)
	{
		auto syslayer = static_cast<VkHardwareTexture*>(mat->GetLayer(i, 0, &layer));
		syslayer->GetImage(layer->layerTexture, 0, layer->scaleFlags);
	}
}

IHardwareTexture *VulkanFrameBuffer::CreateHardwareTexture(int numchannels)
{
	return new VkHardwareTexture(this, numchannels);
}

FMaterial* VulkanFrameBuffer::CreateMaterial(FGameTexture* tex, int scaleflags)
{
	return new VkMaterial(this, tex, scaleflags);
}

IVertexBuffer *VulkanFrameBuffer::CreateVertexBuffer()
{
	return new VkHardwareVertexBuffer();
}

IIndexBuffer *VulkanFrameBuffer::CreateIndexBuffer()
{
	return new VkHardwareIndexBuffer();
}

IDataBuffer *VulkanFrameBuffer::CreateDataBuffer(int bindingpoint, bool ssbo, bool needsresize)
{
	auto buffer = new VkHardwareDataBuffer(bindingpoint, ssbo, needsresize);

	switch (bindingpoint)
	{
	case LIGHTBUF_BINDINGPOINT: LightBufferSSO = buffer; break;
	case VIEWPOINT_BINDINGPOINT: ViewpointUBO = buffer; break;
	case LIGHTNODES_BINDINGPOINT: LightNodes = buffer; break;
	case LIGHTLINES_BINDINGPOINT: LightLines = buffer; break;
	case LIGHTLIST_BINDINGPOINT: LightList = buffer; break;
	case POSTPROCESS_BINDINGPOINT: break;
	default: break;
	}

	return buffer;
}

void VulkanFrameBuffer::SetTextureFilterMode()
{
	if (mSamplerManager)
	{
		mDescriptorSetManager->ResetHWTextureSets();
		mSamplerManager->ResetHWSamplers();
	}
}

void VulkanFrameBuffer::StartPrecaching()
{
	// Destroy the texture descriptors to avoid problems with potentially stale textures.
	mDescriptorSetManager->ResetHWTextureSets();
}

void VulkanFrameBuffer::BlurScene(float amount)
{
	if (mPostprocess)
		mPostprocess->BlurScene(amount);
}

void VulkanFrameBuffer::UpdatePalette()
{
	if (mPostprocess)
		mPostprocess->ClearTonemapPalette();
}

FTexture *VulkanFrameBuffer::WipeStartScreen()
{
	SetViewportRects(nullptr);

	auto tex = new FWrapperTexture(mScreenViewport.width, mScreenViewport.height, 1);
	auto systex = static_cast<VkHardwareTexture*>(tex->GetSystemTexture());

	systex->CreateWipeTexture(mScreenViewport.width, mScreenViewport.height, "WipeStartScreen");

	return tex;
}

FTexture *VulkanFrameBuffer::WipeEndScreen()
{
	GetPostprocess()->SetActiveRenderTarget();
	Draw2D();
	twod->Clear();

	auto tex = new FWrapperTexture(mScreenViewport.width, mScreenViewport.height, 1);
	auto systex = static_cast<VkHardwareTexture*>(tex->GetSystemTexture());

	systex->CreateWipeTexture(mScreenViewport.width, mScreenViewport.height, "WipeEndScreen");

	return tex;
}

void VulkanFrameBuffer::CopyScreenToBuffer(int w, int h, uint8_t *data)
{
	VkTextureImage image;

	// Convert from rgba16f to rgba8 using the GPU:
	ImageBuilder imgbuilder;
	imgbuilder.setFormat(VK_FORMAT_R8G8B8A8_UNORM);
	imgbuilder.setUsage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	imgbuilder.setSize(w, h);
	image.Image = imgbuilder.create(device);
	GetPostprocess()->BlitCurrentToImage(&image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	// Staging buffer for download
	BufferBuilder bufbuilder;
	bufbuilder.setSize(w * h * 4);
	bufbuilder.setUsage(VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU);
	auto staging = bufbuilder.create(device);

	// Copy from image to buffer
	VkBufferImageCopy region = {};
	region.imageExtent.width = w;
	region.imageExtent.height = h;
	region.imageExtent.depth = 1;
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	mCommands->GetDrawCommands()->copyImageToBuffer(image.Image->image, image.Layout, staging->buffer, 1, &region);

	// Submit command buffers and wait for device to finish the work
	mCommands->WaitForCommands(false);

	// Map and convert from rgba8 to rgb8
	uint8_t *dest = (uint8_t*)data;
	uint8_t *pixels = (uint8_t*)staging->Map(0, w * h * 4);
	int dindex = 0;
	for (int y = 0; y < h; y++)
	{
		int sindex = (h - y - 1) * w * 4;
		for (int x = 0; x < w; x++)
		{
			dest[dindex] = pixels[sindex];
			dest[dindex + 1] = pixels[sindex + 1];
			dest[dindex + 2] = pixels[sindex + 2];
			dindex += 3;
			sindex += 4;
		}
	}
	staging->Unmap();
}

void VulkanFrameBuffer::SetActiveRenderTarget()
{
	mPostprocess->SetActiveRenderTarget();
}

TArray<uint8_t> VulkanFrameBuffer::GetScreenshotBuffer(int &pitch, ESSType &color_type, float &gamma)
{
	int w = SCREENWIDTH;
	int h = SCREENHEIGHT;

	IntRect box;
	box.left = 0;
	box.top = 0;
	box.width = w;
	box.height = h;
	mPostprocess->DrawPresentTexture(box, true, true);

	TArray<uint8_t> ScreenshotBuffer(w * h * 3, true);
	CopyScreenToBuffer(w, h, ScreenshotBuffer.Data());

	pitch = w * 3;
	color_type = SS_RGB;
	gamma = 1.0f;
	return ScreenshotBuffer;
}

void VulkanFrameBuffer::BeginFrame()
{
	SetViewportRects(nullptr);
	mScreenBuffers->BeginFrame(screen->mScreenViewport.width, screen->mScreenViewport.height, screen->mSceneViewport.width, screen->mSceneViewport.height);
	mSaveBuffers->BeginFrame(SAVEPICWIDTH, SAVEPICHEIGHT, SAVEPICWIDTH, SAVEPICHEIGHT);
	mRenderState->BeginFrame();

	WaitForCommands(false);
	mDescriptorSetManager->UpdateFixedSet();
	mDescriptorSetManager->UpdateDynamicSet();

	mCommands->BeginFrame();
}

void VulkanFrameBuffer::InitLightmap(int LMTextureSize, int LMTextureCount, TArray<uint16_t>& LMTextureData)
{
	if (LMTextureData.Size() > 0)
	{
		int w = LMTextureSize;
		int h = LMTextureSize;
		int count = LMTextureCount;
		int pixelsize = 8;
		auto& lightmap = mActiveRenderBuffers->Lightmap;

		lightmap.Reset(this);

		ImageBuilder builder;
		builder.setSize(w, h, 1, count);
		builder.setFormat(VK_FORMAT_R16G16B16A16_SFLOAT);
		builder.setUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
		lightmap.Image = builder.create(device);
		lightmap.Image->SetDebugName("VkRenderBuffers.Lightmap");

		ImageViewBuilder viewbuilder;
		viewbuilder.setType(VK_IMAGE_VIEW_TYPE_2D_ARRAY);
		viewbuilder.setImage(lightmap.Image.get(), VK_FORMAT_R16G16B16A16_SFLOAT);
		lightmap.View = viewbuilder.create(device);
		lightmap.View->SetDebugName("VkRenderBuffers.LightmapView");

		auto cmdbuffer = GetCommands()->GetTransferCommands();

		int totalSize = w * h * count * pixelsize;

		BufferBuilder bufbuilder;
		bufbuilder.setSize(totalSize);
		bufbuilder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
		std::unique_ptr<VulkanBuffer> stagingBuffer = bufbuilder.create(device);
		stagingBuffer->SetDebugName("VkHardwareTexture.mStagingBuffer");

		uint16_t one = 0x3c00; // half-float 1.0
		uint16_t* src = &LMTextureData[0];
		uint16_t* data = (uint16_t*)stagingBuffer->Map(0, totalSize);
		for (int i = w * h * count; i > 0; i--)
		{
			*(data++) = *(src++);
			*(data++) = *(src++);
			*(data++) = *(src++);
			*(data++) = one;
		}
		stagingBuffer->Unmap();

		VkImageTransition imageTransition;
		imageTransition.addImage(&lightmap, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, true, 0, count);
		imageTransition.execute(cmdbuffer);

		VkBufferImageCopy region = {};
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.layerCount = count;
		region.imageExtent.depth = 1;
		region.imageExtent.width = w;
		region.imageExtent.height = h;
		cmdbuffer->copyBufferToImage(stagingBuffer->buffer, lightmap.Image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		VkImageTransition barrier;
		barrier.addImage(&lightmap, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false, 0, count);
		barrier.execute(cmdbuffer);

		GetCommands()->FrameTextureUpload.Buffers.push_back(std::move(stagingBuffer));
		GetCommands()->FrameTextureUpload.TotalSize += totalSize;

		LMTextureData.Reset(); // We no longer need this, release the memory
	}
}

void VulkanFrameBuffer::Draw2D()
{
	::Draw2D(twod, *mRenderState);
}

void VulkanFrameBuffer::WaitForCommands(bool finish)
{
	mCommands->WaitForCommands(finish);
}

unsigned int VulkanFrameBuffer::GetLightBufferBlockSize() const
{
	return mLights->GetBlockSize();
}

void VulkanFrameBuffer::PrintStartupLog()
{
	const auto &props = device->PhysicalDevice.Properties;

	FString deviceType;
	switch (props.deviceType)
	{
	case VK_PHYSICAL_DEVICE_TYPE_OTHER: deviceType = "other"; break;
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: deviceType = "integrated gpu"; break;
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: deviceType = "discrete gpu"; break;
	case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: deviceType = "virtual gpu"; break;
	case VK_PHYSICAL_DEVICE_TYPE_CPU: deviceType = "cpu"; break;
	default: deviceType.Format("%d", (int)props.deviceType); break;
	}

	FString apiVersion, driverVersion;
	apiVersion.Format("%d.%d.%d", VK_VERSION_MAJOR(props.apiVersion), VK_VERSION_MINOR(props.apiVersion), VK_VERSION_PATCH(props.apiVersion));
	driverVersion.Format("%d.%d.%d", VK_VERSION_MAJOR(props.driverVersion), VK_VERSION_MINOR(props.driverVersion), VK_VERSION_PATCH(props.driverVersion));

	Printf("Vulkan device: " TEXTCOLOR_ORANGE "%s\n", props.deviceName);
	Printf("Vulkan device type: %s\n", deviceType.GetChars());
	Printf("Vulkan version: %s (api) %s (driver)\n", apiVersion.GetChars(), driverVersion.GetChars());

	Printf(PRINT_LOG, "Vulkan extensions:");
	for (const VkExtensionProperties &p : device->PhysicalDevice.Extensions)
	{
		Printf(PRINT_LOG, " %s", p.extensionName);
	}
	Printf(PRINT_LOG, "\n");

	const auto &limits = props.limits;
	Printf("Max. texture size: %d\n", limits.maxImageDimension2D);
	Printf("Max. uniform buffer range: %d\n", limits.maxUniformBufferRange);
	Printf("Min. uniform buffer offset alignment: %" PRIu64 "\n", limits.minUniformBufferOffsetAlignment);
}

void VulkanFrameBuffer::CreateFanToTrisIndexBuffer()
{
	TArray<uint32_t> data;
	for (int i = 2; i < 1000; i++)
	{
		data.Push(0);
		data.Push(i - 1);
		data.Push(i);
	}

	FanToTrisIndexBuffer.reset(CreateIndexBuffer());
	FanToTrisIndexBuffer->SetData(sizeof(uint32_t) * data.Size(), data.Data(), BufferUsageType::Static);
}

void VulkanFrameBuffer::SetLevelMesh(hwrenderer::LevelMesh* mesh)
{
	mRaytrace->SetLevelMesh(mesh);
}

void VulkanFrameBuffer::UpdateShadowMap()
{
	mPostprocess->UpdateShadowMap();
}

void VulkanFrameBuffer::SetSaveBuffers(bool yes)
{
	if (yes) mActiveRenderBuffers = mSaveBuffers.get();
	else mActiveRenderBuffers = mScreenBuffers.get();
}

void VulkanFrameBuffer::ImageTransitionScene(bool unknown)
{
	mPostprocess->ImageTransitionScene(unknown);
}

FRenderState* VulkanFrameBuffer::RenderState()
{
	return mRenderState.get();
}

void VulkanFrameBuffer::AmbientOccludeScene(float m5)
{
	mPostprocess->AmbientOccludeScene(m5);
}

void VulkanFrameBuffer::SetSceneRenderTarget(bool useSSAO)
{
	mRenderState->SetRenderTarget(&GetBuffers()->SceneColor, GetBuffers()->SceneDepthStencil.View.get(), GetBuffers()->GetWidth(), GetBuffers()->GetHeight(), VK_FORMAT_R16G16B16A16_SFLOAT, GetBuffers()->GetSceneSamples());
}

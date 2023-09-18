 
#include "vk_lightmap.h"
#include "vulkan/vk_renderdevice.h"
#include "vulkan/textures/vk_texture.h"
#include "vulkan/commands/vk_commandbuffer.h"
#include "vk_raytrace.h"
#include "zvulkan/vulkanbuilders.h"
#include "halffloat.h"
#include "filesystem.h"
#include "cmdlib.h"

static int lastSurfaceCount;
static glcycle_t lightmapRaytrace;
static glcycle_t lightmapRaytraceLast;

static uint32_t lastPixelCount;
static uint32_t totalPixelCount;

ADD_STAT(lightmapper)
{
	FString out;
	out.Format("last: %.3fms\ntotal: %3.fms\nLast batch surface count: %d\nLast batch pixel count: %u\nTotal pixel count: %u", lightmapRaytraceLast.TimeMS(), lightmapRaytrace.TimeMS(), lastSurfaceCount, lastPixelCount, totalPixelCount);
	return out;
}

CVAR(Int, lm_background_updates, 8, CVAR_NOSAVE);
CVAR(Int, lm_max_updates, 128, CVAR_NOSAVE);
CVAR(Float, lm_scale, 1.0, CVAR_NOSAVE);

VkLightmap::VkLightmap(VulkanRenderDevice* fb) : fb(fb)
{
	useRayQuery = fb->GetDevice()->PhysicalDevice.Features.RayQuery.rayQuery;

	CreateUniformBuffer();
	CreateSceneLightBuffer();

	CreateShaders();
	CreateRaytracePipeline();
	CreateResolvePipeline();
	CreateBlurPipeline();
	CreateBakeImage();
}

VkLightmap::~VkLightmap()
{
	if (lights.Buffer)
		lights.Buffer->Unmap();
}

void VkLightmap::SetLevelMesh(LevelMesh* level)
{
	mesh = level;
	UpdateAccelStructDescriptors();

	lightmapRaytrace.Reset();
	lightmapRaytraceLast.Reset();
	totalPixelCount = 0;
	lastPixelCount = 0;
	lastSurfaceCount = 0;
}

void VkLightmap::BeginFrame()
{
	lights.Pos = 0;
	lights.ResetCounter++;
}

void VkLightmap::Raytrace(const TArray<LevelMeshSurface*>& surfaces)
{
	if (surfaces.Size())
	{
		lightmapRaytrace.active = true;
		lightmapRaytraceLast.active = true;

		lightmapRaytrace.Clock();
		lightmapRaytraceLast.ResetAndClock();

		SelectSurfaces(surfaces);
		if (selectedSurfaces.Size() > 0)
		{
			UploadUniforms();
			RenderBakeImage();
			ResolveBakeImage();
			BlurBakeImage();
			CopyBakeImageResult();
		}

		lightmapRaytrace.Unclock();
		lightmapRaytraceLast.Unclock();
	}
}

void VkLightmap::SelectSurfaces(const TArray<LevelMeshSurface*>& surfaces)
{
	bakeImage.maxX = 0;
	bakeImage.maxY = 0;
	selectedSurfaces.Clear();

	const int spacing = 3; // Note: the spacing is here to avoid that the resolve sampler finds data from other surface tiles
	RectPacker packer(bakeImageSize, bakeImageSize, RectPacker::Spacing(spacing));

	for (int i = 0, count = surfaces.Size(); i < count; i++)
	{
		LevelMeshSurface* surface = surfaces[i];

		if (!surface->needsUpdate)
			continue;

		// Only grab surfaces until our bake texture is full
		auto result = packer.insert(surface->texWidth + 2, surface->texHeight + 2);
		if (result.pageIndex == 0)
		{
			SelectedSurface selected;
			selected.Surface = surface;
			selected.X = result.pos.x + 1;
			selected.Y = result.pos.y + 1;
			selectedSurfaces.Push(selected);

			bakeImage.maxX = std::max<uint16_t>(bakeImage.maxX, uint16_t(selected.X + surface->texWidth + spacing));
			bakeImage.maxY = std::max<uint16_t>(bakeImage.maxY, uint16_t(selected.Y + surface->texHeight + spacing));

			surface->needsUpdate = false;
		}
	}
}

void VkLightmap::RenderBakeImage()
{
	auto cmdbuffer = fb->GetCommands()->GetTransferCommands();

	RenderPassBegin()
		.RenderPass(raytrace.renderPass.get())
		.RenderArea(0, 0, bakeImageSize, bakeImageSize)
		.Framebuffer(bakeImage.raytrace.Framebuffer.get())
		.AddClearColor(0.0f, 0.0f, 0.0f, 0.0f)
		.Execute(cmdbuffer);

	VkDeviceSize offset = 0;
	cmdbuffer->bindVertexBuffers(0, 1, &fb->GetRaytrace()->GetVertexBuffer()->buffer, &offset);
	cmdbuffer->bindIndexBuffer(fb->GetRaytrace()->GetIndexBuffer()->buffer, 0, VK_INDEX_TYPE_UINT32);
	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, raytrace.pipeline.get());
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, raytrace.pipelineLayout.get(), 0, raytrace.descriptorSet0.get());
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, raytrace.pipelineLayout.get(), 1, raytrace.descriptorSet1.get());

	VkViewport viewport = {};
	viewport.maxDepth = 1;
	viewport.width = (float)bakeImageSize;
	viewport.height = (float)bakeImageSize;
	fb->GetCommands()->GetTransferCommands()->setViewport(0, 1, &viewport);

	for (int i = 0, count = selectedSurfaces.Size(); i < count; i++)
	{
		auto& selectedSurface = selectedSurfaces[i];
		LevelMeshSurface* targetSurface = selectedSurface.Surface;

		if (targetSurface->LightList.empty() && (targetSurface->plane.XYZ() | mesh->SunDirection) < 0.0f) // No lights, no sun
		{
			selectedSurface.Rendered = true;
			continue;
		}

		LightmapPushConstants pc;
		pc.TileX = (float)selectedSurface.X;
		pc.TileY = (float)selectedSurface.Y;
		pc.SurfaceIndex = mesh->GetSurfaceIndex(targetSurface);
		pc.TextureSize = (float)bakeImageSize;
		pc.TileWidth = (float)targetSurface->texWidth;
		pc.TileHeight = (float)targetSurface->texHeight;
		pc.WorldToLocal = targetSurface->translateWorldToLocal;
		pc.ProjLocalToU = targetSurface->projLocalToU;
		pc.ProjLocalToV = targetSurface->projLocalToV;

		bool buffersFull = false;

		if (targetSurface->tileSurfaces.Size() == 0) // To do: move this to where we set up the smoothing groups as we don't need the groups after this step
		{
			for (LevelMeshSurface* surface : mesh->SmoothingGroups[targetSurface->smoothingGroupIndex].surfaces)
			{
				FVector2 minUV = ToUV(surface->bounds.min, targetSurface);
				FVector2 maxUV = ToUV(surface->bounds.max, targetSurface);
				if (surface != targetSurface && (maxUV.X < 0.0f || maxUV.Y < 0.0f || minUV.X > 1.0f || minUV.Y > 1.0f))
					continue; // Bounding box not visible

				targetSurface->tileSurfaces.Push(surface);
			}
		}

		// Paint all surfaces visible in the tile
		for (LevelMeshSurface* surface : targetSurface->tileSurfaces)
		{
			int lightCount = (int)surface->LightList.size();

			if (surface->LightListResetCounter != lights.ResetCounter)
			{
				if (lights.Pos + lightCount > lights.BufferSize)
				{
					// Our light buffer is full. Postpone the rest.
					buffersFull = true;
					break;
				}

				surface->LightListPos = lights.Pos;

				LightInfo* lightinfo = &lights.Lights[lights.Pos];
				for (const LevelMeshLight* light : surface->LightList)
				{
					lightinfo->Origin = light->Origin;
					lightinfo->RelativeOrigin = light->RelativeOrigin;
					lightinfo->Radius = light->Radius;
					lightinfo->Intensity = light->Intensity;
					lightinfo->InnerAngleCos = light->InnerAngleCos;
					lightinfo->OuterAngleCos = light->OuterAngleCos;
					lightinfo->SpotDir = light->SpotDir;
					lightinfo->Color = light->Color;
					lightinfo++;
				}

				lights.Pos += lightCount;
			}

			pc.LightStart = surface->LightListPos;
			pc.LightEnd = pc.LightStart + lightCount;
			fb->GetCommands()->GetTransferCommands()->pushConstants(raytrace.pipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(LightmapPushConstants), &pc);
			fb->GetCommands()->GetTransferCommands()->drawIndexed(surface->numElements, 1, surface->startElementIndex, 0, 0);
		}

		if (buffersFull)
		{
			while (i < count)
			{
				selectedSurfaces[i].Surface->needsUpdate = true;
				i++;
			}
			break;
		}

		selectedSurface.Rendered = true;
	}

	fb->GetCommands()->GetTransferCommands()->endRenderPass();
}

void VkLightmap::UploadUniforms()
{
	Uniforms values = {};
	values.SunDir = mesh->SunDirection;
	values.SunColor = mesh->SunColor;
	values.SunIntensity = 1.0f;

	uniforms.Uniforms = (uint8_t*)uniforms.TransferBuffer->Map(0, uniforms.NumStructs * uniforms.StructStride);
	*reinterpret_cast<Uniforms*>(uniforms.Uniforms + uniforms.StructStride * uniforms.Index) = values;
	uniforms.TransferBuffer->Unmap();

	auto cmdbuffer = fb->GetCommands()->GetTransferCommands();
	cmdbuffer->copyBuffer(uniforms.TransferBuffer.get(), uniforms.Buffer.get());
	PipelineBarrier()
		.AddBuffer(uniforms.Buffer.get(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
		.Execute(cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

void VkLightmap::ResolveBakeImage()
{
	auto cmdbuffer = fb->GetCommands()->GetTransferCommands();

	PipelineBarrier()
		.AddImage(bakeImage.raytrace.Image.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
		.Execute(cmdbuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	RenderPassBegin()
		.RenderPass(resolve.renderPass.get())
		.RenderArea(0, 0, bakeImage.maxX, bakeImage.maxY)
		.Framebuffer(bakeImage.resolve.Framebuffer.get())
		.Execute(cmdbuffer);

	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, resolve.pipeline.get());
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, resolve.pipelineLayout.get(), 0, bakeImage.resolve.DescriptorSet.get());

	VkViewport viewport = {};
	viewport.maxDepth = 1;
	viewport.width = (float)bakeImage.maxX;
	viewport.height = (float)bakeImage.maxY;
	cmdbuffer->setViewport(0, 1, &viewport);

	cmdbuffer->draw(3, 1, 0, 0);

	cmdbuffer->endRenderPass();
}

void VkLightmap::BlurBakeImage()
{
	auto cmdbuffer = fb->GetCommands()->GetTransferCommands();

	PipelineBarrier()
		.AddImage(bakeImage.resolve.Image.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
		.Execute(cmdbuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	// Pass 0
	{
		RenderPassBegin()
			.RenderPass(blur.renderPass.get())
			.RenderArea(0, 0, bakeImage.maxX, bakeImage.maxY)
			.Framebuffer(bakeImage.blur.Framebuffer.get())
			.Execute(cmdbuffer);

		cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, blur.pipeline[0].get());
		cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, blur.pipelineLayout.get(), 0, bakeImage.blur.DescriptorSet[0].get());

		VkViewport viewport = {};
		viewport.maxDepth = 1;
		viewport.width = (float)bakeImage.maxX;
		viewport.height = (float)bakeImage.maxY;
		cmdbuffer->setViewport(0, 1, &viewport);

		cmdbuffer->draw(3, 1, 0, 0);

		cmdbuffer->endRenderPass();
	}

	PipelineBarrier()
		.AddImage(bakeImage.blur.Image.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
		.Execute(cmdbuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	// Pass 1 (outputs back into resolve fb)
	{
		RenderPassBegin()
			.RenderPass(blur.renderPass.get())
			.RenderArea(0, 0, bakeImage.maxX, bakeImage.maxY)
			.Framebuffer(bakeImage.resolve.Framebuffer.get())
			.Execute(cmdbuffer);

		cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, blur.pipeline[1].get());
		cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, blur.pipelineLayout.get(), 0, bakeImage.blur.DescriptorSet[1].get());

		VkViewport viewport = {};
		viewport.maxDepth = 1;
		viewport.width = (float)bakeImage.maxX;
		viewport.height = (float)bakeImage.maxY;
		cmdbuffer->setViewport(0, 1, &viewport);

		cmdbuffer->draw(3, 1, 0, 0);

		cmdbuffer->endRenderPass();
	}
}

void VkLightmap::CopyBakeImageResult()
{
	uint32_t pixels = 0;
	lastSurfaceCount = 0;
	std::set<int> seenPages;
	std::vector<VkImageCopy> regions;
	for (int i = 0, count = selectedSurfaces.Size(); i < count; i++)
	{
		auto& selected = selectedSurfaces[i];
		if (selected.Rendered)
		{
			LevelMeshSurface* surface = selected.Surface;
			VkImageCopy region = {};
			region.srcOffset.x = selected.X;
			region.srcOffset.y = selected.Y;
			region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.srcSubresource.layerCount = 1;
			region.dstOffset.x = surface->atlasX;
			region.dstOffset.y = surface->atlasY;
			region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.dstSubresource.layerCount = 1;
			region.dstSubresource.baseArrayLayer = surface->atlasPageIndex;
			region.extent.width = surface->texWidth;
			region.extent.height = surface->texHeight;
			region.extent.depth = 1;
			regions.push_back(region);
			seenPages.insert(surface->atlasPageIndex);

			pixels += surface->Area();
			lastSurfaceCount++;
		}
	}

	lastPixelCount = pixels;
	totalPixelCount += pixels;

	if (!regions.empty())
	{
		auto cmdbuffer = fb->GetCommands()->GetTransferCommands();

		PipelineBarrier barrier0;
		barrier0.AddImage(bakeImage.resolve.Image.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
		for (int pageIndex : seenPages)
			barrier0.AddImage(fb->GetTextureManager()->Lightmap.Image.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, pageIndex, 1);
		barrier0.Execute(cmdbuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		cmdbuffer->copyImage(bakeImage.resolve.Image->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, fb->GetTextureManager()->Lightmap.Image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t)regions.size(), regions.data());

		PipelineBarrier barrier1;
		for (int pageIndex : seenPages)
			barrier1.AddImage(fb->GetTextureManager()->Lightmap.Image.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, pageIndex, 1);
		barrier1.Execute(cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	}
}

FVector2 VkLightmap::ToUV(const FVector3& vert, const LevelMeshSurface* targetSurface)
{
	FVector3 localPos = vert - targetSurface->translateWorldToLocal;
	float u = (1.0f + (localPos | targetSurface->projLocalToU)) / (targetSurface->texWidth + 2);
	float v = (1.0f + (localPos | targetSurface->projLocalToV)) / (targetSurface->texHeight + 2);
	return FVector2(u, v);
}

void VkLightmap::CreateShaders()
{
	std::string prefix = "#version 460\r\n";
	std::string traceprefix = "#version 460\r\n";
	if (useRayQuery)
	{
		traceprefix += "#extension GL_EXT_ray_query : require\r\n";
		traceprefix += "#define USE_RAYQUERY\r\n";
	}

	shaders.vertRaytrace = ShaderBuilder()
		.Type(ShaderType::Vertex)
		.AddSource("VersionBlock", prefix)
		.AddSource("vert_raytrace.glsl", LoadPrivateShaderLump("shaders/lightmap/vert_raytrace.glsl").GetChars())
		.DebugName("VkLightmap.VertRaytrace")
		.Create("VkLightmap.VertRaytrace", fb->GetDevice());

	shaders.vertScreenquad = ShaderBuilder()
		.Type(ShaderType::Vertex)
		.AddSource("VersionBlock", prefix)
		.AddSource("vert_screenquad.glsl", LoadPrivateShaderLump("shaders/lightmap/vert_screenquad.glsl").GetChars())
		.DebugName("VkLightmap.VertScreenquad")
		.Create("VkLightmap.VertScreenquad", fb->GetDevice());

	shaders.fragRaytrace = ShaderBuilder()
		.Type(ShaderType::Fragment)
		.AddSource("VersionBlock", traceprefix)
		.AddSource("frag.glsl", LoadPrivateShaderLump("shaders/lightmap/frag_raytrace.glsl").GetChars())
		.DebugName("VkLightmap.FragRaytrace")
		.Create("VkLightmap.FragRaytrace", fb->GetDevice());

	shaders.fragResolve = ShaderBuilder()
		.Type(ShaderType::Fragment)
		.AddSource("VersionBlock", prefix)
		.AddSource("frag_resolve.glsl", LoadPrivateShaderLump("shaders/lightmap/frag_resolve.glsl").GetChars())
		.DebugName("VkLightmap.FragResolve")
		.Create("VkLightmap.FragResolve", fb->GetDevice());

	shaders.fragBlur[0] = ShaderBuilder()
		.Type(ShaderType::Fragment)
		.AddSource("VersionBlock", prefix + "#define BLUR_HORIZONTAL\r\n")
		.AddSource("frag_blur.glsl", LoadPrivateShaderLump("shaders/lightmap/frag_blur.glsl").GetChars())
		.DebugName("VkLightmap.FragBlur")
		.Create("VkLightmap.FragBlur", fb->GetDevice());

	shaders.fragBlur[1] = ShaderBuilder()
		.Type(ShaderType::Fragment)
		.AddSource("VersionBlock", prefix + "#define BLUR_VERTICAL\r\n")
		.AddSource("frag_blur.glsl", LoadPrivateShaderLump("shaders/lightmap/frag_blur.glsl").GetChars())
		.DebugName("VkLightmap.FragBlur")
		.Create("VkLightmap.FragBlur", fb->GetDevice());
}

FString VkLightmap::LoadPrivateShaderLump(const char* lumpname)
{
	int lump = fileSystem.CheckNumForFullName(lumpname, 0);
	if (lump == -1) I_Error("Unable to load '%s'", lumpname);
	return GetStringFromLump(lump);
}

void VkLightmap::CreateRaytracePipeline()
{
	raytrace.descriptorSetLayout0 = DescriptorSetLayoutBuilder()
		.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.DebugName("raytrace.descriptorSetLayout0")
		.Create(fb->GetDevice());

	if (useRayQuery)
	{
		raytrace.descriptorSetLayout1 = DescriptorSetLayoutBuilder()
			.AddBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
			.DebugName("raytrace.descriptorSetLayout1")
			.Create(fb->GetDevice());
	}
	else
	{
		raytrace.descriptorSetLayout1 = DescriptorSetLayoutBuilder()
			.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
			.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
			.AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
			.DebugName("raytrace.descriptorSetLayout1")
			.Create(fb->GetDevice());
	}

	raytrace.pipelineLayout = PipelineLayoutBuilder()
		.AddSetLayout(raytrace.descriptorSetLayout0.get())
		.AddSetLayout(raytrace.descriptorSetLayout1.get())
		.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(LightmapPushConstants))
		.DebugName("raytrace.pipelineLayout")
		.Create(fb->GetDevice());

	raytrace.renderPass = RenderPassBuilder()
		.AddAttachment(
			VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_SAMPLE_COUNT_4_BIT,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.AddSubpass()
		.AddSubpassColorAttachmentRef(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.AddExternalSubpassDependency(
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)
		.DebugName("raytrace.renderPass")
		.Create(fb->GetDevice());

	raytrace.pipeline = GraphicsPipelineBuilder()
		.Layout(raytrace.pipelineLayout.get())
		.RenderPass(raytrace.renderPass.get())
		.AddVertexShader(shaders.vertRaytrace.get())
		.AddFragmentShader(shaders.fragRaytrace.get())
		.AddVertexBufferBinding(0, sizeof(FVector4))
		.AddVertexAttribute(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0)
		.Topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
		.RasterizationSamples(VK_SAMPLE_COUNT_4_BIT)
		.Viewport(0.0f, 0.0f, 0.0f, 0.0f)
		.Scissor(0, 0, 4096, 4096)
		.DebugName("raytrace.pipeline")
		.Create(fb->GetDevice());

	raytrace.descriptorPool0 = DescriptorPoolBuilder()
		.AddPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)
		.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4)
		.MaxSets(1)
		.DebugName("raytrace.descriptorPool0")
		.Create(fb->GetDevice());

	if (useRayQuery)
	{
		raytrace.descriptorPool1 = DescriptorPoolBuilder()
			.AddPoolSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1)
			.MaxSets(1)
			.DebugName("raytrace.descriptorPool1")
			.Create(fb->GetDevice());
	}
	else
	{
		raytrace.descriptorPool1 = DescriptorPoolBuilder()
			.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3)
			.MaxSets(1)
			.DebugName("raytrace.descriptorPool1")
			.Create(fb->GetDevice());
	}

	raytrace.descriptorSet0 = raytrace.descriptorPool0->allocate(raytrace.descriptorSetLayout0.get());
	raytrace.descriptorSet0->SetDebugName("raytrace.descriptorSet1");

	raytrace.descriptorSet1 = raytrace.descriptorPool1->allocate(raytrace.descriptorSetLayout1.get());
	raytrace.descriptorSet1->SetDebugName("raytrace.descriptorSet1");
}

void VkLightmap::UpdateAccelStructDescriptors()
{
	if (useRayQuery)
	{
		WriteDescriptors()
			.AddAccelerationStructure(raytrace.descriptorSet1.get(), 0, fb->GetRaytrace()->GetAccelStruct())
			.Execute(fb->GetDevice());
	}
	else
	{
		WriteDescriptors()
			.AddBuffer(raytrace.descriptorSet1.get(), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->GetRaytrace()->GetNodeBuffer())
			.AddBuffer(raytrace.descriptorSet1.get(), 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->GetRaytrace()->GetVertexBuffer())
			.AddBuffer(raytrace.descriptorSet1.get(), 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->GetRaytrace()->GetIndexBuffer())
			.Execute(fb->GetDevice());
	}

	WriteDescriptors()
		.AddBuffer(raytrace.descriptorSet0.get(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniforms.Buffer.get(), 0, sizeof(Uniforms))
		.AddBuffer(raytrace.descriptorSet0.get(), 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->GetRaytrace()->GetSurfaceIndexBuffer())
		.AddBuffer(raytrace.descriptorSet0.get(), 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->GetRaytrace()->GetSurfaceBuffer())
		.AddBuffer(raytrace.descriptorSet0.get(), 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, lights.Buffer.get())
		.AddBuffer(raytrace.descriptorSet0.get(), 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, fb->GetRaytrace()->GetPortalBuffer())
		.Execute(fb->GetDevice());

}

void VkLightmap::CreateResolvePipeline()
{
	resolve.descriptorSetLayout = DescriptorSetLayoutBuilder()
		.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.DebugName("resolve.descriptorSetLayout")
		.Create(fb->GetDevice());

	resolve.pipelineLayout = PipelineLayoutBuilder()
		.AddSetLayout(resolve.descriptorSetLayout.get())
		.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(LightmapPushConstants))
		.DebugName("resolve.pipelineLayout")
		.Create(fb->GetDevice());

	resolve.renderPass = RenderPassBuilder()
		.AddAttachment(
			VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.AddSubpass()
		.AddSubpassColorAttachmentRef(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.AddExternalSubpassDependency(
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)
		.DebugName("resolve.renderpass")
		.Create(fb->GetDevice());

	resolve.pipeline = GraphicsPipelineBuilder()
		.Layout(resolve.pipelineLayout.get())
		.RenderPass(resolve.renderPass.get())
		.AddVertexShader(shaders.vertScreenquad.get())
		.AddFragmentShader(shaders.fragResolve.get())
		.Topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
		.Viewport(0.0f, 0.0f, 0.0f, 0.0f)
		.Scissor(0, 0, 4096, 4096)
		.DebugName("resolve.pipeline")
		.Create(fb->GetDevice());

	resolve.descriptorPool = DescriptorPoolBuilder()
		.AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256)
		.MaxSets(256)
		.DebugName("resolve.descriptorPool")
		.Create(fb->GetDevice());

	resolve.sampler = SamplerBuilder()
		.DebugName("resolve.Sampler")
		.Create(fb->GetDevice());
}

void VkLightmap::CreateBlurPipeline()
{
	blur.descriptorSetLayout = DescriptorSetLayoutBuilder()
		.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.DebugName("blur.descriptorSetLayout")
		.Create(fb->GetDevice());

	blur.pipelineLayout = PipelineLayoutBuilder()
		.AddSetLayout(blur.descriptorSetLayout.get())
		.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(LightmapPushConstants))
		.DebugName("blur.pipelineLayout")
		.Create(fb->GetDevice());

	blur.renderPass = RenderPassBuilder()
		.AddAttachment(
			VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.AddSubpass()
		.AddSubpassColorAttachmentRef(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.AddExternalSubpassDependency(
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)
		.DebugName("blur.renderpass")
		.Create(fb->GetDevice());

	for (int i = 0; i < 2; i++)
	{
		blur.pipeline[i] = GraphicsPipelineBuilder()
			.Layout(blur.pipelineLayout.get())
			.RenderPass(blur.renderPass.get())
			.AddVertexShader(shaders.vertScreenquad.get())
			.AddFragmentShader(shaders.fragBlur[i].get())
			.Topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
			.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
			.Viewport(0.0f, 0.0f, 0.0f, 0.0f)
			.Scissor(0, 0, 4096, 4096)
			.DebugName("blur.pipeline")
			.Create(fb->GetDevice());
	}

	blur.descriptorPool = DescriptorPoolBuilder()
		.AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256)
		.MaxSets(256)
		.DebugName("blur.descriptorPool")
		.Create(fb->GetDevice());

	blur.sampler = SamplerBuilder()
		.DebugName("blur.Sampler")
		.Create(fb->GetDevice());
}

void VkLightmap::CreateBakeImage()
{
	int width = bakeImageSize;
	int height = bakeImageSize;

	bakeImage.raytrace.Image = ImageBuilder()
		.Usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
		.Format(VK_FORMAT_R16G16B16A16_SFLOAT)
		.Size(width, height)
		.Samples(VK_SAMPLE_COUNT_4_BIT)
		.DebugName("LightmapImage.raytrace.Image")
		.Create(fb->GetDevice());

	bakeImage.raytrace.View = ImageViewBuilder()
		.Image(bakeImage.raytrace.Image.get(), VK_FORMAT_R16G16B16A16_SFLOAT)
		.DebugName("LightmapImage.raytrace.View")
		.Create(fb->GetDevice());

	bakeImage.raytrace.Framebuffer = FramebufferBuilder()
		.RenderPass(raytrace.renderPass.get())
		.Size(width, height)
		.AddAttachment(bakeImage.raytrace.View.get())
		.DebugName("LightmapImage.raytrace.Framebuffer")
		.Create(fb->GetDevice());

	bakeImage.resolve.Image = ImageBuilder()
		.Usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
		.Format(VK_FORMAT_R16G16B16A16_SFLOAT)
		.Size(width, height)
		.DebugName("LightmapImage.resolve.Image")
		.Create(fb->GetDevice());

	bakeImage.resolve.View = ImageViewBuilder()
		.Image(bakeImage.resolve.Image.get(), VK_FORMAT_R16G16B16A16_SFLOAT)
		.DebugName("LightmapImage.resolve.View")
		.Create(fb->GetDevice());

	bakeImage.resolve.Framebuffer = FramebufferBuilder()
		.RenderPass(resolve.renderPass.get())
		.Size(width, height)
		.AddAttachment(bakeImage.resolve.View.get())
		.DebugName("LightmapImage.resolve.Framebuffer")
		.Create(fb->GetDevice());

	bakeImage.resolve.DescriptorSet = resolve.descriptorPool->allocate(resolve.descriptorSetLayout.get());
	bakeImage.resolve.DescriptorSet->SetDebugName("resolve.descriptorSet");


	bakeImage.blur.Image = ImageBuilder()
		.Usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
		.Format(VK_FORMAT_R16G16B16A16_SFLOAT)
		.Size(width, height)
		.DebugName("LightmapImage.blur.Image")
		.Create(fb->GetDevice());

	bakeImage.blur.View = ImageViewBuilder()
		.Image(bakeImage.blur.Image.get(), VK_FORMAT_R16G16B16A16_SFLOAT)
		.DebugName("LightmapImage.blur.View")
		.Create(fb->GetDevice());

	bakeImage.blur.Framebuffer = FramebufferBuilder()
		.RenderPass(blur.renderPass.get())
		.Size(width, height)
		.AddAttachment(bakeImage.blur.View.get())
		.DebugName("LightmapImage.blur.Framebuffer")
		.Create(fb->GetDevice());

	bakeImage.blur.DescriptorSet[0] = blur.descriptorPool->allocate(blur.descriptorSetLayout.get());
	bakeImage.blur.DescriptorSet[0]->SetDebugName("blur.descriptorSet");

	bakeImage.blur.DescriptorSet[1] = blur.descriptorPool->allocate(blur.descriptorSetLayout.get());
	bakeImage.blur.DescriptorSet[1]->SetDebugName("blur.descriptorSet");

	WriteDescriptors()
		.AddCombinedImageSampler(bakeImage.resolve.DescriptorSet.get(), 0, bakeImage.raytrace.View.get(), resolve.sampler.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		.AddCombinedImageSampler(bakeImage.blur.DescriptorSet[0].get(), 0, bakeImage.resolve.View.get(), blur.sampler.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		.AddCombinedImageSampler(bakeImage.blur.DescriptorSet[1].get(), 0, bakeImage.blur.View.get(), blur.sampler.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		.Execute(fb->GetDevice());
}

void VkLightmap::CreateUniformBuffer()
{
	VkDeviceSize align = fb->GetDevice()->PhysicalDevice.Properties.Properties.limits.minUniformBufferOffsetAlignment;
	uniforms.StructStride = (sizeof(Uniforms) + align - 1) / align * align;

	uniforms.Buffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)
		.Size(uniforms.NumStructs * uniforms.StructStride)
		.DebugName("LightmapUniformBuffer")
		.Create(fb->GetDevice());

	uniforms.TransferBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU)
		.Size(uniforms.NumStructs * uniforms.StructStride)
		.DebugName("LightmapUniformTransferBuffer")
		.Create(fb->GetDevice());
}

void VkLightmap::CreateSceneLightBuffer()
{
	size_t size = sizeof(LightInfo) * lights.BufferSize;

	lights.Buffer = BufferBuilder()
		.Usage(
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VMA_MEMORY_USAGE_UNKNOWN, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
		.MemoryType(
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		.Size(size)
		.DebugName("LightmapLightBuffer")
		.Create(fb->GetDevice());

	lights.Lights = (LightInfo*)lights.Buffer->Map(0, size);
	lights.Pos = 0;
}

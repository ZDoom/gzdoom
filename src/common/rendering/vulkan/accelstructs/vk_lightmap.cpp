 
#include "vk_lightmap.h"
#include "vulkan/vk_renderdevice.h"
#include "zvulkan/vulkanbuilders.h"
#include "halffloat.h"
#include "filesystem.h"
#include "cmdlib.h"

VkLightmap::VkLightmap(VulkanRenderDevice* fb) : fb(fb)
{
	useRayQuery = fb->GetDevice()->PhysicalDevice.Features.RayQuery.rayQuery;

	submitFence = std::make_unique<VulkanFence>(fb->GetDevice());
	cmdpool = std::make_unique<VulkanCommandPool>(fb->GetDevice(), fb->GetDevice()->GraphicsFamily);

	CreateUniformBuffer();
	CreateSceneVertexBuffer();
	CreateSceneLightBuffer();

	CreateShaders();
	CreateRaytracePipeline();
	CreateResolvePipeline();
}

VkLightmap::~VkLightmap()
{
}

void VkLightmap::Raytrace(hwrenderer::LevelMesh* level)
{
	mesh = level;

	UpdateAccelStructDescriptors(); // To do: we only need to do this if the accel struct changes.

	BeginCommands();
	UploadUniforms();

	for (size_t pageIndex = 0; pageIndex < atlasImages.size(); pageIndex++)
	{
		RenderAtlasImage(pageIndex);
	}

	for (size_t pageIndex = 0; pageIndex < atlasImages.size(); pageIndex++)
	{
		ResolveAtlasImage(pageIndex);
	}

	FinishCommands();
}

void VkLightmap::BeginCommands()
{
	cmdbuffer = cmdpool->createBuffer();
	cmdbuffer->begin();
}

void VkLightmap::FinishCommands()
{
	cmdbuffer->end();

	QueueSubmit()
		.AddCommandBuffer(cmdbuffer.get())
		.Execute(fb->GetDevice(), fb->GetDevice()->GraphicsQueue, submitFence.get());

	VkResult result = vkWaitForFences(fb->GetDevice()->device, 1, &submitFence->fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkWaitForFences failed");
	result = vkResetFences(fb->GetDevice()->device, 1, &submitFence->fence);
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkResetFences failed");
	cmdbuffer.reset();
}

void VkLightmap::RenderAtlasImage(size_t pageIndex)
{
	LightmapImage& img = atlasImages[pageIndex];

	const auto beginPass = [&]() {
		RenderPassBegin()
			.RenderPass(raytrace.renderPass.get())
			.RenderArea(0, 0, atlasImageSize, atlasImageSize)
			.Framebuffer(img.raytrace.Framebuffer.get())
			.Execute(cmdbuffer.get());

		VkDeviceSize offset = 0;
		cmdbuffer->bindVertexBuffers(0, 1, &vertices.Buffer->buffer, &offset);
		cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, raytrace.pipeline.get());
		cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, raytrace.pipelineLayout.get(), 0, raytrace.descriptorSet0.get());
		cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, raytrace.pipelineLayout.get(), 1, raytrace.descriptorSet1.get());
		};
	beginPass();

	for (size_t i = 0; i < mesh->surfaces.Size(); i++)
	{
		hwrenderer::Surface* targetSurface = mesh->surfaces[i].get();
		if (targetSurface->atlasPageIndex != pageIndex)
			continue;

		VkViewport viewport = {};
		viewport.maxDepth = 1;
		viewport.x = (float)targetSurface->atlasX - 1;
		viewport.y = (float)targetSurface->atlasY - 1;
		viewport.width = (float)(targetSurface->texWidth + 2);
		viewport.height = (float)(targetSurface->texHeight + 2);
		cmdbuffer->setViewport(0, 1, &viewport);

		// Paint all surfaces part of the smoothing group into the surface
		for (hwrenderer::Surface* surface : mesh->smoothingGroups[targetSurface->smoothingGroupIndex].surfaces)
		{
			FVector2 minUV = ToUV(surface->boundsMin, targetSurface);
			FVector2 maxUV = ToUV(surface->boundsMax, targetSurface);
			if (surface != targetSurface && (maxUV.X < 0.0f || maxUV.Y < 0.0f || minUV.X > 1.0f || minUV.Y > 1.0f))
				continue; // Bounding box not visible

			int firstLight = lights.Pos;
			int firstVertex = vertices.Pos;
			int lightCount = (int)surface->LightList.size();
			int vertexCount = (int)surface->verts.Size();
			if (lights.Pos + lightCount > lights.BufferSize || vertices.Pos + vertexCount > vertices.BufferSize)
			{
				printf(".");

				// Flush scene buffers
				FinishCommands();
				lights.Pos = 0;
				vertices.Pos = 0;
				firstLight = 0;
				firstVertex = 0;
				BeginCommands();
				beginPass();

				printf(".");

				if (lights.Pos + lightCount > lights.BufferSize)
				{
					throw std::runtime_error("SceneLightBuffer is too small!");
				}
				else if (vertices.Pos + vertexCount > vertices.BufferSize)
				{
					throw std::runtime_error("SceneVertexBuffer is too small!");
				}
			}
			lights.Pos += lightCount;
			vertices.Pos += vertexCount;

			LightInfo* lightinfo = &lights.Lights[firstLight];
			for (hwrenderer::ThingLight* light : surface->LightList)
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

			PushConstants pc;
			pc.LightStart = firstLight;
			pc.LightEnd = firstLight + lightCount;
			pc.SurfaceIndex = (int32_t)i;
			pc.LightmapOrigin = targetSurface->worldOrigin - targetSurface->worldStepX - targetSurface->worldStepY;
			pc.LightmapStepX = targetSurface->worldStepX * viewport.width;
			pc.LightmapStepY = targetSurface->worldStepY * viewport.height;
			cmdbuffer->pushConstants(raytrace.pipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pc);

			SceneVertex* vertex = &vertices.Vertices[firstVertex];

			if (surface->type == hwrenderer::ST_FLOOR || surface->type == hwrenderer::ST_CEILING)
			{
				for (int idx = 0; idx < vertexCount; idx++)
				{
					(vertex++)->Position = ToUV(surface->verts[idx], targetSurface);
				}
			}
			else
			{
				(vertex++)->Position = ToUV(surface->verts[0], targetSurface);
				(vertex++)->Position = ToUV(surface->verts[2], targetSurface);
				(vertex++)->Position = ToUV(surface->verts[3], targetSurface);
				(vertex++)->Position = ToUV(surface->verts[1], targetSurface);
			}

			cmdbuffer->draw(vertexCount, 1, firstVertex, 0);
		}
	}

	cmdbuffer->endRenderPass();
}

void VkLightmap::CreateAtlasImages()
{
	const int spacing = 3; // Note: the spacing is here to avoid that the resolve sampler finds data from other surface tiles
	RectPacker packer(atlasImageSize, atlasImageSize, RectPacker::Spacing(spacing));

	for (size_t i = 0; i < mesh->surfaces.Size(); i++)
	{
		hwrenderer::Surface* surface = mesh->surfaces[i].get();

		auto result = packer.insert(surface->texWidth + 2, surface->texHeight + 2);
		surface->atlasX = result.pos.x + 1;
		surface->atlasY = result.pos.y + 1;
		surface->atlasPageIndex = (int)result.pageIndex;
	}

	for (size_t pageIndex = 0; pageIndex < packer.getNumPages(); pageIndex++)
	{
		atlasImages.push_back(CreateImage(atlasImageSize, atlasImageSize));
	}
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

	cmdbuffer->copyBuffer(uniforms.TransferBuffer.get(), uniforms.Buffer.get());
	PipelineBarrier()
		.AddBuffer(uniforms.Buffer.get(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
		.Execute(cmdbuffer.get(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

void VkLightmap::ResolveAtlasImage(size_t i)
{
	LightmapImage& img = atlasImages[i];

	PipelineBarrier()
		.AddImage(img.raytrace.Image.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
		.Execute(cmdbuffer.get(), VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	RenderPassBegin()
		.RenderPass(resolve.renderPass.get())
		.RenderArea(0, 0, atlasImageSize, atlasImageSize)
		.Framebuffer(img.resolve.Framebuffer.get())
		.Execute(cmdbuffer.get());

	VkDeviceSize offset = 0;
	cmdbuffer->bindVertexBuffers(0, 1, &vertices.Buffer->buffer, &offset);
	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, resolve.pipeline.get());

	auto descriptorSet = resolve.descriptorPool->allocate(resolve.descriptorSetLayout.get());
	descriptorSet->SetDebugName("resolve.descriptorSet");
	WriteDescriptors()
		.AddCombinedImageSampler(descriptorSet.get(), 0, img.raytrace.View.get(), resolve.sampler.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		.Execute(fb->GetDevice());
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, resolve.pipelineLayout.get(), 0, descriptorSet.get());
	resolve.descriptorSets.push_back(std::move(descriptorSet));

	VkViewport viewport = {};
	viewport.maxDepth = 1;
	viewport.width = (float)atlasImageSize;
	viewport.height = (float)atlasImageSize;
	cmdbuffer->setViewport(0, 1, &viewport);

	PushConstants pc;
	pc.LightStart = 0;
	pc.LightEnd = 0;
	pc.SurfaceIndex = 0;
	pc.LightmapOrigin = FVector3(0.0f, 0.0f, 0.0f);
	pc.LightmapStepX = FVector3(0.0f, 0.0f, 0.0f);
	pc.LightmapStepY = FVector3(0.0f, 0.0f, 0.0f);
	cmdbuffer->pushConstants(resolve.pipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc);

	int firstVertex = vertices.Pos;
	int vertexCount = 4;
	vertices.Pos += vertexCount;
	SceneVertex* vertex = &vertices.Vertices[firstVertex];
	vertex[0].Position = FVector2(0.0f, 0.0f);
	vertex[1].Position = FVector2(1.0f, 0.0f);
	vertex[2].Position = FVector2(1.0f, 1.0f);
	vertex[3].Position = FVector2(0.0f, 1.0f);
	cmdbuffer->draw(vertexCount, 1, firstVertex, 0);

	cmdbuffer->endRenderPass();

	PipelineBarrier()
		.AddImage(img.resolve.Image.get(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT)
		.Execute(cmdbuffer.get(), VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	VkBufferImageCopy region = {};
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageExtent.width = atlasImageSize;
	region.imageExtent.height = atlasImageSize;
	region.imageExtent.depth = 1;
	cmdbuffer->copyImageToBuffer(img.resolve.Image->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, img.Transfer->buffer, 1, &region);
}

void VkLightmap::DownloadAtlasImage(size_t pageIndex)
{
	struct hvec4
	{
		unsigned short x, y, z, w;
		FVector3 xyz() { return FVector3(halfToFloat(x), halfToFloat(y), halfToFloat(z)); }
	};

	hvec4* pixels = (hvec4*)atlasImages[pageIndex].Transfer->Map(0, atlasImageSize * atlasImageSize * sizeof(hvec4));

	for (size_t i = 0; i < mesh->surfaces.Size(); i++)
	{
		hwrenderer::Surface* surface = mesh->surfaces[i].get();
		if (surface->atlasPageIndex != pageIndex)
			continue;

		int atlasX = surface->atlasX;
		int atlasY = surface->atlasY;
		int sampleWidth = surface->texWidth;
		int sampleHeight = surface->texHeight;

		for (int y = 0; y < sampleHeight; y++)
		{
			FVector3* dest = &surface->texPixels[y * sampleWidth];
			hvec4* src = &pixels[atlasX + (atlasY + y) * atlasImageSize];
			for (int x = 0; x < sampleWidth; x++)
			{
				dest[x] = src[x].xyz();
			}
		}
	}
	atlasImages[pageIndex].Transfer->Unmap();
}

FVector2 VkLightmap::ToUV(const FVector3& vert, const hwrenderer::Surface* targetSurface)
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

	shaders.vert = ShaderBuilder()
		.Type(ShaderType::Vertex)
		.AddSource("VersionBlock", prefix)
		.AddSource("vert.glsl", LoadPrivateShaderLump("shaders/lightmap/vert.glsl").GetChars())
		.DebugName("VkLightmap.Vert")
		.Create("VkLightmap.Vert", fb->GetDevice());

	shaders.fragRaytrace = ShaderBuilder()
		.Type(ShaderType::Fragment)
		.AddSource("VersionBlock", traceprefix)
		.AddSource("frag.glsl", LoadPrivateShaderLump("shaders/lightmap/frag.glsl").GetChars())
		.DebugName("VkLightmap.FragRaytrace")
		.Create("VkLightmap.FragRaytrace", fb->GetDevice());

	shaders.fragResolve = ShaderBuilder()
		.Type(ShaderType::Fragment)
		.AddSource("VersionBlock", prefix)
		.AddSource("frag_resolve.glsl", LoadPrivateShaderLump("shaders/lightmap/frag_resolve.glsl").GetChars())
		.DebugName("VkLightmap.FragResolve")
		.Create("VkLightmap.FragResolve", fb->GetDevice());
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
		.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants))
		.DebugName("raytrace.pipelineLayout")
		.Create(fb->GetDevice());

	raytrace.renderPass = RenderPassBuilder()
		.AddAttachment(
			VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_SAMPLE_COUNT_4_BIT,
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
		.DebugName("raytrace.renderpass")
		.Create(fb->GetDevice());

	raytrace.pipeline = GraphicsPipelineBuilder()
		.Layout(raytrace.pipelineLayout.get())
		.RenderPass(raytrace.renderPass.get())
		.AddVertexShader(shaders.vert.get())
		.AddFragmentShader(shaders.fragRaytrace.get())
		.AddVertexBufferBinding(0, sizeof(SceneVertex))
		.AddVertexAttribute(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SceneVertex, Position))
		.Topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN)
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
	// To do: fetch this from VkDescriptorSetManager - perhaps manage it all over there?

#if 0
	if (useRayQuery)
	{
		WriteDescriptors()
			.AddAccelerationStructure(raytrace.descriptorSet1.get(), 0, tlAccelStruct.get())
			.Execute(fb->GetDevice());
	}
	else
	{
		WriteDescriptors()
			.AddBuffer(raytrace.descriptorSet1.get(), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nodesBuffer.get())
			.AddBuffer(raytrace.descriptorSet1.get(), 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vertexBuffer.get())
			.AddBuffer(raytrace.descriptorSet1.get(), 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, indexBuffer.get())
			.Execute(fb->GetDevice());
	}

	WriteDescriptors()
		.AddBuffer(raytrace.descriptorSet0.get(), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniformBuffer.get(), 0, sizeof(Uniforms))
		.AddBuffer(raytrace.descriptorSet0.get(), 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, surfaceIndexBuffer.get())
		.AddBuffer(raytrace.descriptorSet0.get(), 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, surfaceBuffer.get())
		.AddBuffer(raytrace.descriptorSet0.get(), 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sceneLightBuffer.get())
		.AddBuffer(raytrace.descriptorSet0.get(), 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, portalBuffer.get())
		.Execute(fb->GetDevice());
#endif
}

void VkLightmap::CreateResolvePipeline()
{
	resolve.descriptorSetLayout = DescriptorSetLayoutBuilder()
		.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.DebugName("resolve.descriptorSetLayout")
		.Create(fb->GetDevice());

	resolve.pipelineLayout = PipelineLayoutBuilder()
		.AddSetLayout(resolve.descriptorSetLayout.get())
		.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants))
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
		.AddVertexShader(shaders.vert.get())
		.AddFragmentShader(shaders.fragResolve.get())
		.AddVertexBufferBinding(0, sizeof(SceneVertex))
		.AddVertexAttribute(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SceneVertex, Position))
		.Topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN)
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

LightmapImage VkLightmap::CreateImage(int width, int height)
{
	LightmapImage img;

	img.raytrace.Image = ImageBuilder()
		.Usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
		.Format(VK_FORMAT_R16G16B16A16_SFLOAT)
		.Size(width, height)
		.Samples(VK_SAMPLE_COUNT_4_BIT)
		.DebugName("LightmapImage.raytrace.Image")
		.Create(fb->GetDevice());

	img.raytrace.View = ImageViewBuilder()
		.Image(img.raytrace.Image.get(), VK_FORMAT_R16G16B16A16_SFLOAT)
		.DebugName("LightmapImage.raytrace.View")
		.Create(fb->GetDevice());

	img.raytrace.Framebuffer = FramebufferBuilder()
		.RenderPass(raytrace.renderPass.get())
		.Size(width, height)
		.AddAttachment(img.raytrace.View.get())
		.DebugName("LightmapImage.raytrace.Framebuffer")
		.Create(fb->GetDevice());

	img.resolve.Image = ImageBuilder()
		.Usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
		.Format(VK_FORMAT_R16G16B16A16_SFLOAT)
		.Size(width, height)
		.DebugName("LightmapImage.resolve.Image")
		.Create(fb->GetDevice());

	img.resolve.View = ImageViewBuilder()
		.Image(img.resolve.Image.get(), VK_FORMAT_R16G16B16A16_SFLOAT)
		.DebugName("LightmapImage.resolve.View")
		.Create(fb->GetDevice());

	img.resolve.Framebuffer = FramebufferBuilder()
		.RenderPass(resolve.renderPass.get())
		.Size(width, height)
		.AddAttachment(img.resolve.View.get())
		.DebugName("LightmapImage.resolve.Framebuffer")
		.Create(fb->GetDevice());

	img.Transfer = BufferBuilder()
		.Size(width * height * sizeof(FVector4))
		.Usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY)
		.DebugName("LightmapImage.Transfer")
		.Create(fb->GetDevice());

	return img;
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

void VkLightmap::CreateSceneVertexBuffer()
{
	size_t size = sizeof(SceneVertex) * vertices.BufferSize;

	vertices.Buffer = BufferBuilder()
		.Usage(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VMA_MEMORY_USAGE_UNKNOWN, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
		.MemoryType(
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		.Size(size)
		.DebugName("LightmapVertexBuffer")
		.Create(fb->GetDevice());

	vertices.Vertices = (SceneVertex*)vertices.Buffer->Map(0, size);
	vertices.Pos = 0;
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

#pragma once

#include "common/rendering/hwrenderer/data/hw_levelmesh.h"
#include "zvulkan/vulkanobjects.h"
#include <dp_rect_pack.h>

typedef dp::rect_pack::RectPacker<int> RectPacker;

class VulkanRenderDevice;
class FString;

struct Uniforms
{
	FVector3 SunDir;
	float Padding1;
	FVector3 SunColor;
	float SunIntensity;
};

struct PushConstants
{
	uint32_t LightStart;
	uint32_t LightEnd;
	int32_t SurfaceIndex;
	int32_t PushPadding1;
	FVector3 LightmapOrigin;
	float PushPadding2;
	FVector3 LightmapStepX;
	float PushPadding3;
	FVector3 LightmapStepY;
	float PushPadding4;
};

struct LightmapImage
{
	struct
	{
		std::unique_ptr<VulkanImage> Image;
		std::unique_ptr<VulkanImageView> View;
		std::unique_ptr<VulkanFramebuffer> Framebuffer;
	} raytrace;

	struct
	{
		std::unique_ptr<VulkanImage> Image;
		std::unique_ptr<VulkanImageView> View;
		std::unique_ptr<VulkanFramebuffer> Framebuffer;
	} resolve;

	std::unique_ptr<VulkanBuffer> Transfer;
};

struct SceneVertex
{
	FVector2 Position;
};

struct LightInfo
{
	FVector3 Origin;
	float Padding0;
	FVector3 RelativeOrigin;
	float Padding1;
	float Radius;
	float Intensity;
	float InnerAngleCos;
	float OuterAngleCos;
	FVector3 SpotDir;
	float Padding2;
	FVector3 Color;
	float Padding3;
};

static_assert(sizeof(LightInfo) == sizeof(float) * 20);

class VkLightmap
{
public:
	VkLightmap(VulkanRenderDevice* fb);
	~VkLightmap();

	void Raytrace(hwrenderer::LevelMesh* level);

private:
	void UpdateAccelStructDescriptors();

	void BeginCommands();
	void FinishCommands();

	void UploadUniforms();
	void CreateAtlasImages();
	void RenderAtlasImage(size_t pageIndex);
	void ResolveAtlasImage(size_t pageIndex);
	void DownloadAtlasImage(size_t pageIndex);

	LightmapImage CreateImage(int width, int height);

	void CreateShaders();
	void CreateRaytracePipeline();
	void CreateResolvePipeline();
	void CreateUniformBuffer();
	void CreateSceneVertexBuffer();
	void CreateSceneLightBuffer();

	static FVector2 ToUV(const FVector3& vert, const hwrenderer::Surface* targetSurface);

	static FString LoadPrivateShaderLump(const char* lumpname);

	VulkanRenderDevice* fb = nullptr;
	hwrenderer::LevelMesh* mesh = nullptr;

	bool useRayQuery = true;

	struct
	{
		std::unique_ptr<VulkanBuffer> Buffer;
		std::unique_ptr<VulkanBuffer> TransferBuffer;

		uint8_t* Uniforms = nullptr;
		int Index = 0;
		int NumStructs = 256;
		VkDeviceSize StructStride = sizeof(Uniforms);
	} uniforms;

	struct
	{
		static const int BufferSize = 1 * 1024 * 1024;
		std::unique_ptr<VulkanBuffer> Buffer;
		SceneVertex* Vertices = nullptr;
		int Pos = 0;
	} vertices;

	struct
	{
		static const int BufferSize = 2 * 1024 * 1024;
		std::unique_ptr<VulkanBuffer> Buffer;
		LightInfo* Lights = nullptr;
		int Pos = 0;
	} lights;

	struct
	{
		std::unique_ptr<VulkanShader> vert;
		std::unique_ptr<VulkanShader> fragRaytrace;
		std::unique_ptr<VulkanShader> fragResolve;
	} shaders;

	struct
	{
		std::unique_ptr<VulkanDescriptorSetLayout> descriptorSetLayout0;
		std::unique_ptr<VulkanDescriptorSetLayout> descriptorSetLayout1;
		std::unique_ptr<VulkanPipelineLayout> pipelineLayout;
		std::unique_ptr<VulkanPipeline> pipeline;
		std::unique_ptr<VulkanRenderPass> renderPass;
		std::unique_ptr<VulkanDescriptorPool> descriptorPool0;
		std::unique_ptr<VulkanDescriptorPool> descriptorPool1;
		std::unique_ptr<VulkanDescriptorSet> descriptorSet0;
		std::unique_ptr<VulkanDescriptorSet> descriptorSet1;
	} raytrace;

	struct
	{
		std::unique_ptr<VulkanDescriptorSetLayout> descriptorSetLayout;
		std::unique_ptr<VulkanPipelineLayout> pipelineLayout;
		std::unique_ptr<VulkanPipeline> pipeline;
		std::unique_ptr<VulkanRenderPass> renderPass;
		std::unique_ptr<VulkanDescriptorPool> descriptorPool;
		std::vector<std::unique_ptr<VulkanDescriptorSet>> descriptorSets;
		std::unique_ptr<VulkanSampler> sampler;
	} resolve;

	std::unique_ptr<VulkanFence> submitFence;
	std::unique_ptr<VulkanCommandPool> cmdpool;
	std::unique_ptr<VulkanCommandBuffer> cmdbuffer;

	std::vector<LightmapImage> atlasImages;
	static const int atlasImageSize = 2048;
};

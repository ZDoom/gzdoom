
#pragma once

#include <memory>

#include "vectors.h"
#include "matrix.h"
#include "name.h"
#include "hw_renderstate.h"
#include <list>
#include <map>

#define SHADER_MIN_REQUIRED_TEXTURE_LAYERS 11

class ShaderIncludeResult;
class VulkanRenderDevice;
class VulkanDevice;
class VulkanShader;
class VkPPShader;
class PPShader;

struct MatricesUBO
{
	VSMatrix ModelMatrix;
	VSMatrix NormalModelMatrix;
	VSMatrix TextureMatrix;
};

#define MAX_STREAM_DATA ((int)(65536 / sizeof(StreamData)))

struct StreamUBO
{
	StreamData data[MAX_STREAM_DATA];
};

#define MAX_LIGHT_DATA ((int)(65536 / sizeof(FVector4)))

struct LightBufferUBO
{
	FVector4 lights[MAX_LIGHT_DATA];
};

struct PushConstants
{
	int uDataIndex; // streamdata index
	int uLightIndex; // dynamic lights
	int uBoneIndexBase; // bone animation
	int padding;
};

class VkShaderKey
{
public:
	union
	{
		struct
		{
			uint64_t AlphaTest : 1;     // !NO_ALPHATEST
			uint64_t Simple2D : 1;      // uFogEnabled == -3
			uint64_t TextureMode : 3;   // uTextureMode & 0xffff
			uint64_t ClampY : 1;        // uTextureMode & TEXF_ClampY
			uint64_t Brightmap : 1;     // uTextureMode & TEXF_Brightmap
			uint64_t Detailmap : 1;     // uTextureMode & TEXF_Detailmap
			uint64_t Glowmap : 1;       // uTextureMode & TEXF_Glowmap
			uint64_t GBufferPass : 1;   // GBUFFER_PASS
			uint64_t UseShadowmap : 1;  // USE_SHADOWMAPS
			uint64_t UseRaytrace : 1;   // USE_RAYTRACE
			uint64_t FogBeforeLights : 1; // FOG_BEFORE_LIGHTS
			uint64_t FogAfterLights : 1;  // FOG_AFTER_LIGHTS
			uint64_t FogRadial : 1;       // FOG_RADIAL
			uint64_t SWLightRadial : 1; // SWLIGHT_RADIAL
			uint64_t SWLightBanded : 1; // SWLIGHT_BANDED
			uint64_t LightMode : 2;     // LIGHTMODE_DEFAULT, LIGHTMODE_SOFTWARE, LIGHTMODE_VANILLA, LIGHTMODE_BUILD
			uint64_t Unused : 45;
		};
		uint64_t AsQWORD = 0;
	};

	int SpecialEffect = 0;
	int EffectState = 0;

	bool operator<(const VkShaderKey& other) const { return memcmp(this, &other, sizeof(VkShaderKey)) < 0; }
	bool operator==(const VkShaderKey& other) const { return memcmp(this, &other, sizeof(VkShaderKey)) == 0; }
	bool operator!=(const VkShaderKey& other) const { return memcmp(this, &other, sizeof(VkShaderKey)) != 0; }
};

static_assert(sizeof(VkShaderKey) == 16, "sizeof(VkShaderKey) is not its expected size!"); // If this assert fails, the flags union no longer adds up to 64 bits. Or there are gaps in the class so the memcmp doesn't work.

class VkShaderProgram
{
public:
	std::unique_ptr<VulkanShader> vert;
	std::unique_ptr<VulkanShader> frag;
};

class VkShaderManager
{
public:
	VkShaderManager(VulkanRenderDevice* fb);
	~VkShaderManager();

	void Deinit();

	VkShaderProgram* Get(const VkShaderKey& key);

	bool CompileNextShader() { return true; }

	VkPPShader* GetVkShader(PPShader* shader);

	void AddVkPPShader(VkPPShader* shader);
	void RemoveVkPPShader(VkPPShader* shader);

private:
	std::unique_ptr<VulkanShader> LoadVertShader(FString shadername, const char *vert_lump, const char *defines);
	std::unique_ptr<VulkanShader> LoadFragShader(FString shadername, const char *frag_lump, const char *material_lump, const char* mateffect_lump, const char *lightmodel_lump, const char *defines, const VkShaderKey& key);

	ShaderIncludeResult OnInclude(FString headerName, FString includerName, size_t depth, bool system);

	FString GetVersionBlock();
	FString LoadPublicShaderLump(const char *lumpname);
	FString LoadPrivateShaderLump(const char *lumpname);

	VulkanRenderDevice* fb = nullptr;

	std::map<VkShaderKey, VkShaderProgram> programs;

	std::list<VkPPShader*> PPShaders;
};

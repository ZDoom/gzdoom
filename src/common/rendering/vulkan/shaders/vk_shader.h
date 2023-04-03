
#pragma once

#include <memory>

#include "vectors.h"
#include "matrix.h"
#include "name.h"
#include "hw_renderstate.h"
#include <list>

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

struct PushConstants
{
	int uTextureMode;
	float uAlphaThreshold;
	FVector2 uClipSplit;

	// Lighting + Fog
	float uLightLevel;
	float uFogDensity;
	float uLightFactor;
	float uLightDist;
	int uFogEnabled;

	// dynamic lights
	int uLightIndex;

	// Blinn glossiness and specular level
	FVector2 uSpecularMaterial;

	// bone animation
	int uBoneIndexBase;

	int uDataIndex;
	int padding1, padding2, padding3;
};

class VkShaderKey
{
public:
	union
	{
		struct
		{
			uint32_t AlphaTest : 1;
			uint32_t Simple2D : 1;      // uFogEnabled == -3
			uint32_t TextureMode : 3;   // uTextureMode & 0xffff
			uint32_t ClampY : 1;        // uTextureMode & TEXF_ClampY
			uint32_t Brightmap : 1;     // uTextureMode & TEXF_Brightmap
			uint32_t Detailmap : 1;     // uTextureMode & TEXF_Detailmap
			uint32_t Glowmap : 1;       // uTextureMode & TEXF_Glowmap
			uint32_t Unused : 23;
		};
		uint32_t AsDWORD = 0;
	};

	int SpecialEffect = 0;
	int EffectState = 0;

	bool operator<(const VkShaderKey& other) const { return memcmp(this, &other, sizeof(VkShaderKey)) < 0; }
	bool operator==(const VkShaderKey& other) const { return memcmp(this, &other, sizeof(VkShaderKey)) == 0; }
	bool operator!=(const VkShaderKey& other) const { return memcmp(this, &other, sizeof(VkShaderKey)) != 0; }
};

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

	VkShaderProgram* Get(const VkShaderKey& key, EPassType passType);

	bool CompileNextShader();

	VkPPShader* GetVkShader(PPShader* shader);

	void AddVkPPShader(VkPPShader* shader);
	void RemoveVkPPShader(VkPPShader* shader);

private:
	VkShaderProgram* GetEffect(int effect, EPassType passType);
	VkShaderProgram* Get(unsigned int eff, bool alphateston, EPassType passType);

	std::unique_ptr<VulkanShader> LoadVertShader(FString shadername, const char *vert_lump, const char *defines);
	std::unique_ptr<VulkanShader> LoadFragShader(FString shadername, const char *frag_lump, const char *material_lump, const char* mateffect_lump, const char *lightmodel_lump, const char *defines, bool alphatest, bool gbufferpass);

	ShaderIncludeResult OnInclude(FString headerName, FString includerName, size_t depth, bool system);

	FString GetVersionBlock();
	FString LoadPublicShaderLump(const char *lumpname);
	FString LoadPrivateShaderLump(const char *lumpname);

	VulkanRenderDevice* fb = nullptr;

	std::vector<VkShaderProgram> mMaterialShaders[MAX_PASS_TYPES];
	std::vector<VkShaderProgram> mMaterialShadersNAT[MAX_PASS_TYPES];
	std::vector<VkShaderProgram> mEffectShaders[MAX_PASS_TYPES];
	uint8_t compilePass = 0, compileState = 0;
	int compileIndex = 0;

	std::list<VkPPShader*> PPShaders;
};

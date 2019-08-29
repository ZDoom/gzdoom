
#pragma once

#include <memory>

#include "utility/vectors.h"
#include "matrix.h"
#include "name.h"
#include "hwrenderer/scene/hw_renderstate.h"

class VulkanDevice;
class VulkanShader;

struct MatricesUBO
{
	VSMatrix ModelMatrix;
	VSMatrix NormalModelMatrix;
	VSMatrix TextureMatrix;
};

#define MAX_STREAM_DATA 256

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

	int uDataIndex;
	int padding1, padding2, padding3;
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
	VkShaderManager(VulkanDevice *device);
	~VkShaderManager();

	VkShaderProgram *GetEffect(int effect, EPassType passType);
	VkShaderProgram *Get(unsigned int eff, bool alphateston, EPassType passType);

private:
	std::unique_ptr<VulkanShader> LoadVertShader(FString shadername, const char *vert_lump, const char *defines);
	std::unique_ptr<VulkanShader> LoadFragShader(FString shadername, const char *frag_lump, const char *material_lump, const char *light_lump, const char *defines, bool alphatest, bool gbufferpass);

	FString GetTargetGlslVersion();
	FString LoadPublicShaderLump(const char *lumpname);
	FString LoadPrivateShaderLump(const char *lumpname);

	VulkanDevice *device;

	std::vector<VkShaderProgram> mMaterialShaders[MAX_PASS_TYPES];
	std::vector<VkShaderProgram> mMaterialShadersNAT[MAX_PASS_TYPES];
	std::vector<VkShaderProgram> mEffectShaders[MAX_PASS_TYPES];
};

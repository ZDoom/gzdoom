
#pragma once

#include "utility/vectors.h"
#include "r_data/matrix.h"
#include "name.h"

class VulkanDevice;
class VulkanShader;

struct PushConstants
{
	int uTextureMode;
	FVector2 uClipSplit;
	float uAlphaThreshold;

	// colors
	FVector4 uObjectColor;
	FVector4 uObjectColor2;
	FVector4 uDynLightColor;
	FVector4 uAddColor;
	FVector4 uFogColor;
	float uDesaturationFactor;
	float uInterpolationFactor;

	// Glowing walls stuff
	FVector4 uGlowTopPlane;
	FVector4 uGlowTopColor;
	FVector4 uGlowBottomPlane;
	FVector4 uGlowBottomColor;

	FVector4 uGradientTopPlane;
	FVector4 uGradientBottomPlane;

	FVector4 uSplitTopPlane;
	FVector4 uSplitBottomPlane;

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

	// matrices
	VSMatrix ModelMatrix;
	VSMatrix NormalModelMatrix;
	VSMatrix TextureMatrix;
};

class VkShaderManager
{
public:
	VkShaderManager(VulkanDevice *device);
	~VkShaderManager();

	std::unique_ptr<VulkanShader> vert;
	std::unique_ptr<VulkanShader> frag;

private:
	std::unique_ptr<VulkanShader> LoadVertShader(const char *vert_lump, const char *defines);
	std::unique_ptr<VulkanShader> LoadFragShader(const char *frag_lump, const char *material_lump, const char *light_lump, const char *defines);

	FString GetTargetGlslVersion();
	FString LoadShaderLump(const char *lumpname);

	VulkanDevice *device;
};

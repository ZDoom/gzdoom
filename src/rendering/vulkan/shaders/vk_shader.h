
#pragma once

#include "utility/vectors.h"
#include "r_data/matrix.h"
#include "name.h"
#include "hwrenderer/scene/hw_renderstate.h"

class VulkanDevice;
class VulkanShader;

template<typename T> int UniformBufferAlignment() { return (sizeof(T) + 127) / 128 * 128; }

struct MatricesUBO
{
	VSMatrix ModelMatrix;
	VSMatrix NormalModelMatrix;
	VSMatrix TextureMatrix;
};

struct ColorsUBO
{
	FVector4 uObjectColor;
	FVector4 uObjectColor2;
	FVector4 uDynLightColor;
	FVector4 uAddColor;
	FVector4 uFogColor;
	float uDesaturationFactor;
	float uInterpolationFactor;
	float timer;
	int useVertexData;
	FVector4 uVertexColor;
	FVector4 uVertexNormal;
};

struct GlowingWallsUBO
{
	FVector4 uGlowTopPlane;
	FVector4 uGlowTopColor;
	FVector4 uGlowBottomPlane;
	FVector4 uGlowBottomColor;

	FVector4 uGradientTopPlane;
	FVector4 uGradientBottomPlane;

	FVector4 uSplitTopPlane;
	FVector4 uSplitBottomPlane;
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

	VkShaderProgram *GetEffect(int effect);
	VkShaderProgram *Get(unsigned int eff, bool alphateston);

private:
	std::unique_ptr<VulkanShader> LoadVertShader(FString shadername, const char *vert_lump, const char *defines);
	std::unique_ptr<VulkanShader> LoadFragShader(FString shadername, const char *frag_lump, const char *material_lump, const char *light_lump, const char *defines, bool alphatest, bool gbufferpass);

	FString GetTargetGlslVersion();
	FString LoadShaderLump(const char *lumpname);

	VulkanDevice *device;

	std::vector<VkShaderProgram> mMaterialShaders;
	std::vector<VkShaderProgram> mMaterialShadersNAT;
	VkShaderProgram mEffectShaders[MAX_EFFECTS];
};

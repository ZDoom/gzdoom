
#pragma once

#include <memory>

#include "vectors.h"
#include "matrix.h"
#include "name.h"
#include "hw_renderstate.h"
#include <list>

#define SHADER_MIN_REQUIRED_TEXTURE_LAYERS 11

class VulkanFrameBuffer;
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

class VkShaderProgram
{
public:
	std::unique_ptr<VulkanShader> vert;
	std::unique_ptr<VulkanShader> frag;
};

class VkShaderManager
{
public:
	VkShaderManager(VulkanFrameBuffer* fb);
	~VkShaderManager();

	void Deinit();

	VkShaderProgram *GetEffect(int effect, EPassType passType);
	VkShaderProgram *Get(unsigned int eff, bool alphateston, EPassType passType);
	bool CompileNextShader();

	VkPPShader* GetVkShader(PPShader* shader);

	void AddVkPPShader(VkPPShader* shader);
	void RemoveVkPPShader(VkPPShader* shader);

private:
	std::unique_ptr<VulkanShader> LoadVertShader(FString shadername, const char *vert_lump, const char *defines);
	std::unique_ptr<VulkanShader> LoadFragShader(FString shadername, const char *frag_lump, const char *material_lump, const char *light_lump, const char *defines, bool alphatest, bool gbufferpass);

	FString GetTargetGlslVersion();
	FString LoadPublicShaderLump(const char *lumpname);
	FString LoadPrivateShaderLump(const char *lumpname);

	VulkanFrameBuffer* fb = nullptr;

	std::vector<VkShaderProgram> mMaterialShaders[MAX_PASS_TYPES];
	std::vector<VkShaderProgram> mMaterialShadersNAT[MAX_PASS_TYPES];
	std::vector<VkShaderProgram> mEffectShaders[MAX_PASS_TYPES];
	uint8_t compilePass = 0, compileState = 0;
	int compileIndex = 0;

	std::list<VkPPShader*> PPShaders;
};

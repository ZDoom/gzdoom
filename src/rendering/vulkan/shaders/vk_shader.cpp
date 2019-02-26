
#include "vk_shader.h"
#include "vulkan/system/vk_builders.h"
#include "hwrenderer/utility/hw_shaderpatcher.h"
#include "w_wad.h"
#include "doomerrors.h"
#include <ShaderLang.h>

VkShaderManager::VkShaderManager(VulkanDevice *device) : device(device)
{
	ShInitialize();

	vert = LoadVertShader("shaders/glsl/main.vp", "");
	frag = LoadFragShader("shaders/glsl/main.fp", "shaders/glsl/func_normal.fp", "shaders/glsl/material_normal.fp", "");
}

VkShaderManager::~VkShaderManager()
{
	ShFinalize();
}

static const char *shaderBindings = R"(

	// This must match the HWViewpointUniforms struct
	layout(set = 0, binding = 0, std140) uniform ViewpointUBO {
		mat4 ProjectionMatrix;
		mat4 ViewMatrix;
		mat4 NormalViewMatrix;

		vec4 uCameraPos;
		vec4 uClipLine;

		float uGlobVis;			// uGlobVis = R_GetGlobVis(r_visibility) / 32.0
		int uPalLightLevels;	
		int uViewHeight;		// Software fuzz scaling
		float uClipHeight;
		float uClipHeightDirection;
		int uShadowmapFilter;

		float timer; // timer data for material shaders
	};

	// light buffers
	layout(set = 0, binding = 1, std430) buffer LightBufferSSO
	{
	    vec4 lights[];
	};

	layout(set = 0, binding = 2, std140) uniform MatricesUBO {
		mat4 ModelMatrix;
		mat4 NormalModelMatrix;
		mat4 TextureMatrix;
	};

	layout(set = 0, binding = 3, std140) uniform ColorsUBO {
		vec4 uObjectColor;
		vec4 uObjectColor2;
		vec4 uDynLightColor;
		vec4 uAddColor;
		vec4 uFogColor;
		float uDesaturationFactor;
		float uInterpolationFactor;
		float padding0, padding1;
	};

	layout(set = 0, binding = 4, std140) uniform GlowingWallsUBO {
		vec4 uGlowTopPlane;
		vec4 uGlowTopColor;
		vec4 uGlowBottomPlane;
		vec4 uGlowBottomColor;

		vec4 uGradientTopPlane;
		vec4 uGradientBottomPlane;

		vec4 uSplitTopPlane;
		vec4 uSplitBottomPlane;
	};

	// textures
	layout(set = 1, binding = 0) uniform sampler2D tex;
	// layout(set = 1, binding = 1) uniform sampler2D texture2;
	// layout(set = 1, binding = 2) uniform sampler2D texture3;
	// layout(set = 1, binding = 3) uniform sampler2D texture4;
	// layout(set = 1, binding = 4) uniform sampler2D texture5;
	// layout(set = 1, binding = 5) uniform sampler2D texture6;
	// layout(set = 1, binding = 16) uniform sampler2D ShadowMap;

	// This must match the PushConstants struct
	layout(push_constant) uniform PushConstants
	{
		int uTextureMode;
		float uAlphaThreshold;
		vec2 uClipSplit;

		// Lighting + Fog
		float uLightLevel;
		float uFogDensity;
		float uLightFactor;
		float uLightDist;
		int uFogEnabled;

		// dynamic lights
		int uLightIndex;

		// Blinn glossiness and specular level
		vec2 uSpecularMaterial;
	};

	// material types
	#if defined(SPECULAR)
	#define normaltexture texture2
	#define speculartexture texture3
	#define brighttexture texture4
	#elif defined(PBR)
	#define normaltexture texture2
	#define metallictexture texture3
	#define roughnesstexture texture4
	#define aotexture texture5
	#define brighttexture texture6
	#else
	#define brighttexture texture2
	#endif

	// #define SUPPORTS_SHADOWMAPS
	#define VULKAN_COORDINATE_SYSTEM
)";

std::unique_ptr<VulkanShader> VkShaderManager::LoadVertShader(const char *vert_lump, const char *defines)
{
	FString code = GetTargetGlslVersion();
	code << defines << shaderBindings;
	code << "#line 1\n";
	code << LoadShaderLump(vert_lump).GetChars() << "\n";

	ShaderBuilder builder;
	builder.setVertexShader(code);
	return builder.create(device);
}

std::unique_ptr<VulkanShader> VkShaderManager::LoadFragShader(const char *frag_lump, const char *material_lump, const char *light_lump, const char *defines)
{
	FString code = GetTargetGlslVersion();
	code << defines << shaderBindings;
	code << "\n#line 1\n";
	code << LoadShaderLump(frag_lump).GetChars() << "\n";

	if (material_lump)
	{
		if (material_lump[0] != '#')
		{
			FString pp_code = LoadShaderLump(material_lump);

			if (pp_code.IndexOf("ProcessMaterial") < 0)
			{
				// this looks like an old custom hardware shader.
				// add ProcessMaterial function that calls the older ProcessTexel function
				code << "\n" << LoadShaderLump("shaders/glsl/func_defaultmat.fp").GetChars() << "\n";

				if (pp_code.IndexOf("ProcessTexel") < 0)
				{
					// this looks like an even older custom hardware shader.
					// We need to replace the ProcessTexel call to make it work.

					code.Substitute("material.Base = ProcessTexel();", "material.Base = Process(vec4(1.0));");
				}

				if (pp_code.IndexOf("ProcessLight") >= 0)
				{
					// The ProcessLight signatured changed. Forward to the old one.
					code << "\nvec4 ProcessLight(vec4 color);\n";
					code << "\nvec4 ProcessLight(Material material, vec4 color) { return ProcessLight(color); }\n";
				}
			}

			code << "\n#line 1\n";
			code << RemoveLegacyUserUniforms(pp_code).GetChars();
			code.Substitute("gl_TexCoord[0]", "vTexCoord");	// fix old custom shaders.

			if (pp_code.IndexOf("ProcessLight") < 0)
			{
				code << "\n" << LoadShaderLump("shaders/glsl/func_defaultlight.fp").GetChars() << "\n";
			}
		}
		else
		{
			// material_lump is not a lump name but the source itself (from generated shaders)
			code << (material_lump + 1) << "\n";
		}
	}

	if (light_lump)
	{
		code << "\n#line 1\n";
		code << LoadShaderLump(light_lump).GetChars();
	}

	ShaderBuilder builder;
	builder.setFragmentShader(code);
	return builder.create(device);
}

FString VkShaderManager::GetTargetGlslVersion()
{
	return "#version 450 core\n";
}

FString VkShaderManager::LoadShaderLump(const char *lumpname)
{
	int lump = Wads.CheckNumForFullName(lumpname, 0);
	if (lump == -1) I_Error("Unable to load '%s'", lumpname);
	FMemLump data = Wads.ReadLump(lump);
	return data.GetString();
}

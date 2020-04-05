/*
**  Vulkan backend
**  Copyright (c) 2016-2020 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include "vk_shader.h"
#include "vulkan/system/vk_builders.h"
#include "hwrenderer/utility/hw_shaderpatcher.h"
#include "w_wad.h"
#include "doomerrors.h"
#include <ShaderLang.h>

VkShaderManager::VkShaderManager(VulkanDevice *device) : device(device)
{
	ShInitialize();

	const char *mainvp = "shaders/glsl/main.vp";
	const char *mainfp = "shaders/glsl/main.fp";

	for (int j = 0; j < MAX_PASS_TYPES; j++)
	{
		bool gbufferpass = j;
		for (int i = 0; defaultshaders[i].ShaderName != nullptr; i++)
		{
			VkShaderProgram prog;
			prog.vert = LoadVertShader(defaultshaders[i].ShaderName, mainvp, defaultshaders[i].Defines);
			prog.frag = LoadFragShader(defaultshaders[i].ShaderName, mainfp, defaultshaders[i].gettexelfunc, defaultshaders[i].lightfunc, defaultshaders[i].Defines, true, gbufferpass);
			mMaterialShaders[j].push_back(std::move(prog));

			if (i < SHADER_NoTexture)
			{
				VkShaderProgram natprog;
				natprog.vert = LoadVertShader(defaultshaders[i].ShaderName, mainvp, defaultshaders[i].Defines);
				natprog.frag = LoadFragShader(defaultshaders[i].ShaderName, mainfp, defaultshaders[i].gettexelfunc, defaultshaders[i].lightfunc, defaultshaders[i].Defines, false, gbufferpass);
				mMaterialShadersNAT[j].push_back(std::move(natprog));
			}
		}

		for (unsigned i = 0; i < usershaders.Size(); i++)
		{
			FString name = ExtractFileBase(usershaders[i].shader);
			FString defines = defaultshaders[usershaders[i].shaderType].Defines + usershaders[i].defines;

			VkShaderProgram prog;
			prog.vert = LoadVertShader(name, mainvp, defines);
			prog.frag = LoadFragShader(name, mainfp, usershaders[i].shader, defaultshaders[usershaders[i].shaderType].lightfunc, defines, true, gbufferpass);
			mMaterialShaders[j].push_back(std::move(prog));
		}

		for (int i = 0; i < MAX_EFFECTS; i++)
		{
			VkShaderProgram prog;
			prog.vert = LoadVertShader(effectshaders[i].ShaderName, effectshaders[i].vp, effectshaders[i].defines);
			prog.frag = LoadFragShader(effectshaders[i].ShaderName, effectshaders[i].fp1, effectshaders[i].fp2, effectshaders[i].fp3, effectshaders[i].defines, true, gbufferpass);
			mEffectShaders[j].push_back(std::move(prog));
		}
	}
}

VkShaderManager::~VkShaderManager()
{
	ShFinalize();
}

VkShaderProgram *VkShaderManager::GetEffect(int effect, EPassType passType)
{
	if (effect >= 0 && effect < MAX_EFFECTS && mEffectShaders[passType][effect].frag)
	{
		return &mEffectShaders[passType][effect];
	}
	return nullptr;
}

VkShaderProgram *VkShaderManager::Get(unsigned int eff, bool alphateston, EPassType passType)
{
	// indices 0-2 match the warping modes, 3 is brightmap, 4 no texture, the following are custom
	if (!alphateston && eff <= 3)
	{
		return &mMaterialShadersNAT[passType][eff];	// Non-alphatest shaders are only created for default, warp1+2 and brightmap. The rest won't get used anyway
	}
	else if (eff < (unsigned int)mMaterialShaders[passType].size())
	{
		return &mMaterialShaders[passType][eff];
	}
	return nullptr;
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

	struct StreamData
	{
		vec4 uObjectColor;
		vec4 uObjectColor2;
		vec4 uDynLightColor;
		vec4 uAddColor;
		vec4 uTextureAddColor;
		vec4 uTextureModulateColor;
		vec4 uTextureBlendColor;
		vec4 uFogColor;
		float uDesaturationFactor;
		float uInterpolationFactor;
		float timer; // timer data for material shaders
		int useVertexData;
		vec4 uVertexColor;
		vec4 uVertexNormal;
		
		vec4 uGlowTopPlane;
		vec4 uGlowTopColor;
		vec4 uGlowBottomPlane;
		vec4 uGlowBottomColor;

		vec4 uGradientTopPlane;
		vec4 uGradientBottomPlane;

		vec4 uSplitTopPlane;
		vec4 uSplitBottomPlane;
	};

	layout(set = 0, binding = 3, std140) uniform StreamUBO {
		StreamData data[MAX_STREAM_DATA];
	};

	layout(set = 0, binding = 4) uniform sampler2D ShadowMap;

	// textures
	layout(set = 1, binding = 0) uniform sampler2D tex;
	layout(set = 1, binding = 1) uniform sampler2D texture2;
	layout(set = 1, binding = 2) uniform sampler2D texture3;
	layout(set = 1, binding = 3) uniform sampler2D texture4;
	layout(set = 1, binding = 4) uniform sampler2D texture5;
	layout(set = 1, binding = 5) uniform sampler2D texture6;

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

		int uDataIndex;
		int padding1, padding2, padding3;
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

	#define uObjectColor data[uDataIndex].uObjectColor
	#define uObjectColor2 data[uDataIndex].uObjectColor2
	#define uDynLightColor data[uDataIndex].uDynLightColor
	#define uAddColor data[uDataIndex].uAddColor
	#define uTextureBlendColor data[uDataIndex].uTextureBlendColor
	#define uTextureModulateColor data[uDataIndex].uTextureModulateColor
	#define uTextureAddColor data[uDataIndex].uTextureAddColor
	#define uFogColor data[uDataIndex].uFogColor
	#define uDesaturationFactor data[uDataIndex].uDesaturationFactor
	#define uInterpolationFactor data[uDataIndex].uInterpolationFactor
	#define timer data[uDataIndex].timer
	#define useVertexData data[uDataIndex].useVertexData
	#define uVertexColor data[uDataIndex].uVertexColor
	#define uVertexNormal data[uDataIndex].uVertexNormal
	#define uGlowTopPlane data[uDataIndex].uGlowTopPlane
	#define uGlowTopColor data[uDataIndex].uGlowTopColor
	#define uGlowBottomPlane data[uDataIndex].uGlowBottomPlane
	#define uGlowBottomColor data[uDataIndex].uGlowBottomColor
	#define uGradientTopPlane data[uDataIndex].uGradientTopPlane
	#define uGradientBottomPlane data[uDataIndex].uGradientBottomPlane
	#define uSplitTopPlane data[uDataIndex].uSplitTopPlane
	#define uSplitBottomPlane data[uDataIndex].uSplitBottomPlane

	#define SUPPORTS_SHADOWMAPS
	#define VULKAN_COORDINATE_SYSTEM
	#define HAS_UNIFORM_VERTEX_DATA

	// GLSL spec 4.60, 8.15. Noise Functions
	// https://www.khronos.org/registry/OpenGL/specs/gl/GLSLangSpec.4.60.pdf
	//  "The noise functions noise1, noise2, noise3, and noise4 have been deprecated starting with version 4.4 of GLSL.
	//   When not generating SPIR-V they are defined to return the value 0.0 or a vector whose components are all 0.0.
	//   When generating SPIR-V the noise functions are not declared and may not be used."
	// However, we need to support mods with custom shaders created for OpenGL renderer
	float noise1(float) { return 0; }
	vec2 noise2(vec2) { return vec2(0); }
	vec3 noise3(vec3) { return vec3(0); }
	vec4 noise4(vec4) { return vec4(0); }
)";

std::unique_ptr<VulkanShader> VkShaderManager::LoadVertShader(FString shadername, const char *vert_lump, const char *defines)
{
	FString code = GetTargetGlslVersion();
	code << defines;
	code << "\n#define MAX_STREAM_DATA " << std::to_string(MAX_STREAM_DATA).c_str() << "\n";
	code << shaderBindings;
	if (!device->UsedDeviceFeatures.shaderClipDistance) code << "#define NO_CLIPDISTANCE_SUPPORT\n";
	code << "#line 1\n";
	code << LoadPrivateShaderLump(vert_lump).GetChars() << "\n";

	ShaderBuilder builder;
	builder.setVertexShader(code);
	return builder.create(shadername.GetChars(), device);
}

std::unique_ptr<VulkanShader> VkShaderManager::LoadFragShader(FString shadername, const char *frag_lump, const char *material_lump, const char *light_lump, const char *defines, bool alphatest, bool gbufferpass)
{
	FString code = GetTargetGlslVersion();
	code << defines;
	code << "\n#define MAX_STREAM_DATA " << std::to_string(MAX_STREAM_DATA).c_str() << "\n";
	code << shaderBindings;

	if (!device->UsedDeviceFeatures.shaderClipDistance) code << "#define NO_CLIPDISTANCE_SUPPORT\n";
	if (!alphatest) code << "#define NO_ALPHATEST\n";
	if (gbufferpass) code << "#define GBUFFER_PASS\n";

	code << "\n#line 1\n";
	code << LoadPrivateShaderLump(frag_lump).GetChars() << "\n";

	if (material_lump)
	{
		if (material_lump[0] != '#')
		{
			FString pp_code = LoadPublicShaderLump(material_lump);

			if (pp_code.IndexOf("ProcessMaterial") < 0)
			{
				// this looks like an old custom hardware shader.
				// add ProcessMaterial function that calls the older ProcessTexel function
				code << "\n" << LoadPrivateShaderLump("shaders/glsl/func_defaultmat.fp").GetChars() << "\n";

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
				code << "\n" << LoadPrivateShaderLump("shaders/glsl/func_defaultlight.fp").GetChars() << "\n";
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
		code << LoadPrivateShaderLump(light_lump).GetChars();
	}

	ShaderBuilder builder;
	builder.setFragmentShader(code);
	return builder.create(shadername.GetChars(), device);
}

FString VkShaderManager::GetTargetGlslVersion()
{
	return "#version 450 core\n";
}

FString VkShaderManager::LoadPublicShaderLump(const char *lumpname)
{
	int lump = Wads.CheckNumForFullName(lumpname);
	if (lump == -1) I_Error("Unable to load '%s'", lumpname);
	FMemLump data = Wads.ReadLump(lump);
	return data.GetString();
}

FString VkShaderManager::LoadPrivateShaderLump(const char *lumpname)
{
	int lump = Wads.CheckNumForFullName(lumpname, 0);
	if (lump == -1) I_Error("Unable to load '%s'", lumpname);
	FMemLump data = Wads.ReadLump(lump);
	return data.GetString();
}

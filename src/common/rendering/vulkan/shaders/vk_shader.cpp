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
#include "vk_ppshader.h"
#include "zvulkan/vulkanbuilders.h"
#include "vulkan/system/vk_renderdevice.h"
#include "hw_shaderpatcher.h"
#include "filesystem.h"
#include "engineerrors.h"
#include "version.h"
#include "cmdlib.h"

bool VkShaderManager::CompileNextShader()
{
	const char *mainvp = "shaders/glsl/vert_scene.vp";
	const char *mainfp = "shaders/glsl/frag_surface.fp";
	int i = compileIndex;

	if (compileState == 0)
	{
		// regular material shaders
		
		VkShaderProgram prog;
		prog.vert = LoadVertShader(defaultshaders[i].ShaderName, mainvp, defaultshaders[i].Defines);
		prog.frag = LoadFragShader(defaultshaders[i].ShaderName, mainfp, defaultshaders[i].gettexelfunc, defaultshaders[i].lightfunc, defaultshaders[i].Defines, true, compilePass == GBUFFER_PASS);
		mMaterialShaders[compilePass].push_back(std::move(prog));
		
		compileIndex++;
		if (defaultshaders[compileIndex].ShaderName == nullptr)
		{
			compileIndex = 0;
			compileState++;
		}
	}
	else if (compileState == 1)
	{
		// NAT material shaders
		
		VkShaderProgram natprog;
		natprog.vert = LoadVertShader(defaultshaders[i].ShaderName, mainvp, defaultshaders[i].Defines);
		natprog.frag = LoadFragShader(defaultshaders[i].ShaderName, mainfp, defaultshaders[i].gettexelfunc, defaultshaders[i].lightfunc, defaultshaders[i].Defines, false, compilePass == GBUFFER_PASS);
		mMaterialShadersNAT[compilePass].push_back(std::move(natprog));

		compileIndex++;
		if (compileIndex == SHADER_NoTexture)
		{
			compileIndex = 0;
			compileState++;
			if (usershaders.Size() == 0) compileState++;
		}
	}
	else if (compileState == 2)
	{
		// user shaders
		
		const FString& name = ExtractFileBase(usershaders[i].shader);
		FString defines = defaultshaders[usershaders[i].shaderType].Defines + usershaders[i].defines;

		VkShaderProgram prog;
		prog.vert = LoadVertShader(name, mainvp, defines);
		prog.frag = LoadFragShader(name, mainfp, usershaders[i].shader, defaultshaders[usershaders[i].shaderType].lightfunc, defines, true, compilePass == GBUFFER_PASS);
		mMaterialShaders[compilePass].push_back(std::move(prog));

		compileIndex++;
		if (compileIndex >= (int)usershaders.Size())
		{
			compileIndex = 0;
			compileState++;
		}
	}
	else if (compileState == 3)
	{
		// Effect shaders
		
		VkShaderProgram prog;
		prog.vert = LoadVertShader(effectshaders[i].ShaderName, effectshaders[i].vp, effectshaders[i].defines);
		prog.frag = LoadFragShader(effectshaders[i].ShaderName, effectshaders[i].fp1, effectshaders[i].fp2, effectshaders[i].fp3, effectshaders[i].defines, true, compilePass == GBUFFER_PASS);
		mEffectShaders[compilePass].push_back(std::move(prog));

		compileIndex++;
		if (compileIndex >= MAX_EFFECTS)
		{
			compileIndex = 0;
			compilePass++;
			if (compilePass == MAX_PASS_TYPES)
			{
				compileIndex = -1; // we're done.
				return true;
			}
			compileState = 0;
		}
	}
	return false;
}

VkShaderManager::VkShaderManager(VulkanRenderDevice* fb) : fb(fb)
{
	CompileNextShader();
}

VkShaderManager::~VkShaderManager()
{
}

void VkShaderManager::Deinit()
{
	while (!PPShaders.empty())
		RemoveVkPPShader(PPShaders.back());
}

VkShaderProgram *VkShaderManager::GetEffect(int effect, EPassType passType)
{
	if (compileIndex == -1 && effect >= 0 && effect < MAX_EFFECTS && mEffectShaders[passType][effect].frag)
	{
		return &mEffectShaders[passType][effect];
	}
	return nullptr;
}

VkShaderProgram *VkShaderManager::Get(unsigned int eff, bool alphateston, EPassType passType)
{
	if (compileIndex != -1)
		return &mMaterialShaders[0][0];
	// indices 0-2 match the warping modes, 3 no texture, the following are custom
	if (!alphateston && eff < SHADER_NoTexture)
	{
		return &mMaterialShadersNAT[passType][eff];	// Non-alphatest shaders are only created for default, warp1+2. The rest won't get used anyway
	}
	else if (eff < (unsigned int)mMaterialShaders[passType].size())
	{
		return &mMaterialShaders[passType][eff];
	}
	return nullptr;
}

std::unique_ptr<VulkanShader> VkShaderManager::LoadVertShader(FString shadername, const char *vert_lump, const char *defines)
{
	FString code = GetTargetGlslVersion();
	code << defines;
	code << "\n#define MAX_STREAM_DATA " << std::to_string(MAX_STREAM_DATA).c_str() << "\n";
#ifdef NPOT_EMULATION
	code << "#define NPOT_EMULATION\n";
#endif
	code << LoadPrivateShaderLump("shaders/scene/layout_shared.glsl");
	if (!fb->device->EnabledFeatures.Features.shaderClipDistance) code << "#define NO_CLIPDISTANCE_SUPPORT\n";
	code << "#line 1\n";
	code << LoadPrivateShaderLump(vert_lump).GetChars() << "\n";

	return ShaderBuilder()
		.Type(ShaderType::Vertex)
		.AddSource(shadername.GetChars(), code.GetChars())
		.DebugName(shadername.GetChars())
		.Create(shadername.GetChars(), fb->device.get());
}

std::unique_ptr<VulkanShader> VkShaderManager::LoadFragShader(FString shadername, const char *frag_lump, const char *material_lump, const char *light_lump, const char *defines, bool alphatest, bool gbufferpass)
{
	FString code = GetTargetGlslVersion();
	if (fb->device->SupportsExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME))
		code << "\n#define SUPPORTS_RAYTRACING\n";
	code << defines;
	code << "\n$placeholder$";	// here the code can later add more needed #defines.
	code << "\n#define MAX_STREAM_DATA " << std::to_string(MAX_STREAM_DATA).c_str() << "\n";
#ifdef NPOT_EMULATION
	code << "#define NPOT_EMULATION\n";
#endif
	code << LoadPrivateShaderLump("shaders/scene/layout_shared.glsl");
	FString placeholder = "\n";

	if (!fb->device->EnabledFeatures.Features.shaderClipDistance) code << "#define NO_CLIPDISTANCE_SUPPORT\n";
	if (!alphatest) code << "#define NO_ALPHATEST\n";
	if (gbufferpass) code << "#define GBUFFER_PASS\n";

	code << "\n#line 1\n";
	code << LoadPrivateShaderLump(frag_lump).GetChars() << "\n";

	if (material_lump)
	{
		if (material_lump[0] != '#')
		{
			FString pp_code = LoadPublicShaderLump(material_lump);

			if (pp_code.IndexOf("ProcessMaterial") < 0 && pp_code.IndexOf("SetupMaterial") < 0)
			{
				// this looks like an old custom hardware shader.
				// add ProcessMaterial function that calls the older ProcessTexel function

				if (pp_code.IndexOf("GetTexCoord") >= 0)
				{
					code << "\n" << LoadPrivateShaderLump("shaders/glsl/func_defaultmat2.fp").GetChars() << "\n";
				}
				else
				{
					code << "\n" << LoadPrivateShaderLump("shaders/glsl/func_defaultmat.fp").GetChars() << "\n";
					if (pp_code.IndexOf("ProcessTexel") < 0)
					{
						// this looks like an even older custom hardware shader.
						// We need to replace the ProcessTexel call to make it work.

						code.Substitute("material.Base = ProcessTexel();", "material.Base = Process(vec4(1.0));");
					}
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

			// ProcessMaterial must be considered broken because it requires the user to fill in data they possibly cannot know all about.
			if (pp_code.IndexOf("ProcessMaterial") >= 0 && pp_code.IndexOf("SetupMaterial") < 0)
			{
				// This reactivates the old logic and disables all features that cannot be supported with that method.
				placeholder << "#define LEGACY_USER_SHADER\n";
			}
		}
		else
		{
			// material_lump is not a lump name but the source itself (from generated shaders)
			code << (material_lump + 1) << "\n";
		}
	}
	code.Substitute("$placeholder$", placeholder);

	if (light_lump)
	{
		code << "\n#line 1\n";
		code << LoadPrivateShaderLump(light_lump).GetChars();
	}

	return ShaderBuilder()
		.Type(ShaderType::Fragment)
		.AddSource(shadername.GetChars(), code.GetChars())
		.DebugName(shadername.GetChars())
		.Create(shadername.GetChars(), fb->device.get());
}

FString VkShaderManager::GetTargetGlslVersion()
{
	if (fb->device->Instance->ApiVersion == VK_API_VERSION_1_2)
	{
		return "#version 460\n#extension GL_EXT_ray_query : enable\n";
	}
	else
	{
		return "#version 450 core\n";
	}
}

FString VkShaderManager::LoadPublicShaderLump(const char *lumpname)
{
	int lump = fileSystem.CheckNumForFullName(lumpname, 0);
	if (lump == -1) lump = fileSystem.CheckNumForFullName(lumpname);
	if (lump == -1) I_Error("Unable to load '%s'", lumpname);
	return GetStringFromLump(lump);
}

FString VkShaderManager::LoadPrivateShaderLump(const char *lumpname)
{
	int lump = fileSystem.CheckNumForFullName(lumpname, 0);
	if (lump == -1) I_Error("Unable to load '%s'", lumpname);
	auto data = fileSystem.ReadFile(lump);
	return GetStringFromLump(lump);
}

VkPPShader* VkShaderManager::GetVkShader(PPShader* shader)
{
	if (!shader->Backend)
		shader->Backend = std::make_unique<VkPPShader>(fb, shader);
	return static_cast<VkPPShader*>(shader->Backend.get());
}

void VkShaderManager::AddVkPPShader(VkPPShader* shader)
{
	shader->it = PPShaders.insert(PPShaders.end(), shader);
}

void VkShaderManager::RemoveVkPPShader(VkPPShader* shader)
{
	shader->Reset();
	shader->fb = nullptr;
	PPShaders.erase(shader->it);
}

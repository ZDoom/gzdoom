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

VkShaderProgram* VkShaderManager::Get(const VkShaderKey& k, EPassType passType)
{
	VkShaderKey key = k;
	key.GBufferPass = passType;
	auto& program = programs[key];
	if (!program.frag)
	{
		const char* mainvp = "shaders/scene/vert_main.glsl";
		const char* mainfp = "shaders/scene/frag_surface.glsl";

		if (key.SpecialEffect != EFF_NONE)
		{
			const auto& desc = effectshaders[key.SpecialEffect];
			program.vert = LoadVertShader(desc.ShaderName, mainvp, desc.defines);
			program.frag = LoadFragShader(desc.ShaderName, desc.fp1, desc.fp2, desc.fp3, desc.fp4, desc.defines, true, key.GBufferPass);
		}
		else
		{
			if (key.EffectState < FIRST_USER_SHADER)
			{
				const auto& desc = defaultshaders[key.EffectState];
				program.vert = LoadVertShader(desc.ShaderName, mainvp, desc.Defines);
				program.frag = LoadFragShader(desc.ShaderName, mainfp, desc.material_lump, desc.mateffect_lump, desc.lightmodel_lump, desc.Defines, key.AlphaTest, key.GBufferPass);
			}
			else
			{
				const auto& desc = usershaders[key.EffectState];
				const FString& name = ExtractFileBase(desc.shader);
				FString defines = defaultshaders[desc.shaderType].Defines + desc.defines;

				program.vert = LoadVertShader(name, mainvp, defines);
				program.frag = LoadFragShader(name, mainfp, desc.shader, defaultshaders[desc.shaderType].mateffect_lump, defaultshaders[desc.shaderType].lightmodel_lump, defines, key.AlphaTest, key.GBufferPass);
			}
		}
	}
	return &program;
}

std::unique_ptr<VulkanShader> VkShaderManager::LoadVertShader(FString shadername, const char *vert_lump, const char *defines)
{
	FString definesBlock;
	definesBlock << defines << "\n";
	definesBlock << "#define MAX_STREAM_DATA " << std::to_string(MAX_STREAM_DATA).c_str() << "\n";
#ifdef NPOT_EMULATION
	definesBlock << "#define NPOT_EMULATION\n";
#endif
	if (!fb->device->EnabledFeatures.Features.shaderClipDistance)
	{
		definesBlock << "#define NO_CLIPDISTANCE_SUPPORT\n";
	}

	FString layoutBlock;
	layoutBlock << LoadPrivateShaderLump("shaders/scene/layout_shared.glsl").GetChars() << "\n";
	layoutBlock << LoadPrivateShaderLump("shaders/scene/layout_vert.glsl").GetChars() << "\n";

	FString codeBlock;
	codeBlock << LoadPrivateShaderLump(vert_lump).GetChars() << "\n";

	return ShaderBuilder()
		.Type(ShaderType::Vertex)
		.DebugName(shadername.GetChars())
		.AddSource("VersionBlock", GetVersionBlock().GetChars())
		.AddSource("DefinesBlock", definesBlock.GetChars())
		.AddSource("LayoutBlock", layoutBlock.GetChars())
		.AddSource(vert_lump, codeBlock.GetChars())
		.OnIncludeLocal([=](std::string headerName, std::string includerName, size_t depth) { return OnInclude(headerName.c_str(), includerName.c_str(), depth, false); })
		.OnIncludeSystem([=](std::string headerName, std::string includerName, size_t depth) { return OnInclude(headerName.c_str(), includerName.c_str(), depth, true); })
		.Create(shadername.GetChars(), fb->device.get());
}

std::unique_ptr<VulkanShader> VkShaderManager::LoadFragShader(FString shadername, const char *frag_lump, const char *material_lump, const char* mateffect_lump, const char *light_lump, const char *defines, bool alphatest, bool gbufferpass)
{
	FString definesBlock;
	definesBlock << defines << "\n";
	if (fb->device->SupportsExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME)) definesBlock << "\n#define SUPPORTS_RAYTRACING\n";
	definesBlock << defines;
	definesBlock << "\n#define MAX_STREAM_DATA " << std::to_string(MAX_STREAM_DATA).c_str() << "\n";
#ifdef NPOT_EMULATION
	definesBlock << "#define NPOT_EMULATION\n";
#endif
	if (!fb->device->EnabledFeatures.Features.shaderClipDistance) definesBlock << "#define NO_CLIPDISTANCE_SUPPORT\n";
	if (!alphatest) definesBlock << "#define NO_ALPHATEST\n";
	if (gbufferpass) definesBlock << "#define GBUFFER_PASS\n";

	FString layoutBlock;
	layoutBlock << LoadPrivateShaderLump("shaders/scene/layout_shared.glsl").GetChars() << "\n";
	layoutBlock << LoadPrivateShaderLump("shaders/scene/layout_frag.glsl").GetChars() << "\n";

	FString codeBlock;
	codeBlock << LoadPrivateShaderLump(frag_lump).GetChars() << "\n";

	FString materialname = "MaterialBlock";
	FString materialBlock;
	if (material_lump)
	{
		materialname = material_lump;
		materialBlock = LoadPublicShaderLump(material_lump);
	}

	FString lightname = "LightBlock";
	FString lightBlock;
	if (light_lump)
	{
		lightname = light_lump;
		lightBlock << LoadPrivateShaderLump(light_lump).GetChars();
	}

	FString mateffectname = "MaterialEffectBlock";
	FString mateffectBlock;
	if (mateffect_lump)
	{
		mateffectname = mateffect_lump;
		mateffectBlock << LoadPrivateShaderLump(mateffect_lump).GetChars();
	}

	return ShaderBuilder()
		.Type(ShaderType::Fragment)
		.DebugName(shadername.GetChars())
		.AddSource("VersionBlock", GetVersionBlock().GetChars())
		.AddSource("DefinesBlock", definesBlock.GetChars())
		.AddSource("LayoutBlock", layoutBlock.GetChars())
		.AddSource("shaders/scene/includes.glsl", LoadPrivateShaderLump("shaders/scene/includes.glsl").GetChars())
		.AddSource(mateffectname.GetChars(), mateffectBlock.GetChars())
		.AddSource(materialname.GetChars(), materialBlock.GetChars())
		.AddSource(lightname.GetChars(), lightBlock.GetChars())
		.AddSource(frag_lump, codeBlock.GetChars())
		.OnIncludeLocal([=](std::string headerName, std::string includerName, size_t depth) { return OnInclude(headerName.c_str(), includerName.c_str(), depth, false); })
		.OnIncludeSystem([=](std::string headerName, std::string includerName, size_t depth) { return OnInclude(headerName.c_str(), includerName.c_str(), depth, true); })
		.Create(shadername.GetChars(), fb->device.get());
}

FString VkShaderManager::GetVersionBlock()
{
	FString versionBlock;

	if (fb->device->Instance->ApiVersion >= VK_API_VERSION_1_2)
	{
		versionBlock << "#version 460 core\n";
	}
	else
	{
		versionBlock << "#version 450 core\n";
	}

	versionBlock << "#extension GL_GOOGLE_include_directive : enable\n";

	if (fb->device->SupportsExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME))
	{
		versionBlock << "#extension GL_EXT_ray_query : enable\n";
	}

	return versionBlock;
}

ShaderIncludeResult VkShaderManager::OnInclude(FString headerName, FString includerName, size_t depth, bool system)
{
	if (depth > 8)
		I_Error("Too much include recursion!");

	FString includeguardname;
	includeguardname << "_HEADERGUARD_" << headerName.GetChars();
	includeguardname.ReplaceChars("/\\.", '_');

	FString code;
	code << "#ifndef " << includeguardname.GetChars() << "\n";
	code << "#define " << includeguardname.GetChars() << "\n";
	code << "#line 1\n";

	if (system)
		code << LoadPrivateShaderLump(headerName.GetChars()).GetChars() << "\n";
	else
		code << LoadPublicShaderLump(headerName.GetChars()).GetChars() << "\n";

	code << "#endif\n";

	return ShaderIncludeResult(headerName.GetChars(), code.GetChars());
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

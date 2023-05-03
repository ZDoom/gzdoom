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

#include "vk_ppshader.h"
#include "vk_shader.h"
#include "vulkan/system/vk_renderdevice.h"
#include "zvulkan/vulkanbuilders.h"
#include "vulkan/system/vk_commandbuffer.h"
#include "filesystem.h"

VkPPShader::VkPPShader(VulkanRenderDevice* fb, PPShader *shader) : fb(fb)
{
	FString prolog;
	if (!shader->Uniforms.empty())
		prolog = UniformBlockDecl::Create("Uniforms", shader->Uniforms, -1);
	prolog += shader->Defines;

	VertexShader = ShaderBuilder()
		.VertexShader(LoadShaderCode(shader->VertexShader, "", shader->Version).GetChars())
		.DebugName(shader->VertexShader.GetChars())
		.Create(shader->VertexShader.GetChars(), fb->device.get());

	FragmentShader = ShaderBuilder()
		.FragmentShader(LoadShaderCode(shader->FragmentShader, prolog, shader->Version).GetChars())
		.DebugName(shader->FragmentShader.GetChars())
		.Create(shader->FragmentShader.GetChars(), fb->device.get());

	fb->GetShaderManager()->AddVkPPShader(this);
}

VkPPShader::~VkPPShader()
{
	if (fb)
		fb->GetShaderManager()->RemoveVkPPShader(this);
}

void VkPPShader::Reset()
{
	if (fb)
	{
		fb->GetCommands()->DrawDeleteList->Add(std::move(VertexShader));
		fb->GetCommands()->DrawDeleteList->Add(std::move(FragmentShader));
	}
}

FString VkPPShader::LoadShaderCode(const FString &lumpName, const FString &defines, int version)
{
	int lump = fileSystem.CheckNumForFullName(lumpName);
	if (lump == -1) I_FatalError("Unable to load '%s'", lumpName.GetChars());
	FString code = fileSystem.ReadFile(lump).GetString().GetChars();

	FString patchedCode;
	patchedCode.AppendFormat("#version %d\n", 450);
	patchedCode << defines;
	patchedCode << "#line 1\n";
	patchedCode << code;
	return patchedCode;
}


#include "vk_shader.h"
#include "vulkan/system/vk_builders.h"
#include <ShaderLang.h>

VkShaderManager::VkShaderManager()
{
	ShInitialize();

#if 0
	{
		const char *lumpName = "shaders/glsl/screenquad.vp";
		int lump = Wads.CheckNumForFullName(lumpName, 0);
		if (lump == -1) I_FatalError("Unable to load '%s'", lumpName);
		FString code = Wads.ReadLump(lump).GetString().GetChars();

		FString patchedCode;
		patchedCode.AppendFormat("#version %d\n", 450);
		patchedCode << "#line 1\n";
		patchedCode << code;

		ShaderBuilder builder;
		builder.setVertexShader(patchedCode);
		auto shader = builder.create(dev);
	}
#endif
}

VkShaderManager::~VkShaderManager()
{
	ShFinalize();
}

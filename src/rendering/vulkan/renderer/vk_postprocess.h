
#pragma once

#include <functional>

#include "vulkan/system/vk_objects.h"

class FString;

class VkPostprocess
{
public:
	VkPostprocess();
	~VkPostprocess();

	void RenderBuffersReset();

	void PostProcessScene(int fixedcm, const std::function<void()> &afterBloomDrawEndScene2D);

	void AmbientOccludeScene(float m5);
	void BlurScene(float gameinfobluramount);
	void ClearTonemapPalette();

private:
	void UpdateEffectTextures();
	void CompileEffectShaders();
	void RenderEffect(const FString &name);
	void NextEye(int eyeCount);
	void RenderScreenQuad();
};

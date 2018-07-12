#ifndef __GL_TONEMAPSHADER_H
#define __GL_TONEMAPSHADER_H

#include "hwrenderer/postprocessing/hw_shaderprogram.h"

class FTonemapShader
{
public:
	void Bind(IRenderQueue *q);

	static bool IsPaletteMode();

private:
	enum TonemapMode
	{
		None,
		Uncharted2,
		HejlDawson,
		Reinhard,
		Linear,
		Palette,
		NumTonemapModes
	};

	static const char *GetDefines(int mode);

	std::unique_ptr<IShaderProgram> mShader[NumTonemapModes];
};

class FExposureExtractShader
{
public:
	void Bind(IRenderQueue *q);

	struct UniformBlock
	{
		FVector2 Scale;
		FVector2 Offset;

		static std::vector<UniformFieldDesc> Desc()
		{
			return
			{
				{ "Scale", UniformType::Vec2, offsetof(UniformBlock, Scale) },
				{ "Offset", UniformType::Vec2, offsetof(UniformBlock, Offset) }
			};
		}
	};

	ShaderUniforms<UniformBlock, POSTPROCESS_BINDINGPOINT> Uniforms;

private:
	std::unique_ptr<IShaderProgram> mShader;
};

class FExposureAverageShader
{
public:
	void Bind(IRenderQueue *q);

private:
	std::unique_ptr<IShaderProgram> mShader;
};

class FExposureCombineShader
{
public:
	void Bind(IRenderQueue *q);

	struct UniformBlock
	{
		float ExposureBase;
		float ExposureMin;
		float ExposureScale;
		float ExposureSpeed;

		static std::vector<UniformFieldDesc> Desc()
		{
			return
			{
				{ "ExposureBase", UniformType::Float, offsetof(UniformBlock, ExposureBase) },
				{ "ExposureMin", UniformType::Float, offsetof(UniformBlock, ExposureMin) },
				{ "ExposureScale", UniformType::Float, offsetof(UniformBlock, ExposureScale) },
				{ "ExposureSpeed", UniformType::Float, offsetof(UniformBlock, ExposureSpeed) }
			};
		}
	};

	ShaderUniforms<UniformBlock, POSTPROCESS_BINDINGPOINT> Uniforms;

private:
	std::unique_ptr<IShaderProgram> mShader;
};

#endif
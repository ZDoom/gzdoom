#ifndef __GL_TONEMAPSHADER_H
#define __GL_TONEMAPSHADER_H

#include "gl_shaderprogram.h"

class FTonemapShader
{
public:
	void Bind();

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

	FShaderProgram mShader[NumTonemapModes];
};

class FExposureExtractShader
{
public:
	void Bind();

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
	FShaderProgram mShader;
};

class FExposureAverageShader
{
public:
	void Bind();

private:
	FShaderProgram mShader;
};

class FExposureCombineShader
{
public:
	void Bind();

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
	FShaderProgram mShader;
};

#endif
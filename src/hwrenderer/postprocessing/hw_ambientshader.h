#ifndef __GL_AMBIENTSHADER_H
#define __GL_AMBIENTSHADER_H

#include "hwrenderer/postprocessing/hw_shaderprogram.h"

class FAmbientShader
{
protected:
	std::unique_ptr<IShaderProgram> mShader;

public:
	void Bind(IRenderQueue *q);
};

class FLinearDepthShader : public FAmbientShader
{
public:

	void Validate();

	struct UniformBlock
	{
		int SampleIndex;
		float LinearizeDepthA;
		float LinearizeDepthB;
		float InverseDepthRangeA;
		float InverseDepthRangeB;
		float Padding0, Padding1, Padding2;
		FVector2 Scale;
		FVector2 Offset;

		static std::vector<UniformFieldDesc> Desc()
		{
			return
			{
				{ "SampleIndex", UniformType::Int, offsetof(UniformBlock, SampleIndex) },
				{ "LinearizeDepthA", UniformType::Float, offsetof(UniformBlock, LinearizeDepthA) },
				{ "LinearizeDepthB", UniformType::Float, offsetof(UniformBlock, LinearizeDepthB) },
				{ "InverseDepthRangeA", UniformType::Float, offsetof(UniformBlock, InverseDepthRangeA) },
				{ "InverseDepthRangeB", UniformType::Float, offsetof(UniformBlock, InverseDepthRangeB) },
				{ "Padding0", UniformType::Float, offsetof(UniformBlock, Padding0) },
				{ "Padding1", UniformType::Float, offsetof(UniformBlock, Padding1) },
				{ "Padding2", UniformType::Float, offsetof(UniformBlock, Padding2) },
				{ "Scale", UniformType::Vec2, offsetof(UniformBlock, Scale) },
				{ "Offset", UniformType::Vec2, offsetof(UniformBlock, Offset) }
			};
		}
	};
	
	ShaderUniforms<UniformBlock, POSTPROCESS_BINDINGPOINT> Uniforms;

private:
	bool mMultisample = false;
};

class FSSAOShader : public FAmbientShader
{
public:
	void Validate();

	struct UniformBlock
	{
		FVector2 UVToViewA;
		FVector2 UVToViewB;
		FVector2 InvFullResolution;
		float NDotVBias;
		float NegInvR2;
		float RadiusToScreen;
		float AOMultiplier;
		float AOStrength;
		int SampleIndex;
		float Padding0, Padding1;
		FVector2 Scale;
		FVector2 Offset;

		static std::vector<UniformFieldDesc> Desc()
		{
			return
			{
				{ "UVToViewA", UniformType::Vec2, offsetof(UniformBlock, UVToViewA) },
				{ "UVToViewB", UniformType::Vec2, offsetof(UniformBlock, UVToViewB) },
				{ "InvFullResolution", UniformType::Vec2, offsetof(UniformBlock, InvFullResolution) },
				{ "NDotVBias", UniformType::Float, offsetof(UniformBlock, NDotVBias) },
				{ "NegInvR2", UniformType::Float, offsetof(UniformBlock, NegInvR2) },
				{ "RadiusToScreen", UniformType::Float, offsetof(UniformBlock, RadiusToScreen) },
				{ "AOMultiplier", UniformType::Float, offsetof(UniformBlock, AOMultiplier) },
				{ "AOStrength", UniformType::Float, offsetof(UniformBlock, AOStrength) },
				{ "SampleIndex", UniformType::Int, offsetof(UniformBlock, SampleIndex) },
				{ "Padding0", UniformType::Float, offsetof(UniformBlock, Padding0) },
				{ "Padding1", UniformType::Float, offsetof(UniformBlock, Padding1) },
				{ "Scale", UniformType::Vec2, offsetof(UniformBlock, Scale) },
				{ "Offset", UniformType::Vec2, offsetof(UniformBlock, Offset) },
			};
		}
	};

	ShaderUniforms<UniformBlock, POSTPROCESS_BINDINGPOINT> Uniforms;

private:
	enum Quality
	{
		Off,
		LowQuality,
		MediumQuality,
		HighQuality,
		NumQualityModes
	};

	FString GetDefines(int mode, bool multisample);

	Quality mCurrentQuality = Off;
	bool mMultisample = false;
};

class FDepthBlurShader : public FAmbientShader
{
	bool mVertical;
public:
	FDepthBlurShader(bool vertical) : mVertical(vertical) {}
	void Validate();

	struct UniformBlock
	{
		float BlurSharpness;
		float PowExponent;
		FVector2 InvFullResolution;

		static std::vector<UniformFieldDesc> Desc()
		{
			return
			{
				{ "BlurSharpness", UniformType::Float, offsetof(UniformBlock, BlurSharpness) },
				{ "PowExponent", UniformType::Float, offsetof(UniformBlock, PowExponent) },
				{ "InvFullResolution", UniformType::Vec2, offsetof(UniformBlock, InvFullResolution) }
			};
		}
	};

	ShaderUniforms<UniformBlock, POSTPROCESS_BINDINGPOINT> Uniforms;
};

class FSSAOCombineShader : public FAmbientShader
{
public:
	void Validate();

	struct UniformBlock
	{
		int SampleCount;
		int Padding0, Padding1, Padding2;
		FVector2 Scale;
		FVector2 Offset;

		static std::vector<UniformFieldDesc> Desc()
		{
			return
			{
				{ "SampleCount", UniformType::Int, offsetof(UniformBlock, SampleCount) },
				{ "Padding0", UniformType::Int, offsetof(UniformBlock, Padding0) },
				{ "Padding1", UniformType::Int, offsetof(UniformBlock, Padding1) },
				{ "Padding2", UniformType::Int, offsetof(UniformBlock, Padding2) },
				{ "Scale", UniformType::Vec2, offsetof(UniformBlock, Scale) },
				{ "Offset", UniformType::Vec2, offsetof(UniformBlock, Offset) }
			};
		}
	};

	ShaderUniforms<UniformBlock, POSTPROCESS_BINDINGPOINT> Uniforms;

private:
	bool mMultisample = false;
};


class FAmbientPass
{
public:
	std::unique_ptr<FLinearDepthShader> mLinearDepthShader;
	std::unique_ptr<FDepthBlurShader> mDepthBlurShader[2];
	std::unique_ptr<FSSAOShader> mSSAOShader;
	std::unique_ptr<FSSAOCombineShader> mSSAOCombineShader;
public:
	int AmbientWidth = 0;
	int AmbientHeight = 0;
	enum { NumAmbientRandomTextures = 3 };

	FAmbientPass();
	void Setup(float proj5, float aspect);
};
#endif

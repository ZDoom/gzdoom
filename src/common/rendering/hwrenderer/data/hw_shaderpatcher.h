#pragma once

#include "tarray.h"
#include "zstring.h"
#include "utility"

FString RemoveLegacyUserUniforms(FString code);
FString RemoveSamplerBindings(FString code, TArray<std::pair<FString, int>> &samplerstobind);	// For GL 3.3 compatibility which cannot declare sampler bindings in the sampler source.
FString RemoveLayoutLocationDecl(FString code, const char *inoutkeyword);

struct FDefaultShader
{
	const char * ShaderName;
	const char * material_lump;
	const char * mateffect_lump;
	const char * lightmodel_lump;
	const char * Defines;
};

struct FEffectShader
{
	const char *ShaderName;
	const char *fp1;
	const char *fp2;
	const char *fp3;
	const char *fp4;
	const char *defines;
};

extern const FDefaultShader defaultshaders[];
extern const FEffectShader effectshaders[];


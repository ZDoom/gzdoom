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
	const char * gettexelfunc;
	const char * lightfunc;
	const char * Defines;
};

struct FEffectShader
{
	const char *ShaderName;
	const char *vp;
	const char *fp1;
	const char *fp2;
	const char *fp3;
	const char *defines;
};

extern const FDefaultShader defaultshaders[];
extern const FEffectShader effectshaders[];


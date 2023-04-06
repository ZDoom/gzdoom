#pragma once

#include "tarray.h"
#include "zstring.h"
#include "utility"

FString RemoveLegacyUserUniforms(FString code);
FString RemoveSamplerBindings(FString code, TArray<std::pair<FString, int>> &samplerstobind);	// For GL 3.3 compatibility which cannot declare sampler bindings in the sampler source.
FString RemoveLayoutLocationDecl(FString code, const char *inoutkeyword);

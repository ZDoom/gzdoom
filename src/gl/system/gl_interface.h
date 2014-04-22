#ifndef R_RENDER
#define R_RENDER

#include "basictypes.h"

enum RenderFlags
{
	// [BB] Added texture compression flags.
	RFL_TEXTURE_COMPRESSION=8,
	RFL_TEXTURE_COMPRESSION_S3TC=16,
	RFL_NVIDIA = 512,
	RFL_ATI = 1024,
};

enum TexMode
{
	TM_MODULATE = 0,
	TM_MASK = 1,
	TM_OPAQUE = 2,
	TM_REDTOALPHA = 3,
	// The following are not implemented yet, because in ZDoom the respective render styles are currently inaccessible.
	TM_INVERT = 4,
	TM_INVERTOPAQUE = 5,
};

struct RenderContext
{
	unsigned int flags;
	unsigned int maxuniforms;
	int max_texturesize;
	char * vendorstring;

	int MaxLights() const
	{
		return maxuniforms>=2048? 128:64;
	}
};

extern RenderContext gl;

#endif


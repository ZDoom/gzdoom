#ifndef R_RENDER
#define R_RENDER

#include "basictypes.h"

enum RenderFlags
{
	// [BB] Added texture compression flags.
	RFL_TEXTURE_COMPRESSION=1,
	RFL_TEXTURE_COMPRESSION_S3TC=2,

	RFL_FRAMEBUFFER = 4,
	RFL_BUFFER_STORAGE = 8,
	RFL_SHADER_STORAGE_BUFFER = 16,
};

enum TexMode
{
	TM_MODULATE = 0,	// (r, g, b, a)
	TM_MASK = 1,		// (1, 1, 1, a)
	TM_OPAQUE = 2,		// (r, g, b, 1)
	TM_INVERSE = 3,		// (1-r, 1-g, 1-b, a)
};

struct RenderContext
{
	unsigned int flags;
	unsigned int maxuniforms;
	float version;
	float glslversion;
	int max_texturesize;
	char * vendorstring;

	int MaxLights() const
	{
		return maxuniforms>=2048? 128:64;
	}

	bool hasGLSL() const
	{
		return glslversion >= 1.3f;
	}
};

extern RenderContext gl;

#endif


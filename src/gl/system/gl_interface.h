#ifndef R_RENDER
#define R_RENDER

#include "basictypes.h"

enum RenderFlags
{
	// [BB] Added texture compression flags.
	RFL_TEXTURE_COMPRESSION=1,
	RFL_TEXTURE_COMPRESSION_S3TC=2,

	RFL_SHADER_STORAGE_BUFFER = 4,
	RFL_BUFFER_STORAGE = 8
};

enum TexMode
{
	TM_MODULATE = 0,	// (r, g, b, a)
	TM_MASK,			// (1, 1, 1, a)
	TM_OPAQUE,			// (r, g, b, 1)
	TM_INVERSE,			// (1-r, 1-g, 1-b, a)
	TM_REDTOALPHA,		// (1, 1, 1, r)
	TM_CLAMPY,			// (r, g, b, (t >= 0.0 && t <= 1.0)? a:0)
};

struct RenderContext
{
	unsigned int flags;
	unsigned int maxuniforms;
	unsigned int maxuniformblock;
	unsigned int uniformblockalignment;
	float version;
	float glslversion;
	int max_texturesize;
	char * vendorstring;

	int MaxLights() const
	{
		return maxuniforms>=2048? 128:64;
	}
};

extern RenderContext gl;

#endif


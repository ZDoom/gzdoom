#ifndef R_RENDER
#define R_RENDER

#include "basictypes.h"

enum RenderFlags
{
	// [BB] Added texture compression flags.
	RFL_TEXTURE_COMPRESSION=1,
	RFL_TEXTURE_COMPRESSION_S3TC=2,

	RFL_SEPARATE_SHADER_OBJECTS = 4,	// we need this extension for glProgramUniform. On hardware not supporting it we need some rather clumsy workarounds
	RFL_BUFFER_STORAGE = 8,				// allows persistently mapped buffers, which are the only efficient way to actually use a dynamic vertex buffer. If this isn't present, a workaround with uniform arrays is used.
	RFL_SHADER_STORAGE_BUFFER = 16,		// to be used later for a parameter buffer
	RFL_BASEINDEX = 32,					// currently unused
	RFL_COREPROFILE = 64,
	RFL_NOBUFFER = 128,					// the static buffer makes no sense on GL 3.x AMD and Intel hardware, as long as compatibility mode is on
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


#ifndef R_RENDER
#define R_RENDER

#include "basictypes.h"

enum RenderFlags
{
	// [BB] Added texture compression flags.
	RFL_TEXTURE_COMPRESSION=8,
	RFL_TEXTURE_COMPRESSION_S3TC=16,

	RFL_MAP_BUFFER_RANGE = 64,
	RFL_FRAMEBUFFER = 128,
	RFL_TEXTUREBUFFER = 256,
	RFL_NVIDIA = 512,
	RFL_ATI = 1024,


	RFL_GL_20 = 0x10000000,
	RFL_GL_21 = 0x20000000,
	RFL_GL_30 = 0x40000000,
};

enum TexMode
{
	TMF_MASKBIT = 1,
	TMF_OPAQUEBIT = 2,
	TMF_INVERTBIT = 4,

	TM_MODULATE = 0,
	TM_MASK = TMF_MASKBIT,
	TM_OPAQUE = TMF_OPAQUEBIT,
	TM_INVERT = TMF_INVERTBIT,
	//TM_INVERTMASK = TMF_MASKBIT | TMF_INVERTBIT
	TM_INVERTOPAQUE = TMF_INVERTBIT | TMF_OPAQUEBIT,
};

struct RenderContext
{
	unsigned int flags;
	unsigned int shadermodel;
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


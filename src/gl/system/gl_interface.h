#ifndef R_RENDER
#define R_RENDER

#include "basictypes.h"

enum GLCompat
{
	CMPT_GL2,
	CMPT_GL2_SHADER,
	CMPT_GL3,
	CMPT_GL4
};

enum RenderFlags
{
	// [BB] Added texture compression flags.
	RFL_TEXTURE_COMPRESSION=1,
	RFL_TEXTURE_COMPRESSION_S3TC=2,

	RFL_SHADER_STORAGE_BUFFER = 4,
	RFL_BUFFER_STORAGE = 8,
	RFL_SAMPLER_OBJECTS = 16,

	RFL_NO_RGBA16F = 32,
	RFL_NO_DEPTHSTENCIL = 64,
	RFL_NO_CLIP_PLANES = 128
};

enum TexMode
{
	TM_MODULATE = 0,	// (r, g, b, a)
	TM_MASK,			// (1, 1, 1, a)
	TM_OPAQUE,			// (r, g, b, 1)
	TM_INVERSE,			// (1-r, 1-g, 1-b, a)
	TM_REDTOALPHA,		// (1, 1, 1, r)
	TM_CLAMPY,			// (r, g, b, (t >= 0.0 && t <= 1.0)? a:0)

	TM_INVERTOPAQUE,	// used by GL 2.x fallback code.
};

enum ELightMethod
{
	LM_SOFTWARE = 0,	// multi-pass texturing
	LM_DEFERRED = 1,	// calculate lights up front in a separate pass
	LM_DIRECT = 2,		// calculate lights on the fly along with the render data
};

enum EBufferMethod
{
	BM_CLIENTARRAY = 0,	// use a client array instead of a hardware buffer
	BM_DEFERRED = 1,	// use a temporarily mapped buffer (only necessary on GL 3.x core profile, i.e. Apple)
	BM_PERSISTENT = 2	// use a persistently mapped buffer
};


struct RenderContext
{
	unsigned int flags;
	unsigned int maxuniforms;
	unsigned int maxuniformblock;
	unsigned int uniformblockalignment;
	int lightmethod;
	int buffermethod;
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


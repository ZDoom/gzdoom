#ifndef R_RENDER
#define R_RENDER

#include "basictypes.h"

enum TexMode
{
	TM_MODULATE = 0,	// (r, g, b, a)
	TM_MASK,			// (1, 1, 1, a)
	TM_OPAQUE,			// (r, g, b, 1)
	TM_INVERSE,			// (1-r, 1-g, 1-b, a)
	TM_REDTOALPHA,		// (1, 1, 1, r)
	TM_CLAMPY,			// (r, g, b, (t >= 0.0 && t <= 1.0)? a:0)
	TM_INVERTOPAQUE,	// (1-r, 1-g, 1-b, 1)
	TM_FOGLAYER,		// (renders a fog layer in the shape of the active texture)
	TM_FIXEDCOLORMAP = TM_FOGLAYER,	// repurposes the objectcolor uniforms to render a fixed colormap range. (Same constant because they cannot be used in the same context.
};

enum ELightMethod
{
	LM_DEFERRED = 1,	// calculate lights up front in a separate pass
	LM_DIRECT = 2,		// calculate lights on the fly along with the render data
};

enum EBufferMethod
{
	BM_DEFERRED = 1,	// use a temporarily mapped buffer, for GL 3.x core profile
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
	float glslversion;
	int max_texturesize;
	char * vendorstring;
	char * modelstring;
};

extern RenderContext gl;

#endif


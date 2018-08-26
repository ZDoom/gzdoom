#ifndef R_RENDER
#define R_RENDER

#include "basictypes.h"

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
	int buffermethod;
	float glslversion;
	int max_texturesize;
	char * vendorstring;
	char * modelstring;
};

extern RenderContext gl;

#endif


#ifndef R_RENDER
#define R_RENDER

#include "m_argv.h"
struct RenderContext
{
	unsigned int flags;
	unsigned int maxuniforms;
	unsigned int maxuniformblock;
	unsigned int uniformblockalignment;
	float glslversion;
	int max_texturesize;
	char * vendorstring;
	char * modelstring;
};

extern RenderContext gl;

EXTERN_FARG(glversion);

#endif


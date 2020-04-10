#ifndef R_RENDER
#define R_RENDER

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

#endif


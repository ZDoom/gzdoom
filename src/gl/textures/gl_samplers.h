#ifndef __GL_SAMPLERS_H
#define __GL_SAMPLERS_H

#include "gl_hwtexture.h"

namespace OpenGLRenderer
{

class FSamplerManager
{
	// We need 6 different samplers: 4 for the different clamping modes,
	// one for 2D-textures and one for voxel textures
	unsigned int mSamplers[7];

	void UnbindAll();

public:

	FSamplerManager();
	~FSamplerManager();

	uint8_t Bind(int texunit, int num, int lastval);
	void SetTextureFilterMode();


};

}
#endif


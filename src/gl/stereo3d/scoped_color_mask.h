#ifndef GL_STEREO3D_SCOPED_COLOR_MASK_H_
#define GL_STEREO3D_SCOPED_COLOR_MASK_H_

#include "gl/system/gl_system.h"

/**
* Temporarily change color mask
*/
class ScopedColorMask
{
public:
	ScopedColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a) 
	{
		glGetBooleanv(GL_COLOR_WRITEMASK, saved);
		glColorMask(r, g, b, a);
	}
	~ScopedColorMask() {
		glColorMask(saved[0], saved[1], saved[2], saved[3]);
	}
private:
	GLboolean saved[4];
};


#endif // GL_STEREO3D_SCOPED_COLOR_MASK_H_

#ifndef GL_STEREO3D_SCOPED_COLOR_MASK_H_
#define GL_STEREO3D_SCOPED_COLOR_MASK_H_

#include "gl/glew.h"

/**
* Temporarily change color mask
*/
class ScopedColorMask
{
public:
	ScopedColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a) 
		: isPushed(false)
	{
		setColorMask(r, g, b, a);
	}
	~ScopedColorMask() {
		revert();
	}
	void setColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a) {
		if (!isPushed) {
			glPushAttrib(GL_COLOR_BUFFER_BIT);
			isPushed = true;
		}
		glColorMask(r, g, b, a);
	}
	void revert() {
		if (isPushed) {
			glPopAttrib();
			isPushed = false;
		}
	}
private:
	bool isPushed;
};


#endif // GL_STEREO3D_SCOPED_COLOR_MASK_H_

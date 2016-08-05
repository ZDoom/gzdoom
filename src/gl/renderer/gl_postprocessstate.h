#ifndef __GL_POSTPROCESSSTATE_H
#define __GL_POSTPROCESSSTATE_H

#include <string.h>
#include "gl/system/gl_interface.h"
#include "gl/data/gl_data.h"
#include "gl/data/gl_matrix.h"
#include "c_cvars.h"
#include "r_defs.h"

class FGLPostProcessState
{
public:
	FGLPostProcessState();
	~FGLPostProcessState();

private:
	FGLPostProcessState(const FGLPostProcessState &) = delete;
	FGLPostProcessState &operator=(const FGLPostProcessState &) = delete;

	GLint activeTex;
	GLint textureBinding;
	GLint samplerBinding;
	GLboolean blendEnabled;
	GLboolean scissorEnabled;
	GLboolean depthEnabled;
	GLboolean multisampleEnabled;
	GLint currentProgram;
	GLint blendEquationRgb;
	GLint blendEquationAlpha;
	GLint blendSrcRgb;
	GLint blendSrcAlpha;
	GLint blendDestRgb;
	GLint blendDestAlpha;
};

#endif

#ifndef __GL_POSTPROCESSSTATE_H
#define __GL_POSTPROCESSSTATE_H

#include <string.h>
#include "gl_load/gl_interface.h"
#include "r_data/matrix.h"
#include "c_cvars.h"
#include "r_defs.h"

class FGLPostProcessState
{
public:
	FGLPostProcessState();
	~FGLPostProcessState();

	void SaveTextureBindings(unsigned int numUnits);

private:
	FGLPostProcessState(const FGLPostProcessState &) = delete;
	FGLPostProcessState &operator=(const FGLPostProcessState &) = delete;

	GLint activeTex;
	TArray<GLint> textureBinding;
	TArray<GLint> samplerBinding;
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

#ifndef __GL_RENDERBUFFERS_H
#define __GL_RENDERBUFFERS_H

#include "gl/shaders/gl_shader.h"

class FGLRenderBuffers
{
public:
	FGLRenderBuffers();
	~FGLRenderBuffers();

	void Setup(int width, int height);
	void BindSceneFB();
	void BindOutputFB();
	void BindSceneTexture(int index);

	static bool IsSupported() { return gl.version >= 3.3f; }

private:
	int mWidth = 0;
	int mHeight = 0;

	GLuint mSceneTexture = 0;
	GLuint mSceneDepthStencil = 0;
	GLuint mSceneFB = 0;
	GLuint mOutputFB = 0;
};

#endif
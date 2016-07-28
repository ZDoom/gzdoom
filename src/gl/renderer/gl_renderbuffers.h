#ifndef __GL_RENDERBUFFERS_H
#define __GL_RENDERBUFFERS_H

#include "gl/shaders/gl_shader.h"

class FGLBloomTextureLevel
{
public:
	GLuint VTexture = 0;
	GLuint VFramebuffer = 0;
	GLuint HTexture = 0;
	GLuint HFramebuffer = 0;
	GLuint Width = 0;
	GLuint Height = 0;
};

class FGLRenderBuffers
{
public:
	FGLRenderBuffers();
	~FGLRenderBuffers();

	void Setup(int width, int height);
	void BindSceneFB();
	void BindHudFB();
	void BindOutputFB();
	void BindSceneTexture(int index);
	void BindHudTexture(int index);

	static bool IsSupported() { return gl.version >= 3.3f; }

	enum { NumBloomLevels = 4 };
	FGLBloomTextureLevel BloomLevels[NumBloomLevels];

private:
	void Clear();

	int mWidth = 0;
	int mHeight = 0;

	GLuint mSceneTexture = 0;
	GLuint mSceneDepthStencil = 0;
	GLuint mSceneFB = 0;
	GLuint mHudTexture = 0;
	GLuint mHudFB = 0;
	GLuint mOutputFB = 0;
};

#endif
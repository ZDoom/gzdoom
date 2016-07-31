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
	void BlitSceneToTexture();
	void BindSceneFB();
	void BindSceneTextureFB();
	void BindHudFB();
	void BindOutputFB();
	void BindSceneTexture(int index);
	void BindHudTexture(int index);

	enum { NumBloomLevels = 4 };
	FGLBloomTextureLevel BloomLevels[NumBloomLevels];

	static bool IsEnabled();

private:
	void ClearScene();
	void ClearHud();
	void ClearBloom();
	void CreateScene(int width, int height, int samples);
	void CreateHud(int width, int height);
	void CreateBloom(int width, int height);
	GLuint Create2DTexture(GLuint format, int width, int height);
	GLuint CreateRenderBuffer(GLuint format, int width, int height);
	GLuint CreateRenderBuffer(GLuint format, int samples, int width, int height);
	GLuint CreateFrameBuffer(GLuint colorbuffer);
	GLuint CreateFrameBuffer(GLuint colorbuffer, GLuint depthstencil, bool colorIsARenderBuffer);
	GLuint CreateFrameBuffer(GLuint colorbuffer, GLuint depth, GLuint stencil, bool colorIsARenderBuffer);
	void CheckFrameBufferCompleteness();
	void DeleteTexture(GLuint &handle);
	void DeleteRenderBuffer(GLuint &handle);
	void DeleteFrameBuffer(GLuint &handle);

	int GetCvarSamples();
	GLuint GetHdrFormat();

	int mWidth = 0;
	int mHeight = 0;
	int mSamples = 0;

	GLuint mSceneTexture = 0;
	GLuint mSceneMultisample = 0;
	GLuint mSceneDepthStencil = 0;
	GLuint mSceneDepth = 0;
	GLuint mSceneStencil = 0;
	GLuint mSceneFB = 0;
	GLuint mSceneTextureFB = 0;
	GLuint mHudTexture = 0;
	GLuint mHudFB = 0;
	GLuint mOutputFB = 0;
};

#endif
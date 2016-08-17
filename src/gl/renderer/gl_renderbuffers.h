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

	void Setup(int width, int height, int sceneWidth, int sceneHeight);

	void BindSceneFB();
	void BlitSceneToTexture();

	void BindCurrentTexture(int index);
	void BindCurrentFB();
	void BindNextFB();
	void NextTexture();

	void BindOutputFB();

	enum { NumBloomLevels = 4 };
	FGLBloomTextureLevel BloomLevels[NumBloomLevels];

	static bool IsEnabled();

	int GetWidth() const { return mWidth; }
	int GetHeight() const { return mHeight; }

private:
	void ClearScene();
	void ClearPipeline();
	void ClearBloom();
	void CreateScene(int width, int height, int samples);
	void CreatePipeline(int width, int height);
	void CreateBloom(int width, int height);
	GLuint Create2DTexture(const FString &name, GLuint format, int width, int height);
	GLuint CreateRenderBuffer(const FString &name, GLuint format, int width, int height);
	GLuint CreateRenderBuffer(const FString &name, GLuint format, int samples, int width, int height);
	GLuint CreateFrameBuffer(const FString &name, GLuint colorbuffer);
	GLuint CreateFrameBuffer(const FString &name, GLuint colorbuffer, GLuint depthstencil, bool colorIsARenderBuffer);
	GLuint CreateFrameBuffer(const FString &name, GLuint colorbuffer, GLuint depth, GLuint stencil, bool colorIsARenderBuffer);
	void CheckFrameBufferCompleteness();
	void ClearFrameBuffer(bool stencil, bool depth);
	void DeleteTexture(GLuint &handle);
	void DeleteRenderBuffer(GLuint &handle);
	void DeleteFrameBuffer(GLuint &handle);

	GLuint GetHdrFormat();

	int mWidth = 0;
	int mHeight = 0;
	int mSamples = 0;
	int mMaxSamples = 0;
	int mBloomWidth = 0;
	int mBloomHeight = 0;

	static const int NumPipelineTextures = 2;
	int mCurrentPipelineTexture = 0;

	// Buffers for the scene
	GLuint mSceneMultisample = 0;
	GLuint mSceneDepthStencil = 0;
	GLuint mSceneDepth = 0;
	GLuint mSceneStencil = 0;
	GLuint mSceneFB = 0;

	// Effect/HUD buffers
	GLuint mPipelineTexture[NumPipelineTextures];
	GLuint mPipelineFB[NumPipelineTextures];

	// Back buffer frame buffer
	GLuint mOutputFB = 0;
};

#endif
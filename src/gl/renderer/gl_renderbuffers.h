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

class FGLExposureTextureLevel
{
public:
	GLuint Texture = 0;
	GLuint Framebuffer = 0;
	GLuint Width = 0;
	GLuint Height = 0;
};

class FGLRenderBuffers
{
public:
	FGLRenderBuffers();
	~FGLRenderBuffers();

	bool Setup(int width, int height, int sceneWidth, int sceneHeight);

	void BindSceneFB(bool sceneData);
	void BindSceneColorTexture(int index);
	void BindSceneDataTexture(int index);
	void BindSceneDepthTexture(int index);
	void BlitSceneToTexture();

	void BindCurrentTexture(int index);
	void BindCurrentFB();
	void BindNextFB();
	void NextTexture();

	void BindOutputFB();

	void BlitToEyeTexture(int eye);
	void BindEyeTexture(int eye, int texunit);
	void BindEyeFB(int eye, bool readBuffer = false);

	enum { NumBloomLevels = 4 };
	FGLBloomTextureLevel BloomLevels[NumBloomLevels];

	TArray<FGLExposureTextureLevel> ExposureLevels;
	GLuint ExposureTexture = 0;
	GLuint ExposureFB = 0;
	bool FirstExposureFrame = true;

	// Ambient occlusion buffers
	GLuint AmbientTexture0 = 0;
	GLuint AmbientTexture1 = 0;
	GLuint AmbientFB0 = 0;
	GLuint AmbientFB1 = 0;
	int AmbientWidth = 0;
	int AmbientHeight = 0;
	GLuint AmbientRandomTexture = 0;

	static bool IsEnabled();

	int GetWidth() const { return mWidth; }
	int GetHeight() const { return mHeight; }

private:
	void ClearScene();
	void ClearPipeline();
	void ClearEyeBuffers();
	void ClearBloom();
	void ClearExposureLevels();
	void ClearAmbientOcclusion();
	void CreateScene(int width, int height, int samples);
	void CreatePipeline(int width, int height);
	void CreateBloom(int width, int height);
	void CreateExposureLevels(int width, int height);
	void CreateEyeBuffers(int eye);
	void CreateAmbientOcclusion(int width, int height);
	GLuint Create2DTexture(const FString &name, GLuint format, int width, int height, const void *data = nullptr);
	GLuint Create2DMultisampleTexture(const FString &name, GLuint format, int width, int height, int samples, bool fixedSampleLocations);
	GLuint CreateRenderBuffer(const FString &name, GLuint format, int width, int height);
	GLuint CreateRenderBuffer(const FString &name, GLuint format, int samples, int width, int height);
	GLuint CreateFrameBuffer(const FString &name, GLuint colorbuffer);
	GLuint CreateFrameBuffer(const FString &name, GLuint colorbuffer0, GLuint colorbuffer1, GLuint depthstencil, bool multisample);
	bool CheckFrameBufferCompleteness();
	void ClearFrameBuffer(bool stencil, bool depth);
	void DeleteTexture(GLuint &handle);
	void DeleteRenderBuffer(GLuint &handle);
	void DeleteFrameBuffer(GLuint &handle);

	int mWidth = 0;
	int mHeight = 0;
	int mSamples = 0;
	int mMaxSamples = 0;
	int mSceneWidth = 0;
	int mSceneHeight = 0;

	static const int NumPipelineTextures = 2;
	int mCurrentPipelineTexture = 0;

	// Buffers for the scene
	GLuint mSceneMultisample = 0;
	GLuint mSceneDepthStencil = 0;
	GLuint mSceneData = 0;
	GLuint mSceneFB = 0;
	GLuint mSceneDataFB = 0;

	// Effect/HUD buffers
	GLuint mPipelineTexture[NumPipelineTextures];
	GLuint mPipelineFB[NumPipelineTextures];

	// Back buffer frame buffer
	GLuint mOutputFB = 0;

	// Eye buffers
	TArray<GLuint> mEyeTextures;
	TArray<GLuint> mEyeFBs;

	static bool FailedCreate;
	static bool BuffersActive;
};

#endif
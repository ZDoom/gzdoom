#ifndef __GL_RENDERBUFFERS_H
#define __GL_RENDERBUFFERS_H

#include "gl/shaders/gl_shader.h"

class PPTexture
{
public:
	void Bind(int index, int filter = GL_NEAREST, int wrap = GL_CLAMP_TO_EDGE)
	{
		glActiveTexture(GL_TEXTURE0 + index);
		glBindTexture(GL_TEXTURE_2D, handle);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
	}

private:
	GLuint handle = 0;

	friend class FGLRenderBuffers;
};

class PPFrameBuffer
{
public:
	void Bind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, handle);
	}

private:
	GLuint handle = 0;

	friend class FGLRenderBuffers;
};

class PPRenderBuffer
{
private:
	GLuint handle = 0;

	friend class FGLRenderBuffers;
};

class FGLBloomTextureLevel
{
public:
	PPTexture VTexture;
	PPFrameBuffer VFramebuffer;
	PPTexture HTexture;
	PPFrameBuffer HFramebuffer;
	int Width = 0;
	int Height = 0;
};

class FGLExposureTextureLevel
{
public:
	PPTexture Texture;
	PPFrameBuffer Framebuffer;
	int Width = 0;
	int Height = 0;
};

class FGLRenderBuffers
{
public:
	FGLRenderBuffers();
	~FGLRenderBuffers();

	bool Setup(int width, int height, int sceneWidth, int sceneHeight);

	void BindSceneFB(bool sceneData);
	void BindSceneColorTexture(int index);
	void BindSceneFogTexture(int index);
	void BindSceneNormalTexture(int index);
	void BindSceneDepthTexture(int index);
	void BlitSceneToTexture();

	void BlitLinear(PPFrameBuffer src, PPFrameBuffer dest, int sx0, int sy0, int sx1, int sy1, int dx0, int dy0, int dx1, int dy1);

	void BindCurrentTexture(int index, int filter = GL_NEAREST, int wrap = GL_CLAMP_TO_EDGE);
	void BindCurrentFB();
	void BindNextFB();
	void NextTexture();

	PPFrameBuffer GetCurrentFB() const { return mPipelineFB[mCurrentPipelineTexture]; }

	void BindOutputFB();

	void BlitToEyeTexture(int eye);
	void BindEyeTexture(int eye, int texunit);
	void BindEyeFB(int eye, bool readBuffer = false);

	void BindShadowMapFB();
	void BindShadowMapTexture(int index);

	enum { NumBloomLevels = 4 };
	FGLBloomTextureLevel BloomLevels[NumBloomLevels];

	TArray<FGLExposureTextureLevel> ExposureLevels;
	PPTexture ExposureTexture;
	PPFrameBuffer ExposureFB;
	bool FirstExposureFrame = true;

	// Ambient occlusion buffers
	PPTexture LinearDepthTexture;
	PPFrameBuffer LinearDepthFB;
	PPTexture AmbientTexture0;
	PPTexture AmbientTexture1;
	PPFrameBuffer AmbientFB0;
	PPFrameBuffer AmbientFB1;
	int AmbientWidth = 0;
	int AmbientHeight = 0;
	enum { NumAmbientRandomTextures = 3 };
	PPTexture AmbientRandomTexture[NumAmbientRandomTextures];

	static bool IsEnabled();

	int GetWidth() const { return mWidth; }
	int GetHeight() const { return mHeight; }

	int GetSceneWidth() const { return mSceneWidth; }
	int GetSceneHeight() const { return mSceneHeight; }

private:
	void ClearScene();
	void ClearPipeline();
	void ClearEyeBuffers();
	void ClearBloom();
	void ClearExposureLevels();
	void ClearAmbientOcclusion();
	void ClearShadowMap();
	void CreateScene(int width, int height, int samples, bool needsSceneTextures);
	void CreatePipeline(int width, int height);
	void CreateBloom(int width, int height);
	void CreateExposureLevels(int width, int height);
	void CreateEyeBuffers(int eye);
	void CreateShadowMap();
	void CreateAmbientOcclusion(int width, int height);

	PPTexture Create2DTexture(const char *name, GLuint format, int width, int height, const void *data = nullptr);
	PPTexture Create2DMultisampleTexture(const char *name, GLuint format, int width, int height, int samples, bool fixedSampleLocations);
	PPRenderBuffer CreateRenderBuffer(const char *name, GLuint format, int width, int height);
	PPRenderBuffer CreateRenderBuffer(const char *name, GLuint format, int width, int height, int samples);
	PPFrameBuffer CreateFrameBuffer(const char *name, PPTexture colorbuffer);
	PPFrameBuffer CreateFrameBuffer(const char *name, PPTexture colorbuffer, PPRenderBuffer depthstencil);
	PPFrameBuffer CreateFrameBuffer(const char *name, PPRenderBuffer colorbuffer, PPRenderBuffer depthstencil);
	PPFrameBuffer CreateFrameBuffer(const char *name, PPTexture colorbuffer0, PPTexture colorbuffer1, PPTexture colorbuffer2, PPTexture depthstencil, bool multisample);
	bool CheckFrameBufferCompleteness();
	void ClearFrameBuffer(bool stencil, bool depth);
	void DeleteTexture(PPTexture &handle);
	void DeleteRenderBuffer(PPRenderBuffer &handle);
	void DeleteFrameBuffer(PPFrameBuffer &handle);

	int mWidth = 0;
	int mHeight = 0;
	int mSamples = 0;
	int mMaxSamples = 0;
	int mSceneWidth = 0;
	int mSceneHeight = 0;

	static const int NumPipelineTextures = 2;
	int mCurrentPipelineTexture = 0;

	// Buffers for the scene
	PPTexture mSceneMultisampleTex;
	PPTexture mSceneDepthStencilTex;
	PPTexture mSceneFogTex;
	PPTexture mSceneNormalTex;
	PPRenderBuffer mSceneMultisampleBuf;
	PPRenderBuffer mSceneDepthStencilBuf;
	PPRenderBuffer mSceneFogBuf;
	PPRenderBuffer mSceneNormalBuf;
	PPFrameBuffer mSceneFB;
	PPFrameBuffer mSceneDataFB;
	bool mSceneUsesTextures = false;

	// Effect/HUD buffers
	PPTexture mPipelineTexture[NumPipelineTextures];
	PPFrameBuffer mPipelineFB[NumPipelineTextures];

	// Eye buffers
	TArray<PPTexture> mEyeTextures;
	TArray<PPFrameBuffer> mEyeFBs;

	// Shadow map texture
	PPTexture mShadowMapTexture;
	PPFrameBuffer mShadowMapFB;
	int mCurrentShadowMapSize = 0;

	static bool FailedCreate;
	static bool BuffersActive;
};

#endif

#pragma once

#include "gl/shaders/gl_shader.h"
#include "hwrenderer/postprocessing/hw_postprocess.h"

class PPGLTexture
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

	int Width = -1;
	int Height = -1;

	explicit operator bool() const { return handle != 0; }

private:
	GLuint handle = 0;

	friend class FGLRenderBuffers;
};

class PPGLFrameBuffer
{
public:
	void Bind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, handle);
	}

	explicit operator bool() const { return handle != 0; }

private:
	GLuint handle = 0;

	friend class FGLRenderBuffers;
};

class PPGLRenderBuffer
{
private:
	GLuint handle = 0;

	explicit operator bool() const { return handle != 0; }

	friend class FGLRenderBuffers;
};

class FShaderProgram;

class FGLRenderBuffers
{
public:
	FGLRenderBuffers();
	~FGLRenderBuffers();

	void Setup(int width, int height, int sceneWidth, int sceneHeight);

	void UpdateEffectTextures();
	void CompileEffectShaders();
	void RenderEffect(const FString &name);

	void BindSceneFB(bool sceneData);
	void BindSceneColorTexture(int index);
	void BindSceneFogTexture(int index);
	void BindSceneNormalTexture(int index);
	void BindSceneDepthTexture(int index);
	void BlitSceneToTexture();

	void BindCurrentTexture(int index, int filter = GL_NEAREST, int wrap = GL_CLAMP_TO_EDGE);
	void BindCurrentFB();
	void BindNextFB();
	void NextTexture();

	PPGLFrameBuffer GetCurrentFB() const { return mPipelineFB[mCurrentPipelineTexture]; }

	void BindOutputFB();

	void BlitToEyeTexture(int eye);
	void BindEyeTexture(int eye, int texunit);
	void BindEyeFB(int eye, bool readBuffer = false);

	void BindShadowMapFB();
	void BindShadowMapTexture(int index);

	int GetWidth() const { return mWidth; }
	int GetHeight() const { return mHeight; }

	int GetSceneWidth() const { return mSceneWidth; }
	int GetSceneHeight() const { return mSceneHeight; }

private:
	void ClearScene();
	void ClearPipeline();
	void ClearEyeBuffers();
	void ClearShadowMap();
	void CreateScene(int width, int height, int samples, bool needsSceneTextures);
	void CreatePipeline(int width, int height);
	void CreateEyeBuffers(int eye);
	void CreateShadowMap();

	PPGLTexture Create2DTexture(const char *name, GLuint format, int width, int height, const void *data = nullptr);
	PPGLTexture Create2DMultisampleTexture(const char *name, GLuint format, int width, int height, int samples, bool fixedSampleLocations);
	PPGLRenderBuffer CreateRenderBuffer(const char *name, GLuint format, int width, int height);
	PPGLRenderBuffer CreateRenderBuffer(const char *name, GLuint format, int width, int height, int samples);
	PPGLFrameBuffer CreateFrameBuffer(const char *name, PPGLTexture colorbuffer);
	PPGLFrameBuffer CreateFrameBuffer(const char *name, PPGLTexture colorbuffer, PPGLRenderBuffer depthstencil);
	PPGLFrameBuffer CreateFrameBuffer(const char *name, PPGLRenderBuffer colorbuffer, PPGLRenderBuffer depthstencil);
	PPGLFrameBuffer CreateFrameBuffer(const char *name, PPGLTexture colorbuffer0, PPGLTexture colorbuffer1, PPGLTexture colorbuffer2, PPGLTexture depthstencil, bool multisample);
	bool CheckFrameBufferCompleteness();
	void ClearFrameBuffer(bool stencil, bool depth);
	void DeleteTexture(PPGLTexture &handle);
	void DeleteRenderBuffer(PPGLRenderBuffer &handle);
	void DeleteFrameBuffer(PPGLFrameBuffer &handle);

	int mWidth = 0;
	int mHeight = 0;
	int mSamples = 0;
	int mMaxSamples = 0;
	int mSceneWidth = 0;
	int mSceneHeight = 0;

	static const int NumPipelineTextures = 2;
	int mCurrentPipelineTexture = 0;

	// Buffers for the scene
	PPGLTexture mSceneMultisampleTex;
	PPGLTexture mSceneDepthStencilTex;
	PPGLTexture mSceneFogTex;
	PPGLTexture mSceneNormalTex;
	PPGLRenderBuffer mSceneMultisampleBuf;
	PPGLRenderBuffer mSceneDepthStencilBuf;
	PPGLRenderBuffer mSceneFogBuf;
	PPGLRenderBuffer mSceneNormalBuf;
	PPGLFrameBuffer mSceneFB;
	PPGLFrameBuffer mSceneDataFB;
	bool mSceneUsesTextures = false;

	// Effect/HUD buffers
	PPGLTexture mPipelineTexture[NumPipelineTextures];
	PPGLFrameBuffer mPipelineFB[NumPipelineTextures];

	// Eye buffers
	TArray<PPGLTexture> mEyeTextures;
	TArray<PPGLFrameBuffer> mEyeFBs;

	// Shadow map texture
	PPGLTexture mShadowMapTexture;
	PPGLFrameBuffer mShadowMapFB;
	int mCurrentShadowMapSize = 0;

	// Postprocess OpenGL objects
	TMap<PPTextureName, PPGLTexture> GLTextures;
	TMap<PPTextureName, PPGLFrameBuffer> GLTextureFBs;
	TMap<PPShaderName, std::shared_ptr<FShaderProgram>> GLShaders;

	static bool FailedCreate;
};

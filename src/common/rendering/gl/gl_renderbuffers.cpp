/*
**  Postprocessing framework
**  Copyright (c) 2016-2020 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
*/

#include "gl_system.h"
#include "v_video.h"
#include "gl_interface.h"
#include "printf.h"
#include "hw_cvars.h"
#include "gl_debug.h"
#include "gl_renderer.h"
#include "gl_renderbuffers.h"
#include "gl_postprocessstate.h"
#include "gl_shaderprogram.h"
#include "gl_buffers.h"
#include "templates.h"
#include <random>

EXTERN_CVAR(Int, gl_debug_level)
CVAR(Int, gl_multisample, 1, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);

namespace OpenGLRenderer
{

//==========================================================================
//
// Initialize render buffers and textures used in rendering passes
//
//==========================================================================

FGLRenderBuffers::FGLRenderBuffers()
{
	glGetIntegerv(GL_MAX_SAMPLES, &mMaxSamples);
}

//==========================================================================
//
// Free render buffer resources
//
//==========================================================================

FGLRenderBuffers::~FGLRenderBuffers()
{
	ClearScene();
	ClearPipeline();
	ClearEyeBuffers();
	ClearShadowMap();
	DeleteTexture(mDitherTexture);
}

void FGLRenderBuffers::ClearScene()
{
	DeleteFrameBuffer(mSceneFB);
	DeleteFrameBuffer(mSceneDataFB);
	if (mSceneUsesTextures)
	{
		DeleteTexture(mSceneMultisampleTex);
		DeleteTexture(mSceneFogTex);
		DeleteTexture(mSceneNormalTex);
		DeleteTexture(mSceneDepthStencilTex);
	}
	else
	{
		DeleteRenderBuffer(mSceneMultisampleBuf);
		DeleteRenderBuffer(mSceneFogBuf);
		DeleteRenderBuffer(mSceneNormalBuf);
		DeleteRenderBuffer(mSceneDepthStencilBuf);
	}
}

void FGLRenderBuffers::ClearPipeline()
{
	for (int i = 0; i < NumPipelineTextures; i++)
	{
		DeleteFrameBuffer(mPipelineFB[i]);
		DeleteTexture(mPipelineTexture[i]);
	}
}

void FGLRenderBuffers::ClearEyeBuffers()
{
	for (auto handle : mEyeFBs)
		DeleteFrameBuffer(handle);

	for (auto handle : mEyeTextures)
		DeleteTexture(handle);

	mEyeTextures.Clear();
	mEyeFBs.Clear();
}

void FGLRenderBuffers::DeleteTexture(PPGLTexture &tex)
{
	if (tex.handle != 0)
		glDeleteTextures(1, &tex.handle);
	tex.handle = 0;
}

void FGLRenderBuffers::DeleteRenderBuffer(PPGLRenderBuffer &buf)
{
	if (buf.handle != 0)
		glDeleteRenderbuffers(1, &buf.handle);
	buf.handle = 0;
}

void FGLRenderBuffers::DeleteFrameBuffer(PPGLFrameBuffer &fb)
{
	if (fb.handle != 0)
		glDeleteFramebuffers(1, &fb.handle);
	fb.handle = 0;
}

//==========================================================================
//
// Makes sure all render buffers have sizes suitable for rending at the
// specified resolution
//
//==========================================================================

void FGLRenderBuffers::Setup(int width, int height, int sceneWidth, int sceneHeight)
{
	if (width <= 0 || height <= 0)
		I_FatalError("Requested invalid render buffer sizes: screen = %dx%d", width, height);

	int samples = clamp((int)gl_multisample, 0, mMaxSamples);
	bool needsSceneTextures = (gl_ssao != 0);

	GLint activeTex;
	GLint textureBinding;
	glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTex);
	glActiveTexture(GL_TEXTURE0);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureBinding);

	if (width != mWidth || height != mHeight)
		CreatePipeline(width, height);

	if (width != mWidth || height != mHeight || mSamples != samples || mSceneUsesTextures != needsSceneTextures)
		CreateScene(width, height, samples, needsSceneTextures);

	mWidth = width;
	mHeight = height;
	mSamples = samples;
	mSceneUsesTextures = needsSceneTextures;
	mSceneWidth = sceneWidth;
	mSceneHeight = sceneHeight;

	glBindTexture(GL_TEXTURE_2D, textureBinding);
	glActiveTexture(activeTex);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (FailedCreate)
	{
		ClearScene();
		ClearPipeline();
		ClearEyeBuffers();
		mWidth = 0;
		mHeight = 0;
		mSamples = 0;
		mSceneWidth = 0;
		mSceneHeight = 0;
		I_FatalError("Unable to create render buffers.");
	}
}

//==========================================================================
//
// Creates the scene buffers
//
//==========================================================================

void FGLRenderBuffers::CreateScene(int width, int height, int samples, bool needsSceneTextures)
{
	ClearScene();

	if (samples > 1)
	{
		if (needsSceneTextures)
		{
			mSceneMultisampleTex = Create2DMultisampleTexture("SceneMultisample", GL_RGBA16F, width, height, samples, false);
			mSceneDepthStencilTex = Create2DMultisampleTexture("SceneDepthStencil", GL_DEPTH24_STENCIL8, width, height, samples, false);
			mSceneFogTex = Create2DMultisampleTexture("SceneFog", GL_RGBA8, width, height, samples, false);
			mSceneNormalTex = Create2DMultisampleTexture("SceneNormal", GL_RGB10_A2, width, height, samples, false);
			mSceneFB = CreateFrameBuffer("SceneFB", mSceneMultisampleTex, {}, {}, mSceneDepthStencilTex, true);
			mSceneDataFB = CreateFrameBuffer("SceneGBufferFB", mSceneMultisampleTex, mSceneFogTex, mSceneNormalTex, mSceneDepthStencilTex, true);
		}
		else
		{
			mSceneMultisampleBuf = CreateRenderBuffer("SceneMultisample", GL_RGBA16F, width, height, samples);
			mSceneDepthStencilBuf = CreateRenderBuffer("SceneDepthStencil", GL_DEPTH24_STENCIL8, width, height, samples);
			mSceneFB = CreateFrameBuffer("SceneFB", mSceneMultisampleBuf, mSceneDepthStencilBuf);
			mSceneDataFB = CreateFrameBuffer("SceneGBufferFB", mSceneMultisampleBuf, mSceneDepthStencilBuf);
		}
	}
	else
	{
		if (needsSceneTextures)
		{
			mSceneDepthStencilTex = Create2DTexture("SceneDepthStencil", GL_DEPTH24_STENCIL8, width, height);
			mSceneFogTex = Create2DTexture("SceneFog", GL_RGBA8, width, height);
			mSceneNormalTex = Create2DTexture("SceneNormal", GL_RGB10_A2, width, height);
			mSceneFB = CreateFrameBuffer("SceneFB", mPipelineTexture[0], {}, {}, mSceneDepthStencilTex, false);
			mSceneDataFB = CreateFrameBuffer("SceneGBufferFB", mPipelineTexture[0], mSceneFogTex, mSceneNormalTex, mSceneDepthStencilTex, false);
		}
		else
		{
			mSceneDepthStencilBuf = CreateRenderBuffer("SceneDepthStencil", GL_DEPTH24_STENCIL8, width, height);
			mSceneFB = CreateFrameBuffer("SceneFB", mPipelineTexture[0], mSceneDepthStencilBuf);
			mSceneDataFB = CreateFrameBuffer("SceneGBufferFB", mPipelineTexture[0], mSceneDepthStencilBuf);
		}
	}
}

//==========================================================================
//
// Creates the buffers needed for post processing steps
//
//==========================================================================

void FGLRenderBuffers::CreatePipeline(int width, int height)
{
	ClearPipeline();
	ClearEyeBuffers();

	for (int i = 0; i < NumPipelineTextures; i++)
	{
		mPipelineTexture[i] = Create2DTexture("PipelineTexture", GL_RGBA16F, width, height);
		mPipelineFB[i] = CreateFrameBuffer("PipelineFB", mPipelineTexture[i]);
	}
}

//==========================================================================
//
// Creates eye buffers if needed
//
//==========================================================================

void FGLRenderBuffers::CreateEyeBuffers(int eye)
{
	if (mEyeFBs.Size() > unsigned(eye))
		return;

	GLint activeTex, textureBinding, frameBufferBinding;
	glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTex);
	glActiveTexture(GL_TEXTURE0);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureBinding);
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &frameBufferBinding);

	while (mEyeFBs.Size() <= unsigned(eye))
	{
		PPGLTexture texture = Create2DTexture("EyeTexture", GL_RGBA16F, mWidth, mHeight);
		mEyeTextures.Push(texture);
		mEyeFBs.Push(CreateFrameBuffer("EyeFB", texture));
	}

	glBindTexture(GL_TEXTURE_2D, textureBinding);
	glActiveTexture(activeTex);
	glBindFramebuffer(GL_FRAMEBUFFER, frameBufferBinding);
}

//==========================================================================
//
// Creates a 2D texture defaulting to linear filtering and clamp to edge
//
//==========================================================================

PPGLTexture FGLRenderBuffers::Create2DTexture(const char *name, GLuint format, int width, int height, const void *data)
{
	PPGLTexture tex;
	tex.Width = width;
	tex.Height = height;
	glGenTextures(1, &tex.handle);
	glBindTexture(GL_TEXTURE_2D, tex.handle);
	FGLDebug::LabelObject(GL_TEXTURE, tex.handle, name);

	GLenum dataformat = 0, datatype = 0;
	switch (format)
	{
	case GL_RGBA8:				dataformat = GL_RGBA; datatype = GL_UNSIGNED_BYTE; break;
	case GL_RGBA16:				dataformat = GL_RGBA; datatype = GL_UNSIGNED_SHORT; break;
	case GL_RGBA16F:			dataformat = GL_RGBA; datatype = GL_FLOAT; break;
	case GL_RGBA32F:			dataformat = GL_RGBA; datatype = GL_FLOAT; break;
	case GL_RGBA16_SNORM:		dataformat = GL_RGBA; datatype = GL_SHORT; break;
	case GL_R32F:				dataformat = GL_RED; datatype = GL_FLOAT; break;
	case GL_R16F:				dataformat = GL_RED; datatype = GL_FLOAT; break;
	case GL_RG32F:				dataformat = GL_RG; datatype = GL_FLOAT; break;
	case GL_RG16F:				dataformat = GL_RG; datatype = GL_FLOAT; break;
	case GL_RGB10_A2:			dataformat = GL_RGBA; datatype = GL_UNSIGNED_INT_10_10_10_2; break;
	case GL_DEPTH_COMPONENT24:	dataformat = GL_DEPTH_COMPONENT; datatype = GL_FLOAT; break;
	case GL_STENCIL_INDEX8:		dataformat = GL_STENCIL_INDEX; datatype = GL_INT; break;
	case GL_DEPTH24_STENCIL8:	dataformat = GL_DEPTH_STENCIL; datatype = GL_UNSIGNED_INT_24_8; break;
	default: I_FatalError("Unknown format passed to FGLRenderBuffers.Create2DTexture");
	}

	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, dataformat, datatype, data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	return tex;
}

PPGLTexture FGLRenderBuffers::Create2DMultisampleTexture(const char *name, GLuint format, int width, int height, int samples, bool fixedSampleLocations)
{
	PPGLTexture tex;
	tex.Width = width;
	tex.Height = height;
	glGenTextures(1, &tex.handle);
	glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, tex.handle);
	FGLDebug::LabelObject(GL_TEXTURE, tex.handle, name);
	glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, format, width, height, fixedSampleLocations);
	glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
	return tex;
}

//==========================================================================
//
// Creates a render buffer
//
//==========================================================================

PPGLRenderBuffer FGLRenderBuffers::CreateRenderBuffer(const char *name, GLuint format, int width, int height)
{
	PPGLRenderBuffer buf;
	glGenRenderbuffers(1, &buf.handle);
	glBindRenderbuffer(GL_RENDERBUFFER, buf.handle);
	FGLDebug::LabelObject(GL_RENDERBUFFER, buf.handle, name);
	glRenderbufferStorage(GL_RENDERBUFFER, format, width, height);
	return buf;
}

PPGLRenderBuffer FGLRenderBuffers::CreateRenderBuffer(const char *name, GLuint format, int width, int height, int samples)
{
	if (samples <= 1)
		return CreateRenderBuffer(name, format, width, height);

	PPGLRenderBuffer buf;
	glGenRenderbuffers(1, &buf.handle);
	glBindRenderbuffer(GL_RENDERBUFFER, buf.handle);
	FGLDebug::LabelObject(GL_RENDERBUFFER, buf.handle, name);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, format, width, height);
	return buf;
}

//==========================================================================
//
// Creates a frame buffer
//
//==========================================================================

PPGLFrameBuffer FGLRenderBuffers::CreateFrameBuffer(const char *name, PPGLTexture colorbuffer)
{
	PPGLFrameBuffer fb;
	glGenFramebuffers(1, &fb.handle);
	glBindFramebuffer(GL_FRAMEBUFFER, fb.handle);
	FGLDebug::LabelObject(GL_FRAMEBUFFER, fb.handle, name);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorbuffer.handle, 0);
	if (CheckFrameBufferCompleteness())
		ClearFrameBuffer(false, false);
	return fb;
}

PPGLFrameBuffer FGLRenderBuffers::CreateFrameBuffer(const char *name, PPGLTexture colorbuffer, PPGLRenderBuffer depthstencil)
{
	PPGLFrameBuffer fb;
	glGenFramebuffers(1, &fb.handle);
	glBindFramebuffer(GL_FRAMEBUFFER, fb.handle);
	FGLDebug::LabelObject(GL_FRAMEBUFFER, fb.handle, name);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorbuffer.handle, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthstencil.handle);
	if (CheckFrameBufferCompleteness())
		ClearFrameBuffer(true, true);
	return fb;
}

PPGLFrameBuffer FGLRenderBuffers::CreateFrameBuffer(const char *name, PPGLRenderBuffer colorbuffer, PPGLRenderBuffer depthstencil)
{
	PPGLFrameBuffer fb;
	glGenFramebuffers(1, &fb.handle);
	glBindFramebuffer(GL_FRAMEBUFFER, fb.handle);
	FGLDebug::LabelObject(GL_FRAMEBUFFER, fb.handle, name);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorbuffer.handle);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthstencil.handle);
	if (CheckFrameBufferCompleteness())
		ClearFrameBuffer(true, true);
	return fb;
}

PPGLFrameBuffer FGLRenderBuffers::CreateFrameBuffer(const char *name, PPGLTexture colorbuffer0, PPGLTexture colorbuffer1, PPGLTexture colorbuffer2, PPGLTexture depthstencil, bool multisample)
{
	PPGLFrameBuffer fb;
	glGenFramebuffers(1, &fb.handle);
	glBindFramebuffer(GL_FRAMEBUFFER, fb.handle);
	FGLDebug::LabelObject(GL_FRAMEBUFFER, fb.handle, name);
	if (multisample)
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, colorbuffer0.handle, 0);
		if (colorbuffer1.handle != 0)
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D_MULTISAMPLE, colorbuffer1.handle, 0);
		if (colorbuffer2.handle != 0)
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D_MULTISAMPLE, colorbuffer2.handle, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, depthstencil.handle, 0);
	}
	else
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorbuffer0.handle, 0);
		if (colorbuffer1.handle != 0)
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, colorbuffer1.handle, 0);
		if (colorbuffer2.handle != 0)
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, colorbuffer2.handle, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthstencil.handle, 0);
	}
	if (CheckFrameBufferCompleteness())
		ClearFrameBuffer(true, true);
	return fb;
}

//==========================================================================
//
// Verifies that the frame buffer setup is valid
//
//==========================================================================

bool FGLRenderBuffers::CheckFrameBufferCompleteness()
{
	GLenum result = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (result == GL_FRAMEBUFFER_COMPLETE)
		return true;

	bool FailedCreate = true;

	if (gl_debug_level > 0)
	{
		FString error = "glCheckFramebufferStatus failed: ";
		switch (result)
		{
		default: error.AppendFormat("error code %d", (int)result); break;
		case GL_FRAMEBUFFER_UNDEFINED: error << "GL_FRAMEBUFFER_UNDEFINED"; break;
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: error << "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT"; break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: error << "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT"; break;
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER: error << "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER"; break;
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER: error << "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER"; break;
		case GL_FRAMEBUFFER_UNSUPPORTED: error << "GL_FRAMEBUFFER_UNSUPPORTED"; break;
		case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: error << "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE"; break;
		case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS: error << "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS"; break;
		}
		Printf("%s\n", error.GetChars());
	}

	return false;
}

//==========================================================================
//
// Clear frame buffer to make sure it never contains uninitialized data
//
//==========================================================================

void FGLRenderBuffers::ClearFrameBuffer(bool stencil, bool depth)
{
	GLboolean scissorEnabled;
	GLint stencilValue;
	GLdouble depthValue;
	glGetBooleanv(GL_SCISSOR_TEST, &scissorEnabled);
	glGetIntegerv(GL_STENCIL_CLEAR_VALUE, &stencilValue);
	glGetDoublev(GL_DEPTH_CLEAR_VALUE, &depthValue);
	glDisable(GL_SCISSOR_TEST);
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClearDepth(0.0);
	glClearStencil(0);
	GLenum flags = GL_COLOR_BUFFER_BIT;
	if (stencil)
		flags |= GL_STENCIL_BUFFER_BIT;
	if (depth)
		flags |= GL_DEPTH_BUFFER_BIT;
	glClear(flags);
	glClearStencil(stencilValue);
	glClearDepth(depthValue);
	if (scissorEnabled)
		glEnable(GL_SCISSOR_TEST);
}

//==========================================================================
//
// Resolves the multisample frame buffer by copying it to the first pipeline texture
//
//==========================================================================

void FGLRenderBuffers::BlitSceneToTexture()
{
	mCurrentPipelineTexture = 0;

	if (mSamples <= 1)
		return;

	glBindFramebuffer(GL_READ_FRAMEBUFFER, mSceneFB.handle);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mPipelineFB[mCurrentPipelineTexture].handle);
	glBlitFramebuffer(0, 0, mWidth, mHeight, 0, 0, mWidth, mHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

	if ((gl.flags & RFL_INVALIDATE_BUFFER) != 0)
	{
		GLenum attachments[2] = { GL_COLOR_ATTACHMENT0, GL_DEPTH_STENCIL_ATTACHMENT };
		glInvalidateFramebuffer(GL_READ_FRAMEBUFFER, 2, attachments);
	}

	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

//==========================================================================
//
// Eye textures and their frame buffers
//
//==========================================================================

void FGLRenderBuffers::BlitToEyeTexture(int eye, bool allowInvalidate)
{
	CreateEyeBuffers(eye);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, mPipelineFB[mCurrentPipelineTexture].handle);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mEyeFBs[eye].handle);
	glBlitFramebuffer(0, 0, mWidth, mHeight, 0, 0, mWidth, mHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

	if ((gl.flags & RFL_INVALIDATE_BUFFER) != 0 && allowInvalidate)
	{
		GLenum attachments[2] = { GL_COLOR_ATTACHMENT0, GL_DEPTH_STENCIL_ATTACHMENT };
		glInvalidateFramebuffer(GL_READ_FRAMEBUFFER, 2, attachments);
	}

	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void FGLRenderBuffers::BlitFromEyeTexture(int eye)
{
	if (mEyeFBs.Size() <= unsigned(eye)) return;

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mPipelineFB[mCurrentPipelineTexture].handle);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, mEyeFBs[eye].handle);
	glBlitFramebuffer(0, 0, mWidth, mHeight, 0, 0, mWidth, mHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

	if ((gl.flags & RFL_INVALIDATE_BUFFER) != 0)
	{
		GLenum attachments[2] = { GL_COLOR_ATTACHMENT0, GL_DEPTH_STENCIL_ATTACHMENT };
		glInvalidateFramebuffer(GL_READ_FRAMEBUFFER, 2, attachments);
	}

	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void FGLRenderBuffers::BindEyeTexture(int eye, int texunit)
{
	CreateEyeBuffers(eye);
	glActiveTexture(GL_TEXTURE0 + texunit);
	glBindTexture(GL_TEXTURE_2D, mEyeTextures[eye].handle);
}

void FGLRenderBuffers::BindDitherTexture(int texunit)
{
	if (!mDitherTexture)
	{
		static const float data[64] =
		{
			 .0078125, .2578125, .1328125, .3828125, .0234375, .2734375, .1484375, .3984375,
			 .7578125, .5078125, .8828125, .6328125, .7734375, .5234375, .8984375, .6484375,
			 .0703125, .3203125, .1953125, .4453125, .0859375, .3359375, .2109375, .4609375,
			 .8203125, .5703125, .9453125, .6953125, .8359375, .5859375, .9609375, .7109375,
			 .0390625, .2890625, .1640625, .4140625, .0546875, .3046875, .1796875, .4296875,
			 .7890625, .5390625, .9140625, .6640625, .8046875, .5546875, .9296875, .6796875,
			 .1015625, .3515625, .2265625, .4765625, .1171875, .3671875, .2421875, .4921875,
			 .8515625, .6015625, .9765625, .7265625, .8671875, .6171875, .9921875, .7421875,
		};

		glActiveTexture(GL_TEXTURE0 + texunit);
		mDitherTexture = Create2DTexture("DitherTexture", GL_R32F, 8, 8, data);
	}
	mDitherTexture.Bind(1, GL_NEAREST, GL_REPEAT);
}

//==========================================================================
//
// Shadow map texture and frame buffers
//
//==========================================================================

void FGLRenderBuffers::BindShadowMapFB()
{
	CreateShadowMap();
	glBindFramebuffer(GL_FRAMEBUFFER, mShadowMapFB.handle);
}

void FGLRenderBuffers::BindShadowMapTexture(int texunit)
{
	CreateShadowMap();
	glActiveTexture(GL_TEXTURE0 + texunit);
	glBindTexture(GL_TEXTURE_2D, mShadowMapTexture.handle);
}

void FGLRenderBuffers::ClearShadowMap()
{
	DeleteFrameBuffer(mShadowMapFB);
	DeleteTexture(mShadowMapTexture);
	mCurrentShadowMapSize = 0;
}

void FGLRenderBuffers::CreateShadowMap()
{
	if (mShadowMapTexture.handle != 0 && gl_shadowmap_quality == mCurrentShadowMapSize)
		return;

	ClearShadowMap();

	GLint activeTex, textureBinding, frameBufferBinding;
	glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTex);
	glActiveTexture(GL_TEXTURE0);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureBinding);
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &frameBufferBinding);

	mShadowMapTexture = Create2DTexture("ShadowMap", GL_R32F, gl_shadowmap_quality, 1024);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	mShadowMapFB = CreateFrameBuffer("ShadowMapFB", mShadowMapTexture);

	glBindTexture(GL_TEXTURE_2D, textureBinding);
	glActiveTexture(activeTex);
	glBindFramebuffer(GL_FRAMEBUFFER, frameBufferBinding);

	mCurrentShadowMapSize = gl_shadowmap_quality;
}

//==========================================================================
//
// Makes the scene frame buffer active (multisample, depth, stecil, etc.)
//
//==========================================================================

void FGLRenderBuffers::BindSceneFB(bool sceneData)
{
	glBindFramebuffer(GL_FRAMEBUFFER, sceneData ? mSceneDataFB.handle : mSceneFB.handle);
}

//==========================================================================
//
// Binds the scene color texture to the specified texture unit
//
//==========================================================================

void FGLRenderBuffers::BindSceneColorTexture(int index)
{
	glActiveTexture(GL_TEXTURE0 + index);
	if (mSamples > 1)
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mSceneMultisampleTex.handle);
	else
		glBindTexture(GL_TEXTURE_2D, mPipelineTexture[0].handle);
}

//==========================================================================
//
// Binds the scene fog data texture to the specified texture unit
//
//==========================================================================

void FGLRenderBuffers::BindSceneFogTexture(int index)
{
	glActiveTexture(GL_TEXTURE0 + index);
	if (mSamples > 1)
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mSceneFogTex.handle);
	else
		glBindTexture(GL_TEXTURE_2D, mSceneFogTex.handle);
}

//==========================================================================
//
// Binds the scene normal data texture to the specified texture unit
//
//==========================================================================

void FGLRenderBuffers::BindSceneNormalTexture(int index)
{
	glActiveTexture(GL_TEXTURE0 + index);
	if (mSamples > 1)
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mSceneNormalTex.handle);
	else
		glBindTexture(GL_TEXTURE_2D, mSceneNormalTex.handle);
}

//==========================================================================
//
// Binds the depth texture to the specified texture unit
//
//==========================================================================

void FGLRenderBuffers::BindSceneDepthTexture(int index)
{
	glActiveTexture(GL_TEXTURE0 + index);
	if (mSamples > 1)
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mSceneDepthStencilTex.handle);
	else
		glBindTexture(GL_TEXTURE_2D, mSceneDepthStencilTex.handle);
}

//==========================================================================
//
// Binds the current scene/effect/hud texture to the specified texture unit
//
//==========================================================================

void FGLRenderBuffers::BindCurrentTexture(int index, int filter, int wrap)
{
	mPipelineTexture[mCurrentPipelineTexture].Bind(index, filter, wrap);
}

//==========================================================================
//
// Makes the frame buffer for the current texture active 
//
//==========================================================================

void FGLRenderBuffers::BindCurrentFB()
{
	mPipelineFB[mCurrentPipelineTexture].Bind();
}

//==========================================================================
//
// Makes the frame buffer for the next texture active
//
//==========================================================================

void FGLRenderBuffers::BindNextFB()
{
	int out = (mCurrentPipelineTexture + 1) % NumPipelineTextures;
	mPipelineFB[out].Bind();
}

//==========================================================================
//
// Next pipeline texture now contains the output
//
//==========================================================================

void FGLRenderBuffers::NextTexture()
{
	mCurrentPipelineTexture = (mCurrentPipelineTexture + 1) % NumPipelineTextures;
}

//==========================================================================
//
// Makes the screen frame buffer active
//
//==========================================================================

void FGLRenderBuffers::BindOutputFB()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

//==========================================================================
//
// Returns true if render buffers are supported and should be used
//
//==========================================================================

bool FGLRenderBuffers::FailedCreate = false;

//==========================================================================
//
// Creates or updates textures used by postprocess effects
//
//==========================================================================

PPGLTextureBackend *GLPPRenderState::GetGLTexture(PPTexture *texture)
{
	if (!texture->Backend)
	{
		FGLPostProcessState savedState;

		auto backend = std::make_unique<PPGLTextureBackend>();

		int glformat;
		switch (texture->Format)
		{
		default:
		case PixelFormat::Rgba8: glformat = GL_RGBA8; break;
		case PixelFormat::Rgba16f: glformat = GL_RGBA16F; break;
		case PixelFormat::R32f: glformat = GL_R32F; break;
		case PixelFormat::Rg16f: glformat = GL_RG16F; break;
		case PixelFormat::Rgba16_snorm: glformat = GL_RGBA16_SNORM; break;
		}

		if (texture->Data)
			backend->Tex = buffers->Create2DTexture("PPTexture", glformat, texture->Width, texture->Height, texture->Data.get());
		else
			backend->Tex = buffers->Create2DTexture("PPTexture", glformat, texture->Width, texture->Height);

		texture->Backend = std::move(backend);
	}
	return static_cast<PPGLTextureBackend*>(texture->Backend.get());
}

//==========================================================================
//
// Compile the shaders declared by post process effects
//
//==========================================================================

FShaderProgram *GLPPRenderState::GetGLShader(PPShader *shader)
{
	if (!shader->Backend)
	{
		auto glshader = std::make_unique<FShaderProgram>();

		FString prolog;
		if (!shader->Uniforms.empty())
			prolog = UniformBlockDecl::Create("Uniforms", shader->Uniforms, POSTPROCESS_BINDINGPOINT);
		prolog += shader->Defines;

		glshader->Compile(FShaderProgram::Vertex, shader->VertexShader, "", shader->Version);
		glshader->Compile(FShaderProgram::Fragment, shader->FragmentShader, prolog, shader->Version);
		glshader->Link(shader->FragmentShader.GetChars());
		if (!shader->Uniforms.empty())
			glshader->SetUniformBufferLocation(POSTPROCESS_BINDINGPOINT, "Uniforms");

		shader->Backend = std::move(glshader);
	}
	return static_cast<FShaderProgram*>(shader->Backend.get());
}

//==========================================================================
//
// Renders one post process effect
//
//==========================================================================

void GLPPRenderState::Draw()
{
	FGLPostProcessState savedState;

	// Bind input textures
	for (unsigned int index = 0; index < Textures.Size(); index++)
	{
		savedState.SaveTextureBindings(index + 1);

		const PPTextureInput &input = Textures[index];
		int filter = (input.Filter == PPFilterMode::Nearest) ? GL_NEAREST : GL_LINEAR;
		int wrap = (input.Wrap == PPWrapMode::Clamp) ? GL_CLAMP_TO_EDGE : GL_REPEAT;

		switch (input.Type)
		{
		default:
		case PPTextureType::CurrentPipelineTexture:
			buffers->BindCurrentTexture(index, filter, wrap);
			break;

		case PPTextureType::NextPipelineTexture:
			I_FatalError("PPTextureType::NextPipelineTexture not allowed as input\n");
			break;

		case PPTextureType::PPTexture:
			GetGLTexture(input.Texture)->Tex.Bind(index, filter, wrap);
			break;

		case PPTextureType::SceneColor:
			buffers->BindSceneColorTexture(index);
			break;

		case PPTextureType::SceneFog:
			buffers->BindSceneFogTexture(index);
			break;

		case PPTextureType::SceneNormal:
			buffers->BindSceneNormalTexture(index);
			break;

		case PPTextureType::SceneDepth:
			buffers->BindSceneDepthTexture(index);
			break;
		}
	}

	// Set render target
	switch (Output.Type)
	{
	default:
		I_FatalError("Unsupported postprocess output type\n");
		break;

	case PPTextureType::CurrentPipelineTexture:
		buffers->BindCurrentFB();
		break;

	case PPTextureType::NextPipelineTexture:
		buffers->BindNextFB();
		break;

	case PPTextureType::PPTexture:
		if (GetGLTexture(Output.Texture)->FB)
			GetGLTexture(Output.Texture)->FB.Bind();
		else
			GetGLTexture(Output.Texture)->FB = buffers->CreateFrameBuffer("PPTextureFB"/*Output.Texture.GetChars()*/, GetGLTexture(Output.Texture)->Tex);
		break;

	case PPTextureType::SceneColor:
		buffers->BindSceneFB(false);
		break;
	}

	// Set blend mode
	if (BlendMode.BlendOp == STYLEOP_Add && BlendMode.SrcAlpha == STYLEALPHA_One && BlendMode.DestAlpha == STYLEALPHA_Zero && BlendMode.Flags == 0)
	{
		glDisable(GL_BLEND);
	}
	else
	{
		// To do: support all the modes
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		if (BlendMode.SrcAlpha == STYLEALPHA_One && BlendMode.DestAlpha == STYLEALPHA_One)
			glBlendFunc(GL_ONE, GL_ONE);
		else
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	// Setup viewport
	glViewport(Viewport.left, Viewport.top, Viewport.width, Viewport.height);

	auto shader = GetGLShader(Shader);

	// Set uniforms
	if (Uniforms.Data.Size() > 0)
	{
		if (!shader->Uniforms)
			shader->Uniforms.reset(screen->CreateDataBuffer(POSTPROCESS_BINDINGPOINT, false, false));
		shader->Uniforms->SetData(Uniforms.Data.Size(), Uniforms.Data.Data(), BufferUsageType::Static);
		static_cast<GLDataBuffer*>(shader->Uniforms.get())->BindBase();
	}

	// Set shader
	shader->Bind();

	// Draw the screen quad
	GLRenderer->RenderScreenQuad();

	// Advance to next PP texture if our output was sent there
	if (Output.Type == PPTextureType::NextPipelineTexture)
		buffers->NextTexture();

	glViewport(screen->mScreenViewport.left, screen->mScreenViewport.top, screen->mScreenViewport.width, screen->mScreenViewport.height);
}

void GLPPRenderState::PushGroup(const FString &name)
{
	FGLDebug::PushGroup(name.GetChars());
}

void GLPPRenderState::PopGroup()
{
	FGLDebug::PopGroup();
}


// Store the current stereo 3D eye buffer, and Load the next one

int FGLRenderBuffers::NextEye(int eyeCount)
{
	int nextEye = (mCurrentEye + 1) % eyeCount;
	if (nextEye == mCurrentEye) return mCurrentEye;
	BlitToEyeTexture(mCurrentEye);
	mCurrentEye = nextEye;
	BlitFromEyeTexture(mCurrentEye);
	return mCurrentEye;
}

}  // namespace OpenGLRenderer
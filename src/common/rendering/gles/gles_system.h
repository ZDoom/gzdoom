#ifndef __GLES_SYSTEM_H
#define __GLES_SYSTEM_H

#include <math.h>
#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <signal.h>
#if !defined(__APPLE__) && !defined(__FreeBSD__) && !defined(__OpenBSD__) && !defined(__DragonFly__)
#include <malloc.h>
#endif
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define USE_GLAD_LOADER 0 // Set to 1 to use the GLAD loader, otherwise use noramal GZDoom loader for PC

#if (USE_GLAD_LOADER)
	#include "glad/glad.h"

	// Below are used extensions for GLES
	typedef void* (APIENTRYP PFNGLMAPBUFFERRANGEEXTPROC)(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
	GLAPI PFNGLMAPBUFFERRANGEEXTPROC glMapBufferRange;

	typedef GLboolean(APIENTRYP PFNGLUNMAPBUFFEROESPROC)(GLenum target);
	GLAPI PFNGLUNMAPBUFFEROESPROC glUnmapBuffer;

	typedef void (APIENTRYP PFNGLVERTEXATTRIBIPOINTERPROC) (GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer);
	GLAPI PFNGLVERTEXATTRIBIPOINTERPROC glVertexAttribIPointer;

	typedef GLsync(APIENTRYP PFNGLFENCESYNCPROC)(GLenum condition, GLbitfield flags);
	GLAPI PFNGLFENCESYNCPROC glFenceSync;
	
	typedef GLenum(APIENTRYP PFNGLCLIENTWAITSYNCPROC)(GLsync sync, GLbitfield flags, GLuint64 timeout);
	GLAPI PFNGLCLIENTWAITSYNCPROC glClientWaitSync;

	typedef void (APIENTRYP PFNGLDELETESYNCPROC)(GLsync sync);
	GLAPI PFNGLDELETESYNCPROC glDeleteSync;

	#define GL_DEPTH24_STENCIL8               0x88F0
	#define GL_MAP_PERSISTENT_BIT             0x0040
	#define GL_MAP_READ_BIT                   0x0001
	#define GL_MAP_WRITE_BIT                  0x0002
	#define GL_MAP_UNSYNCHRONIZED_BIT         0x0020
	#define GL_MAP_INVALIDATE_BUFFER_BIT      0x0008
	#define GL_BGRA                           0x80E1
	#define GL_DEPTH_CLAMP                    0x864F
	#define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
	#define GL_INT_2_10_10_10_REV             0x8D9F
	#define GL_RED                            0x1903
	#define GL_TEXTURE_SWIZZLE_RGBA           0x8E46
	#define GL_SYNC_GPU_COMMANDS_COMPLETE     0x9117
	#define GL_SYNC_FLUSH_COMMANDS_BIT        0x00000001
	#define GL_ALREADY_SIGNALED               0x911A
	#define GL_CONDITION_SATISFIED            0x911C

#else
	#include "gl_load/gl_load.h"
#endif

#if defined(__APPLE__)
	#include <OpenGL/OpenGL.h>
#endif

// This is the number of vec4s make up the light data
#define LIGHT_VEC4_NUM 4

//#define NO_RENDER_BUFFER

//#define NPOT_EMULATION

namespace OpenGLESRenderer
{
	enum 
	{
		GLES_MODE_GLES = 0,
		GLES_MODE_OGL2 = 1,
		GLES_MODE_OGL3 = 2,
	};

	struct RenderContextGLES
	{
		unsigned int flags;
		unsigned int maxlights;
		unsigned int numlightvectors;
		bool useMappedBuffers;
		bool depthStencilAvailable;
		bool npotAvailable;
		bool forceGLSLv100;
		bool depthClampAvailable;
		bool anistropicFilterAvailable;
		int glesMode;
		const char* shaderVersionString;
		int max_texturesize;
		char* vendorstring;
		char* modelstring;
	};

	extern RenderContextGLES gles;

	void InitGLES();
}

#ifdef _MSC_VER
#pragma warning(disable : 4244)     // MIPS
#pragma warning(disable : 4136)     // X86
#pragma warning(disable : 4051)     // ALPHA

#pragma warning(disable : 4018)     // signed/unsigned mismatch
#pragma warning(disable : 4305)     // truncate from double to float
#endif

#endif //__GL_PCH_H

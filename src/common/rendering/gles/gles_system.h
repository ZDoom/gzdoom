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

#define USE_GLES2 0 // For Desktop PC leave as 0, it will use the exisiting OpenGL context creationg code but run with the GLES2 renderer
                    // Set to 1 for when comipling for a real GLES device

#if (USE_GLES2)
	#include "glad/glad.h"

// Below are used extensions for GLES
typedef void* (APIENTRYP PFNGLMAPBUFFERRANGEEXTPROC)(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
GLAPI PFNGLMAPBUFFERRANGEEXTPROC glMapBufferRange;

typedef GLboolean(APIENTRYP PFNGLUNMAPBUFFEROESPROC)(GLenum target);
GLAPI PFNGLUNMAPBUFFEROESPROC glUnmapBuffer;

#define GL_DEPTH24_STENCIL8               0x88F0
#define GL_MAP_PERSISTENT_BIT             0x0040
#define GL_MAP_READ_BIT                   0x0001
#define GL_MAP_WRITE_BIT                  0x0002
#define GL_MAP_UNSYNCHRONIZED_BIT         0x0020
#define GL_MAP_INVALIDATE_BUFFER_BIT      0x0008
#define GL_BGRA                           0x80E1
#define GL_DEPTH_CLAMP                    0x864F
#define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE

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

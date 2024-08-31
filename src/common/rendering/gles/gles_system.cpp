

#include "gles_system.h"
#include "tarray.h"
#include "v_video.h"
#include "printf.h"

CVAR(Bool, gles_use_mapped_buffer, false, 0);
CVAR(Bool, gles_force_glsl_v100, false, 0);
CVAR(Int, gles_max_lights_per_surface, 32, 0);
EXTERN_CVAR(Bool, gl_customshader);
void setGlVersion(double glv);


#if USE_GLAD_LOADER

PFNGLMAPBUFFERRANGEEXTPROC glMapBufferRange = NULL;
PFNGLUNMAPBUFFEROESPROC glUnmapBuffer = NULL;
PFNGLVERTEXATTRIBIPOINTERPROC glVertexAttribIPointer = NULL;
PFNGLFENCESYNCPROC glFenceSync = NULL;
PFNGLCLIENTWAITSYNCPROC glClientWaitSync = NULL;
PFNGLDELETESYNCPROC glDeleteSync = NULL;

#if defined _WIN32

#include <windows.h>

static HMODULE opengl32dll;
static PROC(WINAPI* getprocaddress)(LPCSTR name);

static void* LoadGLES2Proc(const char* name)
{
	HINSTANCE hGetProcIDDLL = LoadLibraryA("libGLESv2.dll");

	int error =	GetLastError();

	void* addr = GetProcAddress(hGetProcIDDLL, name);
	if (!addr)
	{
		//exit(1);
		return nullptr;
	}
	else
	{
		return addr;
	}
}

#else

#include <dlfcn.h>

static void* LoadGLES2Proc(const char* name)
{
	static void* glesLib = NULL;

	if (!glesLib)
	{
		int flags = RTLD_LOCAL | RTLD_NOW;

		glesLib = dlopen("libGLESv2_CM.so", flags);
		if (!glesLib)
		{
			glesLib = dlopen("libGLESv2.so", flags);
		}
		if (!glesLib)
		{
			glesLib = dlopen("libGLESv2.so.2", flags);
		}
	}

	void* ret = NULL;
	ret = dlsym(glesLib, name);

	return ret;
}

#endif

#endif // USE_GLAD_LOADER

static TArray<FString>  m_Extensions;


static void CollectExtensions()
{
	const char* supported = (char*)glGetString(GL_EXTENSIONS);

	if (nullptr != supported)
	{
		char* extensions = new char[strlen(supported) + 1];
		strcpy(extensions, supported);

		char* extension = strtok(extensions, " ");

		while (extension)
		{
			m_Extensions.Push(FString(extension));
			extension = strtok(nullptr, " ");
		}

		delete[] extensions;
	}
}


static bool CheckExtension(const char* ext)
{
	for (unsigned int i = 0; i < m_Extensions.Size(); ++i)
	{
		if (m_Extensions[i].CompareNoCase(ext) == 0) return true;
	}

	return false;
}

namespace OpenGLESRenderer
{
	RenderContextGLES gles;

	void InitGLES()
	{

#if USE_GLAD_LOADER

		if (!gladLoadGLES2Loader(&LoadGLES2Proc))
		{
			exit(-1);
		}

		glMapBufferRange = (PFNGLMAPBUFFERRANGEEXTPROC)LoadGLES2Proc("glMapBufferRange");
		glUnmapBuffer = (PFNGLUNMAPBUFFEROESPROC)LoadGLES2Proc("glUnmapBuffer");
		glVertexAttribIPointer = (PFNGLVERTEXATTRIBIPOINTERPROC)LoadGLES2Proc("glVertexAttribIPointer");

		glFenceSync = (PFNGLFENCESYNCPROC)LoadGLES2Proc("glFenceSync");
		glClientWaitSync = (PFNGLCLIENTWAITSYNCPROC)LoadGLES2Proc("glClientWaitSync");
		glDeleteSync = (PFNGLDELETESYNCPROC)LoadGLES2Proc("glDeleteSync");
#else
		static bool first = true;

		if (first)
		{
			if (ogl_LoadFunctions() == ogl_LOAD_FAILED)
			{
				//I_FatalError("Failed to load OpenGL functions.");
			}
		}
		GLuint vao;
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
#endif
		CollectExtensions();

		Printf("GL_VENDOR: %s\n", glGetString(GL_VENDOR));
		Printf("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
		Printf("GL_VERSION: %s\n", glGetString(GL_VERSION));
		Printf("GL_SHADING_LANGUAGE_VERSION: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
		Printf(PRINT_LOG, "GL_EXTENSIONS:\n");
		for (unsigned i = 0; i < m_Extensions.Size(); i++)
		{
			Printf(PRINT_LOG, " %s\n", m_Extensions[i].GetChars());
		}
		const char* glVersionStr = (const char*)glGetString(GL_VERSION);
		double glVersion = strtod(glVersionStr, NULL);

		Printf("GL Version parsed = %f\n", glVersion);

		gles.flags = RFL_NO_CLIP_PLANES;

		gles.useMappedBuffers = gles_use_mapped_buffer;
		gles.forceGLSLv100 = gles_force_glsl_v100;
		gles.maxlights = gles_max_lights_per_surface;
		gles.numlightvectors = (gles.maxlights * LIGHT_VEC4_NUM);

		gles.modelstring = (char*)glGetString(GL_RENDERER);
		gles.vendorstring = (char*)glGetString(GL_VENDOR);


		gl_customshader = false; // Disable user shaders for GLES renderer

		GLint maxTextureSize[1];
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, maxTextureSize);
		gles.max_texturesize = maxTextureSize[0];

		Printf("GL_MAX_TEXTURE_SIZE: %d\n", gles.max_texturesize);


		// Check if running on a GLES device, version string will start with 'OpenGL ES'
		if (!strncmp(glVersionStr, "OpenGL ES", strlen("OpenGL ES")))
		{
			gles.glesMode = GLES_MODE_GLES;
		}
		else // Else runnning on Desktop, check OpenGL version is 3 or above
		{
			if (glVersion > 3.29)
				gles.glesMode = GLES_MODE_OGL3; // 3.3 or above
			else
				gles.glesMode = GLES_MODE_OGL2; // Below 3.3
		}


		if (gles.glesMode == GLES_MODE_GLES)
		{
			Printf("GLES choosing mode: GLES_MODE_GLES\n");

			gles.shaderVersionString = "100";
			gles.depthStencilAvailable = CheckExtension("GL_OES_packed_depth_stencil");
			gles.npotAvailable = CheckExtension("GL_OES_texture_npot");
			gles.depthClampAvailable = CheckExtension("GL_EXT_depth_clamp");
			gles.anistropicFilterAvailable = CheckExtension("GL_EXT_texture_filter_anisotropic");
		}
		else if (gles.glesMode == GLES_MODE_OGL2)
		{
			Printf("GLES choosing mode: GLES_MODE_OGL2\n");

			gles.shaderVersionString = "100";
			gles.depthStencilAvailable = true;
			gles.npotAvailable = true;
			gles.useMappedBuffers = true;
			gles.depthClampAvailable = true;
			gles.anistropicFilterAvailable = true;
		}
		else if (gles.glesMode == GLES_MODE_OGL3)
		{
			Printf("GLES choosing mode: GLES_MODE_OGL3\n");

			gles.shaderVersionString = "330";
			gles.depthStencilAvailable = true;
			gles.npotAvailable = true;
			gles.useMappedBuffers = true;
			gles.depthClampAvailable = true;
			gles.anistropicFilterAvailable = true;
		}
		
		setGlVersion(glVersion);
	}
}

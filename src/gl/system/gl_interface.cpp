/*
** r_opengl.cpp
**
** OpenGL system interface
**
**---------------------------------------------------------------------------
** Copyright 2005 Tim Stump
** Copyright 2005-2013 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 4. Full disclosure of the entire project's source code, except for third
**    party libraries is mandatory. (NOTE: This clause is non-negotiable!)
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/
#include "gl/system/gl_system.h"
#include "tarray.h"
#include "doomtype.h"
#include "m_argv.h"
#include "zstring.h"
#include "version.h"
#include "i_system.h"
#include "v_text.h"
#include "gl/system/gl_interface.h"
#include "gl/system/gl_cvars.h"

#if defined (__unix__) || defined (__APPLE__)
#include <SDL.h>
#define wglGetProcAddress(x) (*SDL_GL_GetProcAddress)(x)
#endif
static void APIENTRY glBlendEquationDummy (GLenum mode);


static TArray<FString>  m_Extensions;

RenderContext gl;

int occlusion_type=0;

PROC myGetProcAddress(LPCSTR proc)
{
	PROC p = wglGetProcAddress(proc);
	if (p == NULL) I_Error("Fatal: GL function '%s' not found.");
	return p;
}


//==========================================================================
//
// 
//
//==========================================================================

static void CollectExtensions()
{
	const char *supported = NULL;
	char *extensions, *extension;

	supported = (char *)glGetString(GL_EXTENSIONS);

	if (supported)
	{
		extensions = new char[strlen(supported) + 1];
		strcpy(extensions, supported);

		extension = strtok(extensions, " ");
		while(extension)
		{
			m_Extensions.Push(FString(extension));
			extension = strtok(NULL, " ");
		}

		delete [] extensions;
	}
}

//==========================================================================
//
// 
//
//==========================================================================

static bool CheckExtension(const char *ext)
{
	for (unsigned int i = 0; i < m_Extensions.Size(); ++i)
	{
		if (m_Extensions[i].CompareNoCase(ext) == 0) return true;
	}

	return false;
}



//==========================================================================
//
// 
//
//==========================================================================

static void InitContext()
{
	gl.flags=0;
	glBlendEquation = glBlendEquationDummy;
}

//==========================================================================
//
// 
//
//==========================================================================

void gl_LoadExtensions()
{
	InitContext();
	CollectExtensions();

	const char *version = (const char*)glGetString(GL_VERSION);

	// Don't even start if it's lower than 1.3
	if (strcmp(version, "1.3") < 0) 
	{
		I_FatalError("Unsupported OpenGL version.\nAt least GL 1.3 is required to run " GAMENAME ".\n");
	}
	else if (strcmp(version, "1.4") < 0) 
	{
		// The engine will still assume 1.4 but the only 1.4 feature being used is GL_GENERATE_MIPMAP which should be supported as an extension
		// on most 1.3 cards this is present but let's print a warning that not everything may work as intended.
		Printf(TEXTCOLOR_RED "The current graphics driver implements a OpenGL version lower than 1.4 and may not support all features " GAMENAME " requires.\n");
	}

	// This loads any function pointers and flags that require a vaild render context to
	// initialize properly

	gl.shadermodel = 0;	// assume no shader support
	gl.vendorstring=(char*)glGetString(GL_VENDOR);

	// First try the regular function
	glBlendEquation = (PFNGLBLENDEQUATIONPROC)wglGetProcAddress("glBlendEquation");
	// If that fails try the EXT version
	if (!glBlendEquation) glBlendEquation = (PFNGLBLENDEQUATIONPROC)wglGetProcAddress("glBlendEquationEXT");
	// If that fails use a no-op dummy
	if (!glBlendEquation) glBlendEquation = glBlendEquationDummy;

	if (CheckExtension("GL_ARB_texture_non_power_of_two")) gl.flags|=RFL_NPOT_TEXTURE;
	if (CheckExtension("GL_ARB_texture_compression")) gl.flags|=RFL_TEXTURE_COMPRESSION;
	if (CheckExtension("GL_EXT_texture_compression_s3tc")) gl.flags|=RFL_TEXTURE_COMPRESSION_S3TC;
	if (strstr(gl.vendorstring, "NVIDIA")) gl.flags|=RFL_NVIDIA;
	else if (strstr(gl.vendorstring, "ATI Technologies")) gl.flags|=RFL_ATI;

	if (strcmp(version, "2.0") >= 0) gl.flags|=RFL_GL_20;
	if (strcmp(version, "2.1") >= 0) gl.flags|=RFL_GL_21;
	if (strcmp(version, "3.0") >= 0) gl.flags|=RFL_GL_30;

	glGetIntegerv(GL_MAX_TEXTURE_SIZE,&gl.max_texturesize);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	
	if (gl.flags & RFL_GL_20)
	{
		glDeleteShader = (PFNGLDELETESHADERPROC)myGetProcAddress("glDeleteShader");
		glDeleteProgram = (PFNGLDELETEPROGRAMPROC)myGetProcAddress("glDeleteProgram");
		glDetachShader = (PFNGLDETACHSHADERPROC)myGetProcAddress("glDetachShader");
		glCreateShader = (PFNGLCREATESHADERPROC)myGetProcAddress("glCreateShader");
		glShaderSource = (PFNGLSHADERSOURCEPROC)myGetProcAddress("glShaderSource");
		glCompileShader = (PFNGLCOMPILESHADERPROC)myGetProcAddress("glCompileShader");
		glCreateProgram = (PFNGLCREATEPROGRAMPROC)myGetProcAddress("glCreateProgram");
		glAttachShader = (PFNGLATTACHSHADERPROC)myGetProcAddress("glAttachShader");
		glLinkProgram = (PFNGLLINKPROGRAMPROC)myGetProcAddress("glLinkProgram");
		glUseProgram = (PFNGLUSEPROGRAMPROC)myGetProcAddress("glUseProgram");
		glValidateProgram = (PFNGLVALIDATEPROGRAMPROC)myGetProcAddress("glValidateProgram");

		glVertexAttrib1f = (PFNGLVERTEXATTRIB1FPROC)myGetProcAddress("glVertexAttrib1f");
		glVertexAttrib2f = (PFNGLVERTEXATTRIB2FPROC)myGetProcAddress("glVertexAttrib2f");
		glVertexAttrib4f = (PFNGLVERTEXATTRIB4FPROC)myGetProcAddress("glVertexAttrib4f");
		glVertexAttrib2fv = (PFNGLVERTEXATTRIB4FVPROC)myGetProcAddress("glVertexAttrib2fv");
		glVertexAttrib3fv = (PFNGLVERTEXATTRIB4FVPROC)myGetProcAddress("glVertexAttrib3fv");
		glVertexAttrib4fv = (PFNGLVERTEXATTRIB4FVPROC)myGetProcAddress("glVertexAttrib4fv");
		glVertexAttrib4ubv = (PFNGLVERTEXATTRIB4UBVPROC)myGetProcAddress("glVertexAttrib4ubv");
		glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)myGetProcAddress("glGetAttribLocation");
		glBindAttribLocation = (PFNGLBINDATTRIBLOCATIONPROC)myGetProcAddress("glBindAttribLocation");


		glUniform1f = (PFNGLUNIFORM1FPROC)myGetProcAddress("glUniform1f");
		glUniform2f = (PFNGLUNIFORM2FPROC)myGetProcAddress("glUniform2f");
		glUniform3f = (PFNGLUNIFORM3FPROC)myGetProcAddress("glUniform3f");
		glUniform4f = (PFNGLUNIFORM4FPROC)myGetProcAddress("glUniform4f");
		glUniform1i = (PFNGLUNIFORM1IPROC)myGetProcAddress("glUniform1i");
		glUniform2i = (PFNGLUNIFORM2IPROC)myGetProcAddress("glUniform2i");
		glUniform3i = (PFNGLUNIFORM3IPROC)myGetProcAddress("glUniform3i");
		glUniform4i = (PFNGLUNIFORM4IPROC)myGetProcAddress("glUniform4i");
		glUniform1fv = (PFNGLUNIFORM1FVPROC)myGetProcAddress("glUniform1fv");
		glUniform2fv = (PFNGLUNIFORM2FVPROC)myGetProcAddress("glUniform2fv");
		glUniform3fv = (PFNGLUNIFORM3FVPROC)myGetProcAddress("glUniform3fv");
		glUniform4fv = (PFNGLUNIFORM4FVPROC)myGetProcAddress("glUniform4fv");
		glUniform1iv = (PFNGLUNIFORM1IVPROC)myGetProcAddress("glUniform1iv");
		glUniform2iv = (PFNGLUNIFORM2IVPROC)myGetProcAddress("glUniform2iv");
		glUniform3iv = (PFNGLUNIFORM3IVPROC)myGetProcAddress("glUniform3iv");
		glUniform4iv = (PFNGLUNIFORM4IVPROC)myGetProcAddress("glUniform4iv");
		
		glUniformMatrix2fv = (PFNGLUNIFORMMATRIX2FVPROC)myGetProcAddress("glUniformMatrix2fv");
		glUniformMatrix3fv = (PFNGLUNIFORMMATRIX3FVPROC)myGetProcAddress("glUniformMatrix3fv");
		glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)myGetProcAddress("glUniformMatrix4fv");
		
		glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)myGetProcAddress("glGetProgramInfoLog");
		glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)myGetProcAddress("glGetShaderInfoLog");
		glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)myGetProcAddress("glGetUniformLocation");
		glGetActiveUniform = (PFNGLGETACTIVEUNIFORMPROC)myGetProcAddress("glGetActiveUniform");
		glGetUniformfv = (PFNGLGETUNIFORMFVPROC)myGetProcAddress("glGetUniformfv");
		glGetUniformiv = (PFNGLGETUNIFORMIVPROC)myGetProcAddress("glGetUniformiv");
		glGetShaderSource = (PFNGLGETSHADERSOURCEPROC)myGetProcAddress("glGetShaderSource");

		glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)myGetProcAddress("glEnableVertexAttribArray");
		glDisableVertexAttribArray= (PFNGLDISABLEVERTEXATTRIBARRAYPROC)myGetProcAddress("glDisableVertexAttribArray");
		glVertexAttribPointer		= (PFNGLVERTEXATTRIBPOINTERPROC)myGetProcAddress("glVertexAttribPointer");

		// what'S the equivalent of this in GL 2.0???
		glGetObjectParameteriv = (PFNGLGETOBJECTPARAMETERIVARBPROC)myGetProcAddress("glGetObjectParameterivARB");

		// Rules:
		// SM4 will always use shaders. No option to switch them off is needed here.
		// SM3 has shaders optional but they are off by default (they will have a performance impact
		// SM2 only uses shaders for colormaps on camera textures and has no option to use them in general.
		//     On SM2 cards the shaders will be too slow and show visual bugs (at least on GF 6800.)
		if (strcmp((const char*)glGetString(GL_SHADING_LANGUAGE_VERSION), "1.3") >= 0) gl.shadermodel = 4;
		else if (CheckExtension("GL_NV_GPU_shader4")) gl.shadermodel = 4;	// for pre-3.0 drivers that support GF8xxx.
		else if (CheckExtension("GL_EXT_GPU_shader4")) gl.shadermodel = 4;	// for pre-3.0 drivers that support GF8xxx.
		else if (CheckExtension("GL_NV_vertex_program3")) gl.shadermodel = 3;
		else if (!strstr(gl.vendorstring, "NVIDIA")) gl.shadermodel = 3;
		else gl.shadermodel = 2;	// Only for older NVidia cards which had notoriously bad shader support.

		// Command line overrides for testing and problem cases.
		if (Args->CheckParm("-sm2") && gl.shadermodel > 2) gl.shadermodel = 2;
		else if (Args->CheckParm("-sm3") && gl.shadermodel > 3) gl.shadermodel = 3;
	}

	if (CheckExtension("GL_ARB_occlusion_query"))
	{
        glGenQueries         = (PFNGLGENQUERIESARBPROC)myGetProcAddress("glGenQueriesARB");
        glDeleteQueries      = (PFNGLDELETEQUERIESARBPROC)myGetProcAddress("glDeleteQueriesARB");
        glGetQueryObjectuiv  = (PFNGLGETQUERYOBJECTUIVARBPROC)myGetProcAddress("glGetQueryObjectuivARB");
        glBeginQuery         = (PFNGLBEGINQUERYARBPROC)myGetProcAddress("glBeginQueryARB");
        glEndQuery           = (PFNGLENDQUERYPROC)myGetProcAddress("glEndQueryARB");
		gl.flags|=RFL_OCCLUSION_QUERY;
	}

	if (gl.flags & RFL_GL_21)
	{
		glBindBuffer			= (PFNGLBINDBUFFERPROC)myGetProcAddress("glBindBuffer");
		glDeleteBuffers			= (PFNGLDELETEBUFFERSPROC)myGetProcAddress("glDeleteBuffers");
		glGenBuffers			= (PFNGLGENBUFFERSPROC)myGetProcAddress("glGenBuffers");
		glBufferData			= (PFNGLBUFFERDATAPROC)myGetProcAddress("glBufferData");
		glBufferSubData			= (PFNGLBUFFERSUBDATAPROC)myGetProcAddress("glBufferSubData");
		glMapBuffer				= (PFNGLMAPBUFFERPROC)myGetProcAddress("glMapBuffer");
		glUnmapBuffer			= (PFNGLUNMAPBUFFERPROC)myGetProcAddress("glUnmapBuffer");
		gl.flags |= RFL_VBO;
	}
	else if (CheckExtension("GL_ARB_vertex_buffer_object"))
	{
		glBindBuffer			= (PFNGLBINDBUFFERPROC)myGetProcAddress("glBindBufferARB");
		glDeleteBuffers			= (PFNGLDELETEBUFFERSPROC)myGetProcAddress("glDeleteBuffersARB");
		glGenBuffers			= (PFNGLGENBUFFERSPROC)myGetProcAddress("glGenBuffersARB");
		glBufferData			= (PFNGLBUFFERDATAPROC)myGetProcAddress("glBufferDataARB");
		glBufferSubData			= (PFNGLBUFFERSUBDATAPROC)myGetProcAddress("glBufferSubDataARB");
		glMapBuffer				= (PFNGLMAPBUFFERPROC)myGetProcAddress("glMapBufferARB");
		glUnmapBuffer			= (PFNGLUNMAPBUFFERPROC)myGetProcAddress("glUnmapBufferARB");
		gl.flags |= RFL_VBO;
	}

	if (CheckExtension("GL_ARB_map_buffer_range")) 
	{
		glMapBufferRange			= (PFNGLMAPBUFFERRANGEPROC)myGetProcAddress("glMapBufferRange");
		glFlushMappedBufferRange	= (PFNGLFLUSHMAPPEDBUFFERRANGEPROC)myGetProcAddress("glFlushMappedBufferRange");
		gl.flags|=RFL_MAP_BUFFER_RANGE;
	}

	if (CheckExtension("GL_ARB_framebuffer_object"))
	{
		glGenFramebuffers			= (PFNGLGENFRAMEBUFFERSPROC)myGetProcAddress("glGenFramebuffers");
		glDeleteFramebuffers		= (PFNGLDELETEFRAMEBUFFERSPROC)myGetProcAddress("glDeleteFramebuffers");
		glBindFramebuffer			= (PFNGLBINDFRAMEBUFFERPROC)myGetProcAddress("glBindFramebuffer");
		glFramebufferTexture2D		= (PFNGLFRAMEBUFFERTEXTURE2DPROC)myGetProcAddress("glFramebufferTexture2D");
		glGenRenderbuffers			= (PFNGLGENRENDERBUFFERSPROC)myGetProcAddress("glGenRenderbuffers");
		glDeleteRenderbuffers		= (PFNGLDELETERENDERBUFFERSPROC)myGetProcAddress("glDeleteRenderbuffers");
		glBindRenderbuffer			= (PFNGLBINDRENDERBUFFERPROC)myGetProcAddress("glBindRenderbuffer");
		glRenderbufferStorage		= (PFNGLRENDERBUFFERSTORAGEPROC)myGetProcAddress("glRenderbufferStorage");
		glFramebufferRenderbuffer	= (PFNGLFRAMEBUFFERRENDERBUFFERPROC)myGetProcAddress("glFramebufferRenderbuffer");

		gl.flags|=RFL_FRAMEBUFFER;
	}

#if 0
	if (CheckExtension("GL_ARB_texture_buffer_object") && 
		CheckExtension("GL_ARB_texture_float") && 
		CheckExtension("GL_EXT_GPU_Shader4") && 
		CheckExtension("GL_ARB_texture_rg") && 
		gl.shadermodel == 4)
	{
		glTexBufferARB = (PFNGLTEXBUFFERARBPROC)myGetProcAddress("glTexBufferARB");
		gl.flags|=RFL_TEXTUREBUFFER;
	}
#endif



	glActiveTexture = (PFNGLACTIVETEXTUREPROC)myGetProcAddress("glActiveTextureARB");
	glMultiTexCoord2f = (PFNGLMULTITEXCOORD2FPROC) myGetProcAddress("glMultiTexCoord2fARB");
	glMultiTexCoord2fv = (PFNGLMULTITEXCOORD2FVPROC) myGetProcAddress("glMultiTexCoord2fvARB");
}

//==========================================================================
//
// 
//
//==========================================================================

void gl_PrintStartupLog()
{
	Printf ("GL_VENDOR: %s\n", glGetString(GL_VENDOR));
	Printf ("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
	Printf ("GL_VERSION: %s\n", glGetString(GL_VERSION));
	Printf ("GL_SHADING_LANGUAGE_VERSION: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
	Printf ("GL_EXTENSIONS: %s\n", glGetString(GL_EXTENSIONS));
	int v;

	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &v);
	Printf ("Max. texture units: %d\n", v);
	glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &v);
	Printf ("Max. fragment uniforms: %d\n", v);
	if (gl.shadermodel == 4) gl.maxuniforms = v;
	glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &v);
	Printf ("Max. vertex uniforms: %d\n", v);
	glGetIntegerv(GL_MAX_VARYING_FLOATS, &v);
	Printf ("Max. varying: %d\n", v);
	glGetIntegerv(GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS, &v);
	Printf ("Max. combined uniforms: %d\n", v);
	glGetIntegerv(GL_MAX_COMBINED_UNIFORM_BLOCKS, &v);
	Printf ("Max. combined uniform blocks: %d\n", v);

}

//==========================================================================
//
// 
//
//==========================================================================

static void APIENTRY glBlendEquationDummy (GLenum mode)
{
	// If this is not supported all non-existent modes are
	// made to draw nothing.
	if (mode == GL_FUNC_ADD)
	{
		glColorMask(true, true, true, true);
	}
	else
	{
		glColorMask(false, false, false, false);
	}
}

//==========================================================================
//
// 
//
//==========================================================================

void gl_SetTextureMode(int type)
{
	static float white[] = {1.f,1.f,1.f,1.f};

	if (type == TM_MASK)
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);

		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE); 
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_TEXTURE0);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
	}
	else if (type == TM_OPAQUE)
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE); 
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
	}
	else if (type == TM_INVERT)
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_ONE_MINUS_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE); 
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_TEXTURE0);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
	}
	else if (type == TM_INVERTOPAQUE)
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_ONE_MINUS_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE); 
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
	}
	else // if (type == TM_MODULATE)
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}
}

//} // extern "C"

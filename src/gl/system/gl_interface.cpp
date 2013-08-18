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
#include "gl/system/gl_cvars.h"

#if defined (unix) || defined (__APPLE__)
#include <SDL.h>
#define wglGetProcAddress(x) (*SDL_GL_GetProcAddress)(x)
#endif
static void APIENTRY glBlendEquationDummy (GLenum mode);


static TArray<FString>  m_Extensions;

RenderContext gl;

int occlusion_type=0;



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

	gl.Begin = glBegin;
	gl.End = glEnd;
	gl.DrawArrays = glDrawArrays;

	gl.TexCoord2f = glTexCoord2f;
	gl.TexCoord2fv = glTexCoord2fv;

	gl.Vertex2f = glVertex2f;
	gl.Vertex2i = glVertex2i;
	gl.Vertex3f = glVertex3f;
	gl.Vertex3fv = glVertex3fv;
	gl.Vertex3d = glVertex3d;

	gl.Color4f = glColor4f;
	gl.Color4fv = glColor4fv;
	gl.Color3f = glColor3f;
	gl.Color3ub = glColor3ub;
	gl.Color4ub = glColor4ub;

	gl.ColorMask = glColorMask;

	gl.DepthFunc = glDepthFunc;
	gl.DepthMask = glDepthMask;
	gl.DepthRange = glDepthRange;

	gl.StencilFunc = glStencilFunc;
	gl.StencilMask = glStencilMask;
	gl.StencilOp = glStencilOp;

	gl.MatrixMode = glMatrixMode;
	gl.PushMatrix = glPushMatrix;
	gl.PopMatrix = glPopMatrix;
	gl.LoadIdentity = glLoadIdentity;
	gl.MultMatrixd = glMultMatrixd;
	gl.Translatef = glTranslatef;
	gl.Ortho = glOrtho;
	gl.Scalef = glScalef;
	gl.Rotatef = glRotatef;

	gl.Viewport = glViewport;
	gl.Scissor = glScissor;

	gl.Clear = glClear;
	gl.ClearColor = glClearColor;
	gl.ClearDepth = glClearDepth;
	gl.ShadeModel = glShadeModel;
	gl.Hint = glHint;

	gl.DisableClientState = glDisableClientState;
	gl.EnableClientState = glEnableClientState;

	gl.Fogf = glFogf;
	gl.Fogi = glFogi;
	gl.Fogfv = glFogfv;

	gl.Enable = glEnable;
	gl.IsEnabled = glIsEnabled;
	gl.Disable = glDisable;

	gl.TexGeni = glTexGeni;
	gl.DeleteTextures = glDeleteTextures;
	gl.GenTextures = glGenTextures;
	gl.BindTexture = glBindTexture;
	gl.TexImage2D = glTexImage2D;
	gl.TexParameterf = glTexParameterf;
	gl.TexParameteri = glTexParameteri;
	gl.CopyTexSubImage2D = glCopyTexSubImage2D;

	gl.ReadPixels = glReadPixels;
	gl.PolygonOffset = glPolygonOffset;
	gl.ClipPlane = glClipPlane;

	gl.Finish = glFinish;
	gl.Flush = glFlush;

	gl.BlendEquation = glBlendEquationDummy;
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

	// Don't even start if it's lower than 1.4
	if (strcmp(version, "1.4") < 0) 
	{
		I_FatalError("Unsupported OpenGL version.\nAt least GL 1.4 is required to run "GAMENAME".\n");
	}

	// This loads any function pointers and flags that require a vaild render context to
	// initialize properly

	gl.shadermodel = 0;	// assume no shader support
	gl.vendorstring=(char*)glGetString(GL_VENDOR);

	// First try the regular function
	gl.BlendEquation = (PFNGLBLENDEQUATIONPROC)wglGetProcAddress("glBlendEquation");
	// If that fails try the EXT version
	if (!gl.BlendEquation) gl.BlendEquation = (PFNGLBLENDEQUATIONPROC)wglGetProcAddress("glBlendEquationEXT");
	// If that fails use a no-op dummy
	if (!gl.BlendEquation) gl.BlendEquation = glBlendEquationDummy;

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
		gl.DeleteShader = (PFNGLDELETESHADERPROC)wglGetProcAddress("glDeleteShader");
		gl.DeleteProgram = (PFNGLDELETEPROGRAMPROC)wglGetProcAddress("glDeleteProgram");
		gl.DetachShader = (PFNGLDETACHSHADERPROC)wglGetProcAddress("glDetachShader");
		gl.CreateShader = (PFNGLCREATESHADERPROC)wglGetProcAddress("glCreateShader");
		gl.ShaderSource = (PFNGLSHADERSOURCEPROC)wglGetProcAddress("glShaderSource");
		gl.CompileShader = (PFNGLCOMPILESHADERPROC)wglGetProcAddress("glCompileShader");
		gl.CreateProgram = (PFNGLCREATEPROGRAMPROC)wglGetProcAddress("glCreateProgram");
		gl.AttachShader = (PFNGLATTACHSHADERPROC)wglGetProcAddress("glAttachShader");
		gl.LinkProgram = (PFNGLLINKPROGRAMPROC)wglGetProcAddress("glLinkProgram");
		gl.UseProgram = (PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram");
		gl.ValidateProgram = (PFNGLVALIDATEPROGRAMPROC)wglGetProcAddress("glValidateProgram");

		gl.VertexAttrib1f = (PFNGLVERTEXATTRIB1FPROC)wglGetProcAddress("glVertexAttrib1f");
		gl.VertexAttrib2f = (PFNGLVERTEXATTRIB2FPROC)wglGetProcAddress("glVertexAttrib2f");
		gl.VertexAttrib4f = (PFNGLVERTEXATTRIB4FPROC)wglGetProcAddress("glVertexAttrib4f");
		gl.VertexAttrib2fv = (PFNGLVERTEXATTRIB4FVPROC)wglGetProcAddress("glVertexAttrib2fv");
		gl.VertexAttrib3fv = (PFNGLVERTEXATTRIB4FVPROC)wglGetProcAddress("glVertexAttrib3fv");
		gl.VertexAttrib4fv = (PFNGLVERTEXATTRIB4FVPROC)wglGetProcAddress("glVertexAttrib4fv");
		gl.VertexAttrib4ubv = (PFNGLVERTEXATTRIB4UBVPROC)wglGetProcAddress("glVertexAttrib4ubv");
		gl.GetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)wglGetProcAddress("glGetAttribLocation");
		gl.BindAttribLocation = (PFNGLBINDATTRIBLOCATIONPROC)wglGetProcAddress("glBindAttribLocation");


		gl.Uniform1f = (PFNGLUNIFORM1FPROC)wglGetProcAddress("glUniform1f");
		gl.Uniform2f = (PFNGLUNIFORM2FPROC)wglGetProcAddress("glUniform2f");
		gl.Uniform3f = (PFNGLUNIFORM3FPROC)wglGetProcAddress("glUniform3f");
		gl.Uniform4f = (PFNGLUNIFORM4FPROC)wglGetProcAddress("glUniform4f");
		gl.Uniform1i = (PFNGLUNIFORM1IPROC)wglGetProcAddress("glUniform1i");
		gl.Uniform2i = (PFNGLUNIFORM2IPROC)wglGetProcAddress("glUniform2i");
		gl.Uniform3i = (PFNGLUNIFORM3IPROC)wglGetProcAddress("glUniform3i");
		gl.Uniform4i = (PFNGLUNIFORM4IPROC)wglGetProcAddress("glUniform4i");
		gl.Uniform1fv = (PFNGLUNIFORM1FVPROC)wglGetProcAddress("glUniform1fv");
		gl.Uniform2fv = (PFNGLUNIFORM2FVPROC)wglGetProcAddress("glUniform2fv");
		gl.Uniform3fv = (PFNGLUNIFORM3FVPROC)wglGetProcAddress("glUniform3fv");
		gl.Uniform4fv = (PFNGLUNIFORM4FVPROC)wglGetProcAddress("glUniform4fv");
		gl.Uniform1iv = (PFNGLUNIFORM1IVPROC)wglGetProcAddress("glUniform1iv");
		gl.Uniform2iv = (PFNGLUNIFORM2IVPROC)wglGetProcAddress("glUniform2iv");
		gl.Uniform3iv = (PFNGLUNIFORM3IVPROC)wglGetProcAddress("glUniform3iv");
		gl.Uniform4iv = (PFNGLUNIFORM4IVPROC)wglGetProcAddress("glUniform4iv");
		
		gl.UniformMatrix2fv = (PFNGLUNIFORMMATRIX2FVPROC)wglGetProcAddress("glUniformMatrix2fv");
		gl.UniformMatrix3fv = (PFNGLUNIFORMMATRIX3FVPROC)wglGetProcAddress("glUniformMatrix3fv");
		gl.UniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)wglGetProcAddress("glUniformMatrix4fv");
		
		gl.GetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)wglGetProcAddress("glGetProgramInfoLog");
		gl.GetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)wglGetProcAddress("glGetShaderInfoLog");
		gl.GetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)wglGetProcAddress("glGetUniformLocation");
		gl.GetActiveUniform = (PFNGLGETACTIVEUNIFORMPROC)wglGetProcAddress("glGetActiveUniform");
		gl.GetUniformfv = (PFNGLGETUNIFORMFVPROC)wglGetProcAddress("glGetUniformfv");
		gl.GetUniformiv = (PFNGLGETUNIFORMIVPROC)wglGetProcAddress("glGetUniformiv");
		gl.GetShaderSource = (PFNGLGETSHADERSOURCEPROC)wglGetProcAddress("glGetShaderSource");

		gl.EnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)wglGetProcAddress("glEnableVertexAttribArray");
		gl.DisableVertexAttribArray= (PFNGLDISABLEVERTEXATTRIBARRAYPROC)wglGetProcAddress("glDisableVertexAttribArray");
		gl.VertexAttribPointer		= (PFNGLVERTEXATTRIBPOINTERPROC)wglGetProcAddress("glVertexAttribPointer");

		// what'S the equivalent of this in GL 2.0???
		gl.GetObjectParameteriv = (PFNGLGETOBJECTPARAMETERIVARBPROC)wglGetProcAddress("glGetObjectParameterivARB");

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
        gl.GenQueries         = (PFNGLGENQUERIESARBPROC)wglGetProcAddress("glGenQueriesARB");
        gl.DeleteQueries      = (PFNGLDELETEQUERIESARBPROC)wglGetProcAddress("glDeleteQueriesARB");
        gl.GetQueryObjectuiv  = (PFNGLGETQUERYOBJECTUIVARBPROC)wglGetProcAddress("glGetQueryObjectuivARB");
        gl.BeginQuery         = (PFNGLBEGINQUERYARBPROC)wglGetProcAddress("glBeginQueryARB");
        gl.EndQuery           = (PFNGLENDQUERYPROC)wglGetProcAddress("glEndQueryARB");
		gl.flags|=RFL_OCCLUSION_QUERY;
	}

	if (gl.flags & RFL_GL_21)
	{
		gl.BindBuffer				= (PFNGLBINDBUFFERPROC)wglGetProcAddress("glBindBuffer");
		gl.DeleteBuffers			= (PFNGLDELETEBUFFERSPROC)wglGetProcAddress("glDeleteBuffers");
		gl.GenBuffers				= (PFNGLGENBUFFERSPROC)wglGetProcAddress("glGenBuffers");
		gl.BufferData				= (PFNGLBUFFERDATAPROC)wglGetProcAddress("glBufferData");
		gl.BufferSubData			= (PFNGLBUFFERSUBDATAPROC)wglGetProcAddress("glBufferSubData");
		gl.MapBuffer				= (PFNGLMAPBUFFERPROC)wglGetProcAddress("glMapBuffer");
		gl.UnmapBuffer				= (PFNGLUNMAPBUFFERPROC)wglGetProcAddress("glUnmapBuffer");
		gl.flags |= RFL_VBO;
	}
	else if (CheckExtension("GL_ARB_vertex_buffer_object"))
	{
		gl.BindBuffer				= (PFNGLBINDBUFFERPROC)wglGetProcAddress("glBindBufferARB");
		gl.DeleteBuffers			= (PFNGLDELETEBUFFERSPROC)wglGetProcAddress("glDeleteBuffersARB");
		gl.GenBuffers				= (PFNGLGENBUFFERSPROC)wglGetProcAddress("glGenBuffersARB");
		gl.BufferData				= (PFNGLBUFFERDATAPROC)wglGetProcAddress("glBufferDataARB");
		gl.BufferSubData			= (PFNGLBUFFERSUBDATAPROC)wglGetProcAddress("glBufferSubDataARB");
		gl.MapBuffer				= (PFNGLMAPBUFFERPROC)wglGetProcAddress("glMapBufferARB");
		gl.UnmapBuffer				= (PFNGLUNMAPBUFFERPROC)wglGetProcAddress("glUnmapBufferARB");
		gl.flags |= RFL_VBO;
	}

	if (CheckExtension("GL_ARB_map_buffer_range")) 
	{
		gl.MapBufferRange			= (PFNGLMAPBUFFERRANGEPROC)wglGetProcAddress("glMapBufferRange");
		gl.FlushMappedBufferRange	= (PFNGLFLUSHMAPPEDBUFFERRANGEPROC)wglGetProcAddress("glFlushMappedBufferRange");
		gl.flags|=RFL_MAP_BUFFER_RANGE;
	}

	if (CheckExtension("GL_ARB_framebuffer_object"))
	{
		gl.GenFramebuffers			= (PFNGLGENFRAMEBUFFERSPROC)wglGetProcAddress("glGenFramebuffers");
		gl.DeleteFramebuffers		= (PFNGLDELETEFRAMEBUFFERSPROC)wglGetProcAddress("glDeleteFramebuffers");
		gl.BindFramebuffer			= (PFNGLBINDFRAMEBUFFERPROC)wglGetProcAddress("glBindFramebuffer");
		gl.FramebufferTexture2D	= (PFNGLFRAMEBUFFERTEXTURE2DPROC)wglGetProcAddress("glFramebufferTexture2D");
		gl.GenRenderbuffers		= (PFNGLGENRENDERBUFFERSPROC)wglGetProcAddress("glGenRenderbuffers");
		gl.DeleteRenderbuffers		= (PFNGLDELETERENDERBUFFERSPROC)wglGetProcAddress("glDeleteRenderbuffers");
		gl.BindRenderbuffer		= (PFNGLBINDRENDERBUFFERPROC)wglGetProcAddress("glBindRenderbuffer");
		gl.RenderbufferStorage		= (PFNGLRENDERBUFFERSTORAGEPROC)wglGetProcAddress("glRenderbufferStorage");
		gl.FramebufferRenderbuffer	= (PFNGLFRAMEBUFFERRENDERBUFFERPROC)wglGetProcAddress("glFramebufferRenderbuffer");

		gl.flags|=RFL_FRAMEBUFFER;
	}

#if 0
	if (CheckExtension("GL_ARB_texture_buffer_object") && 
		CheckExtension("GL_ARB_texture_float") && 
		CheckExtension("GL_EXT_GPU_Shader4") && 
		CheckExtension("GL_ARB_texture_rg") && 
		gl.shadermodel == 4)
	{
		gl.TexBufferARB = (PFNGLTEXBUFFERARBPROC)wglGetProcAddress("glTexBufferARB");
		gl.flags|=RFL_TEXTUREBUFFER;
	}
#endif



	gl.ActiveTexture = (PFNGLACTIVETEXTUREPROC)wglGetProcAddress("glActiveTextureARB");
	gl.MultiTexCoord2f = (PFNGLMULTITEXCOORD2FPROC) wglGetProcAddress("glMultiTexCoord2fARB");
	gl.MultiTexCoord2fv = (PFNGLMULTITEXCOORD2FVPROC) wglGetProcAddress("glMultiTexCoord2fvARB");
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

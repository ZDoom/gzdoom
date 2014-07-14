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

	const char *version = Args->CheckValue("-glversion");
	if (version == NULL) version = (const char*)glGetString(GL_VERSION);
	else Printf("Emulating OpenGL v %s\n", version);


	// Don't even start if it's lower than 3.0
	if (strcmp(version, "3.0") < 0) 
	{
		I_FatalError("Unsupported OpenGL version.\nAt least GL 3.0 is required to run " GAMENAME ".\n");
	}
	gl.version = strtod(version, NULL);
	gl.glslversion = strtod((char*)glGetString(GL_SHADING_LANGUAGE_VERSION), NULL);

	gl.vendorstring=(char*)glGetString(GL_VENDOR);

	if (CheckExtension("GL_ARB_texture_compression")) gl.flags|=RFL_TEXTURE_COMPRESSION;
	if (CheckExtension("GL_EXT_texture_compression_s3tc")) gl.flags|=RFL_TEXTURE_COMPRESSION_S3TC;
	if (gl.version >= 4.f && CheckExtension("GL_ARB_buffer_storage")) gl.flags |= RFL_BUFFER_STORAGE;

	glGetIntegerv(GL_MAX_TEXTURE_SIZE,&gl.max_texturesize);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
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

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &v);
	Printf("Max. texture size: %d\n", v);
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &v);
	Printf ("Max. texture units: %d\n", v);
	glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &v);
	Printf ("Max. fragment uniforms: %d\n", v);
	gl.maxuniforms = v;
	glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &v);
	Printf ("Max. vertex uniforms: %d\n", v);
	glGetIntegerv(GL_MAX_VARYING_FLOATS, &v);
	Printf ("Max. varying: %d\n", v);
	glGetIntegerv(GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS, &v);
	Printf("Max. combined shader storage blocks: %d\n", v);
	glGetIntegerv(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS, &v);
	Printf("Max. vertex shader storage blocks: %d\n", v);

}


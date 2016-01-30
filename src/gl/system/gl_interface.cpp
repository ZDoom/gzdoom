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
	const char *extension;

	int max = 0;
	glGetIntegerv(GL_NUM_EXTENSIONS, &max);

	for(int i = 0; i < max; i++)
	{
		extension = (const char*)glGetStringi(GL_EXTENSIONS, i);
		m_Extensions.Push(FString(extension));
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
		I_FatalError("Unsupported OpenGL version.\nAt least OpenGL 3.0 is required to run " GAMENAME ".\n");
	}

	// add 0.01 to account for roundoff errors making the number a tad smaller than the actual version
	gl.version = strtod(version, NULL) + 0.01f;
	gl.glslversion = strtod((char*)glGetString(GL_SHADING_LANGUAGE_VERSION), NULL) + 0.01f;

	gl.vendorstring = (char*)glGetString(GL_VENDOR);

	if (gl.version < 3.3f && !CheckExtension("GL_ARB_sampler_objects"))
	{
		I_FatalError("'GL_ARB_sampler_objects' extension not found. Please update your graphics driver.");
	}
	if (CheckExtension("GL_ARB_texture_compression")) gl.flags|=RFL_TEXTURE_COMPRESSION;
	if (CheckExtension("GL_EXT_texture_compression_s3tc")) gl.flags|=RFL_TEXTURE_COMPRESSION_S3TC;
	if (!Args->CheckParm("-gl3"))
	{
		// don't use GL 4.x features when running in GL 3 emulation mode.
		if (CheckExtension("GL_ARB_buffer_storage"))
		{
			// work around a problem with older AMD drivers: Their implementation of shader storage buffer objects is piss-poor and does not match uniform buffers even closely.
			// Recent drivers, GL 4.4 don't have this problem, these can easily be recognized by also supporting the GL_ARB_buffer_storage extension.
			if (CheckExtension("GL_ARB_shader_storage_buffer_object"))
			{
				// Shader storage buffer objects are broken on current Intel drivers.
				if (strstr(gl.vendorstring, "Intel") == NULL)
				{
					gl.flags |= RFL_SHADER_STORAGE_BUFFER;
				}
			}
			gl.flags |= RFL_BUFFER_STORAGE;
		}
	}
	
	int v;
	glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &v);
	gl.maxuniforms = v;
	glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &v);
	gl.maxuniformblock = v;
	glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &v);
	gl.uniformblockalignment = v;
	
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
	int v = 0;
	if (gl.version >= 3.2) glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &v);

	Printf ("GL_VENDOR: %s\n", glGetString(GL_VENDOR));
	Printf ("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
	Printf ("GL_VERSION: %s (%s profile)\n", glGetString(GL_VERSION), (v & GL_CONTEXT_CORE_PROFILE_BIT)? "Core" : "Compatibility");
	Printf ("GL_SHADING_LANGUAGE_VERSION: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
	Printf ("GL_EXTENSIONS:");
	for (unsigned i = 0; i < m_Extensions.Size(); i++)
	{
		Printf(" %s", m_Extensions[i].GetChars());
	}

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &v);
	Printf("\nMax. texture size: %d\n", v);
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &v);
	Printf ("Max. texture units: %d\n", v);
	glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &v);
	Printf ("Max. fragment uniforms: %d\n", v);
	glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &v);
	Printf ("Max. vertex uniforms: %d\n", v);
	glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &v);
	Printf ("Max. uniform block size: %d\n", v);
	glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &v);
	Printf ("Uniform block alignment: %d\n", v);

	glGetIntegerv(GL_MAX_VARYING_FLOATS, &v);
	Printf ("Max. varying: %d\n", v);
	glGetIntegerv(GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS, &v);
	Printf("Max. combined shader storage blocks: %d\n", v);
	glGetIntegerv(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS, &v);
	Printf("Max. vertex shader storage blocks: %d\n", v);


}


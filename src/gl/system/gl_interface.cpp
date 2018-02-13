// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2005-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** r_opengl.cpp
**
** OpenGL system interface
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
#include "r_data/r_translate.h"
#include "gl/system/gl_interface.h"
#include "gl/system/gl_cvars.h"

void gl_PatchMenu();
static TArray<FString>  m_Extensions;
RenderContext gl;
static double realglversion;	// this is public so the statistics code can access it.

EXTERN_CVAR(Bool, gl_legacy_mode)
extern int currentrenderer;
CVAR(Bool, gl_riskymodernpath, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

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

	if (max == 0)
	{
		// Try old method to collect extensions
		const char *supported = (char *)glGetString(GL_EXTENSIONS);

		if (nullptr != supported)
		{
			char *extensions = new char[strlen(supported) + 1];
			strcpy(extensions, supported);

			char *extension = strtok(extensions, " ");

			while (extension)
			{
				m_Extensions.Push(FString(extension));
				extension = strtok(nullptr, " ");
			}

			delete [] extensions;
		}
	}
	else
	{
		// Use modern method to collect extensions
		for (int i = 0; i < max; i++)
		{
			extension = (const char*)glGetStringi(GL_EXTENSIONS, i);
			m_Extensions.Push(FString(extension));
		}
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

#define FUDGE_FUNC(name, ext) 	if (_ptrc_##name == NULL) _ptrc_##name = _ptrc_##name##ext;


void gl_LoadExtensions()
{
	InitContext();
	CollectExtensions();

	const char *glversion = (const char*)glGetString(GL_VERSION);
	gl.es = false;
	
	if (glversion && strlen(glversion) > 10 && memcmp(glversion, "OpenGL ES ", 10) == 0)
	{
		glversion += 10;
		gl.es = true;
	}

	const char *version = Args->CheckValue("-glversion");
	realglversion = strtod(glversion, NULL);


	if (version == NULL)
	{
		version = glversion;
	}
	else
	{
		double v1 = strtod(version, NULL);
		if (v1 >= 3.0 && v1 < 3.3) v1 = 3.3;	// promote '3' to 3.3 to avoid falling back to the legacy path.
		if (realglversion < v1) version = glversion;
		else Printf("Emulating OpenGL v %s\n", version);
	}

	float gl_version = (float)strtod(version, NULL) + 0.01f;

	if (gl.es)
	{
		if (gl_version < 2.0f)
		{
			I_FatalError("Unsupported OpenGL ES version.\nAt least OpenGL ES 2.0 is required to run " GAMENAME ".\n");
		}
		
		const char *glslversion = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
		if (glslversion && strlen(glslversion) > 18 && memcmp(glslversion, "OpenGL ES GLSL ES ", 10) == 0)
		{
			glslversion += 18;
		}
		
		// add 0.01 to account for roundoff errors making the number a tad smaller than the actual version
		gl.glslversion = strtod(glslversion, NULL) + 0.01f;
		gl.vendorstring = (char*)glGetString(GL_VENDOR);

		// Use the slowest/oldest modern path for now
		gl.legacyMode = false;
		gl.lightmethod = LM_DEFERRED;
		gl.buffermethod = BM_DEFERRED;
		gl.flags |= RFL_NO_CLIP_PLANES;
	}
	else
	{
		// Don't even start if it's lower than 2.0 or no framebuffers are available (The framebuffer extension is needed for glGenerateMipmapsEXT!)
		if ((gl_version < 2.0f || !CheckExtension("GL_EXT_framebuffer_object")) && gl_version < 3.0f)
		{
			I_FatalError("Unsupported OpenGL version.\nAt least OpenGL 2.0 with framebuffer support is required to run " GAMENAME ".\n");
		}
		
		gl.es = false;

		// add 0.01 to account for roundoff errors making the number a tad smaller than the actual version
		gl.glslversion = strtod((char*)glGetString(GL_SHADING_LANGUAGE_VERSION), NULL) + 0.01f;

		gl.vendorstring = (char*)glGetString(GL_VENDOR);

		// first test for optional features
		if (CheckExtension("GL_ARB_texture_compression")) gl.flags |= RFL_TEXTURE_COMPRESSION;
		if (CheckExtension("GL_EXT_texture_compression_s3tc")) gl.flags |= RFL_TEXTURE_COMPRESSION_S3TC;

		if ((gl_version >= 3.3f || CheckExtension("GL_ARB_sampler_objects")) && !Args->CheckParm("-nosampler"))
		{
			gl.flags |= RFL_SAMPLER_OBJECTS;
		}
	
		// The minimum requirement for the modern render path is GL 3.3.
		// Although some GL 3.1 or 3.2 solutions may theoretically work they are usually too broken or too slow.
		// unless, of course, we're simply using this as a software backend...
		float minmodernpath = 3.3f;
		if (gl_riskymodernpath)
			minmodernpath = 3.1f;
		if ((gl_version < minmodernpath && (currentrenderer==1)) || gl_version < 3.0f)
		{
			gl.legacyMode = true;
			gl.lightmethod = LM_LEGACY;
			gl.buffermethod = BM_LEGACY;
			gl.glslversion = 0;
			gl.flags |= RFL_NO_CLIP_PLANES;
		}
		else
		{
			gl.legacyMode = false;
			gl.lightmethod = LM_DEFERRED;
			gl.buffermethod = BM_DEFERRED;
			if (gl_version < 4.f)
			{
#ifdef _WIN32
				if (strstr(gl.vendorstring, "ATI Tech"))
				{
					gl.flags |= RFL_NO_CLIP_PLANES;	// gl_ClipDistance is horribly broken on ATI GL3 drivers for Windows.
				}
#endif
			}
			else if (gl_version < 4.5f)
			{
				// don't use GL 4.x features when running a GL 3.x context.
				if (CheckExtension("GL_ARB_buffer_storage"))
				{
					// work around a problem with older AMD drivers: Their implementation of shader storage buffer objects is piss-poor and does not match uniform buffers even closely.
					// Recent drivers, GL 4.4 don't have this problem, these can easily be recognized by also supporting the GL_ARB_buffer_storage extension.
					if (CheckExtension("GL_ARB_shader_storage_buffer_object"))
					{
						// Intel's GLSL compiler is a bit broken with extensions, so unlock the feature only if not on Intel or having GL 4.3.
						if (strstr(gl.vendorstring, "Intel") == NULL || gl_version >= 4.3f)
						{
							gl.flags |= RFL_SHADER_STORAGE_BUFFER;
						}
					}
					gl.flags |= RFL_BUFFER_STORAGE;
					gl.lightmethod = LM_DIRECT;
					gl.buffermethod = BM_PERSISTENT;
				}
			}
			else
			{
				// Assume that everything works without problems on GL 4.5 drivers where these things are core features.
				gl.flags |= RFL_SHADER_STORAGE_BUFFER | RFL_BUFFER_STORAGE;
				gl.lightmethod =	LM_DIRECT;
				gl.buffermethod = BM_PERSISTENT;
			}

			if (gl_version >= 4.3f || CheckExtension("GL_ARB_invalidate_subdata")) gl.flags |= RFL_INVALIDATE_BUFFER;
			if (gl_version >= 4.3f || CheckExtension("GL_KHR_debug")) gl.flags |= RFL_DEBUG;

			const char *lm = Args->CheckValue("-lightmethod");
			if (lm != NULL)
			{
				if (!stricmp(lm, "deferred") && gl.lightmethod == LM_DIRECT) gl.lightmethod = LM_DEFERRED;
			}

			lm = Args->CheckValue("-buffermethod");
			if (lm != NULL)
			{
				if (!stricmp(lm, "deferred") && gl.buffermethod == BM_PERSISTENT) gl.buffermethod = BM_DEFERRED;
			}
		}
	}

	int v;
	
	if (!gl.legacyMode && !(gl.flags & RFL_SHADER_STORAGE_BUFFER))
	{
		glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &v);
		gl.maxuniforms = v;
		glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &v);
		gl.maxuniformblock = v;
		glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &v);
		gl.uniformblockalignment = v;
	}
	else
	{
		gl.maxuniforms = 0;
		gl.maxuniformblock = 0;
		gl.uniformblockalignment = 0;
	}
	

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &gl.max_texturesize);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	if (gl.legacyMode)
	{
		// fudge a bit with the framebuffer stuff to avoid redundancies in the main code. Some of the older cards do not have the ARB stuff but the calls are nearly identical.
		FUDGE_FUNC(glGenerateMipmap, EXT);
		FUDGE_FUNC(glGenFramebuffers, EXT);
		FUDGE_FUNC(glBindFramebuffer, EXT);
		FUDGE_FUNC(glDeleteFramebuffers, EXT);
		FUDGE_FUNC(glFramebufferTexture2D, EXT);
		FUDGE_FUNC(glGenerateMipmap, EXT);
		FUDGE_FUNC(glGenFramebuffers, EXT);
		FUDGE_FUNC(glBindFramebuffer, EXT);
		FUDGE_FUNC(glDeleteFramebuffers, EXT);
		FUDGE_FUNC(glFramebufferTexture2D, EXT);
		FUDGE_FUNC(glFramebufferRenderbuffer, EXT);
		FUDGE_FUNC(glGenRenderbuffers, EXT);
		FUDGE_FUNC(glDeleteRenderbuffers, EXT);
		FUDGE_FUNC(glRenderbufferStorage, EXT);
		FUDGE_FUNC(glBindRenderbuffer, EXT);
		FUDGE_FUNC(glCheckFramebufferStatus, EXT);
	}

	UCVarValue value;
	value.Bool = gl.legacyMode;
	gl_legacy_mode.ForceSet (value, CVAR_Bool);
}

//==========================================================================
//
// 
//
//==========================================================================

void gl_PrintStartupLog()
{
	int v = 0;
	if (!gl.legacyMode) glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &v);

	Printf ("GL_VENDOR: %s\n", glGetString(GL_VENDOR));
	Printf ("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
	Printf ("GL_VERSION: %s (%s profile)\n", glGetString(GL_VERSION), (v & GL_CONTEXT_CORE_PROFILE_BIT)? "Core" : "Compatibility");
	Printf ("GL_SHADING_LANGUAGE_VERSION: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
	Printf (PRINT_LOG, "GL_EXTENSIONS:");
	for (unsigned i = 0; i < m_Extensions.Size(); i++)
	{
		Printf(PRINT_LOG, " %s", m_Extensions[i].GetChars());
	}

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &v);
	Printf("\nMax. texture size: %d\n", v);
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &v);
	Printf ("Max. texture units: %d\n", v);
	glGetIntegerv(GL_MAX_VARYING_FLOATS, &v);
	Printf ("Max. varying: %d\n", v);
	
	if (!gl.legacyMode && !(gl.flags & RFL_SHADER_STORAGE_BUFFER))
	{
		glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &v);
		Printf ("Max. uniform block size: %d\n", v);
		glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &v);
		Printf ("Uniform block alignment: %d\n", v);
	}

	if (gl.flags & RFL_SHADER_STORAGE_BUFFER)
	{
		glGetIntegerv(GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS, &v);
		Printf("Max. combined shader storage blocks: %d\n", v);
		glGetIntegerv(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS, &v);
		Printf("Max. vertex shader storage blocks: %d\n", v);
	}

	// For shader-less, the special alphatexture translation must be changed to actually set the alpha, because it won't get translated by a shader.
	if (gl.legacyMode)
	{
		FRemapTable *remap = translationtables[TRANSLATION_Standard][8];
		for (int i = 0; i < 256; i++)
		{
			remap->Remap[i] = i;
			remap->Palette[i] = PalEntry(i, 255, 255, 255);
		}
	}

}

std::pair<double, bool> gl_getInfo()
{
	// gl_ARB_bindless_texture is the closest we can get to determine Vulkan support from OpenGL.
	// This isn't foolproof because Intel doesn't support it but for NVidia and AMD support of this extension means Vulkan support.
	return std::make_pair(realglversion, CheckExtension("GL_ARB_bindless_texture"));
}
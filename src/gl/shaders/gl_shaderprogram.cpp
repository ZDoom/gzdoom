// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2016 Magnus Norddahl
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
** gl_shaderprogram.cpp
** GLSL shader program compile and link
**
*/

#include "gl_load/gl_system.h"
#include "v_video.h"
#include "gl_load/gl_interface.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "gl/system/gl_debug.h"
#include "gl/shaders/gl_shaderprogram.h"
#include "hwrenderer/utility/hw_shaderpatcher.h"
#include "w_wad.h"

FShaderProgram::FShaderProgram()
{
	for (int i = 0; i < NumShaderTypes; i++)
		mShaders[i] = 0;
}

//==========================================================================
//
// Free shader program resources
//
//==========================================================================

FShaderProgram::~FShaderProgram()
{
	if (mProgram != 0)
		glDeleteProgram(mProgram);

	for (int i = 0; i < NumShaderTypes; i++)
	{
		if (mShaders[i] != 0)
			glDeleteShader(mShaders[i]);
	}
}

//==========================================================================
//
// Creates an OpenGL shader object for the specified type of shader
//
//==========================================================================

void FShaderProgram::CreateShader(ShaderType type)
{
	GLenum gltype = 0;
	switch (type)
	{
	default:
	case Vertex: gltype = GL_VERTEX_SHADER; break;
	case Fragment: gltype = GL_FRAGMENT_SHADER; break;
	}
	mShaders[type] = glCreateShader(gltype);
}

//==========================================================================
//
// Compiles a shader and attaches it the program object
//
//==========================================================================

void FShaderProgram::Compile(ShaderType type, const char *lumpName, const char *defines, int maxGlslVersion)
{
	int lump = Wads.CheckNumForFullName(lumpName, 0);
	if (lump == -1) I_FatalError("Unable to load '%s'", lumpName);
	FString code = Wads.ReadLump(lump).GetString().GetChars();
	Compile(type, lumpName, code, defines, maxGlslVersion);
}

void FShaderProgram::Compile(ShaderType type, const char *name, const FString &code, const char *defines, int maxGlslVersion)
{
	CreateShader(type);

	const auto &handle = mShaders[type];

	FGLDebug::LabelObject(GL_SHADER, handle, name);

	FString patchedCode = PatchShader(type, code, defines, maxGlslVersion);
	int lengths[1] = { (int)patchedCode.Len() };
	const char *sources[1] = { patchedCode.GetChars() };
	glShaderSource(handle, 1, sources, lengths);

	glCompileShader(handle);

	GLint status = 0;
	glGetShaderiv(handle, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE)
	{
		I_FatalError("Compile Shader '%s':\n%s\n", name, GetShaderInfoLog(handle).GetChars());
	}
	else
	{
		if (mProgram == 0)
			mProgram = glCreateProgram();
		glAttachShader(mProgram, handle);
	}
}

//==========================================================================
//
// Links a program with the compiled shaders
//
//==========================================================================

void FShaderProgram::Link(const char *name)
{
	FGLDebug::LabelObject(GL_PROGRAM, mProgram, name);
	glLinkProgram(mProgram);

	GLint status = 0;
	glGetProgramiv(mProgram, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)
	{
		I_FatalError("Link Shader '%s':\n%s\n", name, GetProgramInfoLog(mProgram).GetChars());
	}

	// This is only for old OpenGL which didn't allow to set the binding from within the shader.
	if (screen->glslversion < 4.20)
	{
		glUseProgram(mProgram);
		for (auto &uni : samplerstobind)
		{
			auto index = glGetUniformLocation(mProgram, uni.first);
			if (index >= 0)
			{
				glUniform1i(index, uni.second);
			}
		}
	}
	samplerstobind.Clear();
	samplerstobind.ShrinkToFit();
}

//==========================================================================
//
// Set uniform buffer location (only useful for GL 3.3)
//
//==========================================================================

void FShaderProgram::SetUniformBufferLocation(int index, const char *name)
{
	if (screen->glslversion < 4.20)
	{
		GLuint uniformBlockIndex = glGetUniformBlockIndex(mProgram, name);
		if (uniformBlockIndex != GL_INVALID_INDEX)
			glUniformBlockBinding(mProgram, uniformBlockIndex, index);
	}
}

//==========================================================================
//
// Makes the shader the active program
//
//==========================================================================

void FShaderProgram::Bind(IRenderQueue *)
{
	glUseProgram(mProgram);
}

//==========================================================================
//
// Returns the shader info log (warnings and compile errors)
//
//==========================================================================

FString FShaderProgram::GetShaderInfoLog(GLuint handle)
{
	static char buffer[10000];
	GLsizei length = 0;
	buffer[0] = 0;
	glGetShaderInfoLog(handle, 10000, &length, buffer);
	return FString(buffer);
}

//==========================================================================
//
// Returns the program info log (warnings and compile errors)
//
//==========================================================================

FString FShaderProgram::GetProgramInfoLog(GLuint handle)
{
	static char buffer[10000];
	GLsizei length = 0;
	buffer[0] = 0;
	glGetProgramInfoLog(handle, 10000, &length, buffer);
	return FString(buffer);
}

//==========================================================================
//
// Patches a shader to be compatible with the version of OpenGL in use
//
//==========================================================================

FString FShaderProgram::PatchShader(ShaderType type, const FString &code, const char *defines, int maxGlslVersion)
{
	FString patchedCode;

	// If we have 4.2, always use it because it adds important new syntax.
	if (maxGlslVersion < 420 && gl.glslversion >= 4.2f) maxGlslVersion = 420;
	int shaderVersion = MIN((int)round(gl.glslversion * 10) * 10, maxGlslVersion);
	patchedCode.AppendFormat("#version %d\n", shaderVersion);

	// TODO: Find some way to add extension requirements to the patching
	//
	// #extension GL_ARB_uniform_buffer_object : require
	// #extension GL_ARB_shader_storage_buffer_object : require

	if (defines)
		patchedCode << defines;

	// these settings are actually pointless but there seem to be some old ATI drivers that fail to compile the shader without setting the precision here.
	patchedCode << "precision highp int;\n";
	patchedCode << "precision highp float;\n";

	patchedCode << "#line 1\n";
	patchedCode << code;

	if (maxGlslVersion < 420)
	{
		// Here we must strip out all layout(binding) declarations for sampler uniforms and store them in 'samplerstobind' which can then be processed by the link function.
		patchedCode = RemoveSamplerBindings(patchedCode, samplerstobind);
	}

	return patchedCode;
}

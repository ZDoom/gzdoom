/*
** gl_shaderprogram.cpp
** GLSL shader program compile and link
**
**---------------------------------------------------------------------------
** Copyright 2016 Magnus Norddahl
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
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
** 5. Full disclosure of the entire project's source code, except for third
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
#include "files.h"
#include "m_swap.h"
#include "v_video.h"
#include "gl/gl_functions.h"
#include "vectors.h"
#include "gl/system/gl_interface.h"
#include "gl/system/gl_cvars.h"
#include "gl/shaders/gl_shaderprogram.h"
#include "w_wad.h"
#include "i_system.h"
#include "doomerrors.h"

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
	int lump = Wads.CheckNumForFullName(lumpName);
	if (lump == -1) I_FatalError("Unable to load '%s'", lumpName);
	FString code = Wads.ReadLump(lump).GetString().GetChars();
	Compile(type, lumpName, code, defines, maxGlslVersion);
}

void FShaderProgram::Compile(ShaderType type, const char *name, const FString &code, const char *defines, int maxGlslVersion)
{
	CreateShader(type);

	const auto &handle = mShaders[type];

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
// Binds a fragment output variable to a frame buffer render target
//
//==========================================================================

void FShaderProgram::SetFragDataLocation(int index, const char *name)
{
	glBindFragDataLocation(mProgram, index, name);
}

//==========================================================================
//
// Links a program with the compiled shaders
//
//==========================================================================

void FShaderProgram::Link(const char *name)
{
	glLinkProgram(mProgram);

	GLint status = 0;
	glGetProgramiv(mProgram, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)
	{
		I_FatalError("Link Shader '%s':\n%s\n", name, GetProgramInfoLog(mProgram).GetChars());
	}
}

//==========================================================================
//
// Set vertex attribute location
//
//==========================================================================

void FShaderProgram::SetAttribLocation(int index, const char *name)
{
	glBindAttribLocation(mProgram, index, name);
}

//==========================================================================
//
// Makes the shader the active program
//
//==========================================================================

void FShaderProgram::Bind()
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

	int shaderVersion = MIN((int)round(gl.glslversion * 10) * 10, maxGlslVersion);
	patchedCode.AppendFormat("#version %d\n", shaderVersion);

	// TODO: Find some way to add extension requirements to the patching
	//
	// #extension GL_ARB_uniform_buffer_object : require
	// #extension GL_ARB_shader_storage_buffer_object : require

	if (defines)
		patchedCode << defines;

	if (gl.glslversion >= 1.3)
	{
		// these settings are actually pointless but there seem to be some old ATI drivers that fail to compile the shader without setting the precision here.
		patchedCode << "precision highp int;\n";
		patchedCode << "precision highp float;\n";
	}

	patchedCode << "#line 1\n";
	patchedCode << code;

	if (gl.glslversion < 1.3)
	{
		if (type == Vertex)
			PatchVertShader(patchedCode);
		else if (type == Fragment)
			PatchFragShader(patchedCode);
	}

	return patchedCode;
}

//==========================================================================
//
// patch the shader source to work with 
// GLSL 1.2 keywords and identifiers
//
//==========================================================================

void FShaderProgram::PatchCommon(FString &code)
{
	code.Substitute("precision highp int;", "");
	code.Substitute("precision highp float;", "");
}

void FShaderProgram::PatchVertShader(FString &code)
{
	PatchCommon(code);
	code.Substitute("in vec", "attribute vec");
	code.Substitute("in float", "attribute float");
	code.Substitute("out vec", "varying vec");
	code.Substitute("out float", "varying float");
	code.Substitute("gl_ClipDistance", "//");
}

void FShaderProgram::PatchFragShader(FString &code)
{
	PatchCommon(code);
	code.Substitute("out vec4 FragColor;", "");
	code.Substitute("FragColor", "gl_FragColor");
	code.Substitute("in vec", "varying vec");
	// this patches the switch statement to if's.
	code.Substitute("break;", "");
	code.Substitute("switch (uFixedColormap)", "int i = uFixedColormap;");
	code.Substitute("case 0:", "if (i == 0)");
	code.Substitute("case 1:", "else if (i == 1)");
	code.Substitute("case 2:", "else if (i == 2)");
	code.Substitute("case 3:", "else if (i == 3)");
	code.Substitute("case 4:", "else if (i == 4)");
	code.Substitute("case 5:", "else if (i == 5)");
	code.Substitute("texture(", "texture2D(");
}

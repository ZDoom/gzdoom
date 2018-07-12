// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2018 Christoph Oelckers
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
** gl_swshader20.cpp
**
** Implements the shaders used to render the software renderer's
** 3D output to the screen,
**
*/

#include "gl_load/gl_system.h"
#include "tarray.h"
#include "doomtype.h"
#include "r_utility.h"
#include "w_wad.h"

#include "gl/renderer/gl_renderer.h"


class LegacyShader
{
public:
	~LegacyShader()
	{
		if (Program != 0) glDeleteProgram(Program);
		if (VertexShader != 0) glDeleteShader(VertexShader);
		if (FragmentShader != 0) glDeleteShader(FragmentShader);
	}

	int Program = 0;
	int VertexShader = 0;
	int FragmentShader = 0;

	int ConstantLocations[2];
	int ImageLocation = -1;
	int PaletteLocation = -1;
};

//---------------------------------------------------------------------------
//
// 
//
//---------------------------------------------------------------------------

LegacyShaderContainer::LegacyShaderContainer()
{
	if (!LoadShaders())
	{
		Printf("Unable to compile shaders for software rendering\n");
	}
}

LegacyShaderContainer::~LegacyShaderContainer()
{
	for (auto p : Shaders) if (p) delete p;
}

LegacyShader* LegacyShaderContainer::CreatePixelShader(const FString& vertex, const FString& fragment, const FString &defines)
{
	auto shader = new LegacyShader();
	char buffer[10000];

	shader->Program = glCreateProgram();
	if (shader->Program == 0) { Printf("glCreateProgram failed. Disabling OpenGL hardware acceleration.\n"); return nullptr; }
	shader->VertexShader = glCreateShader(GL_VERTEX_SHADER);
	if (shader->VertexShader == 0) { Printf("glCreateShader(GL_VERTEX_SHADER) failed. Disabling OpenGL hardware acceleration.\n"); return nullptr; }
	shader->FragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	if (shader->FragmentShader == 0) { Printf("glCreateShader(GL_FRAGMENT_SHADER) failed. Disabling OpenGL hardware acceleration.\n"); return nullptr; }
	
	{
		FString vertexsrc = defines + vertex;
		int lengths[1] = { (int)vertexsrc.Len() };
		const char *sources[1] = { vertexsrc.GetChars() };
		glShaderSource(shader->VertexShader, 1, sources, lengths);
		glCompileShader(shader->VertexShader);
	}

	{
		FString fragmentsrc = defines + fragment;
		int lengths[1] = { (int)fragmentsrc.Len() };
		const char *sources[1] = { fragmentsrc.GetChars() };
		glShaderSource(shader->FragmentShader, 1, sources, lengths);
		glCompileShader(shader->FragmentShader);
	}

	GLint status = 0;
	int errorShader = shader->VertexShader;
	glGetShaderiv(shader->VertexShader, GL_COMPILE_STATUS, &status);
	if (status != GL_FALSE) 
	{ 
		errorShader = shader->FragmentShader; 
		glGetShaderiv(shader->FragmentShader, GL_COMPILE_STATUS, &status); 
	}
	if (status == GL_FALSE)
	{
		GLsizei length = 0;
		buffer[0] = 0;
		glGetShaderInfoLog(errorShader, 10000, &length, buffer);
		Printf("Shader compile failed: %s\n", buffer);
		delete shader;
		return nullptr;
	}

	glAttachShader(shader->Program, shader->VertexShader);
	glAttachShader(shader->Program, shader->FragmentShader);
	glLinkProgram(shader->Program);
	glGetProgramiv(shader->Program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)
	{
		GLsizei length = 0;
		buffer[0] = 0;
		glGetProgramInfoLog(shader->Program, 10000, &length, buffer);
		Printf("Shader link failed: %s\n", buffer);
		delete shader;
		return nullptr;
	}

	shader->ConstantLocations[0] = glGetUniformLocation(shader->Program, "uColor1");
	shader->ConstantLocations[1] = glGetUniformLocation(shader->Program, "uColor2");
	shader->ImageLocation = glGetUniformLocation(shader->Program, "tex");
	shader->PaletteLocation = glGetUniformLocation(shader->Program, "palette");

	return shader;
}

//==========================================================================
//
// SWSceneDrawer :: LoadShaders
//
// Returns true if all required shaders were loaded. (Gamma and burn wipe
// are the only ones not considered "required".)
//
//==========================================================================

static const char * const ShaderDefines[] = {
	"#define PAL_TEX\n",
	"#define PAL_TEX\n#define SPECIALCM\n",
	"",
	"#define SPECIALCM\n",
};

bool LegacyShaderContainer::LoadShaders()
{
	int lumpvert = Wads.CheckNumForFullName("shaders/swgl/swshadergl2.vp");
	int lumpfrag = Wads.CheckNumForFullName("shaders/swgl/swshadergl2.fp");
	if (lumpvert < 0 || lumpfrag < 0)
		return false;

	FString vertsource = Wads.ReadLump(lumpvert).GetString();
	FString fragsource = Wads.ReadLump(lumpfrag).GetString();

	FString shaderdir, shaderpath;
	unsigned int i;

	for (i = 0; i < NUM_SHADERS; ++i)
	{
		shaderpath = shaderdir;
		Shaders[i] = CreatePixelShader(vertsource, fragsource, ShaderDefines[i]);
		if (!Shaders[i])
		{
			break;
		}

		glUseProgram(Shaders[i]->Program);
		if (Shaders[i]->ImageLocation != -1) glUniform1i(Shaders[i]->ImageLocation, 0);
		if (Shaders[i]->PaletteLocation != -1) glUniform1i(Shaders[i]->PaletteLocation, 1);
		glUseProgram(0);
	}
	if (i == NUM_SHADERS)
	{ // Success!
		return true;
	}
	// Failure. Release whatever managed to load (which is probably nothing.)
	for (i = 0; i < NUM_SHADERS; ++i)
	{
		if (Shaders[i]) delete Shaders[i];
	}
	return false;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void LegacyShaderContainer::BindShader(int num, const float *p1, const float *p2)
{
	if (num >= 0 && num < 4)
	{
		auto shader = Shaders[num];
		glUseProgram(shader->Program);
		glUniform4fv(shader->ConstantLocations[0], 1, p1);
		glUniform4fv(shader->ConstantLocations[1], 1, p2);
	}
}


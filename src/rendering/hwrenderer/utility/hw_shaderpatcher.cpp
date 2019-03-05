// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2004-2018 Christoph Oelckers
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
** hw_shaderpatcher.cpp
**
** Modifies shader source to account for different syntax versions or engine changes.
**
*/


#include "hw_shaderpatcher.h"


static bool IsGlslWhitespace(char c)
{
	switch (c)
	{
	case ' ':
	case '\r':
	case '\n':
	case '\t':
	case '\f':
		return true;
	default:
		return false;
	}
}

static FString NextGlslToken(const char *chars, long len, long &pos)
{
	// Eat whitespace
	long tokenStart = pos;
	while (tokenStart != len && IsGlslWhitespace(chars[tokenStart]))
		tokenStart++;

	// Find token end
	long tokenEnd = tokenStart;
	while (tokenEnd != len && !IsGlslWhitespace(chars[tokenEnd]) && chars[tokenEnd] != ';')
		tokenEnd++;

	pos = tokenEnd;
	return FString(chars + tokenStart, tokenEnd - tokenStart);
}

static bool isShaderType(const char *name)
{
	return !strcmp(name, "sampler1D") || !strcmp(name, "sampler2D") || !strcmp(name, "sampler3D") || !strcmp(name, "samplerCube") || !strcmp(name, "sampler2DMS");
}

FString RemoveLegacyUserUniforms(FString code)
{
	// User shaders must declare their uniforms via the GLDEFS file.
	// The following code searches for legacy uniform declarations in the shader itself and replaces them with whitespace.

	long len = (long)code.Len();
	char *chars = code.LockBuffer();

	long startIndex = 0;
	while (true)
	{
		long matchIndex = code.IndexOf("uniform", startIndex);
		if (matchIndex == -1)
			break;

		bool isLegacyUniformName = false;

		bool isKeywordStart = matchIndex == 0 || IsGlslWhitespace(chars[matchIndex - 1]);
		bool isKeywordEnd = matchIndex + 7 == len || IsGlslWhitespace(chars[matchIndex + 7]);
		if (isKeywordStart && isKeywordEnd)
		{
			long pos = matchIndex + 7;
			FString type = NextGlslToken(chars, len, pos);
			FString identifier = NextGlslToken(chars, len, pos);

			isLegacyUniformName = type.Compare("float") == 0 && identifier.Compare("timer") == 0;
		}

		if (isLegacyUniformName)
		{
			long statementEndIndex = code.IndexOf(';', matchIndex + 7);
			if (statementEndIndex == -1)
				statementEndIndex = len;
			for (long i = matchIndex; i <= statementEndIndex; i++)
			{
				if (!IsGlslWhitespace(chars[i]))
					chars[i] = ' ';
			}
			startIndex = statementEndIndex;
		}
		else
		{
			startIndex = matchIndex + 7;
		}
	}

	code.UnlockBuffer();

	return code;
}

FString RemoveSamplerBindings(FString code, TArray<std::pair<FString, int>> &samplerstobind)
{
	long len = (long)code.Len();
	char *chars = code.LockBuffer();

	long startIndex = 0;
	long startpos, endpos;
	while (true)
	{
		long matchIndex = code.IndexOf("layout(binding", startIndex);
		if (matchIndex == -1)
			break;

		bool isSamplerUniformName = false;

		bool isKeywordStart = matchIndex == 0 || IsGlslWhitespace(chars[matchIndex - 1]);
		bool isKeywordEnd = matchIndex + 14 == len || IsGlslWhitespace(chars[matchIndex + 14]) || chars[matchIndex + 14] == '=';
		if (isKeywordStart && isKeywordEnd)
		{
			long pos = matchIndex + 14;
			startpos = matchIndex;
			while (IsGlslWhitespace(chars[pos])) pos++;
			if (chars[pos] == '=')
			{
				char *p;
				pos++;
				auto val = strtol(&chars[pos], &p, 0);
				if (p != &chars[pos])
				{
					pos = long(p - chars);
					while (IsGlslWhitespace(chars[pos])) pos++;
					if (chars[pos] == ')')
					{
						endpos = ++pos;
						FString uniform = NextGlslToken(chars, len, pos);
						FString type = NextGlslToken(chars, len, pos);
						FString identifier = NextGlslToken(chars, len, pos);

						isSamplerUniformName = uniform.Compare("uniform") == 0 && isShaderType(type);
						if (isSamplerUniformName)
						{
							samplerstobind.Push(std::make_pair(identifier, val));
							for (auto pos = startpos; pos < endpos; pos++)
							{
								if (!IsGlslWhitespace(chars[pos]))
									chars[pos] = ' ';
							}
						}
					}
				}
			}
		}

		if (isSamplerUniformName)
		{
			startIndex = endpos;
		}
		else
		{
			startIndex = matchIndex + 7;
		}
	}

	code.UnlockBuffer();

	return code;
}

FString RemoveLayoutLocationDecl(FString code, const char *inoutkeyword)
{
	long len = (long)code.Len();
	char *chars = code.LockBuffer();

	long startIndex = 0;
	while (true)
	{
		long matchIndex = code.IndexOf("layout(location", startIndex);
		if (matchIndex == -1)
			break;

		long endIndex = startIndex;

		// Find end of layout declaration
		while (chars[endIndex] != ')' && chars[endIndex] != 0)
			endIndex++;

		// Skip whitespace
		while (IsGlslWhitespace(chars[endIndex]))
			endIndex++;

		// keyword following the declaration?
		bool keywordFound = true;
		long i;
		for (i = 0; inoutkeyword[i] != 0; i++)
		{
			if (chars[endIndex + i] != inoutkeyword[i])
			{
				keywordFound = false;
				break;
			}
		}
		if (keywordFound && IsGlslWhitespace(chars[endIndex + i]))
		{
			// yes - replace declaration with spaces
			for (long i = startIndex; i < endIndex; i++)
				chars[i] = ' ';
		}

		startIndex = endIndex;
	}

	code.UnlockBuffer();

	return code;
}

/////////////////////////////////////////////////////////////////////////////

// Note: the MaterialShaderIndex enum in gl_shader.h needs to be updated whenever this array is modified.
const FDefaultShader defaultshaders[] =
{
	{"Default",	"shaders/glsl/func_normal.fp", "shaders/glsl/material_normal.fp", ""},
	{"Warp 1",	"shaders/glsl/func_warp1.fp", "shaders/glsl/material_normal.fp", ""},
	{"Warp 2",	"shaders/glsl/func_warp2.fp", "shaders/glsl/material_normal.fp", ""},
	{"Brightmap","shaders/glsl/func_brightmap.fp", "shaders/glsl/material_normal.fp", "#define BRIGHTMAP\n"},
	{"Specular", "shaders/glsl/func_spec.fp", "shaders/glsl/material_specular.fp", "#define SPECULAR\n#define NORMALMAP\n"},
	{"SpecularBrightmap", "shaders/glsl/func_spec.fp", "shaders/glsl/material_specular.fp", "#define SPECULAR\n#define NORMALMAP\n#define BRIGHTMAP\n"},
	{"PBR","shaders/glsl/func_pbr.fp", "shaders/glsl/material_pbr.fp", "#define PBR\n#define NORMALMAP\n"},
	{"PBRBrightmap","shaders/glsl/func_pbr.fp", "shaders/glsl/material_pbr.fp", "#define PBR\n#define NORMALMAP\n#define BRIGHTMAP\n"},
	{"Paletted",	"shaders/glsl/func_paletted.fp", "shaders/glsl/material_nolight.fp", ""},
	{"No Texture", "shaders/glsl/func_notexture.fp", "shaders/glsl/material_normal.fp", ""},
	{"Basic Fuzz", "shaders/glsl/fuzz_standard.fp", "shaders/glsl/material_normal.fp", ""},
	{"Smooth Fuzz", "shaders/glsl/fuzz_smooth.fp", "shaders/glsl/material_normal.fp", ""},
	{"Swirly Fuzz", "shaders/glsl/fuzz_swirly.fp", "shaders/glsl/material_normal.fp", ""},
	{"Translucent Fuzz", "shaders/glsl/fuzz_smoothtranslucent.fp", "shaders/glsl/material_normal.fp", ""},
	{"Jagged Fuzz", "shaders/glsl/fuzz_jagged.fp", "shaders/glsl/material_normal.fp", ""},
	{"Noise Fuzz", "shaders/glsl/fuzz_noise.fp", "shaders/glsl/material_normal.fp", ""},
	{"Smooth Noise Fuzz", "shaders/glsl/fuzz_smoothnoise.fp", "shaders/glsl/material_normal.fp", ""},
	{"Software Fuzz", "shaders/glsl/fuzz_software.fp", "shaders/glsl/material_normal.fp", ""},
	{nullptr,nullptr,nullptr,nullptr}
};

const FEffectShader effectshaders[] =
{
	{ "fogboundary", "shaders/glsl/main.vp", "shaders/glsl/fogboundary.fp", nullptr, nullptr, "#define NO_ALPHATEST\n" },
	{ "spheremap", "shaders/glsl/main.vp", "shaders/glsl/main.fp", "shaders/glsl/func_normal.fp", "shaders/glsl/material_normal.fp", "#define SPHEREMAP\n#define NO_ALPHATEST\n" },
	{ "burn", "shaders/glsl/main.vp", "shaders/glsl/burn.fp", nullptr, nullptr, "#define SIMPLE\n#define NO_ALPHATEST\n" },
	{ "stencil", "shaders/glsl/main.vp", "shaders/glsl/stencil.fp", nullptr, nullptr, "#define SIMPLE\n#define NO_ALPHATEST\n" },
};

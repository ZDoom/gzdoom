/*
** hw_shaderpatcher.cpp
** Modifies shader source to account for different syntax versions or engine changes.
**
**---------------------------------------------------------------------------
** Copyright(C) 2004-2018 Christoph Oelckers
** Copyright(C) 2016-2018 Magnus Norddahl
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


#include "cmdlib.h"
#include "hw_shaderpatcher.h"
#include "textures.h"
#include "hw_renderstate.h"
#include "v_video.h"
#include "printf.h"


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

static FString NextGlslToken(const char *chars, ptrdiff_t len, ptrdiff_t &pos)
{
	// Eat whitespace
	ptrdiff_t tokenStart = pos;
	while (tokenStart != len && IsGlslWhitespace(chars[tokenStart]))
		tokenStart++;

	// Find token end
	ptrdiff_t tokenEnd = tokenStart;
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

	bool found = code.Substitute("uniform sampler2D tex;", "                      ");
	found = code.Substitute("uniform float timer;", "                    ") || found;

	// The following code searches for legacy uniform declarations in the shader itself and replaces them with whitespace.

	ptrdiff_t len = code.Len();
	char *chars = code.LockBuffer();

	ptrdiff_t startIndex = 0;
	while (true)
	{
		ptrdiff_t matchIndex = code.IndexOf("uniform", startIndex);
		if (matchIndex == -1)
			break;

		bool isLegacyUniformName = false;

		bool isKeywordStart = matchIndex == 0 || IsGlslWhitespace(chars[matchIndex - 1]);
		bool isKeywordEnd = matchIndex + 7 == len || IsGlslWhitespace(chars[matchIndex + 7]);
		if (isKeywordStart && isKeywordEnd)
		{
			ptrdiff_t pos = matchIndex + 7;
			FString type = NextGlslToken(chars, len, pos);
			FString identifier = NextGlslToken(chars, len, pos);

			isLegacyUniformName = type.Compare("float") == 0 && identifier.Compare("timer") == 0;
		}

		if (isLegacyUniformName)
		{
			ptrdiff_t statementEndIndex = code.IndexOf(';', matchIndex + 7);
			if (statementEndIndex == -1)
				statementEndIndex = len;
			for (ptrdiff_t i = matchIndex; i <= statementEndIndex; i++)
			{
				if (!IsGlslWhitespace(chars[i]))
					chars[i] = ' ';
			}
			startIndex = statementEndIndex;
			found = true;
		}
		else
		{
			startIndex = matchIndex + 7;
		}
	}

	if(found)
	{
		DPrintf(DMSG_WARNING, TEXTCOLOR_ORANGE "timer and tex uniforms should not be explicitly declared.\n");
	}

	bool foundtexture2d = false;

	// Also remove all occurences of the token 'texture2d'. Some shaders may still use this deprecated function to access a sampler.
	// Modern GLSL only allows use of 'texture'.
	while (true)
	{
		ptrdiff_t matchIndex = code.IndexOf("texture2d", startIndex);
		if (matchIndex == -1)
			break;

		foundtexture2d = true;

		// Check if this is a real token.
		bool isKeywordStart = matchIndex == 0 || !isalnum(chars[matchIndex - 1] & 255);
		bool isKeywordEnd = matchIndex + 9 == len || !isalnum(chars[matchIndex + 9] & 255);
		if (isKeywordStart && isKeywordEnd)
		{
			chars[matchIndex + 7] = chars[matchIndex + 8] = ' ';
		}
		startIndex = matchIndex + 9;
	}

	if(foundtexture2d)
	{
		DPrintf(DMSG_WARNING, TEXTCOLOR_ORANGE "texture2d is deprecated, use texture instead.\n");
	}

	code.UnlockBuffer();

	return code;
}

FString RemoveSamplerBindings(FString code, TArray<std::pair<FString, int>> &samplerstobind)
{
	ptrdiff_t len = code.Len();
	char *chars = code.LockBuffer();

	ptrdiff_t startIndex = 0;
	ptrdiff_t startpos, endpos = 0;
	while (true)
	{
		ptrdiff_t matchIndex = code.IndexOf("layout(binding", startIndex);
		if (matchIndex == -1)
			break;

		bool isSamplerUniformName = false;

		bool isKeywordStart = matchIndex == 0 || IsGlslWhitespace(chars[matchIndex - 1]);
		bool isKeywordEnd = matchIndex + 14 == len || IsGlslWhitespace(chars[matchIndex + 14]) || chars[matchIndex + 14] == '=';
		if (isKeywordStart && isKeywordEnd)
		{
			ptrdiff_t pos = matchIndex + 14;
			startpos = matchIndex;
			while (IsGlslWhitespace(chars[pos])) pos++;
			if (chars[pos] == '=')
			{
				char *p;
				pos++;
				auto val = strtol(&chars[pos], &p, 0);
				if (p != &chars[pos])
				{
					pos = (p - chars);
					while (IsGlslWhitespace(chars[pos])) pos++;
					if (chars[pos] == ')')
					{
						endpos = ++pos;
						FString uniform = NextGlslToken(chars, len, pos);
						FString type = NextGlslToken(chars, len, pos);
						FString identifier = NextGlslToken(chars, len, pos);

						isSamplerUniformName = uniform.Compare("uniform") == 0 && isShaderType(type.GetChars());
						if (isSamplerUniformName)
						{
							samplerstobind.Push(std::make_pair(identifier, val));
							for (auto posi = startpos; posi < endpos; posi++)
							{
								if (!IsGlslWhitespace(chars[posi]))
									chars[posi] = ' ';
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
	char *chars = code.LockBuffer();

	ptrdiff_t startIndex = 0;
	while (true)
	{
		ptrdiff_t matchIndex = code.IndexOf("layout(location", startIndex);
		if (matchIndex == -1)
			break;

		ptrdiff_t endIndex = matchIndex;

		// Find end of layout declaration
		while (chars[endIndex] != ')' && chars[endIndex] != 0)
			endIndex++;

		if (chars[endIndex] == ')')
			endIndex++;
		else if (chars[endIndex] == 0)
			break;

		// Skip whitespace
		while (IsGlslWhitespace(chars[endIndex]))
			endIndex++;

		// keyword following the declaration?
		bool keywordFound = true;
		ptrdiff_t i;
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
			for (auto ii = matchIndex; ii < endIndex; ii++)
				chars[ii] = ' ';
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
	{"Specular", "shaders/glsl/func_spec.fp", "shaders/glsl/material_specular.fp", "#define SPECULAR\n#define NORMALMAP\n"},
	{"PBR","shaders/glsl/func_pbr.fp", "shaders/glsl/material_pbr.fp", "#define PBR\n#define NORMALMAP\n"},
	{"Paletted",	"shaders/glsl/func_paletted.fp", "shaders/glsl/material_nolight.fp", "#define PALETTE_EMULATION\n"},
	{"No Texture", "shaders/glsl/func_notexture.fp", "shaders/glsl/material_normal.fp", "#define NO_LAYERS\n"},
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
	{ "dithertrans", "shaders/glsl/main.vp", "shaders/glsl/main.fp", "shaders/glsl/func_normal.fp", "shaders/glsl/material_normal.fp", "#define NO_ALPHATEST\n#define DITHERTRANS\n" },
};

int DFrameBuffer::GetShaderCount()
{
	int i;
	for (i = 0; defaultshaders[i].ShaderName != nullptr; i++);

	return MAX_PASS_TYPES * (countof(defaultshaders) - 1 + usershaders.Size() + MAX_EFFECTS + SHADER_NoTexture);
}


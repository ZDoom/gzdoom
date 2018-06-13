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


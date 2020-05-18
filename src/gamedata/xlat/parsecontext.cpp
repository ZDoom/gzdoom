/*
** parsecontext.cpp
** Base class for Lemon-based parsers
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
** Copyright 2008 Christoph Oelckers
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

#include <string.h>
#include <ctype.h>
#include "filesystem.h"
#include "parsecontext.h"
#include "p_lnspec.h"


//==========================================================================
//
//
//
//==========================================================================
void FParseContext::AddSym (char *sym, int val)
{
	FParseSymbol syme;
	syme.Value = val;
	strncpy (syme.Sym, sym, 79);
	syme.Sym[79]=0;
	symbols.Push(syme);
}

//==========================================================================
//
//
//
//==========================================================================
bool FParseContext::FindSym (char *sym, FParseSymbol **val)
{
	for(unsigned i=0;i<symbols.Size(); i++)
	{
		if (strcmp (symbols[i].Sym, sym) == 0)
		{
			*val = &symbols[i];
			return true;
		}
	}
	return false;
}


//==========================================================================
//
//
//
//==========================================================================
int FParseContext::GetToken (char *&sourcep, FParseToken *yylval)
{
	char token[80];
	int toksize;
	int c;

loop:
	while (isspace (c = *sourcep++) && c != 0)
	{
		if (c == '\n')
			SourceLine++;
	}

	if (c == 0)
	{
		return 0;
	}
	if (isdigit (c))
	{
		int buildup = c - '0';
		if (c == '0')
		{
			c = *sourcep++;
			if (c == 'x' || c == 'X')
			{
				yylval->val = (int)strtoll(sourcep, &sourcep, 16);
				return TokenTrans[NUM];
			}
			else
			{
				sourcep--;
			}
		}
		char *endp;

		sourcep--;
		yylval->val = (int)strtoll(sourcep, &endp, 10);
		if (*endp == '.')
		{
			// It's a float
			yylval->fval = strtod(sourcep, &sourcep);
			return TokenTrans[FLOATVAL];
		}
		else
		{
			sourcep = endp;
			return TokenTrans[NUM];
		}
	}
	if (isalpha (c))
	{
		int buildup = 0;
		
		token[0] = c;
		toksize = 1;
		while (toksize < 79 && (isalnum (c = *sourcep++) || c == '_'))
		{
			token[toksize++] = c;
		}
		token[toksize] = 0;
		if (toksize == 79 && isalnum (c))
		{
			while (isalnum (c = *sourcep++))
				;
		}
		sourcep--;
		if (FindToken (token, &buildup))
		{
			return buildup;
		}
		if (FindSym (token, &yylval->symval))
		{
			yylval->val = yylval->symval->Value;
			return TokenTrans[NUM];
		}
		if ((yylval->val = P_FindLineSpecial(token)) != 0)
		{
			return TokenTrans[NUM];
		}
#if __GNUC__ == 4 && __GNUC_MINOR__ == 8
		// Work around GCC 4.8 bug 54570 causing release build crashes.
		asm("" : "+g" (yylval));
#endif
		strcpy (yylval->sym, token);
		return TokenTrans[SYM];
	}
	if (c == '/')
	{
		c = *sourcep++;
		if (c == '*')
		{
			for (;;)
			{
				while ((c = *sourcep++) != '*' && c != 0)
				{
					if (c == '\n')
						SourceLine++;
				}
				if (c == 0)
					return 0;
				if ((c = *sourcep++) == '/')
					goto loop;
				if (c == 0)
					return 0;
				sourcep--;
			}
		}
		else if (c == '/')
		{
			while ((c = *sourcep++) != '\n' && c != 0)
				;
			if (c == '\n')
				SourceLine++;
			else if (c == EOF)
				return 0;
			goto loop;
		}
		else
		{
			sourcep--;
			return TokenTrans[DIVIDE];
		}
	}
	if (c == '"')
	{
		int tokensize = 0;
		while ((c = *sourcep++) != '"' && c != 0)
		{
			yylval->string[tokensize++] = c;
		}
		yylval->string[tokensize] = 0;
		return TokenTrans[STRING];
	}
	if (c == '|')
	{
		c = *sourcep++;
		if (c == '=')
			return TokenTrans[OR_EQUAL];
		sourcep--;
		return TokenTrans[OR];
	}
	if (c == '<')
	{
		c = *sourcep++;
		if (c == '<')
		{
			c = *sourcep++;
			if (c == '=')
			{
				return TokenTrans[LSHASSIGN];
			}
			sourcep--;
			return 0;
		}
		c--;
		return 0;
	}
	if (c == '>')
	{
		c = *sourcep++;
		if (c == '>')
		{
			c = *sourcep++;
			if (c == '=')
			{
				return TokenTrans[RSHASSIGN];
			}
			sourcep--;
			return 0;
		}
		c--;
		return 0;
	}
	if (c == '#')
	{
		if (!strnicmp(sourcep, "include", 7))
		{
			sourcep+=7;
			return TokenTrans[INCLUDE];
		}
		if (!strnicmp(sourcep, "define", 6))
		{
			sourcep+=6;
			return TokenTrans[DEFINE];
		}
	}
	switch (c)
	{
	case '^': return TokenTrans[XOR];
	case '&': return TokenTrans[AND];
	case '-': return TokenTrans[MINUS];
	case '+': return TokenTrans[PLUS];
	case '*': return TokenTrans[MULTIPLY];
	case '%': return TokenTrans[MODULUS];
	case '(': return TokenTrans[LPAREN];
	case ')': return TokenTrans[RPAREN];
	case ',': return TokenTrans[COMMA];
	case '{': return TokenTrans[LBRACE];
	case '}': return TokenTrans[RBRACE];
	case '=': return TokenTrans[EQUALS];
	case ';': return TokenTrans[SEMICOLON];
	case ':': return TokenTrans[COLON];
	case '[': return TokenTrans[LBRACKET];
	case ']': return TokenTrans[RBRACKET];
	default:  return 0;
	}
}

//==========================================================================
//
//
//
//==========================================================================
int FParseContext::PrintError (const char *s)
{
	if (SourceFile != NULL)
		Printf ("%s, line %d: %s\n", SourceFile, SourceLine, s);
	else
		Printf ("%s\n", s);
	return 0;
}


//==========================================================================
//
//
//
//==========================================================================

void FParseContext::ParseLump(const char *lumpname)
{
	int tokentype;
	int SavedSourceLine = SourceLine;
	const char *SavedSourceFile = SourceFile;
	FParseToken token;

	int lumpno = fileSystem.CheckNumForFullName(lumpname, true);

	if (lumpno == -1) 
	{
		Printf ("%s, line %d: Lump '%s' not found\n", SourceFile, SourceLine, lumpname);
		return;
	}

	// Read the lump into a buffer and add a 0-terminator
	auto lumpdata = fileSystem.GetFileData(lumpno, 1);

	SourceLine = 0;
	SourceFile = lumpname;

	char *sourcep = (char*)lumpdata.Data();
	while ( (tokentype = GetToken(sourcep, &token)) )
	{
		// It is much easier to handle include statements outside the main parser.
		if (tokentype == TokenTrans[INCLUDE])
		{
			if (GetToken(sourcep, &token) != TokenTrans[STRING])
			{
				Printf("%s, line %d: Include: String parameter expected\n", SourceFile, SourceLine);
				return;
			}
			ParseLump(token.string);
		}
		else
		{
			Parse(pParser, tokentype, token, this);
		}
	}
	SourceLine = SavedSourceLine;
	SourceFile = SavedSourceFile;
}


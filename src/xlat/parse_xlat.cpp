/*
** parse_xlat.cpp
** Translation definition compiler
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


#include "doomtype.h"
#include "tarray.h"
#include "w_wad.h"
#include "c_console.h"
#include "p_lnspec.h"
#include "info.h"
#include "gi.h"
#include "xlat_parser.h"
#include "xlat.h"

static FString LastTranslator;
TAutoGrowArray<FLineTrans> SimpleLineTranslations;
FBoomTranslator Boomish[MAX_BOOMISH];
int NumBoomish;
TAutoGrowArray<FSectorTrans> SectorTranslations;
TArray<FSectorMask> SectorMasks;

// Token types not used in the grammar
enum
{
	INCLUDE=128,
	STRING,
};


struct Symbol
{
	int Value;
	char Sym[80];
};

union XlatToken
{
	int val;
	char sym[80];
	char string[80];
	Symbol *symval;
};

struct SpecialArgs
{
	int addflags;
	int args[5];
};

struct ListFilter
{
	WORD filter;
	BYTE value;
};

struct MoreFilters
{
	MoreFilters *next;
	ListFilter filter;
};

struct MoreLines
{
	MoreLines *next;
	FBoomArg arg;
};

struct ParseBoomArg
{
	BYTE constant;
	WORD mask;
	MoreFilters *filters;
};


struct XlatParseContext
{
	TArray<Symbol> symbols;
	int SourceLine;
	const char *SourceFile;
	int EnumVal;

	XlatParseContext()
	{
		SourceLine = 0;
		SourceFile = NULL;
	}

	//==========================================================================
	//
	//
	//
	//==========================================================================
	void AddSym (char *sym, int val)
	{
		Symbol syme;
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
	bool FindSym (char *sym, Symbol **val)
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
	bool FindToken (char *tok, int *type)
	{
		static const char tokens[][10] =
		{
			"include", "define", "enum",
			"arg5", "arg4", "arg3", "arg2", "flags", "lineid", "tag",
			"sector", "bitmask", "nobitmask", "clear"
			
		};
		static const short types[] =
		{
			INCLUDE, DEFINE, ENUM,
			ARG5, ARG4, ARG3, ARG2, FLAGS, TAG, TAG,
			SECTOR, BITMASK, NOBITMASK, CLEAR
		};
		int i;

		for (i = sizeof(tokens)/sizeof(tokens[0])-1; i >= 0; i--)
		{
			if (strcmp (tok, tokens[i]) == 0)
			{
				*type = types[i];
				return 1;
			}
		}
		return 0;
	}

	//==========================================================================
	//
	//
	//
	//==========================================================================
	int GetToken (char *&sourcep, XlatToken *yylval)
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
					for (;;)
					{
						c = *sourcep++;
						if (isdigit (c))
						{
							buildup = (buildup<<4) + c - '0';
						}
						else if (c >= 'a' && c <= 'f')
						{
							buildup = (buildup<<4) + c - 'a' + 10;
						}
						else if (c >= 'A' && c <= 'F')
						{
							buildup = (buildup<<4) + c - 'A' + 10;
						}
						else
						{
							sourcep--;
							yylval->val = buildup;
							return NUM;
						}
					}
				}
				else
				{
					sourcep--;
				}
			}
			while (isdigit (c = *sourcep++))
			{
				buildup = buildup*10 + c - '0';
			}
			sourcep--;
			yylval->val = buildup;
			return NUM;
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
				return SYMNUM;
			}
			if ((yylval->val = P_FindLineSpecial(token)) != 0)
			{
				return NUM;
			}
			strcpy (yylval->sym, token);
			return SYM;
		}
		if (c == '/')
		{
			c = *sourcep++;;
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
				return DIVIDE;
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
			return STRING;
		}
		if (c == '|')
		{
			c = *sourcep++;
			if (c == '=')
				return OR_EQUAL;
			sourcep--;
			return OR;
		}
		if (c == '<')
		{
			c = *sourcep++;
			if (c == '<')
			{
				c = *sourcep++;
				if (c == '=')
				{
					return LSHASSIGN;
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
					return RSHASSIGN;
				}
				sourcep--;
				return 0;
			}
			c--;
			return 0;
		}
		switch (c)
		{
		case '^': return XOR;
		case '&': return AND;
		case '-': return MINUS;
		case '+': return PLUS;
		case '*': return MULTIPLY;
		case '(': return LPAREN;
		case ')': return RPAREN;
		case ',': return COMMA;
		case '{': return LBRACE;
		case '}': return RBRACE;
		case '=': return EQUALS;
		case ';': return SEMICOLON;
		case ':': return COLON;
		case '[': return LBRACKET;
		case ']': return RBRACKET;
		default:  return 0;
		}
	}

	int PrintError (const char *s)
	{
		if (SourceFile != NULL)
			Printf ("%s, line %d: %s\n", SourceFile, SourceLine, s);
		else
			Printf ("%s\n", s);
		return 0;
	}


};

#include "xlat_parser.c"

//==========================================================================
//
//
//
//==========================================================================

void ParseXlatLump(const char *lumpname, void *pParser, XlatParseContext *context)
{
	int tokentype;
	int SavedSourceLine = context->SourceLine;
	const char *SavedSourceFile = context->SourceFile;
	XlatToken token;

	int lumpno = Wads.CheckNumForFullName(lumpname, true);

	if (lumpno == -1) 
	{
		Printf ("%s, line %d: Lump '%s' not found\n", context->SourceFile, context->SourceLine, lumpname);
		return;
	}

	// Read the lump into a buffer and add a 0-terminator
	int lumplen = Wads.LumpLength(lumpno);
	char *lumpdata = new char[lumplen+1];
	Wads.ReadLump(lumpno, lumpdata);
	lumpdata[lumplen] = 0;

	context->SourceLine = 0;
	context->SourceFile = lumpname;

	char *sourcep = lumpdata;
	while ( (tokentype = context->GetToken(sourcep, &token)) )
	{
		// It is much easier to handle include statements outside the main parser.
		if (tokentype == INCLUDE)
		{
			if (context->GetToken(sourcep, &token) != STRING)
			{
				Printf("%s, line %d: Include: String parameter expected\n", context->SourceFile, context->SourceLine);
				return;
			}
			ParseXlatLump(token.string, pParser, context);
		}
		else
		{
			XlatParse(pParser, tokentype, token, context);
		}
	}
	delete [] lumpdata;
	context->SourceLine = SavedSourceLine;
	context->SourceFile = SavedSourceFile;
}


//==========================================================================
//
//
//
//==========================================================================

void P_LoadTranslator(const char *lumpname)
{
	// Only read the lump if it differs from the previous one.
	if (LastTranslator.CompareNoCase(lumpname))
	{
		// Clear the old data before parsing the lump.
		SimpleLineTranslations.Clear();
		NumBoomish = 0;
		SectorTranslations.Clear();
		SectorMasks.Clear();

		void *pParser = XlatParseAlloc(malloc);

		XlatParseContext context;

		ParseXlatLump(lumpname, pParser, &context);
		XlatToken tok;
		tok.val=0;
		XlatParse(pParser, 0, tok, &context);
		XlatParseFree(pParser, free);
		LastTranslator = lumpname;
	}
}

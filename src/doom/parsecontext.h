/*
** parsecontext.h
** Base class for Lemon-based parsers
**
**---------------------------------------------------------------------------
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

#ifndef __PARSECONTEST__H_
#define __PARSECONTEST__H_

#include "tarray.h"

// The basic tokens the parser requires the grammar to understand
enum
{
	 OR       =1,
	 XOR      ,
	 AND      ,
	 MINUS    ,
	 PLUS     ,
	 MULTIPLY ,
	 DIVIDE   ,
	 MODULUS  ,
	 NUM      ,
	 FLOATVAL ,
	 LPAREN   ,
	 RPAREN   ,
	 SYM      ,
	 RBRACE   ,
	 LBRACE   ,
	 COMMA    ,
	 EQUALS   ,
	 LBRACKET ,
	 RBRACKET ,
	 OR_EQUAL ,
	 COLON    ,
	 SEMICOLON,
	 LSHASSIGN,
	 RSHASSIGN,
	 STRING   ,
	 INCLUDE  ,
	 DEFINE   ,
};

#define DEFINE_TOKEN_TRANS(prefix) \
	static int TokenTrans[] = { \
	0, \
	prefix##OR, \
	prefix##XOR, \
	prefix##AND, \
	prefix##MINUS, \
	prefix##PLUS, \
	prefix##MULTIPLY, \
	prefix##DIVIDE, \
	prefix##MODULUS, \
	prefix##NUM, \
	prefix##FLOATVAL, \
	prefix##LPAREN, \
	prefix##RPAREN, \
	prefix##SYM, \
	prefix##RBRACE, \
	prefix##LBRACE, \
	prefix##COMMA, \
	prefix##EQUALS, \
	prefix##LBRACKET, \
	prefix##RBRACKET, \
	prefix##OR_EQUAL, \
	prefix##COLON, \
	prefix##SEMICOLON, \
	prefix##LSHASSIGN, \
	prefix##RSHASSIGN, \
	prefix##STRING, \
	prefix##INCLUDE, \
	prefix##DEFINE, \
	 };


struct FParseSymbol
{
	int Value;
	char Sym[80];
};

union FParseToken
{
	int val;
	double fval;
	char sym[80];
	char string[80];
	FParseSymbol *symval;
};


struct FParseContext;

typedef void (*ParseFunc)(void *pParser, int tokentype, FParseToken token, FParseContext *context);

struct FParseContext
{
	TArray<FParseSymbol> symbols;
	int SourceLine;
	const char *SourceFile;
	int EnumVal;
	int *TokenTrans;
	void *pParser;
	ParseFunc Parse;

	FParseContext(void *parser, ParseFunc parse, int *tt)
	{
		SourceLine = 0;
		SourceFile = NULL;
		pParser = parser;
		Parse = parse;
		TokenTrans = tt;
	}

	virtual ~FParseContext() {}

	void AddSym (char *sym, int val);
	bool FindSym (char *sym, FParseSymbol **val);
	virtual bool FindToken (char *tok, int *type) = 0;
	int GetToken (char *&sourcep, FParseToken *yylval);
	int PrintError (const char *s);
	void ParseLump(const char *lumpname);
};


#endif

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
#include "w_wad.h"
#include "c_console.h"
#include "info.h"
#include "gi.h"
#include "parsecontext.h"
#include "xlat_parser.h"
#include "xlat.h"


// Token types not used in the grammar
enum
{
	XLAT_INCLUDE=128,
	XLAT_STRING,
	XLAT_FLOATVAL,	// floats are not used by the grammar
};

DEFINE_TOKEN_TRANS(XLAT_)


static FString LastTranslator;
TAutoGrowArray<FLineTrans> SimpleLineTranslations;
TArray<int> XlatExpressions;
FBoomTranslator Boomish[MAX_BOOMISH];
int NumBoomish;
TAutoGrowArray<FSectorTrans> SectorTranslations;
TArray<FSectorMask> SectorMasks;
FLineFlagTrans LineFlagTranslations[16];


struct SpecialArgs
{
	int addflags;
	int argcount;
	int args[5];
};

struct SpecialArg
{
	int arg;
	ELineTransArgOp argop;
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


struct XlatParseContext : public FParseContext
{
	XlatParseContext(void *parser, ParseFunc parse, int *tt)
		: FParseContext(parser, parse, tt)
	{
		DefiningLineType = -1;
	}

	//==========================================================================
	//
	//
	//
	//==========================================================================
	bool FindToken (char *tok, int *type)
	{
		static const char *tokens[] =
		{
			"arg2", "arg3", "arg4", "arg5", "bitmask", "clear",
			"define", "enum", "flags", "include", "lineflag", "lineid", 
			"maxlinespecial", "nobitmask", "sector", "tag"
		};
		static const short types[] =
		{
			XLAT_ARG2, XLAT_ARG3, XLAT_ARG4, XLAT_ARG5, XLAT_BITMASK, XLAT_CLEAR,
			XLAT_DEFINE, XLAT_ENUM, XLAT_FLAGS, XLAT_INCLUDE, XLAT_LINEFLAG, XLAT_TAG,
			XLAT_MAXLINESPECIAL, XLAT_NOBITMASK, XLAT_SECTOR, XLAT_TAG
		};

		int min = 0, max = countof(tokens) - 1;

		while (min <= max)
		{
			int mid = (min + max) / 2;
			int lexval = stricmp (tok, tokens[mid]);
			if (lexval == 0)
			{
				*type = types[mid];
				return true;
			}
			else if (lexval > 0)
			{
				min = mid + 1;
			}
			else
			{
				max = mid - 1;
			}
		}
		return false;
	}

	int DefiningLineType;
};

#include "xlat_parser.c"


//==========================================================================
//
//
//
//==========================================================================

void P_ClearTranslator()
{
	SimpleLineTranslations.Clear();
	XlatExpressions.Clear();
	NumBoomish = 0;
	SectorTranslations.Clear();
	SectorMasks.Clear();
	memset(LineFlagTranslations, 0, sizeof(LineFlagTranslations));
	LastTranslator = "";
}

void P_LoadTranslator(const char *lumpname)
{
	// Only read the lump if it differs from the previous one.
	if (LastTranslator.CompareNoCase(lumpname))
	{
		// Clear the old data before parsing the lump.
		P_ClearTranslator();

		void *pParser = XlatParseAlloc(malloc);

		XlatParseContext context(pParser, XlatParse, TokenTrans);

		context.ParseLump(lumpname);
		FParseToken tok;
		tok.val=0;
		XlatParse(pParser, 0, tok, &context);
		XlatParseFree(pParser, free);
		LastTranslator = lumpname;
	}
}



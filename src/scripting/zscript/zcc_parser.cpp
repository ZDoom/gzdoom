/*
** zcc_expr.cpp
**
**---------------------------------------------------------------------------
** Copyright -2016 Randy Heit
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

#include "dobject.h"
#include "sc_man.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "w_wad.h"
#include "cmdlib.h"
#include "m_alloc.h"
#include "i_system.h"
#include "m_argv.h"
#include "v_text.h"
#include "version.h"
#include "zcc_parser.h"
#include "zcc_compile.h"

TArray<FString> Includes;
TArray<FScriptPosition> IncludeLocs;

static FString ZCCTokenName(int terminal);
void AddInclude(ZCC_ExprConstant *node)
{
	assert(node->Type == TypeString);
	if (Includes.Find(*node->StringVal) >= Includes.Size())
	{
		Includes.Push(*node->StringVal);
		IncludeLocs.Push(*node);
	}
}

#include "zcc-parse.h"
#include "zcc-parse.c"

struct TokenMapEntry
{
	int16_t TokenType;
	uint16_t TokenName;
	TokenMapEntry(int16_t a, uint16_t b)
	: TokenType(a), TokenName(b)
	{ }
};
static TMap<int16_t, TokenMapEntry> TokenMap;
static int16_t BackTokenMap[YYERRORSYMBOL];	// YYERRORSYMBOL immediately follows the terminals described by the grammar

#define TOKENDEF2(sc, zcc, name)	{ TokenMapEntry tme(zcc, name); TokenMap.Insert(sc, tme); } BackTokenMap[zcc] = sc
#define TOKENDEF(sc, zcc)			TOKENDEF2(sc, zcc, NAME_None)

static void InitTokenMap()
{
	TOKENDEF ('=',				ZCC_EQ);
	TOKENDEF (TK_MulEq,			ZCC_MULEQ);
	TOKENDEF (TK_DivEq,			ZCC_DIVEQ);
	TOKENDEF (TK_ModEq,			ZCC_MODEQ);
	TOKENDEF (TK_AddEq,			ZCC_ADDEQ);
	TOKENDEF (TK_SubEq,			ZCC_SUBEQ);
	TOKENDEF (TK_LShiftEq,		ZCC_LSHEQ);
	TOKENDEF (TK_RShiftEq,		ZCC_RSHEQ);
	TOKENDEF (TK_URShiftEq,		ZCC_URSHEQ);
	TOKENDEF (TK_AndEq,			ZCC_ANDEQ);
	TOKENDEF (TK_OrEq,			ZCC_OREQ);
	TOKENDEF (TK_XorEq,			ZCC_XOREQ);
	TOKENDEF ('?',				ZCC_QUESTION);
	TOKENDEF (':',				ZCC_COLON);
	TOKENDEF ('@',				ZCC_ATSIGN);
	TOKENDEF (TK_OrOr,			ZCC_OROR);
	TOKENDEF (TK_AndAnd,		ZCC_ANDAND);
	TOKENDEF (TK_Eq,			ZCC_EQEQ);
	TOKENDEF (TK_Neq,			ZCC_NEQ);
	TOKENDEF (TK_ApproxEq,		ZCC_APPROXEQ);
	TOKENDEF ('<',				ZCC_LT);
	TOKENDEF ('>',				ZCC_GT);
	TOKENDEF (TK_Leq,			ZCC_LTEQ);
	TOKENDEF (TK_Geq,			ZCC_GTEQ);
	TOKENDEF (TK_LtGtEq,		ZCC_LTGTEQ);
	TOKENDEF (TK_Is,			ZCC_IS);
	TOKENDEF (TK_DotDot,		ZCC_DOTDOT);
	TOKENDEF (TK_Ellipsis,		ZCC_ELLIPSIS);
	TOKENDEF ('|',				ZCC_OR);
	TOKENDEF ('^',				ZCC_XOR);
	TOKENDEF ('&',				ZCC_AND);
	TOKENDEF (TK_LShift,		ZCC_LSH);
	TOKENDEF (TK_RShift,		ZCC_RSH);
	TOKENDEF (TK_URShift,		ZCC_URSH);
	TOKENDEF ('-',				ZCC_SUB);
	TOKENDEF ('+',				ZCC_ADD);
	TOKENDEF ('*',				ZCC_MUL);
	TOKENDEF ('/',				ZCC_DIV);
	TOKENDEF ('%',				ZCC_MOD);
	TOKENDEF (TK_Cross,			ZCC_CROSSPROD);
	TOKENDEF (TK_Dot,			ZCC_DOTPROD);
	TOKENDEF (TK_MulMul,		ZCC_POW);
	TOKENDEF (TK_Incr,			ZCC_ADDADD);
	TOKENDEF (TK_Decr,			ZCC_SUBSUB);
	TOKENDEF ('.',				ZCC_DOT);
	TOKENDEF ('(',				ZCC_LPAREN);
	TOKENDEF (')',				ZCC_RPAREN);
	TOKENDEF (TK_ColonColon,	ZCC_SCOPE);
	TOKENDEF (';',				ZCC_SEMICOLON);
	TOKENDEF (',',				ZCC_COMMA);
	TOKENDEF (TK_Class,			ZCC_CLASS);
	TOKENDEF (TK_Abstract,		ZCC_ABSTRACT);
	TOKENDEF (TK_Native,		ZCC_NATIVE);
	TOKENDEF (TK_Action,		ZCC_ACTION);
	TOKENDEF (TK_Replaces,		ZCC_REPLACES);
	TOKENDEF (TK_Static,		ZCC_STATIC);
	TOKENDEF (TK_Private,		ZCC_PRIVATE);
	TOKENDEF (TK_Protected,		ZCC_PROTECTED);
	TOKENDEF (TK_Latent,		ZCC_LATENT);
	TOKENDEF (TK_Virtual,		ZCC_VIRTUAL);
	TOKENDEF (TK_VarArg,        ZCC_VARARG);
	TOKENDEF (TK_UI,			ZCC_UI);
	TOKENDEF (TK_Play,			ZCC_PLAY);
	TOKENDEF (TK_ClearScope,	ZCC_CLEARSCOPE);
	TOKENDEF (TK_VirtualScope,  ZCC_VIRTUALSCOPE);
	TOKENDEF (TK_Override,		ZCC_OVERRIDE);
	TOKENDEF (TK_Final,			ZCC_FINAL);
	TOKENDEF (TK_Meta,			ZCC_META);
	TOKENDEF (TK_Deprecated,	ZCC_DEPRECATED);
	TOKENDEF (TK_Version,		ZCC_VERSION);
	TOKENDEF (TK_ReadOnly,		ZCC_READONLY);
	TOKENDEF ('{',				ZCC_LBRACE);
	TOKENDEF ('}',				ZCC_RBRACE);
	TOKENDEF (TK_Struct,		ZCC_STRUCT);
	TOKENDEF (TK_Property,		ZCC_PROPERTY);
	TOKENDEF (TK_Transient,		ZCC_TRANSIENT);
	TOKENDEF (TK_Enum,			ZCC_ENUM);
	TOKENDEF2(TK_SByte,			ZCC_SBYTE,		NAME_sByte);
	TOKENDEF2(TK_Byte,			ZCC_BYTE,		NAME_Byte);
	TOKENDEF2(TK_Short,			ZCC_SHORT,		NAME_Short);
	TOKENDEF2(TK_UShort,		ZCC_USHORT,		NAME_uShort);
	TOKENDEF2(TK_Int8,			ZCC_SBYTE,		NAME_int8);
	TOKENDEF2(TK_UInt8,			ZCC_BYTE,		NAME_uint8);
	TOKENDEF2(TK_Int16,			ZCC_SHORT,		NAME_int16);
	TOKENDEF2(TK_UInt16,		ZCC_USHORT,		NAME_uint16);
	TOKENDEF2(TK_Int,			ZCC_INT,		NAME_Int);
	TOKENDEF2(TK_UInt,			ZCC_UINT,		NAME_uInt);
	TOKENDEF2(TK_Bool,			ZCC_BOOL,		NAME_Bool);
	TOKENDEF2(TK_Float,			ZCC_FLOAT,		NAME_Float);
	TOKENDEF2(TK_Double,		ZCC_DOUBLE,		NAME_Double);
	TOKENDEF2(TK_String,		ZCC_STRING,		NAME_String);
	TOKENDEF2(TK_Vector2,		ZCC_VECTOR2,	NAME_Vector2);
	TOKENDEF2(TK_Vector3,		ZCC_VECTOR3,	NAME_Vector3);
	TOKENDEF2(TK_Name,			ZCC_NAME,		NAME_Name);
	TOKENDEF2(TK_Map,			ZCC_MAP,		NAME_Map);
	TOKENDEF2(TK_Array,			ZCC_ARRAY,		NAME_Array);
	TOKENDEF2(TK_Include,		ZCC_INCLUDE,	NAME_Include);
	TOKENDEF (TK_Void,			ZCC_VOID);
	TOKENDEF (TK_True,			ZCC_TRUE);
	TOKENDEF (TK_False,			ZCC_FALSE);
	TOKENDEF ('[',				ZCC_LBRACKET);
	TOKENDEF (']',				ZCC_RBRACKET);
	TOKENDEF (TK_In,			ZCC_IN);
	TOKENDEF (TK_Out,			ZCC_OUT);
	TOKENDEF (TK_Super,			ZCC_SUPER);
	TOKENDEF (TK_Null,			ZCC_NULLPTR);
	TOKENDEF ('~',				ZCC_TILDE);
	TOKENDEF ('!',				ZCC_BANG);
	TOKENDEF (TK_SizeOf,		ZCC_SIZEOF);
	TOKENDEF (TK_AlignOf,		ZCC_ALIGNOF);
	TOKENDEF (TK_Continue,		ZCC_CONTINUE);
	TOKENDEF (TK_Break,			ZCC_BREAK);
	TOKENDEF (TK_Return,		ZCC_RETURN);
	TOKENDEF (TK_Do,			ZCC_DO);
	TOKENDEF (TK_For,			ZCC_FOR);
	TOKENDEF (TK_While,			ZCC_WHILE);
	TOKENDEF (TK_Until,			ZCC_UNTIL);
	TOKENDEF (TK_If,			ZCC_IF);
	TOKENDEF (TK_Else,			ZCC_ELSE);
	TOKENDEF (TK_Switch,		ZCC_SWITCH);
	TOKENDEF (TK_Case,			ZCC_CASE);
	TOKENDEF2(TK_Default,		ZCC_DEFAULT,	NAME_Default);
	TOKENDEF (TK_Const,			ZCC_CONST);
	TOKENDEF (TK_Stop,			ZCC_STOP);
	TOKENDEF (TK_Wait,			ZCC_WAIT);
	TOKENDEF (TK_Fail,			ZCC_FAIL);
	TOKENDEF (TK_Loop,			ZCC_LOOP);
	TOKENDEF (TK_Goto,			ZCC_GOTO);
	TOKENDEF (TK_States,		ZCC_STATES);
	TOKENDEF2(TK_State,			ZCC_STATE,		NAME_State);
	TOKENDEF2(TK_Color,			ZCC_COLOR,		NAME_Color);
	TOKENDEF2(TK_Sound,			ZCC_SOUND,		NAME_Sound);
	TOKENDEF2(TK_Let,			ZCC_LET,		NAME_let);
	TOKENDEF2(TK_StaticConst,	ZCC_STATICCONST,NAME_Staticconst);

	TOKENDEF (TK_Identifier,	ZCC_IDENTIFIER);
	TOKENDEF (TK_StringConst,	ZCC_STRCONST);
	TOKENDEF (TK_NameConst,		ZCC_NAMECONST);
	TOKENDEF (TK_IntConst,		ZCC_INTCONST);
	TOKENDEF (TK_UIntConst,		ZCC_UINTCONST);
	TOKENDEF (TK_FloatConst,	ZCC_FLOATCONST);
	TOKENDEF (TK_NonWhitespace,	ZCC_NWS);

	TOKENDEF (TK_Bright,		ZCC_BRIGHT);
	TOKENDEF (TK_Slow,			ZCC_SLOW);
	TOKENDEF (TK_Fast,			ZCC_FAST);
	TOKENDEF (TK_NoDelay,		ZCC_NODELAY);
	TOKENDEF (TK_Offset,		ZCC_OFFSET);
	TOKENDEF (TK_CanRaise,		ZCC_CANRAISE);
	TOKENDEF (TK_Light,			ZCC_LIGHT);
	TOKENDEF (TK_Extend,		ZCC_EXTEND);
}
#undef TOKENDEF
#undef TOKENDEF2

//**--------------------------------------------------------------------------

static void ParseSingleFile(FScanner *pSC, const char *filename, int lump, void *parser, ZCCParseState &state)
{
	int tokentype;
	//bool failed;
	ZCCToken value;
	FScanner lsc;

	if (pSC == nullptr)
	{
		if (filename != nullptr)
		{
			lump = Wads.CheckNumForFullName(filename, true);
			if (lump >= 0)
			{
				lsc.OpenLumpNum(lump);
			}
			else
			{
				Printf("Could not find script lump '%s'\n", filename);
				return;
			}
		}
		else lsc.OpenLumpNum(lump);

		pSC = &lsc;
	}
	FScanner &sc = *pSC;
	sc.SetParseVersion(state.ParseVersion);
	state.sc = &sc;

	while (sc.GetToken())
	{
		value.SourceLoc = sc.GetMessageLine();
		switch (sc.TokenType)
		{
		case TK_StringConst:
			value.String = state.Strings.Alloc(sc.String, sc.StringLen);
			tokentype = ZCC_STRCONST;
			break;

		case TK_NameConst:
			value.Int = sc.Name;
			tokentype = ZCC_NAMECONST;
			break;

		case TK_IntConst:
			value.Int = sc.Number;
			tokentype = ZCC_INTCONST;
			break;

		case TK_UIntConst:
			value.Int = sc.Number;
			tokentype = ZCC_UINTCONST;
			break;

		case TK_FloatConst:
			value.Float = sc.Float;
			tokentype = ZCC_FLOATCONST;
			break;

		case TK_None:	// 'NONE' is a token for SBARINFO but not here.
		case TK_Identifier:
			value.Int = FName(sc.String);
			tokentype = ZCC_IDENTIFIER;
			break;

		case TK_NonWhitespace:
			value.Int = FName(sc.String);
			tokentype = ZCC_NWS;
			break;

		case TK_Static:
			sc.MustGetAnyToken();
			// The oh so wonderful grammar has problems with the 'const' token thanks to the overly broad rule for constants,
			// which effectively prevents use of this token nearly anywhere else. So in order to get 'static const' through
			// on the class/struct level we have to muck around with the token type here so that both words get combined into 
			// a single token that doesn't make the grammar throw a fit.
			if (sc.TokenType == TK_Const)
			{
				tokentype = ZCC_STATICCONST;
				value.Int = NAME_Staticconst;
			}
			else
			{
				tokentype = ZCC_STATIC;
				value.Int = NAME_Static;
				sc.UnGet();
			}
			break;

		default:
			TokenMapEntry *zcctoken = TokenMap.CheckKey(sc.TokenType);
			if (zcctoken != nullptr)
			{
				tokentype = zcctoken->TokenType;
				value.Int = zcctoken->TokenName;
			}
			else
			{
				sc.ScriptMessage("Unexpected token %s.\n", sc.TokenName(sc.TokenType).GetChars());
				goto parse_end;
			}
			break;
		}
		ZCCParse(parser, tokentype, value, &state);
	}
parse_end:
	value.Int = -1;
	ZCCParse(parser, ZCC_EOF, value, &state);
	state.sc = nullptr;
}

//**--------------------------------------------------------------------------

static void DoParse(int lumpnum)
{
	FScanner sc;
	void *parser;
	ZCCToken value;
	auto baselump = lumpnum;
	auto fileno = Wads.GetLumpFile(lumpnum);

	parser = ZCCParseAlloc(malloc);
	ZCCParseState state;

#ifndef NDEBUG
	FILE *f = nullptr;
	const char *tracefile = Args->CheckValue("-tracefile");
	if (tracefile != nullptr)
	{
		f = fopen(tracefile, "w");
		char prompt = '\0';
		ZCCParseTrace(f, &prompt);
	}
#endif

	sc.OpenLumpNum(lumpnum);
	sc.SetParseVersion({ 2, 4 });	// To get 'version' we need parse version 2.4 for the initial test
	auto saved = sc.SavePos();

	if (sc.GetToken())
	{
		if (sc.TokenType == TK_Version)
		{
			char *endp;
			sc.MustGetString();
			state.ParseVersion.major = (int16_t)clamp<unsigned long long>(strtoull(sc.String, &endp, 10), 0, USHRT_MAX);
			if (*endp != '.')
			{
				sc.ScriptError("Bad version directive");
			}
			state.ParseVersion.minor = (int16_t)clamp<unsigned long long>(strtoll(endp + 1, &endp, 10), 0, USHRT_MAX);
			if (*endp == '.')
			{
				state.ParseVersion.revision = (int16_t)clamp<unsigned long long>(strtoll(endp + 1, &endp, 10), 0, USHRT_MAX);
			}
			else state.ParseVersion.revision = 0;
			if (*endp != 0)
			{
				sc.ScriptError("Bad version directive");
			}
			if (state.ParseVersion.major == USHRT_MAX || state.ParseVersion.minor == USHRT_MAX || state.ParseVersion.revision == USHRT_MAX)
			{
				sc.ScriptError("Bad version directive");
			}
			if (state.ParseVersion > MakeVersion(VER_MAJOR, VER_MINOR, VER_REVISION))
			{
				sc.ScriptError("Version mismatch. %d.%d.%d expected but only %d.%d.%d supported", state.ParseVersion.major, state.ParseVersion.minor, state.ParseVersion.revision, VER_MAJOR, VER_MINOR, VER_REVISION);
			}
		}
		else
		{
			state.ParseVersion = MakeVersion(2, 3);	// 2.3 is the first version of ZScript.
			sc.RestorePos(saved);
		}
	}

	ParseSingleFile(&sc, nullptr, lumpnum, parser, state);
	for (unsigned i = 0; i < Includes.Size(); i++)
	{
		lumpnum = Wads.CheckNumForFullName(Includes[i], true);
		if (lumpnum == -1)
		{
			IncludeLocs[i].Message(MSG_ERROR, "Include script lump %s not found", Includes[i].GetChars());
		}
		else
		{
			auto fileno2 = Wads.GetLumpFile(lumpnum);
			if (fileno == 0 && fileno2 != 0)
			{
				I_FatalError("File %s is overriding core lump %s.",
					Wads.GetWadFullName(Wads.GetLumpFile(baselump)), Includes[i].GetChars());
			}

			ParseSingleFile(nullptr, nullptr, lumpnum, parser, state);
		}
	}
	Includes.Clear();
	Includes.ShrinkToFit();
	IncludeLocs.Clear();
	IncludeLocs.ShrinkToFit();

	value.Int = -1;
	value.SourceLoc = sc.GetMessageLine();
	ZCCParse(parser, 0, value, &state);
	ZCCParseFree(parser, free);

	// If the parser fails, there is no point starting the compiler, because it'd only flood the output with endless errors.
	if (FScriptPosition::ErrorCounter > 0)
	{
		I_Error("%d errors while parsing %s", FScriptPosition::ErrorCounter, Wads.GetLumpFullPath(lumpnum).GetChars());
	}

#ifndef NDEBUG
	if (f != nullptr)
	{
		fclose(f);
	}
#endif

	// Make a dump of the AST before running the compiler for diagnostic purposes.
	if (Args->CheckParm("-dumpast"))
	{
		FString ast = ZCC_PrintAST(state.TopNode);
		FString filename = Wads.GetLumpFullPath(lumpnum);
		filename.ReplaceChars(":\\/?|", '.');
		filename << ".ast";
		FILE *ff = fopen(filename, "w");
		if (ff != NULL)
		{
			fputs(ast.GetChars(), ff);
			fclose(ff);
		}
	}

	PSymbolTable symtable;
	auto newns = Wads.GetLumpFile(lumpnum) == 0 ? Namespaces.GlobalNamespace : Namespaces.NewNamespace(Wads.GetLumpFile(lumpnum));
	ZCCCompiler cc(state, NULL, symtable, newns, lumpnum, state.ParseVersion);
	cc.Compile();

	if (FScriptPosition::ErrorCounter > 0)
	{
		// Abort if the compiler produced any errors. Also do not compile further lumps, because they very likely miss some stuff.
		I_Error("%d errors, %d warnings while compiling %s", FScriptPosition::ErrorCounter, FScriptPosition::WarnCounter, Wads.GetLumpFullPath(lumpnum).GetChars());
	}
	else if (FScriptPosition::WarnCounter > 0)
	{
		// If we got warnings, but no errors, print the information but continue.
		Printf(TEXTCOLOR_ORANGE "%d warnings while compiling %s", FScriptPosition::WarnCounter, Wads.GetLumpFullPath(lumpnum).GetChars());
	}

}

void ParseScripts()
{
	if (TokenMap.CountUsed() == 0)
	{
		InitTokenMap();
	}
	int lump, lastlump = 0;
	FScriptPosition::ResetErrorCounter();

	while ((lump = Wads.FindLump("ZSCRIPT", &lastlump)) != -1)
	{
		DoParse(lump);
	}
}

static FString ZCCTokenName(int terminal)
{
	if (terminal == ZCC_EOF)
	{
		return "end of file";
	}
	int sc_token;
	if (terminal > 0 && terminal < (int)countof(BackTokenMap))
	{
		sc_token = BackTokenMap[terminal];
		if (sc_token == 0)
		{ // This token was not initialized. Whoops!
			sc_token = -terminal;
		}
	}
	else
	{ // This should never happen.
		sc_token = -terminal;
	}
	return FScanner::TokenName(sc_token);
}

ZCC_TreeNode *ZCC_AST::InitNode(size_t size, EZCCTreeNodeType type, ZCC_TreeNode *basis)
{
	ZCC_TreeNode *node = (ZCC_TreeNode *)SyntaxArena.Alloc(size);
	node->SiblingNext = node;
	node->SiblingPrev = node;
	node->NodeType = type;
	if (basis != NULL)
	{
		node->SourceName = basis->SourceName;
		node->SourceLump = basis->SourceLump;
		node->SourceLoc = basis->SourceLoc;
	}
	return node;
}

ZCC_TreeNode *ZCCParseState::InitNode(size_t size, EZCCTreeNodeType type)
{
	ZCC_TreeNode *node = ZCC_AST::InitNode(size, type, NULL);
	node->SourceName = Strings.Alloc(sc->ScriptName);
	node->SourceLump = sc->LumpNum;
	return node;
}

// Appends a sibling to this node's sibling list.
void AppendTreeNodeSibling(ZCC_TreeNode *thisnode, ZCC_TreeNode *sibling)
{
		if (thisnode == nullptr)
		{
			// Some bad syntax can actually get here, so better abort so that the user can see the error which caused this.
			I_FatalError("Internal script compiler error. Execution aborted.");
		}
		if (sibling == nullptr)
		{
			return;
		}

		ZCC_TreeNode *&SiblingPrev = thisnode->SiblingPrev;
		ZCC_TreeNode *&SiblingNext = thisnode->SiblingNext;

		// Check integrity of our sibling list.
		assert(SiblingPrev->SiblingNext == thisnode);
		assert(SiblingNext->SiblingPrev == thisnode);

		// Check integrity of new sibling list.
		assert(sibling->SiblingPrev->SiblingNext == sibling);
		assert(sibling->SiblingNext->SiblingPrev == sibling);

		ZCC_TreeNode *siblingend = sibling->SiblingPrev;
		SiblingPrev->SiblingNext = sibling;
		sibling->SiblingPrev = SiblingPrev;
		SiblingPrev = siblingend;
		siblingend->SiblingNext = thisnode;
}

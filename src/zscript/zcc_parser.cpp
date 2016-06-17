#include "dobject.h"
#include "sc_man.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "w_wad.h"
#include "cmdlib.h"
#include "m_alloc.h"
#include "zcc_parser.h"
#include "zcc_compile.h"

static FString ZCCTokenName(int terminal);

#include "zcc-parse.h"
#include "zcc-parse.c"

struct TokenMapEntry
{
	SWORD TokenType;
	WORD TokenName;
	TokenMapEntry(SWORD a, WORD b)
	: TokenType(a), TokenName(b)
	{ }
};
static TMap<SWORD, TokenMapEntry> TokenMap;
static SWORD BackTokenMap[YYERRORSYMBOL];	// YYERRORSYMBOL immediately follows the terminals described by the grammar

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
	TOKENDEF (TK_AndEq,			ZCC_ANDEQ);
	TOKENDEF (TK_OrEq,			ZCC_OREQ);
	TOKENDEF (TK_XorEq,			ZCC_XOREQ);
	TOKENDEF ('?',				ZCC_QUESTION);
	TOKENDEF (':',				ZCC_COLON);
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
	TOKENDEF ('|',				ZCC_OR);
	TOKENDEF ('^',				ZCC_XOR);
	TOKENDEF ('&',				ZCC_AND);
	TOKENDEF (TK_LShift,		ZCC_LSH);
	TOKENDEF (TK_RShift,		ZCC_RSH);
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
	TOKENDEF (TK_Replaces,		ZCC_REPLACES);
	TOKENDEF (TK_Static,		ZCC_STATIC);
	TOKENDEF (TK_Private,		ZCC_PRIVATE);
	TOKENDEF (TK_Protected,		ZCC_PROTECTED);
	TOKENDEF (TK_Latent,		ZCC_LATENT);
	TOKENDEF (TK_Final,			ZCC_FINAL);
	TOKENDEF (TK_Meta,			ZCC_META);
	TOKENDEF (TK_Deprecated,	ZCC_DEPRECATED);
	TOKENDEF (TK_ReadOnly,		ZCC_READONLY);
	TOKENDEF ('{',				ZCC_LBRACE);
	TOKENDEF ('}',				ZCC_RBRACE);
	TOKENDEF (TK_Struct,		ZCC_STRUCT);
	TOKENDEF (TK_Enum,			ZCC_ENUM);
	TOKENDEF2(TK_SByte,			ZCC_SBYTE,		NAME_sByte);
	TOKENDEF2(TK_Byte,			ZCC_BYTE,		NAME_Byte);
	TOKENDEF2(TK_Short,			ZCC_SHORT,		NAME_Short);
	TOKENDEF2(TK_UShort,		ZCC_USHORT,		NAME_uShort);
	TOKENDEF2(TK_Int,			ZCC_INT,		NAME_Int);
	TOKENDEF2(TK_UInt,			ZCC_UINT,		NAME_uInt);
	TOKENDEF2(TK_Bool,			ZCC_BOOL,		NAME_Bool);
	TOKENDEF2(TK_Float,			ZCC_FLOAT,		NAME_Float);
	TOKENDEF2(TK_Double,		ZCC_DOUBLE,		NAME_Double);
	TOKENDEF2(TK_String,		ZCC_STRING,		NAME_String);
	TOKENDEF2(TK_Vector,		ZCC_VECTOR,		NAME_Vector);
	TOKENDEF2(TK_Name,			ZCC_NAME,		NAME_Name);
	TOKENDEF2(TK_Map,			ZCC_MAP,		NAME_Map);
	TOKENDEF2(TK_Array,			ZCC_ARRAY,		NAME_Array);
	TOKENDEF (TK_Void,			ZCC_VOID);
	TOKENDEF (TK_True,			ZCC_TRUE);
	TOKENDEF (TK_False,			ZCC_FALSE);
	TOKENDEF ('[',				ZCC_LBRACKET);
	TOKENDEF (']',				ZCC_RBRACKET);
	TOKENDEF (TK_In,			ZCC_IN);
	TOKENDEF (TK_Out,			ZCC_OUT);
	TOKENDEF (TK_Optional,		ZCC_OPTIONAL);
	TOKENDEF (TK_Super,			ZCC_SUPER);
	TOKENDEF (TK_Self,			ZCC_SELF);
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

	ZCC_InitOperators();
	ZCC_InitConversions();
}
#undef TOKENDEF
#undef TOKENDEF2

static void DoParse(const char *filename)
{
	if (TokenMap.CountUsed() == 0)
	{
		InitTokenMap();
	}

	FScanner sc;
	void *parser;
	int tokentype;
	int lump;
	bool failed;
	ZCCToken value;

	lump = Wads.CheckNumForFullName(filename, true);
	if (lump >= 0)
	{
		sc.OpenLumpNum(lump);
	}
	else if (FileExists(filename))
	{
		sc.OpenFile(filename);
	}
	else
	{
		Printf("Could not find script lump '%s'\n", filename);
		return;
	}
	
	parser = ZCCParseAlloc(malloc);
	failed = false;
#ifdef _DEBUG
	FILE *f = fopen("trace.txt", "w");
	char prompt = '\0';
	ZCCParseTrace(f, &prompt);
#endif
	ZCCParseState state(sc);

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

		case TK_Identifier:
			value.Int = FName(sc.String);
			tokentype = ZCC_IDENTIFIER;
			break;

		case TK_NonWhitespace:
			value.Int = FName(sc.String);
			tokentype = ZCC_NWS;
			break;

		default:
			TokenMapEntry *zcctoken = TokenMap.CheckKey(sc.TokenType);
			if (zcctoken != NULL)
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
		if (failed)
		{
			sc.ScriptMessage("Parse failed\n");
			goto parse_end;
		}
	}
parse_end:
	value.Int = -1;
	ZCCParse(parser, ZCC_EOF, value, &state);
	ZCCParse(parser, 0, value, &state);
	ZCCParseFree(parser, free);

	PSymbolTable symbols(&GlobalSymbols);
	ZCCCompiler cc(state, NULL, symbols);
	cc.Compile();
#ifdef _DEBUG
	if (f != NULL)
	{
		fclose(f);
	}
	FString ast = ZCC_PrintAST(state.TopNode);
	FString astfile = ExtractFileBase(filename, false);
	astfile << ".ast";
	f = fopen(astfile, "w");
	if (f != NULL)
	{
		fputs(ast.GetChars(), f);
		fclose(f);
	}
#endif
}

CCMD(parse)
{
	if (argv.argc() == 2)
	{
		DoParse(argv[1]);
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
		node->SourceLoc = basis->SourceLoc;
	}
	return node;
}

ZCC_TreeNode *ZCCParseState::InitNode(size_t size, EZCCTreeNodeType type)
{
	ZCC_TreeNode *node = ZCC_AST::InitNode(size, type, NULL);
	node->SourceName = Strings.Alloc(sc.ScriptName);
	return node;
}

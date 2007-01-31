/* Driver template for the LEMON parser generator.
** The author disclaims copyright to this source code.
*/
/* First off, code is included which follows the "include" declaration
** in the input file. */
#include <stdio.h>
#include <string.h>
#line 1 "xlat-parse.y"

#include "xlat.h"
#include "xlat-parse.h"
#include <malloc.h>
#include <string.h>

int yyerror (char *s);

typedef struct _Symbol
{
	struct _Symbol *Next;
	int Value;
	char Sym[1];
} Symbol;

static bool FindToken (char *tok, int *type);
static void AddSym (char *sym, int val);
static bool FindSym (char *sym, Symbol **val);

static int EnumVal;

struct ListFilter
{
	WORD filter;
	BYTE value;
};

typedef struct _morefilters
{
	struct _morefilters *next;
	struct ListFilter filter;
} MoreFilters;

typedef struct _morelines
{
	struct _morelines *next;
	BoomArg arg;
} MoreLines;

typedef struct _specialargs
{
	BYTE addflags;
	BYTE args[5];
} SpecialArgs;

typedef struct _boomarg
{
	BYTE constant;
	WORD mask;
	MoreFilters *filters;
} ParseBoomArg;

typedef union YYSTYPE
{
	int val;
	char sym[80];
	char string[80];
	Symbol *symval;
} YYSTYPE;

#include <ctype.h>
#include <stdio.h>

int yylex (YYSTYPE *yylval)
{
	char token[80];
	int toksize;
	int c;

loop:
	while (Source == NULL)
	{
		if (!EndFile ())
			return 0;
	}
	while (isspace (c = fgetc (Source)) && c != EOF)
	{
		if (c == '\n')
			SourceLine++;
	}

	if (c == EOF)
	{
		if (EndFile ())
			goto loop;
		return 0;
	}
	if (isdigit (c))
	{
		int buildup = c - '0';
		if (c == '0')
		{
			c = fgetc (Source);
			if (c == 'x' || c == 'X')
			{
				for (;;)
				{
					c = fgetc (Source);
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
						ungetc (c, Source);
						yylval->val = buildup;
						return NUM;
					}
				}
			}
			else
			{
				ungetc (c, Source);
			}
		}
		while (isdigit (c = fgetc (Source)))
		{
			buildup = buildup*10 + c - '0';
		}
		ungetc (c, Source);
		yylval->val = buildup;
		return NUM;
	}
	if (isalpha (c))
	{
		int buildup = 0;
		
		token[0] = c;
		toksize = 1;
		while (toksize < 79 && (isalnum (c = fgetc (Source)) || c == '_'))
		{
			token[toksize++] = c;
		}
		token[toksize] = 0;
		if (toksize == 79 && isalnum (c))
		{
			while (isalnum (c = fgetc (Source)))
				;
		}
		ungetc (c, Source);
		if (FindToken (token, &buildup))
		{
			return buildup;
		}
		if (FindSym (token, &yylval->symval))
		{
			return SYMNUM;
		}
		strcpy (yylval->sym, token);
		return SYM;
	}
	if (c == '/')
	{
		c = fgetc (Source);
		if (c == '*')
		{
			for (;;)
			{
				while ((c = fgetc (Source)) != '*' && c != EOF)
				{
					if (c == '\n')
						SourceLine++;
				}
				if (c == EOF)
					return 0;
				if ((c = fgetc (Source)) == '/')
					goto loop;
				if (c == EOF)
					return 0;
				ungetc (c, Source);
			}
		}
		else if (c == '/')
		{
			while ((c = fgetc (Source)) != '\n' && c != EOF)
				;
			if (c == '\n')
				SourceLine++;
			else if (c == EOF)
				return 0;
			goto loop;
		}
		else
		{
			ungetc (c, Source);
			return DIVIDE;
		}
	}
	if (c == '"')
	{
		int tokensize = 0;
		while ((c = fgetc (Source)) != '"' && c != EOF)
		{
			yylval->string[tokensize++] = c;
		}
		yylval->string[tokensize] = 0;
		return STRING;
	}
	if (c == '|')
	{
		c = fgetc (Source);
		if (c == '=')
			return OR_EQUAL;
		ungetc (c, Source);
		return OR;
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

void *ParseAlloc(void *(*mallocProc)(size_t));
void Parse(void *yyp, int yymajor, YYSTYPE yyminor);
void ParseFree(void *p, void (*freeProc)(void*));
void ParseTrace(FILE *TraceFILE, char *zTracePrompt);

void yyparse (void)
{
	void *pParser = ParseAlloc (malloc);
	YYSTYPE token;
	int tokentype;

	while ((tokentype = yylex(&token)) != 0)
	{
		Parse (pParser, tokentype, token);
	}
	memset (&token, 0, sizeof(token));
	Parse (pParser, 0, token);
	ParseFree (pParser, free);
}

static Symbol *FirstSym;

static void AddSym (char *sym, int val)
{
	Symbol *syme = malloc (strlen (sym) + sizeof(Symbol));
	syme->Next = FirstSym;
	syme->Value = val;
	strcpy (syme->Sym, sym);
	FirstSym = syme;
}

static bool FindSym (char *sym, Symbol **val)
{
	Symbol *syme = FirstSym;

	while (syme != NULL)
	{
		if (strcmp (syme->Sym, sym) == 0)
		{
			*val = syme;
			return 1;
		}
		syme = syme->Next;
	}
	return 0;
}

static bool FindToken (char *tok, int *type)
{
	static const char tokens[][8] =
	{
		"endl", "print", "include", "special", "define", "enum",
		"arg5", "arg4", "arg3", "arg2", "flags", "lineid", "tag"
		
	};
	static const short types[] =
	{
		ENDL, PRINT, INCLUDE, SPECIAL, DEFINE, ENUM,
		ARG5, ARG4, ARG3, ARG2, FLAGS, LINEID, TAG
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

int yyerror (char *s)
{
	if (SourceName != NULL)
		printf ("%s, line %d: %s\n", SourceName, SourceLine, s);
	else
		printf ("%s\n", s);
	return 0;
}

#line 327 "xlat-parse.c"
/* Next is all token values, in a form suitable for use by makeheaders.
** This section will be null unless lemon is run with the -m switch.
*/
/* 
** These constants (all generated automatically by the parser generator)
** specify the various kinds of tokens (terminals) that the parser
** understands. 
**
** Each symbol here is a terminal symbol in the grammar.
*/
/* Make sure the INTERFACE macro is defined.
*/
#ifndef INTERFACE
# define INTERFACE 1
#endif
/* The next thing included is series of defines which control
** various aspects of the generated parser.
**    YYCODETYPE         is the data type used for storing terminal
**                       and nonterminal numbers.  "unsigned char" is
**                       used if there are fewer than 250 terminals
**                       and nonterminals.  "int" is used otherwise.
**    YYNOCODE           is a number of type YYCODETYPE which corresponds
**                       to no legal terminal or nonterminal number.  This
**                       number is used to fill in empty slots of the hash 
**                       table.
**    YYFALLBACK         If defined, this indicates that one or more tokens
**                       have fall-back values which should be used if the
**                       original value of the token will not parse.
**    YYACTIONTYPE       is the data type used for storing terminal
**                       and nonterminal numbers.  "unsigned char" is
**                       used if there are fewer than 250 rules and
**                       states combined.  "int" is used otherwise.
**    ParseTOKENTYPE     is the data type used for minor tokens given 
**                       directly to the parser from the tokenizer.
**    YYMINORTYPE        is the data type used for all minor tokens.
**                       This is typically a union of many types, one of
**                       which is ParseTOKENTYPE.  The entry in the union
**                       for base tokens is called "yy0".
**    YYSTACKDEPTH       is the maximum depth of the parser's stack.
**    ParseARG_SDECL     A static variable declaration for the %extra_argument
**    ParseARG_PDECL     A parameter declaration for the %extra_argument
**    ParseARG_STORE     Code to store %extra_argument into yypParser
**    ParseARG_FETCH     Code to extract %extra_argument from yypParser
**    YYNSTATE           the combined number of states.
**    YYNRULE            the number of rules in the grammar
**    YYERRORSYMBOL      is the code number of the error symbol.  If not
**                       defined, then do no error processing.
*/
#define YYCODETYPE unsigned char
#define YYNOCODE 66
#define YYACTIONTYPE unsigned short int
#define ParseTOKENTYPE YYSTYPE
typedef union {
  ParseTOKENTYPE yy0;
  MoreLines * yy49;
  SpecialArgs yy57;
  BoomArg yy58;
  int yy72;
  ParseBoomArg yy83;
  MoreFilters * yy87;
  struct ListFilter yy124;
  int yy131;
} YYMINORTYPE;
#define YYSTACKDEPTH 100
#define ParseARG_SDECL
#define ParseARG_PDECL
#define ParseARG_FETCH
#define ParseARG_STORE
#define YYNSTATE 173
#define YYNRULE 93
#define YYERRORSYMBOL 37
#define YYERRSYMDT yy131
#define YY_NO_ACTION      (YYNSTATE+YYNRULE+2)
#define YY_ACCEPT_ACTION  (YYNSTATE+YYNRULE+1)
#define YY_ERROR_ACTION   (YYNSTATE+YYNRULE)

/* Next are that tables used to determine what action to take based on the
** current state and lookahead token.  These tables are used to implement
** functions that take a state number and lookahead value and return an
** action integer.  
**
** Suppose the action integer is N.  Then the action is determined as
** follows
**
**   0 <= N < YYNSTATE                  Shift N.  That is, push the lookahead
**                                      token onto the stack and goto state N.
**
**   YYNSTATE <= N < YYNSTATE+YYNRULE   Reduce by rule N-YYNSTATE.
**
**   N == YYNSTATE+YYNRULE              A syntax error has occurred.
**
**   N == YYNSTATE+YYNRULE+1            The parser accepts its input.
**
**   N == YYNSTATE+YYNRULE+2            No such action.  Denotes unused
**                                      slots in the yy_action[] table.
**
** The action table is constructed as a single large table named yy_action[].
** Given state S and lookahead X, the action is computed as
**
**      yy_action[ yy_shift_ofst[S] + X ]
**
** If the index value yy_shift_ofst[S]+X is out of range or if the value
** yy_lookahead[yy_shift_ofst[S]+X] is not equal to X or if yy_shift_ofst[S]
** is equal to YY_SHIFT_USE_DFLT, it means that the action is not in the table
** and that yy_default[S] should be used instead.  
**
** The formula above is for computing the action when the lookahead is
** a terminal symbol.  If the lookahead is a non-terminal (as occurs after
** a reduce action) then the yy_reduce_ofst[] array is used in place of
** the yy_shift_ofst[] array and YY_REDUCE_USE_DFLT is used in place of
** YY_SHIFT_USE_DFLT.
**
** The following are the tables generated in this section:
**
**  yy_action[]        A single table containing all actions.
**  yy_lookahead[]     A table containing the lookahead for each entry in
**                     yy_action.  Used to detect hash collisions.
**  yy_shift_ofst[]    For each state, the offset into yy_action for
**                     shifting terminals.
**  yy_reduce_ofst[]   For each state, the offset into yy_action for
**                     shifting non-terminals after a reduce.
**  yy_default[]       Default action for each state.
*/
static const YYACTIONTYPE yy_action[] = {
 /*     0 */    65,   41,   32,   35,   52,   42,   28,   51,   77,  117,
 /*    10 */   267,  163,  160,  149,  148,  147,  146,  145,  144,  143,
 /*    20 */    30,   80,   14,  128,  164,  156,  151,   23,  123,  127,
 /*    30 */    55,    4,  152,  102,   33,  108,   80,  119,  128,  107,
 /*    40 */     5,  111,  113,   22,    2,   41,   32,   35,   52,   42,
 /*    50 */    28,   51,   32,   35,   52,   42,   28,   51,   19,    3,
 /*    60 */    41,   32,   35,   52,   42,   28,   51,   52,   42,   28,
 /*    70 */    51,  135,  116,   41,   32,   35,   52,   42,   28,   51,
 /*    80 */    41,   32,   35,   52,   42,   28,   51,   28,   51,   11,
 /*    90 */     9,  168,  169,  170,  171,  172,   77,  106,   26,   66,
 /*   100 */    41,   32,   35,   52,   42,   28,   51,   35,   52,   42,
 /*   110 */    28,   51,   10,   27,   61,   41,   32,   35,   52,   42,
 /*   120 */    28,   51,   68,  120,  100,  139,  165,   49,   18,  166,
 /*   130 */    41,   32,   35,   52,   42,   28,   51,   55,    4,  101,
 /*   140 */   167,   12,   40,   29,   43,   41,   32,   35,   52,   42,
 /*   150 */    28,   51,  136,  131,  113,   91,   66,   98,   16,  115,
 /*   160 */    41,   32,   35,   52,   42,   28,   51,  124,  130,  159,
 /*   170 */    56,   59,  129,   37,   61,   41,   32,   35,   52,   42,
 /*   180 */    28,   51,  138,  109,   48,   57,   60,  154,   20,   54,
 /*   190 */    41,   32,   35,   52,   42,   28,   51,   13,   62,   90,
 /*   200 */    89,   17,   15,   21,    7,   41,   32,   35,   52,   42,
 /*   210 */    28,   51,   39,  132,   63,   99,   94,   87,   31,  142,
 /*   220 */    41,   32,   35,   52,   42,   28,   51,  134,  133,   67,
 /*   230 */    88,  157,   74,   41,   32,   35,   52,   42,   28,   51,
 /*   240 */    41,   32,   35,   52,   42,   28,   51,   84,  161,   30,
 /*   250 */   141,  104,   86,   38,  156,  151,   23,    1,  153,    6,
 /*   260 */    96,   41,   32,   35,   52,   42,   28,   51,   41,   32,
 /*   270 */    35,   52,   42,   28,   51,    8,   41,   32,   35,   52,
 /*   280 */    42,   28,   51,   25,  103,   64,   97,  125,   71,   95,
 /*   290 */    24,   76,   41,   32,   35,   52,   42,   28,   51,  150,
 /*   300 */   158,   78,  268,  162,  268,   45,   83,   41,   32,   35,
 /*   310 */    52,   42,   28,   51,   92,   81,   82,  268,   70,   69,
 /*   320 */    44,   85,   41,   32,   35,   52,   42,   28,   51,  268,
 /*   330 */    72,   79,   75,   58,   73,   36,   93,   41,   32,   35,
 /*   340 */    52,   42,   28,   51,  268,  268,  268,  268,  268,  268,
 /*   350 */    46,  268,   41,   32,   35,   52,   42,   28,   51,  268,
 /*   360 */   268,  268,  268,  268,  268,   34,  268,   41,   32,   35,
 /*   370 */    52,   42,   28,   51,  268,  268,  268,  268,  268,  268,
 /*   380 */    47,  268,   41,   32,   35,   52,   42,   28,   51,  268,
 /*   390 */   268,  268,  268,  268,  268,   50,  268,   41,   32,   35,
 /*   400 */    52,   42,   28,   51,  268,  268,  268,  268,  268,  268,
 /*   410 */    53,   88,   41,   32,   35,   52,   42,   28,   51,   30,
 /*   420 */   268,  268,   30,  268,  156,  151,   23,  156,  151,   23,
 /*   430 */    30,  110,  104,  140,  137,  156,  151,   23,  268,  268,
 /*   440 */    30,  268,  268,  268,  105,  156,  151,   23,  268,   30,
 /*   450 */   114,  126,   30,  268,  156,  151,   23,  156,  151,   23,
 /*   460 */    30,  268,  268,   30,  268,  156,  151,   23,  156,  151,
 /*   470 */    23,  155,  268,  268,  268,  268,  268,  268,  268,  268,
 /*   480 */   122,  268,  268,  118,  268,  268,  268,  268,  268,  268,
 /*   490 */   268,  121,  268,  268,  112,
};
static const YYCODETYPE yy_lookahead[] = {
 /*     0 */    38,    1,    2,    3,    4,    5,    6,    7,   38,   39,
 /*    10 */    48,   49,   50,   51,   52,   53,   54,   55,   56,   57,
 /*    20 */     4,   38,   60,   40,   41,    9,   10,   11,   28,   13,
 /*    30 */    44,   45,   46,   17,   14,   19,   38,   21,   40,   41,
 /*    40 */    24,   61,   62,   27,   11,    1,    2,    3,    4,    5,
 /*    50 */     6,    7,    2,    3,    4,    5,    6,    7,   14,   22,
 /*    60 */     1,    2,    3,    4,    5,    6,    7,    4,    5,    6,
 /*    70 */     7,   12,   18,    1,    2,    3,    4,    5,    6,    7,
 /*    80 */     1,    2,    3,    4,    5,    6,    7,    6,    7,   11,
 /*    90 */    11,   29,   30,   31,   32,   33,   38,   39,   26,   38,
 /*   100 */     1,    2,    3,    4,    5,    6,    7,    3,    4,    5,
 /*   110 */     6,    7,   43,   14,   38,    1,    2,    3,    4,    5,
 /*   120 */     6,    7,   38,   47,   63,   64,   42,   11,   14,   23,
 /*   130 */     1,    2,    3,    4,    5,    6,    7,   44,   45,   46,
 /*   140 */    34,   14,   14,   14,   14,    1,    2,    3,    4,    5,
 /*   150 */     6,    7,   25,   61,   62,   38,   38,   38,   14,   10,
 /*   160 */     1,    2,    3,    4,    5,    6,    7,   18,   12,   38,
 /*   170 */    38,   38,   22,   14,   38,    1,    2,    3,    4,    5,
 /*   180 */     6,    7,   64,   47,   14,   38,   38,   12,   14,   23,
 /*   190 */     1,    2,    3,    4,    5,    6,    7,   11,   38,   38,
 /*   200 */    38,   14,   14,   14,   14,    1,    2,    3,    4,    5,
 /*   210 */     6,    7,   14,   20,   38,   38,   38,   38,   14,   12,
 /*   220 */     1,    2,    3,    4,    5,    6,    7,   12,   15,   38,
 /*   230 */    38,   12,   38,    1,    2,    3,    4,    5,    6,    7,
 /*   240 */     1,    2,    3,    4,    5,    6,    7,   38,   28,    4,
 /*   250 */    58,   59,   38,   14,    9,   10,   11,   14,   12,   27,
 /*   260 */    38,    1,    2,    3,    4,    5,    6,    7,    1,    2,
 /*   270 */     3,    4,    5,    6,    7,   11,    1,    2,    3,    4,
 /*   280 */     5,    6,    7,   11,   18,   38,   26,   12,   38,   38,
 /*   290 */    23,   38,    1,    2,    3,    4,    5,    6,    7,   20,
 /*   300 */    38,   38,   65,   38,   65,   14,   38,    1,    2,    3,
 /*   310 */     4,    5,    6,    7,   38,   38,   38,   65,   38,   38,
 /*   320 */    14,   38,    1,    2,    3,    4,    5,    6,    7,   65,
 /*   330 */    38,   38,   38,   38,   38,   14,   38,    1,    2,    3,
 /*   340 */     4,    5,    6,    7,   65,   65,   65,   65,   65,   65,
 /*   350 */    14,   65,    1,    2,    3,    4,    5,    6,    7,   65,
 /*   360 */    65,   65,   65,   65,   65,   14,   65,    1,    2,    3,
 /*   370 */     4,    5,    6,    7,   65,   65,   65,   65,   65,   65,
 /*   380 */    14,   65,    1,    2,    3,    4,    5,    6,    7,   65,
 /*   390 */    65,   65,   65,   65,   65,   14,   65,    1,    2,    3,
 /*   400 */     4,    5,    6,    7,   65,   65,   65,   65,   65,   65,
 /*   410 */    14,   38,    1,    2,    3,    4,    5,    6,    7,    4,
 /*   420 */    65,   65,    4,   65,    9,   10,   11,    9,   10,   11,
 /*   430 */     4,   58,   59,   15,   16,    9,   10,   11,   65,   65,
 /*   440 */     4,   65,   65,   65,   18,    9,   10,   11,   65,    4,
 /*   450 */    35,   36,    4,   65,    9,   10,   11,    9,   10,   11,
 /*   460 */     4,   65,   65,    4,   65,    9,   10,   11,    9,   10,
 /*   470 */    11,   35,   65,   65,   65,   65,   65,   65,   65,   65,
 /*   480 */    35,   65,   65,   35,   65,   65,   65,   65,   65,   65,
 /*   490 */    65,   35,   65,   65,   35,
};
#define YY_SHIFT_USE_DFLT (-1)
#define YY_SHIFT_MAX 128
static const short yy_shift_ofst[] = {
 /*     0 */    16,  418,  418,   62,   62,  245,  245,  245,  415,  415,
 /*    10 */   245,  245,  245,  245,   54,   54,  456,  459,  448,  426,
 /*    20 */   445,  436,  245,  245,  245,  245,  245,  245,  245,  245,
 /*    30 */   245,  245,  245,  245,  245,  245,  245,  245,  245,  245,
 /*    40 */   245,  245,  245,  245,  245,  245,  245,  245,  245,  245,
 /*    50 */   245,  245,  245,  245,  245,  106,  396,  381,  366,  351,
 /*    60 */   336,  321,  306,  291,  275,  267,  260,  239,  232,    0,
 /*    70 */   219,  204,  189,  174,  159,  144,  129,  114,   99,   79,
 /*    80 */    72,   59,   44,  411,  411,  411,  411,  411,  411,  411,
 /*    90 */   411,  411,  411,  411,   50,  104,   63,  149,   81,   81,
 /*   100 */   127,  279,  266,  272,  243,  264,  246,  220,  213,  215,
 /*   110 */   207,  193,  198,  188,  187,  186,  166,  175,  170,  150,
 /*   120 */   156,  130,  128,  116,   78,   37,   20,   33,  190,
};
#define YY_REDUCE_USE_DFLT (-39)
#define YY_REDUCE_MAX 55
static const short yy_reduce_ofst[] = {
 /*     0 */   -38,  192,  373,   93,  -14,   61,   -2,  -17,   58,  -30,
 /*    10 */    84,   76,  118,  136,  -20,   92,  296,  295,  294,  293,
 /*    20 */   292,  283,  281,  280,  278,  277,  276,  268,  265,  263,
 /*    30 */   262,  253,  251,  250,  247,  222,  214,  209,  194,  191,
 /*    40 */   179,  178,  177,  176,  162,  161,  160,  148,  147,  133,
 /*    50 */   132,  131,  119,  298,  117,   69,
};
static const YYACTIONTYPE yy_default[] = {
 /*     0 */   185,  195,  195,  221,  221,  266,  266,  266,  236,  236,
 /*    10 */   266,  215,  266,  215,  205,  205,  266,  266,  266,  266,
 /*    20 */   266,  266,  266,  266,  266,  266,  266,  266,  266,  266,
 /*    30 */   266,  266,  266,  266,  266,  266,  266,  266,  266,  266,
 /*    40 */   266,  266,  266,  266,  266,  266,  266,  266,  266,  266,
 /*    50 */   266,  266,  266,  266,  266,  266,  258,  257,  238,  266,
 /*    60 */   239,  216,  240,  261,  266,  266,  266,  243,  231,  266,
 /*    70 */   266,  247,  254,  253,  244,  252,  248,  251,  249,  266,
 /*    80 */   266,  266,  266,  250,  245,  255,  217,  264,  199,  241,
 /*    90 */   262,  209,  235,  259,  180,  182,  181,  266,  177,  176,
 /*   100 */   266,  266,  266,  266,  196,  266,  266,  266,  266,  266,
 /*   110 */   266,  266,  242,  206,  237,  266,  208,  266,  256,  266,
 /*   120 */   266,  260,  263,  266,  266,  266,  246,  266,  233,  204,
 /*   130 */   213,  207,  203,  202,  214,  201,  210,  200,  212,  211,
 /*   140 */   198,  197,  194,  193,  192,  191,  190,  189,  188,  187,
 /*   150 */   220,  175,  222,  219,  218,  265,  174,  184,  183,  179,
 /*   160 */   186,  232,  178,  173,  234,  223,  229,  230,  224,  225,
 /*   170 */   226,  227,  228,
};
#define YY_SZ_ACTTAB (int)(sizeof(yy_action)/sizeof(yy_action[0]))

/* The next table maps tokens into fallback tokens.  If a construct
** like the following:
** 
**      %fallback ID X Y Z.
**
** appears in the grammer, then ID becomes a fallback token for X, Y,
** and Z.  Whenever one of the tokens X, Y, or Z is input to the parser
** but it does not parse, the type of the token is changed to ID and
** the parse is retried before an error is thrown.
*/
#ifdef YYFALLBACK
static const YYCODETYPE yyFallback[] = {
};
#endif /* YYFALLBACK */

/* The following structure represents a single element of the
** parser's stack.  Information stored includes:
**
**   +  The state number for the parser at this level of the stack.
**
**   +  The value of the token stored at this level of the stack.
**      (In other words, the "major" token.)
**
**   +  The semantic value stored at this level of the stack.  This is
**      the information used by the action routines in the grammar.
**      It is sometimes called the "minor" token.
*/
struct yyStackEntry {
  int stateno;       /* The state-number */
  int major;         /* The major token value.  This is the code
                     ** number for the token at this stack level */
  YYMINORTYPE minor; /* The user-supplied minor token value.  This
                     ** is the value of the token  */
};
typedef struct yyStackEntry yyStackEntry;

/* The state of the parser is completely contained in an instance of
** the following structure */
struct yyParser {
  int yyidx;                    /* Index of top element in stack */
  int yyerrcnt;                 /* Shifts left before out of the error */
  ParseARG_SDECL                /* A place to hold %extra_argument */
  yyStackEntry yystack[YYSTACKDEPTH];  /* The parser's stack */
};
typedef struct yyParser yyParser;

#ifndef NDEBUG
#include <stdio.h>
static FILE *yyTraceFILE = 0;
static char *yyTracePrompt = 0;
#endif /* NDEBUG */

#ifndef NDEBUG
/* 
** Turn parser tracing on by giving a stream to which to write the trace
** and a prompt to preface each trace message.  Tracing is turned off
** by making either argument NULL 
**
** Inputs:
** <ul>
** <li> A FILE* to which trace output should be written.
**      If NULL, then tracing is turned off.
** <li> A prefix string written at the beginning of every
**      line of trace output.  If NULL, then tracing is
**      turned off.
** </ul>
**
** Outputs:
** None.
*/
void ParseTrace(FILE *TraceFILE, char *zTracePrompt){
  yyTraceFILE = TraceFILE;
  yyTracePrompt = zTracePrompt;
  if( yyTraceFILE==0 ) yyTracePrompt = 0;
  else if( yyTracePrompt==0 ) yyTraceFILE = 0;
}
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing shifts, the names of all terminals and nonterminals
** are required.  The following table supplies these names */
static const char *const yyTokenName[] = { 
  "$",             "OR",            "XOR",           "AND",         
  "MINUS",         "PLUS",          "MULTIPLY",      "DIVIDE",      
  "NEG",           "NUM",           "SYMNUM",        "LPAREN",      
  "RPAREN",        "PRINT",         "COMMA",         "STRING",      
  "ENDL",          "DEFINE",        "SYM",           "INCLUDE",     
  "RBRACE",        "ENUM",          "LBRACE",        "EQUALS",      
  "SPECIAL",       "SEMICOLON",     "COLON",         "LBRACKET",    
  "RBRACKET",      "FLAGS",         "ARG2",          "ARG3",        
  "ARG4",          "ARG5",          "OR_EQUAL",      "TAG",         
  "LINEID",        "error",         "exp",           "special_args",
  "list_val",      "arg_list",      "boom_args",     "boom_op",     
  "boom_selector",  "boom_line",     "boom_body",     "maybe_argcount",
  "main",          "translation_unit",  "external_declaration",  "define_statement",
  "include_statement",  "print_statement",  "enum_statement",  "linetype_declaration",
  "boom_declaration",  "special_declaration",  "print_list",    "print_item",  
  "enum_open",     "enum_list",     "single_enum",   "special_list",
  "special_def", 
};
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing reduce actions, the names of all rules are required.
*/
static const char *const yyRuleName[] = {
 /*   0 */ "main ::= translation_unit",
 /*   1 */ "exp ::= NUM",
 /*   2 */ "exp ::= SYMNUM",
 /*   3 */ "exp ::= exp PLUS exp",
 /*   4 */ "exp ::= exp MINUS exp",
 /*   5 */ "exp ::= exp MULTIPLY exp",
 /*   6 */ "exp ::= exp DIVIDE exp",
 /*   7 */ "exp ::= exp OR exp",
 /*   8 */ "exp ::= exp AND exp",
 /*   9 */ "exp ::= exp XOR exp",
 /*  10 */ "exp ::= MINUS exp",
 /*  11 */ "exp ::= LPAREN exp RPAREN",
 /*  12 */ "translation_unit ::=",
 /*  13 */ "translation_unit ::= external_declaration",
 /*  14 */ "external_declaration ::= define_statement",
 /*  15 */ "external_declaration ::= include_statement",
 /*  16 */ "external_declaration ::= print_statement",
 /*  17 */ "external_declaration ::= enum_statement",
 /*  18 */ "external_declaration ::= linetype_declaration",
 /*  19 */ "external_declaration ::= boom_declaration",
 /*  20 */ "external_declaration ::= special_declaration",
 /*  21 */ "print_statement ::= PRINT LPAREN print_list RPAREN",
 /*  22 */ "print_list ::=",
 /*  23 */ "print_list ::= print_item",
 /*  24 */ "print_list ::= print_item COMMA print_list",
 /*  25 */ "print_item ::= STRING",
 /*  26 */ "print_item ::= exp",
 /*  27 */ "print_item ::= ENDL",
 /*  28 */ "define_statement ::= DEFINE SYM LPAREN exp RPAREN",
 /*  29 */ "include_statement ::= INCLUDE STRING",
 /*  30 */ "enum_statement ::= enum_open enum_list RBRACE",
 /*  31 */ "enum_open ::= ENUM LBRACE",
 /*  32 */ "enum_list ::=",
 /*  33 */ "enum_list ::= single_enum",
 /*  34 */ "enum_list ::= single_enum COMMA enum_list",
 /*  35 */ "single_enum ::= SYM",
 /*  36 */ "single_enum ::= SYM EQUALS exp",
 /*  37 */ "special_declaration ::= SPECIAL special_list SEMICOLON",
 /*  38 */ "special_list ::= special_def",
 /*  39 */ "special_list ::= special_list COMMA special_def",
 /*  40 */ "special_def ::= exp COLON SYM LPAREN maybe_argcount RPAREN",
 /*  41 */ "special_def ::= exp COLON SYMNUM LPAREN maybe_argcount RPAREN",
 /*  42 */ "maybe_argcount ::=",
 /*  43 */ "maybe_argcount ::= exp",
 /*  44 */ "maybe_argcount ::= exp COMMA exp",
 /*  45 */ "linetype_declaration ::= exp EQUALS exp COMMA exp LPAREN special_args RPAREN",
 /*  46 */ "linetype_declaration ::= exp EQUALS exp COMMA SYM LPAREN special_args RPAREN",
 /*  47 */ "boom_declaration ::= LBRACKET exp RBRACKET LPAREN exp COMMA exp RPAREN LBRACE boom_body RBRACE",
 /*  48 */ "boom_body ::=",
 /*  49 */ "boom_body ::= boom_line boom_body",
 /*  50 */ "boom_line ::= boom_selector boom_op boom_args",
 /*  51 */ "boom_selector ::= FLAGS",
 /*  52 */ "boom_selector ::= ARG2",
 /*  53 */ "boom_selector ::= ARG3",
 /*  54 */ "boom_selector ::= ARG4",
 /*  55 */ "boom_selector ::= ARG5",
 /*  56 */ "boom_op ::= EQUALS",
 /*  57 */ "boom_op ::= OR_EQUAL",
 /*  58 */ "boom_args ::= exp",
 /*  59 */ "boom_args ::= exp LBRACKET arg_list RBRACKET",
 /*  60 */ "arg_list ::= list_val",
 /*  61 */ "arg_list ::= list_val COMMA arg_list",
 /*  62 */ "list_val ::= exp COLON exp",
 /*  63 */ "special_args ::=",
 /*  64 */ "special_args ::= TAG",
 /*  65 */ "special_args ::= TAG COMMA exp",
 /*  66 */ "special_args ::= TAG COMMA exp COMMA exp",
 /*  67 */ "special_args ::= TAG COMMA exp COMMA exp COMMA exp",
 /*  68 */ "special_args ::= TAG COMMA exp COMMA exp COMMA exp COMMA exp",
 /*  69 */ "special_args ::= TAG COMMA TAG",
 /*  70 */ "special_args ::= TAG COMMA TAG COMMA exp",
 /*  71 */ "special_args ::= TAG COMMA TAG COMMA exp COMMA exp",
 /*  72 */ "special_args ::= TAG COMMA TAG COMMA exp COMMA exp COMMA exp",
 /*  73 */ "special_args ::= LINEID",
 /*  74 */ "special_args ::= LINEID COMMA exp",
 /*  75 */ "special_args ::= LINEID COMMA exp COMMA exp",
 /*  76 */ "special_args ::= LINEID COMMA exp COMMA exp COMMA exp",
 /*  77 */ "special_args ::= LINEID COMMA exp COMMA exp COMMA exp COMMA exp",
 /*  78 */ "special_args ::= exp",
 /*  79 */ "special_args ::= exp COMMA exp",
 /*  80 */ "special_args ::= exp COMMA exp COMMA exp",
 /*  81 */ "special_args ::= exp COMMA exp COMMA exp COMMA exp",
 /*  82 */ "special_args ::= exp COMMA exp COMMA exp COMMA exp COMMA exp",
 /*  83 */ "special_args ::= exp COMMA TAG",
 /*  84 */ "special_args ::= exp COMMA TAG COMMA exp",
 /*  85 */ "special_args ::= exp COMMA TAG COMMA exp COMMA exp",
 /*  86 */ "special_args ::= exp COMMA TAG COMMA exp COMMA exp COMMA exp",
 /*  87 */ "special_args ::= exp COMMA exp COMMA TAG",
 /*  88 */ "special_args ::= exp COMMA exp COMMA TAG COMMA exp",
 /*  89 */ "special_args ::= exp COMMA exp COMMA TAG COMMA exp COMMA exp",
 /*  90 */ "special_args ::= exp COMMA exp COMMA exp COMMA TAG",
 /*  91 */ "special_args ::= exp COMMA exp COMMA exp COMMA TAG COMMA exp",
 /*  92 */ "special_args ::= exp COMMA exp COMMA exp COMMA exp COMMA TAG",
};
#endif /* NDEBUG */

/*
** This function returns the symbolic name associated with a token
** value.
*/
const char *ParseTokenName(int tokenType){
#ifndef NDEBUG
  if( tokenType>0 && tokenType<(sizeof(yyTokenName)/sizeof(yyTokenName[0])) ){
    return yyTokenName[tokenType];
  }else{
    return "Unknown";
  }
#else
  return "";
#endif
}

/* 
** This function allocates a new parser.
** The only argument is a pointer to a function which works like
** malloc.
**
** Inputs:
** A pointer to the function used to allocate memory.
**
** Outputs:
** A pointer to a parser.  This pointer is used in subsequent calls
** to Parse and ParseFree.
*/
void *ParseAlloc(void *(*mallocProc)(size_t)){
  yyParser *pParser;
  pParser = (yyParser*)(*mallocProc)( (size_t)sizeof(yyParser) );
  if( pParser ){
    pParser->yyidx = -1;
  }
  return pParser;
}

/* The following function deletes the value associated with a
** symbol.  The symbol can be either a terminal or nonterminal.
** "yymajor" is the symbol code, and "yypminor" is a pointer to
** the value.
*/
static void yy_destructor(YYCODETYPE yymajor, YYMINORTYPE *yypminor){
  switch( yymajor ){
    /* Here is inserted the actions which take place when a
    ** terminal or non-terminal is destroyed.  This can happen
    ** when the symbol is popped from the stack during a
    ** reduce or during error processing or when a parser is 
    ** being destroyed before it is finished parsing.
    **
    ** Note: during a reduce, the only symbols destroyed are those
    ** which appear on the RHS of the rule, but which are not used
    ** inside the C code.
    */
    default:  break;   /* If no destructor action specified: do nothing */
  }
}

/*
** Pop the parser's stack once.
**
** If there is a destructor routine associated with the token which
** is popped from the stack, then call it.
**
** Return the major token number for the symbol popped.
*/
static int yy_pop_parser_stack(yyParser *pParser){
  YYCODETYPE yymajor;
  yyStackEntry *yytos = &pParser->yystack[pParser->yyidx];

  if( pParser->yyidx<0 ) return 0;
#ifndef NDEBUG
  if( yyTraceFILE && pParser->yyidx>=0 ){
    fprintf(yyTraceFILE,"%sPopping %s\n",
      yyTracePrompt,
      yyTokenName[yytos->major]);
  }
#endif
  yymajor = yytos->major;
  yy_destructor( yymajor, &yytos->minor);
  pParser->yyidx--;
  return yymajor;
}

/* 
** Deallocate and destroy a parser.  Destructors are all called for
** all stack elements before shutting the parser down.
**
** Inputs:
** <ul>
** <li>  A pointer to the parser.  This should be a pointer
**       obtained from ParseAlloc.
** <li>  A pointer to a function used to reclaim memory obtained
**       from malloc.
** </ul>
*/
void ParseFree(
  void *p,                    /* The parser to be deleted */
  void (*freeProc)(void*)     /* Function used to reclaim memory */
){
  yyParser *pParser = (yyParser*)p;
  if( pParser==0 ) return;
  while( pParser->yyidx>=0 ) yy_pop_parser_stack(pParser);
  (*freeProc)((void*)pParser);
}

/*
** Find the appropriate action for a parser given the terminal
** look-ahead token iLookAhead.
**
** If the look-ahead token is YYNOCODE, then check to see if the action is
** independent of the look-ahead.  If it is, return the action, otherwise
** return YY_NO_ACTION.
*/
static int yy_find_shift_action(
  yyParser *pParser,        /* The parser */
  YYCODETYPE iLookAhead     /* The look-ahead token */
){
  int i;
  int stateno = pParser->yystack[pParser->yyidx].stateno;
 
  if( stateno>YY_SHIFT_MAX || (i = yy_shift_ofst[stateno])==YY_SHIFT_USE_DFLT ){
    return yy_default[stateno];
  }
  if( iLookAhead==YYNOCODE ){
    return YY_NO_ACTION;
  }
  i += iLookAhead;
  if( i<0 || i>=YY_SZ_ACTTAB || yy_lookahead[i]!=iLookAhead ){
    if( iLookAhead>0 ){
#ifdef YYFALLBACK
      int iFallback;            /* Fallback token */
      if( iLookAhead<sizeof(yyFallback)/sizeof(yyFallback[0])
             && (iFallback = yyFallback[iLookAhead])!=0 ){
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE, "%sFALLBACK %s => %s\n",
             yyTracePrompt, yyTokenName[iLookAhead], yyTokenName[iFallback]);
        }
#endif
        return yy_find_shift_action(pParser, iFallback);
      }
#endif
#ifdef YYWILDCARD
      {
        int j = i - iLookAhead + YYWILDCARD;
        if( j>=0 && j<YY_SZ_ACTTAB && yy_lookahead[j]==YYWILDCARD ){
#ifndef NDEBUG
          if( yyTraceFILE ){
            fprintf(yyTraceFILE, "%sWILDCARD %s => %s\n",
               yyTracePrompt, yyTokenName[iLookAhead], yyTokenName[YYWILDCARD]);
          }
#endif /* NDEBUG */
          return yy_action[j];
        }
      }
#endif /* YYWILDCARD */
    }
    return yy_default[stateno];
  }else{
    return yy_action[i];
  }
}

/*
** Find the appropriate action for a parser given the non-terminal
** look-ahead token iLookAhead.
**
** If the look-ahead token is YYNOCODE, then check to see if the action is
** independent of the look-ahead.  If it is, return the action, otherwise
** return YY_NO_ACTION.
*/
static int yy_find_reduce_action(
  int stateno,              /* Current state number */
  YYCODETYPE iLookAhead     /* The look-ahead token */
){
  int i;
  /* int stateno = pParser->yystack[pParser->yyidx].stateno; */
 
  if( stateno>YY_REDUCE_MAX ||
      (i = yy_reduce_ofst[stateno])==YY_REDUCE_USE_DFLT ){
    return yy_default[stateno];
  }
  if( iLookAhead==YYNOCODE ){
    return YY_NO_ACTION;
  }
  i += iLookAhead;
  if( i<0 || i>=YY_SZ_ACTTAB || yy_lookahead[i]!=iLookAhead ){
    return yy_default[stateno];
  }else{
    return yy_action[i];
  }
}

/*
** Perform a shift action.
*/
static void yy_shift(
  yyParser *yypParser,          /* The parser to be shifted */
  int yyNewState,               /* The new state to shift in */
  int yyMajor,                  /* The major token to shift in */
  YYMINORTYPE *yypMinor         /* Pointer ot the minor token to shift in */
){
  yyStackEntry *yytos;
  yypParser->yyidx++;
  if( yypParser->yyidx>=YYSTACKDEPTH ){
     ParseARG_FETCH;
     yypParser->yyidx--;
#ifndef NDEBUG
     if( yyTraceFILE ){
       fprintf(yyTraceFILE,"%sStack Overflow!\n",yyTracePrompt);
     }
#endif
     while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
     /* Here code is inserted which will execute if the parser
     ** stack ever overflows */
     ParseARG_STORE; /* Suppress warning about unused %extra_argument var */
     return;
  }
  yytos = &yypParser->yystack[yypParser->yyidx];
  yytos->stateno = yyNewState;
  yytos->major = yyMajor;
  yytos->minor = *yypMinor;
#ifndef NDEBUG
  if( yyTraceFILE && yypParser->yyidx>0 ){
    int i;
    fprintf(yyTraceFILE,"%sShift %d\n",yyTracePrompt,yyNewState);
    fprintf(yyTraceFILE,"%sStack:",yyTracePrompt);
    for(i=1; i<=yypParser->yyidx; i++)
      fprintf(yyTraceFILE," %s",yyTokenName[yypParser->yystack[i].major]);
    fprintf(yyTraceFILE,"\n");
  }
#endif
}

/* The following table contains information about every rule that
** is used during the reduce.
*/
static const struct {
  YYCODETYPE lhs;         /* Symbol on the left-hand side of the rule */
  unsigned char nrhs;     /* Number of right-hand side symbols in the rule */
} yyRuleInfo[] = {
  { 48, 1 },
  { 38, 1 },
  { 38, 1 },
  { 38, 3 },
  { 38, 3 },
  { 38, 3 },
  { 38, 3 },
  { 38, 3 },
  { 38, 3 },
  { 38, 3 },
  { 38, 2 },
  { 38, 3 },
  { 49, 0 },
  { 49, 1 },
  { 50, 1 },
  { 50, 1 },
  { 50, 1 },
  { 50, 1 },
  { 50, 1 },
  { 50, 1 },
  { 50, 1 },
  { 53, 4 },
  { 58, 0 },
  { 58, 1 },
  { 58, 3 },
  { 59, 1 },
  { 59, 1 },
  { 59, 1 },
  { 51, 5 },
  { 52, 2 },
  { 54, 3 },
  { 60, 2 },
  { 61, 0 },
  { 61, 1 },
  { 61, 3 },
  { 62, 1 },
  { 62, 3 },
  { 57, 3 },
  { 63, 1 },
  { 63, 3 },
  { 64, 6 },
  { 64, 6 },
  { 47, 0 },
  { 47, 1 },
  { 47, 3 },
  { 55, 8 },
  { 55, 8 },
  { 56, 11 },
  { 46, 0 },
  { 46, 2 },
  { 45, 3 },
  { 44, 1 },
  { 44, 1 },
  { 44, 1 },
  { 44, 1 },
  { 44, 1 },
  { 43, 1 },
  { 43, 1 },
  { 42, 1 },
  { 42, 4 },
  { 41, 1 },
  { 41, 3 },
  { 40, 3 },
  { 39, 0 },
  { 39, 1 },
  { 39, 3 },
  { 39, 5 },
  { 39, 7 },
  { 39, 9 },
  { 39, 3 },
  { 39, 5 },
  { 39, 7 },
  { 39, 9 },
  { 39, 1 },
  { 39, 3 },
  { 39, 5 },
  { 39, 7 },
  { 39, 9 },
  { 39, 1 },
  { 39, 3 },
  { 39, 5 },
  { 39, 7 },
  { 39, 9 },
  { 39, 3 },
  { 39, 5 },
  { 39, 7 },
  { 39, 9 },
  { 39, 5 },
  { 39, 7 },
  { 39, 9 },
  { 39, 7 },
  { 39, 9 },
  { 39, 9 },
};

static void yy_accept(yyParser*);  /* Forward Declaration */

/*
** Perform a reduce action and the shift that must immediately
** follow the reduce.
*/
static void yy_reduce(
  yyParser *yypParser,         /* The parser */
  int yyruleno                 /* Number of the rule by which to reduce */
){
  int yygoto;                     /* The next state */
  int yyact;                      /* The next action */
  YYMINORTYPE yygotominor;        /* The LHS of the rule reduced */
  yyStackEntry *yymsp;            /* The top of the parser's stack */
  int yysize;                     /* Amount to pop the stack */
  ParseARG_FETCH;
  yymsp = &yypParser->yystack[yypParser->yyidx];
#ifndef NDEBUG
  if( yyTraceFILE && yyruleno>=0 
        && yyruleno<(int)(sizeof(yyRuleName)/sizeof(yyRuleName[0])) ){
    fprintf(yyTraceFILE, "%sReduce [%s].\n", yyTracePrompt,
      yyRuleName[yyruleno]);
  }
#endif /* NDEBUG */

  /* Silence complaints from purify about yygotominor being uninitialized
  ** in some cases when it is copied into the stack after the following
  ** switch.  yygotominor is uninitialized when a rule reduces that does
  ** not set the value of its left-hand side nonterminal.  Leaving the
  ** value of the nonterminal uninitialized is utterly harmless as long
  ** as the value is never used.  So really the only thing this code
  ** accomplishes is to quieten purify.  
  **
  ** 2007-01-16:  The wireshark project (www.wireshark.org) reports that
  ** without this code, their parser segfaults.  I'm not sure what there
  ** parser is doing to make this happen.  This is the second bug report
  ** from wireshark this week.  Clearly they are stressing Lemon in ways
  ** that it has not been previously stressed...  (SQLite ticket #2172)
  */
  memset(&yygotominor, 0, sizeof(yygotominor));


  switch( yyruleno ){
  /* Beginning here are the reduction cases.  A typical example
  ** follows:
  **   case 0:
  **  #line <lineno> <grammarfile>
  **     { ... }           // User supplied code
  **  #line <lineno> <thisfile>
  **     break;
  */
      case 1:
#line 344 "xlat-parse.y"
{ yygotominor.yy72 = yymsp[0].minor.yy0.val; }
#line 1197 "xlat-parse.c"
        break;
      case 2:
#line 345 "xlat-parse.y"
{ yygotominor.yy72 = yymsp[0].minor.yy0.symval->Value; }
#line 1202 "xlat-parse.c"
        break;
      case 3:
#line 346 "xlat-parse.y"
{ yygotominor.yy72 = yymsp[-2].minor.yy72 + yymsp[0].minor.yy72; }
#line 1207 "xlat-parse.c"
        break;
      case 4:
#line 347 "xlat-parse.y"
{ yygotominor.yy72 = yymsp[-2].minor.yy72 - yymsp[0].minor.yy72; }
#line 1212 "xlat-parse.c"
        break;
      case 5:
#line 348 "xlat-parse.y"
{ yygotominor.yy72 = yymsp[-2].minor.yy72 * yymsp[0].minor.yy72; }
#line 1217 "xlat-parse.c"
        break;
      case 6:
#line 349 "xlat-parse.y"
{ yygotominor.yy72 = yymsp[-2].minor.yy72 / yymsp[0].minor.yy72; }
#line 1222 "xlat-parse.c"
        break;
      case 7:
#line 350 "xlat-parse.y"
{ yygotominor.yy72 = yymsp[-2].minor.yy72 | yymsp[0].minor.yy72; }
#line 1227 "xlat-parse.c"
        break;
      case 8:
#line 351 "xlat-parse.y"
{ yygotominor.yy72 = yymsp[-2].minor.yy72 & yymsp[0].minor.yy72; }
#line 1232 "xlat-parse.c"
        break;
      case 9:
#line 352 "xlat-parse.y"
{ yygotominor.yy72 = yymsp[-2].minor.yy72 ^ yymsp[0].minor.yy72; }
#line 1237 "xlat-parse.c"
        break;
      case 10:
#line 353 "xlat-parse.y"
{ yygotominor.yy72 = -yymsp[0].minor.yy72; }
#line 1242 "xlat-parse.c"
        break;
      case 11:
#line 354 "xlat-parse.y"
{ yygotominor.yy72 = yymsp[-1].minor.yy72; }
#line 1247 "xlat-parse.c"
        break;
      case 21:
#line 368 "xlat-parse.y"
{
  printf ("\n");
}
#line 1254 "xlat-parse.c"
        break;
      case 25:
#line 376 "xlat-parse.y"
{ printf ("%s", yymsp[0].minor.yy0.string); }
#line 1259 "xlat-parse.c"
        break;
      case 26:
#line 377 "xlat-parse.y"
{ printf ("%d", yymsp[0].minor.yy72); }
#line 1264 "xlat-parse.c"
        break;
      case 27:
#line 378 "xlat-parse.y"
{ printf ("\n"); }
#line 1269 "xlat-parse.c"
        break;
      case 28:
#line 381 "xlat-parse.y"
{
	AddSym (yymsp[-3].minor.yy0.sym, yymsp[-1].minor.yy72);
}
#line 1276 "xlat-parse.c"
        break;
      case 29:
#line 386 "xlat-parse.y"
{
	IncludeFile (yymsp[0].minor.yy0.string);
}
#line 1283 "xlat-parse.c"
        break;
      case 31:
#line 393 "xlat-parse.y"
{
	EnumVal = 0;
}
#line 1290 "xlat-parse.c"
        break;
      case 35:
#line 402 "xlat-parse.y"
{
	AddSym (yymsp[0].minor.yy0.sym, EnumVal++);
}
#line 1297 "xlat-parse.c"
        break;
      case 36:
#line 407 "xlat-parse.y"
{
	AddSym (yymsp[-2].minor.yy0.sym, EnumVal = yymsp[0].minor.yy72);
}
#line 1304 "xlat-parse.c"
        break;
      case 40:
#line 420 "xlat-parse.y"
{
	AddSym (yymsp[-3].minor.yy0.sym, yymsp[-5].minor.yy72);
}
#line 1311 "xlat-parse.c"
        break;
      case 41:
#line 424 "xlat-parse.y"
{
	printf ("%s, line %d: %s is already defined\n", SourceName, SourceLine, yymsp[-3].minor.yy0.symval->Sym);
}
#line 1318 "xlat-parse.c"
        break;
      case 45:
#line 433 "xlat-parse.y"
{
	Simple[yymsp[-7].minor.yy72].NewSpecial = yymsp[-3].minor.yy72;
	Simple[yymsp[-7].minor.yy72].Flags = yymsp[-5].minor.yy72 | yymsp[-1].minor.yy57.addflags;
	Simple[yymsp[-7].minor.yy72].Args[0] = yymsp[-1].minor.yy57.args[0];
	Simple[yymsp[-7].minor.yy72].Args[1] = yymsp[-1].minor.yy57.args[1];
	Simple[yymsp[-7].minor.yy72].Args[2] = yymsp[-1].minor.yy57.args[2];
	Simple[yymsp[-7].minor.yy72].Args[3] = yymsp[-1].minor.yy57.args[3];
	Simple[yymsp[-7].minor.yy72].Args[4] = yymsp[-1].minor.yy57.args[4];
}
#line 1331 "xlat-parse.c"
        break;
      case 46:
#line 443 "xlat-parse.y"
{
	printf ("%s, line %d: %s is undefined\n", SourceName, SourceLine, yymsp[-3].minor.yy0.sym);
}
#line 1338 "xlat-parse.c"
        break;
      case 47:
#line 448 "xlat-parse.y"
{
	if (NumBoomish == MAX_BOOMISH)
	{
		MoreLines *probe = yymsp[-1].minor.yy49;

		while (probe != NULL)
		{
			MoreLines *next = probe->next;
			free (probe);
			probe = next;
		}
		printf ("%s, line %d: Too many BOOM translators\n", SourceName, SourceLine);
	}
	else
	{
		int i;
		MoreLines *probe;

		Boomish[NumBoomish].FirstLinetype = yymsp[-6].minor.yy72;
		Boomish[NumBoomish].LastLinetype = yymsp[-4].minor.yy72;
		Boomish[NumBoomish].NewSpecial = yymsp[-9].minor.yy72;
		
		for (i = 0, probe = yymsp[-1].minor.yy49; probe != NULL; i++)
		{
			MoreLines *next = probe->next;;
			if (i < MAX_BOOMISH_EXEC)
			{
				Boomish[NumBoomish].Args[i] = probe->arg;
			}
			else if (i == MAX_BOOMISH_EXEC)
			{
				printf ("%s, line %d: Too many commands for this BOOM translator\n", SourceName, SourceLine);
			}
			free (probe);
			probe = next;
		}
		if (i < MAX_BOOMISH_EXEC)
		{
			Boomish[NumBoomish].Args[i].bDefined = 0;
		}
		NumBoomish++;
	}
}
#line 1385 "xlat-parse.c"
        break;
      case 48:
#line 493 "xlat-parse.y"
{
	yygotominor.yy49 = NULL;
}
#line 1392 "xlat-parse.c"
        break;
      case 49:
#line 497 "xlat-parse.y"
{
	yygotominor.yy49 = malloc (sizeof(MoreLines));
	yygotominor.yy49->next = yymsp[0].minor.yy49;
	yygotominor.yy49->arg = yymsp[-1].minor.yy58;
}
#line 1401 "xlat-parse.c"
        break;
      case 50:
#line 504 "xlat-parse.y"
{
	yygotominor.yy58.bDefined = 1;
	yygotominor.yy58.bOrExisting = (yymsp[-1].minor.yy72 == OR_EQUAL);
	yygotominor.yy58.bUseConstant = (yymsp[0].minor.yy83.filters == NULL);
	yygotominor.yy58.ArgNum = yymsp[-2].minor.yy72;
	yygotominor.yy58.ConstantValue = yymsp[0].minor.yy83.constant;
	yygotominor.yy58.AndValue = yymsp[0].minor.yy83.mask;

	if (yymsp[0].minor.yy83.filters != NULL)
	{
		int i;
		MoreFilters *probe;

		for (i = 0, probe = yymsp[0].minor.yy83.filters; probe != NULL; i++)
		{
			MoreFilters *next = probe->next;
			if (i < 15)
			{
				yygotominor.yy58.ResultFilter[i] = probe->filter.filter;
				yygotominor.yy58.ResultValue[i] = probe->filter.value;
			}
			else if (i == 15)
			{
				yyerror ("Lists can only have 15 elements");
			}
			free (probe);
			probe = next;
		}
		yygotominor.yy58.ListSize = i > 15 ? 15 : i;
	}
}
#line 1436 "xlat-parse.c"
        break;
      case 51:
#line 536 "xlat-parse.y"
{ yygotominor.yy72 = 4; }
#line 1441 "xlat-parse.c"
        break;
      case 52:
#line 537 "xlat-parse.y"
{ yygotominor.yy72 = 0; }
#line 1446 "xlat-parse.c"
        break;
      case 53:
#line 538 "xlat-parse.y"
{ yygotominor.yy72 = 1; }
#line 1451 "xlat-parse.c"
        break;
      case 54:
#line 539 "xlat-parse.y"
{ yygotominor.yy72 = 2; }
#line 1456 "xlat-parse.c"
        break;
      case 55:
#line 540 "xlat-parse.y"
{ yygotominor.yy72 = 3; }
#line 1461 "xlat-parse.c"
        break;
      case 56:
#line 542 "xlat-parse.y"
{ yygotominor.yy72 = '='; }
#line 1466 "xlat-parse.c"
        break;
      case 57:
#line 543 "xlat-parse.y"
{ yygotominor.yy72 = OR_EQUAL; }
#line 1471 "xlat-parse.c"
        break;
      case 58:
#line 546 "xlat-parse.y"
{
	yygotominor.yy83.constant = yymsp[0].minor.yy72;
	yygotominor.yy83.filters = NULL;
}
#line 1479 "xlat-parse.c"
        break;
      case 59:
#line 551 "xlat-parse.y"
{
	yygotominor.yy83.mask = yymsp[-3].minor.yy72;
	yygotominor.yy83.filters = yymsp[-1].minor.yy87;
}
#line 1487 "xlat-parse.c"
        break;
      case 60:
#line 557 "xlat-parse.y"
{
	yygotominor.yy87 = malloc (sizeof(MoreFilters));
	yygotominor.yy87->next = NULL;
	yygotominor.yy87->filter = yymsp[0].minor.yy124;
}
#line 1496 "xlat-parse.c"
        break;
      case 61:
#line 563 "xlat-parse.y"
{
	yygotominor.yy87 = malloc (sizeof(MoreFilters));
	yygotominor.yy87->next = yymsp[0].minor.yy87;
	yygotominor.yy87->filter = yymsp[-2].minor.yy124;
}
#line 1505 "xlat-parse.c"
        break;
      case 62:
#line 570 "xlat-parse.y"
{
	yygotominor.yy124.filter = yymsp[-2].minor.yy72;
	yygotominor.yy124.value = yymsp[0].minor.yy72;
}
#line 1513 "xlat-parse.c"
        break;
      case 63:
#line 576 "xlat-parse.y"
{
	yygotominor.yy57.addflags = 0;
	memset (yygotominor.yy57.args, 0, 5);
}
#line 1521 "xlat-parse.c"
        break;
      case 64:
#line 581 "xlat-parse.y"
{
	yygotominor.yy57.addflags = SIMPLE_HASTAGAT1;
	memset (yygotominor.yy57.args, 0, 5);
}
#line 1529 "xlat-parse.c"
        break;
      case 65:
#line 586 "xlat-parse.y"
{
	yygotominor.yy57.addflags = SIMPLE_HASTAGAT1;
	yygotominor.yy57.args[0] = 0;
	yygotominor.yy57.args[1] = yymsp[0].minor.yy72;
	yygotominor.yy57.args[2] = 0;
	yygotominor.yy57.args[3] = 0;
	yygotominor.yy57.args[4] = 0;
}
#line 1541 "xlat-parse.c"
        break;
      case 66:
#line 595 "xlat-parse.y"
{
	yygotominor.yy57.addflags = SIMPLE_HASTAGAT1;
	yygotominor.yy57.args[0] = 0;
	yygotominor.yy57.args[1] = yymsp[-2].minor.yy72;
	yygotominor.yy57.args[2] = yymsp[0].minor.yy72;
	yygotominor.yy57.args[3] = 0;
	yygotominor.yy57.args[4] = 0;
}
#line 1553 "xlat-parse.c"
        break;
      case 67:
#line 604 "xlat-parse.y"
{
	yygotominor.yy57.addflags = SIMPLE_HASTAGAT1;
	yygotominor.yy57.args[0] = 0;
	yygotominor.yy57.args[1] = yymsp[-4].minor.yy72;
	yygotominor.yy57.args[2] = yymsp[-2].minor.yy72;
	yygotominor.yy57.args[3] = yymsp[0].minor.yy72;
	yygotominor.yy57.args[4] = 0;
}
#line 1565 "xlat-parse.c"
        break;
      case 68:
#line 613 "xlat-parse.y"
{
	yygotominor.yy57.addflags = SIMPLE_HASTAGAT1;
	yygotominor.yy57.args[0] = 0;
	yygotominor.yy57.args[1] = yymsp[-6].minor.yy72;
	yygotominor.yy57.args[2] = yymsp[-4].minor.yy72;
	yygotominor.yy57.args[3] = yymsp[-2].minor.yy72;
	yygotominor.yy57.args[4] = yymsp[0].minor.yy72;
}
#line 1577 "xlat-parse.c"
        break;
      case 69:
#line 622 "xlat-parse.y"
{
	yygotominor.yy57.addflags = SIMPLE_HAS2TAGS;
	yygotominor.yy57.args[0] = yygotominor.yy57.args[1] = 0;
	yygotominor.yy57.args[2] = 0;
	yygotominor.yy57.args[3] = 0;
	yygotominor.yy57.args[4] = 0;
}
#line 1588 "xlat-parse.c"
        break;
      case 70:
#line 630 "xlat-parse.y"
{
	yygotominor.yy57.addflags = SIMPLE_HAS2TAGS;
	yygotominor.yy57.args[0] = yygotominor.yy57.args[1] = 0;
	yygotominor.yy57.args[2] = yymsp[0].minor.yy72;
	yygotominor.yy57.args[3] = 0;
	yygotominor.yy57.args[4] = 0;
}
#line 1599 "xlat-parse.c"
        break;
      case 71:
#line 638 "xlat-parse.y"
{
	yygotominor.yy57.addflags = SIMPLE_HAS2TAGS;
	yygotominor.yy57.args[0] = yygotominor.yy57.args[1] = 0;
	yygotominor.yy57.args[2] = yymsp[-2].minor.yy72;
	yygotominor.yy57.args[3] = yymsp[0].minor.yy72;
	yygotominor.yy57.args[4] = 0;
}
#line 1610 "xlat-parse.c"
        break;
      case 72:
#line 646 "xlat-parse.y"
{
	yygotominor.yy57.addflags = SIMPLE_HAS2TAGS;
	yygotominor.yy57.args[0] = yygotominor.yy57.args[1] = 0;
	yygotominor.yy57.args[2] = yymsp[-4].minor.yy72;
	yygotominor.yy57.args[3] = yymsp[-2].minor.yy72;
	yygotominor.yy57.args[4] = yymsp[0].minor.yy72;
}
#line 1621 "xlat-parse.c"
        break;
      case 73:
#line 654 "xlat-parse.y"
{
	yygotominor.yy57.addflags = SIMPLE_HASLINEID;
	memset (yygotominor.yy57.args, 0, 5);
}
#line 1629 "xlat-parse.c"
        break;
      case 74:
#line 659 "xlat-parse.y"
{
	yygotominor.yy57.addflags = SIMPLE_HASLINEID;
	yygotominor.yy57.args[0] = 0;
	yygotominor.yy57.args[1] = yymsp[0].minor.yy72;
	yygotominor.yy57.args[2] = 0;
	yygotominor.yy57.args[3] = 0;
	yygotominor.yy57.args[4] = 0;
}
#line 1641 "xlat-parse.c"
        break;
      case 75:
#line 668 "xlat-parse.y"
{
	yygotominor.yy57.addflags = SIMPLE_HASLINEID;
	yygotominor.yy57.args[0] = 0;
	yygotominor.yy57.args[1] = yymsp[-2].minor.yy72;
	yygotominor.yy57.args[2] = yymsp[0].minor.yy72;
	yygotominor.yy57.args[3] = 0;
	yygotominor.yy57.args[4] = 0;
}
#line 1653 "xlat-parse.c"
        break;
      case 76:
#line 677 "xlat-parse.y"
{
	yygotominor.yy57.addflags = SIMPLE_HASLINEID;
	yygotominor.yy57.args[0] = 0;
	yygotominor.yy57.args[1] = yymsp[-4].minor.yy72;
	yygotominor.yy57.args[2] = yymsp[-2].minor.yy72;
	yygotominor.yy57.args[3] = yymsp[0].minor.yy72;
	yygotominor.yy57.args[4] = 0;
}
#line 1665 "xlat-parse.c"
        break;
      case 77:
#line 686 "xlat-parse.y"
{
	yygotominor.yy57.addflags = SIMPLE_HASLINEID;
	yygotominor.yy57.args[0] = 0;
	yygotominor.yy57.args[1] = yymsp[-6].minor.yy72;
	yygotominor.yy57.args[2] = yymsp[-4].minor.yy72;
	yygotominor.yy57.args[3] = yymsp[-2].minor.yy72;
	yygotominor.yy57.args[4] = yymsp[0].minor.yy72;
}
#line 1677 "xlat-parse.c"
        break;
      case 78:
#line 695 "xlat-parse.y"
{
	yygotominor.yy57.addflags = 0;
	yygotominor.yy57.args[0] = yymsp[0].minor.yy72;
	yygotominor.yy57.args[1] = 0;
	yygotominor.yy57.args[2] = 0;
	yygotominor.yy57.args[3] = 0;
	yygotominor.yy57.args[4] = 0;
}
#line 1689 "xlat-parse.c"
        break;
      case 79:
#line 704 "xlat-parse.y"
{
	yygotominor.yy57.addflags = 0;
	yygotominor.yy57.args[0] = yymsp[-2].minor.yy72;
	yygotominor.yy57.args[1] = yymsp[0].minor.yy72;
	yygotominor.yy57.args[2] = 0;
	yygotominor.yy57.args[3] = 0;
	yygotominor.yy57.args[4] = 0;
}
#line 1701 "xlat-parse.c"
        break;
      case 80:
#line 713 "xlat-parse.y"
{
	yygotominor.yy57.addflags = 0;
	yygotominor.yy57.args[0] = yymsp[-4].minor.yy72;
	yygotominor.yy57.args[1] = yymsp[-2].minor.yy72;
	yygotominor.yy57.args[2] = yymsp[0].minor.yy72;
	yygotominor.yy57.args[3] = 0;
	yygotominor.yy57.args[4] = 0;
}
#line 1713 "xlat-parse.c"
        break;
      case 81:
#line 722 "xlat-parse.y"
{
	yygotominor.yy57.addflags = 0;
	yygotominor.yy57.args[0] = yymsp[-6].minor.yy72;
	yygotominor.yy57.args[1] = yymsp[-4].minor.yy72;
	yygotominor.yy57.args[2] = yymsp[-2].minor.yy72;
	yygotominor.yy57.args[3] = yymsp[0].minor.yy72;
	yygotominor.yy57.args[4] = 0;
}
#line 1725 "xlat-parse.c"
        break;
      case 82:
#line 731 "xlat-parse.y"
{
	yygotominor.yy57.addflags = 0;
	yygotominor.yy57.args[0] = yymsp[-8].minor.yy72;
	yygotominor.yy57.args[1] = yymsp[-6].minor.yy72;
	yygotominor.yy57.args[2] = yymsp[-4].minor.yy72;
	yygotominor.yy57.args[3] = yymsp[-2].minor.yy72;
	yygotominor.yy57.args[4] = yymsp[0].minor.yy72;
}
#line 1737 "xlat-parse.c"
        break;
      case 83:
#line 740 "xlat-parse.y"
{
	yygotominor.yy57.addflags = SIMPLE_HASTAGAT2;
	yygotominor.yy57.args[0] = yymsp[-2].minor.yy72;
	yygotominor.yy57.args[1] = 0;
	yygotominor.yy57.args[2] = 0;
	yygotominor.yy57.args[3] = 0;
	yygotominor.yy57.args[4] = 0;
}
#line 1749 "xlat-parse.c"
        break;
      case 84:
#line 749 "xlat-parse.y"
{
	yygotominor.yy57.addflags = SIMPLE_HASTAGAT2;
	yygotominor.yy57.args[0] = yymsp[-4].minor.yy72;
	yygotominor.yy57.args[1] = 0;
	yygotominor.yy57.args[2] = yymsp[0].minor.yy72;
	yygotominor.yy57.args[3] = 0;
	yygotominor.yy57.args[4] = 0;
}
#line 1761 "xlat-parse.c"
        break;
      case 85:
#line 758 "xlat-parse.y"
{
	yygotominor.yy57.addflags = SIMPLE_HASTAGAT2;
	yygotominor.yy57.args[0] = yymsp[-6].minor.yy72;
	yygotominor.yy57.args[1] = 0;
	yygotominor.yy57.args[2] = yymsp[-2].minor.yy72;
	yygotominor.yy57.args[3] = yymsp[0].minor.yy72;
	yygotominor.yy57.args[4] = 0;
}
#line 1773 "xlat-parse.c"
        break;
      case 86:
#line 767 "xlat-parse.y"
{
	yygotominor.yy57.addflags = SIMPLE_HASTAGAT2;
	yygotominor.yy57.args[0] = yymsp[-8].minor.yy72;
	yygotominor.yy57.args[1] = 0;
	yygotominor.yy57.args[2] = yymsp[-4].minor.yy72;
	yygotominor.yy57.args[3] = yymsp[-2].minor.yy72;
	yygotominor.yy57.args[4] = yymsp[0].minor.yy72;
}
#line 1785 "xlat-parse.c"
        break;
      case 87:
#line 776 "xlat-parse.y"
{
	yygotominor.yy57.addflags = SIMPLE_HASTAGAT3;
	yygotominor.yy57.args[0] = yymsp[-4].minor.yy72;
	yygotominor.yy57.args[1] = yymsp[-2].minor.yy72;
	yygotominor.yy57.args[2] = 0;
	yygotominor.yy57.args[3] = 0;
	yygotominor.yy57.args[4] = 0;
}
#line 1797 "xlat-parse.c"
        break;
      case 88:
#line 785 "xlat-parse.y"
{
	yygotominor.yy57.addflags = SIMPLE_HASTAGAT3;
	yygotominor.yy57.args[0] = yymsp[-6].minor.yy72;
	yygotominor.yy57.args[1] = yymsp[-4].minor.yy72;
	yygotominor.yy57.args[2] = 0;
	yygotominor.yy57.args[3] = yymsp[0].minor.yy72;
	yygotominor.yy57.args[4] = 0;
}
#line 1809 "xlat-parse.c"
        break;
      case 89:
#line 794 "xlat-parse.y"
{
	yygotominor.yy57.addflags = SIMPLE_HASTAGAT3;
	yygotominor.yy57.args[0] = yymsp[-8].minor.yy72;
	yygotominor.yy57.args[1] = yymsp[-6].minor.yy72;
	yygotominor.yy57.args[2] = 0;
	yygotominor.yy57.args[3] = yymsp[-2].minor.yy72;
	yygotominor.yy57.args[4] = yymsp[0].minor.yy72;
}
#line 1821 "xlat-parse.c"
        break;
      case 90:
#line 803 "xlat-parse.y"
{
	yygotominor.yy57.addflags = SIMPLE_HASTAGAT4;
	yygotominor.yy57.args[0] = yymsp[-6].minor.yy72;
	yygotominor.yy57.args[1] = yymsp[-4].minor.yy72;
	yygotominor.yy57.args[2] = yymsp[-2].minor.yy72;
	yygotominor.yy57.args[3] = 0;
	yygotominor.yy57.args[4] = 0;
}
#line 1833 "xlat-parse.c"
        break;
      case 91:
#line 812 "xlat-parse.y"
{
	yygotominor.yy57.addflags = SIMPLE_HASTAGAT4;
	yygotominor.yy57.args[0] = yymsp[-8].minor.yy72;
	yygotominor.yy57.args[1] = yymsp[-6].minor.yy72;
	yygotominor.yy57.args[2] = yymsp[-4].minor.yy72;
	yygotominor.yy57.args[3] = 0;
	yygotominor.yy57.args[4] = yymsp[0].minor.yy72;
}
#line 1845 "xlat-parse.c"
        break;
      case 92:
#line 821 "xlat-parse.y"
{
	yygotominor.yy57.addflags = SIMPLE_HASTAGAT5;
	yygotominor.yy57.args[0] = yymsp[-8].minor.yy72;
	yygotominor.yy57.args[1] = yymsp[-6].minor.yy72;
	yygotominor.yy57.args[2] = yymsp[-4].minor.yy72;
	yygotominor.yy57.args[3] = yymsp[-2].minor.yy72;
	yygotominor.yy57.args[4] = 0;
}
#line 1857 "xlat-parse.c"
        break;
  };
  yygoto = yyRuleInfo[yyruleno].lhs;
  yysize = yyRuleInfo[yyruleno].nrhs;
  yypParser->yyidx -= yysize;
  yyact = yy_find_reduce_action(yymsp[-yysize].stateno,yygoto);
  if( yyact < YYNSTATE ){
#ifdef NDEBUG
    /* If we are not debugging and the reduce action popped at least
    ** one element off the stack, then we can push the new element back
    ** onto the stack here, and skip the stack overflow test in yy_shift().
    ** That gives a significant speed improvement. */
    if( yysize ){
      yypParser->yyidx++;
      yymsp -= yysize-1;
      yymsp->stateno = yyact;
      yymsp->major = yygoto;
      yymsp->minor = yygotominor;
    }else
#endif
    {
      yy_shift(yypParser,yyact,yygoto,&yygotominor);
    }
  }else if( yyact == YYNSTATE + YYNRULE + 1 ){
    yy_accept(yypParser);
  }
}

/*
** The following code executes when the parse fails
*/
static void yy_parse_failed(
  yyParser *yypParser           /* The parser */
){
  ParseARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sFail!\n",yyTracePrompt);
  }
#endif
  while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser fails */
  ParseARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/*
** The following code executes when a syntax error first occurs.
*/
static void yy_syntax_error(
  yyParser *yypParser,           /* The parser */
  int yymajor,                   /* The major type of the error token */
  YYMINORTYPE yyminor            /* The minor type of the error token */
){
  ParseARG_FETCH;
#define TOKEN (yyminor.yy0)
#line 322 "xlat-parse.y"
yyerror("syntax error");
#line 1916 "xlat-parse.c"
  ParseARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/*
** The following is executed when the parser accepts
*/
static void yy_accept(
  yyParser *yypParser           /* The parser */
){
  ParseARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sAccept!\n",yyTracePrompt);
  }
#endif
  while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser accepts */
  ParseARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/* The main parser program.
** The first argument is a pointer to a structure obtained from
** "ParseAlloc" which describes the current state of the parser.
** The second argument is the major token number.  The third is
** the minor token.  The fourth optional argument is whatever the
** user wants (and specified in the grammar) and is available for
** use by the action routines.
**
** Inputs:
** <ul>
** <li> A pointer to the parser (an opaque structure.)
** <li> The major token number.
** <li> The minor token number.
** <li> An option argument of a grammar-specified type.
** </ul>
**
** Outputs:
** None.
*/
void Parse(
  void *yyp,                   /* The parser */
  int yymajor,                 /* The major token code number */
  ParseTOKENTYPE yyminor       /* The value for the token */
  ParseARG_PDECL               /* Optional %extra_argument parameter */
){
  YYMINORTYPE yyminorunion;
  int yyact;            /* The parser action. */
  int yyendofinput;     /* True if we are at the end of input */
  int yyerrorhit = 0;   /* True if yymajor has invoked an error */
  yyParser *yypParser;  /* The parser */

  /* (re)initialize the parser, if necessary */
  yypParser = (yyParser*)yyp;
  if( yypParser->yyidx<0 ){
    /* if( yymajor==0 ) return; // not sure why this was here... */
    yypParser->yyidx = 0;
    yypParser->yyerrcnt = -1;
    yypParser->yystack[0].stateno = 0;
    yypParser->yystack[0].major = 0;
  }
  yyminorunion.yy0 = yyminor;
  yyendofinput = (yymajor==0);
  ParseARG_STORE;

#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sInput %s\n",yyTracePrompt,yyTokenName[yymajor]);
  }
#endif

  do{
    yyact = yy_find_shift_action(yypParser,yymajor);
    if( yyact<YYNSTATE ){
      yy_shift(yypParser,yyact,yymajor,&yyminorunion);
      yypParser->yyerrcnt--;
      if( yyendofinput && yypParser->yyidx>=0 ){
        yymajor = 0;
      }else{
        yymajor = YYNOCODE;
        while( yypParser->yyidx>= 0 && (yyact = yy_find_shift_action(yypParser,YYNOCODE)) < YYNSTATE + YYNRULE ){
          yy_reduce(yypParser,yyact-YYNSTATE);
        }
      }
    }else if( yyact < YYNSTATE + YYNRULE ){
      yy_reduce(yypParser,yyact-YYNSTATE);
    }else if( yyact == YY_ERROR_ACTION ){
      int yymx;
#ifndef NDEBUG
      if( yyTraceFILE ){
        fprintf(yyTraceFILE,"%sSyntax Error!\n",yyTracePrompt);
      }
#endif
#ifdef YYERRORSYMBOL
      /* A syntax error has occurred.
      ** The response to an error depends upon whether or not the
      ** grammar defines an error token "ERROR".  
      **
      ** This is what we do if the grammar does define ERROR:
      **
      **  * Call the %syntax_error function.
      **
      **  * Begin popping the stack until we enter a state where
      **    it is legal to shift the error symbol, then shift
      **    the error symbol.
      **
      **  * Set the error count to three.
      **
      **  * Begin accepting and shifting new tokens.  No new error
      **    processing will occur until three tokens have been
      **    shifted successfully.
      **
      */
      if( yypParser->yyerrcnt<0 ){
        yy_syntax_error(yypParser,yymajor,yyminorunion);
      }
      yymx = yypParser->yystack[yypParser->yyidx].major;
      if( yymx==YYERRORSYMBOL || yyerrorhit ){
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE,"%sDiscard input token %s\n",
             yyTracePrompt,yyTokenName[yymajor]);
        }
#endif
        yy_destructor(yymajor,&yyminorunion);
        yymajor = YYNOCODE;
      }else{
         while(
          yypParser->yyidx >= 0 &&
          yymx != YYERRORSYMBOL &&
          (yyact = yy_find_reduce_action(
                        yypParser->yystack[yypParser->yyidx].stateno,
                        YYERRORSYMBOL)) >= YYNSTATE
        ){
          yy_pop_parser_stack(yypParser);
        }
        if( yypParser->yyidx < 0 || yymajor==0 ){
          yy_destructor(yymajor,&yyminorunion);
          yy_parse_failed(yypParser);
          yymajor = YYNOCODE;
        }else if( yymx!=YYERRORSYMBOL ){
          YYMINORTYPE u2;
          u2.YYERRSYMDT = 0;
          yy_shift(yypParser,yyact,YYERRORSYMBOL,&u2);
        }
      }
      yypParser->yyerrcnt = 3;
      yyerrorhit = 1;
#else  /* YYERRORSYMBOL is not defined */
      /* This is what we do if the grammar does not define ERROR:
      **
      **  * Report an error message, and throw away the input token.
      **
      **  * If the input token is $, then fail the parse.
      **
      ** As before, subsequent error messages are suppressed until
      ** three input tokens have been successfully shifted.
      */
      if( yypParser->yyerrcnt<=0 ){
        yy_syntax_error(yypParser,yymajor,yyminorunion);
      }
      yypParser->yyerrcnt = 3;
      yy_destructor(yymajor,&yyminorunion);
      if( yyendofinput ){
        yy_parse_failed(yypParser);
      }
      yymajor = YYNOCODE;
#endif
    }else{
      yy_accept(yypParser);
      yymajor = YYNOCODE;
    }
  }while( yymajor!=YYNOCODE && yypParser->yyidx>=0 );
  return;
}

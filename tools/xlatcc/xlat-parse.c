/* Driver template for the LEMON parser generator.
** The author disclaims copyright to this source code.
*/
/* First off, code is included which follows the "include" declaration
** in the input file. */
#include <stdio.h>
#include <string.h>
#include <assert.h>
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
	int include_state = 0;

	while ((tokentype = yylex(&token)) != 0)
	{
		/* Whenever the sequence INCLUDE STRING is encountered in the token
		 * stream, feed a dummy NOP token to the parser so that it will
		 * reduce the include_statement before grabbing any more tokens
		 * from the current file.
		 */
		if (tokentype == INCLUDE && include_state == 0)
		{
			include_state = 1;
		}
		else if (tokentype == STRING && include_state == 1)
		{
			include_state = 2;
		}
		else
		{
			include_state = 0;
		}
		Parse (pParser, tokentype, token);
		if (include_state == 2)
		{
			include_state = 0;
			Parse (pParser, NOP, token);
		}
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

#line 351 "xlat-parse.c"
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
**    YYSTACKDEPTH       is the maximum depth of the parser's stack.  If
**                       zero the stack is dynamically sized using realloc()
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
#define YYNOCODE 67
#define YYACTIONTYPE unsigned short int
#define ParseTOKENTYPE YYSTYPE
typedef union {
  ParseTOKENTYPE yy0;
  struct ListFilter yy20;
  MoreFilters * yy39;
  MoreLines * yy57;
  int yy64;
  BoomArg yy65;
  ParseBoomArg yy87;
  SpecialArgs yy123;
} YYMINORTYPE;
#ifndef YYSTACKDEPTH
#define YYSTACKDEPTH 100
#endif
#define ParseARG_SDECL
#define ParseARG_PDECL
#define ParseARG_FETCH
#define ParseARG_STORE
#define YYNSTATE 174
#define YYNRULE 94
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
 /*     0 */   174,   30,   71,  123,   30,   70,  157,  152,   26,  157,
 /*    10 */   152,   26,   79,  144,  115,   83,  126,   80,  104,  166,
 /*    20 */   109,   64,  120,  128,  127,    6,   71,  102,   53,  154,
 /*    30 */   150,  164,  149,  148,  147,  146,  145,  100,  140,   16,
 /*    40 */   162,   39,   34,   37,   54,   32,   55,   41,   39,   34,
 /*    50 */    37,   54,   32,   55,   41,   49,   34,   37,   54,   32,
 /*    60 */    55,   41,   51,   11,   39,   34,   37,   54,   32,   55,
 /*    70 */    41,   39,   34,   37,   54,   32,   55,   41,   50,   37,
 /*    80 */    54,   32,   55,   41,    9,   44,   14,   39,   34,   37,
 /*    90 */    54,   32,   55,   41,   39,   34,   37,   54,   32,   55,
 /*   100 */    41,   45,  169,  170,  171,  172,  173,   81,   42,  151,
 /*   110 */    39,   34,   37,   54,   32,   55,   41,   54,   32,   55,
 /*   120 */    41,  136,   79,   39,   34,   37,   54,   32,   55,   41,
 /*   130 */    39,   34,   37,   54,   32,   55,   41,   39,   34,   37,
 /*   140 */    54,   32,   55,   41,   29,   56,    5,  124,  139,   23,
 /*   150 */   163,   20,  155,   39,   34,   37,   54,   32,   55,   41,
 /*   160 */    39,   34,   37,   54,   32,   55,   41,   39,   34,   37,
 /*   170 */    54,   32,   55,   41,   47,   56,    5,  153,  132,  114,
 /*   180 */     7,   19,   39,   34,   37,   54,   32,   55,   41,   39,
 /*   190 */    34,   37,   54,   32,   55,   41,   39,   34,   37,   54,
 /*   200 */    32,   55,   41,  167,   86,   24,   30,   82,  112,  114,
 /*   210 */    36,  157,  152,   26,  168,  101,  110,   39,   34,   37,
 /*   220 */    54,   32,   55,   41,  142,  106,   55,   41,  159,   86,
 /*   230 */    39,   34,   37,   54,   32,   55,   41,   39,   34,   37,
 /*   240 */    54,   32,   55,   41,   52,   64,   98,  128,  165,  108,
 /*   250 */   106,   21,  117,   39,   34,   37,   54,   32,   55,   41,
 /*   260 */    39,   34,   37,   54,   32,   55,   41,   25,  269,    1,
 /*   270 */    10,   82,   39,   34,   37,   54,   32,   55,   41,    4,
 /*   280 */   121,   68,   43,  122,  131,   39,   34,   37,   54,   32,
 /*   290 */    55,   41,   39,   34,   37,   54,   32,   55,   41,   18,
 /*   300 */    94,  130,   78,   27,   57,   35,   17,   60,   39,   34,
 /*   310 */    37,   54,   32,   55,   41,   39,   34,   37,   54,   32,
 /*   320 */    55,   41,   31,   39,   34,   37,   54,   32,   55,   41,
 /*   330 */    39,   34,   37,   54,   32,   55,   41,   33,   59,   30,
 /*   340 */    28,   13,   30,  107,  157,  152,   26,  157,  152,   26,
 /*   350 */    30,  141,  138,   30,  116,  157,  152,   26,  157,  152,
 /*   360 */    26,   30,   12,  125,   30,   84,  157,  152,   26,  157,
 /*   370 */   152,   26,   30,  137,  111,  119,    3,  157,  152,   26,
 /*   380 */    66,   15,  129,  133,   85,  103,   40,   62,   22,   61,
 /*   390 */    93,  135,  161,  158,  134,   87,  118,  143,   95,   38,
 /*   400 */    65,    2,   97,   46,  113,   48,   88,  156,  105,   67,
 /*   410 */   270,   96,    8,   69,   99,   91,  160,  270,  270,   72,
 /*   420 */    90,   74,   77,   76,  270,  270,   75,  270,  270,  270,
 /*   430 */    92,  270,   58,   73,  270,   89,  270,   63,
};
static const YYCODETYPE yy_lookahead[] = {
 /*     0 */     0,    4,   39,   40,    4,   39,    9,   10,   11,    9,
 /*    10 */    10,   11,   39,   13,   14,   39,   19,   39,   18,   43,
 /*    20 */    20,   39,   22,   41,   42,   25,   39,   40,   28,   51,
 /*    30 */    52,   53,   54,   55,   56,   57,   58,   64,   65,   61,
 /*    40 */    29,    1,    2,    3,    4,    5,    6,    7,    1,    2,
 /*    50 */     3,    4,    5,    6,    7,   15,    2,    3,    4,    5,
 /*    60 */     6,    7,   15,   44,    1,    2,    3,    4,    5,    6,
 /*    70 */     7,    1,    2,    3,    4,    5,    6,    7,   15,    3,
 /*    80 */     4,    5,    6,    7,   11,   15,   11,    1,    2,    3,
 /*    90 */     4,    5,    6,    7,    1,    2,    3,    4,    5,    6,
 /*   100 */     7,   15,   30,   31,   32,   33,   34,   39,   15,   21,
 /*   110 */     1,    2,    3,    4,    5,    6,    7,    4,    5,    6,
 /*   120 */     7,   12,   39,    1,    2,    3,    4,    5,    6,    7,
 /*   130 */     1,    2,    3,    4,    5,    6,    7,    1,    2,    3,
 /*   140 */     4,    5,    6,    7,   15,   45,   46,   47,   65,   27,
 /*   150 */    39,   15,   12,    1,    2,    3,    4,    5,    6,    7,
 /*   160 */     1,    2,    3,    4,    5,    6,    7,    1,    2,    3,
 /*   170 */     4,    5,    6,    7,   15,   45,   46,   47,   62,   63,
 /*   180 */    28,   15,    1,    2,    3,    4,    5,    6,    7,    1,
 /*   190 */     2,    3,    4,    5,    6,    7,    1,    2,    3,    4,
 /*   200 */     5,    6,    7,   24,   39,   24,    4,   39,   62,   63,
 /*   210 */    15,    9,   10,   11,   35,   27,   48,    1,    2,    3,
 /*   220 */     4,    5,    6,    7,   59,   60,    6,    7,   12,   39,
 /*   230 */     1,    2,    3,    4,    5,    6,    7,    1,    2,    3,
 /*   240 */     4,    5,    6,    7,   15,   39,   39,   41,   42,   59,
 /*   250 */    60,   15,   19,    1,    2,    3,    4,    5,    6,    7,
 /*   260 */     1,    2,    3,    4,    5,    6,    7,   15,   49,   50,
 /*   270 */    11,   39,    1,    2,    3,    4,    5,    6,    7,   23,
 /*   280 */    48,   39,   15,   12,   12,    1,    2,    3,    4,    5,
 /*   290 */     6,    7,    1,    2,    3,    4,    5,    6,    7,   15,
 /*   300 */    39,   23,   39,   15,   39,   15,   15,   39,    1,    2,
 /*   310 */     3,    4,    5,    6,    7,    1,    2,    3,    4,    5,
 /*   320 */     6,    7,   15,    1,    2,    3,    4,    5,    6,    7,
 /*   330 */     1,    2,    3,    4,    5,    6,    7,   15,   39,    4,
 /*   340 */    24,   11,    4,   29,    9,   10,   11,    9,   10,   11,
 /*   350 */     4,   16,   17,    4,   10,    9,   10,   11,    9,   10,
 /*   360 */    11,    4,   15,   19,    4,   39,    9,   10,   11,    9,
 /*   370 */    10,   11,    4,   26,   36,   37,   11,    9,   10,   11,
 /*   380 */    39,   15,   36,   21,   39,   36,   15,   39,   15,   39,
 /*   390 */    39,   12,   39,   36,   16,   39,   36,   12,   39,   11,
 /*   400 */    39,   15,   39,   11,   36,   15,   39,   12,   19,   39,
 /*   410 */    66,   39,   15,   39,   39,   39,   39,   66,   66,   39,
 /*   420 */    39,   39,   39,   39,   66,   66,   39,   66,   66,   66,
 /*   430 */    39,   66,   39,   39,   66,   39,   66,   39,
};
#define YY_SHIFT_USE_DFLT (-4)
#define YY_SHIFT_MAX 129
static const short yy_shift_ofst[] = {
 /*     0 */    -4,    0,  335,  335,   72,   72,  202,  202,  202,  338,
 /*    10 */   338,  202,  202,  202,  202,  233,  233,  346,  349,  368,
 /*    20 */   357,   -3,  360,  202,  202,  202,  202,  202,  202,  202,
 /*    30 */   202,  202,  202,  202,  202,  202,  202,  202,  202,  202,
 /*    40 */   202,  202,  202,  202,  202,  202,  202,  202,  202,  202,
 /*    50 */   202,  202,  202,  202,  202,  202,  179,  229,   40,   63,
 /*    60 */    70,   86,   93,  109,  122,  129,  136,  322,  314,  307,
 /*    70 */   291,  284,  271,  259,  252,  236,   47,  216,  195,  188,
 /*    80 */   181,  166,  159,  152,  329,  329,  329,  329,  329,  329,
 /*    90 */   329,  329,  329,  329,  329,   54,   76,  113,  220,  220,
 /*   100 */   347,  344,  395,  390,  389,  392,  386,  388,  385,  378,
 /*   110 */   379,  373,  362,  371,  366,  365,  330,  316,  290,  288,
 /*   120 */   278,  272,  256,  140,   88,   75,   73,   11,  397,  267,
};
#define YY_REDUCE_USE_DFLT (-38)
#define YY_REDUCE_MAX 56
static const short yy_reduce_ofst[] = {
 /*     0 */   219,  -22,  165,  190,  100,  130,  -27,  -18,  206,  -37,
 /*    10 */   -13,  -24,   83,  168,  232,  116,  146,   68,  -34,  341,
 /*    20 */   396,  394,  393,  391,  387,  384,  383,  382,  381,  380,
 /*    30 */   377,  376,  375,  374,  372,  370,  367,  363,  361,  359,
 /*    40 */   356,  353,  351,  350,  348,  345,  398,  326,  299,  268,
 /*    50 */   265,  263,  261,  242,  207,  111,   19,
};
static const YYACTIONTYPE yy_default[] = {
 /*     0 */   186,  268,  197,  197,  223,  223,  268,  268,  268,  238,
 /*    10 */   238,  268,  268,  217,  217,  207,  207,  268,  268,  268,
 /*    20 */   268,  268,  268,  268,  268,  268,  268,  268,  268,  268,
 /*    30 */   268,  268,  268,  268,  268,  268,  268,  268,  268,  268,
 /*    40 */   268,  268,  268,  268,  268,  268,  268,  268,  268,  268,
 /*    50 */   268,  268,  268,  268,  268,  268,  268,  260,  240,  259,
 /*    60 */   241,  263,  242,  268,  268,  268,  256,  245,  268,  246,
 /*    70 */   254,  253,  268,  268,  249,  268,  250,  268,  251,  268,
 /*    80 */   268,  255,  218,  233,  219,  264,  201,  266,  252,  257,
 /*    90 */   211,  247,  237,  243,  261,  181,  183,  182,  178,  177,
 /*   100 */   268,  268,  268,  258,  268,  268,  198,  268,  268,  268,
 /*   110 */   268,  239,  268,  265,  208,  268,  268,  210,  244,  248,
 /*   120 */   268,  268,  268,  268,  268,  268,  268,  268,  235,  262,
 /*   130 */   206,  215,  209,  205,  204,  216,  203,  212,  202,  214,
 /*   140 */   213,  200,  199,  196,  195,  194,  193,  192,  191,  190,
 /*   150 */   188,  222,  176,  224,  187,  221,  220,  175,  267,  185,
 /*   160 */   184,  180,  234,  179,  189,  236,  225,  231,  232,  226,
 /*   170 */   227,  228,  229,  230,
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
#if YYSTACKDEPTH<=0
  int yystksz;                  /* Current side of the stack */
  yyStackEntry *yystack;        /* The parser's stack */
#else
  yyStackEntry yystack[YYSTACKDEPTH];  /* The parser's stack */
#endif
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
  "RPAREN",        "NOP",           "PRINT",         "COMMA",       
  "STRING",        "ENDL",          "DEFINE",        "SYM",         
  "INCLUDE",       "RBRACE",        "ENUM",          "LBRACE",      
  "EQUALS",        "SPECIAL",       "SEMICOLON",     "COLON",       
  "LBRACKET",      "RBRACKET",      "FLAGS",         "ARG2",        
  "ARG3",          "ARG4",          "ARG5",          "OR_EQUAL",    
  "TAG",           "LINEID",        "error",         "exp",         
  "special_args",  "list_val",      "arg_list",      "boom_args",   
  "boom_op",       "boom_selector",  "boom_line",     "boom_body",   
  "maybe_argcount",  "main",          "translation_unit",  "external_declaration",
  "define_statement",  "include_statement",  "print_statement",  "enum_statement",
  "linetype_declaration",  "boom_declaration",  "special_declaration",  "print_list",  
  "print_item",    "enum_open",     "enum_list",     "single_enum", 
  "special_list",  "special_def", 
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
 /*  13 */ "translation_unit ::= translation_unit external_declaration",
 /*  14 */ "external_declaration ::= define_statement",
 /*  15 */ "external_declaration ::= include_statement",
 /*  16 */ "external_declaration ::= print_statement",
 /*  17 */ "external_declaration ::= enum_statement",
 /*  18 */ "external_declaration ::= linetype_declaration",
 /*  19 */ "external_declaration ::= boom_declaration",
 /*  20 */ "external_declaration ::= special_declaration",
 /*  21 */ "external_declaration ::= NOP",
 /*  22 */ "print_statement ::= PRINT LPAREN print_list RPAREN",
 /*  23 */ "print_list ::=",
 /*  24 */ "print_list ::= print_item",
 /*  25 */ "print_list ::= print_item COMMA print_list",
 /*  26 */ "print_item ::= STRING",
 /*  27 */ "print_item ::= exp",
 /*  28 */ "print_item ::= ENDL",
 /*  29 */ "define_statement ::= DEFINE SYM LPAREN exp RPAREN",
 /*  30 */ "include_statement ::= INCLUDE STRING",
 /*  31 */ "enum_statement ::= enum_open enum_list RBRACE",
 /*  32 */ "enum_open ::= ENUM LBRACE",
 /*  33 */ "enum_list ::=",
 /*  34 */ "enum_list ::= single_enum",
 /*  35 */ "enum_list ::= single_enum COMMA enum_list",
 /*  36 */ "single_enum ::= SYM",
 /*  37 */ "single_enum ::= SYM EQUALS exp",
 /*  38 */ "special_declaration ::= SPECIAL special_list SEMICOLON",
 /*  39 */ "special_list ::= special_def",
 /*  40 */ "special_list ::= special_list COMMA special_def",
 /*  41 */ "special_def ::= exp COLON SYM LPAREN maybe_argcount RPAREN",
 /*  42 */ "special_def ::= exp COLON SYMNUM LPAREN maybe_argcount RPAREN",
 /*  43 */ "maybe_argcount ::=",
 /*  44 */ "maybe_argcount ::= exp",
 /*  45 */ "maybe_argcount ::= exp COMMA exp",
 /*  46 */ "linetype_declaration ::= exp EQUALS exp COMMA exp LPAREN special_args RPAREN",
 /*  47 */ "linetype_declaration ::= exp EQUALS exp COMMA SYM LPAREN special_args RPAREN",
 /*  48 */ "boom_declaration ::= LBRACKET exp RBRACKET LPAREN exp COMMA exp RPAREN LBRACE boom_body RBRACE",
 /*  49 */ "boom_body ::=",
 /*  50 */ "boom_body ::= boom_line boom_body",
 /*  51 */ "boom_line ::= boom_selector boom_op boom_args",
 /*  52 */ "boom_selector ::= FLAGS",
 /*  53 */ "boom_selector ::= ARG2",
 /*  54 */ "boom_selector ::= ARG3",
 /*  55 */ "boom_selector ::= ARG4",
 /*  56 */ "boom_selector ::= ARG5",
 /*  57 */ "boom_op ::= EQUALS",
 /*  58 */ "boom_op ::= OR_EQUAL",
 /*  59 */ "boom_args ::= exp",
 /*  60 */ "boom_args ::= exp LBRACKET arg_list RBRACKET",
 /*  61 */ "arg_list ::= list_val",
 /*  62 */ "arg_list ::= list_val COMMA arg_list",
 /*  63 */ "list_val ::= exp COLON exp",
 /*  64 */ "special_args ::=",
 /*  65 */ "special_args ::= TAG",
 /*  66 */ "special_args ::= TAG COMMA exp",
 /*  67 */ "special_args ::= TAG COMMA exp COMMA exp",
 /*  68 */ "special_args ::= TAG COMMA exp COMMA exp COMMA exp",
 /*  69 */ "special_args ::= TAG COMMA exp COMMA exp COMMA exp COMMA exp",
 /*  70 */ "special_args ::= TAG COMMA TAG",
 /*  71 */ "special_args ::= TAG COMMA TAG COMMA exp",
 /*  72 */ "special_args ::= TAG COMMA TAG COMMA exp COMMA exp",
 /*  73 */ "special_args ::= TAG COMMA TAG COMMA exp COMMA exp COMMA exp",
 /*  74 */ "special_args ::= LINEID",
 /*  75 */ "special_args ::= LINEID COMMA exp",
 /*  76 */ "special_args ::= LINEID COMMA exp COMMA exp",
 /*  77 */ "special_args ::= LINEID COMMA exp COMMA exp COMMA exp",
 /*  78 */ "special_args ::= LINEID COMMA exp COMMA exp COMMA exp COMMA exp",
 /*  79 */ "special_args ::= exp",
 /*  80 */ "special_args ::= exp COMMA exp",
 /*  81 */ "special_args ::= exp COMMA exp COMMA exp",
 /*  82 */ "special_args ::= exp COMMA exp COMMA exp COMMA exp",
 /*  83 */ "special_args ::= exp COMMA exp COMMA exp COMMA exp COMMA exp",
 /*  84 */ "special_args ::= exp COMMA TAG",
 /*  85 */ "special_args ::= exp COMMA TAG COMMA exp",
 /*  86 */ "special_args ::= exp COMMA TAG COMMA exp COMMA exp",
 /*  87 */ "special_args ::= exp COMMA TAG COMMA exp COMMA exp COMMA exp",
 /*  88 */ "special_args ::= exp COMMA exp COMMA TAG",
 /*  89 */ "special_args ::= exp COMMA exp COMMA TAG COMMA exp",
 /*  90 */ "special_args ::= exp COMMA exp COMMA TAG COMMA exp COMMA exp",
 /*  91 */ "special_args ::= exp COMMA exp COMMA exp COMMA TAG",
 /*  92 */ "special_args ::= exp COMMA exp COMMA exp COMMA TAG COMMA exp",
 /*  93 */ "special_args ::= exp COMMA exp COMMA exp COMMA exp COMMA TAG",
};
#endif /* NDEBUG */

#if YYSTACKDEPTH<=0
/*
** Try to increase the size of the parser stack.
*/
static void yyGrowStack(yyParser *p){
  int newSize;
  yyStackEntry *pNew;

  newSize = p->yystksz*2 + 100;
  pNew = realloc(p->yystack, newSize*sizeof(pNew[0]));
  if( pNew ){
    p->yystack = pNew;
    p->yystksz = newSize;
#ifndef NDEBUG
    if( yyTraceFILE ){
      fprintf(yyTraceFILE,"%sStack grows to %d entries!\n",
              yyTracePrompt, p->yystksz);
    }
#endif
  }
}
#endif

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
#if YYSTACKDEPTH<=0
    yyGrowStack(pParser);
#endif
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
#if YYSTACKDEPTH<=0
  free(pParser->yystack);
#endif
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
  assert( iLookAhead!=YYNOCODE );
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
  if( stateno>YY_REDUCE_MAX ||
	  (i = yy_reduce_ofst[stateno])==YY_REDUCE_USE_DFLT ){
	return yy_default[stateno];
  }
  assert( i!=YY_REDUCE_USE_DFLT );
  assert( iLookAhead!=YYNOCODE );
  i += iLookAhead;
  if( i<0 || i>=YY_SZ_ACTTAB || yy_lookahead[i]!=iLookAhead ){
    return yy_default[stateno];
  }else{
	return yy_action[i];
  }
  return yy_action[i];
}

/*
** The following routine is called if the stack overflows.
*/
static void yyStackOverflow(yyParser *yypParser, YYMINORTYPE *yypMinor){
   ParseARG_FETCH;
   yypParser->yyidx--;
#ifndef NDEBUG
   if( yyTraceFILE ){
     fprintf(yyTraceFILE,"%sStack Overflow!\n",yyTracePrompt);
   }
#endif
   while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
   /* Here code is inserted which will execute if the parser
   ** stack every overflows */
   ParseARG_STORE; /* Suppress warning about unused %extra_argument var */
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
#if YYSTACKDEPTH>0
  if( yypParser->yyidx>=YYSTACKDEPTH ){
    yyStackOverflow(yypParser, yypMinor);
    return;
  }
#else
  if( yypParser->yyidx>=yypParser->yystksz ){
    yyGrowStack(yypParser);
    if( yypParser->yyidx>=yypParser->yystksz ){
      yyStackOverflow(yypParser, yypMinor);
      return;
    }
  }
#endif
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
      fprintf(yyTraceFILE," (%d)%s",yypParser->yystack[i].stateno,yyTokenName[yypParser->yystack[i].major]);
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
  { 49, 1 },
  { 39, 1 },
  { 39, 1 },
  { 39, 3 },
  { 39, 3 },
  { 39, 3 },
  { 39, 3 },
  { 39, 3 },
  { 39, 3 },
  { 39, 3 },
  { 39, 2 },
  { 39, 3 },
  { 50, 0 },
  { 50, 2 },
  { 51, 1 },
  { 51, 1 },
  { 51, 1 },
  { 51, 1 },
  { 51, 1 },
  { 51, 1 },
  { 51, 1 },
  { 51, 1 },
  { 54, 4 },
  { 59, 0 },
  { 59, 1 },
  { 59, 3 },
  { 60, 1 },
  { 60, 1 },
  { 60, 1 },
  { 52, 5 },
  { 53, 2 },
  { 55, 3 },
  { 61, 2 },
  { 62, 0 },
  { 62, 1 },
  { 62, 3 },
  { 63, 1 },
  { 63, 3 },
  { 58, 3 },
  { 64, 1 },
  { 64, 3 },
  { 65, 6 },
  { 65, 6 },
  { 48, 0 },
  { 48, 1 },
  { 48, 3 },
  { 56, 8 },
  { 56, 8 },
  { 57, 11 },
  { 47, 0 },
  { 47, 2 },
  { 46, 3 },
  { 45, 1 },
  { 45, 1 },
  { 45, 1 },
  { 45, 1 },
  { 45, 1 },
  { 44, 1 },
  { 44, 1 },
  { 43, 1 },
  { 43, 4 },
  { 42, 1 },
  { 42, 3 },
  { 41, 3 },
  { 40, 0 },
  { 40, 1 },
  { 40, 3 },
  { 40, 5 },
  { 40, 7 },
  { 40, 9 },
  { 40, 3 },
  { 40, 5 },
  { 40, 7 },
  { 40, 9 },
  { 40, 1 },
  { 40, 3 },
  { 40, 5 },
  { 40, 7 },
  { 40, 9 },
  { 40, 1 },
  { 40, 3 },
  { 40, 5 },
  { 40, 7 },
  { 40, 9 },
  { 40, 3 },
  { 40, 5 },
  { 40, 7 },
  { 40, 9 },
  { 40, 5 },
  { 40, 7 },
  { 40, 9 },
  { 40, 7 },
  { 40, 9 },
  { 40, 9 },
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
      case 0: /* main ::= translation_unit */
      case 12: /*translation_unit ::= */
      case 13: /*translation_unit ::= translation_unit external_declaration */
      case 14: /*external_declaration ::= define_statement */
      case 15: /*external_declaration ::= include_statement */
      case 16: /*external_declaration ::= print_statement */
      case 17: /*external_declaration ::= enum_statement */
      case 18: /*external_declaration ::= linetype_declaration */
      case 19: /*external_declaration ::= boom_declaration */
      case 20: /*external_declaration ::= special_declaration */
      case 21: /*external_declaration ::= NOP */
      case 23: /*print_list ::= */
      case 24: /*print_list ::= print_item */
      case 25: /*print_list ::= print_item COMMA print_list */
      case 31: /*enum_statement ::= enum_open enum_list RBRACE */
      case 33: /*enum_list ::= */
      case 34: /*enum_list ::= single_enum */
      case 35: /*enum_list ::= single_enum COMMA enum_list */
      case 38: /*special_declaration ::= SPECIAL special_list SEMICOLON */
      case 39: /*special_list ::= special_def */
      case 40: /*special_list ::= special_list COMMA special_def */
      case 43: /*maybe_argcount ::= */
      case 44: /*maybe_argcount ::= exp */
      case 45: /*maybe_argcount ::= exp COMMA exp */
#line 365 "xlat-parse.y"
{
}
#line 1266 "xlat-parse.c"
        break;
      case 1: /* exp ::= NUM */
#line 367 "xlat-parse.y"
{ yygotominor.yy64 = yymsp[0].minor.yy0.val; }
#line 1271 "xlat-parse.c"
        break;
      case 2: /* exp ::= SYMNUM */
#line 368 "xlat-parse.y"
{ yygotominor.yy64 = yymsp[0].minor.yy0.symval->Value; }
#line 1276 "xlat-parse.c"
        break;
      case 3: /* exp ::= exp PLUS exp */
#line 369 "xlat-parse.y"
{ yygotominor.yy64 = yymsp[-2].minor.yy64 + yymsp[0].minor.yy64; }
#line 1281 "xlat-parse.c"
        break;
      case 4: /* exp ::= exp MINUS exp */
#line 370 "xlat-parse.y"
{ yygotominor.yy64 = yymsp[-2].minor.yy64 - yymsp[0].minor.yy64; }
#line 1286 "xlat-parse.c"
        break;
      case 5: /* exp ::= exp MULTIPLY exp */
#line 371 "xlat-parse.y"
{ yygotominor.yy64 = yymsp[-2].minor.yy64 * yymsp[0].minor.yy64; }
#line 1291 "xlat-parse.c"
        break;
      case 6: /* exp ::= exp DIVIDE exp */
#line 372 "xlat-parse.y"
{ yygotominor.yy64 = yymsp[-2].minor.yy64 / yymsp[0].minor.yy64; }
#line 1296 "xlat-parse.c"
        break;
      case 7: /* exp ::= exp OR exp */
#line 373 "xlat-parse.y"
{ yygotominor.yy64 = yymsp[-2].minor.yy64 | yymsp[0].minor.yy64; }
#line 1301 "xlat-parse.c"
        break;
      case 8: /* exp ::= exp AND exp */
#line 374 "xlat-parse.y"
{ yygotominor.yy64 = yymsp[-2].minor.yy64 & yymsp[0].minor.yy64; }
#line 1306 "xlat-parse.c"
        break;
      case 9: /* exp ::= exp XOR exp */
#line 375 "xlat-parse.y"
{ yygotominor.yy64 = yymsp[-2].minor.yy64 ^ yymsp[0].minor.yy64; }
#line 1311 "xlat-parse.c"
        break;
      case 10: /* exp ::= MINUS exp */
#line 376 "xlat-parse.y"
{ yygotominor.yy64 = -yymsp[0].minor.yy64; }
#line 1316 "xlat-parse.c"
        break;
      case 11: /* exp ::= LPAREN exp RPAREN */
#line 377 "xlat-parse.y"
{ yygotominor.yy64 = yymsp[-1].minor.yy64; }
#line 1321 "xlat-parse.c"
        break;
      case 22: /* print_statement ::= PRINT LPAREN print_list RPAREN */
#line 392 "xlat-parse.y"
{
  printf ("\n");
}
#line 1328 "xlat-parse.c"
        break;
      case 26: /* print_item ::= STRING */
#line 400 "xlat-parse.y"
{ printf ("%s", yymsp[0].minor.yy0.string); }
#line 1333 "xlat-parse.c"
        break;
      case 27: /* print_item ::= exp */
#line 401 "xlat-parse.y"
{ printf ("%d", yymsp[0].minor.yy64); }
#line 1338 "xlat-parse.c"
        break;
      case 28: /* print_item ::= ENDL */
#line 402 "xlat-parse.y"
{ printf ("\n"); }
#line 1343 "xlat-parse.c"
        break;
      case 29: /* define_statement ::= DEFINE SYM LPAREN exp RPAREN */
#line 405 "xlat-parse.y"
{
	AddSym (yymsp[-3].minor.yy0.sym, yymsp[-1].minor.yy64);
}
#line 1350 "xlat-parse.c"
        break;
      case 30: /* include_statement ::= INCLUDE STRING */
#line 410 "xlat-parse.y"
{
	IncludeFile (yymsp[0].minor.yy0.string);
}
#line 1357 "xlat-parse.c"
        break;
      case 32: /* enum_open ::= ENUM LBRACE */
#line 417 "xlat-parse.y"
{
	EnumVal = 0;
}
#line 1364 "xlat-parse.c"
        break;
      case 36: /* single_enum ::= SYM */
#line 426 "xlat-parse.y"
{
	AddSym (yymsp[0].minor.yy0.sym, EnumVal++);
}
#line 1371 "xlat-parse.c"
        break;
      case 37: /* single_enum ::= SYM EQUALS exp */
#line 431 "xlat-parse.y"
{
	AddSym (yymsp[-2].minor.yy0.sym, EnumVal = yymsp[0].minor.yy64);
}
#line 1378 "xlat-parse.c"
        break;
      case 41: /* special_def ::= exp COLON SYM LPAREN maybe_argcount RPAREN */
#line 444 "xlat-parse.y"
{
	AddSym (yymsp[-3].minor.yy0.sym, yymsp[-5].minor.yy64);
}
#line 1385 "xlat-parse.c"
        break;
      case 42: /* special_def ::= exp COLON SYMNUM LPAREN maybe_argcount RPAREN */
#line 448 "xlat-parse.y"
{
	printf ("%s, line %d: %s is already defined\n", SourceName, SourceLine, yymsp[-3].minor.yy0.symval->Sym);
}
#line 1392 "xlat-parse.c"
        break;
      case 46: /* linetype_declaration ::= exp EQUALS exp COMMA exp LPAREN special_args RPAREN */
#line 457 "xlat-parse.y"
{
	Simple[yymsp[-7].minor.yy64].NewSpecial = yymsp[-3].minor.yy64;
	Simple[yymsp[-7].minor.yy64].Flags = yymsp[-5].minor.yy64 | yymsp[-1].minor.yy123.addflags;
	Simple[yymsp[-7].minor.yy64].Args[0] = yymsp[-1].minor.yy123.args[0];
	Simple[yymsp[-7].minor.yy64].Args[1] = yymsp[-1].minor.yy123.args[1];
	Simple[yymsp[-7].minor.yy64].Args[2] = yymsp[-1].minor.yy123.args[2];
	Simple[yymsp[-7].minor.yy64].Args[3] = yymsp[-1].minor.yy123.args[3];
	Simple[yymsp[-7].minor.yy64].Args[4] = yymsp[-1].minor.yy123.args[4];
}
#line 1405 "xlat-parse.c"
        break;
      case 47: /* linetype_declaration ::= exp EQUALS exp COMMA SYM LPAREN special_args RPAREN */
#line 467 "xlat-parse.y"
{
	printf ("%s, line %d: %s is undefined\n", SourceName, SourceLine, yymsp[-3].minor.yy0.sym);
}
#line 1412 "xlat-parse.c"
        break;
      case 48: /* boom_declaration ::= LBRACKET exp RBRACKET LPAREN exp COMMA exp RPAREN LBRACE boom_body RBRACE */
#line 472 "xlat-parse.y"
{
	if (NumBoomish == MAX_BOOMISH)
	{
		MoreLines *probe = yymsp[-1].minor.yy57;

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

		Boomish[NumBoomish].FirstLinetype = yymsp[-6].minor.yy64;
		Boomish[NumBoomish].LastLinetype = yymsp[-4].minor.yy64;
		Boomish[NumBoomish].NewSpecial = yymsp[-9].minor.yy64;
		
		for (i = 0, probe = yymsp[-1].minor.yy57; probe != NULL; i++)
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
#line 1459 "xlat-parse.c"
        break;
      case 49: /* boom_body ::= */
#line 517 "xlat-parse.y"
{
	yygotominor.yy57 = NULL;
}
#line 1466 "xlat-parse.c"
        break;
      case 50: /* boom_body ::= boom_line boom_body */
#line 521 "xlat-parse.y"
{
	yygotominor.yy57 = malloc (sizeof(MoreLines));
	yygotominor.yy57->next = yymsp[0].minor.yy57;
	yygotominor.yy57->arg = yymsp[-1].minor.yy65;
}
#line 1475 "xlat-parse.c"
        break;
      case 51: /* boom_line ::= boom_selector boom_op boom_args */
#line 528 "xlat-parse.y"
{
	yygotominor.yy65.bDefined = 1;
	yygotominor.yy65.bOrExisting = (yymsp[-1].minor.yy64 == OR_EQUAL);
	yygotominor.yy65.bUseConstant = (yymsp[0].minor.yy87.filters == NULL);
	yygotominor.yy65.ArgNum = yymsp[-2].minor.yy64;
	yygotominor.yy65.ConstantValue = yymsp[0].minor.yy87.constant;
	yygotominor.yy65.AndValue = yymsp[0].minor.yy87.mask;

	if (yymsp[0].minor.yy87.filters != NULL)
	{
		int i;
		MoreFilters *probe;

		for (i = 0, probe = yymsp[0].minor.yy87.filters; probe != NULL; i++)
		{
			MoreFilters *next = probe->next;
			if (i < 15)
			{
				yygotominor.yy65.ResultFilter[i] = probe->filter.filter;
				yygotominor.yy65.ResultValue[i] = probe->filter.value;
			}
			else if (i == 15)
			{
				yyerror ("Lists can only have 15 elements");
			}
			free (probe);
			probe = next;
		}
		yygotominor.yy65.ListSize = i > 15 ? 15 : i;
	}
}
#line 1510 "xlat-parse.c"
        break;
      case 52: /* boom_selector ::= FLAGS */
#line 560 "xlat-parse.y"
{ yygotominor.yy64 = 4; }
#line 1515 "xlat-parse.c"
        break;
      case 53: /* boom_selector ::= ARG2 */
#line 561 "xlat-parse.y"
{ yygotominor.yy64 = 0; }
#line 1520 "xlat-parse.c"
        break;
      case 54: /* boom_selector ::= ARG3 */
#line 562 "xlat-parse.y"
{ yygotominor.yy64 = 1; }
#line 1525 "xlat-parse.c"
        break;
      case 55: /* boom_selector ::= ARG4 */
#line 563 "xlat-parse.y"
{ yygotominor.yy64 = 2; }
#line 1530 "xlat-parse.c"
        break;
      case 56: /* boom_selector ::= ARG5 */
#line 564 "xlat-parse.y"
{ yygotominor.yy64 = 3; }
#line 1535 "xlat-parse.c"
        break;
      case 57: /* boom_op ::= EQUALS */
#line 566 "xlat-parse.y"
{ yygotominor.yy64 = '='; }
#line 1540 "xlat-parse.c"
        break;
      case 58: /* boom_op ::= OR_EQUAL */
#line 567 "xlat-parse.y"
{ yygotominor.yy64 = OR_EQUAL; }
#line 1545 "xlat-parse.c"
        break;
      case 59: /* boom_args ::= exp */
#line 570 "xlat-parse.y"
{
	yygotominor.yy87.constant = yymsp[0].minor.yy64;
	yygotominor.yy87.filters = NULL;
}
#line 1553 "xlat-parse.c"
        break;
      case 60: /* boom_args ::= exp LBRACKET arg_list RBRACKET */
#line 575 "xlat-parse.y"
{
	yygotominor.yy87.mask = yymsp[-3].minor.yy64;
	yygotominor.yy87.filters = yymsp[-1].minor.yy39;
}
#line 1561 "xlat-parse.c"
        break;
      case 61: /* arg_list ::= list_val */
#line 581 "xlat-parse.y"
{
	yygotominor.yy39 = malloc (sizeof(MoreFilters));
	yygotominor.yy39->next = NULL;
	yygotominor.yy39->filter = yymsp[0].minor.yy20;
}
#line 1570 "xlat-parse.c"
        break;
      case 62: /* arg_list ::= list_val COMMA arg_list */
#line 587 "xlat-parse.y"
{
	yygotominor.yy39 = malloc (sizeof(MoreFilters));
	yygotominor.yy39->next = yymsp[0].minor.yy39;
	yygotominor.yy39->filter = yymsp[-2].minor.yy20;
}
#line 1579 "xlat-parse.c"
        break;
      case 63: /* list_val ::= exp COLON exp */
#line 594 "xlat-parse.y"
{
	yygotominor.yy20.filter = yymsp[-2].minor.yy64;
	yygotominor.yy20.value = yymsp[0].minor.yy64;
}
#line 1587 "xlat-parse.c"
        break;
      case 64: /* special_args ::= */
#line 600 "xlat-parse.y"
{
	yygotominor.yy123.addflags = 0;
	memset (yygotominor.yy123.args, 0, 5);
}
#line 1595 "xlat-parse.c"
        break;
      case 65: /* special_args ::= TAG */
#line 605 "xlat-parse.y"
{
	yygotominor.yy123.addflags = SIMPLE_HASTAGAT1;
	memset (yygotominor.yy123.args, 0, 5);
}
#line 1603 "xlat-parse.c"
        break;
      case 66: /* special_args ::= TAG COMMA exp */
#line 610 "xlat-parse.y"
{
	yygotominor.yy123.addflags = SIMPLE_HASTAGAT1;
	yygotominor.yy123.args[0] = 0;
	yygotominor.yy123.args[1] = yymsp[0].minor.yy64;
	yygotominor.yy123.args[2] = 0;
	yygotominor.yy123.args[3] = 0;
	yygotominor.yy123.args[4] = 0;
}
#line 1615 "xlat-parse.c"
        break;
      case 67: /* special_args ::= TAG COMMA exp COMMA exp */
#line 619 "xlat-parse.y"
{
	yygotominor.yy123.addflags = SIMPLE_HASTAGAT1;
	yygotominor.yy123.args[0] = 0;
	yygotominor.yy123.args[1] = yymsp[-2].minor.yy64;
	yygotominor.yy123.args[2] = yymsp[0].minor.yy64;
	yygotominor.yy123.args[3] = 0;
	yygotominor.yy123.args[4] = 0;
}
#line 1627 "xlat-parse.c"
        break;
      case 68: /* special_args ::= TAG COMMA exp COMMA exp COMMA exp */
#line 628 "xlat-parse.y"
{
	yygotominor.yy123.addflags = SIMPLE_HASTAGAT1;
	yygotominor.yy123.args[0] = 0;
	yygotominor.yy123.args[1] = yymsp[-4].minor.yy64;
	yygotominor.yy123.args[2] = yymsp[-2].minor.yy64;
	yygotominor.yy123.args[3] = yymsp[0].minor.yy64;
	yygotominor.yy123.args[4] = 0;
}
#line 1639 "xlat-parse.c"
        break;
      case 69: /* special_args ::= TAG COMMA exp COMMA exp COMMA exp COMMA exp */
#line 637 "xlat-parse.y"
{
	yygotominor.yy123.addflags = SIMPLE_HASTAGAT1;
	yygotominor.yy123.args[0] = 0;
	yygotominor.yy123.args[1] = yymsp[-6].minor.yy64;
	yygotominor.yy123.args[2] = yymsp[-4].minor.yy64;
	yygotominor.yy123.args[3] = yymsp[-2].minor.yy64;
	yygotominor.yy123.args[4] = yymsp[0].minor.yy64;
}
#line 1651 "xlat-parse.c"
        break;
      case 70: /* special_args ::= TAG COMMA TAG */
#line 646 "xlat-parse.y"
{
	yygotominor.yy123.addflags = SIMPLE_HAS2TAGS;
	yygotominor.yy123.args[0] = yygotominor.yy123.args[1] = 0;
	yygotominor.yy123.args[2] = 0;
	yygotominor.yy123.args[3] = 0;
	yygotominor.yy123.args[4] = 0;
}
#line 1662 "xlat-parse.c"
        break;
      case 71: /* special_args ::= TAG COMMA TAG COMMA exp */
#line 654 "xlat-parse.y"
{
	yygotominor.yy123.addflags = SIMPLE_HAS2TAGS;
	yygotominor.yy123.args[0] = yygotominor.yy123.args[1] = 0;
	yygotominor.yy123.args[2] = yymsp[0].minor.yy64;
	yygotominor.yy123.args[3] = 0;
	yygotominor.yy123.args[4] = 0;
}
#line 1673 "xlat-parse.c"
        break;
      case 72: /* special_args ::= TAG COMMA TAG COMMA exp COMMA exp */
#line 662 "xlat-parse.y"
{
	yygotominor.yy123.addflags = SIMPLE_HAS2TAGS;
	yygotominor.yy123.args[0] = yygotominor.yy123.args[1] = 0;
	yygotominor.yy123.args[2] = yymsp[-2].minor.yy64;
	yygotominor.yy123.args[3] = yymsp[0].minor.yy64;
	yygotominor.yy123.args[4] = 0;
}
#line 1684 "xlat-parse.c"
        break;
      case 73: /* special_args ::= TAG COMMA TAG COMMA exp COMMA exp COMMA exp */
#line 670 "xlat-parse.y"
{
	yygotominor.yy123.addflags = SIMPLE_HAS2TAGS;
	yygotominor.yy123.args[0] = yygotominor.yy123.args[1] = 0;
	yygotominor.yy123.args[2] = yymsp[-4].minor.yy64;
	yygotominor.yy123.args[3] = yymsp[-2].minor.yy64;
	yygotominor.yy123.args[4] = yymsp[0].minor.yy64;
}
#line 1695 "xlat-parse.c"
        break;
      case 74: /* special_args ::= LINEID */
#line 678 "xlat-parse.y"
{
	yygotominor.yy123.addflags = SIMPLE_HASLINEID;
	memset (yygotominor.yy123.args, 0, 5);
}
#line 1703 "xlat-parse.c"
        break;
      case 75: /* special_args ::= LINEID COMMA exp */
#line 683 "xlat-parse.y"
{
	yygotominor.yy123.addflags = SIMPLE_HASLINEID;
	yygotominor.yy123.args[0] = 0;
	yygotominor.yy123.args[1] = yymsp[0].minor.yy64;
	yygotominor.yy123.args[2] = 0;
	yygotominor.yy123.args[3] = 0;
	yygotominor.yy123.args[4] = 0;
}
#line 1715 "xlat-parse.c"
        break;
      case 76: /* special_args ::= LINEID COMMA exp COMMA exp */
#line 692 "xlat-parse.y"
{
	yygotominor.yy123.addflags = SIMPLE_HASLINEID;
	yygotominor.yy123.args[0] = 0;
	yygotominor.yy123.args[1] = yymsp[-2].minor.yy64;
	yygotominor.yy123.args[2] = yymsp[0].minor.yy64;
	yygotominor.yy123.args[3] = 0;
	yygotominor.yy123.args[4] = 0;
}
#line 1727 "xlat-parse.c"
        break;
      case 77: /* special_args ::= LINEID COMMA exp COMMA exp COMMA exp */
#line 701 "xlat-parse.y"
{
	yygotominor.yy123.addflags = SIMPLE_HASLINEID;
	yygotominor.yy123.args[0] = 0;
	yygotominor.yy123.args[1] = yymsp[-4].minor.yy64;
	yygotominor.yy123.args[2] = yymsp[-2].minor.yy64;
	yygotominor.yy123.args[3] = yymsp[0].minor.yy64;
	yygotominor.yy123.args[4] = 0;
}
#line 1739 "xlat-parse.c"
        break;
      case 78: /* special_args ::= LINEID COMMA exp COMMA exp COMMA exp COMMA exp */
#line 710 "xlat-parse.y"
{
	yygotominor.yy123.addflags = SIMPLE_HASLINEID;
	yygotominor.yy123.args[0] = 0;
	yygotominor.yy123.args[1] = yymsp[-6].minor.yy64;
	yygotominor.yy123.args[2] = yymsp[-4].minor.yy64;
	yygotominor.yy123.args[3] = yymsp[-2].minor.yy64;
	yygotominor.yy123.args[4] = yymsp[0].minor.yy64;
}
#line 1751 "xlat-parse.c"
        break;
      case 79: /* special_args ::= exp */
#line 719 "xlat-parse.y"
{
	yygotominor.yy123.addflags = 0;
	yygotominor.yy123.args[0] = yymsp[0].minor.yy64;
	yygotominor.yy123.args[1] = 0;
	yygotominor.yy123.args[2] = 0;
	yygotominor.yy123.args[3] = 0;
	yygotominor.yy123.args[4] = 0;
}
#line 1763 "xlat-parse.c"
        break;
      case 80: /* special_args ::= exp COMMA exp */
#line 728 "xlat-parse.y"
{
	yygotominor.yy123.addflags = 0;
	yygotominor.yy123.args[0] = yymsp[-2].minor.yy64;
	yygotominor.yy123.args[1] = yymsp[0].minor.yy64;
	yygotominor.yy123.args[2] = 0;
	yygotominor.yy123.args[3] = 0;
	yygotominor.yy123.args[4] = 0;
}
#line 1775 "xlat-parse.c"
        break;
      case 81: /* special_args ::= exp COMMA exp COMMA exp */
#line 737 "xlat-parse.y"
{
	yygotominor.yy123.addflags = 0;
	yygotominor.yy123.args[0] = yymsp[-4].minor.yy64;
	yygotominor.yy123.args[1] = yymsp[-2].minor.yy64;
	yygotominor.yy123.args[2] = yymsp[0].minor.yy64;
	yygotominor.yy123.args[3] = 0;
	yygotominor.yy123.args[4] = 0;
}
#line 1787 "xlat-parse.c"
        break;
      case 82: /* special_args ::= exp COMMA exp COMMA exp COMMA exp */
#line 746 "xlat-parse.y"
{
	yygotominor.yy123.addflags = 0;
	yygotominor.yy123.args[0] = yymsp[-6].minor.yy64;
	yygotominor.yy123.args[1] = yymsp[-4].minor.yy64;
	yygotominor.yy123.args[2] = yymsp[-2].minor.yy64;
	yygotominor.yy123.args[3] = yymsp[0].minor.yy64;
	yygotominor.yy123.args[4] = 0;
}
#line 1799 "xlat-parse.c"
        break;
      case 83: /* special_args ::= exp COMMA exp COMMA exp COMMA exp COMMA exp */
#line 755 "xlat-parse.y"
{
	yygotominor.yy123.addflags = 0;
	yygotominor.yy123.args[0] = yymsp[-8].minor.yy64;
	yygotominor.yy123.args[1] = yymsp[-6].minor.yy64;
	yygotominor.yy123.args[2] = yymsp[-4].minor.yy64;
	yygotominor.yy123.args[3] = yymsp[-2].minor.yy64;
	yygotominor.yy123.args[4] = yymsp[0].minor.yy64;
}
#line 1811 "xlat-parse.c"
        break;
      case 84: /* special_args ::= exp COMMA TAG */
#line 764 "xlat-parse.y"
{
	yygotominor.yy123.addflags = SIMPLE_HASTAGAT2;
	yygotominor.yy123.args[0] = yymsp[-2].minor.yy64;
	yygotominor.yy123.args[1] = 0;
	yygotominor.yy123.args[2] = 0;
	yygotominor.yy123.args[3] = 0;
	yygotominor.yy123.args[4] = 0;
}
#line 1823 "xlat-parse.c"
        break;
      case 85: /* special_args ::= exp COMMA TAG COMMA exp */
#line 773 "xlat-parse.y"
{
	yygotominor.yy123.addflags = SIMPLE_HASTAGAT2;
	yygotominor.yy123.args[0] = yymsp[-4].minor.yy64;
	yygotominor.yy123.args[1] = 0;
	yygotominor.yy123.args[2] = yymsp[0].minor.yy64;
	yygotominor.yy123.args[3] = 0;
	yygotominor.yy123.args[4] = 0;
}
#line 1835 "xlat-parse.c"
        break;
      case 86: /* special_args ::= exp COMMA TAG COMMA exp COMMA exp */
#line 782 "xlat-parse.y"
{
	yygotominor.yy123.addflags = SIMPLE_HASTAGAT2;
	yygotominor.yy123.args[0] = yymsp[-6].minor.yy64;
	yygotominor.yy123.args[1] = 0;
	yygotominor.yy123.args[2] = yymsp[-2].minor.yy64;
	yygotominor.yy123.args[3] = yymsp[0].minor.yy64;
	yygotominor.yy123.args[4] = 0;
}
#line 1847 "xlat-parse.c"
        break;
      case 87: /* special_args ::= exp COMMA TAG COMMA exp COMMA exp COMMA exp */
#line 791 "xlat-parse.y"
{
	yygotominor.yy123.addflags = SIMPLE_HASTAGAT2;
	yygotominor.yy123.args[0] = yymsp[-8].minor.yy64;
	yygotominor.yy123.args[1] = 0;
	yygotominor.yy123.args[2] = yymsp[-4].minor.yy64;
	yygotominor.yy123.args[3] = yymsp[-2].minor.yy64;
	yygotominor.yy123.args[4] = yymsp[0].minor.yy64;
}
#line 1859 "xlat-parse.c"
        break;
      case 88: /* special_args ::= exp COMMA exp COMMA TAG */
#line 800 "xlat-parse.y"
{
	yygotominor.yy123.addflags = SIMPLE_HASTAGAT3;
	yygotominor.yy123.args[0] = yymsp[-4].minor.yy64;
	yygotominor.yy123.args[1] = yymsp[-2].minor.yy64;
	yygotominor.yy123.args[2] = 0;
	yygotominor.yy123.args[3] = 0;
	yygotominor.yy123.args[4] = 0;
}
#line 1871 "xlat-parse.c"
        break;
      case 89: /* special_args ::= exp COMMA exp COMMA TAG COMMA exp */
#line 809 "xlat-parse.y"
{
	yygotominor.yy123.addflags = SIMPLE_HASTAGAT3;
	yygotominor.yy123.args[0] = yymsp[-6].minor.yy64;
	yygotominor.yy123.args[1] = yymsp[-4].minor.yy64;
	yygotominor.yy123.args[2] = 0;
	yygotominor.yy123.args[3] = yymsp[0].minor.yy64;
	yygotominor.yy123.args[4] = 0;
}
#line 1883 "xlat-parse.c"
        break;
      case 90: /* special_args ::= exp COMMA exp COMMA TAG COMMA exp COMMA exp */
#line 818 "xlat-parse.y"
{
	yygotominor.yy123.addflags = SIMPLE_HASTAGAT3;
	yygotominor.yy123.args[0] = yymsp[-8].minor.yy64;
	yygotominor.yy123.args[1] = yymsp[-6].minor.yy64;
	yygotominor.yy123.args[2] = 0;
	yygotominor.yy123.args[3] = yymsp[-2].minor.yy64;
	yygotominor.yy123.args[4] = yymsp[0].minor.yy64;
}
#line 1895 "xlat-parse.c"
        break;
      case 91: /* special_args ::= exp COMMA exp COMMA exp COMMA TAG */
#line 827 "xlat-parse.y"
{
	yygotominor.yy123.addflags = SIMPLE_HASTAGAT4;
	yygotominor.yy123.args[0] = yymsp[-6].minor.yy64;
	yygotominor.yy123.args[1] = yymsp[-4].minor.yy64;
	yygotominor.yy123.args[2] = yymsp[-2].minor.yy64;
	yygotominor.yy123.args[3] = 0;
	yygotominor.yy123.args[4] = 0;
}
#line 1907 "xlat-parse.c"
        break;
      case 92: /* special_args ::= exp COMMA exp COMMA exp COMMA TAG COMMA exp */
#line 836 "xlat-parse.y"
{
	yygotominor.yy123.addflags = SIMPLE_HASTAGAT4;
	yygotominor.yy123.args[0] = yymsp[-8].minor.yy64;
	yygotominor.yy123.args[1] = yymsp[-6].minor.yy64;
	yygotominor.yy123.args[2] = yymsp[-4].minor.yy64;
	yygotominor.yy123.args[3] = 0;
	yygotominor.yy123.args[4] = yymsp[0].minor.yy64;
}
#line 1919 "xlat-parse.c"
        break;
      case 93: /* special_args ::= exp COMMA exp COMMA exp COMMA exp COMMA TAG */
#line 845 "xlat-parse.y"
{
	yygotominor.yy123.addflags = SIMPLE_HASTAGAT5;
	yygotominor.yy123.args[0] = yymsp[-8].minor.yy64;
	yygotominor.yy123.args[1] = yymsp[-6].minor.yy64;
	yygotominor.yy123.args[2] = yymsp[-4].minor.yy64;
	yygotominor.yy123.args[3] = yymsp[-2].minor.yy64;
	yygotominor.yy123.args[4] = 0;
}
#line 1931 "xlat-parse.c"
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
  }else{
    assert( yyact == YYNSTATE + YYNRULE + 1 );
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
#line 345 "xlat-parse.y"
yyerror("syntax error");
#line 1991 "xlat-parse.c"
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
#ifdef YYERRORSYMBOL
  int yyerrorhit = 0;   /* True if yymajor has invoked an error */
#endif
  yyParser *yypParser;  /* The parser */

  /* (re)initialize the parser, if necessary */
  yypParser = (yyParser*)yyp;
  if( yypParser->yyidx<0 ){
#if YYSTACKDEPTH<=0
    if( yypParser->yystksz <=0 ){
      memset(&yyminorunion, 0, sizeof(yyminorunion));
      yyStackOverflow(yypParser, &yyminorunion);
      return;
    }
#endif
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
      assert( !yyendofinput );  /* Impossible to shift the $ token */
      yy_shift(yypParser,yyact,yymajor,&yyminorunion);
      yypParser->yyerrcnt--;
      yymajor = YYNOCODE;
    }else if( yyact < YYNSTATE + YYNRULE ){
      yy_reduce(yypParser,yyact-YYNSTATE);
    }else{
#ifdef YYERRORSYMBOL
      int yymx;
#endif
      assert( yyact == YY_ERROR_ACTION );
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
    }
  }while( yymajor!=YYNOCODE && yypParser->yyidx>=0 );
  return;
}

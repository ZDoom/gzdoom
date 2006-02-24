/* A Bison parser, made from xlat-parse.y
   by GNU bison 1.35.  */

#define YYBISON 1  /* Identify Bison output.  */

# define	NUM	257
# define	SYMNUM	258
# define	SYM	259
# define	STRING	260
# define	DEFINE	261
# define	INCLUDE	262
# define	TAG	263
# define	LINEID	264
# define	SPECIAL	265
# define	FLAGS	266
# define	ARG2	267
# define	ARG3	268
# define	ARG4	269
# define	ARG5	270
# define	OR_EQUAL	271
# define	ENUM	272
# define	PRINT	273
# define	ENDL	274
# define	NEG	275

#line 7 "xlat-parse.y"

#include "xlat.h"
#include <malloc.h>
#include <string.h>

// verbose doesn't seem to help all that much
//#define YYERROR_VERBOSE 1

int yyerror (char *s);
int yylex (void);

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


#line 51 "xlat-parse.y"
#ifndef YYSTYPE
typedef union {
	int val;
	char sym[80];
	char string[80];
	struct
	{
		BYTE addflags;
		BYTE args[5];
	} args;
	struct ListFilter filter;
	MoreFilters *filter_p;
	struct
	{
		BYTE constant;
		WORD mask;
		MoreFilters *filters;
	} boomarg;
	Symbol *symval;
	BoomArg boomline;
	MoreLines *boomlines;
} yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
#ifndef YYDEBUG
# define YYDEBUG 0
#endif



#define	YYFINAL		175
#define	YYFLAG		-32768
#define	YYNTBASE	39

/* YYTRANSLATE(YYLEX) -- Bison token number corresponding to YYLEX. */
#define YYTRANSLATE(x) ((unsigned)(x) <= 275 ? yytranslate[x] : 66)

/* YYTRANSLATE[YYLEX] -- Bison token number corresponding to YYLEX. */
static const char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    23,     2,
      29,    30,    26,    25,    31,    24,     2,    27,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    36,    35,
       2,    34,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    37,     2,    38,    22,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    32,    21,    33,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    28
};

#if YYDEBUG
static const short yyprhs[] =
{
       0,     0,     2,     4,     8,    12,    16,    20,    24,    28,
      32,    35,    39,    40,    43,    45,    47,    49,    51,    53,
      55,    57,    62,    63,    65,    69,    71,    73,    75,    81,
      84,    85,    91,    92,    94,    98,   100,   104,   108,   109,
     111,   115,   122,   123,   131,   132,   134,   138,   147,   156,
     168,   169,   172,   176,   178,   180,   182,   184,   186,   188,
     190,   192,   197,   199,   203,   207,   208,   210,   214,   220,
     228,   238,   242,   248,   256,   266,   268,   272,   278,   286,
     296,   298,   302,   308,   316,   326,   330,   336,   344,   354,
     360,   368,   378,   386,   396
};
static const short yyrhs[] =
{
       3,     0,     4,     0,    39,    25,    39,     0,    39,    24,
      39,     0,    39,    26,    39,     0,    39,    27,    39,     0,
      39,    21,    39,     0,    39,    23,    39,     0,    39,    22,
      39,     0,    24,    39,     0,    29,    39,    30,     0,     0,
      40,    41,     0,    45,     0,    46,     0,    42,     0,    47,
       0,    56,     0,    57,     0,    51,     0,    19,    29,    43,
      30,     0,     0,    44,     0,    44,    31,    43,     0,     6,
       0,    39,     0,    20,     0,     7,     5,    29,    39,    30,
       0,     8,     6,     0,     0,    18,    32,    48,    49,    33,
       0,     0,    50,     0,    50,    31,    49,     0,     5,     0,
       5,    34,    39,     0,    11,    52,    35,     0,     0,    53,
       0,    53,    31,    52,     0,    39,    36,     5,    29,    55,
      30,     0,     0,    39,    36,     4,    54,    29,    55,    30,
       0,     0,    39,     0,    39,    31,    39,     0,    39,    34,
      39,    31,    39,    29,    65,    30,     0,    39,    34,    39,
      31,     5,    29,    65,    30,     0,    37,    39,    38,    29,
      39,    31,    39,    30,    32,    58,    33,     0,     0,    59,
      58,     0,    60,    61,    62,     0,    12,     0,    13,     0,
      14,     0,    15,     0,    16,     0,    34,     0,    17,     0,
      39,     0,    39,    37,    63,    38,     0,    64,     0,    64,
      31,    63,     0,    39,    36,    39,     0,     0,     9,     0,
       9,    31,    39,     0,     9,    31,    39,    31,    39,     0,
       9,    31,    39,    31,    39,    31,    39,     0,     9,    31,
      39,    31,    39,    31,    39,    31,    39,     0,     9,    31,
       9,     0,     9,    31,     9,    31,    39,     0,     9,    31,
       9,    31,    39,    31,    39,     0,     9,    31,     9,    31,
      39,    31,    39,    31,    39,     0,    10,     0,    10,    31,
      39,     0,    10,    31,    39,    31,    39,     0,    10,    31,
      39,    31,    39,    31,    39,     0,    10,    31,    39,    31,
      39,    31,    39,    31,    39,     0,    39,     0,    39,    31,
      39,     0,    39,    31,    39,    31,    39,     0,    39,    31,
      39,    31,    39,    31,    39,     0,    39,    31,    39,    31,
      39,    31,    39,    31,    39,     0,    39,    31,     9,     0,
      39,    31,     9,    31,    39,     0,    39,    31,     9,    31,
      39,    31,    39,     0,    39,    31,     9,    31,    39,    31,
      39,    31,    39,     0,    39,    31,    39,    31,     9,     0,
      39,    31,    39,    31,     9,    31,    39,     0,    39,    31,
      39,    31,     9,    31,    39,    31,    39,     0,    39,    31,
      39,    31,    39,    31,     9,     0,    39,    31,    39,    31,
      39,    31,     9,    31,    39,     0,    39,    31,    39,    31,
      39,    31,    39,    31,     9,     0
};

#endif

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined. */
static const short yyrline[] =
{
       0,   115,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   129,   130,   133,   135,   136,   137,   138,   139,
     140,   143,   150,   152,   153,   156,   158,   159,   162,   166,
     170,   170,   174,   176,   177,   180,   182,   188,   192,   194,
     195,   198,   200,   200,   205,   207,   208,   211,   222,   228,
     275,   280,   288,   323,   325,   326,   327,   328,   331,   333,
     336,   342,   349,   356,   364,   368,   374,   379,   388,   397,
     406,   416,   424,   432,   440,   449,   454,   463,   472,   481,
     491,   500,   509,   518,   527,   537,   546,   555,   564,   574,
     583,   592,   602,   611,   621
};
#endif


#if (YYDEBUG) || defined YYERROR_VERBOSE

/* YYTNAME[TOKEN_NUM] -- String name of the token TOKEN_NUM. */
static const char *const yytname[] =
{
  "$", "error", "$undefined.", "NUM", "SYMNUM", "SYM", "STRING", "DEFINE", 
  "INCLUDE", "TAG", "LINEID", "SPECIAL", "FLAGS", "ARG2", "ARG3", "ARG4", 
  "ARG5", "OR_EQUAL", "ENUM", "PRINT", "ENDL", "'|'", "'^'", "'&'", "'-'", 
  "'+'", "'*'", "'/'", "NEG", "'('", "')'", "','", "'{'", "'}'", "'='", 
  "';'", "':'", "'['", "']'", "exp", "translation_unit", 
  "external_declaration", "print_statement", "print_list", "print_item", 
  "define_statement", "include_statement", "enum_statement", "@1", 
  "enum_list", "single_enum", "special_declaration", "special_list", 
  "special_def", "@2", "maybe_argcount", "linetype_declaration", 
  "boom_declaration", "boom_body", "boom_line", "boom_selector", 
  "boom_op", "boom_args", "arg_list", "list_val", "special_args", 0
};
#endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives. */
static const short yyr1[] =
{
       0,    39,    39,    39,    39,    39,    39,    39,    39,    39,
      39,    39,    40,    40,    41,    41,    41,    41,    41,    41,
      41,    42,    43,    43,    43,    44,    44,    44,    45,    46,
      48,    47,    49,    49,    49,    50,    50,    51,    52,    52,
      52,    53,    54,    53,    55,    55,    55,    56,    56,    57,
      58,    58,    59,    60,    60,    60,    60,    60,    61,    61,
      62,    62,    63,    63,    64,    65,    65,    65,    65,    65,
      65,    65,    65,    65,    65,    65,    65,    65,    65,    65,
      65,    65,    65,    65,    65,    65,    65,    65,    65,    65,
      65,    65,    65,    65,    65
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN. */
static const short yyr2[] =
{
       0,     1,     1,     3,     3,     3,     3,     3,     3,     3,
       2,     3,     0,     2,     1,     1,     1,     1,     1,     1,
       1,     4,     0,     1,     3,     1,     1,     1,     5,     2,
       0,     5,     0,     1,     3,     1,     3,     3,     0,     1,
       3,     6,     0,     7,     0,     1,     3,     8,     8,    11,
       0,     2,     3,     1,     1,     1,     1,     1,     1,     1,
       1,     4,     1,     3,     3,     0,     1,     3,     5,     7,
       9,     3,     5,     7,     9,     1,     3,     5,     7,     9,
       1,     3,     5,     7,     9,     3,     5,     7,     9,     5,
       7,     9,     7,     9,     9
};

/* YYDEFACT[S] -- default rule to reduce with in state S when YYTABLE
   doesn't specify something else to do.  Zero means the default is an
   error. */
static const short yydefact[] =
{
      12,     0,     1,     2,     0,     0,    38,     0,     0,     0,
       0,     0,     0,    13,    16,    14,    15,    17,    20,    18,
      19,     0,    29,     0,     0,    39,    30,    22,    10,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    37,    38,    32,    25,    27,    26,     0,    23,    11,
       0,     7,     9,     8,     4,     3,     5,     6,     0,     0,
      42,     0,    40,    35,     0,    33,    21,    22,     0,     0,
      28,     0,    44,     0,    31,    32,    24,     0,     0,     0,
      44,    45,     0,    36,    34,     0,    65,    65,     0,     0,
      41,     0,    66,    75,    80,     0,     0,    43,    46,     0,
       0,     0,     0,    48,    47,    50,    71,    67,    76,    85,
      81,    53,    54,    55,    56,    57,     0,    50,     0,     0,
       0,     0,     0,     0,    49,    51,    59,    58,     0,    72,
      68,    77,    86,    89,    82,    60,    52,     0,     0,     0,
       0,     0,     0,     0,    73,    69,    78,    87,    90,    92,
      83,     0,     0,    62,     0,     0,     0,     0,     0,     0,
       0,     0,    61,     0,    74,    70,    79,    88,    91,    93,
      94,    84,    64,    63,     0,     0
};

static const short yydefgoto[] =
{
      23,     1,    13,    14,    47,    48,    15,    16,    17,    43,
      64,    65,    18,    24,    25,    71,    82,    19,    20,   116,
     117,   118,   128,   136,   152,   153,    95
};

static const short yypact[] =
{
  -32768,    40,-32768,-32768,     0,    -3,    17,   -25,    10,    17,
      17,    17,   165,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
  -32768,    13,-32768,   142,   -16,    14,-32768,    -2,-32768,   377,
     -10,    17,    17,    17,    17,    17,    17,    17,    17,    17,
      20,-32768,    17,    24,-32768,-32768,   416,    27,    34,-32768,
      45,   157,   170,    36,    67,    67,-32768,-32768,   179,   387,
  -32768,    47,-32768,    62,    50,    77,-32768,    -2,    17,    49,
  -32768,    51,    17,    17,-32768,    24,-32768,   190,    69,   407,
      17,   201,    83,   416,-32768,    17,    46,    46,    84,    17,
  -32768,   397,    85,    92,   212,    95,    98,-32768,   416,   101,
      78,    17,    86,-32768,-32768,   432,    99,   223,   234,   103,
     245,-32768,-32768,-32768,-32768,-32768,   110,   432,   -11,    17,
      17,    17,    17,    88,-32768,-32768,-32768,-32768,    17,   256,
     267,   278,   289,   104,   300,   124,-32768,    17,    17,    17,
      17,    17,   100,    17,   311,   322,   333,   344,   355,   113,
     366,   149,    94,   121,    17,    17,    17,    17,    17,    17,
     102,    17,-32768,    17,   416,   416,   416,   416,   416,   416,
  -32768,   416,   416,-32768,   177,-32768
};

static const short yypgoto[] =
{
      -1,-32768,-32768,-32768,   131,-32768,-32768,-32768,-32768,-32768,
     132,-32768,-32768,   166,-32768,-32768,   129,-32768,-32768,   112,
  -32768,-32768,-32768,-32768,    55,-32768,   133
};


#define	YYLAST		448


static const short yytable[] =
{
      12,     2,     3,    22,    44,    21,   126,    26,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    45,    41,
       2,     3,     9,   127,    60,    61,    46,    10,    50,    63,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    27,
     174,     9,    39,     2,     3,    42,    10,     4,     5,     2,
       3,     6,     2,     3,    78,    92,    93,    66,     7,     8,
      34,    35,    36,    37,     9,    67,    46,    77,    79,    10,
       9,    81,    83,     9,    68,    10,    72,    11,    10,    81,
      80,     2,     3,    74,    91,    94,    94,   106,    98,     2,
       3,     2,     3,    36,    37,   109,    73,   133,    86,   107,
     108,   110,     9,     2,     3,     2,     3,    10,    75,   149,
       9,   170,     9,    90,    97,    10,   100,    10,   129,   130,
     131,   132,   134,   101,     9,   103,     9,   135,   104,    10,
     119,    10,   162,   105,   122,   141,   144,   145,   146,   147,
     148,   150,   151,   124,   159,    31,    32,    33,    34,    35,
      36,    37,   163,   164,   165,   166,   167,   168,   169,   171,
     172,   143,   151,    31,    32,    33,    34,    35,    36,    37,
      31,    32,    33,    34,    35,    36,    37,   175,    40,    32,
      33,    34,    35,    36,    37,   161,    31,    32,    33,    34,
      35,    36,    37,    33,    34,    35,    36,    37,    76,    38,
      31,    32,    33,    34,    35,    36,    37,    84,    62,    88,
      69,    31,    32,    33,    34,    35,    36,    37,   173,     0,
      96,    85,    31,    32,    33,    34,    35,    36,    37,   125,
       0,     0,    89,    31,    32,    33,    34,    35,    36,    37,
       0,     0,     0,   102,    31,    32,    33,    34,    35,    36,
      37,     0,     0,     0,   120,    31,    32,    33,    34,    35,
      36,    37,     0,     0,     0,   121,    31,    32,    33,    34,
      35,    36,    37,     0,     0,     0,   123,    31,    32,    33,
      34,    35,    36,    37,     0,     0,     0,   137,    31,    32,
      33,    34,    35,    36,    37,     0,     0,     0,   138,    31,
      32,    33,    34,    35,    36,    37,     0,     0,     0,   139,
      31,    32,    33,    34,    35,    36,    37,     0,     0,     0,
     140,    31,    32,    33,    34,    35,    36,    37,     0,     0,
       0,   142,    31,    32,    33,    34,    35,    36,    37,     0,
       0,     0,   154,    31,    32,    33,    34,    35,    36,    37,
       0,     0,     0,   155,    31,    32,    33,    34,    35,    36,
      37,     0,     0,     0,   156,    31,    32,    33,    34,    35,
      36,    37,     0,     0,     0,   157,    31,    32,    33,    34,
      35,    36,    37,     0,     0,     0,   158,    31,    32,    33,
      34,    35,    36,    37,     0,     0,     0,   160,    31,    32,
      33,    34,    35,    36,    37,     0,     0,    49,    31,    32,
      33,    34,    35,    36,    37,     0,     0,    70,    31,    32,
      33,    34,    35,    36,    37,     0,     0,    99,    31,    32,
      33,    34,    35,    36,    37,     0,    87,    31,    32,    33,
      34,    35,    36,    37,   111,   112,   113,   114,   115
};

static const short yycheck[] =
{
       1,     3,     4,     6,     6,     5,    17,    32,     9,    10,
      11,    21,    22,    23,    24,    25,    26,    27,    20,    35,
       3,     4,    24,    34,     4,     5,    27,    29,    38,     5,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    29,
       0,    24,    29,     3,     4,    31,    29,     7,     8,     3,
       4,    11,     3,     4,     5,     9,    10,    30,    18,    19,
      24,    25,    26,    27,    24,    31,    67,    68,    69,    29,
      24,    72,    73,    24,    29,    29,    29,    37,    29,    80,
      29,     3,     4,    33,    85,    86,    87,     9,    89,     3,
       4,     3,     4,    26,    27,     9,    34,     9,    29,   100,
     101,   102,    24,     3,     4,     3,     4,    29,    31,     9,
      24,     9,    24,    30,    30,    29,    31,    29,   119,   120,
     121,   122,   123,    31,    24,    30,    24,   128,    30,    29,
      31,    29,    38,    32,    31,    31,   137,   138,   139,   140,
     141,   142,   143,    33,    31,    21,    22,    23,    24,    25,
      26,    27,    31,   154,   155,   156,   157,   158,   159,   160,
     161,    37,   163,    21,    22,    23,    24,    25,    26,    27,
      21,    22,    23,    24,    25,    26,    27,     0,    36,    22,
      23,    24,    25,    26,    27,    36,    21,    22,    23,    24,
      25,    26,    27,    23,    24,    25,    26,    27,    67,    34,
      21,    22,    23,    24,    25,    26,    27,    75,    42,    80,
      31,    21,    22,    23,    24,    25,    26,    27,   163,    -1,
      87,    31,    21,    22,    23,    24,    25,    26,    27,   117,
      -1,    -1,    31,    21,    22,    23,    24,    25,    26,    27,
      -1,    -1,    -1,    31,    21,    22,    23,    24,    25,    26,
      27,    -1,    -1,    -1,    31,    21,    22,    23,    24,    25,
      26,    27,    -1,    -1,    -1,    31,    21,    22,    23,    24,
      25,    26,    27,    -1,    -1,    -1,    31,    21,    22,    23,
      24,    25,    26,    27,    -1,    -1,    -1,    31,    21,    22,
      23,    24,    25,    26,    27,    -1,    -1,    -1,    31,    21,
      22,    23,    24,    25,    26,    27,    -1,    -1,    -1,    31,
      21,    22,    23,    24,    25,    26,    27,    -1,    -1,    -1,
      31,    21,    22,    23,    24,    25,    26,    27,    -1,    -1,
      -1,    31,    21,    22,    23,    24,    25,    26,    27,    -1,
      -1,    -1,    31,    21,    22,    23,    24,    25,    26,    27,
      -1,    -1,    -1,    31,    21,    22,    23,    24,    25,    26,
      27,    -1,    -1,    -1,    31,    21,    22,    23,    24,    25,
      26,    27,    -1,    -1,    -1,    31,    21,    22,    23,    24,
      25,    26,    27,    -1,    -1,    -1,    31,    21,    22,    23,
      24,    25,    26,    27,    -1,    -1,    -1,    31,    21,    22,
      23,    24,    25,    26,    27,    -1,    -1,    30,    21,    22,
      23,    24,    25,    26,    27,    -1,    -1,    30,    21,    22,
      23,    24,    25,    26,    27,    -1,    -1,    30,    21,    22,
      23,    24,    25,    26,    27,    -1,    29,    21,    22,    23,
      24,    25,    26,    27,    12,    13,    14,    15,    16
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "C:\\UnixUtils\\usr\\local\\wbin\\bison.simple"

/* Skeleton output parser for bison,

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002 Free Software
   Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* This is the parser code that is written into each bison parser when
   the %semantic_parser declaration is not specified in the grammar.
   It was written by Richard Stallman by simplifying the hairy parser
   used when %semantic_parser is specified.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

#if ! defined (yyoverflow) || defined (YYERROR_VERBOSE)

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# if YYSTACK_USE_ALLOCA
#  define YYSTACK_ALLOC alloca
# else
#  ifndef YYSTACK_USE_ALLOCA
#   if defined (alloca) || defined (_ALLOCA_H)
#    define YYSTACK_ALLOC alloca
#   else
#    ifdef __GNUC__
#     define YYSTACK_ALLOC __builtin_alloca
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning. */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
# else
#  if defined (__STDC__) || defined (__cplusplus)
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   define YYSIZE_T size_t
#  endif
#  define YYSTACK_ALLOC malloc
#  define YYSTACK_FREE free
# endif
#endif /* ! defined (yyoverflow) || defined (YYERROR_VERBOSE) */


#if (! defined (yyoverflow) \
     && (! defined (__cplusplus) \
	 || (YYLTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  short yyss;
  YYSTYPE yyvs;
# if YYLSP_NEEDED
  YYLTYPE yyls;
# endif
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAX (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# if YYLSP_NEEDED
#  define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE) + sizeof (YYLTYPE))	\
      + 2 * YYSTACK_GAP_MAX)
# else
#  define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE))				\
      + YYSTACK_GAP_MAX)
# endif

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  register YYSIZE_T yyi;		\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (0)
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAX;	\
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (0)

#endif


#if ! defined (YYSIZE_T) && defined (__SIZE_TYPE__)
# define YYSIZE_T __SIZE_TYPE__
#endif
#if ! defined (YYSIZE_T) && defined (size_t)
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T)
# if defined (__STDC__) || defined (__cplusplus)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# endif
#endif
#if ! defined (YYSIZE_T)
# define YYSIZE_T unsigned int
#endif

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	goto yyacceptlab
#define YYABORT 	goto yyabortlab
#define YYERROR		goto yyerrlab1
/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { 								\
      yyerror ("syntax error: cannot back up");			\
      YYERROR;							\
    }								\
while (0)

#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Compute the default location (before the actions
   are run).

   When YYLLOC_DEFAULT is run, CURRENT is set the location of the
   first token.  By default, to implement support for ranges, extend
   its range to the last symbol.  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)       	\
   Current.last_line   = Rhs[N].last_line;	\
   Current.last_column = Rhs[N].last_column;
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#if YYPURE
# if YYLSP_NEEDED
#  ifdef YYLEX_PARAM
#   define YYLEX		yylex (&yylval, &yylloc, YYLEX_PARAM)
#  else
#   define YYLEX		yylex (&yylval, &yylloc)
#  endif
# else /* !YYLSP_NEEDED */
#  ifdef YYLEX_PARAM
#   define YYLEX		yylex (&yylval, YYLEX_PARAM)
#  else
#   define YYLEX		yylex (&yylval)
#  endif
# endif /* !YYLSP_NEEDED */
#else /* !YYPURE */
# define YYLEX			yylex ()
#endif /* !YYPURE */


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (0)
/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
#endif /* !YYDEBUG */

/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   SIZE_MAX < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#if YYMAXDEPTH == 0
# undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif

#ifdef YYERROR_VERBOSE

# ifndef yystrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
yystrlen (const char *yystr)
#   else
yystrlen (yystr)
     const char *yystr;
#   endif
{
  register const char *yys = yystr;

  while (*yys++ != '\0')
    continue;

  return yys - yystr - 1;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
#   if defined (__STDC__) || defined (__cplusplus)
yystpcpy (char *yydest, const char *yysrc)
#   else
yystpcpy (yydest, yysrc)
     char *yydest;
     const char *yysrc;
#   endif
{
  register char *yyd = yydest;
  register const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif
#endif

#line 315 "C:\\UnixUtils\\usr\\local\\wbin\\bison.simple"


/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
#  define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#  define YYPARSE_PARAM_DECL
# else
#  define YYPARSE_PARAM_ARG YYPARSE_PARAM
#  define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
# endif
#else /* !YYPARSE_PARAM */
# define YYPARSE_PARAM_ARG
# define YYPARSE_PARAM_DECL
#endif /* !YYPARSE_PARAM */

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
# ifdef YYPARSE_PARAM
int yyparse (void *);
# else
int yyparse (void);
# endif
#endif

/* YY_DECL_VARIABLES -- depending whether we use a pure parser,
   variables are global, or local to YYPARSE.  */

#define YY_DECL_NON_LSP_VARIABLES			\
/* The lookahead symbol.  */				\
int yychar;						\
							\
/* The semantic value of the lookahead symbol. */	\
YYSTYPE yylval;						\
							\
/* Number of parse errors so far.  */			\
int yynerrs;

#if YYLSP_NEEDED
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES			\
						\
/* Location data for the lookahead symbol.  */	\
YYLTYPE yylloc;
#else
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES
#endif


/* If nonreentrant, generate the variables here. */

#if !YYPURE
YY_DECL_VARIABLES
#endif  /* !YYPURE */

int
yyparse (YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  /* If reentrant, generate the variables here. */
#if YYPURE
  YY_DECL_VARIABLES
#endif  /* !YYPURE */

  register int yystate;
  register int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Lookahead token as an internal (translated) token number.  */
  int yychar1 = 0;

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack. */
  short	yyssa[YYINITDEPTH];
  short *yyss = yyssa;
  register short *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  register YYSTYPE *yyvsp;

#if YYLSP_NEEDED
  /* The location stack.  */
  YYLTYPE yylsa[YYINITDEPTH];
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;
#endif

#if YYLSP_NEEDED
# define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
# define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  YYSIZE_T yystacksize = YYINITDEPTH;


  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
#if YYLSP_NEEDED
  YYLTYPE yyloc;
#endif

  /* When reducing, the number of symbols on the RHS of the reduced
     rule. */
  int yylen;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;
#if YYLSP_NEEDED
  yylsp = yyls;
#endif
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed. so pushing a state here evens the stacks.
     */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	short *yyss1 = yyss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  */
# if YYLSP_NEEDED
	YYLTYPE *yyls1 = yyls;
	/* This used to be a conditional around just the two extra args,
	   but that might be undefined if yyoverflow is a macro.  */
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yyls1, yysize * sizeof (*yylsp),
		    &yystacksize);
	yyls = yyls1;
# else
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);
# endif
	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyoverflowlab;
# else
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
	goto yyoverflowlab;
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;

      {
	short *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyoverflowlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);
# if YYLSP_NEEDED
	YYSTACK_RELOCATE (yyls);
# endif
# undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
#if YYLSP_NEEDED
      yylsp = yyls + yysize - 1;
#endif

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyssp >= yyss + yystacksize - 1)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)		/* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more */

      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yychar1 = YYTRANSLATE (yychar);

#if YYDEBUG
     /* We have to keep this `#if YYDEBUG', since we use variables
	which are defined only if `YYDEBUG' is set.  */
      if (yydebug)
	{
	  YYFPRINTF (stderr, "Next token is %d (%s",
		     yychar, yytname[yychar1]);
	  /* Give the individual parser a way to print the precise
	     meaning of a token, for further debugging info.  */
# ifdef YYPRINT
	  YYPRINT (stderr, yychar, yylval);
# endif
	  YYFPRINTF (stderr, ")\n");
	}
#endif
    }

  yyn += yychar1;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != yychar1)
    goto yydefault;

  yyn = yytable[yyn];

  /* yyn is what to do for this token type in this state.
     Negative => reduce, -yyn is rule number.
     Positive => shift, yyn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrlab;

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */
  YYDPRINTF ((stderr, "Shifting token %d (%s), ",
	      yychar, yytname[yychar1]));

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#if YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  yystate = yyn;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to the semantic value of
     the lookahead token.  This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

#if YYLSP_NEEDED
  /* Similarly for the default location.  Let the user run additional
     commands if for instance locations are ranges.  */
  yyloc = yylsp[1-yylen];
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
#endif

#if YYDEBUG
  /* We have to keep this `#if YYDEBUG', since we use variables which
     are defined only if `YYDEBUG' is set.  */
  if (yydebug)
    {
      int yyi;

      YYFPRINTF (stderr, "Reducing via rule %d (line %d), ",
		 yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (yyi = yyprhs[yyn]; yyrhs[yyi] > 0; yyi++)
	YYFPRINTF (stderr, "%s ", yytname[yyrhs[yyi]]);
      YYFPRINTF (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif

  switch (yyn) {

case 2:
#line 117 "xlat-parse.y"
{ yyval.val = yyvsp[0].symval->Value; ;
    break;}
case 3:
#line 118 "xlat-parse.y"
{ yyval.val = yyvsp[-2].val + yyvsp[0].val; ;
    break;}
case 4:
#line 119 "xlat-parse.y"
{ yyval.val = yyvsp[-2].val - yyvsp[0].val; ;
    break;}
case 5:
#line 120 "xlat-parse.y"
{ yyval.val = yyvsp[-2].val * yyvsp[0].val; ;
    break;}
case 6:
#line 121 "xlat-parse.y"
{ yyval.val = yyvsp[-2].val / yyvsp[0].val; ;
    break;}
case 7:
#line 122 "xlat-parse.y"
{ yyval.val = yyvsp[-2].val | yyvsp[0].val; ;
    break;}
case 8:
#line 123 "xlat-parse.y"
{ yyval.val = yyvsp[-2].val & yyvsp[0].val; ;
    break;}
case 9:
#line 124 "xlat-parse.y"
{ yyval.val = yyvsp[-2].val ^ yyvsp[0].val; ;
    break;}
case 10:
#line 125 "xlat-parse.y"
{ yyval.val = -yyvsp[0].val ;
    break;}
case 11:
#line 126 "xlat-parse.y"
{ yyval.val = yyvsp[-1].val; ;
    break;}
case 21:
#line 145 "xlat-parse.y"
{
			  printf ("\n");
		  ;
    break;}
case 25:
#line 157 "xlat-parse.y"
{ printf ("%s", yyvsp[0].string); ;
    break;}
case 26:
#line 158 "xlat-parse.y"
{ printf ("%d", yyvsp[0].val); ;
    break;}
case 27:
#line 159 "xlat-parse.y"
{ printf ("\n"); ;
    break;}
case 28:
#line 163 "xlat-parse.y"
{ AddSym (yyvsp[-3].sym, yyvsp[-1].val); ;
    break;}
case 29:
#line 167 "xlat-parse.y"
{ IncludeFile (yyvsp[0].string); ;
    break;}
case 30:
#line 171 "xlat-parse.y"
{ EnumVal = 0; ;
    break;}
case 35:
#line 181 "xlat-parse.y"
{ AddSym (yyvsp[0].sym, EnumVal++); ;
    break;}
case 36:
#line 182 "xlat-parse.y"
{ AddSym (yyvsp[-2].sym, EnumVal = yyvsp[0].val); ;
    break;}
case 41:
#line 199 "xlat-parse.y"
{ AddSym (yyvsp[-3].sym, yyvsp[-5].val); ;
    break;}
case 42:
#line 201 "xlat-parse.y"
{ printf ("%s, line %d: %s is already defined\n", SourceName, SourceLine, yyvsp[0].symval->Sym); ;
    break;}
case 44:
#line 206 "xlat-parse.y"
{ yyval.val = 0; ;
    break;}
case 47:
#line 213 "xlat-parse.y"
{
			  Simple[yyvsp[-7].val].NewSpecial = yyvsp[-3].val;
			  Simple[yyvsp[-7].val].Flags = yyvsp[-5].val | yyvsp[-1].args.addflags;
			  Simple[yyvsp[-7].val].Args[0] = yyvsp[-1].args.args[0];
			  Simple[yyvsp[-7].val].Args[1] = yyvsp[-1].args.args[1];
			  Simple[yyvsp[-7].val].Args[2] = yyvsp[-1].args.args[2];
			  Simple[yyvsp[-7].val].Args[3] = yyvsp[-1].args.args[3];
			  Simple[yyvsp[-7].val].Args[4] = yyvsp[-1].args.args[4];
		  ;
    break;}
case 48:
#line 223 "xlat-parse.y"
{
			printf ("%s, line %d: %s is undefined\n", SourceName, SourceLine, yyvsp[-3].sym);
		;
    break;}
case 49:
#line 230 "xlat-parse.y"
{
			if (NumBoomish == MAX_BOOMISH)
			{
				MoreLines *probe = yyvsp[-1].boomlines;

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

				Boomish[NumBoomish].FirstLinetype = yyvsp[-6].val;
				Boomish[NumBoomish].LastLinetype = yyvsp[-4].val;
				Boomish[NumBoomish].NewSpecial = yyvsp[-9].val;
				
				for (i = 0, probe = yyvsp[-1].boomlines; probe != NULL; i++)
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
		;
    break;}
case 50:
#line 277 "xlat-parse.y"
{
			yyval.boomlines = NULL;
		;
    break;}
case 51:
#line 281 "xlat-parse.y"
{
			yyval.boomlines = malloc (sizeof(MoreLines));
			yyval.boomlines->next = yyvsp[0].boomlines;
			yyval.boomlines->arg = yyvsp[-1].boomline;
		;
    break;}
case 52:
#line 290 "xlat-parse.y"
{
			yyval.boomline.bDefined = 1;
			yyval.boomline.bOrExisting = (yyvsp[-1].val == OR_EQUAL);
			yyval.boomline.bUseConstant = (yyvsp[0].boomarg.filters == NULL);
			yyval.boomline.ArgNum = yyvsp[-2].val;
			yyval.boomline.ConstantValue = yyvsp[0].boomarg.constant;
			yyval.boomline.AndValue = yyvsp[0].boomarg.mask;

			if (yyvsp[0].boomarg.filters != NULL)
			{
				int i;
				MoreFilters *probe;

				for (i = 0, probe = yyvsp[0].boomarg.filters; probe != NULL; i++)
				{
					MoreFilters *next = probe->next;
					if (i < 15)
					{
						yyval.boomline.ResultFilter[i] = probe->filter.filter;
						yyval.boomline.ResultValue[i] = probe->filter.value;
					}
					else if (i == 15)
					{
						yyerror ("Lists can only have 15 elements");
					}
					free (probe);
					probe = next;
				}
				yyval.boomline.ListSize = i > 15 ? 15 : i;
			}
		;
    break;}
case 53:
#line 324 "xlat-parse.y"
{ yyval.val = 4; ;
    break;}
case 54:
#line 325 "xlat-parse.y"
{ yyval.val = 0; ;
    break;}
case 55:
#line 326 "xlat-parse.y"
{ yyval.val = 1; ;
    break;}
case 56:
#line 327 "xlat-parse.y"
{ yyval.val = 2; ;
    break;}
case 57:
#line 328 "xlat-parse.y"
{ yyval.val = 3; ;
    break;}
case 58:
#line 332 "xlat-parse.y"
{ yyval.val = '='; ;
    break;}
case 59:
#line 333 "xlat-parse.y"
{ yyval.val = OR_EQUAL; ;
    break;}
case 60:
#line 338 "xlat-parse.y"
{
			  yyval.boomarg.constant = yyvsp[0].val;
			  yyval.boomarg.filters = NULL;
		  ;
    break;}
case 61:
#line 343 "xlat-parse.y"
{
			yyval.boomarg.mask = yyvsp[-3].val;
			yyval.boomarg.filters = yyvsp[-1].filter_p;
		;
    break;}
case 62:
#line 351 "xlat-parse.y"
{
			  yyval.filter_p = malloc (sizeof (MoreFilters));
			  yyval.filter_p->next = NULL;
			  yyval.filter_p->filter = yyvsp[0].filter;
		  ;
    break;}
case 63:
#line 357 "xlat-parse.y"
{
			yyval.filter_p = malloc (sizeof (MoreFilters));
			yyval.filter_p->next = yyvsp[0].filter_p;
			yyval.filter_p->filter = yyvsp[-2].filter;
		;
    break;}
case 64:
#line 365 "xlat-parse.y"
{ yyval.filter.filter = yyvsp[-2].val; yyval.filter.value = yyvsp[0].val; ;
    break;}
case 65:
#line 370 "xlat-parse.y"
{
			yyval.args.addflags = 0;
			memset (yyval.args.args, 0, 5);
		;
    break;}
case 66:
#line 375 "xlat-parse.y"
{
			yyval.args.addflags = SIMPLE_HASTAGAT1;
			memset (yyval.args.args, 0, 5);
		;
    break;}
case 67:
#line 380 "xlat-parse.y"
{
			yyval.args.addflags = SIMPLE_HASTAGAT1;
			yyval.args.args[0] = 0;
			yyval.args.args[1] = yyvsp[0].val;
			yyval.args.args[2] = 0;
			yyval.args.args[3] = 0;
			yyval.args.args[4] = 0;
		;
    break;}
case 68:
#line 389 "xlat-parse.y"
{
			yyval.args.addflags = SIMPLE_HASTAGAT1;
			yyval.args.args[0] = 0;
			yyval.args.args[1] = yyvsp[-2].val;
			yyval.args.args[2] = yyvsp[0].val;
			yyval.args.args[3] = 0;
			yyval.args.args[4] = 0;
		;
    break;}
case 69:
#line 398 "xlat-parse.y"
{
			yyval.args.addflags = SIMPLE_HASTAGAT1;
			yyval.args.args[0] = 0;
			yyval.args.args[1] = yyvsp[-4].val;
			yyval.args.args[2] = yyvsp[-2].val;
			yyval.args.args[3] = yyvsp[0].val;
			yyval.args.args[4] = 0;
		;
    break;}
case 70:
#line 407 "xlat-parse.y"
{
			yyval.args.addflags = SIMPLE_HASTAGAT1;
			yyval.args.args[0] = 0;
			yyval.args.args[1] = yyvsp[-6].val;
			yyval.args.args[2] = yyvsp[-4].val;
			yyval.args.args[3] = yyvsp[-2].val;
			yyval.args.args[4] = yyvsp[0].val;
		;
    break;}
case 71:
#line 417 "xlat-parse.y"
{
			yyval.args.addflags = SIMPLE_HAS2TAGS;
			yyval.args.args[0] = yyval.args.args[1] = 0;
			yyval.args.args[2] = 0;
			yyval.args.args[3] = 0;
			yyval.args.args[4] = 0;
		;
    break;}
case 72:
#line 425 "xlat-parse.y"
{
			yyval.args.addflags = SIMPLE_HAS2TAGS;
			yyval.args.args[0] = yyval.args.args[1] = 0;
			yyval.args.args[2] = yyvsp[0].val;
			yyval.args.args[3] = 0;
			yyval.args.args[4] = 0;
		;
    break;}
case 73:
#line 433 "xlat-parse.y"
{
			yyval.args.addflags = SIMPLE_HAS2TAGS;
			yyval.args.args[0] = yyval.args.args[1] = 0;
			yyval.args.args[2] = yyvsp[-2].val;
			yyval.args.args[3] = yyvsp[0].val;
			yyval.args.args[4] = 0;
		;
    break;}
case 74:
#line 441 "xlat-parse.y"
{
			yyval.args.addflags = SIMPLE_HAS2TAGS;
			yyval.args.args[0] = yyval.args.args[1] = 0;
			yyval.args.args[2] = yyvsp[-4].val;
			yyval.args.args[3] = yyvsp[-2].val;
			yyval.args.args[4] = yyvsp[0].val;
		;
    break;}
case 75:
#line 450 "xlat-parse.y"
{
			yyval.args.addflags = SIMPLE_HASLINEID;
			memset (yyval.args.args, 0, 5);
		;
    break;}
case 76:
#line 455 "xlat-parse.y"
{
			yyval.args.addflags = SIMPLE_HASLINEID;
			yyval.args.args[0] = 0;
			yyval.args.args[1] = yyvsp[0].val;
			yyval.args.args[2] = 0;
			yyval.args.args[3] = 0;
			yyval.args.args[4] = 0;
		;
    break;}
case 77:
#line 464 "xlat-parse.y"
{
			yyval.args.addflags = SIMPLE_HASLINEID;
			yyval.args.args[0] = 0;
			yyval.args.args[1] = yyvsp[-2].val;
			yyval.args.args[2] = yyvsp[0].val;
			yyval.args.args[3] = 0;
			yyval.args.args[4] = 0;
		;
    break;}
case 78:
#line 473 "xlat-parse.y"
{
			yyval.args.addflags = SIMPLE_HASLINEID;
			yyval.args.args[0] = 0;
			yyval.args.args[1] = yyvsp[-4].val;
			yyval.args.args[2] = yyvsp[-2].val;
			yyval.args.args[3] = yyvsp[0].val;
			yyval.args.args[4] = 0;
		;
    break;}
case 79:
#line 482 "xlat-parse.y"
{
			yyval.args.addflags = SIMPLE_HASLINEID;
			yyval.args.args[0] = 0;
			yyval.args.args[1] = yyvsp[-6].val;
			yyval.args.args[2] = yyvsp[-4].val;
			yyval.args.args[3] = yyvsp[-2].val;
			yyval.args.args[4] = yyvsp[0].val;
		;
    break;}
case 80:
#line 492 "xlat-parse.y"
{
			yyval.args.addflags = 0;
			yyval.args.args[0] = yyvsp[0].val;
			yyval.args.args[1] = 0;
			yyval.args.args[2] = 0;
			yyval.args.args[3] = 0;
			yyval.args.args[4] = 0;
		;
    break;}
case 81:
#line 501 "xlat-parse.y"
{
			yyval.args.addflags = 0;
			yyval.args.args[0] = yyvsp[-2].val;
			yyval.args.args[1] = yyvsp[0].val;
			yyval.args.args[2] = 0;
			yyval.args.args[3] = 0;
			yyval.args.args[4] = 0;
		;
    break;}
case 82:
#line 510 "xlat-parse.y"
{
			yyval.args.addflags = 0;
			yyval.args.args[0] = yyvsp[-4].val;
			yyval.args.args[1] = yyvsp[-2].val;
			yyval.args.args[2] = yyvsp[0].val;
			yyval.args.args[3] = 0;
			yyval.args.args[4] = 0;
		;
    break;}
case 83:
#line 519 "xlat-parse.y"
{
			yyval.args.addflags = 0;
			yyval.args.args[0] = yyvsp[-6].val;
			yyval.args.args[1] = yyvsp[-4].val;
			yyval.args.args[2] = yyvsp[-2].val;
			yyval.args.args[3] = yyvsp[0].val;
			yyval.args.args[4] = 0;
		;
    break;}
case 84:
#line 528 "xlat-parse.y"
{
			yyval.args.addflags = 0;
			yyval.args.args[0] = yyvsp[-8].val;
			yyval.args.args[1] = yyvsp[-6].val;
			yyval.args.args[2] = yyvsp[-4].val;
			yyval.args.args[3] = yyvsp[-2].val;
			yyval.args.args[4] = yyvsp[0].val;
		;
    break;}
case 85:
#line 538 "xlat-parse.y"
{
			yyval.args.addflags = SIMPLE_HASTAGAT2;
			yyval.args.args[0] = yyvsp[-2].val;
			yyval.args.args[1] = 0;
			yyval.args.args[2] = 0;
			yyval.args.args[3] = 0;
			yyval.args.args[4] = 0;
		;
    break;}
case 86:
#line 547 "xlat-parse.y"
{
			yyval.args.addflags = SIMPLE_HASTAGAT2;
			yyval.args.args[0] = yyvsp[-4].val;
			yyval.args.args[1] = 0;
			yyval.args.args[2] = yyvsp[0].val;
			yyval.args.args[3] = 0;
			yyval.args.args[4] = 0;
		;
    break;}
case 87:
#line 556 "xlat-parse.y"
{
			yyval.args.addflags = SIMPLE_HASTAGAT2;
			yyval.args.args[0] = yyvsp[-6].val;
			yyval.args.args[1] = 0;
			yyval.args.args[2] = yyvsp[-2].val;
			yyval.args.args[3] = yyvsp[0].val;
			yyval.args.args[4] = 0;
		;
    break;}
case 88:
#line 565 "xlat-parse.y"
{
			yyval.args.addflags = SIMPLE_HASTAGAT2;
			yyval.args.args[0] = yyvsp[-8].val;
			yyval.args.args[1] = 0;
			yyval.args.args[2] = yyvsp[-4].val;
			yyval.args.args[3] = yyvsp[-2].val;
			yyval.args.args[4] = yyvsp[0].val;
		;
    break;}
case 89:
#line 575 "xlat-parse.y"
{
			yyval.args.addflags = SIMPLE_HASTAGAT3;
			yyval.args.args[0] = yyvsp[-4].val;
			yyval.args.args[1] = yyvsp[-2].val;
			yyval.args.args[2] = 0;
			yyval.args.args[3] = 0;
			yyval.args.args[4] = 0;
		;
    break;}
case 90:
#line 584 "xlat-parse.y"
{
			yyval.args.addflags = SIMPLE_HASTAGAT3;
			yyval.args.args[0] = yyvsp[-6].val;
			yyval.args.args[1] = yyvsp[-4].val;
			yyval.args.args[2] = 0;
			yyval.args.args[3] = yyvsp[0].val;
			yyval.args.args[4] = 0;
		;
    break;}
case 91:
#line 593 "xlat-parse.y"
{
			yyval.args.addflags = SIMPLE_HASTAGAT3;
			yyval.args.args[0] = yyvsp[-8].val;
			yyval.args.args[1] = yyvsp[-6].val;
			yyval.args.args[2] = 0;
			yyval.args.args[3] = yyvsp[-2].val;
			yyval.args.args[4] = yyvsp[0].val;
		;
    break;}
case 92:
#line 603 "xlat-parse.y"
{
			yyval.args.addflags = SIMPLE_HASTAGAT4;
			yyval.args.args[0] = yyvsp[-6].val;
			yyval.args.args[1] = yyvsp[-4].val;
			yyval.args.args[2] = yyvsp[-2].val;
			yyval.args.args[3] = 0;
			yyval.args.args[4] = 0;
		;
    break;}
case 93:
#line 612 "xlat-parse.y"
{
			yyval.args.addflags = SIMPLE_HASTAGAT4;
			yyval.args.args[0] = yyvsp[-8].val;
			yyval.args.args[1] = yyvsp[-6].val;
			yyval.args.args[2] = yyvsp[-4].val;
			yyval.args.args[3] = 0;
			yyval.args.args[4] = yyvsp[0].val;
		;
    break;}
case 94:
#line 622 "xlat-parse.y"
{
			yyval.args.addflags = SIMPLE_HASTAGAT5;
			yyval.args.args[0] = yyvsp[-8].val;
			yyval.args.args[1] = yyvsp[-6].val;
			yyval.args.args[2] = yyvsp[-4].val;
			yyval.args.args[3] = yyvsp[-2].val;
			yyval.args.args[4] = 0;
		;
    break;}
}

#line 705 "C:\\UnixUtils\\usr\\local\\wbin\\bison.simple"


  yyvsp -= yylen;
  yyssp -= yylen;
#if YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG
  if (yydebug)
    {
      short *yyssp1 = yyss - 1;
      YYFPRINTF (stderr, "state stack now");
      while (yyssp1 != yyssp)
	YYFPRINTF (stderr, " %d", *++yyssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;
#if YYLSP_NEEDED
  *++yylsp = yyloc;
#endif

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
	{
	  YYSIZE_T yysize = 0;
	  char *yymsg;
	  int yyx, yycount;

	  yycount = 0;
	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  for (yyx = yyn < 0 ? -yyn : 0;
	       yyx < (int) (sizeof (yytname) / sizeof (char *)); yyx++)
	    if (yycheck[yyx + yyn] == yyx)
	      yysize += yystrlen (yytname[yyx]) + 15, yycount++;
	  yysize += yystrlen ("parse error, unexpected ") + 1;
	  yysize += yystrlen (yytname[YYTRANSLATE (yychar)]);
	  yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg != 0)
	    {
	      char *yyp = yystpcpy (yymsg, "parse error, unexpected ");
	      yyp = yystpcpy (yyp, yytname[YYTRANSLATE (yychar)]);

	      if (yycount < 5)
		{
		  yycount = 0;
		  for (yyx = yyn < 0 ? -yyn : 0;
		       yyx < (int) (sizeof (yytname) / sizeof (char *));
		       yyx++)
		    if (yycheck[yyx + yyn] == yyx)
		      {
			const char *yyq = ! yycount ? ", expecting " : " or ";
			yyp = yystpcpy (yyp, yyq);
			yyp = yystpcpy (yyp, yytname[yyx]);
			yycount++;
		      }
		}
	      yyerror (yymsg);
	      YYSTACK_FREE (yymsg);
	    }
	  else
	    yyerror ("parse error; also virtual memory exhausted");
	}
      else
#endif /* defined (YYERROR_VERBOSE) */
	yyerror ("parse error");
    }
  goto yyerrlab1;


/*--------------------------------------------------.
| yyerrlab1 -- error raised explicitly by an action |
`--------------------------------------------------*/
yyerrlab1:
  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
	YYABORT;
      YYDPRINTF ((stderr, "Discarding token %d (%s).\n",
		  yychar, yytname[yychar1]));
      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */

  yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto yyerrhandle;


/*-------------------------------------------------------------------.
| yyerrdefault -- current state does not do anything special for the |
| error token.                                                       |
`-------------------------------------------------------------------*/
yyerrdefault:
#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */

  /* If its default is to accept any token, ok.  Otherwise pop it.  */
  yyn = yydefact[yystate];
  if (yyn)
    goto yydefault;
#endif


/*---------------------------------------------------------------.
| yyerrpop -- pop the current state because it cannot handle the |
| error token                                                    |
`---------------------------------------------------------------*/
yyerrpop:
  if (yyssp == yyss)
    YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#if YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG
  if (yydebug)
    {
      short *yyssp1 = yyss - 1;
      YYFPRINTF (stderr, "Error: state stack now");
      while (yyssp1 != yyssp)
	YYFPRINTF (stderr, " %d", *++yyssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

/*--------------.
| yyerrhandle.  |
`--------------*/
yyerrhandle:
  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yyerrdefault;

  yyn += YYTERROR;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != YYTERROR)
    goto yyerrdefault;

  yyn = yytable[yyn];
  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrpop;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrpop;

  if (yyn == YYFINAL)
    YYACCEPT;

  YYDPRINTF ((stderr, "Shifting error token, "));

  *++yyvsp = yylval;
#if YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

/*---------------------------------------------.
| yyoverflowab -- parser overflow comes here.  |
`---------------------------------------------*/
yyoverflowlab:
  yyerror ("parser stack overflow");
  yyresult = 2;
  /* Fall through.  */

yyreturn:
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  return yyresult;
}
#line 632 "xlat-parse.y"


#include <ctype.h>
#include <stdio.h>

int yylex (void)
{
	char token[80];
	int toksize;
	int buildup;
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
		buildup = c - '0';
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
						yylval.val = buildup;
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
		yylval.val = buildup;
		return NUM;
	}
	if (isalpha (c))
	{
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
		if (FindSym (token, &yylval.symval))
		{
			return SYMNUM;
		}
		strcpy (yylval.sym, token);
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
			return '/';
		}
	}
	if (c == '"')
	{
		int tokensize = 0;
		while ((c = fgetc (Source)) != '"' && c != EOF)
		{
			yylval.string[tokensize++] = c;
		}
		yylval.string[tokensize] = 0;
		return STRING;
	}
	if (c == '|')
	{
		c = fgetc (Source);
		if (c == '=')
			return OR_EQUAL;
		ungetc (c, Source);
		return '|';
	}
	return c;
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

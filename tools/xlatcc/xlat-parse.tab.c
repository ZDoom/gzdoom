
/*  A Bison parser, made from xlat-parse.y
    by GNU Bison version 1.28  */

#define YYBISON 1  /* Identify Bison output.  */

#define	NUM	257
#define	SYMNUM	258
#define	SYM	259
#define	STRING	260
#define	DEFINE	261
#define	INCLUDE	262
#define	TAG	263
#define	LINEID	264
#define	SPECIAL	265
#define	FLAGS	266
#define	ARG2	267
#define	ARG3	268
#define	ARG4	269
#define	ARG5	270
#define	OR_EQUAL	271
#define	ENUM	272
#define	PRINT	273
#define	ENDL	274
#define	NEG	275

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
} YYSTYPE;
#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		175
#define	YYFLAG		-32768
#define	YYNTBASE	39

#define YYTRANSLATE(x) ((unsigned)(x) <= 275 ? yytranslate[x] : 66)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,    23,     2,    29,
    30,    26,    25,    31,    24,     2,    27,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,    36,    35,     2,
    34,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
    37,     2,    38,    22,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,    32,    21,    33,     2,     2,     2,     2,     2,
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
     2,     2,     2,     2,     2,     1,     3,     4,     5,     6,
     7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
    17,    18,    19,    20,    28
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     2,     4,     8,    12,    16,    20,    24,    28,    32,
    35,    39,    40,    43,    45,    47,    49,    51,    53,    55,
    57,    62,    63,    65,    69,    71,    73,    75,    81,    84,
    85,    91,    92,    94,    98,   100,   104,   108,   109,   111,
   115,   122,   123,   131,   132,   134,   138,   147,   156,   168,
   169,   172,   176,   178,   180,   182,   184,   186,   188,   190,
   192,   197,   199,   203,   207,   208,   210,   214,   220,   228,
   238,   242,   248,   256,   266,   268,   272,   278,   286,   296,
   298,   302,   308,   316,   326,   330,   336,   344,   354,   360,
   368,   378,   386,   396
};

static const short yyrhs[] = {     3,
     0,     4,     0,    39,    25,    39,     0,    39,    24,    39,
     0,    39,    26,    39,     0,    39,    27,    39,     0,    39,
    21,    39,     0,    39,    23,    39,     0,    39,    22,    39,
     0,    24,    39,     0,    29,    39,    30,     0,     0,    40,
    41,     0,    45,     0,    46,     0,    42,     0,    47,     0,
    56,     0,    57,     0,    51,     0,    19,    29,    43,    30,
     0,     0,    44,     0,    44,    31,    43,     0,     6,     0,
    39,     0,    20,     0,     7,     5,    29,    39,    30,     0,
     8,     6,     0,     0,    18,    32,    48,    49,    33,     0,
     0,    50,     0,    50,    31,    49,     0,     5,     0,     5,
    34,    39,     0,    11,    52,    35,     0,     0,    53,     0,
    53,    31,    52,     0,    39,    36,     5,    29,    55,    30,
     0,     0,    39,    36,     4,    54,    29,    55,    30,     0,
     0,    39,     0,    39,    31,    39,     0,    39,    34,    39,
    31,    39,    29,    65,    30,     0,    39,    34,    39,    31,
     5,    29,    65,    30,     0,    37,    39,    38,    29,    39,
    31,    39,    30,    32,    58,    33,     0,     0,    59,    58,
     0,    60,    61,    62,     0,    12,     0,    13,     0,    14,
     0,    15,     0,    16,     0,    34,     0,    17,     0,    39,
     0,    39,    37,    63,    38,     0,    64,     0,    64,    31,
    63,     0,    39,    36,    39,     0,     0,     9,     0,     9,
    31,    39,     0,     9,    31,    39,    31,    39,     0,     9,
    31,    39,    31,    39,    31,    39,     0,     9,    31,    39,
    31,    39,    31,    39,    31,    39,     0,     9,    31,     9,
     0,     9,    31,     9,    31,    39,     0,     9,    31,     9,
    31,    39,    31,    39,     0,     9,    31,     9,    31,    39,
    31,    39,    31,    39,     0,    10,     0,    10,    31,    39,
     0,    10,    31,    39,    31,    39,     0,    10,    31,    39,
    31,    39,    31,    39,     0,    10,    31,    39,    31,    39,
    31,    39,    31,    39,     0,    39,     0,    39,    31,    39,
     0,    39,    31,    39,    31,    39,     0,    39,    31,    39,
    31,    39,    31,    39,     0,    39,    31,    39,    31,    39,
    31,    39,    31,    39,     0,    39,    31,     9,     0,    39,
    31,     9,    31,    39,     0,    39,    31,     9,    31,    39,
    31,    39,     0,    39,    31,     9,    31,    39,    31,    39,
    31,    39,     0,    39,    31,    39,    31,     9,     0,    39,
    31,    39,    31,     9,    31,    39,     0,    39,    31,    39,
    31,     9,    31,    39,    31,    39,     0,    39,    31,    39,
    31,    39,    31,     9,     0,    39,    31,    39,    31,    39,
    31,     9,    31,    39,     0,    39,    31,    39,    31,    39,
    31,    39,    31,     9,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   115,   117,   118,   119,   120,   121,   122,   123,   124,   125,
   126,   129,   130,   133,   135,   136,   137,   138,   139,   140,
   143,   150,   152,   153,   156,   158,   159,   162,   166,   170,
   171,   174,   176,   177,   180,   182,   188,   192,   194,   195,
   198,   200,   202,   205,   207,   208,   211,   222,   228,   275,
   280,   288,   323,   325,   326,   327,   328,   331,   333,   336,
   342,   349,   356,   364,   368,   374,   379,   388,   397,   406,
   416,   424,   432,   440,   449,   454,   463,   472,   481,   491,
   500,   509,   518,   527,   537,   546,   555,   564,   574,   583,
   592,   602,   611,   621
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","NUM","SYMNUM",
"SYM","STRING","DEFINE","INCLUDE","TAG","LINEID","SPECIAL","FLAGS","ARG2","ARG3",
"ARG4","ARG5","OR_EQUAL","ENUM","PRINT","ENDL","'|'","'^'","'&'","'-'","'+'",
"'*'","'/'","NEG","'('","')'","','","'{'","'}'","'='","';'","':'","'['","']'",
"exp","translation_unit","external_declaration","print_statement","print_list",
"print_item","define_statement","include_statement","enum_statement","@1","enum_list",
"single_enum","special_declaration","special_list","special_def","@2","maybe_argcount",
"linetype_declaration","boom_declaration","boom_body","boom_line","boom_selector",
"boom_op","boom_args","arg_list","list_val","special_args", NULL
};
#endif

static const short yyr1[] = {     0,
    39,    39,    39,    39,    39,    39,    39,    39,    39,    39,
    39,    40,    40,    41,    41,    41,    41,    41,    41,    41,
    42,    43,    43,    43,    44,    44,    44,    45,    46,    48,
    47,    49,    49,    49,    50,    50,    51,    52,    52,    52,
    53,    54,    53,    55,    55,    55,    56,    56,    57,    58,
    58,    59,    60,    60,    60,    60,    60,    61,    61,    62,
    62,    63,    63,    64,    65,    65,    65,    65,    65,    65,
    65,    65,    65,    65,    65,    65,    65,    65,    65,    65,
    65,    65,    65,    65,    65,    65,    65,    65,    65,    65,
    65,    65,    65,    65
};

static const short yyr2[] = {     0,
     1,     1,     3,     3,     3,     3,     3,     3,     3,     2,
     3,     0,     2,     1,     1,     1,     1,     1,     1,     1,
     4,     0,     1,     3,     1,     1,     1,     5,     2,     0,
     5,     0,     1,     3,     1,     3,     3,     0,     1,     3,
     6,     0,     7,     0,     1,     3,     8,     8,    11,     0,
     2,     3,     1,     1,     1,     1,     1,     1,     1,     1,
     4,     1,     3,     3,     0,     1,     3,     5,     7,     9,
     3,     5,     7,     9,     1,     3,     5,     7,     9,     1,
     3,     5,     7,     9,     3,     5,     7,     9,     5,     7,
     9,     7,     9,     9
};

static const short yydefact[] = {    12,
     0,     1,     2,     0,     0,    38,     0,     0,     0,     0,
     0,     0,    13,    16,    14,    15,    17,    20,    18,    19,
     0,    29,     0,     0,    39,    30,    22,    10,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    37,    38,    32,    25,    27,    26,     0,    23,    11,     0,
     7,     9,     8,     4,     3,     5,     6,     0,     0,    42,
     0,    40,    35,     0,    33,    21,    22,     0,     0,    28,
     0,    44,     0,    31,    32,    24,     0,     0,     0,    44,
    45,     0,    36,    34,     0,    65,    65,     0,     0,    41,
     0,    66,    75,    80,     0,     0,    43,    46,     0,     0,
     0,     0,    48,    47,    50,    71,    67,    76,    85,    81,
    53,    54,    55,    56,    57,     0,    50,     0,     0,     0,
     0,     0,     0,    49,    51,    59,    58,     0,    72,    68,
    77,    86,    89,    82,    60,    52,     0,     0,     0,     0,
     0,     0,     0,    73,    69,    78,    87,    90,    92,    83,
     0,     0,    62,     0,     0,     0,     0,     0,     0,     0,
     0,    61,     0,    74,    70,    79,    88,    91,    93,    94,
    84,    64,    63,     0,     0
};

static const short yydefgoto[] = {    23,
     1,    13,    14,    47,    48,    15,    16,    17,    43,    64,
    65,    18,    24,    25,    71,    82,    19,    20,   116,   117,
   118,   128,   136,   152,   153,    95
};

static const short yypact[] = {-32768,
    40,-32768,-32768,     0,    -3,    17,   -25,    10,    17,    17,
    17,   165,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
    13,-32768,   142,   -16,    14,-32768,    -2,-32768,   377,   -10,
    17,    17,    17,    17,    17,    17,    17,    17,    17,    20,
-32768,    17,    24,-32768,-32768,   416,    27,    34,-32768,    45,
   157,   170,    36,    67,    67,-32768,-32768,   179,   387,-32768,
    47,-32768,    62,    50,    77,-32768,    -2,    17,    49,-32768,
    51,    17,    17,-32768,    24,-32768,   190,    69,   407,    17,
   201,    83,   416,-32768,    17,    46,    46,    84,    17,-32768,
   397,    85,    92,   212,    95,    98,-32768,   416,   101,    78,
    17,    86,-32768,-32768,   432,    99,   223,   234,   103,   245,
-32768,-32768,-32768,-32768,-32768,   110,   432,   -11,    17,    17,
    17,    17,    88,-32768,-32768,-32768,-32768,    17,   256,   267,
   278,   289,   104,   300,   124,-32768,    17,    17,    17,    17,
    17,   100,    17,   311,   322,   333,   344,   355,   113,   366,
   149,    94,   121,    17,    17,    17,    17,    17,    17,   102,
    17,-32768,    17,   416,   416,   416,   416,   416,   416,-32768,
   416,   416,-32768,   177,-32768
};

static const short yypgoto[] = {    -1,
-32768,-32768,-32768,   131,-32768,-32768,-32768,-32768,-32768,   132,
-32768,-32768,   166,-32768,-32768,   129,-32768,-32768,   112,-32768,
-32768,-32768,-32768,    55,-32768,   133
};


#define	YYLAST		448


static const short yytable[] = {    12,
     2,     3,    22,    44,    21,   126,    26,    28,    29,    30,
    31,    32,    33,    34,    35,    36,    37,    45,    41,     2,
     3,     9,   127,    60,    61,    46,    10,    50,    63,    51,
    52,    53,    54,    55,    56,    57,    58,    59,    27,   174,
     9,    39,     2,     3,    42,    10,     4,     5,     2,     3,
     6,     2,     3,    78,    92,    93,    66,     7,     8,    34,
    35,    36,    37,     9,    67,    46,    77,    79,    10,     9,
    81,    83,     9,    68,    10,    72,    11,    10,    81,    80,
     2,     3,    74,    91,    94,    94,   106,    98,     2,     3,
     2,     3,    36,    37,   109,    73,   133,    86,   107,   108,
   110,     9,     2,     3,     2,     3,    10,    75,   149,     9,
   170,     9,    90,    97,    10,   100,    10,   129,   130,   131,
   132,   134,   101,     9,   103,     9,   135,   104,    10,   119,
    10,   162,   105,   122,   141,   144,   145,   146,   147,   148,
   150,   151,   124,   159,    31,    32,    33,    34,    35,    36,
    37,   163,   164,   165,   166,   167,   168,   169,   171,   172,
   143,   151,    31,    32,    33,    34,    35,    36,    37,    31,
    32,    33,    34,    35,    36,    37,   175,    40,    32,    33,
    34,    35,    36,    37,   161,    31,    32,    33,    34,    35,
    36,    37,    33,    34,    35,    36,    37,    76,    38,    31,
    32,    33,    34,    35,    36,    37,    84,    62,    88,    69,
    31,    32,    33,    34,    35,    36,    37,   173,     0,    96,
    85,    31,    32,    33,    34,    35,    36,    37,   125,     0,
     0,    89,    31,    32,    33,    34,    35,    36,    37,     0,
     0,     0,   102,    31,    32,    33,    34,    35,    36,    37,
     0,     0,     0,   120,    31,    32,    33,    34,    35,    36,
    37,     0,     0,     0,   121,    31,    32,    33,    34,    35,
    36,    37,     0,     0,     0,   123,    31,    32,    33,    34,
    35,    36,    37,     0,     0,     0,   137,    31,    32,    33,
    34,    35,    36,    37,     0,     0,     0,   138,    31,    32,
    33,    34,    35,    36,    37,     0,     0,     0,   139,    31,
    32,    33,    34,    35,    36,    37,     0,     0,     0,   140,
    31,    32,    33,    34,    35,    36,    37,     0,     0,     0,
   142,    31,    32,    33,    34,    35,    36,    37,     0,     0,
     0,   154,    31,    32,    33,    34,    35,    36,    37,     0,
     0,     0,   155,    31,    32,    33,    34,    35,    36,    37,
     0,     0,     0,   156,    31,    32,    33,    34,    35,    36,
    37,     0,     0,     0,   157,    31,    32,    33,    34,    35,
    36,    37,     0,     0,     0,   158,    31,    32,    33,    34,
    35,    36,    37,     0,     0,     0,   160,    31,    32,    33,
    34,    35,    36,    37,     0,     0,    49,    31,    32,    33,
    34,    35,    36,    37,     0,     0,    70,    31,    32,    33,
    34,    35,    36,    37,     0,     0,    99,    31,    32,    33,
    34,    35,    36,    37,     0,    87,    31,    32,    33,    34,
    35,    36,    37,   111,   112,   113,   114,   115
};

static const short yycheck[] = {     1,
     3,     4,     6,     6,     5,    17,    32,     9,    10,    11,
    21,    22,    23,    24,    25,    26,    27,    20,    35,     3,
     4,    24,    34,     4,     5,    27,    29,    38,     5,    31,
    32,    33,    34,    35,    36,    37,    38,    39,    29,     0,
    24,    29,     3,     4,    31,    29,     7,     8,     3,     4,
    11,     3,     4,     5,     9,    10,    30,    18,    19,    24,
    25,    26,    27,    24,    31,    67,    68,    69,    29,    24,
    72,    73,    24,    29,    29,    29,    37,    29,    80,    29,
     3,     4,    33,    85,    86,    87,     9,    89,     3,     4,
     3,     4,    26,    27,     9,    34,     9,    29,   100,   101,
   102,    24,     3,     4,     3,     4,    29,    31,     9,    24,
     9,    24,    30,    30,    29,    31,    29,   119,   120,   121,
   122,   123,    31,    24,    30,    24,   128,    30,    29,    31,
    29,    38,    32,    31,    31,   137,   138,   139,   140,   141,
   142,   143,    33,    31,    21,    22,    23,    24,    25,    26,
    27,    31,   154,   155,   156,   157,   158,   159,   160,   161,
    37,   163,    21,    22,    23,    24,    25,    26,    27,    21,
    22,    23,    24,    25,    26,    27,     0,    36,    22,    23,
    24,    25,    26,    27,    36,    21,    22,    23,    24,    25,
    26,    27,    23,    24,    25,    26,    27,    67,    34,    21,
    22,    23,    24,    25,    26,    27,    75,    42,    80,    31,
    21,    22,    23,    24,    25,    26,    27,   163,    -1,    87,
    31,    21,    22,    23,    24,    25,    26,    27,   117,    -1,
    -1,    31,    21,    22,    23,    24,    25,    26,    27,    -1,
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
    24,    25,    26,    27,    -1,    -1,    30,    21,    22,    23,
    24,    25,    26,    27,    -1,    -1,    30,    21,    22,    23,
    24,    25,    26,    27,    -1,    -1,    30,    21,    22,    23,
    24,    25,    26,    27,    -1,    29,    21,    22,    23,    24,
    25,    26,    27,    12,    13,    14,    15,    16
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/local/share/bison.simple"
/* This file comes from bison-1.28.  */

/* Skeleton output parser for bison,
   Copyright (C) 1984, 1989, 1990 Free Software Foundation, Inc.

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

/* This is the parser code that is written into each bison parser
  when the %semantic_parser declaration is not specified in the grammar.
  It was written by Richard Stallman by simplifying the hairy parser
  used when %semantic_parser is specified.  */

#ifndef YYSTACK_USE_ALLOCA
#ifdef alloca
#define YYSTACK_USE_ALLOCA
#else /* alloca not defined */
#ifdef __GNUC__
#define YYSTACK_USE_ALLOCA
#define alloca __builtin_alloca
#else /* not GNU C.  */
#if (!defined (__STDC__) && defined (sparc)) || defined (__sparc__) || defined (__sparc) || defined (__sgi) || (defined (__sun) && defined (__i386))
#define YYSTACK_USE_ALLOCA
#include <alloca.h>
#else /* not sparc */
/* We think this test detects Watcom and Microsoft C.  */
/* This used to test MSDOS, but that is a bad idea
   since that symbol is in the user namespace.  */
#if (defined (_MSDOS) || defined (_MSDOS_)) && !defined (__TURBOC__)
#if 0 /* No need for malloc.h, which pollutes the namespace;
	 instead, just don't use alloca.  */
#include <malloc.h>
#endif
#else /* not MSDOS, or __TURBOC__ */
#if defined(_AIX)
/* I don't know what this was needed for, but it pollutes the namespace.
   So I turned it off.   rms, 2 May 1997.  */
/* #include <malloc.h>  */
 #pragma alloca
#define YYSTACK_USE_ALLOCA
#else /* not MSDOS, or __TURBOC__, or _AIX */
#if 0
#ifdef __hpux /* haible@ilog.fr says this works for HPUX 9.05 and up,
		 and on HPUX 10.  Eventually we can turn this on.  */
#define YYSTACK_USE_ALLOCA
#define alloca __builtin_alloca
#endif /* __hpux */
#endif
#endif /* not _AIX */
#endif /* not MSDOS, or __TURBOC__ */
#endif /* not sparc */
#endif /* not GNU C */
#endif /* alloca not defined */
#endif /* YYSTACK_USE_ALLOCA not defined */

#ifdef YYSTACK_USE_ALLOCA
#define YYSTACK_ALLOC alloca
#else
#define YYSTACK_ALLOC malloc
#endif

/* Note: there must be only one dollar sign in this file.
   It is replaced by the list of actions, each action
   as one case of the switch.  */

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	goto yyacceptlab
#define YYABORT 	goto yyabortlab
#define YYERROR		goto yyerrlab1
/* Like YYERROR except do call yyerror.
   This remains here temporarily to ease the
   transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(token, value) \
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    { yychar = (token), yylval = (value);			\
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { yyerror ("syntax error: cannot back up"); YYERROR; }	\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

#ifndef YYPURE
#define YYLEX		yylex()
#endif

#ifdef YYPURE
#ifdef YYLSP_NEEDED
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, &yylloc, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval, &yylloc)
#endif
#else /* not YYLSP_NEEDED */
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval)
#endif
#endif /* not YYLSP_NEEDED */
#endif

/* If nonreentrant, generate the variables here */

#ifndef YYPURE

int	yychar;			/*  the lookahead symbol		*/
YYSTYPE	yylval;			/*  the semantic value of the		*/
				/*  lookahead symbol			*/

#ifdef YYLSP_NEEDED
YYLTYPE yylloc;			/*  location data for the lookahead	*/
				/*  symbol				*/
#endif

int yynerrs;			/*  number of parse errors so far       */
#endif  /* not YYPURE */

#if YYDEBUG != 0
int yydebug;			/*  nonzero means print parse trace	*/
/* Since this is uninitialized, it does not stop multiple parsers
   from coexisting.  */
#endif

/*  YYINITDEPTH indicates the initial size of the parser's stacks	*/

#ifndef	YYINITDEPTH
#define YYINITDEPTH 200
#endif

/*  YYMAXDEPTH is the maximum size the stacks can grow to
    (effective only if the built-in stack extension method is used).  */

#if YYMAXDEPTH == 0
#undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
#define YYMAXDEPTH 10000
#endif

/* Define __yy_memcpy.  Note that the size argument
   should be passed with type unsigned int, because that is what the non-GCC
   definitions require.  With GCC, __builtin_memcpy takes an arg
   of type size_t, but it can handle unsigned int.  */

#if __GNUC__ > 1		/* GNU C and GNU C++ define this.  */
#define __yy_memcpy(TO,FROM,COUNT)	__builtin_memcpy(TO,FROM,COUNT)
#else				/* not GNU C or C++ */
#ifndef __cplusplus

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (to, from, count)
     char *to;
     char *from;
     unsigned int count;
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#else /* __cplusplus */

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (char *to, char *from, unsigned int count)
{
  register char *t = to;
  register char *f = from;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#endif
#endif

#line 217 "/usr/local/share/bison.simple"

/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
#ifdef __cplusplus
#define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#else /* not __cplusplus */
#define YYPARSE_PARAM_ARG YYPARSE_PARAM
#define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
#endif /* not __cplusplus */
#else /* not YYPARSE_PARAM */
#define YYPARSE_PARAM_ARG
#define YYPARSE_PARAM_DECL
#endif /* not YYPARSE_PARAM */

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
#ifdef YYPARSE_PARAM
int yyparse (void *);
#else
int yyparse (void);
#endif
#endif

int
yyparse(YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  register int yystate;
  register int yyn;
  register short *yyssp;
  register YYSTYPE *yyvsp;
  int yyerrstatus;	/*  number of tokens to shift before error messages enabled */
  int yychar1 = 0;		/*  lookahead token as an internal (translated) token number */

  short	yyssa[YYINITDEPTH];	/*  the state stack			*/
  YYSTYPE yyvsa[YYINITDEPTH];	/*  the semantic value stack		*/

  short *yyss = yyssa;		/*  refer to the stacks thru separate pointers */
  YYSTYPE *yyvs = yyvsa;	/*  to allow yyoverflow to reallocate them elsewhere */

#ifdef YYLSP_NEEDED
  YYLTYPE yylsa[YYINITDEPTH];	/*  the location stack			*/
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;

#define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
#define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  int yystacksize = YYINITDEPTH;
  int yyfree_stacks = 0;

#ifdef YYPURE
  int yychar;
  YYSTYPE yylval;
  int yynerrs;
#ifdef YYLSP_NEEDED
  YYLTYPE yylloc;
#endif
#endif

  YYSTYPE yyval;		/*  the variable used to return		*/
				/*  semantic values from the action	*/
				/*  routines				*/

  int yylen;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Starting parse\n");
#endif

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss - 1;
  yyvsp = yyvs;
#ifdef YYLSP_NEEDED
  yylsp = yyls;
#endif

/* Push a new state, which is found in  yystate  .  */
/* In all cases, when you get here, the value and location stacks
   have just been pushed. so pushing a state here evens the stacks.  */
yynewstate:

  *++yyssp = yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Give user a chance to reallocate the stack */
      /* Use copies of these so that the &'s don't force the real ones into memory. */
      YYSTYPE *yyvs1 = yyvs;
      short *yyss1 = yyss;
#ifdef YYLSP_NEEDED
      YYLTYPE *yyls1 = yyls;
#endif

      /* Get the current used size of the three stacks, in elements.  */
      int size = yyssp - yyss + 1;

#ifdef yyoverflow
      /* Each stack pointer address is followed by the size of
	 the data in use in that stack, in bytes.  */
#ifdef YYLSP_NEEDED
      /* This used to be a conditional around just the two extra args,
	 but that might be undefined if yyoverflow is a macro.  */
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yyls1, size * sizeof (*yylsp),
		 &yystacksize);
#else
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yystacksize);
#endif

      yyss = yyss1; yyvs = yyvs1;
#ifdef YYLSP_NEEDED
      yyls = yyls1;
#endif
#else /* no yyoverflow */
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
	{
	  yyerror("parser stack overflow");
	  if (yyfree_stacks)
	    {
	      free (yyss);
	      free (yyvs);
#ifdef YYLSP_NEEDED
	      free (yyls);
#endif
	    }
	  return 2;
	}
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;
#ifndef YYSTACK_USE_ALLOCA
      yyfree_stacks = 1;
#endif
      yyss = (short *) YYSTACK_ALLOC (yystacksize * sizeof (*yyssp));
      __yy_memcpy ((char *)yyss, (char *)yyss1,
		   size * (unsigned int) sizeof (*yyssp));
      yyvs = (YYSTYPE *) YYSTACK_ALLOC (yystacksize * sizeof (*yyvsp));
      __yy_memcpy ((char *)yyvs, (char *)yyvs1,
		   size * (unsigned int) sizeof (*yyvsp));
#ifdef YYLSP_NEEDED
      yyls = (YYLTYPE *) YYSTACK_ALLOC (yystacksize * sizeof (*yylsp));
      __yy_memcpy ((char *)yyls, (char *)yyls1,
		   size * (unsigned int) sizeof (*yylsp));
#endif
#endif /* no yyoverflow */

      yyssp = yyss + size - 1;
      yyvsp = yyvs + size - 1;
#ifdef YYLSP_NEEDED
      yylsp = yyls + size - 1;
#endif

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Stack size increased to %d\n", yystacksize);
#endif

      if (yyssp >= yyss + yystacksize - 1)
	YYABORT;
    }

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Entering state %d\n", yystate);
#endif

  goto yybackup;
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
#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Reading a token: ");
#endif
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)		/* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more */

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Now at end of input.\n");
#endif
    }
  else
    {
      yychar1 = YYTRANSLATE(yychar);

#if YYDEBUG != 0
      if (yydebug)
	{
	  fprintf (stderr, "Next token is %d (%s", yychar, yytname[yychar1]);
	  /* Give the individual parser a way to print the precise meaning
	     of a token, for further debugging info.  */
#ifdef YYPRINT
	  YYPRINT (stderr, yychar, yylval);
#endif
	  fprintf (stderr, ")\n");
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

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting token %d (%s), ", yychar, yytname[yychar1]);
#endif

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  /* count tokens shifted since error; after three, turn off error status.  */
  if (yyerrstatus) yyerrstatus--;

  yystate = yyn;
  goto yynewstate;

/* Do the default action for the current state.  */
yydefault:

  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;

/* Do a reduction.  yyn is the number of a rule to reduce with.  */
yyreduce:
  yylen = yyr2[yyn];
  if (yylen > 0)
    yyval = yyvsp[1-yylen]; /* implement default value of the action */

#if YYDEBUG != 0
  if (yydebug)
    {
      int i;

      fprintf (stderr, "Reducing via rule %d (line %d), ",
	       yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (i = yyprhs[yyn]; yyrhs[i] > 0; i++)
	fprintf (stderr, "%s ", yytname[yyrhs[i]]);
      fprintf (stderr, " -> %s\n", yytname[yyr1[yyn]]);
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
   /* the action file gets copied in in place of this dollarsign */
#line 543 "/usr/local/share/bison.simple"

  yyvsp -= yylen;
  yyssp -= yylen;
#ifdef YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;

#ifdef YYLSP_NEEDED
  yylsp++;
  if (yylen == 0)
    {
      yylsp->first_line = yylloc.first_line;
      yylsp->first_column = yylloc.first_column;
      yylsp->last_line = (yylsp-1)->last_line;
      yylsp->last_column = (yylsp-1)->last_column;
      yylsp->text = 0;
    }
  else
    {
      yylsp->last_line = (yylsp+yylen-1)->last_line;
      yylsp->last_column = (yylsp+yylen-1)->last_column;
    }
#endif

  /* Now "shift" the result of the reduction.
     Determine what state that goes to,
     based on the state we popped back to
     and the rule number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;

yyerrlab:   /* here on detecting error */

  if (! yyerrstatus)
    /* If not already recovering from an error, report this error.  */
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
	{
	  int size = 0;
	  char *msg;
	  int x, count;

	  count = 0;
	  /* Start X at -yyn if nec to avoid negative indexes in yycheck.  */
	  for (x = (yyn < 0 ? -yyn : 0);
	       x < (sizeof(yytname) / sizeof(char *)); x++)
	    if (yycheck[x + yyn] == x)
	      size += strlen(yytname[x]) + 15, count++;
	  msg = (char *) malloc(size + 15);
	  if (msg != 0)
	    {
	      strcpy(msg, "parse error");

	      if (count < 5)
		{
		  count = 0;
		  for (x = (yyn < 0 ? -yyn : 0);
		       x < (sizeof(yytname) / sizeof(char *)); x++)
		    if (yycheck[x + yyn] == x)
		      {
			strcat(msg, count == 0 ? ", expecting `" : " or `");
			strcat(msg, yytname[x]);
			strcat(msg, "'");
			count++;
		      }
		}
	      yyerror(msg);
	      free(msg);
	    }
	  else
	    yyerror ("parse error; also virtual memory exceeded");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror("parse error");
    }

  goto yyerrlab1;
yyerrlab1:   /* here on error raised explicitly by an action */

  if (yyerrstatus == 3)
    {
      /* if just tried and failed to reuse lookahead token after an error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
	YYABORT;

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Discarding token %d (%s).\n", yychar, yytname[yychar1]);
#endif

      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token
     after shifting the error token.  */

  yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto yyerrhandle;

yyerrdefault:  /* current state does not do anything special for the error token. */

#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */
  yyn = yydefact[yystate];  /* If its default is to accept any token, ok.  Otherwise pop it.*/
  if (yyn) goto yydefault;
#endif

yyerrpop:   /* pop the current state because it cannot handle the error token */

  if (yyssp == yyss) YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#ifdef YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "Error: state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

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

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting error token, ");
#endif

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;

 yyacceptlab:
  /* YYACCEPT comes here.  */
  if (yyfree_stacks)
    {
      free (yyss);
      free (yyvs);
#ifdef YYLSP_NEEDED
      free (yyls);
#endif
    }
  return 0;

 yyabortlab:
  /* YYABORT comes here.  */
  if (yyfree_stacks)
    {
      free (yyss);
      free (yyvs);
#ifdef YYLSP_NEEDED
      free (yyls);
#endif
    }
  return 1;
}
#line 632 "xlat-parse.y"


#include <ctype.h>
#include <stdio.h>

int yylex (void)
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

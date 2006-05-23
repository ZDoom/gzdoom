
/*  A Bison parser, made from parse.y
    by GNU Bison version 1.28  */

#define YYBISON 1  /* Identify Bison output.  */

#define	NUM	257
#define	SYM	258
#define	STRING	259
#define	PRINT	260
#define	ENDL	261
#define	Actions	262
#define	OrgHeights	263
#define	ActionList	264
#define	CodePConv	265
#define	OrgSprNames	266
#define	StateMap	267
#define	SoundMap	268
#define	InfoNames	269
#define	ThingBits	270
#define	FirstState	271
#define	SpawnState	272
#define	DeathState	273
#define	RenderStyles	274
#define	NEG	275

#line 1 "parse.y"

#include <malloc.h>
#include <stdlib.h>
#include <search.h>
#include <string.h>
#include <stdio.h>

typedef enum { false, true } bool;
typedef short name;

// verbose doesn't seem to help all that much
//#define YYERROR_VERBOSE 1

int yyerror (char *s);
int yylex (void);
int yyparse ();

FILE *Source, *Dest;
int SourceLine;
int ErrorCount;


void WriteWord (int word);
void WriteLabel (char *label);
void WriteWords (int count, short *array);
void WriteBytes (int count, unsigned char *array);

void WriteNameTable ();

void WriteActions ();
void WriteActionMap ();
void WriteHeights ();
void WriteCodePConv ();
void WriteSprites ();
void WriteStates ();
void WriteSounds ();
void WriteInfoNames ();
void WriteThingBits ();
void WriteRenderStyles ();


static bool FindToken (char *tok, int *type);

struct StringList
{
	struct StringList *Next;
	char String[1];
};

struct StringSorter
{
	name OldName;
	struct StringList *Entry;
};

struct StringList *NameList, **NameListLast = &NameList;
int NameCount;

static name AddName (char *name);
static name FindName (char *name);

static void SortNames ();

struct StateMapE
{
	name Name;
	unsigned char State;
	unsigned char Count;
};

struct ThingBitsE
{
	name Name;
	unsigned char BitNum;
	unsigned char FlagNum;
};

struct RenderStylesE
{
	name Name;
	unsigned char StyleNum;
};

static void AddAction (char *name);
static int FindAction (char *name);

name *ActionsList;
unsigned char *HeightsArray;
unsigned char *ActionMap;
unsigned short *CodePMap;
char *SpriteNames;
struct StateMapE *StateMaps;
name *SoundMaps;
name *InfoNamesArray;
struct ThingBitsE *ThingBitsMap;
struct RenderStylesE *RenderStylesMap;

int ActionsListSize, MaxActionsListSize;
int HeightsSize, MaxHeightsSize;
int ActionMapSize, MaxActionMapSize;
int CodePMapSize, MaxCodePMapSize;
int SpriteNamesSize, MaxSpriteNamesSize;
int StateMapsSize, MaxStateMapsSize;
int SoundMapsSize, MaxSoundMapsSize;
int InfoNamesSize, MaxInfoNamesSize;
int ThingBitsMapSize, MaxThingBitsMapSize;
int RenderStylesSize, MaxRenderStylesSize;

static void AddHeight (int h);
static void AddActionMap (char *name);
static void AddCodeP (int codep);
static void AddSpriteName (char *name);
static void AddStateMap (char *name, int type, int count);
static void AddSoundMap (char *sound);
static void AddInfoName (char *sound);
static void AddThingBits (char *name, int bitnum, int flagnum);
static void AddRenderStyle (char *name, int stylenum);


#line 121 "parse.y"
typedef union {
	int val;
	char sym[80];
	char string[80];
} YYSTYPE;
#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		141
#define	YYFLAG		-32768
#define	YYNTBASE	35

#define YYTRANSLATE(x) ((unsigned)(x) <= 275 ? yytranslate[x] : 65)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,    23,     2,    29,
    30,    26,    25,    31,    24,     2,    27,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,    34,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,    22,     2,     2,     2,     2,     2,     2,
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
     0,     2,     6,    10,    14,    18,    22,    26,    30,    33,
    37,    38,    41,    46,    47,    49,    53,    55,    57,    59,
    61,    63,    65,    67,    69,    71,    73,    75,    77,    79,
    81,    87,    88,    90,    94,   100,   101,   103,   107,   113,
   114,   116,   120,   126,   127,   129,   133,   139,   140,   142,
   146,   152,   153,   155,   159,   165,   167,   169,   171,   177,
   178,   180,   184,   190,   191,   193,   197,   203,   204,   206,
   210,   216,   222,   223,   225,   229
};

static const short yyrhs[] = {     3,
     0,    35,    25,    35,     0,    35,    24,    35,     0,    35,
    26,    35,     0,    35,    27,    35,     0,    35,    21,    35,
     0,    35,    23,    35,     0,    35,    22,    35,     0,    24,
    35,     0,    29,    35,    30,     0,     0,    36,    40,     0,
     6,    29,    38,    30,     0,     0,    39,     0,    39,    31,
    38,     0,     5,     0,    35,     0,     7,     0,    37,     0,
    41,     0,    43,     0,    45,     0,    47,     0,    49,     0,
    51,     0,    55,     0,    57,     0,    59,     0,    62,     0,
     8,    32,    42,    33,    34,     0,     0,     4,     0,    42,
    31,     4,     0,     9,    32,    44,    33,    34,     0,     0,
    35,     0,    44,    31,    35,     0,    10,    32,    46,    33,
    34,     0,     0,     4,     0,    46,    31,     4,     0,    11,
    32,    48,    33,    34,     0,     0,    35,     0,    48,    31,
    35,     0,    12,    32,    50,    33,    34,     0,     0,     4,
     0,    50,    31,     4,     0,    13,    32,    52,    33,    34,
     0,     0,    53,     0,    52,    31,    53,     0,     4,    31,
    54,    31,    35,     0,    17,     0,    18,     0,    19,     0,
    14,    32,    56,    33,    34,     0,     0,     5,     0,    56,
    31,     5,     0,    15,    32,    58,    33,    34,     0,     0,
     4,     0,    58,    31,     4,     0,    16,    32,    60,    33,
    34,     0,     0,    61,     0,    60,    31,    61,     0,    35,
    31,    35,    31,     4,     0,    20,    32,    63,    33,    34,
     0,     0,    64,     0,    63,    31,    64,     0,    35,    31,
     4,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   161,   163,   164,   165,   166,   167,   168,   169,   170,   171,
   174,   175,   178,   185,   187,   188,   191,   193,   194,   197,
   199,   200,   201,   202,   203,   204,   205,   206,   207,   208,
   211,   215,   217,   218,   221,   225,   227,   228,   231,   235,
   237,   238,   241,   245,   247,   248,   251,   255,   257,   258,
   261,   265,   267,   268,   271,   275,   277,   278,   281,   285,
   287,   288,   291,   295,   297,   298,   301,   305,   307,   308,
   311,   315,   319,   321,   322,   325
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","NUM","SYM",
"STRING","PRINT","ENDL","Actions","OrgHeights","ActionList","CodePConv","OrgSprNames",
"StateMap","SoundMap","InfoNames","ThingBits","FirstState","SpawnState","DeathState",
"RenderStyles","'|'","'^'","'&'","'-'","'+'","'*'","'/'","NEG","'('","')'","','",
"'{'","'}'","';'","exp","translation_unit","print_statement","print_list","print_item",
"external_declaration","ActionsDef","ActionsList","OrgHeightsDef","OrgHeightsList",
"ActionListDef","ActionListList","CodePConvDef","CodePConvList","OrgSprNamesDef",
"OrgSprNamesList","StateMapDef","StateMapList","StateMapEntry","StateType","SoundMapDef",
"SoundMapList","InfoNamesDef","InfoNamesList","ThingBitsDef","ThingBitsList",
"ThingBitsEntry","RenderStylesDef","RenderStylesList","RenderStylesEntry", NULL
};
#endif

static const short yyr1[] = {     0,
    35,    35,    35,    35,    35,    35,    35,    35,    35,    35,
    36,    36,    37,    38,    38,    38,    39,    39,    39,    40,
    40,    40,    40,    40,    40,    40,    40,    40,    40,    40,
    41,    42,    42,    42,    43,    44,    44,    44,    45,    46,
    46,    46,    47,    48,    48,    48,    49,    50,    50,    50,
    51,    52,    52,    52,    53,    54,    54,    54,    55,    56,
    56,    56,    57,    58,    58,    58,    59,    60,    60,    60,
    61,    62,    63,    63,    63,    64
};

static const short yyr2[] = {     0,
     1,     3,     3,     3,     3,     3,     3,     3,     2,     3,
     0,     2,     4,     0,     1,     3,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     5,     0,     1,     3,     5,     0,     1,     3,     5,     0,
     1,     3,     5,     0,     1,     3,     5,     0,     1,     3,
     5,     0,     1,     3,     5,     1,     1,     1,     5,     0,
     1,     3,     5,     0,     1,     3,     5,     0,     1,     3,
     5,     5,     0,     1,     3,     3
};

static const short yydefact[] = {    11,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,    20,    12,    21,    22,    23,    24,    25,    26,
    27,    28,    29,    30,    14,    32,    36,    40,    44,    48,
    52,    60,    64,    68,    73,     1,    17,    19,     0,     0,
    18,     0,    15,    33,     0,    37,     0,    41,     0,    45,
     0,    49,     0,     0,     0,    53,    61,     0,    65,     0,
     0,     0,    69,     0,     0,    74,     9,     0,     0,     0,
     0,     0,     0,     0,     0,    13,    14,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    10,     6,     8,     7,     3,     2,     4,     5,    16,    34,
    31,    38,    35,    42,    39,    46,    43,    50,    47,    56,
    57,    58,     0,    54,    51,    62,    59,    66,    63,     0,
    70,    67,    76,    75,    72,     0,     0,    55,    71,     0,
     0
};

static const short yydefgoto[] = {    41,
     1,    13,    42,    43,    14,    15,    45,    16,    47,    17,
    49,    18,    51,    19,    53,    20,    55,    56,   123,    21,
    58,    22,    60,    23,    62,    63,    24,    65,    66
};

static const short yypact[] = {-32768,
    50,   -23,    -3,    23,    35,    39,    75,    76,    95,    96,
    97,    98,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,    -2,    77,     1,   127,     1,   128,
   129,    34,   130,     1,     1,-32768,-32768,-32768,     1,     1,
    73,   105,   106,-32768,   -10,    73,    -5,-32768,     7,    73,
    18,-32768,    21,   107,    49,-32768,-32768,    88,-32768,    89,
    -7,    92,-32768,    10,    93,-32768,-32768,    63,     1,     1,
     1,     1,     1,     1,     1,-32768,    -2,   132,   108,     1,
   109,   135,   110,     1,   111,   136,   112,    -8,   129,   113,
   143,   115,   137,   116,     1,     1,   117,   148,     1,   119,
-32768,    79,    87,    91,    65,    65,-32768,-32768,-32768,-32768,
-32768,    73,-32768,-32768,-32768,    73,-32768,-32768,-32768,-32768,
-32768,-32768,   123,-32768,-32768,-32768,-32768,-32768,-32768,    52,
-32768,-32768,-32768,-32768,-32768,     1,   151,    73,-32768,   156,
-32768
};

static const short yypgoto[] = {   -27,
-32768,-32768,    80,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,    69,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,    64,-32768,-32768,    60
};


#define	YYLAST		160


static const short yytable[] = {    46,
    36,    50,    37,    36,    38,    25,    61,    64,   120,   121,
   122,    67,    68,    69,    70,    71,    72,    73,    74,    75,
    78,    39,    79,    95,    39,    80,    40,    81,    26,    40,
    69,    70,    71,    72,    73,    74,    75,    82,    57,    83,
    98,   102,   103,   104,   105,   106,   107,   108,    84,   140,
    85,    86,   112,    87,    27,     2,   116,     3,     4,     5,
     6,     7,     8,     9,    10,    11,    28,   130,    61,    12,
    29,    64,    69,    70,    71,    72,    73,    74,    75,    89,
    44,    90,   137,    69,    70,    71,    72,    73,    74,    75,
    74,    75,   101,    69,    70,    71,    72,    73,    74,    75,
    70,    71,    72,    73,    74,    75,    30,    31,   138,    71,
    72,    73,    74,    75,    72,    73,    74,    75,    91,    93,
    92,    94,    96,    99,    97,   100,    32,    33,    34,    35,
    48,    52,    54,    59,    76,   110,    77,    88,   114,   118,
   128,   111,   113,   115,   117,   119,   125,   126,   127,   129,
   132,   133,   135,   136,   139,   141,   109,   124,   134,   131
};

static const short yycheck[] = {    27,
     3,    29,     5,     3,     7,    29,    34,    35,    17,    18,
    19,    39,    40,    21,    22,    23,    24,    25,    26,    27,
    31,    24,    33,    31,    24,    31,    29,    33,    32,    29,
    21,    22,    23,    24,    25,    26,    27,    31,     5,    33,
    31,    69,    70,    71,    72,    73,    74,    75,    31,     0,
    33,    31,    80,    33,    32,     6,    84,     8,     9,    10,
    11,    12,    13,    14,    15,    16,    32,    95,    96,    20,
    32,    99,    21,    22,    23,    24,    25,    26,    27,    31,
     4,    33,    31,    21,    22,    23,    24,    25,    26,    27,
    26,    27,    30,    21,    22,    23,    24,    25,    26,    27,
    22,    23,    24,    25,    26,    27,    32,    32,   136,    23,
    24,    25,    26,    27,    24,    25,    26,    27,    31,    31,
    33,    33,    31,    31,    33,    33,    32,    32,    32,    32,
     4,     4,     4,     4,    30,     4,    31,    31,     4,     4,
     4,    34,    34,    34,    34,    34,    34,     5,    34,    34,
    34,     4,    34,    31,     4,     0,    77,    89,    99,    96
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
#line 163 "parse.y"
{ yyval.val = yyvsp[-2].val + yyvsp[0].val; ;
    break;}
case 3:
#line 164 "parse.y"
{ yyval.val = yyvsp[-2].val - yyvsp[0].val; ;
    break;}
case 4:
#line 165 "parse.y"
{ yyval.val = yyvsp[-2].val * yyvsp[0].val; ;
    break;}
case 5:
#line 166 "parse.y"
{ yyval.val = yyvsp[-2].val / yyvsp[0].val; ;
    break;}
case 6:
#line 167 "parse.y"
{ yyval.val = yyvsp[-2].val | yyvsp[0].val; ;
    break;}
case 7:
#line 168 "parse.y"
{ yyval.val = yyvsp[-2].val & yyvsp[0].val; ;
    break;}
case 8:
#line 169 "parse.y"
{ yyval.val = yyvsp[-2].val ^ yyvsp[0].val; ;
    break;}
case 9:
#line 170 "parse.y"
{ yyval.val = -yyvsp[0].val ;
    break;}
case 10:
#line 171 "parse.y"
{ yyval.val = yyvsp[-1].val; ;
    break;}
case 13:
#line 180 "parse.y"
{
			  printf ("\n");
		  ;
    break;}
case 17:
#line 192 "parse.y"
{ printf ("%s", yyvsp[0].string); ;
    break;}
case 18:
#line 193 "parse.y"
{ printf ("%d", yyvsp[0].val); ;
    break;}
case 19:
#line 194 "parse.y"
{ printf ("\n"); ;
    break;}
case 33:
#line 217 "parse.y"
{ AddAction (yyvsp[0].sym); ;
    break;}
case 34:
#line 218 "parse.y"
{ AddAction (yyvsp[0].sym); ;
    break;}
case 37:
#line 227 "parse.y"
{ AddHeight (yyvsp[0].val); ;
    break;}
case 38:
#line 228 "parse.y"
{ AddHeight (yyvsp[0].val); ;
    break;}
case 41:
#line 237 "parse.y"
{ AddActionMap (yyvsp[0].sym); ;
    break;}
case 42:
#line 238 "parse.y"
{ AddActionMap (yyvsp[0].sym); ;
    break;}
case 45:
#line 247 "parse.y"
{ AddCodeP (yyvsp[0].val); ;
    break;}
case 46:
#line 248 "parse.y"
{ AddCodeP (yyvsp[0].val); ;
    break;}
case 49:
#line 257 "parse.y"
{ AddSpriteName (yyvsp[0].sym); ;
    break;}
case 50:
#line 258 "parse.y"
{ AddSpriteName (yyvsp[0].sym); ;
    break;}
case 55:
#line 272 "parse.y"
{ AddStateMap (yyvsp[-4].sym, yyvsp[-2].val, yyvsp[0].val); ;
    break;}
case 56:
#line 276 "parse.y"
{ yyval.val = 0; ;
    break;}
case 57:
#line 277 "parse.y"
{ yyval.val = 1; ;
    break;}
case 58:
#line 278 "parse.y"
{ yyval.val = 2; ;
    break;}
case 61:
#line 287 "parse.y"
{ AddSoundMap (yyvsp[0].string); ;
    break;}
case 62:
#line 288 "parse.y"
{ AddSoundMap (yyvsp[0].string); ;
    break;}
case 65:
#line 297 "parse.y"
{ AddInfoName (yyvsp[0].sym); ;
    break;}
case 66:
#line 298 "parse.y"
{ AddInfoName (yyvsp[0].sym); ;
    break;}
case 71:
#line 312 "parse.y"
{ AddThingBits (yyvsp[0].sym, yyvsp[-4].val, yyvsp[-2].val); ;
    break;}
case 76:
#line 326 "parse.y"
{ AddRenderStyle (yyvsp[0].sym, yyvsp[-2].val); ;
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
#line 329 "parse.y"


#include <ctype.h>
#include <stdio.h>

// bison.simple #defines const to nothing
#ifdef const
#undef const
#endif

int yylex (void)
{
	char token[80];
	int toksize;
	int buildup;
	int c;

loop:
	if (Source == NULL)
	{
		return 0;
	}
	while (isspace (c = fgetc (Source)) && c != EOF)
	{
		if (c == '\n')
		{
			SourceLine++;
		}
	}

	if (c == EOF)
	{
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
			if (c == '\\')
			{
				c = fgetc (Source);
				if (c != '"' && c != EOF)
				{
					ungetc (c, Source);
					c = '\\';
				}
			}
			if (tokensize < 79)
			{
				yylval.string[tokensize++] = c;
			}
		}
		yylval.string[tokensize] = 0;
		return STRING;
	}
	return c;
}

static bool FindToken (char *tok, int *type)
{
	static const char tokens[][13] =
	{
		"endl", "print", "Actions",
		"OrgHeights", "ActionList", "CodePConv", "OrgSprNames",
		"StateMap", "SoundMap", "InfoNames", "ThingBits",
		"DeathState", "SpawnState", "FirstState", "RenderStyles"
	};
	static const short types[] =
	{
		ENDL, PRINT, Actions,
		OrgHeights, ActionList, CodePConv, OrgSprNames,
		StateMap, SoundMap, InfoNames, ThingBits,
		DeathState, SpawnState, FirstState, RenderStyles
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

static name FindName (char *name)
{
	struct StringList *probe = NameList;
	int count = 0;

	while (probe != NULL)
	{
		if (stricmp (probe->String, name) == 0)
		{
			return count;
		}
		count++;
		probe = probe->Next;
	}
	return -1;
}

static name AddName (char *name)
{
	struct StringList *newone;
	int index = FindName (name);

	if (index != -1)
		return index;

	newone = malloc (sizeof(*newone) + strlen(name));
	strcpy (newone->String, name);
	newone->Next = NULL;
	*NameListLast = newone;
	NameListLast = &newone->Next;
	return NameCount++;
}

static int FindAction (char *namei)
{
	int name = FindName (namei);

	if (name != -1)
	{
		int i;

		for (i = 0; i < ActionsListSize; i++)
		{
			if (ActionsList[i] == name)
				return i;
		}
	}
	printf ("Line %d: Unknown action %s\n", SourceLine, namei);
	ErrorCount++;
	return -1;
}

static void AddAction (char *name)
{
	int newname = AddName (name);
	if (ActionsListSize == MaxActionsListSize)
	{
		MaxActionsListSize = MaxActionsListSize ? MaxActionsListSize * 2 : 256;
		ActionsList = realloc (ActionsList, MaxActionsListSize*sizeof(*ActionsList));
	}
	ActionsList[ActionsListSize++] = newname;
}

static void AddHeight (int h)
{
	if (MaxHeightsSize == HeightsSize)
	{
		MaxHeightsSize = MaxHeightsSize ? MaxHeightsSize * 2 : 256;
		HeightsArray = realloc (HeightsArray, MaxHeightsSize);
	}
	HeightsArray[HeightsSize++] = h;
}

static void AddActionMap (char *name)
{
	int index = FindAction (name);

	if (index != -1)
	{
		if (ActionMapSize == MaxActionMapSize)
		{
			MaxActionMapSize = MaxActionMapSize ? MaxActionMapSize * 2 : 256;
			ActionMap = realloc (ActionMap, MaxActionMapSize*sizeof(*ActionMap));
		}
		ActionMap[ActionMapSize++] = index;
	}
}

static void AddCodeP (int codep)
{
	if (CodePMapSize == MaxCodePMapSize)
	{
		MaxCodePMapSize = MaxCodePMapSize ? MaxCodePMapSize * 2 : 256;
		CodePMap = realloc (CodePMap, MaxCodePMapSize*sizeof(*CodePMap));
	}
	CodePMap[CodePMapSize++] = codep;
}

static void AddSpriteName (char *name)
{
	if (strlen (name) != 4)
	{
		printf ("Line %d: Sprite name %s must be 4 characters\n", SourceLine, name);
		ErrorCount++;
		return;
	}
	if (SpriteNamesSize == MaxSpriteNamesSize)
	{
		MaxSpriteNamesSize = MaxSpriteNamesSize ? MaxSpriteNamesSize * 2 : 256*4;
		SpriteNames = realloc (SpriteNames, MaxSpriteNamesSize*sizeof(*SpriteNames));
	}
	SpriteNames[SpriteNamesSize+0] = toupper (name[0]);
	SpriteNames[SpriteNamesSize+1] = toupper (name[1]);
	SpriteNames[SpriteNamesSize+2] = toupper (name[2]);
	SpriteNames[SpriteNamesSize+3] = toupper (name[3]);
	SpriteNamesSize += 4;
}

static void AddSoundMap (char *name)
{
	if (SoundMapsSize == MaxSoundMapsSize)
	{
		MaxSoundMapsSize = MaxSoundMapsSize ? MaxSoundMapsSize * 2 : 256;
		SoundMaps = realloc (SoundMaps, MaxSoundMapsSize*sizeof(*SoundMaps));
	}
	SoundMaps[SoundMapsSize++] = AddName (name);
}

static void AddInfoName (char *name)
{
	if (InfoNamesSize == MaxInfoNamesSize)
	{
		MaxInfoNamesSize = MaxInfoNamesSize ? MaxInfoNamesSize * 2 : 256;
		InfoNamesArray = realloc (InfoNamesArray, MaxInfoNamesSize*sizeof(*InfoNamesArray));
	}
	InfoNamesArray[InfoNamesSize++] = AddName (name);
}

static void AddStateMap (char *name, int state, int count)
{
	if (count == 0)
	{
		printf ("Line %d: Count is 0. Is this right?\n", SourceLine);
		return;
	}
	if ((unsigned)count > 255)
	{
		printf ("Line %d: Count must be in the range 1-255\n", SourceLine);
		ErrorCount++;
	}
	if (StateMapsSize == MaxStateMapsSize)
	{
		MaxStateMapsSize = MaxStateMapsSize ? MaxStateMapsSize*2 : 256;
		StateMaps = realloc (StateMaps, MaxStateMapsSize*sizeof(*StateMaps));
	}
	StateMaps[StateMapsSize].Name = AddName (name);
	StateMaps[StateMapsSize].State = state;
	StateMaps[StateMapsSize].Count = count;
	StateMapsSize++;
}

static void AddThingBits (char *name, int bitnum, int flagnum)
{
	if ((unsigned)bitnum > 31)
	{
		printf ("Line %d: Bit %d must be in the range 0-31\n", SourceLine, bitnum);
		ErrorCount++;
		return;
	}
	if (MaxThingBitsMapSize == ThingBitsMapSize)
	{
		MaxThingBitsMapSize = MaxThingBitsMapSize ? MaxThingBitsMapSize*2 : 128;
		ThingBitsMap = realloc (ThingBitsMap, MaxThingBitsMapSize*sizeof(*ThingBitsMap));
	}
	ThingBitsMap[ThingBitsMapSize].Name = AddName (name);
	ThingBitsMap[ThingBitsMapSize].BitNum = bitnum;
	ThingBitsMap[ThingBitsMapSize].FlagNum = flagnum;
	ThingBitsMapSize++;
}

static void AddRenderStyle (char *name, int stylenum)
{
	if ((unsigned)stylenum > 255)
	{
		printf ("Line %d: %s must be in the range 0-255\n", SourceLine, name);
		ErrorCount++;
		return;
	}
	if (MaxRenderStylesSize == RenderStylesSize)
	{
		MaxRenderStylesSize = MaxRenderStylesSize ? MaxRenderStylesSize*2 : 16;
		RenderStylesMap = realloc (RenderStylesMap, MaxRenderStylesSize*sizeof(*RenderStylesMap));
	}
	RenderStylesMap[RenderStylesSize].Name = AddName (name);
	RenderStylesMap[RenderStylesSize].StyleNum = stylenum;
	RenderStylesSize++;
}

static int sortfunc (const void *a, const void *b)
{
	return stricmp (((struct StringSorter *)a)->Entry->String,
		((struct StringSorter *)b)->Entry->String);
}

static void SortNames ()
{
	name *remap = malloc (NameCount * sizeof(*remap));
	struct StringSorter *sorter = malloc (NameCount * sizeof(*sorter));
	struct StringList *probe, **prev;
	int i;

	for (i = 0, probe = NameList; probe != NULL; probe = probe->Next, i++)
	{
		sorter[i].OldName = i;
		sorter[i].Entry = probe;
	}

	// There are some warnings here, though I have no idea why.
	qsort (sorter, NameCount, sizeof(*sorter), sortfunc);

	for (i = 0, prev = &NameList; i < NameCount; i++)
	{
		*prev = sorter[i].Entry;
		prev = &sorter[i].Entry->Next;
	}
	*prev = NULL;

	for (i = 0; i < NameCount; i++)
	{
		remap[sorter[i].OldName] = i;
	}

	for (i = 0; i < ActionsListSize; i++)
	{
		ActionsList[i] = remap[ActionsList[i]];
	}
	for (i = 0; i < SoundMapsSize; i++)
	{
		SoundMaps[i] = remap[SoundMaps[i]];
	}
	for (i = 0; i < InfoNamesSize; i++)
	{
		InfoNamesArray[i] = remap[InfoNamesArray[i]];
	}
	for (i = 0; i < StateMapsSize; i++)
	{
		StateMaps[i].Name = remap[StateMaps[i].Name];
	}
	for (i = 0; i < ThingBitsMapSize; i++)
	{
		ThingBitsMap[i].Name = remap[ThingBitsMap[i].Name];
	}
	for (i = 0; i < RenderStylesSize; i++)
	{
		RenderStylesMap[i].Name = remap[RenderStylesMap[i].Name];
	}
}

int yyerror (char *s)
{
	printf ("Line %d: %s\n", SourceLine, s);
	ErrorCount++;
	return 0;
}

int main (int argc, char **argv)
{
	if (argc != 3)
	{
		printf ("Usage: dehsupp <infile> <outfile>\n");
		return -1;
	}
	Source = fopen (argv[1], "r");
	if (Source == NULL)
	{
		printf ("Could not open %s\n", argv[1]);
		return -2;
	}
	SourceLine = 1;
	yyparse ();
	fclose (Source);
	if (ErrorCount)
	{
		printf ("There were %d errors\n", ErrorCount);
		return -3;
	}
	SortNames ();
	Dest = fopen (argv[2], "wb");
	if (Dest == NULL)
	{
		printf ("Could not open %s\n", argv[2]);
		return -4;
	}
	WriteNameTable ();
	WriteActions ();
	WriteCodePConv ();
	WriteSprites ();
	WriteSounds ();
	WriteInfoNames ();
	WriteStates ();
	WriteActionMap ();
	WriteThingBits ();
	WriteRenderStyles ();
	WriteHeights ();
	WriteLabel ("END ");
	fclose (Dest);
	return 0;
}

void fail (int code, char *err)
{
	fclose (Dest);
	printf ("%s\n", err);
	exit (code);
}

void WriteWord (int word)
{
	putc (word >> 8, Dest);
	putc (word & 255, Dest);
}

void WriteLabel (char *label)
{
	fwrite (label, 1, 4, Dest);
}

void WriteWords (int count, short *array)
{
	int i;

	WriteWord (count);
	for (i = 0; i < count; i++)
	{
		WriteWord (array[i]);
	}
}

void WriteBytes (int count, unsigned char *array)
{
	WriteWord (count);
	fwrite (array, 1, count, Dest);
}

void WriteNameTable ()
{
	struct StringList *probe;
	int i, size;

	WriteLabel ("NAME");
	// Count the length of each string, including nulls
	for (probe = NameList, size = 0; probe != NULL; probe = probe->Next)
	{
		size += (int)strlen (probe->String) + 1;
	}

	if (size == 0)
	{
		WriteWord (2);		// Size of this lump
		WriteWord (0);		// Number of names
		return;
	}
	size += NameCount*2 + 2;
	if (size >= 65536)
	{
		fail (-5, "Name table is larger than 64K");
	}
	WriteWord (size);		// Size of this lump
	WriteWord (NameCount);	// Number of names

	// Write each name's offset from the first name, which is stored
	// immediately after this list
	for (i = size = 0, probe = NameList; i < NameCount; i++, probe = probe->Next)
	{
		WriteWord (size);
		size += (int)strlen (probe->String) + 1;
	}

	// Write each name's string in order now
	for (probe = NameList; probe != NULL; probe = probe->Next)
	{
		fputs (probe->String, Dest);
		putc (0, Dest);
	}
}

typedef struct
{
	name Name;
	short Num;
} NameNum;

int sortfunc2 (const void *a, const void *b)
{
	return ((NameNum *)a)->Name - ((NameNum *)b)->Name;
}

void WriteActions ()
{
	NameNum *sorter = malloc (ActionsListSize * sizeof(*sorter));
	int i;
	
	WriteLabel ("ACTF");
	WriteWord (ActionsListSize);

	for (i = 0; i < ActionsListSize; i++)
	{
		sorter[i].Name = ActionsList[i];
		sorter[i].Num = i;
	}
	// warnings here. ignore.
	qsort (sorter, ActionsListSize, sizeof(*sorter), sortfunc2);
	for (i = 0; i < ActionsListSize; i++)
	{
		WriteWord (sorter[i].Name);
		WriteWord (sorter[i].Num);
	}
	free (sorter);
}

void WriteActionMap ()
{
	WriteLabel ("ACTM");
	WriteBytes (ActionMapSize, ActionMap);
}

void WriteHeights ()
{
	WriteLabel ("HIGH");
	WriteBytes (HeightsSize, HeightsArray);
}

void WriteCodePConv ()
{
	WriteLabel ("CODP");
	WriteWords (CodePMapSize, (short *)CodePMap);
}

void WriteSprites ()
{
	WriteLabel ("SPRN");
	WriteWord (SpriteNamesSize / 4);
	fwrite (SpriteNames, SpriteNamesSize, 1, Dest);
}

void WriteStates ()
{
	int i;

	WriteLabel ("STAT");
	WriteWord (StateMapsSize);
	for (i = 0; i < StateMapsSize; i++)
	{
		WriteWord (StateMaps[i].Name);
		putc (StateMaps[i].State, Dest);
		putc (StateMaps[i].Count, Dest);
	}
}

void WriteSounds ()
{
	WriteLabel ("SND ");
	WriteWords (SoundMapsSize, SoundMaps);
}

void WriteInfoNames ()
{
	WriteLabel ("INFN");
	WriteWords (InfoNamesSize, InfoNamesArray);
}

static int sortfunc3 (const void *a, const void *b)
{
	return ((struct ThingBitsE *)a)->Name - ((struct ThingBitsE *)b)->Name;
}

void WriteThingBits ()
{
	int i;

	WriteLabel ("TBIT");
	WriteWord (ThingBitsMapSize);
	// warnings here; ignore them
	qsort (ThingBitsMap, ThingBitsMapSize, sizeof(*ThingBitsMap), sortfunc3);
	for (i = 0; i < ThingBitsMapSize; i++)
	{
		WriteWord (ThingBitsMap[i].Name);
		putc (ThingBitsMap[i].BitNum | (ThingBitsMap[i].FlagNum<<5), Dest);
	}
}

static int sortfunc4 (const void *a, const void *b)
{
	return ((struct RenderStylesE *)a)->Name - ((struct RenderStylesE *)b)->Name;
}

void WriteRenderStyles ()
{
	int i;

	WriteLabel ("REND");
	WriteWord (RenderStylesSize);
	// More warnings; ignore
	qsort (RenderStylesMap, RenderStylesSize, sizeof(*RenderStylesMap), sortfunc4);
	for (i = 0; i < RenderStylesSize; i++)
	{
		WriteWord (RenderStylesMap[i].Name);
		putc (RenderStylesMap[i].StyleNum, Dest);
	}
}

/* A Bison parser, made from parse.y
   by GNU bison 1.35.  */

#define YYBISON 1  /* Identify Bison output.  */

# define	NUM	257
# define	SYM	258
# define	STRING	259
# define	PRINT	260
# define	ENDL	261
# define	Actions	262
# define	OrgHeights	263
# define	ActionList	264
# define	CodePConv	265
# define	OrgSprNames	266
# define	StateMap	267
# define	SoundMap	268
# define	InfoNames	269
# define	ThingBits	270
# define	FirstState	271
# define	SpawnState	272
# define	DeathState	273
# define	RenderStyles	274
# define	NEG	275

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
void WriteBytes (int count, char *array);

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
#ifndef YYSTYPE
typedef union {
	int val;
	char sym[80];
	char string[80];
} yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
#ifndef YYDEBUG
# define YYDEBUG 0
#endif



#define	YYFINAL		141
#define	YYFLAG		-32768
#define	YYNTBASE	35

/* YYTRANSLATE(YYLEX) -- Bison token number corresponding to YYLEX. */
#define YYTRANSLATE(x) ((unsigned)(x) <= 275 ? yytranslate[x] : 65)

/* YYTRANSLATE[YYLEX] -- Bison token number corresponding to YYLEX. */
static const char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    23,     2,
      29,    30,    26,    25,    31,    24,     2,    27,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    34,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    22,     2,     2,     2,     2,     2,
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
       0,     0,     2,     6,    10,    14,    18,    22,    26,    30,
      33,    37,    38,    41,    46,    47,    49,    53,    55,    57,
      59,    61,    63,    65,    67,    69,    71,    73,    75,    77,
      79,    81,    87,    88,    90,    94,   100,   101,   103,   107,
     113,   114,   116,   120,   126,   127,   129,   133,   139,   140,
     142,   146,   152,   153,   155,   159,   165,   167,   169,   171,
     177,   178,   180,   184,   190,   191,   193,   197,   203,   204,
     206,   210,   216,   222,   223,   225,   229
};
static const short yyrhs[] =
{
       3,     0,    35,    25,    35,     0,    35,    24,    35,     0,
      35,    26,    35,     0,    35,    27,    35,     0,    35,    21,
      35,     0,    35,    23,    35,     0,    35,    22,    35,     0,
      24,    35,     0,    29,    35,    30,     0,     0,    36,    40,
       0,     6,    29,    38,    30,     0,     0,    39,     0,    39,
      31,    38,     0,     5,     0,    35,     0,     7,     0,    37,
       0,    41,     0,    43,     0,    45,     0,    47,     0,    49,
       0,    51,     0,    55,     0,    57,     0,    59,     0,    62,
       0,     8,    32,    42,    33,    34,     0,     0,     4,     0,
      42,    31,     4,     0,     9,    32,    44,    33,    34,     0,
       0,    35,     0,    44,    31,    35,     0,    10,    32,    46,
      33,    34,     0,     0,     4,     0,    46,    31,     4,     0,
      11,    32,    48,    33,    34,     0,     0,    35,     0,    48,
      31,    35,     0,    12,    32,    50,    33,    34,     0,     0,
       4,     0,    50,    31,     4,     0,    13,    32,    52,    33,
      34,     0,     0,    53,     0,    52,    31,    53,     0,     4,
      31,    54,    31,    35,     0,    17,     0,    18,     0,    19,
       0,    14,    32,    56,    33,    34,     0,     0,     5,     0,
      56,    31,     5,     0,    15,    32,    58,    33,    34,     0,
       0,     4,     0,    58,    31,     4,     0,    16,    32,    60,
      33,    34,     0,     0,    61,     0,    60,    31,    61,     0,
      35,    31,    35,    31,     4,     0,    20,    32,    63,    33,
      34,     0,     0,    64,     0,    63,    31,    64,     0,    35,
      31,     4,     0
};

#endif

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined. */
static const short yyrline[] =
{
       0,   161,   163,   164,   165,   166,   167,   168,   169,   170,
     171,   174,   175,   178,   185,   187,   188,   191,   193,   194,
     197,   199,   200,   201,   202,   203,   204,   205,   206,   207,
     208,   211,   215,   217,   218,   221,   225,   227,   228,   231,
     235,   237,   238,   241,   245,   247,   248,   251,   255,   257,
     258,   261,   265,   267,   268,   271,   275,   277,   278,   281,
     285,   287,   288,   291,   295,   297,   298,   301,   305,   307,
     308,   311,   315,   319,   321,   322,   325
};
#endif


#if (YYDEBUG) || defined YYERROR_VERBOSE

/* YYTNAME[TOKEN_NUM] -- String name of the token TOKEN_NUM. */
static const char *const yytname[] =
{
  "$", "error", "$undefined.", "NUM", "SYM", "STRING", "PRINT", "ENDL", 
  "Actions", "OrgHeights", "ActionList", "CodePConv", "OrgSprNames", 
  "StateMap", "SoundMap", "InfoNames", "ThingBits", "FirstState", 
  "SpawnState", "DeathState", "RenderStyles", "'|'", "'^'", "'&'", "'-'", 
  "'+'", "'*'", "'/'", "NEG", "'('", "')'", "','", "'{'", "'}'", "';'", 
  "exp", "translation_unit", "print_statement", "print_list", 
  "print_item", "external_declaration", "ActionsDef", "ActionsList", 
  "OrgHeightsDef", "OrgHeightsList", "ActionListDef", "ActionListList", 
  "CodePConvDef", "CodePConvList", "OrgSprNamesDef", "OrgSprNamesList", 
  "StateMapDef", "StateMapList", "StateMapEntry", "StateType", 
  "SoundMapDef", "SoundMapList", "InfoNamesDef", "InfoNamesList", 
  "ThingBitsDef", "ThingBitsList", "ThingBitsEntry", "RenderStylesDef", 
  "RenderStylesList", "RenderStylesEntry", 0
};
#endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives. */
static const short yyr1[] =
{
       0,    35,    35,    35,    35,    35,    35,    35,    35,    35,
      35,    36,    36,    37,    38,    38,    38,    39,    39,    39,
      40,    40,    40,    40,    40,    40,    40,    40,    40,    40,
      40,    41,    42,    42,    42,    43,    44,    44,    44,    45,
      46,    46,    46,    47,    48,    48,    48,    49,    50,    50,
      50,    51,    52,    52,    52,    53,    54,    54,    54,    55,
      56,    56,    56,    57,    58,    58,    58,    59,    60,    60,
      60,    61,    62,    63,    63,    63,    64
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN. */
static const short yyr2[] =
{
       0,     1,     3,     3,     3,     3,     3,     3,     3,     2,
       3,     0,     2,     4,     0,     1,     3,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     5,     0,     1,     3,     5,     0,     1,     3,     5,
       0,     1,     3,     5,     0,     1,     3,     5,     0,     1,
       3,     5,     0,     1,     3,     5,     1,     1,     1,     5,
       0,     1,     3,     5,     0,     1,     3,     5,     0,     1,
       3,     5,     5,     0,     1,     3,     3
};

/* YYDEFACT[S] -- default rule to reduce with in state S when YYTABLE
   doesn't specify something else to do.  Zero means the default is an
   error. */
static const short yydefact[] =
{
      11,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    20,    12,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    14,    32,    36,    40,    44,
      48,    52,    60,    64,    68,    73,     1,    17,    19,     0,
       0,    18,     0,    15,    33,     0,    37,     0,    41,     0,
      45,     0,    49,     0,     0,     0,    53,    61,     0,    65,
       0,     0,     0,    69,     0,     0,    74,     9,     0,     0,
       0,     0,     0,     0,     0,     0,    13,    14,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    10,     6,     8,     7,     3,     2,     4,     5,    16,
      34,    31,    38,    35,    42,    39,    46,    43,    50,    47,
      56,    57,    58,     0,    54,    51,    62,    59,    66,    63,
       0,    70,    67,    76,    75,    72,     0,     0,    55,    71,
       0,     0
};

static const short yydefgoto[] =
{
      41,     1,    13,    42,    43,    14,    15,    45,    16,    47,
      17,    49,    18,    51,    19,    53,    20,    55,    56,   123,
      21,    58,    22,    60,    23,    62,    63,    24,    65,    66
};

static const short yypact[] =
{
  -32768,    50,   -23,    -3,    23,    35,    39,    75,    76,    95,
      96,    97,    98,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
  -32768,-32768,-32768,-32768,-32768,    -2,    77,     1,   127,     1,
     128,   129,    34,   130,     1,     1,-32768,-32768,-32768,     1,
       1,    73,   105,   106,-32768,   -10,    73,    -5,-32768,     7,
      73,    18,-32768,    21,   107,    49,-32768,-32768,    88,-32768,
      89,    -7,    92,-32768,    10,    93,-32768,-32768,    63,     1,
       1,     1,     1,     1,     1,     1,-32768,    -2,   132,   108,
       1,   109,   135,   110,     1,   111,   136,   112,    -8,   129,
     113,   143,   115,   137,   116,     1,     1,   117,   148,     1,
     119,-32768,    79,    87,    91,    65,    65,-32768,-32768,-32768,
  -32768,-32768,    73,-32768,-32768,-32768,    73,-32768,-32768,-32768,
  -32768,-32768,-32768,   123,-32768,-32768,-32768,-32768,-32768,-32768,
      52,-32768,-32768,-32768,-32768,-32768,     1,   151,    73,-32768,
     156,-32768
};

static const short yypgoto[] =
{
     -27,-32768,-32768,    80,-32768,-32768,-32768,-32768,-32768,-32768,
  -32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,    69,-32768,
  -32768,-32768,-32768,-32768,-32768,-32768,    64,-32768,-32768,    60
};


#define	YYLAST		160


static const short yytable[] =
{
      46,    36,    50,    37,    36,    38,    25,    61,    64,   120,
     121,   122,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    78,    39,    79,    95,    39,    80,    40,    81,    26,
      40,    69,    70,    71,    72,    73,    74,    75,    82,    57,
      83,    98,   102,   103,   104,   105,   106,   107,   108,    84,
     140,    85,    86,   112,    87,    27,     2,   116,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    28,   130,    61,
      12,    29,    64,    69,    70,    71,    72,    73,    74,    75,
      89,    44,    90,   137,    69,    70,    71,    72,    73,    74,
      75,    74,    75,   101,    69,    70,    71,    72,    73,    74,
      75,    70,    71,    72,    73,    74,    75,    30,    31,   138,
      71,    72,    73,    74,    75,    72,    73,    74,    75,    91,
      93,    92,    94,    96,    99,    97,   100,    32,    33,    34,
      35,    48,    52,    54,    59,    76,   110,    77,    88,   114,
     118,   128,   111,   113,   115,   117,   119,   125,   126,   127,
     129,   132,   133,   135,   136,   139,   141,   109,   124,   134,
     131
};

static const short yycheck[] =
{
      27,     3,    29,     5,     3,     7,    29,    34,    35,    17,
      18,    19,    39,    40,    21,    22,    23,    24,    25,    26,
      27,    31,    24,    33,    31,    24,    31,    29,    33,    32,
      29,    21,    22,    23,    24,    25,    26,    27,    31,     5,
      33,    31,    69,    70,    71,    72,    73,    74,    75,    31,
       0,    33,    31,    80,    33,    32,     6,    84,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    32,    95,    96,
      20,    32,    99,    21,    22,    23,    24,    25,    26,    27,
      31,     4,    33,    31,    21,    22,    23,    24,    25,    26,
      27,    26,    27,    30,    21,    22,    23,    24,    25,    26,
      27,    22,    23,    24,    25,    26,    27,    32,    32,   136,
      23,    24,    25,    26,    27,    24,    25,    26,    27,    31,
      31,    33,    33,    31,    31,    33,    33,    32,    32,    32,
      32,     4,     4,     4,     4,    30,     4,    31,    31,     4,
       4,     4,    34,    34,    34,    34,    34,    34,     5,    34,
      34,    34,     4,    34,    31,     4,     0,    77,    89,    99,
      96
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
		if (strcmp (probe->String, name) == 0)
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

yyerror (char *s)
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

void WriteBytes (int count, char *array)
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
	WriteWords (CodePMapSize, CodePMap);
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

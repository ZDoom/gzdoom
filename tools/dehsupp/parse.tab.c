/* A Bison parser, made by GNU Bison 2.1.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005 Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* Written by Richard Stallman by simplifying the original so called
   ``semantic'' parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.1"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Using locations.  */
#define YYLSP_NEEDED 0



/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     NUM = 258,
     SYM = 259,
     STRING = 260,
     PRINT = 261,
     ENDL = 262,
     Actions = 263,
     OrgHeights = 264,
     ActionList = 265,
     CodePConv = 266,
     OrgSprNames = 267,
     StateMap = 268,
     SoundMap = 269,
     InfoNames = 270,
     ThingBits = 271,
     FirstState = 272,
     SpawnState = 273,
     DeathState = 274,
     RenderStyles = 275,
     NEG = 276
   };
#endif
/* Tokens.  */
#define NUM 258
#define SYM 259
#define STRING 260
#define PRINT 261
#define ENDL 262
#define Actions 263
#define OrgHeights 264
#define ActionList 265
#define CodePConv 266
#define OrgSprNames 267
#define StateMap 268
#define SoundMap 269
#define InfoNames 270
#define ThingBits 271
#define FirstState 272
#define SpawnState 273
#define DeathState 274
#define RenderStyles 275
#define NEG 276




/* Copy the first part of user declarations.  */
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



/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif

#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 121 "parse.y"
typedef union YYSTYPE {
	int val;
	char sym[80];
	char string[80];
} YYSTYPE;
/* Line 196 of yacc.c.  */
#line 253 "parse.tab.c"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 219 of yacc.c.  */
#line 265 "parse.tab.c"

#if ! defined (YYSIZE_T) && defined (__SIZE_TYPE__)
# define YYSIZE_T __SIZE_TYPE__
#endif
#if ! defined (YYSIZE_T) && defined (size_t)
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T) && (defined (__STDC__) || defined (__cplusplus))
# include <stddef.h> /* INFRINGES ON USER NAME SPACE */
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T)
# define YYSIZE_T unsigned int
#endif

#ifndef YY_
# if YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

#if ! defined (yyoverflow) || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if defined (__STDC__) || defined (__cplusplus)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     define YYINCLUDED_STDLIB_H
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning. */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2005 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM ((YYSIZE_T) -1)
#  endif
#  ifdef __cplusplus
extern "C" {
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if (! defined (malloc) && ! defined (YYINCLUDED_STDLIB_H) \
	&& (defined (__STDC__) || defined (__cplusplus)))
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if (! defined (free) && ! defined (YYINCLUDED_STDLIB_H) \
	&& (defined (__STDC__) || defined (__cplusplus)))
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifdef __cplusplus
}
#  endif
# endif
#endif /* ! defined (yyoverflow) || YYERROR_VERBOSE */


#if (! defined (yyoverflow) \
     && (! defined (__cplusplus) \
	 || (defined (YYSTYPE_IS_TRIVIAL) && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  short int yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short int) + sizeof (YYSTYPE))			\
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined (__GNUC__) && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
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
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (0)

#endif

#if defined (__STDC__) || defined (__cplusplus)
   typedef signed char yysigned_char;
#else
   typedef short int yysigned_char;
#endif

/* YYFINAL -- State number of the termination state. */
#define YYFINAL  2
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   159

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  35
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  31
/* YYNRULES -- Number of rules. */
#define YYNRULES  77
/* YYNRULES -- Number of states. */
#define YYNSTATES  141

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   276

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const unsigned char yytranslate[] =
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
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    28
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const unsigned char yyprhs[] =
{
       0,     0,     3,     5,     9,    13,    17,    21,    25,    29,
      33,    36,    40,    41,    44,    49,    50,    52,    56,    58,
      60,    62,    64,    66,    68,    70,    72,    74,    76,    78,
      80,    82,    84,    90,    91,    93,    97,   103,   104,   106,
     110,   116,   117,   119,   123,   129,   130,   132,   136,   142,
     143,   145,   149,   155,   156,   158,   162,   168,   170,   172,
     174,   180,   181,   183,   187,   193,   194,   196,   200,   206,
     207,   209,   213,   219,   225,   226,   228,   232
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const yysigned_char yyrhs[] =
{
      37,     0,    -1,     3,    -1,    36,    25,    36,    -1,    36,
      24,    36,    -1,    36,    26,    36,    -1,    36,    27,    36,
      -1,    36,    21,    36,    -1,    36,    23,    36,    -1,    36,
      22,    36,    -1,    24,    36,    -1,    29,    36,    30,    -1,
      -1,    37,    41,    -1,     6,    29,    39,    30,    -1,    -1,
      40,    -1,    40,    31,    39,    -1,     5,    -1,    36,    -1,
       7,    -1,    38,    -1,    42,    -1,    44,    -1,    46,    -1,
      48,    -1,    50,    -1,    52,    -1,    56,    -1,    58,    -1,
      60,    -1,    63,    -1,     8,    32,    43,    33,    34,    -1,
      -1,     4,    -1,    43,    31,     4,    -1,     9,    32,    45,
      33,    34,    -1,    -1,    36,    -1,    45,    31,    36,    -1,
      10,    32,    47,    33,    34,    -1,    -1,     4,    -1,    47,
      31,     4,    -1,    11,    32,    49,    33,    34,    -1,    -1,
      36,    -1,    49,    31,    36,    -1,    12,    32,    51,    33,
      34,    -1,    -1,     4,    -1,    51,    31,     4,    -1,    13,
      32,    53,    33,    34,    -1,    -1,    54,    -1,    53,    31,
      54,    -1,     4,    31,    55,    31,    36,    -1,    17,    -1,
      18,    -1,    19,    -1,    14,    32,    57,    33,    34,    -1,
      -1,     5,    -1,    57,    31,     5,    -1,    15,    32,    59,
      33,    34,    -1,    -1,     4,    -1,    59,    31,     4,    -1,
      16,    32,    61,    33,    34,    -1,    -1,    62,    -1,    61,
      31,    62,    -1,    36,    31,    36,    31,     4,    -1,    20,
      32,    64,    33,    34,    -1,    -1,    65,    -1,    64,    31,
      65,    -1,    36,    31,     4,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short int yyrline[] =
{
       0,   162,   162,   163,   164,   165,   166,   167,   168,   169,
     170,   171,   174,   175,   179,   185,   187,   188,   192,   193,
     194,   198,   199,   200,   201,   202,   203,   204,   205,   206,
     207,   208,   212,   215,   217,   218,   222,   225,   227,   228,
     232,   235,   237,   238,   242,   245,   247,   248,   252,   255,
     257,   258,   262,   265,   267,   268,   272,   276,   277,   278,
     282,   285,   287,   288,   292,   295,   297,   298,   302,   305,
     307,   308,   312,   316,   319,   321,   322,   326
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals. */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "NUM", "SYM", "STRING", "PRINT", "ENDL",
  "Actions", "OrgHeights", "ActionList", "CodePConv", "OrgSprNames",
  "StateMap", "SoundMap", "InfoNames", "ThingBits", "FirstState",
  "SpawnState", "DeathState", "RenderStyles", "'|'", "'^'", "'&'", "'-'",
  "'+'", "'*'", "'/'", "NEG", "'('", "')'", "','", "'{'", "'}'", "';'",
  "$accept", "exp", "translation_unit", "print_statement", "print_list",
  "print_item", "external_declaration", "ActionsDef", "ActionsList",
  "OrgHeightsDef", "OrgHeightsList", "ActionListDef", "ActionListList",
  "CodePConvDef", "CodePConvList", "OrgSprNamesDef", "OrgSprNamesList",
  "StateMapDef", "StateMapList", "StateMapEntry", "StateType",
  "SoundMapDef", "SoundMapList", "InfoNamesDef", "InfoNamesList",
  "ThingBitsDef", "ThingBitsList", "ThingBitsEntry", "RenderStylesDef",
  "RenderStylesList", "RenderStylesEntry", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const unsigned short int yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   124,    94,    38,    45,    43,    42,    47,   276,    40,
      41,    44,   123,   125,    59
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned char yyr1[] =
{
       0,    35,    36,    36,    36,    36,    36,    36,    36,    36,
      36,    36,    37,    37,    38,    39,    39,    39,    40,    40,
      40,    41,    41,    41,    41,    41,    41,    41,    41,    41,
      41,    41,    42,    43,    43,    43,    44,    45,    45,    45,
      46,    47,    47,    47,    48,    49,    49,    49,    50,    51,
      51,    51,    52,    53,    53,    53,    54,    55,    55,    55,
      56,    57,    57,    57,    58,    59,    59,    59,    60,    61,
      61,    61,    62,    63,    64,    64,    64,    65
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     1,     3,     3,     3,     3,     3,     3,     3,
       2,     3,     0,     2,     4,     0,     1,     3,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     5,     0,     1,     3,     5,     0,     1,     3,
       5,     0,     1,     3,     5,     0,     1,     3,     5,     0,
       1,     3,     5,     0,     1,     3,     5,     1,     1,     1,
       5,     0,     1,     3,     5,     0,     1,     3,     5,     0,
       1,     3,     5,     5,     0,     1,     3,     3
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned char yydefact[] =
{
      12,     0,     1,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    21,    13,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    15,    33,    37,    41,
      45,    49,    53,    61,    65,    69,    74,     2,    18,    20,
       0,     0,    19,     0,    16,    34,     0,    38,     0,    42,
       0,    46,     0,    50,     0,     0,     0,    54,    62,     0,
      66,     0,     0,     0,    70,     0,     0,    75,    10,     0,
       0,     0,     0,     0,     0,     0,     0,    14,    15,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    11,     7,     9,     8,     4,     3,     5,     6,
      17,    35,    32,    39,    36,    43,    40,    47,    44,    51,
      48,    57,    58,    59,     0,    55,    52,    63,    60,    67,
      64,     0,    71,    68,    77,    76,    73,     0,     0,    56,
      72
};

/* YYDEFGOTO[NTERM-NUM]. */
static const yysigned_char yydefgoto[] =
{
      -1,    42,     1,    14,    43,    44,    15,    16,    46,    17,
      48,    18,    50,    19,    52,    20,    54,    21,    56,    57,
     124,    22,    59,    23,    61,    24,    63,    64,    25,    66,
      67
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -29
static const short int yypact[] =
{
     -29,    50,   -29,   -23,    -3,    23,    35,    39,    75,    76,
      95,    96,    97,    98,   -29,   -29,   -29,   -29,   -29,   -29,
     -29,   -29,   -29,   -29,   -29,   -29,    -2,    77,     1,   127,
       1,   128,   129,    34,   130,     1,     1,   -29,   -29,   -29,
       1,     1,    73,   105,   106,   -29,   -10,    73,    -5,   -29,
       7,    73,    18,   -29,    21,   107,    49,   -29,   -29,    88,
     -29,    89,    -7,    92,   -29,    10,    93,   -29,   -29,    63,
       1,     1,     1,     1,     1,     1,     1,   -29,    -2,   132,
     108,     1,   109,   135,   110,     1,   111,   136,   112,    -8,
     129,   113,   143,   115,   137,   116,     1,     1,   117,   148,
       1,   119,   -29,    79,    87,    91,    65,    65,   -29,   -29,
     -29,   -29,   -29,    73,   -29,   -29,   -29,    73,   -29,   -29,
     -29,   -29,   -29,   -29,   123,   -29,   -29,   -29,   -29,   -29,
     -29,    52,   -29,   -29,   -29,   -29,   -29,     1,   151,    73,
     -29
};

/* YYPGOTO[NTERM-NUM].  */
static const yysigned_char yypgoto[] =
{
     -29,   -28,   -29,   -29,    78,   -29,   -29,   -29,   -29,   -29,
     -29,   -29,   -29,   -29,   -29,   -29,   -29,   -29,   -29,    67,
     -29,   -29,   -29,   -29,   -29,   -29,   -29,    61,   -29,   -29,
      59
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const unsigned char yytable[] =
{
      47,    37,    51,    38,    37,    39,    26,    62,    65,   121,
     122,   123,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    79,    40,    80,    96,    40,    81,    41,    82,    27,
      41,    70,    71,    72,    73,    74,    75,    76,    83,    58,
      84,    99,   103,   104,   105,   106,   107,   108,   109,    85,
       2,    86,    87,   113,    88,    28,     3,   117,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    29,   131,    62,
      13,    30,    65,    70,    71,    72,    73,    74,    75,    76,
      90,    45,    91,   138,    70,    71,    72,    73,    74,    75,
      76,    75,    76,   102,    70,    71,    72,    73,    74,    75,
      76,    71,    72,    73,    74,    75,    76,    31,    32,   139,
      72,    73,    74,    75,    76,    73,    74,    75,    76,    92,
      94,    93,    95,    97,   100,    98,   101,    33,    34,    35,
      36,    49,    53,    55,    60,    77,   111,    78,    89,   115,
     119,   129,   112,   114,   116,   118,   120,   126,   127,   128,
     130,   133,   134,   136,   137,   140,   110,   125,   132,   135
};

static const unsigned char yycheck[] =
{
      28,     3,    30,     5,     3,     7,    29,    35,    36,    17,
      18,    19,    40,    41,    21,    22,    23,    24,    25,    26,
      27,    31,    24,    33,    31,    24,    31,    29,    33,    32,
      29,    21,    22,    23,    24,    25,    26,    27,    31,     5,
      33,    31,    70,    71,    72,    73,    74,    75,    76,    31,
       0,    33,    31,    81,    33,    32,     6,    85,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    32,    96,    97,
      20,    32,   100,    21,    22,    23,    24,    25,    26,    27,
      31,     4,    33,    31,    21,    22,    23,    24,    25,    26,
      27,    26,    27,    30,    21,    22,    23,    24,    25,    26,
      27,    22,    23,    24,    25,    26,    27,    32,    32,   137,
      23,    24,    25,    26,    27,    24,    25,    26,    27,    31,
      31,    33,    33,    31,    31,    33,    33,    32,    32,    32,
      32,     4,     4,     4,     4,    30,     4,    31,    31,     4,
       4,     4,    34,    34,    34,    34,    34,    34,     5,    34,
      34,    34,     4,    34,    31,     4,    78,    90,    97,   100
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,    37,     0,     6,     8,     9,    10,    11,    12,    13,
      14,    15,    16,    20,    38,    41,    42,    44,    46,    48,
      50,    52,    56,    58,    60,    63,    29,    32,    32,    32,
      32,    32,    32,    32,    32,    32,    32,     3,     5,     7,
      24,    29,    36,    39,    40,     4,    43,    36,    45,     4,
      47,    36,    49,     4,    51,     4,    53,    54,     5,    57,
       4,    59,    36,    61,    62,    36,    64,    65,    36,    36,
      21,    22,    23,    24,    25,    26,    27,    30,    31,    31,
      33,    31,    33,    31,    33,    31,    33,    31,    33,    31,
      31,    33,    31,    33,    31,    33,    31,    31,    33,    31,
      31,    33,    30,    36,    36,    36,    36,    36,    36,    36,
      39,     4,    34,    36,    34,     4,    34,    36,    34,     4,
      34,    17,    18,    19,    55,    54,    34,     5,    34,     4,
      34,    36,    62,    34,     4,    65,    34,    31,    31,    36,
       4
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


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
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (0)


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (N)								\
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (0)
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
              (Loc).first_line, (Loc).first_column,	\
              (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

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

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)		\
do {								\
  if (yydebug)							\
    {								\
      YYFPRINTF (stderr, "%s ", Title);				\
      yysymprint (stderr,					\
                  Type, Value);	\
      YYFPRINTF (stderr, "\n");					\
    }								\
} while (0)

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_stack_print (short int *bottom, short int *top)
#else
static void
yy_stack_print (bottom, top)
    short int *bottom;
    short int *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (/* Nothing. */; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_reduce_print (int yyrule)
#else
static void
yy_reduce_print (yyrule)
    int yyrule;
#endif
{
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu), ",
             yyrule - 1, yylno);
  /* Print the symbols being reduced, and their result.  */
  for (yyi = yyprhs[yyrule]; 0 <= yyrhs[yyi]; yyi++)
    YYFPRINTF (stderr, "%s ", yytname[yyrhs[yyi]]);
  YYFPRINTF (stderr, "-> %s\n", yytname[yyr1[yyrule]]);
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (Rule);		\
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

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
  const char *yys = yystr;

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
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      size_t yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

#endif /* YYERROR_VERBOSE */



#if YYDEBUG
/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yysymprint (FILE *yyoutput, int yytype, YYSTYPE *yyvaluep)
#else
static void
yysymprint (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;

  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);


# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  switch (yytype)
    {
      default:
        break;
    }
  YYFPRINTF (yyoutput, ")");
}

#endif /* ! YYDEBUG */
/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
        break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM);
# else
int yyparse ();
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */



/* The look-ahead symbol.  */
int yychar;

/* The semantic value of the look-ahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;



/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM)
# else
int yyparse (YYPARSE_PARAM)
  void *YYPARSE_PARAM;
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
int
yyparse (void)
#else
int
yyparse ()
    ;
#endif
#endif
{
  
  int yystate;
  int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Look-ahead token as an internal (translated) token number.  */
  int yytoken = 0;

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  short int yyssa[YYINITDEPTH];
  short int *yyss = yyssa;
  short int *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  YYSTYPE *yyvsp;



#define YYPOPSTACK   (yyvsp--, yyssp--)

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;


  /* When reducing, the number of symbols on the RHS of the reduced
     rule.  */
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

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	short int *yyss1 = yyss;


	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),

		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	short int *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);

#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;


      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

/* Do appropriate processing given the current state.  */
/* Read a look-ahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to look-ahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a look-ahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid look-ahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the look-ahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;


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

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 3:
#line 163 "parse.y"
    { (yyval.val) = (yyvsp[-2].val) + (yyvsp[0].val); ;}
    break;

  case 4:
#line 164 "parse.y"
    { (yyval.val) = (yyvsp[-2].val) - (yyvsp[0].val); ;}
    break;

  case 5:
#line 165 "parse.y"
    { (yyval.val) = (yyvsp[-2].val) * (yyvsp[0].val); ;}
    break;

  case 6:
#line 166 "parse.y"
    { (yyval.val) = (yyvsp[-2].val) / (yyvsp[0].val); ;}
    break;

  case 7:
#line 167 "parse.y"
    { (yyval.val) = (yyvsp[-2].val) | (yyvsp[0].val); ;}
    break;

  case 8:
#line 168 "parse.y"
    { (yyval.val) = (yyvsp[-2].val) & (yyvsp[0].val); ;}
    break;

  case 9:
#line 169 "parse.y"
    { (yyval.val) = (yyvsp[-2].val) ^ (yyvsp[0].val); ;}
    break;

  case 10:
#line 170 "parse.y"
    { (yyval.val) = -(yyvsp[0].val) ;}
    break;

  case 11:
#line 171 "parse.y"
    { (yyval.val) = (yyvsp[-1].val); ;}
    break;

  case 14:
#line 180 "parse.y"
    {
			  printf ("\n");
		  ;}
    break;

  case 18:
#line 192 "parse.y"
    { printf ("%s", (yyvsp[0].string)); ;}
    break;

  case 19:
#line 193 "parse.y"
    { printf ("%d", (yyvsp[0].val)); ;}
    break;

  case 20:
#line 194 "parse.y"
    { printf ("\n"); ;}
    break;

  case 34:
#line 217 "parse.y"
    { AddAction ((yyvsp[0].sym)); ;}
    break;

  case 35:
#line 218 "parse.y"
    { AddAction ((yyvsp[0].sym)); ;}
    break;

  case 38:
#line 227 "parse.y"
    { AddHeight ((yyvsp[0].val)); ;}
    break;

  case 39:
#line 228 "parse.y"
    { AddHeight ((yyvsp[0].val)); ;}
    break;

  case 42:
#line 237 "parse.y"
    { AddActionMap ((yyvsp[0].sym)); ;}
    break;

  case 43:
#line 238 "parse.y"
    { AddActionMap ((yyvsp[0].sym)); ;}
    break;

  case 46:
#line 247 "parse.y"
    { AddCodeP ((yyvsp[0].val)); ;}
    break;

  case 47:
#line 248 "parse.y"
    { AddCodeP ((yyvsp[0].val)); ;}
    break;

  case 50:
#line 257 "parse.y"
    { AddSpriteName ((yyvsp[0].sym)); ;}
    break;

  case 51:
#line 258 "parse.y"
    { AddSpriteName ((yyvsp[0].sym)); ;}
    break;

  case 56:
#line 272 "parse.y"
    { AddStateMap ((yyvsp[-4].sym), (yyvsp[-2].val), (yyvsp[0].val)); ;}
    break;

  case 57:
#line 276 "parse.y"
    { (yyval.val) = 0; ;}
    break;

  case 58:
#line 277 "parse.y"
    { (yyval.val) = 1; ;}
    break;

  case 59:
#line 278 "parse.y"
    { (yyval.val) = 2; ;}
    break;

  case 62:
#line 287 "parse.y"
    { AddSoundMap ((yyvsp[0].string)); ;}
    break;

  case 63:
#line 288 "parse.y"
    { AddSoundMap ((yyvsp[0].string)); ;}
    break;

  case 66:
#line 297 "parse.y"
    { AddInfoName ((yyvsp[0].sym)); ;}
    break;

  case 67:
#line 298 "parse.y"
    { AddInfoName ((yyvsp[0].sym)); ;}
    break;

  case 72:
#line 312 "parse.y"
    { AddThingBits ((yyvsp[0].sym), (yyvsp[-4].val), (yyvsp[-2].val)); ;}
    break;

  case 77:
#line 326 "parse.y"
    { AddRenderStyle ((yyvsp[0].sym), (yyvsp[-2].val)); ;}
    break;


      default: break;
    }

/* Line 1126 of yacc.c.  */
#line 1550 "parse.tab.c"

  yyvsp -= yylen;
  yyssp -= yylen;


  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;


  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (YYPACT_NINF < yyn && yyn < YYLAST)
	{
	  int yytype = YYTRANSLATE (yychar);
	  YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
	  YYSIZE_T yysize = yysize0;
	  YYSIZE_T yysize1;
	  int yysize_overflow = 0;
	  char *yymsg = 0;
#	  define YYERROR_VERBOSE_ARGS_MAXIMUM 5
	  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
	  int yyx;

#if 0
	  /* This is so xgettext sees the translatable formats that are
	     constructed on the fly.  */
	  YY_("syntax error, unexpected %s");
	  YY_("syntax error, unexpected %s, expecting %s");
	  YY_("syntax error, unexpected %s, expecting %s or %s");
	  YY_("syntax error, unexpected %s, expecting %s or %s or %s");
	  YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
#endif
	  char *yyfmt;
	  char const *yyf;
	  static char const yyunexpected[] = "syntax error, unexpected %s";
	  static char const yyexpecting[] = ", expecting %s";
	  static char const yyor[] = " or %s";
	  char yyformat[sizeof yyunexpected
			+ sizeof yyexpecting - 1
			+ ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
			   * (sizeof yyor - 1))];
	  char const *yyprefix = yyexpecting;

	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  int yyxbegin = yyn < 0 ? -yyn : 0;

	  /* Stay within bounds of both yycheck and yytname.  */
	  int yychecklim = YYLAST - yyn;
	  int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
	  int yycount = 1;

	  yyarg[0] = yytname[yytype];
	  yyfmt = yystpcpy (yyformat, yyunexpected);

	  for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	      {
		if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
		  {
		    yycount = 1;
		    yysize = yysize0;
		    yyformat[sizeof yyunexpected - 1] = '\0';
		    break;
		  }
		yyarg[yycount++] = yytname[yyx];
		yysize1 = yysize + yytnamerr (0, yytname[yyx]);
		yysize_overflow |= yysize1 < yysize;
		yysize = yysize1;
		yyfmt = yystpcpy (yyfmt, yyprefix);
		yyprefix = yyor;
	      }

	  yyf = YY_(yyformat);
	  yysize1 = yysize + yystrlen (yyf);
	  yysize_overflow |= yysize1 < yysize;
	  yysize = yysize1;

	  if (!yysize_overflow && yysize <= YYSTACK_ALLOC_MAXIMUM)
	    yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg)
	    {
	      /* Avoid sprintf, as that infringes on the user's name space.
		 Don't have undefined behavior even if the translation
		 produced a string with the wrong number of "%s"s.  */
	      char *yyp = yymsg;
	      int yyi = 0;
	      while ((*yyp = *yyf))
		{
		  if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		    {
		      yyp += yytnamerr (yyp, yyarg[yyi++]);
		      yyf += 2;
		    }
		  else
		    {
		      yyp++;
		      yyf++;
		    }
		}
	      yyerror (yymsg);
	      YYSTACK_FREE (yymsg);
	    }
	  else
	    {
	      yyerror (YY_("syntax error"));
	      goto yyexhaustedlab;
	    }
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror (YY_("syntax error"));
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse look-ahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
        {
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
        }
      else
	{
	  yydestruct ("Error: discarding", yytoken, &yylval);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse look-ahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (0)
     goto yyerrorlab;

yyvsp -= yylen;
  yyssp -= yylen;
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;


      yydestruct ("Error: popping", yystos[yystate], yyvsp);
      YYPOPSTACK;
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  *++yyvsp = yylval;


  /* Shift the error token. */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

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

#ifndef yyoverflow
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEOF && yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp);
      YYPOPSTACK;
    }
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



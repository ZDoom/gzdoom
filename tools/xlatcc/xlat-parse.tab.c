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
     SYMNUM = 259,
     SYM = 260,
     STRING = 261,
     DEFINE = 262,
     INCLUDE = 263,
     TAG = 264,
     LINEID = 265,
     SPECIAL = 266,
     FLAGS = 267,
     ARG2 = 268,
     ARG3 = 269,
     ARG4 = 270,
     ARG5 = 271,
     OR_EQUAL = 272,
     ENUM = 273,
     PRINT = 274,
     ENDL = 275,
     NEG = 276
   };
#endif
/* Tokens.  */
#define NUM 258
#define SYMNUM 259
#define SYM 260
#define STRING 261
#define DEFINE 262
#define INCLUDE 263
#define TAG 264
#define LINEID 265
#define SPECIAL 266
#define FLAGS 267
#define ARG2 268
#define ARG3 269
#define ARG4 270
#define ARG5 271
#define OR_EQUAL 272
#define ENUM 273
#define PRINT 274
#define ENDL 275
#define NEG 276




/* Copy the first part of user declarations.  */
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
#line 51 "xlat-parse.y"
typedef union YYSTYPE {
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
/* Line 196 of yacc.c.  */
#line 193 "xlat-parse.tab.c"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 219 of yacc.c.  */
#line 205 "xlat-parse.tab.c"

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
#define YYLAST   449

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  39
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  28
/* YYNRULES -- Number of rules. */
#define YYNRULES  95
/* YYNRULES -- Number of states. */
#define YYNSTATES  175

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
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    28
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const unsigned short int yyprhs[] =
{
       0,     0,     3,     5,     7,    11,    15,    19,    23,    27,
      31,    35,    38,    42,    43,    46,    48,    50,    52,    54,
      56,    58,    60,    65,    66,    68,    72,    74,    76,    78,
      84,    87,    88,    94,    95,    97,   101,   103,   107,   111,
     112,   114,   118,   125,   126,   134,   135,   137,   141,   150,
     159,   171,   172,   175,   179,   181,   183,   185,   187,   189,
     191,   193,   195,   200,   202,   206,   210,   211,   213,   217,
     223,   231,   241,   245,   251,   259,   269,   271,   275,   281,
     289,   299,   301,   305,   311,   319,   329,   333,   339,   347,
     357,   363,   371,   381,   389,   399
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const yysigned_char yyrhs[] =
{
      41,     0,    -1,     3,    -1,     4,    -1,    40,    25,    40,
      -1,    40,    24,    40,    -1,    40,    26,    40,    -1,    40,
      27,    40,    -1,    40,    21,    40,    -1,    40,    23,    40,
      -1,    40,    22,    40,    -1,    24,    40,    -1,    29,    40,
      30,    -1,    -1,    41,    42,    -1,    46,    -1,    47,    -1,
      43,    -1,    48,    -1,    57,    -1,    58,    -1,    52,    -1,
      19,    29,    44,    30,    -1,    -1,    45,    -1,    45,    31,
      44,    -1,     6,    -1,    40,    -1,    20,    -1,     7,     5,
      29,    40,    30,    -1,     8,     6,    -1,    -1,    18,    32,
      49,    50,    33,    -1,    -1,    51,    -1,    51,    31,    50,
      -1,     5,    -1,     5,    34,    40,    -1,    11,    53,    35,
      -1,    -1,    54,    -1,    54,    31,    53,    -1,    40,    36,
       5,    29,    56,    30,    -1,    -1,    40,    36,     4,    55,
      29,    56,    30,    -1,    -1,    40,    -1,    40,    31,    40,
      -1,    40,    34,    40,    31,    40,    29,    66,    30,    -1,
      40,    34,    40,    31,     5,    29,    66,    30,    -1,    37,
      40,    38,    29,    40,    31,    40,    30,    32,    59,    33,
      -1,    -1,    60,    59,    -1,    61,    62,    63,    -1,    12,
      -1,    13,    -1,    14,    -1,    15,    -1,    16,    -1,    34,
      -1,    17,    -1,    40,    -1,    40,    37,    64,    38,    -1,
      65,    -1,    65,    31,    64,    -1,    40,    36,    40,    -1,
      -1,     9,    -1,     9,    31,    40,    -1,     9,    31,    40,
      31,    40,    -1,     9,    31,    40,    31,    40,    31,    40,
      -1,     9,    31,    40,    31,    40,    31,    40,    31,    40,
      -1,     9,    31,     9,    -1,     9,    31,     9,    31,    40,
      -1,     9,    31,     9,    31,    40,    31,    40,    -1,     9,
      31,     9,    31,    40,    31,    40,    31,    40,    -1,    10,
      -1,    10,    31,    40,    -1,    10,    31,    40,    31,    40,
      -1,    10,    31,    40,    31,    40,    31,    40,    -1,    10,
      31,    40,    31,    40,    31,    40,    31,    40,    -1,    40,
      -1,    40,    31,    40,    -1,    40,    31,    40,    31,    40,
      -1,    40,    31,    40,    31,    40,    31,    40,    -1,    40,
      31,    40,    31,    40,    31,    40,    31,    40,    -1,    40,
      31,     9,    -1,    40,    31,     9,    31,    40,    -1,    40,
      31,     9,    31,    40,    31,    40,    -1,    40,    31,     9,
      31,    40,    31,    40,    31,    40,    -1,    40,    31,    40,
      31,     9,    -1,    40,    31,    40,    31,     9,    31,    40,
      -1,    40,    31,    40,    31,     9,    31,    40,    31,    40,
      -1,    40,    31,    40,    31,    40,    31,     9,    -1,    40,
      31,    40,    31,    40,    31,     9,    31,    40,    -1,    40,
      31,    40,    31,    40,    31,    40,    31,     9,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short int yyrline[] =
{
       0,   116,   116,   117,   118,   119,   120,   121,   122,   123,
     124,   125,   126,   129,   130,   134,   135,   136,   137,   138,
     139,   140,   144,   150,   152,   153,   157,   158,   159,   163,
     167,   171,   171,   174,   176,   177,   181,   182,   189,   192,
     194,   195,   199,   201,   200,   206,   207,   208,   212,   222,
     229,   277,   280,   289,   324,   325,   326,   327,   328,   332,
     333,   337,   342,   350,   356,   365,   370,   374,   379,   388,
     397,   406,   416,   424,   432,   440,   449,   454,   463,   472,
     481,   491,   500,   509,   518,   527,   537,   546,   555,   564,
     574,   583,   592,   602,   611,   621
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals. */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "NUM", "SYMNUM", "SYM", "STRING",
  "DEFINE", "INCLUDE", "TAG", "LINEID", "SPECIAL", "FLAGS", "ARG2", "ARG3",
  "ARG4", "ARG5", "OR_EQUAL", "ENUM", "PRINT", "ENDL", "'|'", "'^'", "'&'",
  "'-'", "'+'", "'*'", "'/'", "NEG", "'('", "')'", "','", "'{'", "'}'",
  "'='", "';'", "':'", "'['", "']'", "$accept", "exp", "translation_unit",
  "external_declaration", "print_statement", "print_list", "print_item",
  "define_statement", "include_statement", "enum_statement", "@1",
  "enum_list", "single_enum", "special_declaration", "special_list",
  "special_def", "@2", "maybe_argcount", "linetype_declaration",
  "boom_declaration", "boom_body", "boom_line", "boom_selector", "boom_op",
  "boom_args", "arg_list", "list_val", "special_args", 0
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
      41,    44,   123,   125,    61,    59,    58,    91,    93
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned char yyr1[] =
{
       0,    39,    40,    40,    40,    40,    40,    40,    40,    40,
      40,    40,    40,    41,    41,    42,    42,    42,    42,    42,
      42,    42,    43,    44,    44,    44,    45,    45,    45,    46,
      47,    49,    48,    50,    50,    50,    51,    51,    52,    53,
      53,    53,    54,    55,    54,    56,    56,    56,    57,    57,
      58,    59,    59,    60,    61,    61,    61,    61,    61,    62,
      62,    63,    63,    64,    64,    65,    66,    66,    66,    66,
      66,    66,    66,    66,    66,    66,    66,    66,    66,    66,
      66,    66,    66,    66,    66,    66,    66,    66,    66,    66,
      66,    66,    66,    66,    66,    66
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     1,     1,     3,     3,     3,     3,     3,     3,
       3,     2,     3,     0,     2,     1,     1,     1,     1,     1,
       1,     1,     4,     0,     1,     3,     1,     1,     1,     5,
       2,     0,     5,     0,     1,     3,     1,     3,     3,     0,
       1,     3,     6,     0,     7,     0,     1,     3,     8,     8,
      11,     0,     2,     3,     1,     1,     1,     1,     1,     1,
       1,     1,     4,     1,     3,     3,     0,     1,     3,     5,
       7,     9,     3,     5,     7,     9,     1,     3,     5,     7,
       9,     1,     3,     5,     7,     9,     3,     5,     7,     9,
       5,     7,     9,     7,     9,     9
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned char yydefact[] =
{
      13,     0,     1,     2,     3,     0,     0,    39,     0,     0,
       0,     0,     0,     0,    14,    17,    15,    16,    18,    21,
      19,    20,     0,    30,     0,     0,    40,    31,    23,    11,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    38,    39,    33,    26,    28,    27,     0,    24,
      12,     0,     8,    10,     9,     5,     4,     6,     7,     0,
       0,    43,     0,    41,    36,     0,    34,    22,    23,     0,
       0,    29,     0,    45,     0,    32,    33,    25,     0,     0,
       0,    45,    46,     0,    37,    35,     0,    66,    66,     0,
       0,    42,     0,    67,    76,    81,     0,     0,    44,    47,
       0,     0,     0,     0,    49,    48,    51,    72,    68,    77,
      86,    82,    54,    55,    56,    57,    58,     0,    51,     0,
       0,     0,     0,     0,     0,    50,    52,    60,    59,     0,
      73,    69,    78,    87,    90,    83,    61,    53,     0,     0,
       0,     0,     0,     0,     0,    74,    70,    79,    88,    91,
      93,    84,     0,     0,    63,     0,     0,     0,     0,     0,
       0,     0,     0,    62,     0,    75,    71,    80,    89,    92,
      94,    95,    85,    65,    64
};

/* YYDEFGOTO[NTERM-NUM]. */
static const short int yydefgoto[] =
{
      -1,    24,     1,    14,    15,    48,    49,    16,    17,    18,
      44,    65,    66,    19,    25,    26,    72,    83,    20,    21,
     117,   118,   119,   129,   137,   153,   154,    96
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -28
static const short int yypact[] =
{
     -28,    41,   -28,   -28,   -28,     2,     6,    50,   -27,    11,
      50,    50,    50,   166,   -28,   -28,   -28,   -28,   -28,   -28,
     -28,   -28,    14,   -28,   143,    -7,    -5,   -28,     0,   -28,
     378,    -8,    50,    50,    50,    50,    50,    50,    50,    50,
      50,    -3,   -28,    50,    53,   -28,   -28,   417,    16,    24,
     -28,    46,   158,   171,    37,    68,    68,   -28,   -28,   180,
     388,   -28,    48,   -28,    32,    51,    66,   -28,     0,    50,
      18,   -28,    52,    50,    50,   -28,    53,   -28,   191,    70,
     408,    50,   202,    84,   417,   -28,    50,    47,    47,    85,
      50,   -28,   398,    78,    86,   213,    94,    96,   -28,   417,
      97,    79,    50,    87,   -28,   -28,   433,   100,   224,   235,
     102,   246,   -28,   -28,   -28,   -28,   -28,   111,   433,    -9,
      50,    50,    50,    50,    89,   -28,   -28,   -28,   -28,    50,
     257,   268,   279,   290,   104,   301,   125,   -28,    50,    50,
      50,    50,    50,   101,    50,   312,   323,   334,   345,   356,
     105,   367,   150,   107,   122,    50,    50,    50,    50,    50,
      50,   103,    50,   -28,    50,   417,   417,   417,   417,   417,
     417,   -28,   417,   417,   -28
};

/* YYPGOTO[NTERM-NUM].  */
static const short int yypgoto[] =
{
     -28,    -1,   -28,   -28,   -28,   110,   -28,   -28,   -28,   -28,
     -28,    58,   -28,   -28,   156,   -28,   -28,   127,   -28,   -28,
      91,   -28,   -28,   -28,   -28,    55,   -28,   132
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const unsigned char yytable[] =
{
      13,    61,    62,     3,     4,    27,    45,    22,   127,    29,
      30,    31,    23,    32,    33,    34,    35,    36,    37,    38,
      46,     3,     4,    79,    10,   128,    43,    47,    42,    11,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      28,     2,    10,    40,     3,     4,    67,    11,     5,     6,
       3,     4,     7,     3,     4,    68,    93,    94,    64,     8,
       9,    35,    36,    37,    38,    10,    74,    47,    78,    80,
      11,    10,    82,    84,    10,    69,    11,    73,    12,    11,
      82,    81,     3,     4,    75,    92,    95,    95,   107,    99,
       3,     4,     3,     4,    37,    38,   110,    76,   134,    87,
     108,   109,   111,    10,     3,     4,     3,     4,    11,   101,
     150,    10,   171,    10,    91,    98,    11,   102,    11,   130,
     131,   132,   133,   135,   104,    10,   105,    10,   136,   106,
      11,   120,    11,   123,    85,   142,   160,   145,   146,   147,
     148,   149,   151,   152,   125,   163,    32,    33,    34,    35,
      36,    37,    38,   164,   165,   166,   167,   168,   169,   170,
     172,   173,   144,   152,    32,    33,    34,    35,    36,    37,
      38,    32,    33,    34,    35,    36,    37,    38,    77,    41,
      33,    34,    35,    36,    37,    38,   162,    32,    33,    34,
      35,    36,    37,    38,    34,    35,    36,    37,    38,    63,
      39,    32,    33,    34,    35,    36,    37,    38,    89,   126,
       0,    70,    32,    33,    34,    35,    36,    37,    38,   174,
      97,     0,    86,    32,    33,    34,    35,    36,    37,    38,
       0,     0,     0,    90,    32,    33,    34,    35,    36,    37,
      38,     0,     0,     0,   103,    32,    33,    34,    35,    36,
      37,    38,     0,     0,     0,   121,    32,    33,    34,    35,
      36,    37,    38,     0,     0,     0,   122,    32,    33,    34,
      35,    36,    37,    38,     0,     0,     0,   124,    32,    33,
      34,    35,    36,    37,    38,     0,     0,     0,   138,    32,
      33,    34,    35,    36,    37,    38,     0,     0,     0,   139,
      32,    33,    34,    35,    36,    37,    38,     0,     0,     0,
     140,    32,    33,    34,    35,    36,    37,    38,     0,     0,
       0,   141,    32,    33,    34,    35,    36,    37,    38,     0,
       0,     0,   143,    32,    33,    34,    35,    36,    37,    38,
       0,     0,     0,   155,    32,    33,    34,    35,    36,    37,
      38,     0,     0,     0,   156,    32,    33,    34,    35,    36,
      37,    38,     0,     0,     0,   157,    32,    33,    34,    35,
      36,    37,    38,     0,     0,     0,   158,    32,    33,    34,
      35,    36,    37,    38,     0,     0,     0,   159,    32,    33,
      34,    35,    36,    37,    38,     0,     0,     0,   161,    32,
      33,    34,    35,    36,    37,    38,     0,     0,    50,    32,
      33,    34,    35,    36,    37,    38,     0,     0,    71,    32,
      33,    34,    35,    36,    37,    38,     0,     0,   100,    32,
      33,    34,    35,    36,    37,    38,     0,    88,    32,    33,
      34,    35,    36,    37,    38,   112,   113,   114,   115,   116
};

static const short int yycheck[] =
{
       1,     4,     5,     3,     4,    32,     6,     5,    17,    10,
      11,    12,     6,    21,    22,    23,    24,    25,    26,    27,
      20,     3,     4,     5,    24,    34,    31,    28,    35,    29,
      38,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      29,     0,    24,    29,     3,     4,    30,    29,     7,     8,
       3,     4,    11,     3,     4,    31,     9,    10,     5,    18,
      19,    24,    25,    26,    27,    24,    34,    68,    69,    70,
      29,    24,    73,    74,    24,    29,    29,    29,    37,    29,
      81,    29,     3,     4,    33,    86,    87,    88,     9,    90,
       3,     4,     3,     4,    26,    27,     9,    31,     9,    29,
     101,   102,   103,    24,     3,     4,     3,     4,    29,    31,
       9,    24,     9,    24,    30,    30,    29,    31,    29,   120,
     121,   122,   123,   124,    30,    24,    30,    24,   129,    32,
      29,    31,    29,    31,    76,    31,    31,   138,   139,   140,
     141,   142,   143,   144,    33,    38,    21,    22,    23,    24,
      25,    26,    27,    31,   155,   156,   157,   158,   159,   160,
     161,   162,    37,   164,    21,    22,    23,    24,    25,    26,
      27,    21,    22,    23,    24,    25,    26,    27,    68,    36,
      22,    23,    24,    25,    26,    27,    36,    21,    22,    23,
      24,    25,    26,    27,    23,    24,    25,    26,    27,    43,
      34,    21,    22,    23,    24,    25,    26,    27,    81,   118,
      -1,    31,    21,    22,    23,    24,    25,    26,    27,   164,
      88,    -1,    31,    21,    22,    23,    24,    25,    26,    27,
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
      23,    24,    25,    26,    27,    -1,    -1,    -1,    31,    21,
      22,    23,    24,    25,    26,    27,    -1,    -1,    30,    21,
      22,    23,    24,    25,    26,    27,    -1,    -1,    30,    21,
      22,    23,    24,    25,    26,    27,    -1,    -1,    30,    21,
      22,    23,    24,    25,    26,    27,    -1,    29,    21,    22,
      23,    24,    25,    26,    27,    12,    13,    14,    15,    16
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,    41,     0,     3,     4,     7,     8,    11,    18,    19,
      24,    29,    37,    40,    42,    43,    46,    47,    48,    52,
      57,    58,     5,     6,    40,    53,    54,    32,    29,    40,
      40,    40,    21,    22,    23,    24,    25,    26,    27,    34,
      29,    36,    35,    31,    49,     6,    20,    40,    44,    45,
      30,    38,    40,    40,    40,    40,    40,    40,    40,    40,
      40,     4,     5,    53,     5,    50,    51,    30,    31,    29,
      31,    30,    55,    29,    34,    33,    31,    44,    40,     5,
      40,    29,    40,    56,    40,    50,    31,    29,    29,    56,
      31,    30,    40,     9,    10,    40,    66,    66,    30,    40,
      30,    31,    31,    31,    30,    30,    32,     9,    40,    40,
       9,    40,    12,    13,    14,    15,    16,    59,    60,    61,
      31,    31,    31,    31,    31,    33,    59,    17,    34,    62,
      40,    40,    40,    40,     9,    40,    40,    63,    31,    31,
      31,    31,    31,    31,    37,    40,    40,    40,    40,    40,
       9,    40,    40,    64,    65,    31,    31,    31,    31,    31,
      31,    31,    36,    38,    31,    40,    40,    40,    40,    40,
      40,     9,    40,    40,    64
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
#line 117 "xlat-parse.y"
    { (yyval.val) = (yyvsp[0].symval)->Value; ;}
    break;

  case 4:
#line 118 "xlat-parse.y"
    { (yyval.val) = (yyvsp[-2].val) + (yyvsp[0].val); ;}
    break;

  case 5:
#line 119 "xlat-parse.y"
    { (yyval.val) = (yyvsp[-2].val) - (yyvsp[0].val); ;}
    break;

  case 6:
#line 120 "xlat-parse.y"
    { (yyval.val) = (yyvsp[-2].val) * (yyvsp[0].val); ;}
    break;

  case 7:
#line 121 "xlat-parse.y"
    { (yyval.val) = (yyvsp[-2].val) / (yyvsp[0].val); ;}
    break;

  case 8:
#line 122 "xlat-parse.y"
    { (yyval.val) = (yyvsp[-2].val) | (yyvsp[0].val); ;}
    break;

  case 9:
#line 123 "xlat-parse.y"
    { (yyval.val) = (yyvsp[-2].val) & (yyvsp[0].val); ;}
    break;

  case 10:
#line 124 "xlat-parse.y"
    { (yyval.val) = (yyvsp[-2].val) ^ (yyvsp[0].val); ;}
    break;

  case 11:
#line 125 "xlat-parse.y"
    { (yyval.val) = -(yyvsp[0].val) ;}
    break;

  case 12:
#line 126 "xlat-parse.y"
    { (yyval.val) = (yyvsp[-1].val); ;}
    break;

  case 22:
#line 145 "xlat-parse.y"
    {
			  printf ("\n");
		  ;}
    break;

  case 26:
#line 157 "xlat-parse.y"
    { printf ("%s", (yyvsp[0].string)); ;}
    break;

  case 27:
#line 158 "xlat-parse.y"
    { printf ("%d", (yyvsp[0].val)); ;}
    break;

  case 28:
#line 159 "xlat-parse.y"
    { printf ("\n"); ;}
    break;

  case 29:
#line 163 "xlat-parse.y"
    { AddSym ((yyvsp[-3].sym), (yyvsp[-1].val)); ;}
    break;

  case 30:
#line 167 "xlat-parse.y"
    { IncludeFile ((yyvsp[0].string)); ;}
    break;

  case 31:
#line 171 "xlat-parse.y"
    { EnumVal = 0; ;}
    break;

  case 36:
#line 181 "xlat-parse.y"
    { AddSym ((yyvsp[0].sym), EnumVal++); ;}
    break;

  case 37:
#line 182 "xlat-parse.y"
    { AddSym ((yyvsp[-2].sym), EnumVal = (yyvsp[0].val)); ;}
    break;

  case 42:
#line 199 "xlat-parse.y"
    { AddSym ((yyvsp[-3].sym), (yyvsp[-5].val)); ;}
    break;

  case 43:
#line 201 "xlat-parse.y"
    { printf ("%s, line %d: %s is already defined\n", SourceName, SourceLine, (yyvsp[0].symval)->Sym); ;}
    break;

  case 45:
#line 206 "xlat-parse.y"
    { (yyval.val) = 0; ;}
    break;

  case 48:
#line 213 "xlat-parse.y"
    {
			  Simple[(yyvsp[-7].val)].NewSpecial = (yyvsp[-3].val);
			  Simple[(yyvsp[-7].val)].Flags = (yyvsp[-5].val) | (yyvsp[-1].args).addflags;
			  Simple[(yyvsp[-7].val)].Args[0] = (yyvsp[-1].args).args[0];
			  Simple[(yyvsp[-7].val)].Args[1] = (yyvsp[-1].args).args[1];
			  Simple[(yyvsp[-7].val)].Args[2] = (yyvsp[-1].args).args[2];
			  Simple[(yyvsp[-7].val)].Args[3] = (yyvsp[-1].args).args[3];
			  Simple[(yyvsp[-7].val)].Args[4] = (yyvsp[-1].args).args[4];
		  ;}
    break;

  case 49:
#line 223 "xlat-parse.y"
    {
			printf ("%s, line %d: %s is undefined\n", SourceName, SourceLine, (yyvsp[-3].sym));
		;}
    break;

  case 50:
#line 230 "xlat-parse.y"
    {
			if (NumBoomish == MAX_BOOMISH)
			{
				MoreLines *probe = (yyvsp[-1].boomlines);

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

				Boomish[NumBoomish].FirstLinetype = (yyvsp[-6].val);
				Boomish[NumBoomish].LastLinetype = (yyvsp[-4].val);
				Boomish[NumBoomish].NewSpecial = (yyvsp[-9].val);
				
				for (i = 0, probe = (yyvsp[-1].boomlines); probe != NULL; i++)
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
		;}
    break;

  case 51:
#line 277 "xlat-parse.y"
    {
			(yyval.boomlines) = NULL;
		;}
    break;

  case 52:
#line 281 "xlat-parse.y"
    {
			(yyval.boomlines) = malloc (sizeof(MoreLines));
			(yyval.boomlines)->next = (yyvsp[0].boomlines);
			(yyval.boomlines)->arg = (yyvsp[-1].boomline);
		;}
    break;

  case 53:
#line 290 "xlat-parse.y"
    {
			(yyval.boomline).bDefined = 1;
			(yyval.boomline).bOrExisting = ((yyvsp[-1].val) == OR_EQUAL);
			(yyval.boomline).bUseConstant = ((yyvsp[0].boomarg).filters == NULL);
			(yyval.boomline).ArgNum = (yyvsp[-2].val);
			(yyval.boomline).ConstantValue = (yyvsp[0].boomarg).constant;
			(yyval.boomline).AndValue = (yyvsp[0].boomarg).mask;

			if ((yyvsp[0].boomarg).filters != NULL)
			{
				int i;
				MoreFilters *probe;

				for (i = 0, probe = (yyvsp[0].boomarg).filters; probe != NULL; i++)
				{
					MoreFilters *next = probe->next;
					if (i < 15)
					{
						(yyval.boomline).ResultFilter[i] = probe->filter.filter;
						(yyval.boomline).ResultValue[i] = probe->filter.value;
					}
					else if (i == 15)
					{
						yyerror ("Lists can only have 15 elements");
					}
					free (probe);
					probe = next;
				}
				(yyval.boomline).ListSize = i > 15 ? 15 : i;
			}
		;}
    break;

  case 54:
#line 324 "xlat-parse.y"
    { (yyval.val) = 4; ;}
    break;

  case 55:
#line 325 "xlat-parse.y"
    { (yyval.val) = 0; ;}
    break;

  case 56:
#line 326 "xlat-parse.y"
    { (yyval.val) = 1; ;}
    break;

  case 57:
#line 327 "xlat-parse.y"
    { (yyval.val) = 2; ;}
    break;

  case 58:
#line 328 "xlat-parse.y"
    { (yyval.val) = 3; ;}
    break;

  case 59:
#line 332 "xlat-parse.y"
    { (yyval.val) = '='; ;}
    break;

  case 60:
#line 333 "xlat-parse.y"
    { (yyval.val) = OR_EQUAL; ;}
    break;

  case 61:
#line 338 "xlat-parse.y"
    {
			  (yyval.boomarg).constant = (yyvsp[0].val);
			  (yyval.boomarg).filters = NULL;
		  ;}
    break;

  case 62:
#line 343 "xlat-parse.y"
    {
			(yyval.boomarg).mask = (yyvsp[-3].val);
			(yyval.boomarg).filters = (yyvsp[-1].filter_p);
		;}
    break;

  case 63:
#line 351 "xlat-parse.y"
    {
			  (yyval.filter_p) = malloc (sizeof (MoreFilters));
			  (yyval.filter_p)->next = NULL;
			  (yyval.filter_p)->filter = (yyvsp[0].filter);
		  ;}
    break;

  case 64:
#line 357 "xlat-parse.y"
    {
			(yyval.filter_p) = malloc (sizeof (MoreFilters));
			(yyval.filter_p)->next = (yyvsp[0].filter_p);
			(yyval.filter_p)->filter = (yyvsp[-2].filter);
		;}
    break;

  case 65:
#line 365 "xlat-parse.y"
    { (yyval.filter).filter = (yyvsp[-2].val); (yyval.filter).value = (yyvsp[0].val); ;}
    break;

  case 66:
#line 370 "xlat-parse.y"
    {
			(yyval.args).addflags = 0;
			memset ((yyval.args).args, 0, 5);
		;}
    break;

  case 67:
#line 375 "xlat-parse.y"
    {
			(yyval.args).addflags = SIMPLE_HASTAGAT1;
			memset ((yyval.args).args, 0, 5);
		;}
    break;

  case 68:
#line 380 "xlat-parse.y"
    {
			(yyval.args).addflags = SIMPLE_HASTAGAT1;
			(yyval.args).args[0] = 0;
			(yyval.args).args[1] = (yyvsp[0].val);
			(yyval.args).args[2] = 0;
			(yyval.args).args[3] = 0;
			(yyval.args).args[4] = 0;
		;}
    break;

  case 69:
#line 389 "xlat-parse.y"
    {
			(yyval.args).addflags = SIMPLE_HASTAGAT1;
			(yyval.args).args[0] = 0;
			(yyval.args).args[1] = (yyvsp[-2].val);
			(yyval.args).args[2] = (yyvsp[0].val);
			(yyval.args).args[3] = 0;
			(yyval.args).args[4] = 0;
		;}
    break;

  case 70:
#line 398 "xlat-parse.y"
    {
			(yyval.args).addflags = SIMPLE_HASTAGAT1;
			(yyval.args).args[0] = 0;
			(yyval.args).args[1] = (yyvsp[-4].val);
			(yyval.args).args[2] = (yyvsp[-2].val);
			(yyval.args).args[3] = (yyvsp[0].val);
			(yyval.args).args[4] = 0;
		;}
    break;

  case 71:
#line 407 "xlat-parse.y"
    {
			(yyval.args).addflags = SIMPLE_HASTAGAT1;
			(yyval.args).args[0] = 0;
			(yyval.args).args[1] = (yyvsp[-6].val);
			(yyval.args).args[2] = (yyvsp[-4].val);
			(yyval.args).args[3] = (yyvsp[-2].val);
			(yyval.args).args[4] = (yyvsp[0].val);
		;}
    break;

  case 72:
#line 417 "xlat-parse.y"
    {
			(yyval.args).addflags = SIMPLE_HAS2TAGS;
			(yyval.args).args[0] = (yyval.args).args[1] = 0;
			(yyval.args).args[2] = 0;
			(yyval.args).args[3] = 0;
			(yyval.args).args[4] = 0;
		;}
    break;

  case 73:
#line 425 "xlat-parse.y"
    {
			(yyval.args).addflags = SIMPLE_HAS2TAGS;
			(yyval.args).args[0] = (yyval.args).args[1] = 0;
			(yyval.args).args[2] = (yyvsp[0].val);
			(yyval.args).args[3] = 0;
			(yyval.args).args[4] = 0;
		;}
    break;

  case 74:
#line 433 "xlat-parse.y"
    {
			(yyval.args).addflags = SIMPLE_HAS2TAGS;
			(yyval.args).args[0] = (yyval.args).args[1] = 0;
			(yyval.args).args[2] = (yyvsp[-2].val);
			(yyval.args).args[3] = (yyvsp[0].val);
			(yyval.args).args[4] = 0;
		;}
    break;

  case 75:
#line 441 "xlat-parse.y"
    {
			(yyval.args).addflags = SIMPLE_HAS2TAGS;
			(yyval.args).args[0] = (yyval.args).args[1] = 0;
			(yyval.args).args[2] = (yyvsp[-4].val);
			(yyval.args).args[3] = (yyvsp[-2].val);
			(yyval.args).args[4] = (yyvsp[0].val);
		;}
    break;

  case 76:
#line 450 "xlat-parse.y"
    {
			(yyval.args).addflags = SIMPLE_HASLINEID;
			memset ((yyval.args).args, 0, 5);
		;}
    break;

  case 77:
#line 455 "xlat-parse.y"
    {
			(yyval.args).addflags = SIMPLE_HASLINEID;
			(yyval.args).args[0] = 0;
			(yyval.args).args[1] = (yyvsp[0].val);
			(yyval.args).args[2] = 0;
			(yyval.args).args[3] = 0;
			(yyval.args).args[4] = 0;
		;}
    break;

  case 78:
#line 464 "xlat-parse.y"
    {
			(yyval.args).addflags = SIMPLE_HASLINEID;
			(yyval.args).args[0] = 0;
			(yyval.args).args[1] = (yyvsp[-2].val);
			(yyval.args).args[2] = (yyvsp[0].val);
			(yyval.args).args[3] = 0;
			(yyval.args).args[4] = 0;
		;}
    break;

  case 79:
#line 473 "xlat-parse.y"
    {
			(yyval.args).addflags = SIMPLE_HASLINEID;
			(yyval.args).args[0] = 0;
			(yyval.args).args[1] = (yyvsp[-4].val);
			(yyval.args).args[2] = (yyvsp[-2].val);
			(yyval.args).args[3] = (yyvsp[0].val);
			(yyval.args).args[4] = 0;
		;}
    break;

  case 80:
#line 482 "xlat-parse.y"
    {
			(yyval.args).addflags = SIMPLE_HASLINEID;
			(yyval.args).args[0] = 0;
			(yyval.args).args[1] = (yyvsp[-6].val);
			(yyval.args).args[2] = (yyvsp[-4].val);
			(yyval.args).args[3] = (yyvsp[-2].val);
			(yyval.args).args[4] = (yyvsp[0].val);
		;}
    break;

  case 81:
#line 492 "xlat-parse.y"
    {
			(yyval.args).addflags = 0;
			(yyval.args).args[0] = (yyvsp[0].val);
			(yyval.args).args[1] = 0;
			(yyval.args).args[2] = 0;
			(yyval.args).args[3] = 0;
			(yyval.args).args[4] = 0;
		;}
    break;

  case 82:
#line 501 "xlat-parse.y"
    {
			(yyval.args).addflags = 0;
			(yyval.args).args[0] = (yyvsp[-2].val);
			(yyval.args).args[1] = (yyvsp[0].val);
			(yyval.args).args[2] = 0;
			(yyval.args).args[3] = 0;
			(yyval.args).args[4] = 0;
		;}
    break;

  case 83:
#line 510 "xlat-parse.y"
    {
			(yyval.args).addflags = 0;
			(yyval.args).args[0] = (yyvsp[-4].val);
			(yyval.args).args[1] = (yyvsp[-2].val);
			(yyval.args).args[2] = (yyvsp[0].val);
			(yyval.args).args[3] = 0;
			(yyval.args).args[4] = 0;
		;}
    break;

  case 84:
#line 519 "xlat-parse.y"
    {
			(yyval.args).addflags = 0;
			(yyval.args).args[0] = (yyvsp[-6].val);
			(yyval.args).args[1] = (yyvsp[-4].val);
			(yyval.args).args[2] = (yyvsp[-2].val);
			(yyval.args).args[3] = (yyvsp[0].val);
			(yyval.args).args[4] = 0;
		;}
    break;

  case 85:
#line 528 "xlat-parse.y"
    {
			(yyval.args).addflags = 0;
			(yyval.args).args[0] = (yyvsp[-8].val);
			(yyval.args).args[1] = (yyvsp[-6].val);
			(yyval.args).args[2] = (yyvsp[-4].val);
			(yyval.args).args[3] = (yyvsp[-2].val);
			(yyval.args).args[4] = (yyvsp[0].val);
		;}
    break;

  case 86:
#line 538 "xlat-parse.y"
    {
			(yyval.args).addflags = SIMPLE_HASTAGAT2;
			(yyval.args).args[0] = (yyvsp[-2].val);
			(yyval.args).args[1] = 0;
			(yyval.args).args[2] = 0;
			(yyval.args).args[3] = 0;
			(yyval.args).args[4] = 0;
		;}
    break;

  case 87:
#line 547 "xlat-parse.y"
    {
			(yyval.args).addflags = SIMPLE_HASTAGAT2;
			(yyval.args).args[0] = (yyvsp[-4].val);
			(yyval.args).args[1] = 0;
			(yyval.args).args[2] = (yyvsp[0].val);
			(yyval.args).args[3] = 0;
			(yyval.args).args[4] = 0;
		;}
    break;

  case 88:
#line 556 "xlat-parse.y"
    {
			(yyval.args).addflags = SIMPLE_HASTAGAT2;
			(yyval.args).args[0] = (yyvsp[-6].val);
			(yyval.args).args[1] = 0;
			(yyval.args).args[2] = (yyvsp[-2].val);
			(yyval.args).args[3] = (yyvsp[0].val);
			(yyval.args).args[4] = 0;
		;}
    break;

  case 89:
#line 565 "xlat-parse.y"
    {
			(yyval.args).addflags = SIMPLE_HASTAGAT2;
			(yyval.args).args[0] = (yyvsp[-8].val);
			(yyval.args).args[1] = 0;
			(yyval.args).args[2] = (yyvsp[-4].val);
			(yyval.args).args[3] = (yyvsp[-2].val);
			(yyval.args).args[4] = (yyvsp[0].val);
		;}
    break;

  case 90:
#line 575 "xlat-parse.y"
    {
			(yyval.args).addflags = SIMPLE_HASTAGAT3;
			(yyval.args).args[0] = (yyvsp[-4].val);
			(yyval.args).args[1] = (yyvsp[-2].val);
			(yyval.args).args[2] = 0;
			(yyval.args).args[3] = 0;
			(yyval.args).args[4] = 0;
		;}
    break;

  case 91:
#line 584 "xlat-parse.y"
    {
			(yyval.args).addflags = SIMPLE_HASTAGAT3;
			(yyval.args).args[0] = (yyvsp[-6].val);
			(yyval.args).args[1] = (yyvsp[-4].val);
			(yyval.args).args[2] = 0;
			(yyval.args).args[3] = (yyvsp[0].val);
			(yyval.args).args[4] = 0;
		;}
    break;

  case 92:
#line 593 "xlat-parse.y"
    {
			(yyval.args).addflags = SIMPLE_HASTAGAT3;
			(yyval.args).args[0] = (yyvsp[-8].val);
			(yyval.args).args[1] = (yyvsp[-6].val);
			(yyval.args).args[2] = 0;
			(yyval.args).args[3] = (yyvsp[-2].val);
			(yyval.args).args[4] = (yyvsp[0].val);
		;}
    break;

  case 93:
#line 603 "xlat-parse.y"
    {
			(yyval.args).addflags = SIMPLE_HASTAGAT4;
			(yyval.args).args[0] = (yyvsp[-6].val);
			(yyval.args).args[1] = (yyvsp[-4].val);
			(yyval.args).args[2] = (yyvsp[-2].val);
			(yyval.args).args[3] = 0;
			(yyval.args).args[4] = 0;
		;}
    break;

  case 94:
#line 612 "xlat-parse.y"
    {
			(yyval.args).addflags = SIMPLE_HASTAGAT4;
			(yyval.args).args[0] = (yyvsp[-8].val);
			(yyval.args).args[1] = (yyvsp[-6].val);
			(yyval.args).args[2] = (yyvsp[-4].val);
			(yyval.args).args[3] = 0;
			(yyval.args).args[4] = (yyvsp[0].val);
		;}
    break;

  case 95:
#line 622 "xlat-parse.y"
    {
			(yyval.args).addflags = SIMPLE_HASTAGAT5;
			(yyval.args).args[0] = (yyvsp[-8].val);
			(yyval.args).args[1] = (yyvsp[-6].val);
			(yyval.args).args[2] = (yyvsp[-4].val);
			(yyval.args).args[3] = (yyvsp[-2].val);
			(yyval.args).args[4] = 0;
		;}
    break;


      default: break;
    }

/* Line 1126 of yacc.c.  */
#line 2059 "xlat-parse.tab.c"

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



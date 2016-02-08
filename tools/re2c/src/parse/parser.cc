/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.0.4"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* Copy the first part of user declarations.  */
#line 1 "../src/parse/parser.ypp" /* yacc.c:339  */


#include "src/util/c99_stdint.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <limits>
#include <list>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "src/codegen/output.h"
#include "src/conf/opt.h"
#include "src/globals.h"
#include "src/ir/compile.h"
#include "src/ir/adfa/adfa.h"
#include "src/ir/regexp/encoding/enc.h"
#include "src/ir/regexp/encoding/range_suffix.h"
#include "src/ir/regexp/regexp.h"
#include "src/ir/regexp/regexp_cat.h"
#include "src/ir/regexp/regexp_close.h"
#include "src/ir/regexp/regexp_null.h"
#include "src/ir/regexp/regexp_rule.h"
#include "src/ir/rule_rank.h"
#include "src/ir/skeleton/skeleton.h"
#include "src/parse/code.h"
#include "src/parse/extop.h"
#include "src/parse/loc.h"
#include "src/parse/parser.h"
#include "src/parse/scanner.h"
#include "src/parse/spec.h"
#include "src/util/counter.h"
#include "src/util/free_list.h"
#include "src/util/range.h"
#include "src/util/smart_ptr.h"

#define YYMALLOC malloc
#define YYFREE free

using namespace re2c;

extern "C"
{
int yylex();
void yyerror(const char*);
}

static counter_t<rule_rank_t> rank_counter;
static std::vector<std::string> condnames;
static re2c::SpecMap  specMap;
static Spec spec;
static RuleOp *specNone = NULL;
static RuleOpList       specStar;
static RuleOp * star_default = NULL;
static Scanner          *in = NULL;
static Scanner::ParseMode  parseMode;
static SetupMap            ruleSetupMap;
static bool                foundRules;
static symbol_table_t symbol_table;

/* Bison version 1.875 emits a definition that is not working
 * with several g++ version. Hence we disable it here.
 */
#if defined(__GNUC__)
#define __attribute__(x)
#endif

void context_check(CondList *clist)
{
	if (!opts->cFlag)
	{
		delete clist;
		in->fatal("conditions are only allowed when using -c switch");
	}
}

void context_none(CondList *clist)
{
	delete clist;
	context_check(NULL);
	in->fatal("no expression specified");
}

void context_rule
	( CondList * clist
	, const Loc & loc
	, RegExp * expr
	, RegExp * look
	, const Code * code
	, const std::string * newcond
	)
{
	context_check(clist);
	for(CondList::const_iterator it = clist->begin(); it != clist->end(); ++it)
	{
		if (specMap.find(*it) == specMap.end())
		{
			condnames.push_back (*it);
		}

		RuleOp * rule = new RuleOp
			( loc
			, expr
			, look
			, rank_counter.next ()
			, code
			, newcond
			);
		specMap[*it].add (rule);
	}
	delete clist;
	delete newcond;
}

void setup_rule(CondList *clist, const Code * code)
{
	assert(clist);
	assert(code);
	context_check(clist);
	for(CondList::const_iterator it = clist->begin(); it != clist->end(); ++it)
	{
		if (ruleSetupMap.find(*it) != ruleSetupMap.end())
		{
			in->fatalf_at(code->loc.line, "code to setup rule '%s' is already defined", it->c_str());
		}
		ruleSetupMap[*it] = std::make_pair(code->loc.line, code->text);
	}
	delete clist;
}

void default_rule(CondList *clist, const Code * code)
{
	assert(clist);
	assert(code);
	context_check(clist);
	for(CondList::const_iterator it = clist->begin(); it != clist->end(); ++it)
	{
		RuleOp * def = new RuleOp
			( code->loc
			, in->mkDefault ()
			, new NullOp
			, rule_rank_t::def ()
			, code
			, NULL
			);
		if (!specMap[*it].add_def (def))
		{
			in->fatalf_at(code->loc.line, "code to default rule '%s' is already defined", it->c_str());
		}
	}
	delete clist;
}


#line 224 "src/parse/parser.cc" /* yacc.c:339  */

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* In a future release of Bison, this section will be replaced
   by #include "y.tab.h".  */
#ifndef YY_YY_SRC_PARSE_Y_TAB_H_INCLUDED
# define YY_YY_SRC_PARSE_Y_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    TOKEN_CLOSE = 258,
    TOKEN_CLOSESIZE = 259,
    TOKEN_CODE = 260,
    TOKEN_CONF = 261,
    TOKEN_ID = 262,
    TOKEN_FID = 263,
    TOKEN_FID_END = 264,
    TOKEN_NOCOND = 265,
    TOKEN_REGEXP = 266,
    TOKEN_SETUP = 267,
    TOKEN_STAR = 268
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 161 "../src/parse/parser.ypp" /* yacc.c:355  */

	re2c::RegExp * regexp;
	const re2c::Code * code;
	char op;
	re2c::ExtOp extop;
	std::string * str;
	re2c::CondList * clist;

#line 287 "src/parse/parser.cc" /* yacc.c:355  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_SRC_PARSE_Y_TAB_H_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 304 "src/parse/parser.cc" /* yacc.c:358  */

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

#if !defined _Noreturn \
     && (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112)
# if defined _MSC_VER && 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn YY_ATTRIBUTE ((__noreturn__))
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif


#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  2
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   104

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  25
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  14
/* YYNRULES -- Number of rules.  */
#define YYNRULES  49
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  92

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   268

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      23,    24,     2,     2,    20,     2,     2,    16,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    19,    15,
      17,    14,    18,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,    22,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    21,     2,     2,     2,     2,     2,
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
       5,     6,     7,     8,     9,    10,    11,    12,    13
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   193,   193,   195,   199,   203,   211,   219,   223,   227,
     231,   247,   264,   268,   274,   279,   285,   289,   303,   319,
     324,   330,   345,   362,   381,   387,   395,   398,   405,   411,
     421,   424,   432,   435,   442,   446,   453,   457,   464,   468,
     475,   479,   494,   513,   517,   521,   525,   532,   542,   546
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "TOKEN_CLOSE", "TOKEN_CLOSESIZE",
  "TOKEN_CODE", "TOKEN_CONF", "TOKEN_ID", "TOKEN_FID", "TOKEN_FID_END",
  "TOKEN_NOCOND", "TOKEN_REGEXP", "TOKEN_SETUP", "TOKEN_STAR", "'='",
  "';'", "'/'", "'<'", "'>'", "':'", "','", "'|'", "'\\\\'", "'('", "')'",
  "$accept", "spec", "decl", "rule", "cond", "clist", "newcond", "look",
  "expr", "diff", "term", "factor", "close", "primary", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,    61,    59,    47,    60,    62,    58,
      44,   124,    92,    40,    41
};
# endif

#define YYPACT_NINF -43

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-43)))

#define YYTABLE_NINF -1

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int8 yypact[] =
{
     -43,    11,   -43,   -43,   -11,    30,    47,   -43,    25,    10,
      33,    30,   -43,   -43,    48,    17,    30,   -43,     1,    30,
     -43,     4,    40,    60,    70,   -43,    61,    63,    42,   -43,
      64,    66,    59,    30,    30,    73,    30,   -43,   -43,   -43,
     -43,    32,    -9,   -43,   -43,    78,   -43,   -43,    81,    82,
      83,    20,    44,   -43,    67,    17,   -43,    30,   -43,   -43,
     -43,   -43,   -43,   -43,   -43,   -43,    84,    51,    48,    86,
      54,    48,   -43,    60,    87,    57,   -43,    60,    88,    58,
     -43,   -43,    60,    89,   -43,   -43,    60,    90,   -43,   -43,
     -43,   -43
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       2,     0,     1,     9,    47,     0,    30,    48,    26,     0,
      26,     0,     4,     3,    32,    34,    36,    38,    40,     0,
      47,     0,     0,    30,     0,    28,     0,     0,    27,    11,
       0,     0,     0,     0,     0,     0,     0,    39,    43,    42,
      44,    41,     0,     6,     8,     0,    23,    22,     0,     0,
       0,    32,    32,    49,    33,    35,    10,    37,    45,    46,
       5,     7,    31,    24,    25,    29,     0,    30,    32,     0,
      30,    32,    21,    30,     0,    30,    16,    30,     0,    30,
      20,    19,    30,     0,    15,    14,    30,     0,    18,    17,
      13,    12
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -43,   -43,   -43,   -43,    91,   -43,   -23,   -42,    -3,    62,
      68,   -15,   -43,   -43
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
      -1,     1,    12,    13,    27,    28,    24,    35,    14,    15,
      16,    17,    41,    18
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_uint8 yytable[] =
{
      46,    37,    21,    19,    38,    39,    60,    61,    32,    67,
      70,     2,    34,    43,    40,    29,    42,     3,     4,     5,
      44,     6,     7,     8,     9,    34,    75,    20,    10,    79,
      54,     7,    25,    66,    11,    58,    33,    20,    26,    36,
      25,     7,    37,    11,    74,    59,    30,    78,    68,    71,
      80,    20,    83,    11,    84,     7,    87,    69,    45,    88,
      33,    22,    50,    90,    33,    22,    23,    11,    22,    34,
      73,    22,    22,    77,    22,    47,    82,    86,    56,    48,
      34,    49,    51,    53,    52,    62,    63,    64,    34,    72,
      65,    76,    81,    85,    89,    91,    55,     0,     0,     0,
       0,    31,     0,     0,    57
};

static const yytype_int8 yycheck[] =
{
      23,    16,     5,    14,     3,     4,    15,    16,    11,    51,
      52,     0,    21,     9,    13,     5,    19,     6,     7,     8,
      16,    10,    11,    12,    13,    21,    68,     7,    17,    71,
      33,    11,     7,    13,    23,     3,    16,     7,    13,    22,
       7,    11,    57,    23,    67,    13,    13,    70,    51,    52,
      73,     7,    75,    23,    77,    11,    79,    13,    18,    82,
      16,    14,    20,    86,    16,    14,    19,    23,    14,    21,
      19,    14,    14,    19,    14,     5,    19,    19,     5,    18,
      21,    18,    18,    24,    18,     7,     5,     5,    21,     5,
       7,     5,     5,     5,     5,     5,    34,    -1,    -1,    -1,
      -1,    10,    -1,    -1,    36
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    26,     0,     6,     7,     8,    10,    11,    12,    13,
      17,    23,    27,    28,    33,    34,    35,    36,    38,    14,
       7,    33,    14,    19,    31,     7,    13,    29,    30,     5,
      13,    29,    33,    16,    21,    32,    22,    36,     3,     4,
      13,    37,    33,     9,    16,    18,    31,     5,    18,    18,
      20,    18,    18,    24,    33,    34,     5,    35,     3,    13,
      15,    16,     7,     5,     5,     7,    13,    32,    33,    13,
      32,    33,     5,    19,    31,    32,     5,    19,    31,    32,
      31,     5,    19,    31,    31,     5,    19,    31,    31,     5,
      31,     5
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    25,    26,    26,    26,    27,    27,    27,    27,    27,
      28,    28,    28,    28,    28,    28,    28,    28,    28,    28,
      28,    28,    28,    28,    28,    28,    29,    29,    30,    30,
      31,    31,    32,    32,    33,    33,    34,    34,    35,    35,
      36,    36,    36,    37,    37,    37,    37,    38,    38,    38
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     0,     2,     2,     4,     3,     4,     3,     1,
       3,     2,     7,     7,     6,     6,     5,     7,     7,     6,
       6,     5,     3,     3,     4,     4,     0,     1,     1,     3,
       0,     3,     0,     2,     1,     3,     1,     3,     1,     2,
       1,     2,     2,     1,     1,     2,     2,     1,     1,     3
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
    {                                                           \
      yychar = (Token);                                         \
      yylval = (Value);                                         \
      YYPOPSTACK (yylen);                                       \
      yystate = *yyssp;                                         \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    {                                                           \
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;                                                  \
    }                                                           \
while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule)
{
  unsigned long int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
                                              );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
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
#ifndef YYINITDEPTH
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
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
yystrlen (const char *yystr)
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
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
      YYSIZE_T yyn = 0;
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

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
{
  YYUSE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;


/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        YYSTYPE *yyvs1 = yyvs;
        yytype_int16 *yyss1 = yyss;

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
        yytype_int16 *yyss1 = yyss;
        union yyalloc *yyptr =
          (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
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

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex ();
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
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

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
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:
#line 193 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
		}
#line 1438 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 3:
#line 196 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			foundRules = true;
		}
#line 1446 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 5:
#line 204 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			if (!symbol_table.insert (std::make_pair (* (yyvsp[-3].str), (yyvsp[-1].regexp))).second)
			{
				in->fatal("sym already defined");
			}
			delete (yyvsp[-3].str);
		}
#line 1458 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 6:
#line 212 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			if (!symbol_table.insert (std::make_pair (* (yyvsp[-2].str), (yyvsp[-1].regexp))).second)
			{
				in->fatal("sym already defined");
			}
			delete (yyvsp[-2].str);
		}
#line 1470 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 7:
#line 220 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			in->fatal("trailing contexts are not allowed in named definitions");
		}
#line 1478 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 8:
#line 224 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			in->fatal("trailing contexts are not allowed in named definitions");
		}
#line 1486 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 9:
#line 227 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {}
#line 1492 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 10:
#line 232 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			if (opts->cFlag)
			{
				in->fatal("condition or '<*>' required when using -c switch");
			}
			RuleOp * rule = new RuleOp
				( (yyvsp[0].code)->loc
				, (yyvsp[-2].regexp)
				, (yyvsp[-1].regexp)
				, rank_counter.next ()
				, (yyvsp[0].code)
				, NULL
				);
			spec.add (rule);
		}
#line 1512 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 11:
#line 248 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			if (opts->cFlag)
				in->fatal("condition or '<*>' required when using -c switch");
			RuleOp * def = new RuleOp
				( (yyvsp[0].code)->loc
				, in->mkDefault ()
				, new NullOp
				, rule_rank_t::def ()
				, (yyvsp[0].code)
				, NULL
				);
			if (!spec.add_def (def))
			{
				in->fatal("code to default rule is already defined");
			}
		}
#line 1533 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 12:
#line 265 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			context_rule ((yyvsp[-5].clist), (yyvsp[0].code)->loc, (yyvsp[-3].regexp), (yyvsp[-2].regexp), (yyvsp[0].code), (yyvsp[-1].str));
		}
#line 1541 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 13:
#line 269 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			assert((yyvsp[0].str));
			Loc loc (in->get_fname (), in->get_cline ());
			context_rule ((yyvsp[-5].clist), loc, (yyvsp[-3].regexp), (yyvsp[-2].regexp), NULL, (yyvsp[0].str));
		}
#line 1551 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 14:
#line 275 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			context_none((yyvsp[-4].clist));
			delete (yyvsp[-1].str);
		}
#line 1560 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 15:
#line 280 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			assert((yyvsp[0].str));
			context_none((yyvsp[-4].clist));
			delete (yyvsp[0].str);
		}
#line 1570 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 16:
#line 286 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			default_rule((yyvsp[-3].clist), (yyvsp[0].code));
		}
#line 1578 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 17:
#line 290 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			context_check(NULL);
			RuleOp * rule = new RuleOp
				( (yyvsp[0].code)->loc
				, (yyvsp[-3].regexp)
				, (yyvsp[-2].regexp)
				, rank_counter.next ()
				, (yyvsp[0].code)
				, (yyvsp[-1].str)
				);
			specStar.push_back (rule);
			delete (yyvsp[-1].str);
		}
#line 1596 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 18:
#line 304 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			assert((yyvsp[0].str));
			context_check(NULL);
			Loc loc (in->get_fname (), in->get_cline ());
			RuleOp * rule = new RuleOp
				( loc
				, (yyvsp[-3].regexp)
				, (yyvsp[-2].regexp)
				, rank_counter.next ()
				, NULL
				, (yyvsp[0].str)
				);
			specStar.push_back (rule);
			delete (yyvsp[0].str);
		}
#line 1616 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 19:
#line 320 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			context_none(NULL);
			delete (yyvsp[-1].str);
		}
#line 1625 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 20:
#line 325 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			assert((yyvsp[0].str));
			context_none(NULL);
			delete (yyvsp[0].str);
		}
#line 1635 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 21:
#line 331 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			if (star_default)
			{
				in->fatal ("code to default rule '*' is already defined");
			}
			star_default = new RuleOp
				( (yyvsp[0].code)->loc
				, in->mkDefault ()
				, new NullOp
				, rule_rank_t::def ()
				, (yyvsp[0].code)
				, NULL
				);
		}
#line 1654 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 22:
#line 346 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			context_check(NULL);
			if (specNone)
			{
				in->fatal("code to handle illegal condition already defined");
			}
			(yyval.regexp) = specNone = new RuleOp
				( (yyvsp[0].code)->loc
				, new NullOp
				, new NullOp
				, rank_counter.next ()
				, (yyvsp[0].code)
				, (yyvsp[-1].str)
				);
			delete (yyvsp[-1].str);
		}
#line 1675 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 23:
#line 363 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			assert((yyvsp[0].str));
			context_check(NULL);
			if (specNone)
			{
				in->fatal("code to handle illegal condition already defined");
			}
			Loc loc (in->get_fname (), in->get_cline ());
			(yyval.regexp) = specNone = new RuleOp
				( loc
				, new NullOp
				, new NullOp
				, rank_counter.next ()
				, NULL
				, (yyvsp[0].str)
				);
			delete (yyvsp[0].str);
		}
#line 1698 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 24:
#line 382 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			CondList *clist = new CondList();
			clist->insert("*");
			setup_rule(clist, (yyvsp[0].code));
		}
#line 1708 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 25:
#line 388 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			setup_rule((yyvsp[-2].clist), (yyvsp[0].code));
		}
#line 1716 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 26:
#line 395 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			in->fatal("unnamed condition not supported");
		}
#line 1724 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 27:
#line 399 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			(yyval.clist) = (yyvsp[0].clist);
		}
#line 1732 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 28:
#line 406 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			(yyval.clist) = new CondList();
			(yyval.clist)->insert(* (yyvsp[0].str));
			delete (yyvsp[0].str);
		}
#line 1742 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 29:
#line 412 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			(yyvsp[-2].clist)->insert(* (yyvsp[0].str));
			delete (yyvsp[0].str);
			(yyval.clist) = (yyvsp[-2].clist);
		}
#line 1752 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 30:
#line 421 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			(yyval.str) = NULL;
		}
#line 1760 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 31:
#line 425 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			(yyval.str) = (yyvsp[0].str);
		}
#line 1768 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 32:
#line 432 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			(yyval.regexp) = new NullOp;
		}
#line 1776 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 33:
#line 436 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			(yyval.regexp) = (yyvsp[0].regexp);
		}
#line 1784 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 34:
#line 443 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			(yyval.regexp) = (yyvsp[0].regexp);
		}
#line 1792 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 35:
#line 447 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			(yyval.regexp) = mkAlt((yyvsp[-2].regexp), (yyvsp[0].regexp));
		}
#line 1800 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 36:
#line 454 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			(yyval.regexp) = (yyvsp[0].regexp);
		}
#line 1808 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 37:
#line 458 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			(yyval.regexp) = in->mkDiff((yyvsp[-2].regexp), (yyvsp[0].regexp));
		}
#line 1816 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 38:
#line 465 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			(yyval.regexp) = (yyvsp[0].regexp);
		}
#line 1824 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 39:
#line 469 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			(yyval.regexp) = new CatOp((yyvsp[-1].regexp), (yyvsp[0].regexp));
		}
#line 1832 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 40:
#line 476 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			(yyval.regexp) = (yyvsp[0].regexp);
		}
#line 1840 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 41:
#line 480 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			switch((yyvsp[0].op))
			{
			case '*':
				(yyval.regexp) = new CloseOp((yyvsp[-1].regexp));
				break;
			case '+':
				(yyval.regexp) = new CatOp (new CloseOp((yyvsp[-1].regexp)), (yyvsp[-1].regexp));
				break;
			case '?':
				(yyval.regexp) = mkAlt((yyvsp[-1].regexp), new NullOp());
				break;
			}
		}
#line 1859 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 42:
#line 495 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			if ((yyvsp[0].extop).max == std::numeric_limits<uint32_t>::max())
			{
				(yyval.regexp) = repeat_from ((yyvsp[-1].regexp), (yyvsp[0].extop).min);
			}
			else if ((yyvsp[0].extop).min == (yyvsp[0].extop).max)
			{
				(yyval.regexp) = repeat ((yyvsp[-1].regexp), (yyvsp[0].extop).min);
			}
			else
			{
				(yyval.regexp) = repeat_from_to ((yyvsp[-1].regexp), (yyvsp[0].extop).min, (yyvsp[0].extop).max);
			}
			(yyval.regexp) = (yyval.regexp) ? (yyval.regexp) : new NullOp;
		}
#line 1879 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 43:
#line 514 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			(yyval.op) = (yyvsp[0].op);
		}
#line 1887 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 44:
#line 518 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			(yyval.op) = (yyvsp[0].op);
		}
#line 1895 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 45:
#line 522 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			(yyval.op) = ((yyvsp[-1].op) == (yyvsp[0].op)) ? (yyvsp[-1].op) : '*';
		}
#line 1903 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 46:
#line 526 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			(yyval.op) = ((yyvsp[-1].op) == (yyvsp[0].op)) ? (yyvsp[-1].op) : '*';
		}
#line 1911 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 47:
#line 533 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			symbol_table_t::iterator i = symbol_table.find (* (yyvsp[0].str));
			delete (yyvsp[0].str);
			if (i == symbol_table.end ())
			{
				in->fatal("can't find symbol");
			}
			(yyval.regexp) = i->second;
		}
#line 1925 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 48:
#line 543 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			(yyval.regexp) = (yyvsp[0].regexp);
		}
#line 1933 "src/parse/parser.cc" /* yacc.c:1646  */
    break;

  case 49:
#line 547 "../src/parse/parser.ypp" /* yacc.c:1646  */
    {
			(yyval.regexp) = (yyvsp[-1].regexp);
		}
#line 1941 "src/parse/parser.cc" /* yacc.c:1646  */
    break;


#line 1945 "src/parse/parser.cc" /* yacc.c:1646  */
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
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


      yydestruct ("Error: popping",
                  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
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

#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  return yyresult;
}
#line 552 "../src/parse/parser.ypp" /* yacc.c:1906  */


extern "C" {
void yyerror(const char* s)
{
	in->fatal(s);
}

int yylex(){
	return in ? in->scan() : 0;
}
} // end extern "C"

namespace re2c
{

void parse(Scanner& i, Output & o)
{
	std::map<std::string, smart_ptr<DFA> >  dfa_map;
	ScannerState rules_state;

	in = &i;

	o.source.wversion_time ()
		.wline_info (in->get_cline (), in->get_fname ().c_str ());
	if (opts->target == opt_t::SKELETON)
	{
		Skeleton::emit_prolog (o.source);
	}

	Enc encodingOld = opts->encoding;
	
	while ((parseMode = i.echo()) != Scanner::Stop)
	{
		o.source.new_block ();
		bool bPrologBrace = false;
		ScannerState curr_state;

		i.save_state(curr_state);
		foundRules = false;

		if (opts->rFlag && parseMode == Scanner::Rules && dfa_map.size())
		{
			in->fatal("cannot have a second 'rules:re2c' block");
		}
		if (parseMode == Scanner::Reuse)
		{
			if (dfa_map.empty())
			{
				in->fatal("got 'use:re2c' without 'rules:re2c'");
			}
		}
		else if (parseMode == Scanner::Rules)
		{
			i.save_state(rules_state);
		}
		else
		{
			dfa_map.clear();
		}
		rank_counter.reset ();
		spec.clear ();
		in->set_in_parse(true);
		yyparse();
		in->set_in_parse(false);
		if (opts->rFlag && parseMode == Scanner::Reuse)
		{
			if (foundRules || opts->encoding != encodingOld)
			{
				// Re-parse rules
				parseMode = Scanner::Parse;
				i.restore_state(rules_state);
				i.reuse();
				dfa_map.clear();
				parse_cleanup();
				spec.clear ();
				rank_counter.reset ();
				in->set_in_parse(true);
				yyparse();
				in->set_in_parse(false);

				// Now append potential new rules
				i.restore_state(curr_state);
				parseMode = Scanner::Parse;
				in->set_in_parse(true);
				yyparse();
				in->set_in_parse(false);
			}
			encodingOld = opts->encoding;
		}
		o.source.set_block_line (in->get_cline ());
		uint32_t ind = opts->topIndent;
		if (opts->cFlag)
		{
			SpecMap::iterator it;
			SetupMap::const_iterator itRuleSetup;

			if (parseMode != Scanner::Reuse)
			{
				// <*> rules must have the lowest priority
				// now that all rules have been parsed, we can fix it
				for (RuleOpList::const_iterator itOp = specStar.begin(); itOp != specStar.end(); ++itOp)
				{
					(*itOp)->rank = rank_counter.next ();
				}
				// merge <*> rules to all conditions
				for (it = specMap.begin(); it != specMap.end(); ++it)
				{
					for (RuleOpList::const_iterator itOp = specStar.begin(); itOp != specStar.end(); ++itOp)
					{
						it->second.add (*itOp);
					}
					if (star_default)
					{
						it->second.add_def (star_default);
					}
				}

				if (specNone)
				{
					specMap["0"].add (specNone);
					// Note that "0" inserts first, which is important.
					condnames.insert (condnames.begin (), "0");
				}
				o.types = condnames;
			}

			size_t nCount = specMap.size();

			for (it = specMap.begin(); it != specMap.end(); ++it)
			{
				if (parseMode != Scanner::Reuse)
				{
					itRuleSetup = ruleSetupMap.find(it->first);				
					if (itRuleSetup != ruleSetupMap.end())
					{
						yySetupRule = itRuleSetup->second.second;
					}
					else
					{
						itRuleSetup = ruleSetupMap.find("*");
						if (itRuleSetup != ruleSetupMap.end())
						{
							yySetupRule = itRuleSetup->second.second;
						}
						else
						{
							yySetupRule = "";
						}
					}

					dfa_map[it->first] = compile(it->second, o, it->first, opts->encoding.nCodeUnits ());
				}
				if (parseMode != Scanner::Rules && dfa_map.find(it->first) != dfa_map.end())
				{
					dfa_map[it->first]->emit(o, ind, !--nCount, bPrologBrace);
				}
			}
		}
		else
		{
			if (spec.re || !dfa_map.empty())
			{
				if (parseMode != Scanner::Reuse)
				{
					dfa_map[""] = compile(spec, o, "", opts->encoding.nCodeUnits ());
				}
				if (parseMode != Scanner::Rules && dfa_map.find("") != dfa_map.end())
				{
					dfa_map[""]->emit(o, ind, 0, bPrologBrace);
				}
			}
		}
		o.source.wline_info (in->get_cline (), in->get_fname ().c_str ());
		/* restore original char handling mode*/
		opts.reset_encoding (encodingOld);
	}

	if (opts->cFlag)
	{
		SetupMap::const_iterator itRuleSetup;
		for (itRuleSetup = ruleSetupMap.begin(); itRuleSetup != ruleSetupMap.end(); ++itRuleSetup)
		{
			if (itRuleSetup->first != "*" && specMap.find(itRuleSetup->first) == specMap.end())
			{
				in->fatalf_at(itRuleSetup->second.first, "setup for non existing rule '%s' found", itRuleSetup->first.c_str());
			}
		}
		if (specMap.size() < ruleSetupMap.size())
		{
			uint32_t line = in->get_cline();
			itRuleSetup = ruleSetupMap.find("*");
			if (itRuleSetup != ruleSetupMap.end())
			{
				line = itRuleSetup->second.first;
			}
			in->fatalf_at(line, "setup for all rules with '*' not possible when all rules are setup explicitly");
		}
	}

	if (opts->target == opt_t::SKELETON)
	{
		Skeleton::emit_epilog (o.source, o.skeletons);
	}

	parse_cleanup();
	in = NULL;
}

void parse_cleanup()
{
	RegExp::vFreeList.clear();
	Range::vFreeList.clear();
	RangeSuffix::freeList.clear();
	Code::freelist.clear();
	symbol_table.clear ();
	condnames.clear ();
	specMap.clear();
	specStar.clear();
	star_default = NULL;
	specNone = NULL;
}

} // end namespace re2c

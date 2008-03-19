/* Driver template for the LEMON parser generator.
** The author disclaims copyright to this source code.
*/
/* First off, code is included which follows the "include" declaration
** in the input file. */
#include <stdio.h>
#include <string.h>
#include <assert.h>

#ifdef _MSC_VER
#define CDECL __cdecl
#else
#define CDECL
#endif

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
**    XlatParseTOKENTYPE     is the data type used for minor tokens given 
**                       directly to the parser from the tokenizer.
**    YYMINORTYPE        is the data type used for all minor tokens.
**                       This is typically a union of many types, one of
**                       which is XlatParseTOKENTYPE.  The entry in the union
**                       for base tokens is called "yy0".
**    YYSTACKDEPTH       is the maximum depth of the parser's stack.  If
**                       zero the stack is dynamically sized using realloc()
**    XlatParseARG_SDECL     A static variable declaration for the %extra_argument
**    XlatParseARG_PDECL     A parameter declaration for the %extra_argument
**    XlatParseARG_STORE     Code to store %extra_argument into yypParser
**    XlatParseARG_FETCH     Code to extract %extra_argument from yypParser
**    YYNSTATE           the combined number of states.
**    YYNRULE            the number of rules in the grammar
**    YYERRORSYMBOL      is the code number of the error symbol.  If not
**                       defined, then do no error processing.
*/
#define YYCODETYPE unsigned char
#define YYNOCODE 52
#define YYACTIONTYPE unsigned char
#define XlatParseTOKENTYPE XlatToken
typedef union {
  XlatParseTOKENTYPE yy0;
  ListFilter yy11;
  SpecialArgs yy15;
  FBoomArg yy20;
  MoreFilters * yy39;
  ParseBoomArg yy40;
  MoreLines * yy81;
  int yy94;
} YYMINORTYPE;
#ifndef YYSTACKDEPTH
#define YYSTACKDEPTH 100
#endif
#define XlatParseARG_SDECL  XlatParseContext *context ;
#define XlatParseARG_PDECL , XlatParseContext *context 
#define XlatParseARG_FETCH  XlatParseContext *context  = yypParser->context 
#define XlatParseARG_STORE yypParser->context  = context 
#define YYNSTATE 131
#define YYNRULE 70
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
 /*     0 */   131,  112,   25,   29,   17,   25,   63,  111,  109,   27,
 /*    10 */   111,  109,   27,  123,   82,  104,   90,   96,   84,   45,
 /*    20 */     2,   92,   24,   88,   90,   19,   23,   21,   36,   40,
 /*    30 */    29,   17,   19,   23,   21,   36,   40,   29,   17,   45,
 /*    40 */     2,  110,   14,  126,  127,  128,  129,  130,   67,   12,
 /*    50 */   202,    1,   19,   23,   21,   36,   40,   29,   17,   19,
 /*    60 */    23,   21,   36,   40,   29,   17,   49,   25,   55,   16,
 /*    70 */    87,   49,  111,  109,   27,   86,   34,   89,    8,   19,
 /*    80 */    23,   21,   36,   40,   29,   17,   19,   23,   21,   36,
 /*    90 */    40,   29,   17,  124,   19,   23,   21,   36,   40,   29,
 /*   100 */    17,   20,  125,   37,    6,   54,   46,   39,  100,  103,
 /*   110 */    19,   23,   21,   36,   40,   29,   17,   19,   23,   21,
 /*   120 */    36,   40,   29,   17,    5,   11,   18,   13,   21,   36,
 /*   130 */    40,   29,   17,   53,   28,   57,   43,   19,   23,   21,
 /*   140 */    36,   40,   29,   17,   36,   40,   29,   17,  107,  108,
 /*   150 */    48,   19,   23,   21,   36,   40,   29,   17,   19,   23,
 /*   160 */    21,   36,   40,   29,   17,   25,    3,   56,   38,   51,
 /*   170 */   111,  109,   27,    9,   95,   81,   30,   64,  106,   19,
 /*   180 */    23,   21,   36,   40,   29,   17,   19,   23,   21,   36,
 /*   190 */    40,   29,   17,  120,   69,  118,   42,   61,   70,   19,
 /*   200 */    23,   21,   36,   40,   29,   17,    4,   19,   23,   21,
 /*   210 */    36,   40,   29,   17,   93,   66,   32,   31,   91,   19,
 /*   220 */    23,   21,   36,   40,   29,   17,   19,   23,   21,   36,
 /*   230 */    40,   29,   17,   41,   52,   35,   26,   98,  119,   51,
 /*   240 */   101,   73,   58,   33,   95,  122,   19,   23,   21,   36,
 /*   250 */    40,   29,   17,   19,   23,   21,   36,   40,   29,   17,
 /*   260 */    19,   23,   21,   36,   40,   29,   17,   97,   75,   79,
 /*   270 */    22,  105,   50,   19,   23,   21,   36,   40,   29,   17,
 /*   280 */    19,   23,   21,   36,   40,   29,   17,   77,   68,   74,
 /*   290 */    15,  121,  116,  115,  114,  113,   65,   10,   23,   21,
 /*   300 */    36,   40,   29,   17,   25,   78,   60,   25,   76,  111,
 /*   310 */   109,   27,  111,  109,   27,   72,   59,    7,   62,   25,
 /*   320 */    85,  102,   80,   94,  111,  109,   27,   47,   71,   44,
 /*   330 */   203,   25,  203,  203,  203,   83,  111,  109,   27,  203,
 /*   340 */   203,  203,  203,   25,  203,  203,  203,   99,  111,  109,
 /*   350 */    27,  203,  203,  203,  203,  203,  203,  203,  203,  117,
};
static const YYCODETYPE yy_lookahead[] = {
 /*     0 */     0,    1,    5,    7,    8,    5,   39,   10,   11,   12,
 /*    10 */    10,   11,   12,   46,   14,   41,   42,   17,   21,   48,
 /*    20 */    49,   50,   22,   41,   42,    2,    3,    4,    5,    6,
 /*    30 */     7,    8,    2,    3,    4,    5,    6,    7,    8,   48,
 /*    40 */    49,   50,   19,   24,   25,   26,   27,   28,   39,   19,
 /*    50 */    32,   33,    2,    3,    4,    5,    6,    7,    8,    2,
 /*    60 */     3,    4,    5,    6,    7,    8,   39,    5,   39,   19,
 /*    70 */    43,   39,   10,   11,   12,   43,   19,   15,   47,    2,
 /*    80 */     3,    4,    5,    6,    7,    8,    2,    3,    4,    5,
 /*    90 */     6,    7,    8,   20,    2,    3,    4,    5,    6,    7,
 /*   100 */     8,   12,   29,   19,   12,   39,   39,   30,   18,   39,
 /*   110 */     2,    3,    4,    5,    6,    7,    8,    2,    3,    4,
 /*   120 */     5,    6,    7,    8,   19,   19,   19,   19,    4,    5,
 /*   130 */     6,    7,    8,   39,   19,   39,   20,    2,    3,    4,
 /*   140 */     5,    6,    7,    8,    5,    6,    7,    8,   13,   16,
 /*   150 */    39,    2,    3,    4,    5,    6,    7,    8,    2,    3,
 /*   160 */     4,    5,    6,    7,    8,    5,   18,   39,   19,   39,
 /*   170 */    10,   11,   12,   19,   44,   45,   20,   39,   16,    2,
 /*   180 */     3,    4,    5,    6,    7,    8,    2,    3,    4,    5,
 /*   190 */     6,    7,    8,   13,   39,   13,   19,   39,   39,    2,
 /*   200 */     3,    4,    5,    6,    7,    8,   22,    2,    3,    4,
 /*   210 */     5,    6,    7,    8,   15,   39,   19,   19,   13,    2,
 /*   220 */     3,    4,    5,    6,    7,    8,    2,    3,    4,    5,
 /*   230 */     6,    7,    8,   19,   39,   19,   19,   15,   23,   39,
 /*   240 */    39,   39,   39,   19,   44,   45,    2,    3,    4,    5,
 /*   250 */     6,    7,    8,    2,    3,    4,    5,    6,    7,    8,
 /*   260 */     2,    3,    4,    5,    6,    7,    8,   23,   39,   39,
 /*   270 */    19,   13,   39,    2,    3,    4,    5,    6,    7,    8,
 /*   280 */     2,    3,    4,    5,    6,    7,    8,   39,   39,   39,
 /*   290 */    19,   34,   35,   36,   37,   38,   39,   40,    3,    4,
 /*   300 */     5,    6,    7,    8,    5,   39,   39,    5,   39,   10,
 /*   310 */    11,   12,   10,   11,   12,   39,   39,   12,   39,    5,
 /*   320 */    21,   39,   39,   21,   10,   11,   12,   39,   39,   12,
 /*   330 */    51,    5,   51,   51,   51,   21,   10,   11,   12,   51,
 /*   340 */    51,   51,   51,    5,   51,   51,   51,   21,   10,   11,
 /*   350 */    12,   51,   51,   51,   51,   51,   51,   51,   51,   21,
};
#define YY_SHIFT_USE_DFLT (-5)
#define YY_SHIFT_MAX 99
static const short yy_shift_ofst[] = {
 /*     0 */    -5,    0,   19,   19,  160,  160,  326,  326,  160,  199,
 /*    10 */   199,  302,  299,   62,   -3,  338,  314,  160,  160,  160,
 /*    20 */   160,  160,  160,  160,  160,  160,  160,  160,  160,  160,
 /*    30 */   160,  160,  160,  160,  160,  160,  160,  160,  160,  160,
 /*    40 */   160,  160,  160,  160,  160,   73,  271,   23,   50,   30,
 /*    50 */    57,   77,   84,   92,  108,  115,  258,  251,  244,  224,
 /*    60 */   217,  205,  197,  184,  177,  156,  149,  135,  278,  278,
 /*    70 */   278,  278,  278,  278,  278,  278,  295,  124,  139,   -4,
 /*    80 */    -4,  215,  222,  216,  214,  198,  182,  180,  162,  305,
 /*    90 */   154,  148,  133,  116,  107,  105,   90,   89,  317,  106,
};
#define YY_REDUCE_USE_DFLT (-34)
#define YY_REDUCE_MAX 45
static const short yy_reduce_ofst[] = {
 /*     0 */    18,  257,   -9,  -29,  130,  200,   27,   32,  -33,  -26,
 /*    10 */   -18,  138,  111,   94,   67,  276,  288,  282,  277,  269,
 /*    20 */   267,  266,  249,  248,  203,  201,  158,  128,   96,   70,
 /*    30 */    66,   29,  159,  195,  229,  233,  283,  289,  279,  250,
 /*    40 */   230,  202,  176,  155,    9,   31,
};
static const YYACTIONTYPE yy_default[] = {
 /*     0 */   132,  201,  186,  186,  201,  201,  160,  160,  201,  153,
 /*    10 */   153,  201,  201,  201,  201,  201,  201,  201,  201,  201,
 /*    20 */   201,  201,  201,  201,  201,  201,  201,  201,  201,  201,
 /*    30 */   201,  201,  201,  201,  201,  201,  201,  201,  201,  201,
 /*    40 */   201,  201,  201,  201,  201,  201,  173,  172,  171,  170,
 /*    50 */   180,  201,  168,  201,  201,  176,  201,  177,  201,  167,
 /*    60 */   201,  201,  164,  196,  162,  201,  163,  201,  178,  157,
 /*    70 */   165,  169,  174,  183,  200,  181,  145,  147,  146,  141,
 /*    80 */   142,  201,  201,  179,  182,  175,  201,  201,  201,  201,
 /*    90 */   154,  201,  201,  156,  166,  198,  201,  201,  201,  161,
 /*   100 */   152,  148,  144,  143,  155,  149,  151,  150,  185,  140,
 /*   110 */   187,  139,  138,  137,  136,  135,  134,  184,  159,  197,
 /*   120 */   158,  133,  199,  188,  194,  195,  189,  190,  191,  192,
 /*   130 */   193,
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
  XlatParseARG_SDECL                /* A place to hold %extra_argument */
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
void XlatParseTrace(FILE *TraceFILE, char *zTracePrompt){
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
  "$",             "NOP",           "OR",            "XOR",         
  "AND",           "MINUS",         "PLUS",          "MULTIPLY",    
  "DIVIDE",        "NEG",           "NUM",           "SYMNUM",      
  "LPAREN",        "RPAREN",        "DEFINE",        "SYM",         
  "RBRACE",        "ENUM",          "LBRACE",        "COMMA",       
  "EQUALS",        "TAG",           "LBRACKET",      "RBRACKET",    
  "FLAGS",         "ARG2",          "ARG3",          "ARG4",        
  "ARG5",          "OR_EQUAL",      "COLON",         "error",       
  "main",          "translation_unit",  "external_declaration",  "define_statement",
  "enum_statement",  "linetype_declaration",  "boom_declaration",  "exp",         
  "enum_open",     "enum_list",     "single_enum",   "special_args",
  "list_val",      "arg_list",      "boom_args",     "boom_op",     
  "boom_selector",  "boom_line",     "boom_body",   
};
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing reduce actions, the names of all rules are required.
*/
static const char *const yyRuleName[] = {
 /*   0 */ "main ::= translation_unit",
 /*   1 */ "translation_unit ::=",
 /*   2 */ "translation_unit ::= translation_unit external_declaration",
 /*   3 */ "external_declaration ::= define_statement",
 /*   4 */ "external_declaration ::= enum_statement",
 /*   5 */ "external_declaration ::= linetype_declaration",
 /*   6 */ "external_declaration ::= boom_declaration",
 /*   7 */ "external_declaration ::= NOP",
 /*   8 */ "exp ::= NUM",
 /*   9 */ "exp ::= SYMNUM",
 /*  10 */ "exp ::= exp PLUS exp",
 /*  11 */ "exp ::= exp MINUS exp",
 /*  12 */ "exp ::= exp MULTIPLY exp",
 /*  13 */ "exp ::= exp DIVIDE exp",
 /*  14 */ "exp ::= exp OR exp",
 /*  15 */ "exp ::= exp AND exp",
 /*  16 */ "exp ::= exp XOR exp",
 /*  17 */ "exp ::= MINUS exp",
 /*  18 */ "exp ::= LPAREN exp RPAREN",
 /*  19 */ "define_statement ::= DEFINE SYM LPAREN exp RPAREN",
 /*  20 */ "enum_statement ::= enum_open enum_list RBRACE",
 /*  21 */ "enum_open ::= ENUM LBRACE",
 /*  22 */ "enum_list ::=",
 /*  23 */ "enum_list ::= single_enum",
 /*  24 */ "enum_list ::= single_enum COMMA enum_list",
 /*  25 */ "single_enum ::= SYM",
 /*  26 */ "single_enum ::= SYM EQUALS exp",
 /*  27 */ "linetype_declaration ::= exp EQUALS exp COMMA exp LPAREN special_args RPAREN",
 /*  28 */ "linetype_declaration ::= exp EQUALS exp COMMA SYM LPAREN special_args RPAREN",
 /*  29 */ "special_args ::=",
 /*  30 */ "special_args ::= TAG",
 /*  31 */ "special_args ::= TAG COMMA exp",
 /*  32 */ "special_args ::= TAG COMMA exp COMMA exp",
 /*  33 */ "special_args ::= TAG COMMA exp COMMA exp COMMA exp",
 /*  34 */ "special_args ::= TAG COMMA exp COMMA exp COMMA exp COMMA exp",
 /*  35 */ "special_args ::= TAG COMMA TAG",
 /*  36 */ "special_args ::= TAG COMMA TAG COMMA exp",
 /*  37 */ "special_args ::= TAG COMMA TAG COMMA exp COMMA exp",
 /*  38 */ "special_args ::= TAG COMMA TAG COMMA exp COMMA exp COMMA exp",
 /*  39 */ "special_args ::= exp",
 /*  40 */ "special_args ::= exp COMMA exp",
 /*  41 */ "special_args ::= exp COMMA exp COMMA exp",
 /*  42 */ "special_args ::= exp COMMA exp COMMA exp COMMA exp",
 /*  43 */ "special_args ::= exp COMMA exp COMMA exp COMMA exp COMMA exp",
 /*  44 */ "special_args ::= exp COMMA TAG",
 /*  45 */ "special_args ::= exp COMMA TAG COMMA exp",
 /*  46 */ "special_args ::= exp COMMA TAG COMMA exp COMMA exp",
 /*  47 */ "special_args ::= exp COMMA TAG COMMA exp COMMA exp COMMA exp",
 /*  48 */ "special_args ::= exp COMMA exp COMMA TAG",
 /*  49 */ "special_args ::= exp COMMA exp COMMA TAG COMMA exp",
 /*  50 */ "special_args ::= exp COMMA exp COMMA TAG COMMA exp COMMA exp",
 /*  51 */ "special_args ::= exp COMMA exp COMMA exp COMMA TAG",
 /*  52 */ "special_args ::= exp COMMA exp COMMA exp COMMA TAG COMMA exp",
 /*  53 */ "special_args ::= exp COMMA exp COMMA exp COMMA exp COMMA TAG",
 /*  54 */ "boom_declaration ::= LBRACKET exp RBRACKET LPAREN exp COMMA exp RPAREN LBRACE boom_body RBRACE",
 /*  55 */ "boom_body ::=",
 /*  56 */ "boom_body ::= boom_line boom_body",
 /*  57 */ "boom_line ::= boom_selector boom_op boom_args",
 /*  58 */ "boom_selector ::= FLAGS",
 /*  59 */ "boom_selector ::= ARG2",
 /*  60 */ "boom_selector ::= ARG3",
 /*  61 */ "boom_selector ::= ARG4",
 /*  62 */ "boom_selector ::= ARG5",
 /*  63 */ "boom_op ::= EQUALS",
 /*  64 */ "boom_op ::= OR_EQUAL",
 /*  65 */ "boom_args ::= exp",
 /*  66 */ "boom_args ::= exp LBRACKET arg_list RBRACKET",
 /*  67 */ "arg_list ::= list_val",
 /*  68 */ "arg_list ::= list_val COMMA arg_list",
 /*  69 */ "list_val ::= exp COLON exp",
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
** to XlatParse and XlatParseFree.
*/
void *XlatParseAlloc(void *(CDECL *mallocProc)(size_t)){
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
    case 1: /* NOP */
    case 2: /* OR */
    case 3: /* XOR */
    case 4: /* AND */
    case 5: /* MINUS */
    case 6: /* PLUS */
    case 7: /* MULTIPLY */
    case 8: /* DIVIDE */
    case 9: /* NEG */
    case 10: /* NUM */
    case 11: /* SYMNUM */
    case 12: /* LPAREN */
    case 13: /* RPAREN */
    case 14: /* DEFINE */
    case 15: /* SYM */
    case 16: /* RBRACE */
    case 17: /* ENUM */
    case 18: /* LBRACE */
    case 19: /* COMMA */
    case 20: /* EQUALS */
    case 21: /* TAG */
    case 22: /* LBRACKET */
    case 23: /* RBRACKET */
    case 24: /* FLAGS */
    case 25: /* ARG2 */
    case 26: /* ARG3 */
    case 27: /* ARG4 */
    case 28: /* ARG5 */
    case 29: /* OR_EQUAL */
    case 30: /* COLON */
#line 3 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{}
#line 533 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
      break;
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
**       obtained from XlatParseAlloc.
** <li>  A pointer to a function used to reclaim memory obtained
**       from malloc.
** </ul>
*/
void XlatParseFree(
  void *p,                    /* The parser to be deleted */
  void (CDECL *freeProc)(void*)     /* Function used to reclaim memory */
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
   XlatParseARG_FETCH;
   yypParser->yyidx--;
#ifndef NDEBUG
   if( yyTraceFILE ){
     fprintf(yyTraceFILE,"%sStack Overflow!\n",yyTracePrompt);
   }
#endif
   while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
   /* Here code is inserted which will execute if the parser
   ** stack every overflows */
   XlatParseARG_STORE; /* Suppress warning about unused %extra_argument var */
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
  { 32, 1 },
  { 33, 0 },
  { 33, 2 },
  { 34, 1 },
  { 34, 1 },
  { 34, 1 },
  { 34, 1 },
  { 34, 1 },
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
  { 35, 5 },
  { 36, 3 },
  { 40, 2 },
  { 41, 0 },
  { 41, 1 },
  { 41, 3 },
  { 42, 1 },
  { 42, 3 },
  { 37, 8 },
  { 37, 8 },
  { 43, 0 },
  { 43, 1 },
  { 43, 3 },
  { 43, 5 },
  { 43, 7 },
  { 43, 9 },
  { 43, 3 },
  { 43, 5 },
  { 43, 7 },
  { 43, 9 },
  { 43, 1 },
  { 43, 3 },
  { 43, 5 },
  { 43, 7 },
  { 43, 9 },
  { 43, 3 },
  { 43, 5 },
  { 43, 7 },
  { 43, 9 },
  { 43, 5 },
  { 43, 7 },
  { 43, 9 },
  { 43, 7 },
  { 43, 9 },
  { 43, 9 },
  { 38, 11 },
  { 50, 0 },
  { 50, 2 },
  { 49, 3 },
  { 48, 1 },
  { 48, 1 },
  { 48, 1 },
  { 48, 1 },
  { 48, 1 },
  { 47, 1 },
  { 47, 1 },
  { 46, 1 },
  { 46, 4 },
  { 45, 1 },
  { 45, 3 },
  { 44, 3 },
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
  XlatParseARG_FETCH;
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
      case 1: /*translation_unit ::= */
      case 2: /*translation_unit ::= translation_unit external_declaration */
      case 3: /*external_declaration ::= define_statement */
      case 4: /*external_declaration ::= enum_statement */
      case 5: /*external_declaration ::= linetype_declaration */
      case 6: /*external_declaration ::= boom_declaration */
      case 22: /*enum_list ::= */
      case 23: /*enum_list ::= single_enum */
#line 9 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
}
#line 874 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 7: /* external_declaration ::= NOP */
#line 18 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
  yy_destructor(1,&yymsp[0].minor);
}
#line 881 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 8: /* exp ::= NUM */
#line 29 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{ yygotominor.yy94 = yymsp[0].minor.yy0.val; }
#line 886 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 9: /* exp ::= SYMNUM */
#line 30 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{ yygotominor.yy94 = yymsp[0].minor.yy0.symval->Value; }
#line 891 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 10: /* exp ::= exp PLUS exp */
#line 31 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{ yygotominor.yy94 = yymsp[-2].minor.yy94 + yymsp[0].minor.yy94;   yy_destructor(6,&yymsp[-1].minor);
}
#line 897 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 11: /* exp ::= exp MINUS exp */
#line 32 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{ yygotominor.yy94 = yymsp[-2].minor.yy94 - yymsp[0].minor.yy94;   yy_destructor(5,&yymsp[-1].minor);
}
#line 903 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 12: /* exp ::= exp MULTIPLY exp */
#line 33 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{ yygotominor.yy94 = yymsp[-2].minor.yy94 * yymsp[0].minor.yy94;   yy_destructor(7,&yymsp[-1].minor);
}
#line 909 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 13: /* exp ::= exp DIVIDE exp */
#line 34 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{ yygotominor.yy94 = yymsp[-2].minor.yy94 / yymsp[0].minor.yy94;   yy_destructor(8,&yymsp[-1].minor);
}
#line 915 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 14: /* exp ::= exp OR exp */
#line 35 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{ yygotominor.yy94 = yymsp[-2].minor.yy94 | yymsp[0].minor.yy94;   yy_destructor(2,&yymsp[-1].minor);
}
#line 921 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 15: /* exp ::= exp AND exp */
#line 36 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{ yygotominor.yy94 = yymsp[-2].minor.yy94 & yymsp[0].minor.yy94;   yy_destructor(4,&yymsp[-1].minor);
}
#line 927 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 16: /* exp ::= exp XOR exp */
#line 37 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{ yygotominor.yy94 = yymsp[-2].minor.yy94 ^ yymsp[0].minor.yy94;   yy_destructor(3,&yymsp[-1].minor);
}
#line 933 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 17: /* exp ::= MINUS exp */
#line 38 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{ yygotominor.yy94 = -yymsp[0].minor.yy94;   yy_destructor(5,&yymsp[-1].minor);
}
#line 939 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 18: /* exp ::= LPAREN exp RPAREN */
#line 39 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{ yygotominor.yy94 = yymsp[-1].minor.yy94;   yy_destructor(12,&yymsp[-2].minor);
  yy_destructor(13,&yymsp[0].minor);
}
#line 946 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 19: /* define_statement ::= DEFINE SYM LPAREN exp RPAREN */
#line 49 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	context->AddSym (yymsp[-3].minor.yy0.sym, yymsp[-1].minor.yy94);
  yy_destructor(14,&yymsp[-4].minor);
  yy_destructor(12,&yymsp[-2].minor);
  yy_destructor(13,&yymsp[0].minor);
}
#line 956 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 20: /* enum_statement ::= enum_open enum_list RBRACE */
#line 59 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
  yy_destructor(16,&yymsp[0].minor);
}
#line 963 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 21: /* enum_open ::= ENUM LBRACE */
#line 62 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	context->EnumVal = 0;
  yy_destructor(17,&yymsp[-1].minor);
  yy_destructor(18,&yymsp[0].minor);
}
#line 972 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 24: /* enum_list ::= single_enum COMMA enum_list */
#line 68 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
  yy_destructor(19,&yymsp[-1].minor);
}
#line 979 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 25: /* single_enum ::= SYM */
#line 71 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	context->AddSym (yymsp[0].minor.yy0.sym, context->EnumVal++);
}
#line 986 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 26: /* single_enum ::= SYM EQUALS exp */
#line 76 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	context->AddSym (yymsp[-2].minor.yy0.sym, context->EnumVal = yymsp[0].minor.yy94);
  yy_destructor(20,&yymsp[-1].minor);
}
#line 994 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 27: /* linetype_declaration ::= exp EQUALS exp COMMA exp LPAREN special_args RPAREN */
#line 87 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	SimpleLineTranslations.SetVal(yymsp[-7].minor.yy94, 
		FLineTrans(yymsp[-3].minor.yy94&0xffff, yymsp[-5].minor.yy94+yymsp[-1].minor.yy15.addflags, yymsp[-1].minor.yy15.args[0], yymsp[-1].minor.yy15.args[1], yymsp[-1].minor.yy15.args[2], yymsp[-1].minor.yy15.args[3], yymsp[-1].minor.yy15.args[4]));
  yy_destructor(20,&yymsp[-6].minor);
  yy_destructor(19,&yymsp[-4].minor);
  yy_destructor(12,&yymsp[-2].minor);
  yy_destructor(13,&yymsp[0].minor);
}
#line 1006 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 28: /* linetype_declaration ::= exp EQUALS exp COMMA SYM LPAREN special_args RPAREN */
#line 93 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	Printf ("%s, line %d: %s is undefined\n", context->SourceFile, context->SourceLine, yymsp[-3].minor.yy0.sym);
  yy_destructor(20,&yymsp[-6].minor);
  yy_destructor(19,&yymsp[-4].minor);
  yy_destructor(12,&yymsp[-2].minor);
  yy_destructor(13,&yymsp[0].minor);
}
#line 1017 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 29: /* special_args ::= */
#line 100 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy15.addflags = 0;
	yygotominor.yy15.args[0] = 0;
	yygotominor.yy15.args[1] = 0;
	yygotominor.yy15.args[2] = 0;
	yygotominor.yy15.args[3] = 0;
	yygotominor.yy15.args[4] = 0;
}
#line 1029 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 30: /* special_args ::= TAG */
#line 109 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy15.addflags = LINETRANS_HASTAGAT1;
	yygotominor.yy15.args[0] = 0;
	yygotominor.yy15.args[1] = 0;
	yygotominor.yy15.args[2] = 0;
	yygotominor.yy15.args[3] = 0;
	yygotominor.yy15.args[4] = 0;
  yy_destructor(21,&yymsp[0].minor);
}
#line 1042 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 31: /* special_args ::= TAG COMMA exp */
#line 118 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy15.addflags = LINETRANS_HASTAGAT1;
	yygotominor.yy15.args[0] = 0;
	yygotominor.yy15.args[1] = yymsp[0].minor.yy94;
	yygotominor.yy15.args[2] = 0;
	yygotominor.yy15.args[3] = 0;
	yygotominor.yy15.args[4] = 0;
  yy_destructor(21,&yymsp[-2].minor);
  yy_destructor(19,&yymsp[-1].minor);
}
#line 1056 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 32: /* special_args ::= TAG COMMA exp COMMA exp */
#line 127 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy15.addflags = LINETRANS_HASTAGAT1;
	yygotominor.yy15.args[0] = 0;
	yygotominor.yy15.args[1] = yymsp[-2].minor.yy94;
	yygotominor.yy15.args[2] = yymsp[0].minor.yy94;
	yygotominor.yy15.args[3] = 0;
	yygotominor.yy15.args[4] = 0;
  yy_destructor(21,&yymsp[-4].minor);
  yy_destructor(19,&yymsp[-3].minor);
  yy_destructor(19,&yymsp[-1].minor);
}
#line 1071 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 33: /* special_args ::= TAG COMMA exp COMMA exp COMMA exp */
#line 136 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy15.addflags = LINETRANS_HASTAGAT1;
	yygotominor.yy15.args[0] = 0;
	yygotominor.yy15.args[1] = yymsp[-4].minor.yy94;
	yygotominor.yy15.args[2] = yymsp[-2].minor.yy94;
	yygotominor.yy15.args[3] = yymsp[0].minor.yy94;
	yygotominor.yy15.args[4] = 0;
  yy_destructor(21,&yymsp[-6].minor);
  yy_destructor(19,&yymsp[-5].minor);
  yy_destructor(19,&yymsp[-3].minor);
  yy_destructor(19,&yymsp[-1].minor);
}
#line 1087 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 34: /* special_args ::= TAG COMMA exp COMMA exp COMMA exp COMMA exp */
#line 145 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy15.addflags = LINETRANS_HASTAGAT1;
	yygotominor.yy15.args[0] = 0;
	yygotominor.yy15.args[1] = yymsp[-6].minor.yy94;
	yygotominor.yy15.args[2] = yymsp[-4].minor.yy94;
	yygotominor.yy15.args[3] = yymsp[-2].minor.yy94;
	yygotominor.yy15.args[4] = yymsp[0].minor.yy94;
  yy_destructor(21,&yymsp[-8].minor);
  yy_destructor(19,&yymsp[-7].minor);
  yy_destructor(19,&yymsp[-5].minor);
  yy_destructor(19,&yymsp[-3].minor);
  yy_destructor(19,&yymsp[-1].minor);
}
#line 1104 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 35: /* special_args ::= TAG COMMA TAG */
#line 154 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy15.addflags = LINETRANS_HAS2TAGS;
	yygotominor.yy15.args[0] = yygotominor.yy15.args[1] = 0;
	yygotominor.yy15.args[2] = 0;
	yygotominor.yy15.args[3] = 0;
	yygotominor.yy15.args[4] = 0;
  yy_destructor(21,&yymsp[-2].minor);
  yy_destructor(19,&yymsp[-1].minor);
  yy_destructor(21,&yymsp[0].minor);
}
#line 1118 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 36: /* special_args ::= TAG COMMA TAG COMMA exp */
#line 162 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy15.addflags = LINETRANS_HAS2TAGS;
	yygotominor.yy15.args[0] = yygotominor.yy15.args[1] = 0;
	yygotominor.yy15.args[2] = yymsp[0].minor.yy94;
	yygotominor.yy15.args[3] = 0;
	yygotominor.yy15.args[4] = 0;
  yy_destructor(21,&yymsp[-4].minor);
  yy_destructor(19,&yymsp[-3].minor);
  yy_destructor(21,&yymsp[-2].minor);
  yy_destructor(19,&yymsp[-1].minor);
}
#line 1133 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 37: /* special_args ::= TAG COMMA TAG COMMA exp COMMA exp */
#line 170 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy15.addflags = LINETRANS_HAS2TAGS;
	yygotominor.yy15.args[0] = yygotominor.yy15.args[1] = 0;
	yygotominor.yy15.args[2] = yymsp[-2].minor.yy94;
	yygotominor.yy15.args[3] = yymsp[0].minor.yy94;
	yygotominor.yy15.args[4] = 0;
  yy_destructor(21,&yymsp[-6].minor);
  yy_destructor(19,&yymsp[-5].minor);
  yy_destructor(21,&yymsp[-4].minor);
  yy_destructor(19,&yymsp[-3].minor);
  yy_destructor(19,&yymsp[-1].minor);
}
#line 1149 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 38: /* special_args ::= TAG COMMA TAG COMMA exp COMMA exp COMMA exp */
#line 178 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy15.addflags = LINETRANS_HAS2TAGS;
	yygotominor.yy15.args[0] = yygotominor.yy15.args[1] = 0;
	yygotominor.yy15.args[2] = yymsp[-4].minor.yy94;
	yygotominor.yy15.args[3] = yymsp[-2].minor.yy94;
	yygotominor.yy15.args[4] = yymsp[0].minor.yy94;
  yy_destructor(21,&yymsp[-8].minor);
  yy_destructor(19,&yymsp[-7].minor);
  yy_destructor(21,&yymsp[-6].minor);
  yy_destructor(19,&yymsp[-5].minor);
  yy_destructor(19,&yymsp[-3].minor);
  yy_destructor(19,&yymsp[-1].minor);
}
#line 1166 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 39: /* special_args ::= exp */
#line 186 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy15.addflags = 0;
	yygotominor.yy15.args[0] = yymsp[0].minor.yy94;
	yygotominor.yy15.args[1] = 0;
	yygotominor.yy15.args[2] = 0;
	yygotominor.yy15.args[3] = 0;
	yygotominor.yy15.args[4] = 0;
}
#line 1178 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 40: /* special_args ::= exp COMMA exp */
#line 195 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy15.addflags = 0;
	yygotominor.yy15.args[0] = yymsp[-2].minor.yy94;
	yygotominor.yy15.args[1] = yymsp[0].minor.yy94;
	yygotominor.yy15.args[2] = 0;
	yygotominor.yy15.args[3] = 0;
	yygotominor.yy15.args[4] = 0;
  yy_destructor(19,&yymsp[-1].minor);
}
#line 1191 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 41: /* special_args ::= exp COMMA exp COMMA exp */
#line 204 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy15.addflags = 0;
	yygotominor.yy15.args[0] = yymsp[-4].minor.yy94;
	yygotominor.yy15.args[1] = yymsp[-2].minor.yy94;
	yygotominor.yy15.args[2] = yymsp[0].minor.yy94;
	yygotominor.yy15.args[3] = 0;
	yygotominor.yy15.args[4] = 0;
  yy_destructor(19,&yymsp[-3].minor);
  yy_destructor(19,&yymsp[-1].minor);
}
#line 1205 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 42: /* special_args ::= exp COMMA exp COMMA exp COMMA exp */
#line 213 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy15.addflags = 0;
	yygotominor.yy15.args[0] = yymsp[-6].minor.yy94;
	yygotominor.yy15.args[1] = yymsp[-4].minor.yy94;
	yygotominor.yy15.args[2] = yymsp[-2].minor.yy94;
	yygotominor.yy15.args[3] = yymsp[0].minor.yy94;
	yygotominor.yy15.args[4] = 0;
  yy_destructor(19,&yymsp[-5].minor);
  yy_destructor(19,&yymsp[-3].minor);
  yy_destructor(19,&yymsp[-1].minor);
}
#line 1220 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 43: /* special_args ::= exp COMMA exp COMMA exp COMMA exp COMMA exp */
#line 222 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy15.addflags = 0;
	yygotominor.yy15.args[0] = yymsp[-8].minor.yy94;
	yygotominor.yy15.args[1] = yymsp[-6].minor.yy94;
	yygotominor.yy15.args[2] = yymsp[-4].minor.yy94;
	yygotominor.yy15.args[3] = yymsp[-2].minor.yy94;
	yygotominor.yy15.args[4] = yymsp[0].minor.yy94;
  yy_destructor(19,&yymsp[-7].minor);
  yy_destructor(19,&yymsp[-5].minor);
  yy_destructor(19,&yymsp[-3].minor);
  yy_destructor(19,&yymsp[-1].minor);
}
#line 1236 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 44: /* special_args ::= exp COMMA TAG */
#line 231 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy15.addflags = LINETRANS_HASTAGAT2;
	yygotominor.yy15.args[0] = yymsp[-2].minor.yy94;
	yygotominor.yy15.args[1] = 0;
	yygotominor.yy15.args[2] = 0;
	yygotominor.yy15.args[3] = 0;
	yygotominor.yy15.args[4] = 0;
  yy_destructor(19,&yymsp[-1].minor);
  yy_destructor(21,&yymsp[0].minor);
}
#line 1250 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 45: /* special_args ::= exp COMMA TAG COMMA exp */
#line 240 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy15.addflags = LINETRANS_HASTAGAT2;
	yygotominor.yy15.args[0] = yymsp[-4].minor.yy94;
	yygotominor.yy15.args[1] = 0;
	yygotominor.yy15.args[2] = yymsp[0].minor.yy94;
	yygotominor.yy15.args[3] = 0;
	yygotominor.yy15.args[4] = 0;
  yy_destructor(19,&yymsp[-3].minor);
  yy_destructor(21,&yymsp[-2].minor);
  yy_destructor(19,&yymsp[-1].minor);
}
#line 1265 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 46: /* special_args ::= exp COMMA TAG COMMA exp COMMA exp */
#line 249 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy15.addflags = LINETRANS_HASTAGAT2;
	yygotominor.yy15.args[0] = yymsp[-6].minor.yy94;
	yygotominor.yy15.args[1] = 0;
	yygotominor.yy15.args[2] = yymsp[-2].minor.yy94;
	yygotominor.yy15.args[3] = yymsp[0].minor.yy94;
	yygotominor.yy15.args[4] = 0;
  yy_destructor(19,&yymsp[-5].minor);
  yy_destructor(21,&yymsp[-4].minor);
  yy_destructor(19,&yymsp[-3].minor);
  yy_destructor(19,&yymsp[-1].minor);
}
#line 1281 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 47: /* special_args ::= exp COMMA TAG COMMA exp COMMA exp COMMA exp */
#line 258 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy15.addflags = LINETRANS_HASTAGAT2;
	yygotominor.yy15.args[0] = yymsp[-8].minor.yy94;
	yygotominor.yy15.args[1] = 0;
	yygotominor.yy15.args[2] = yymsp[-4].minor.yy94;
	yygotominor.yy15.args[3] = yymsp[-2].minor.yy94;
	yygotominor.yy15.args[4] = yymsp[0].minor.yy94;
  yy_destructor(19,&yymsp[-7].minor);
  yy_destructor(21,&yymsp[-6].minor);
  yy_destructor(19,&yymsp[-5].minor);
  yy_destructor(19,&yymsp[-3].minor);
  yy_destructor(19,&yymsp[-1].minor);
}
#line 1298 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 48: /* special_args ::= exp COMMA exp COMMA TAG */
#line 267 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy15.addflags = LINETRANS_HASTAGAT3;
	yygotominor.yy15.args[0] = yymsp[-4].minor.yy94;
	yygotominor.yy15.args[1] = yymsp[-2].minor.yy94;
	yygotominor.yy15.args[2] = 0;
	yygotominor.yy15.args[3] = 0;
	yygotominor.yy15.args[4] = 0;
  yy_destructor(19,&yymsp[-3].minor);
  yy_destructor(19,&yymsp[-1].minor);
  yy_destructor(21,&yymsp[0].minor);
}
#line 1313 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 49: /* special_args ::= exp COMMA exp COMMA TAG COMMA exp */
#line 276 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy15.addflags = LINETRANS_HASTAGAT3;
	yygotominor.yy15.args[0] = yymsp[-6].minor.yy94;
	yygotominor.yy15.args[1] = yymsp[-4].minor.yy94;
	yygotominor.yy15.args[2] = 0;
	yygotominor.yy15.args[3] = yymsp[0].minor.yy94;
	yygotominor.yy15.args[4] = 0;
  yy_destructor(19,&yymsp[-5].minor);
  yy_destructor(19,&yymsp[-3].minor);
  yy_destructor(21,&yymsp[-2].minor);
  yy_destructor(19,&yymsp[-1].minor);
}
#line 1329 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 50: /* special_args ::= exp COMMA exp COMMA TAG COMMA exp COMMA exp */
#line 285 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy15.addflags = LINETRANS_HASTAGAT3;
	yygotominor.yy15.args[0] = yymsp[-8].minor.yy94;
	yygotominor.yy15.args[1] = yymsp[-6].minor.yy94;
	yygotominor.yy15.args[2] = 0;
	yygotominor.yy15.args[3] = yymsp[-2].minor.yy94;
	yygotominor.yy15.args[4] = yymsp[0].minor.yy94;
  yy_destructor(19,&yymsp[-7].minor);
  yy_destructor(19,&yymsp[-5].minor);
  yy_destructor(21,&yymsp[-4].minor);
  yy_destructor(19,&yymsp[-3].minor);
  yy_destructor(19,&yymsp[-1].minor);
}
#line 1346 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 51: /* special_args ::= exp COMMA exp COMMA exp COMMA TAG */
#line 294 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy15.addflags = LINETRANS_HASTAGAT4;
	yygotominor.yy15.args[0] = yymsp[-6].minor.yy94;
	yygotominor.yy15.args[1] = yymsp[-4].minor.yy94;
	yygotominor.yy15.args[2] = yymsp[-2].minor.yy94;
	yygotominor.yy15.args[3] = 0;
	yygotominor.yy15.args[4] = 0;
  yy_destructor(19,&yymsp[-5].minor);
  yy_destructor(19,&yymsp[-3].minor);
  yy_destructor(19,&yymsp[-1].minor);
  yy_destructor(21,&yymsp[0].minor);
}
#line 1362 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 52: /* special_args ::= exp COMMA exp COMMA exp COMMA TAG COMMA exp */
#line 303 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy15.addflags = LINETRANS_HASTAGAT4;
	yygotominor.yy15.args[0] = yymsp[-8].minor.yy94;
	yygotominor.yy15.args[1] = yymsp[-6].minor.yy94;
	yygotominor.yy15.args[2] = yymsp[-4].minor.yy94;
	yygotominor.yy15.args[3] = 0;
	yygotominor.yy15.args[4] = yymsp[0].minor.yy94;
  yy_destructor(19,&yymsp[-7].minor);
  yy_destructor(19,&yymsp[-5].minor);
  yy_destructor(19,&yymsp[-3].minor);
  yy_destructor(21,&yymsp[-2].minor);
  yy_destructor(19,&yymsp[-1].minor);
}
#line 1379 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 53: /* special_args ::= exp COMMA exp COMMA exp COMMA exp COMMA TAG */
#line 312 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy15.addflags = LINETRANS_HASTAGAT5;
	yygotominor.yy15.args[0] = yymsp[-8].minor.yy94;
	yygotominor.yy15.args[1] = yymsp[-6].minor.yy94;
	yygotominor.yy15.args[2] = yymsp[-4].minor.yy94;
	yygotominor.yy15.args[3] = yymsp[-2].minor.yy94;
	yygotominor.yy15.args[4] = 0;
  yy_destructor(19,&yymsp[-7].minor);
  yy_destructor(19,&yymsp[-5].minor);
  yy_destructor(19,&yymsp[-3].minor);
  yy_destructor(19,&yymsp[-1].minor);
  yy_destructor(21,&yymsp[0].minor);
}
#line 1396 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 54: /* boom_declaration ::= LBRACKET exp RBRACKET LPAREN exp COMMA exp RPAREN LBRACE boom_body RBRACE */
#line 337 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	int i;
	MoreLines *probe;

	if (NumBoomish == MAX_BOOMISH)
	{
		MoreLines *probe = yymsp[-1].minor.yy81;

		while (probe != NULL)
		{
			MoreLines *next = probe->next;
			delete probe;
			probe = next;
		}
		Printf ("%s, line %d: Too many BOOM translators\n", context->SourceFile, context->SourceLine);
	}
	else
	{
		Boomish[NumBoomish].FirstLinetype = yymsp[-6].minor.yy94;
		Boomish[NumBoomish].LastLinetype = yymsp[-4].minor.yy94;
		Boomish[NumBoomish].NewSpecial = yymsp[-9].minor.yy94;
		
		for (i = 0, probe = yymsp[-1].minor.yy81; probe != NULL; i++)
		{
			MoreLines *next = probe->next;
			Boomish[NumBoomish].Args.Push(probe->arg);
			delete probe;
			probe = next;
		}
		NumBoomish++;
	}
  yy_destructor(22,&yymsp[-10].minor);
  yy_destructor(23,&yymsp[-8].minor);
  yy_destructor(12,&yymsp[-7].minor);
  yy_destructor(19,&yymsp[-5].minor);
  yy_destructor(13,&yymsp[-3].minor);
  yy_destructor(18,&yymsp[-2].minor);
  yy_destructor(16,&yymsp[0].minor);
}
#line 1439 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 55: /* boom_body ::= */
#line 371 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy81 = NULL;
}
#line 1446 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 56: /* boom_body ::= boom_line boom_body */
#line 375 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy81 = new MoreLines;
	yygotominor.yy81->next = yymsp[0].minor.yy81;
	yygotominor.yy81->arg = yymsp[-1].minor.yy20;
}
#line 1455 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 57: /* boom_line ::= boom_selector boom_op boom_args */
#line 382 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy20.bOrExisting = (yymsp[-1].minor.yy94 == OR_EQUAL);
	yygotominor.yy20.bUseConstant = (yymsp[0].minor.yy40.filters == NULL);
	yygotominor.yy20.ArgNum = yymsp[-2].minor.yy94;
	yygotominor.yy20.ConstantValue = yymsp[0].minor.yy40.constant;
	yygotominor.yy20.AndValue = yymsp[0].minor.yy40.mask;

	if (yymsp[0].minor.yy40.filters != NULL)
	{
		int i;
		MoreFilters *probe;

		for (i = 0, probe = yymsp[0].minor.yy40.filters; probe != NULL; i++)
		{
			MoreFilters *next = probe->next;
			if (i < 15)
			{
				yygotominor.yy20.ResultFilter[i] = probe->filter.filter;
				yygotominor.yy20.ResultValue[i] = probe->filter.value;
			}
			else if (i == 15)
			{
				context->PrintError ("Lists can only have 15 elements");
			}
			delete probe;
			probe = next;
		}
		yygotominor.yy20.ListSize = i > 15 ? 15 : i;
	}
}
#line 1489 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 58: /* boom_selector ::= FLAGS */
#line 413 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{ yygotominor.yy94 = 4;   yy_destructor(24,&yymsp[0].minor);
}
#line 1495 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 59: /* boom_selector ::= ARG2 */
#line 414 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{ yygotominor.yy94 = 0;   yy_destructor(25,&yymsp[0].minor);
}
#line 1501 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 60: /* boom_selector ::= ARG3 */
#line 415 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{ yygotominor.yy94 = 1;   yy_destructor(26,&yymsp[0].minor);
}
#line 1507 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 61: /* boom_selector ::= ARG4 */
#line 416 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{ yygotominor.yy94 = 2;   yy_destructor(27,&yymsp[0].minor);
}
#line 1513 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 62: /* boom_selector ::= ARG5 */
#line 417 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{ yygotominor.yy94 = 3;   yy_destructor(28,&yymsp[0].minor);
}
#line 1519 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 63: /* boom_op ::= EQUALS */
#line 419 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{ yygotominor.yy94 = '=';   yy_destructor(20,&yymsp[0].minor);
}
#line 1525 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 64: /* boom_op ::= OR_EQUAL */
#line 420 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{ yygotominor.yy94 = OR_EQUAL;   yy_destructor(29,&yymsp[0].minor);
}
#line 1531 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 65: /* boom_args ::= exp */
#line 423 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy40.constant = yymsp[0].minor.yy94;
	yygotominor.yy40.filters = NULL;
}
#line 1539 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 66: /* boom_args ::= exp LBRACKET arg_list RBRACKET */
#line 428 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy40.mask = yymsp[-3].minor.yy94;
	yygotominor.yy40.filters = yymsp[-1].minor.yy39;
  yy_destructor(22,&yymsp[-2].minor);
  yy_destructor(23,&yymsp[0].minor);
}
#line 1549 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 67: /* arg_list ::= list_val */
#line 434 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy39 = new MoreFilters;
	yygotominor.yy39->next = NULL;
	yygotominor.yy39->filter = yymsp[0].minor.yy11;
}
#line 1558 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 68: /* arg_list ::= list_val COMMA arg_list */
#line 440 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy39 = new MoreFilters;
	yygotominor.yy39->next = yymsp[0].minor.yy39;
	yygotominor.yy39->filter = yymsp[-2].minor.yy11;
  yy_destructor(19,&yymsp[-1].minor);
}
#line 1568 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
        break;
      case 69: /* list_val ::= exp COLON exp */
#line 447 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
{
	yygotominor.yy11.filter = yymsp[-2].minor.yy94;
	yygotominor.yy11.value = yymsp[0].minor.yy94;
  yy_destructor(30,&yymsp[-1].minor);
}
#line 1577 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
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
  XlatParseARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sFail!\n",yyTracePrompt);
  }
#endif
  while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser fails */
  XlatParseARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/*
** The following code executes when a syntax error first occurs.
*/
static void yy_syntax_error(
  yyParser *yypParser,           /* The parser */
  int yymajor,                   /* The major type of the error token */
  YYMINORTYPE yyminor            /* The minor type of the error token */
){
  XlatParseARG_FETCH;
#define TOKEN (yyminor.yy0)
#line 6 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.y"
 context->PrintError("syntax error");
#line 1637 "c:\\zdoom\\trunk\\src\\xlat\\xlat_parser.c"
  XlatParseARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/*
** The following is executed when the parser accepts
*/
static void yy_accept(
  yyParser *yypParser           /* The parser */
){
  XlatParseARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sAccept!\n",yyTracePrompt);
  }
#endif
  while( yypParser->yyidx>=0 ) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser accepts */
  XlatParseARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/* The main parser program.
** The first argument is a pointer to a structure obtained from
** "XlatParseAlloc" which describes the current state of the parser.
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
void XlatParse(
  void *yyp,                   /* The parser */
  int yymajor,                 /* The major token code number */
  XlatParseTOKENTYPE yyminor       /* The value for the token */
  XlatParseARG_PDECL               /* Optional %extra_argument parameter */
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
  XlatParseARG_STORE;

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

/* Driver template for the LEMON parser generator.
** The author disclaims copyright to this source code.
*/
/* First off, code is included which follows the "include" declaration
** in the input file. */
#include <stdio.h>
#include <string.h>
#line 1 "parse.y"

#include <malloc.h>
#include "dehsupp.h"
#line 13 "parse.c"
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
#define YYNOCODE 68
#define YYACTIONTYPE unsigned char
#define ParseTOKENTYPE struct Token
typedef union {
  ParseTOKENTYPE yy0;
  int yy62;
  int yy135;
} YYMINORTYPE;
#define YYSTACKDEPTH 100
#define ParseARG_SDECL
#define ParseARG_PDECL
#define ParseARG_FETCH
#define ParseARG_STORE
#define YYNSTATE 140
#define YYNRULE 77
#define YYERRORSYMBOL 35
#define YYERRSYMDT yy135
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
 /*     0 */   137,  131,  130,  126,  120,  119,  118,  117,  109,  101,
 /*    10 */    95,   94,   80,   14,   17,   20,   23,   18,   15,   71,
 /*    20 */    20,   23,   18,   15,   86,   60,   45,   68,   76,   84,
 /*    30 */    93,   77,   36,   74,   66,   57,   73,   16,   14,   17,
 /*    40 */    20,   23,   18,   15,   17,   20,   23,   18,   15,   11,
 /*    50 */    33,   16,   14,   17,   20,   23,   18,   15,   21,   32,
 /*    60 */    82,   50,  128,   61,   72,   16,   14,   17,   20,   23,
 /*    70 */    18,   15,   54,  139,   18,   15,  132,   32,   16,   14,
 /*    80 */    17,   20,   23,   18,   15,   96,   97,   98,   55,   99,
 /*    90 */    78,  138,   16,   14,   17,   20,   23,   18,   15,   19,
 /*   100 */    19,   83,   77,   36,   40,   90,   24,   24,   44,  100,
 /*   110 */   129,  103,  103,   67,   62,   81,   58,    8,   65,   70,
 /*   120 */    69,   79,   59,   64,   22,   33,   38,    9,   52,   63,
 /*   130 */    56,  107,   30,   85,  218,    1,    7,  127,   87,   47,
 /*   140 */   136,   10,   89,   49,   88,   48,   13,  115,   91,    3,
 /*   150 */    46,  121,  133,    2,   92,   28,   12,   29,   34,   53,
 /*   160 */    37,   39,   51,    4,  134,  135,  102,  111,   43,   25,
 /*   170 */   124,    6,   75,  106,   41,   26,  113,   42,    5,  108,
 /*   180 */    35,   31,  125,  219,  105,  110,  104,  219,  123,  116,
 /*   190 */    27,  122,  114,  112,
};
static const YYCODETYPE yy_lookahead[] = {
 /*     0 */    38,   39,   40,   41,   42,   43,   44,   45,   46,   47,
 /*    10 */    48,   49,   10,    2,    3,    4,    5,    6,    7,   17,
 /*    20 */     4,    5,    6,    7,   22,   23,   61,   25,   26,   27,
 /*    30 */    50,   51,   52,   31,   32,   33,   34,    1,    2,    3,
 /*    40 */     4,    5,    6,    7,    3,    4,    5,    6,    7,   13,
 /*    50 */    52,    1,    2,    3,    4,    5,    6,    7,   13,   52,
 /*    60 */    21,   63,   64,   13,   19,    1,    2,    3,    4,    5,
 /*    70 */     6,    7,   65,   66,    6,    7,   12,   52,    1,    2,
 /*    80 */     3,    4,    5,    6,    7,   28,   29,   30,   58,   59,
 /*    90 */    13,   66,    1,    2,    3,    4,    5,    6,    7,    4,
 /*   100 */     4,   50,   51,   52,   52,   59,   11,   11,   56,   14,
 /*   110 */    15,   16,   16,   13,   13,   13,   13,   13,   13,   19,
 /*   120 */    19,   19,   19,   19,   13,   52,   52,   13,   54,   24,
 /*   130 */    19,   14,   13,   19,   36,   37,   18,   64,   19,   57,
 /*   140 */    20,   18,   21,   55,   12,   62,   13,   21,   21,   11,
 /*   150 */    53,   21,   21,   13,   20,   18,   13,   18,   52,   52,
 /*   160 */    52,   52,   52,   18,   20,   52,   20,   52,   52,   18,
 /*   170 */    20,   18,   60,   14,   52,   18,   52,   52,   18,   21,
 /*   180 */    52,   52,   20,   67,   21,   20,   20,   67,   21,   20,
 /*   190 */    18,   21,   21,   20,
};
#define YY_SHIFT_USE_DFLT (-1)
#define YY_SHIFT_MAX 87
static const short yy_shift_ofst[] = {
 /*     0 */    -1,    2,   95,   95,   96,   96,   96,   96,   96,   96,
 /*    10 */    39,   96,   96,   57,   96,   96,   96,   96,   96,   96,
 /*    20 */    96,   96,   96,   96,   96,  130,  126,  163,  121,  117,
 /*    30 */    39,   50,   77,   36,   64,   91,   91,   91,   91,   91,
 /*    40 */    91,   11,   41,   16,   45,  100,  101,  102,  103,  105,
 /*    50 */   104,   68,  111,   68,  114,  119,  169,  160,  171,  173,
 /*    60 */   172,  170,  167,  165,  162,  158,  157,  159,  153,  150,
 /*    70 */   166,  151,  146,  145,  139,  143,  137,  140,  131,  134,
 /*    80 */   138,  127,  133,  132,  123,  120,  118,  144,
};
#define YY_REDUCE_USE_DFLT (-39)
#define YY_REDUCE_MAX 30
static const short yy_reduce_ofst[] = {
 /*     0 */    98,  -38,  -20,   51,    7,   -2,   52,   74,   73,   25,
 /*    10 */    30,  129,  128,  112,  125,  124,  122,  116,  115,  113,
 /*    20 */   110,  109,  108,  107,  106,   97,   83,   88,   82,  -35,
 /*    30 */    46,
};
static const YYACTIONTYPE yy_default[] = {
 /*     0 */   141,  140,  155,  155,  213,  208,  184,  176,  217,  217,
 /*    10 */   192,  217,  217,  217,  217,  217,  217,  217,  217,  217,
 /*    20 */   217,  217,  217,  217,  217,  172,  204,  180,  188,  200,
 /*    30 */   217,  217,  217,  217,  217,  195,  159,  178,  177,  186,
 /*    40 */   185,  166,  168,  167,  217,  217,  217,  217,  217,  217,
 /*    50 */   217,  163,  217,  162,  217,  217,  217,  217,  217,  217,
 /*    60 */   217,  217,  217,  217,  217,  217,  217,  217,  217,  217,
 /*    70 */   217,  217,  217,  217,  217,  217,  217,  156,  217,  217,
 /*    80 */   217,  217,  217,  217,  217,  217,  217,  217,  154,  189,
 /*    90 */   194,  190,  187,  157,  153,  152,  196,  197,  198,  193,
 /*   100 */   158,  151,  183,  161,  199,  181,  202,  201,  182,  150,
 /*   110 */   179,  164,  203,  165,  206,  205,  175,  149,  148,  147,
 /*   120 */   146,  173,  211,  174,  171,  207,  145,  210,  209,  160,
 /*   130 */   144,  143,  170,  216,  191,  169,  212,  142,  215,  214,
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
  "NEG",           "EOI",           "PRINT",         "LPAREN",      
  "RPAREN",        "COMMA",         "STRING",        "ENDL",        
  "NUM",           "Actions",       "LBRACE",        "RBRACE",      
  "SEMICOLON",     "SYM",           "OrgHeights",    "ActionList",  
  "RBARCE",        "CodePConv",     "OrgSprNames",   "StateMap",    
  "FirstState",    "SpawnState",    "DeathState",    "SoundMap",    
  "InfoNames",     "ThingBits",     "RenderStyles",  "error",       
  "main",          "translation_unit",  "external_declaration",  "print_statement",
  "actions_def",   "org_heights_def",  "action_list_def",  "codep_conv_def",
  "org_spr_names_def",  "state_map_def",  "sound_map_def",  "info_names_def",
  "thing_bits_def",  "render_styles_def",  "print_list",    "print_item",  
  "exp",           "actions_list",  "org_heights_list",  "action_list_list",
  "codep_conv_list",  "org_spr_names_list",  "state_map_list",  "state_map_entry",
  "state_type",    "sound_map_list",  "info_names_list",  "thing_bits_list",
  "thing_bits_entry",  "render_styles_list",  "render_styles_entry",
};
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing reduce actions, the names of all rules are required.
*/
static const char *const yyRuleName[] = {
 /*   0 */ "main ::= translation_unit",
 /*   1 */ "translation_unit ::=",
 /*   2 */ "translation_unit ::= translation_unit external_declaration",
 /*   3 */ "external_declaration ::= print_statement",
 /*   4 */ "external_declaration ::= actions_def",
 /*   5 */ "external_declaration ::= org_heights_def",
 /*   6 */ "external_declaration ::= action_list_def",
 /*   7 */ "external_declaration ::= codep_conv_def",
 /*   8 */ "external_declaration ::= org_spr_names_def",
 /*   9 */ "external_declaration ::= state_map_def",
 /*  10 */ "external_declaration ::= sound_map_def",
 /*  11 */ "external_declaration ::= info_names_def",
 /*  12 */ "external_declaration ::= thing_bits_def",
 /*  13 */ "external_declaration ::= render_styles_def",
 /*  14 */ "print_statement ::= PRINT LPAREN print_list RPAREN",
 /*  15 */ "print_list ::=",
 /*  16 */ "print_list ::= print_item",
 /*  17 */ "print_list ::= print_item COMMA print_list",
 /*  18 */ "print_item ::= STRING",
 /*  19 */ "print_item ::= exp",
 /*  20 */ "print_item ::= ENDL",
 /*  21 */ "exp ::= NUM",
 /*  22 */ "exp ::= exp PLUS exp",
 /*  23 */ "exp ::= exp MINUS exp",
 /*  24 */ "exp ::= exp MULTIPLY exp",
 /*  25 */ "exp ::= exp DIVIDE exp",
 /*  26 */ "exp ::= exp OR exp",
 /*  27 */ "exp ::= exp AND exp",
 /*  28 */ "exp ::= exp XOR exp",
 /*  29 */ "exp ::= MINUS exp",
 /*  30 */ "exp ::= LPAREN exp RPAREN",
 /*  31 */ "actions_def ::= Actions LBRACE actions_list RBRACE SEMICOLON",
 /*  32 */ "actions_list ::=",
 /*  33 */ "actions_list ::= SYM",
 /*  34 */ "actions_list ::= actions_list COMMA SYM",
 /*  35 */ "org_heights_def ::= OrgHeights LBRACE org_heights_list RBRACE SEMICOLON",
 /*  36 */ "org_heights_list ::=",
 /*  37 */ "org_heights_list ::= exp",
 /*  38 */ "org_heights_list ::= org_heights_list COMMA exp",
 /*  39 */ "action_list_def ::= ActionList LBRACE action_list_list RBARCE SEMICOLON",
 /*  40 */ "action_list_list ::=",
 /*  41 */ "action_list_list ::= SYM",
 /*  42 */ "action_list_list ::= action_list_list COMMA SYM",
 /*  43 */ "codep_conv_def ::= CodePConv LBRACE codep_conv_list RBRACE SEMICOLON",
 /*  44 */ "codep_conv_list ::=",
 /*  45 */ "codep_conv_list ::= exp",
 /*  46 */ "codep_conv_list ::= codep_conv_list COMMA exp",
 /*  47 */ "org_spr_names_def ::= OrgSprNames LBRACE org_spr_names_list RBRACE SEMICOLON",
 /*  48 */ "org_spr_names_list ::=",
 /*  49 */ "org_spr_names_list ::= SYM",
 /*  50 */ "org_spr_names_list ::= org_spr_names_list COMMA SYM",
 /*  51 */ "state_map_def ::= StateMap LBRACE state_map_list RBRACE SEMICOLON",
 /*  52 */ "state_map_list ::=",
 /*  53 */ "state_map_list ::= state_map_entry",
 /*  54 */ "state_map_list ::= state_map_list COMMA state_map_entry",
 /*  55 */ "state_map_entry ::= SYM COMMA state_type COMMA exp",
 /*  56 */ "state_type ::= FirstState",
 /*  57 */ "state_type ::= SpawnState",
 /*  58 */ "state_type ::= DeathState",
 /*  59 */ "sound_map_def ::= SoundMap LBRACE sound_map_list RBRACE SEMICOLON",
 /*  60 */ "sound_map_list ::=",
 /*  61 */ "sound_map_list ::= STRING",
 /*  62 */ "sound_map_list ::= sound_map_list COMMA STRING",
 /*  63 */ "info_names_def ::= InfoNames LBRACE info_names_list RBRACE SEMICOLON",
 /*  64 */ "info_names_list ::=",
 /*  65 */ "info_names_list ::= SYM",
 /*  66 */ "info_names_list ::= info_names_list COMMA SYM",
 /*  67 */ "thing_bits_def ::= ThingBits LBRACE thing_bits_list RBRACE SEMICOLON",
 /*  68 */ "thing_bits_list ::=",
 /*  69 */ "thing_bits_list ::= thing_bits_entry",
 /*  70 */ "thing_bits_list ::= thing_bits_list COMMA thing_bits_entry",
 /*  71 */ "thing_bits_entry ::= exp COMMA exp COMMA SYM",
 /*  72 */ "render_styles_def ::= RenderStyles LBRACE render_styles_list RBRACE SEMICOLON",
 /*  73 */ "render_styles_list ::=",
 /*  74 */ "render_styles_list ::= render_styles_entry",
 /*  75 */ "render_styles_list ::= render_styles_list COMMA render_styles_entry",
 /*  76 */ "render_styles_entry ::= exp COMMA SYM",
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
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
    case 27:
    case 28:
    case 29:
    case 30:
    case 31:
    case 32:
    case 33:
    case 34:
#line 8 "parse.y"
{ if ((yypminor->yy0).string) free((yypminor->yy0).string); }
#line 490 "parse.c"
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
  { 36, 1 },
  { 37, 0 },
  { 37, 2 },
  { 38, 1 },
  { 38, 1 },
  { 38, 1 },
  { 38, 1 },
  { 38, 1 },
  { 38, 1 },
  { 38, 1 },
  { 38, 1 },
  { 38, 1 },
  { 38, 1 },
  { 38, 1 },
  { 39, 4 },
  { 50, 0 },
  { 50, 1 },
  { 50, 3 },
  { 51, 1 },
  { 51, 1 },
  { 51, 1 },
  { 52, 1 },
  { 52, 3 },
  { 52, 3 },
  { 52, 3 },
  { 52, 3 },
  { 52, 3 },
  { 52, 3 },
  { 52, 3 },
  { 52, 2 },
  { 52, 3 },
  { 40, 5 },
  { 53, 0 },
  { 53, 1 },
  { 53, 3 },
  { 41, 5 },
  { 54, 0 },
  { 54, 1 },
  { 54, 3 },
  { 42, 5 },
  { 55, 0 },
  { 55, 1 },
  { 55, 3 },
  { 43, 5 },
  { 56, 0 },
  { 56, 1 },
  { 56, 3 },
  { 44, 5 },
  { 57, 0 },
  { 57, 1 },
  { 57, 3 },
  { 45, 5 },
  { 58, 0 },
  { 58, 1 },
  { 58, 3 },
  { 59, 5 },
  { 60, 1 },
  { 60, 1 },
  { 60, 1 },
  { 46, 5 },
  { 61, 0 },
  { 61, 1 },
  { 61, 3 },
  { 47, 5 },
  { 62, 0 },
  { 62, 1 },
  { 62, 3 },
  { 48, 5 },
  { 63, 0 },
  { 63, 1 },
  { 63, 3 },
  { 64, 5 },
  { 49, 5 },
  { 65, 0 },
  { 65, 1 },
  { 65, 3 },
  { 66, 3 },
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
      case 14:
#line 37 "parse.y"
{
	printf ("\n");
  yy_destructor(10,&yymsp[-3].minor);
  yy_destructor(11,&yymsp[-2].minor);
  yy_destructor(12,&yymsp[0].minor);
}
#line 818 "parse.c"
        break;
      case 18:
#line 45 "parse.y"
{ printf ("%s", yymsp[0].minor.yy0.string); }
#line 823 "parse.c"
        break;
      case 19:
#line 46 "parse.y"
{ printf ("%d", yymsp[0].minor.yy62); }
#line 828 "parse.c"
        break;
      case 20:
#line 47 "parse.y"
{ printf ("\n");   yy_destructor(15,&yymsp[0].minor);
}
#line 834 "parse.c"
        break;
      case 21:
#line 50 "parse.y"
{ yygotominor.yy62 = yymsp[0].minor.yy0.val; }
#line 839 "parse.c"
        break;
      case 22:
#line 51 "parse.y"
{ yygotominor.yy62 = yymsp[-2].minor.yy62 + yymsp[0].minor.yy62;   yy_destructor(5,&yymsp[-1].minor);
}
#line 845 "parse.c"
        break;
      case 23:
#line 52 "parse.y"
{ yygotominor.yy62 = yymsp[-2].minor.yy62 - yymsp[0].minor.yy62;   yy_destructor(4,&yymsp[-1].minor);
}
#line 851 "parse.c"
        break;
      case 24:
#line 53 "parse.y"
{ yygotominor.yy62 = yymsp[-2].minor.yy62 * yymsp[0].minor.yy62;   yy_destructor(6,&yymsp[-1].minor);
}
#line 857 "parse.c"
        break;
      case 25:
#line 54 "parse.y"
{ yygotominor.yy62 = yymsp[-2].minor.yy62 / yymsp[0].minor.yy62;   yy_destructor(7,&yymsp[-1].minor);
}
#line 863 "parse.c"
        break;
      case 26:
#line 55 "parse.y"
{ yygotominor.yy62 = yymsp[-2].minor.yy62 | yymsp[0].minor.yy62;   yy_destructor(1,&yymsp[-1].minor);
}
#line 869 "parse.c"
        break;
      case 27:
#line 56 "parse.y"
{ yygotominor.yy62 = yymsp[-2].minor.yy62 & yymsp[0].minor.yy62;   yy_destructor(3,&yymsp[-1].minor);
}
#line 875 "parse.c"
        break;
      case 28:
#line 57 "parse.y"
{ yygotominor.yy62 = yymsp[-2].minor.yy62 ^ yymsp[0].minor.yy62;   yy_destructor(2,&yymsp[-1].minor);
}
#line 881 "parse.c"
        break;
      case 29:
#line 58 "parse.y"
{ yygotominor.yy62 = -yymsp[0].minor.yy62;   yy_destructor(4,&yymsp[-1].minor);
}
#line 887 "parse.c"
        break;
      case 30:
#line 59 "parse.y"
{ yygotominor.yy62 = yymsp[-1].minor.yy62;   yy_destructor(11,&yymsp[-2].minor);
  yy_destructor(12,&yymsp[0].minor);
}
#line 894 "parse.c"
        break;
      case 33:
#line 65 "parse.y"
{ AddAction (yymsp[0].minor.yy0.string); }
#line 899 "parse.c"
        break;
      case 34:
#line 66 "parse.y"
{ AddAction (yymsp[0].minor.yy0.string);   yy_destructor(13,&yymsp[-1].minor);
}
#line 905 "parse.c"
        break;
      case 37:
#line 72 "parse.y"
{ AddHeight (yymsp[0].minor.yy62); }
#line 910 "parse.c"
        break;
      case 38:
#line 73 "parse.y"
{ AddHeight (yymsp[0].minor.yy62);   yy_destructor(13,&yymsp[-1].minor);
}
#line 916 "parse.c"
        break;
      case 41:
#line 79 "parse.y"
{ AddActionMap (yymsp[0].minor.yy0.string); }
#line 921 "parse.c"
        break;
      case 42:
#line 80 "parse.y"
{ AddActionMap (yymsp[0].minor.yy0.string);   yy_destructor(13,&yymsp[-1].minor);
}
#line 927 "parse.c"
        break;
      case 45:
#line 86 "parse.y"
{ AddCodeP (yymsp[0].minor.yy62); }
#line 932 "parse.c"
        break;
      case 46:
#line 87 "parse.y"
{ AddCodeP (yymsp[0].minor.yy62);   yy_destructor(13,&yymsp[-1].minor);
}
#line 938 "parse.c"
        break;
      case 49:
#line 93 "parse.y"
{ AddSpriteName (yymsp[0].minor.yy0.string); }
#line 943 "parse.c"
        break;
      case 50:
#line 94 "parse.y"
{ AddSpriteName (yymsp[0].minor.yy0.string);   yy_destructor(13,&yymsp[-1].minor);
}
#line 949 "parse.c"
        break;
      case 55:
#line 103 "parse.y"
{ AddStateMap (yymsp[-4].minor.yy0.string, yymsp[-2].minor.yy62, yymsp[0].minor.yy62);   yy_destructor(13,&yymsp[-3].minor);
  yy_destructor(13,&yymsp[-1].minor);
}
#line 956 "parse.c"
        break;
      case 56:
#line 106 "parse.y"
{ yygotominor.yy62 = 0;   yy_destructor(28,&yymsp[0].minor);
}
#line 962 "parse.c"
        break;
      case 57:
#line 107 "parse.y"
{ yygotominor.yy62 = 1;   yy_destructor(29,&yymsp[0].minor);
}
#line 968 "parse.c"
        break;
      case 58:
#line 108 "parse.y"
{ yygotominor.yy62 = 2;   yy_destructor(30,&yymsp[0].minor);
}
#line 974 "parse.c"
        break;
      case 61:
#line 114 "parse.y"
{ AddSoundMap (yymsp[0].minor.yy0.string); }
#line 979 "parse.c"
        break;
      case 62:
#line 115 "parse.y"
{ AddSoundMap (yymsp[0].minor.yy0.string);   yy_destructor(13,&yymsp[-1].minor);
}
#line 985 "parse.c"
        break;
      case 65:
#line 121 "parse.y"
{ AddInfoName (yymsp[0].minor.yy0.string); }
#line 990 "parse.c"
        break;
      case 66:
#line 122 "parse.y"
{ AddInfoName (yymsp[0].minor.yy0.string);   yy_destructor(13,&yymsp[-1].minor);
}
#line 996 "parse.c"
        break;
      case 71:
#line 131 "parse.y"
{ AddThingBits (yymsp[0].minor.yy0.string, yymsp[-4].minor.yy62, yymsp[-2].minor.yy62);   yy_destructor(13,&yymsp[-3].minor);
  yy_destructor(13,&yymsp[-1].minor);
}
#line 1003 "parse.c"
        break;
      case 76:
#line 140 "parse.y"
{ AddRenderStyle (yymsp[0].minor.yy0.string, yymsp[-2].minor.yy62);   yy_destructor(13,&yymsp[-1].minor);
}
#line 1009 "parse.c"
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

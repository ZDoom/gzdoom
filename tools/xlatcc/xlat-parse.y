%include{
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

}

%token_type {YYSTYPE}

%syntax_error {yyerror("syntax error");}

%type exp {int}
%type special_args {SpecialArgs}
%type list_val {struct ListFilter}
%type arg_list {MoreFilters *}
%type boom_args {ParseBoomArg}
%type boom_op {int}
%type boom_selector {int}
%type boom_line {BoomArg}
%type boom_body {MoreLines *}
%type maybe_argcount {int}

%left OR.
%left XOR.
%left AND.
%left MINUS PLUS.
%left MULTIPLY DIVIDE.
%left NEG.

main ::= translation_unit.

exp(A) ::= NUM(B).					{ A = B.val; }
exp(A) ::= SYMNUM(B).				{ A = B.symval->Value; }
exp(A) ::= exp(B) PLUS exp(C).		{ A = B + C; }
exp(A) ::= exp(B) MINUS exp(C).		{ A = B - C; }
exp(A) ::= exp(B) MULTIPLY exp(C).	{ A = B * C; }
exp(A) ::= exp(B) DIVIDE exp(C).	{ A = B / C; }
exp(A) ::= exp(B) OR exp(C).		{ A = B | C; }
exp(A) ::= exp(B) AND exp(C).		{ A = B & C; }
exp(A) ::= exp(B) XOR exp(C).		{ A = B ^ C; }
exp(A) ::= MINUS exp(B). [NEG]		{ A = -B; }
exp(A) ::= LPAREN exp(B) RPAREN.	{ A = B; }

translation_unit ::= . /* empty */
translation_unit ::= translation_unit external_declaration.

external_declaration ::= define_statement.
external_declaration ::= include_statement.
external_declaration ::= print_statement.
external_declaration ::= enum_statement.
external_declaration ::= linetype_declaration.
external_declaration ::= boom_declaration.
external_declaration ::= special_declaration.
external_declaration ::= NOP.

print_statement ::= PRINT LPAREN print_list RPAREN.
{
  printf ("\n");
}

print_list ::= .	/* EMPTY */
print_list ::= print_item.
print_list ::= print_item COMMA print_list.

print_item ::= STRING(A).			{ printf ("%s", A.string); }
print_item ::= exp(A).				{ printf ("%d", A); }
print_item ::= ENDL.				{ printf ("\n"); }

define_statement ::= DEFINE SYM(A) LPAREN exp(B) RPAREN.
{
	AddSym (A.sym, B);
}

include_statement ::= INCLUDE STRING(A).
{
	IncludeFile (A.string);
}

enum_statement ::= enum_open enum_list RBRACE.

enum_open ::= ENUM LBRACE.
{
	EnumVal = 0;
}

enum_list ::= . /* empty */
enum_list ::= single_enum.
enum_list ::= single_enum COMMA enum_list.

single_enum ::= SYM(A).
{
	AddSym (A.sym, EnumVal++);
}

single_enum ::= SYM(A) EQUALS exp(B).
{
	AddSym (A.sym, EnumVal = B);
}

/* special declarations work just like they do for ACS, so
 * specials can be defined just by including zspecial.acs
 */
special_declaration ::= SPECIAL special_list SEMICOLON.

special_list ::= special_def.
special_list ::= special_list COMMA special_def.

special_def ::= exp(A) COLON SYM(B) LPAREN maybe_argcount RPAREN.
{
	AddSym (B.sym, A);
}
special_def ::= exp COLON SYMNUM(B) LPAREN maybe_argcount RPAREN.
{
	printf ("%s, line %d: %s is already defined\n", SourceName, SourceLine, B.symval->Sym);
}

maybe_argcount ::= . /* empty */
maybe_argcount ::= exp.
maybe_argcount ::= exp COMMA exp.

linetype_declaration ::= exp(linetype) EQUALS exp(flags) COMMA exp(special) LPAREN special_args(arg) RPAREN.
{
	Simple[linetype].NewSpecial = special;
	Simple[linetype].Flags = flags | arg.addflags;
	Simple[linetype].Args[0] = arg.args[0];
	Simple[linetype].Args[1] = arg.args[1];
	Simple[linetype].Args[2] = arg.args[2];
	Simple[linetype].Args[3] = arg.args[3];
	Simple[linetype].Args[4] = arg.args[4];
}
linetype_declaration ::= exp EQUALS exp COMMA SYM(S) LPAREN special_args RPAREN.
{
	printf ("%s, line %d: %s is undefined\n", SourceName, SourceLine, S.sym);
}

boom_declaration ::= LBRACKET exp(special) RBRACKET LPAREN exp(firsttype) COMMA exp(lasttype) RPAREN LBRACE boom_body(stores) RBRACE.
{
	if (NumBoomish == MAX_BOOMISH)
	{
		MoreLines *probe = stores;

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

		Boomish[NumBoomish].FirstLinetype = firsttype;
		Boomish[NumBoomish].LastLinetype = lasttype;
		Boomish[NumBoomish].NewSpecial = special;
		
		for (i = 0, probe = stores; probe != NULL; i++)
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

boom_body(A) ::= . /* empty */
{
	A = NULL;
}
boom_body(A) ::= boom_line(B) boom_body(C).
{
	A = malloc (sizeof(MoreLines));
	A->next = C;
	A->arg = B;
}

boom_line(A) ::= boom_selector(sel) boom_op(op) boom_args(args).
{
	A.bDefined = 1;
	A.bOrExisting = (op == OR_EQUAL);
	A.bUseConstant = (args.filters == NULL);
	A.ArgNum = sel;
	A.ConstantValue = args.constant;
	A.AndValue = args.mask;

	if (args.filters != NULL)
	{
		int i;
		MoreFilters *probe;

		for (i = 0, probe = args.filters; probe != NULL; i++)
		{
			MoreFilters *next = probe->next;
			if (i < 15)
			{
				A.ResultFilter[i] = probe->filter.filter;
				A.ResultValue[i] = probe->filter.value;
			}
			else if (i == 15)
			{
				yyerror ("Lists can only have 15 elements");
			}
			free (probe);
			probe = next;
		}
		A.ListSize = i > 15 ? 15 : i;
	}
}

boom_selector(A) ::= FLAGS.		{ A = 4; }
boom_selector(A) ::= ARG2.		{ A = 0; }
boom_selector(A) ::= ARG3.		{ A = 1; }
boom_selector(A) ::= ARG4.		{ A = 2; }
boom_selector(A) ::= ARG5.		{ A = 3; }

boom_op(A) ::= EQUALS.			{ A = '='; }
boom_op(A) ::= OR_EQUAL.		{ A = OR_EQUAL; }

boom_args(A) ::= exp(B).
{
	A.constant = B;
	A.filters = NULL;
}
boom_args(A) ::= exp(B) LBRACKET arg_list(C) RBRACKET.
{
	A.mask = B;
	A.filters = C;
}

arg_list(A) ::= list_val(B).
{
	A = malloc (sizeof(MoreFilters));
	A->next = NULL;
	A->filter = B;
}
arg_list(A) ::= list_val(B) COMMA arg_list(C).
{
	A = malloc (sizeof(MoreFilters));
	A->next = C;
	A->filter = B;
}

list_val(A) ::= exp(B) COLON exp(C).
{
	A.filter = B;
	A.value = C;
}

special_args(Z) ::= . /* empty */
{
	Z.addflags = 0;
	memset (Z.args, 0, 5);
}
special_args(Z) ::= TAG.
{
	Z.addflags = SIMPLE_HASTAGAT1;
	memset (Z.args, 0, 5);
}
special_args(Z) ::= TAG COMMA exp(B).
{
	Z.addflags = SIMPLE_HASTAGAT1;
	Z.args[0] = 0;
	Z.args[1] = B;
	Z.args[2] = 0;
	Z.args[3] = 0;
	Z.args[4] = 0;
}
special_args(Z) ::= TAG COMMA exp(B) COMMA exp(C).
{
	Z.addflags = SIMPLE_HASTAGAT1;
	Z.args[0] = 0;
	Z.args[1] = B;
	Z.args[2] = C;
	Z.args[3] = 0;
	Z.args[4] = 0;
}
special_args(Z) ::= TAG COMMA exp(B) COMMA exp(C) COMMA exp(D).
{
	Z.addflags = SIMPLE_HASTAGAT1;
	Z.args[0] = 0;
	Z.args[1] = B;
	Z.args[2] = C;
	Z.args[3] = D;
	Z.args[4] = 0;
}
special_args(Z) ::= TAG COMMA exp(B) COMMA exp(C) COMMA exp(D) COMMA exp(E).
{
	Z.addflags = SIMPLE_HASTAGAT1;
	Z.args[0] = 0;
	Z.args[1] = B;
	Z.args[2] = C;
	Z.args[3] = D;
	Z.args[4] = E;
}
special_args(Z) ::= TAG COMMA TAG.
{
	Z.addflags = SIMPLE_HAS2TAGS;
	Z.args[0] = Z.args[1] = 0;
	Z.args[2] = 0;
	Z.args[3] = 0;
	Z.args[4] = 0;
}
special_args(Z) ::= TAG COMMA TAG COMMA exp(C).
{
	Z.addflags = SIMPLE_HAS2TAGS;
	Z.args[0] = Z.args[1] = 0;
	Z.args[2] = C;
	Z.args[3] = 0;
	Z.args[4] = 0;
}
special_args(Z) ::= TAG COMMA TAG COMMA exp(C) COMMA exp(D).
{
	Z.addflags = SIMPLE_HAS2TAGS;
	Z.args[0] = Z.args[1] = 0;
	Z.args[2] = C;
	Z.args[3] = D;
	Z.args[4] = 0;
}
special_args(Z) ::= TAG COMMA TAG COMMA exp(C) COMMA exp(D) COMMA exp(E).
{
	Z.addflags = SIMPLE_HAS2TAGS;
	Z.args[0] = Z.args[1] = 0;
	Z.args[2] = C;
	Z.args[3] = D;
	Z.args[4] = E;
}
special_args(Z) ::= LINEID.
{
	Z.addflags = SIMPLE_HASLINEID;
	memset (Z.args, 0, 5);
}
special_args(Z) ::= LINEID COMMA exp(B).
{
	Z.addflags = SIMPLE_HASLINEID;
	Z.args[0] = 0;
	Z.args[1] = B;
	Z.args[2] = 0;
	Z.args[3] = 0;
	Z.args[4] = 0;
}
special_args(Z) ::= LINEID COMMA exp(B) COMMA exp(C).
{
	Z.addflags = SIMPLE_HASLINEID;
	Z.args[0] = 0;
	Z.args[1] = B;
	Z.args[2] = C;
	Z.args[3] = 0;
	Z.args[4] = 0;
}
special_args(Z) ::= LINEID COMMA exp(B) COMMA exp(C) COMMA exp(D).
{
	Z.addflags = SIMPLE_HASLINEID;
	Z.args[0] = 0;
	Z.args[1] = B;
	Z.args[2] = C;
	Z.args[3] = D;
	Z.args[4] = 0;
}
special_args(Z) ::= LINEID COMMA exp(B) COMMA exp(C) COMMA exp(D) COMMA exp(E).
{
	Z.addflags = SIMPLE_HASLINEID;
	Z.args[0] = 0;
	Z.args[1] = B;
	Z.args[2] = C;
	Z.args[3] = D;
	Z.args[4] = E;
}
special_args(Z) ::= exp(A).
{
	Z.addflags = 0;
	Z.args[0] = A;
	Z.args[1] = 0;
	Z.args[2] = 0;
	Z.args[3] = 0;
	Z.args[4] = 0;
}
special_args(Z) ::= exp(A) COMMA exp(B).
{
	Z.addflags = 0;
	Z.args[0] = A;
	Z.args[1] = B;
	Z.args[2] = 0;
	Z.args[3] = 0;
	Z.args[4] = 0;
}
special_args(Z) ::= exp(A) COMMA exp(B) COMMA exp(C).
{
	Z.addflags = 0;
	Z.args[0] = A;
	Z.args[1] = B;
	Z.args[2] = C;
	Z.args[3] = 0;
	Z.args[4] = 0;
}
special_args(Z) ::= exp(A) COMMA exp(B) COMMA exp(C) COMMA exp(D).
{
	Z.addflags = 0;
	Z.args[0] = A;
	Z.args[1] = B;
	Z.args[2] = C;
	Z.args[3] = D;
	Z.args[4] = 0;
}
special_args(Z) ::= exp(A) COMMA exp(B) COMMA exp(C) COMMA exp(D) COMMA exp(E).
{
	Z.addflags = 0;
	Z.args[0] = A;
	Z.args[1] = B;
	Z.args[2] = C;
	Z.args[3] = D;
	Z.args[4] = E;
}
special_args(Z) ::= exp(A) COMMA TAG.
{
	Z.addflags = SIMPLE_HASTAGAT2;
	Z.args[0] = A;
	Z.args[1] = 0;
	Z.args[2] = 0;
	Z.args[3] = 0;
	Z.args[4] = 0;
}
special_args(Z) ::= exp(A) COMMA TAG COMMA exp(C).
{
	Z.addflags = SIMPLE_HASTAGAT2;
	Z.args[0] = A;
	Z.args[1] = 0;
	Z.args[2] = C;
	Z.args[3] = 0;
	Z.args[4] = 0;
}
special_args(Z) ::= exp(A) COMMA TAG COMMA exp(C) COMMA exp(D).
{
	Z.addflags = SIMPLE_HASTAGAT2;
	Z.args[0] = A;
	Z.args[1] = 0;
	Z.args[2] = C;
	Z.args[3] = D;
	Z.args[4] = 0;
}
special_args(Z) ::= exp(A) COMMA TAG COMMA exp(C) COMMA exp(D) COMMA exp(E).
{
	Z.addflags = SIMPLE_HASTAGAT2;
	Z.args[0] = A;
	Z.args[1] = 0;
	Z.args[2] = C;
	Z.args[3] = D;
	Z.args[4] = E;
}
special_args(Z) ::= exp(A) COMMA exp(B) COMMA TAG.
{
	Z.addflags = SIMPLE_HASTAGAT3;
	Z.args[0] = A;
	Z.args[1] = B;
	Z.args[2] = 0;
	Z.args[3] = 0;
	Z.args[4] = 0;
}
special_args(Z) ::= exp(A) COMMA exp(B) COMMA TAG COMMA exp(D).
{
	Z.addflags = SIMPLE_HASTAGAT3;
	Z.args[0] = A;
	Z.args[1] = B;
	Z.args[2] = 0;
	Z.args[3] = D;
	Z.args[4] = 0;
}
special_args(Z) ::= exp(A) COMMA exp(B) COMMA TAG COMMA exp(D) COMMA exp(E).
{
	Z.addflags = SIMPLE_HASTAGAT3;
	Z.args[0] = A;
	Z.args[1] = B;
	Z.args[2] = 0;
	Z.args[3] = D;
	Z.args[4] = E;
}
special_args(Z) ::= exp(A) COMMA exp(B) COMMA exp(C) COMMA TAG.
{
	Z.addflags = SIMPLE_HASTAGAT4;
	Z.args[0] = A;
	Z.args[1] = B;
	Z.args[2] = C;
	Z.args[3] = 0;
	Z.args[4] = 0;
}
special_args(Z) ::= exp(A) COMMA exp(B) COMMA exp(C) COMMA TAG COMMA exp(E).
{
	Z.addflags = SIMPLE_HASTAGAT4;
	Z.args[0] = A;
	Z.args[1] = B;
	Z.args[2] = C;
	Z.args[3] = 0;
	Z.args[4] = E;
}
special_args(Z) ::= exp(A) COMMA exp(B) COMMA exp(C) COMMA exp(D) COMMA TAG.
{
	Z.addflags = SIMPLE_HASTAGAT5;
	Z.args[0] = A;
	Z.args[1] = B;
	Z.args[2] = C;
	Z.args[3] = D;
	Z.args[4] = 0;
}

/* You need Bison (or maybe Yacc) to rebuild the .c file from this grammar.
 * Or you could rewrite it to use Lemon, since the ZDoom source comes with Lemon.
 * Since I have Bison, and the existing code works fine, there's no motivation
 * for me to rewrite it.
 */
 
%{
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

%}

%union {
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
}

%token <val> NUM
%token <symval> SYMNUM
%token <sym> SYM
%token <string> STRING

%type <val> exp
%type <args> special_args
%type <filter> list_val
%type <filter_p> arg_list
%type <boomarg> boom_args
%type <val> boom_op
%type <val> boom_selector
%type <boomline> boom_line
%type <boomlines> boom_body
%type <val> maybe_argcount

%token DEFINE
%token INCLUDE
%token TAG
%token LINEID
%token SPECIAL
%token FLAGS
%token ARG2
%token ARG3
%token ARG4
%token ARG5
%token OR_EQUAL
%token ENUM
%token PRINT
%token ENDL

%left '|'
%left '^'
%left '&'
%left '-' '+'
%left '*' '/'
%left NEG

%start translation_unit

%%

exp:
		  NUM
		| SYMNUM			{ $$ = $1->Value; }
		| exp '+' exp		{ $$ = $1 + $3; }
		| exp '-' exp		{ $$ = $1 - $3; }
		| exp '*' exp		{ $$ = $1 * $3; }
		| exp '/' exp		{ $$ = $1 / $3; }
		| exp '|' exp		{ $$ = $1 | $3; }
		| exp '&' exp		{ $$ = $1 & $3; }
		| exp '^' exp		{ $$ = $1 ^ $3; }
		| '-' exp	%prec NEG	{ $$ = -$2 }
		| '(' exp ')'		{ $$ = $2; }
;

translation_unit:	/* empty line */
		| translation_unit external_declaration
;

external_declaration:
		  define_statement
		| include_statement
		| print_statement
		| enum_statement
		| linetype_declaration
		| boom_declaration
		| special_declaration
;

print_statement:
		  PRINT '(' print_list ')'
		  {
			  printf ("\n");
		  }
;

print_list:
		  /* EMPTY */
		| print_item
		| print_item ',' print_list
;

print_item:
		  STRING { printf ("%s", $1); }
		| exp { printf ("%d", $1); }
		| ENDL { printf ("\n"); }
;

define_statement:
		DEFINE SYM '(' exp ')' { AddSym ($2, $4); }
;

include_statement:
		INCLUDE STRING { IncludeFile ($2); }
;

enum_statement:
		ENUM '{' { EnumVal = 0; } enum_list '}'
;

enum_list:
		/* empty */
		| single_enum
		| single_enum ',' enum_list
;

single_enum:
		  SYM { AddSym ($1, EnumVal++); }
		| SYM '=' exp { AddSym ($1, EnumVal = $3); }
;

/* special declarations work just like they do for ACS, so
 * specials can be defined just by including zspecial.acs
 */
special_declaration:
		  SPECIAL special_list ';'
;

special_list:
		  /* empty */
		| special_def
		| special_def ',' special_list
;

special_def:
		  exp ':' SYM '(' maybe_argcount ')' { AddSym ($3, $1); }
		| exp ':' SYMNUM
			{ printf ("%s, line %d: %s is already defined\n", SourceName, SourceLine, $3->Sym); }
		  '(' maybe_argcount ')'
;

maybe_argcount:
		  /* empty */	{ $$ = 0; }
		| exp
		| exp ',' exp
;

linetype_declaration:
		  exp '=' exp ',' exp '(' special_args ')'
		  {
			  Simple[$1].NewSpecial = $5;
			  Simple[$1].Flags = $3 | $7.addflags;
			  Simple[$1].Args[0] = $7.args[0];
			  Simple[$1].Args[1] = $7.args[1];
			  Simple[$1].Args[2] = $7.args[2];
			  Simple[$1].Args[3] = $7.args[3];
			  Simple[$1].Args[4] = $7.args[4];
		  }
		| exp '=' exp ',' SYM '(' special_args ')'
		{
			printf ("%s, line %d: %s is undefined\n", SourceName, SourceLine, $5);
		}
;

boom_declaration:
		'[' exp ']' '(' exp ',' exp ')' '{' boom_body '}'
		{
			if (NumBoomish == MAX_BOOMISH)
			{
				MoreLines *probe = $10;

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

				Boomish[NumBoomish].FirstLinetype = $5;
				Boomish[NumBoomish].LastLinetype = $7;
				Boomish[NumBoomish].NewSpecial = $2;
				
				for (i = 0, probe = $10; probe != NULL; i++)
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
;

boom_body:
		/* empty */
		{
			$$ = NULL;
		}
		| boom_line boom_body
		{
			$$ = malloc (sizeof(MoreLines));
			$$->next = $2;
			$$->arg = $1;
		}
;

boom_line:
		boom_selector boom_op boom_args
		{
			$$.bDefined = 1;
			$$.bOrExisting = ($2 == OR_EQUAL);
			$$.bUseConstant = ($3.filters == NULL);
			$$.ArgNum = $1;
			$$.ConstantValue = $3.constant;
			$$.AndValue = $3.mask;

			if ($3.filters != NULL)
			{
				int i;
				MoreFilters *probe;

				for (i = 0, probe = $3.filters; probe != NULL; i++)
				{
					MoreFilters *next = probe->next;
					if (i < 15)
					{
						$$.ResultFilter[i] = probe->filter.filter;
						$$.ResultValue[i] = probe->filter.value;
					}
					else if (i == 15)
					{
						yyerror ("Lists can only have 15 elements");
					}
					free (probe);
					probe = next;
				}
				$$.ListSize = i > 15 ? 15 : i;
			}
		}
;

boom_selector:
		  FLAGS { $$ = 4; }
		| ARG2  { $$ = 0; }
		| ARG3  { $$ = 1; }
		| ARG4  { $$ = 2; }
		| ARG5  { $$ = 3; }
;

boom_op:
		  '=' { $$ = '='; }
		| OR_EQUAL { $$ = OR_EQUAL; }
;

boom_args:
		  exp
		  {
			  $$.constant = $1;
			  $$.filters = NULL;
		  }
		| exp '[' arg_list ']'
		{
			$$.mask = $1;
			$$.filters = $3;
		}
;

arg_list:
		  list_val
		  {
			  $$ = malloc (sizeof (MoreFilters));
			  $$->next = NULL;
			  $$->filter = $1;
		  }
		| list_val ',' arg_list
		{
			$$ = malloc (sizeof (MoreFilters));
			$$->next = $3;
			$$->filter = $1;
		}
;

list_val:
		  exp ':' exp	{ $$.filter = $1; $$.value = $3; }
;

special_args:
		/* EMPTY LINE */
		{
			$$.addflags = 0;
			memset ($$.args, 0, 5);
		}
		| TAG
		{
			$$.addflags = SIMPLE_HASTAGAT1;
			memset ($$.args, 0, 5);
		}
		| TAG ',' exp
		{
			$$.addflags = SIMPLE_HASTAGAT1;
			$$.args[0] = 0;
			$$.args[1] = $3;
			$$.args[2] = 0;
			$$.args[3] = 0;
			$$.args[4] = 0;
		}
		| TAG ',' exp ',' exp
		{
			$$.addflags = SIMPLE_HASTAGAT1;
			$$.args[0] = 0;
			$$.args[1] = $3;
			$$.args[2] = $5;
			$$.args[3] = 0;
			$$.args[4] = 0;
		}
		| TAG ',' exp ',' exp ',' exp
		{
			$$.addflags = SIMPLE_HASTAGAT1;
			$$.args[0] = 0;
			$$.args[1] = $3;
			$$.args[2] = $5;
			$$.args[3] = $7;
			$$.args[4] = 0;
		}
		| TAG ',' exp ',' exp ',' exp ',' exp
		{
			$$.addflags = SIMPLE_HASTAGAT1;
			$$.args[0] = 0;
			$$.args[1] = $3;
			$$.args[2] = $5;
			$$.args[3] = $7;
			$$.args[4] = $9;
		}

		| TAG ',' TAG
		{
			$$.addflags = SIMPLE_HAS2TAGS;
			$$.args[0] = $$.args[1] = 0;
			$$.args[2] = 0;
			$$.args[3] = 0;
			$$.args[4] = 0;
		}
		| TAG ',' TAG ',' exp
		{
			$$.addflags = SIMPLE_HAS2TAGS;
			$$.args[0] = $$.args[1] = 0;
			$$.args[2] = $5;
			$$.args[3] = 0;
			$$.args[4] = 0;
		}
		| TAG ',' TAG ',' exp ',' exp
		{
			$$.addflags = SIMPLE_HAS2TAGS;
			$$.args[0] = $$.args[1] = 0;
			$$.args[2] = $5;
			$$.args[3] = $7;
			$$.args[4] = 0;
		}
		| TAG ',' TAG ',' exp ',' exp ',' exp
		{
			$$.addflags = SIMPLE_HAS2TAGS;
			$$.args[0] = $$.args[1] = 0;
			$$.args[2] = $5;
			$$.args[3] = $7;
			$$.args[4] = $9;
		}

		| LINEID
		{
			$$.addflags = SIMPLE_HASLINEID;
			memset ($$.args, 0, 5);
		}
		| LINEID ',' exp
		{
			$$.addflags = SIMPLE_HASLINEID;
			$$.args[0] = 0;
			$$.args[1] = $3;
			$$.args[2] = 0;
			$$.args[3] = 0;
			$$.args[4] = 0;
		}
		| LINEID ',' exp ',' exp
		{
			$$.addflags = SIMPLE_HASLINEID;
			$$.args[0] = 0;
			$$.args[1] = $3;
			$$.args[2] = $5;
			$$.args[3] = 0;
			$$.args[4] = 0;
		}
		| LINEID ',' exp ',' exp ',' exp
		{
			$$.addflags = SIMPLE_HASLINEID;
			$$.args[0] = 0;
			$$.args[1] = $3;
			$$.args[2] = $5;
			$$.args[3] = $7;
			$$.args[4] = 0;
		}
		| LINEID ',' exp ',' exp ',' exp ',' exp
		{
			$$.addflags = SIMPLE_HASLINEID;
			$$.args[0] = 0;
			$$.args[1] = $3;
			$$.args[2] = $5;
			$$.args[3] = $7;
			$$.args[4] = $9;
		}

		| exp
		{
			$$.addflags = 0;
			$$.args[0] = $1;
			$$.args[1] = 0;
			$$.args[2] = 0;
			$$.args[3] = 0;
			$$.args[4] = 0;
		}
		| exp ',' exp
		{
			$$.addflags = 0;
			$$.args[0] = $1;
			$$.args[1] = $3;
			$$.args[2] = 0;
			$$.args[3] = 0;
			$$.args[4] = 0;
		}
		| exp ',' exp ',' exp
		{
			$$.addflags = 0;
			$$.args[0] = $1;
			$$.args[1] = $3;
			$$.args[2] = $5;
			$$.args[3] = 0;
			$$.args[4] = 0;
		}
		| exp ',' exp ',' exp ',' exp
		{
			$$.addflags = 0;
			$$.args[0] = $1;
			$$.args[1] = $3;
			$$.args[2] = $5;
			$$.args[3] = $7;
			$$.args[4] = 0;
		}
		| exp ',' exp ',' exp ',' exp ',' exp
		{
			$$.addflags = 0;
			$$.args[0] = $1;
			$$.args[1] = $3;
			$$.args[2] = $5;
			$$.args[3] = $7;
			$$.args[4] = $9;
		}

		| exp ',' TAG
		{
			$$.addflags = SIMPLE_HASTAGAT2;
			$$.args[0] = $1;
			$$.args[1] = 0;
			$$.args[2] = 0;
			$$.args[3] = 0;
			$$.args[4] = 0;
		}
		| exp ',' TAG ',' exp
		{
			$$.addflags = SIMPLE_HASTAGAT2;
			$$.args[0] = $1;
			$$.args[1] = 0;
			$$.args[2] = $5;
			$$.args[3] = 0;
			$$.args[4] = 0;
		}
		| exp ',' TAG ',' exp ',' exp
		{
			$$.addflags = SIMPLE_HASTAGAT2;
			$$.args[0] = $1;
			$$.args[1] = 0;
			$$.args[2] = $5;
			$$.args[3] = $7;
			$$.args[4] = 0;
		}
		| exp ',' TAG ',' exp ',' exp ',' exp
		{
			$$.addflags = SIMPLE_HASTAGAT2;
			$$.args[0] = $1;
			$$.args[1] = 0;
			$$.args[2] = $5;
			$$.args[3] = $7;
			$$.args[4] = $9;
		}

		| exp ',' exp ',' TAG
		{
			$$.addflags = SIMPLE_HASTAGAT3;
			$$.args[0] = $1;
			$$.args[1] = $3;
			$$.args[2] = 0;
			$$.args[3] = 0;
			$$.args[4] = 0;
		}
		| exp ',' exp ',' TAG ',' exp
		{
			$$.addflags = SIMPLE_HASTAGAT3;
			$$.args[0] = $1;
			$$.args[1] = $3;
			$$.args[2] = 0;
			$$.args[3] = $7;
			$$.args[4] = 0;
		}
		| exp ',' exp ',' TAG ',' exp ',' exp
		{
			$$.addflags = SIMPLE_HASTAGAT3;
			$$.args[0] = $1;
			$$.args[1] = $3;
			$$.args[2] = 0;
			$$.args[3] = $7;
			$$.args[4] = $9;
		}

		| exp ',' exp ',' exp ',' TAG
		{
			$$.addflags = SIMPLE_HASTAGAT4;
			$$.args[0] = $1;
			$$.args[1] = $3;
			$$.args[2] = $5;
			$$.args[3] = 0;
			$$.args[4] = 0;
		}
		| exp ',' exp ',' exp ',' TAG ',' exp
		{
			$$.addflags = SIMPLE_HASTAGAT4;
			$$.args[0] = $1;
			$$.args[1] = $3;
			$$.args[2] = $5;
			$$.args[3] = 0;
			$$.args[4] = $9;
		}

		| exp ',' exp ',' exp ',' exp ',' TAG
		{
			$$.addflags = SIMPLE_HASTAGAT5;
			$$.args[0] = $1;
			$$.args[1] = $3;
			$$.args[2] = $5;
			$$.args[3] = $7;
			$$.args[4] = 0;
		}
;

%%

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

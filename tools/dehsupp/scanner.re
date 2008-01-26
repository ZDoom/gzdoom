#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include "dehsupp.h"

#define	BSIZE	8192

#define	YYCTYPE		uchar
#define	YYCURSOR	cursor
#define	YYLIMIT		s->lim
#define	YYMARKER	s->ptr
#define	YYFILL(n)	{cursor = fill(s, cursor);}

#define	RET(i)	{s->cur = cursor; return i;}

uchar *fill(Scanner *s, uchar *cursor)
{
	if(!s->eof)
	{
		ptrdiff_t cnt = s->tok - s->bot;
		if(cnt)
		{
			memcpy(s->bot, s->tok, s->lim - s->tok);
			s->tok = s->bot;
			s->ptr -= cnt;
			cursor -= cnt;
			s->pos -= cnt;
			s->lim -= cnt;
		}
		if((s->top - s->lim) < BSIZE)
		{
			uchar *buf = (uchar*) malloc(((s->lim - s->bot) + BSIZE)*sizeof(uchar));
			memcpy(buf, s->tok, s->lim - s->tok);
			s->tok = buf;
			s->ptr = &buf[s->ptr - s->bot];
			cursor = &buf[cursor - s->bot];
			s->pos = &buf[s->pos - s->bot];
			s->lim = &buf[s->lim - s->bot];
			s->top = &s->lim[BSIZE];
			free(s->bot);
			s->bot = buf;
		}
		if((cnt = fread((char*) s->lim, 1, BSIZE, s->fd)) != BSIZE)
		{
			s->eof = &s->lim[cnt]; *(s->eof)++ = '\n';
		}
		s->lim += cnt;
	}
	return cursor;
}

int scan(Scanner *s)
{
	uchar *cursor = s->cur;
std:
	s->tok = cursor;
/*!re2c
any	= [\000-\377];
O	= [0-7];
D	= [0-9];
L	= [a-zA-Z_];
H	= [a-fA-F0-9];
ESC	= [\\] ([abfnrtv?'"\\] | "x" H+ | O+);
*/

/*!re2c
	"/*"			{ goto comment; }	/* C comment */
	"//" (any\"\n")* "\n"				/* C++ comment */
		{
		if(cursor == s->eof) RET(EOI);
		s->tok = s->pos = cursor; s->line++;
		goto std;
		}

	"endl"			{ RET(ENDL); }
	"print"			{ RET(PRINT); }
	"Actions"		{ RET(Actions); }
	"OrgHeights"	{ RET(OrgHeights); }
	"ActionList"	{ RET(ActionList); }
	"CodePConv"		{ RET(CodePConv); }
	"OrgSprNames"	{ RET(OrgSprNames); }
	"StateMap"		{ RET(StateMap); }
	"SoundMap"		{ RET(SoundMap); }
	"InfoNames"		{ RET(InfoNames); }
	"ThingBits"		{ RET(ThingBits); }
	"DeathState"	{ RET(DeathState); }
	"SpawnState"	{ RET(SpawnState); }
	"FirstState"	{ RET(FirstState); }
	"RenderStyles"	{ RET(RenderStyles); }

	L (L|D)*		{ RET(SYM); }

	("0" [xX] H+) | ("0" D+) | (D+)
					{ RET(NUM); }

	(["] (ESC|any\[\n\\"])* ["])
					{ RET(STRING); }

	[ \t\v\f]+		{ goto std; }

	"|"				{ RET(OR); }
	"^"				{ RET(XOR); }
	"&"				{ RET(AND); }
	"-"				{ RET(MINUS); }
	"+"				{ RET(PLUS); }
	"*"				{ RET(MULTIPLY); }
	"/"				{ RET(DIVIDE); }
	"("				{ RET(LPAREN); }
	")"				{ RET(RPAREN); }
	","				{ RET(COMMA); }
	"{"				{ RET(LBRACE); }
	"}"				{ RET(RBRACE); }
	";"				{ RET(SEMICOLON); }


	"\n"
		{
		if(cursor == s->eof) RET(EOI);
		s->pos = cursor; s->line++;
		goto std;
		}

	any
		{
		printf("unexpected character: %c\n", *s->tok);
		goto std;
		}
*/

comment:
/*!re2c
	"*/"			{ goto std; }
	"\n"
		{
		if(cursor == s->eof) RET(EOI);
		s->tok = s->pos = cursor; s->line++;
		goto comment;
		}
		any			{ goto comment; }
*/
}

int lex(Scanner *s, struct Token *tok)
{
	int tokentype = scan(s);
	char *p, *q;

	tok->val = 0;
	tok->string = NULL;

	switch (tokentype)
	{
	case NUM:
		tok->val = strtol((char *)s->tok, NULL, 0);
		break;

	case STRING:
		tok->string = (char *)malloc(s->cur - s->tok - 1);
		strncpy(tok->string, (char *)s->tok + 1, s->cur - s->tok - 2);
		tok->string[s->cur - s->tok - 2] = '\0';
		for (p = q = tok->string; *p; ++p, ++q)
		{
			if (p[0] == '\\' && p[1] == '\\')
				++p;
			*q = *p;
		}
		break;

	case SYM:
		tok->string = (char *)malloc(s->cur - s->tok + 1);
		strncpy(tok->string, (char *)s->tok, s->cur - s->tok);
		tok->string[s->cur - s->tok] = '\0';
		break;
	}
	return tokentype;
}

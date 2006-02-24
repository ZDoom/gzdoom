/* $Id: scanner.re,v 1.10 2004/05/13 02:58:18 nuffer Exp $ */
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include "scanner.h"
#include "parser.h"
#include "y.tab.h"

extern YYSTYPE yylval;

#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

#define	BSIZE	8192

#define	YYCTYPE		char
#define	YYCURSOR	cursor
#define	YYLIMIT		lim
#define	YYMARKER	ptr
#define	YYFILL(n)	{cursor = fill(cursor);}

#define	RETURN(i)	{cur = cursor; return i;}


Scanner::Scanner(std::istream& i) : in(i),
	bot(NULL), tok(NULL), ptr(NULL), cur(NULL), pos(NULL), lim(NULL),
	top(NULL), eof(NULL), tchar(0), tline(0), cline(1) {
    ;
}

char *Scanner::fill(char *cursor){
    if(!eof){
	uint cnt = tok - bot;
	if(cnt){
	    memcpy(bot, tok, lim - tok);
	    tok = bot;
	    ptr -= cnt;
	    cursor -= cnt;
	    pos -= cnt;
	    lim -= cnt;
	}
	if((top - lim) < BSIZE){
	    char *buf = new char[(lim - bot) + BSIZE];
	    memcpy(buf, tok, lim - tok);
	    tok = buf;
	    ptr = &buf[ptr - bot];
	    cursor = &buf[cursor - bot];
	    pos = &buf[pos - bot];
	    lim = &buf[lim - bot];
	    top = &lim[BSIZE];
	    delete [] bot;
	    bot = buf;
	}
	if((cnt = in.rdbuf()->sgetn((char*) lim, BSIZE)) != BSIZE){
	    eof = &lim[cnt]; *eof++ = '\n';
	}
	lim += cnt;
    }
    return cursor;
}

/*!re2c
any		= [\000-\377];
dot		= any \ [\n];
esc		= dot \ [\\];
cstring		= "["  ((esc \ [\]]) | "\\" dot)* "]" ;
dstring		= "\"" ((esc \ ["] ) | "\\" dot)* "\"";
sstring		= "'"  ((esc \ ['] ) | "\\" dot)* "'" ;
letter		= [a-zA-Z];
digit		= [0-9];
*/

int Scanner::echo(std::ostream &out){
    char *cursor = cur;

    // Catch EOF
    if (eof && cursor == eof)
    	return 0;

    tok = cursor;
echo:
/*!re2c
	"/*!re2c"		{ out.write((const char*)(tok), (const char*)(&cursor[-7]) - (const char*)(tok));
				  tok = cursor;
				  RETURN(1); }
	"\n"			{ if(cursor == eof) RETURN(0);
				  out.write((const char*)(tok), (const char*)(cursor) - (const char*)(tok));
				  tok = pos = cursor; cline++;
				  goto echo; }
        any			{ goto echo; }
*/
}


int Scanner::scan(){
    char *cursor = cur;
    uint depth;

scan:
    tchar = cursor - pos;
    tline = cline;
    tok = cursor;
/*!re2c
	"{"			{ depth = 1;
				  goto code;
				}
	"/*"			{ depth = 1;
				  goto comment; }

	"*/"			{ tok = cursor;
				  RETURN(0); }

	dstring			{ cur = cursor;
				  yylval.regexp = strToRE(token());
				  return STRING; }

	sstring			{ cur = cursor;
				  yylval.regexp = strToCaseInsensitiveRE(token());
				  return STRING; }

	"\""			{ fatal("unterminated string constant (missing \")"); }
	"'"				{ fatal("unterminated string constant (missing ')"); }

	cstring			{ cur = cursor;
				  yylval.regexp = ranToRE(token());
				  return RANGE; }

	"["			{ fatal("unterminated range (missing ])"); }

	[()|=;/\\]		{ RETURN(*tok); }

	[*+?]			{ yylval.op = *tok;
				  RETURN(CLOSE); }

	"{" [0-9]+ "}"		{ yylval.extop.minsize = atoi((char *)tok+1);
				  yylval.extop.maxsize = atoi((char *)tok+1);
				  RETURN(CLOSESIZE); }

	"{" [0-9]+ "," [0-9]+ "}"	{ yylval.extop.minsize = atoi((char *)tok+1);
				  yylval.extop.maxsize = MAX(yylval.extop.minsize,atoi(strchr((char *)tok, ',')+1));
				  RETURN(CLOSESIZE); }

	"{" [0-9]+ ",}"		{ yylval.extop.minsize = atoi((char *)tok+1);
				  yylval.extop.maxsize = -1;
				  RETURN(CLOSESIZE); }

	letter (letter|digit)*	{ cur = cursor;
				  yylval.symbol = Symbol::find(token());
				  return ID; }

	[ \t]+			{ goto scan; }

	"\n"			{ if(cursor == eof) RETURN(0);
				  pos = cursor; cline++;
				  goto scan;
	    			}

	any			{ std::cerr << "unexpected character: " << *tok << std::endl;
				  goto scan;
				}
*/

code:
/*!re2c
	"}"			{ if(--depth == 0){
					cur = cursor;
					yylval.token = new Token(token(), tline);
					return CODE;
				  }
				  goto code; }
	"{"			{ ++depth;
				  goto code; }
	"\n"			{ if(cursor == eof) fatal("missing '}'");
				  pos = cursor; cline++;
				  goto code;
				}
	dstring | sstring | any	{ goto code; }
*/

comment:
/*!re2c
	"*/"			{ if(--depth == 0)
					goto scan;
				    else
					goto comment; }
	"/*"			{ ++depth;
				  goto comment; }
	"\n"			{ if(cursor == eof) RETURN(0);
				  tok = pos = cursor; cline++;
				  goto comment;
				}
        any			{ goto comment; }
*/
}

void Scanner::fatal(char *msg){
    std::cerr << "line " << tline << ", column " << (tchar + 1) << ": "
	<< msg << std::endl;
    exit(1);
}

/* $Id: scanner.re 663 2007-04-01 11:22:15Z helly $ */
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include "scanner.h"
#include "parser.h"
#include "y.tab.h"
#include "globals.h"
#include "dfa.h"

extern YYSTYPE yylval;

#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

#define	BSIZE	8192

#define	YYCTYPE		unsigned char
#define	YYCURSOR	cursor
#define	YYLIMIT		lim
#define	YYMARKER	ptr
#define	YYFILL(n)	{cursor = fill(cursor);}

#define	RETURN(i)	{cur = cursor; return i;}

namespace re2c
{

Scanner::Scanner(const char *fn, std::istream& i, std::ostream& o)
	: in(i)
	, out(o)
	, bot(NULL), tok(NULL), ptr(NULL), cur(NULL), pos(NULL), lim(NULL)
	, top(NULL), eof(NULL), tchar(0), tline(0), cline(1), iscfg(0), filename(fn)
{
    ;
}

char *Scanner::fill(char *cursor)
{
	if(!eof)
	{
		uint cnt = tok - bot;
		if(cnt)
		{
			memcpy(bot, tok, lim - tok);
			tok = bot;
			ptr -= cnt;
			cursor -= cnt;
			pos -= cnt;
			lim -= cnt;
		}
		if((top - lim) < BSIZE)
		{
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
		in.read(lim, BSIZE);
		if ((cnt = in.gcount()) != BSIZE )
		{
			eof = &lim[cnt]; *eof++ = '\0';
		}
		lim += cnt;
	}
	return cursor;
}

/*!re2c
zero    = "\000";
any     = [\000-\377];
dot     = any \ [\n];
esc     = dot \ [\\];
istring = "[" "^" ((esc \ [\]]) | "\\" dot)* "]" ;
cstring = "["     ((esc \ [\]]) | "\\" dot)* "]" ;
dstring = "\""    ((esc \ ["] ) | "\\" dot)* "\"";
sstring = "'"     ((esc \ ['] ) | "\\" dot)* "'" ;
letter  = [a-zA-Z];
digit   = [0-9];
number  = "0" | ("-"? [1-9] digit*);
name    = (letter|"_") (letter|digit|"_")*;
cname   = ":" name;
space   = [ \t];
eol     = ("\r\n" | "\n");
config  = "re2c" cname+;
value   = [^\r\n; \t]* | dstring | sstring;
*/

int Scanner::echo()
{
    char *cursor = cur;
    bool ignore_eoc = false;
    int  ignore_cnt = 0;

    if (eof && cursor == eof) // Catch EOF
	{
    	return 0;
	}

    tok = cursor;
echo:
/*!re2c
	"/*!re2c"	{
					if (bUsedYYMaxFill && bSinglePass) {
						fatal("found scanner block after YYMAXFILL declaration");
					}
					out.write((const char*)(tok), (const char*)(&cursor[-7]) - (const char*)(tok));
					tok = cursor;
					RETURN(1);
				}
	"/*!max:re2c" {
					if (bUsedYYMaxFill) {
						fatal("cannot generate YYMAXFILL twice");
					}
					out << "#define YYMAXFILL " << maxFill << std::endl;
					tok = pos = cursor;
					ignore_eoc = true;
					bUsedYYMaxFill = true;
					goto echo;
				}
	"/*!getstate:re2c" {
					tok = pos = cursor;
					genGetState(out, topIndent, 0);
					ignore_eoc = true;
					goto echo;
				}
	"/*!ignore:re2c" {
					tok = pos = cursor;
					ignore_eoc = true;
					goto echo;
				}
	"*" "/"	"\r"? "\n"	{
					cline++;
					if (ignore_eoc) {
						if (ignore_cnt) {
							out << sourceFileInfo;
						}
						ignore_eoc = false;
						ignore_cnt = 0;
					} else {
						out.write((const char*)(tok), (const char*)(cursor) - (const char*)(tok));
					}
					tok = pos = cursor;
					goto echo;
				}
	"*" "/"		{
					if (ignore_eoc) {
						if (ignore_cnt) {
							out << "\n" << sourceFileInfo;
						}
						ignore_eoc = false;
						ignore_cnt = 0;
					} else {
						out.write((const char*)(tok), (const char*)(cursor) - (const char*)(tok));
					}
					tok = pos = cursor;
					goto echo;
				}
	"\n"		{
					if (ignore_eoc) {
						ignore_cnt++;
					} else {
						out.write((const char*)(tok), (const char*)(cursor) - (const char*)(tok));
					}
					tok = pos = cursor; cline++;
				  	goto echo;
				}
	zero		{
					if (!ignore_eoc) {
						out.write((const char*)(tok), (const char*)(cursor) - (const char*)(tok) - 1); // -1 so we don't write out the \0
					}
					if(cursor == eof) {
						RETURN(0);
					}
				}
	any			{
					goto echo;
				}
*/
}


int Scanner::scan()
{
    char *cursor = cur;
    uint depth;

scan:
    tchar = cursor - pos;
    tline = cline;
    tok = cursor;
	if (iscfg == 1)
	{
		goto config;
	}
	else if (iscfg == 2)
	{
   		goto value;
    }
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

	istring			{ cur = cursor;
				  yylval.regexp = invToRE(token());
				  return RANGE; }

	cstring			{ cur = cursor;
				  yylval.regexp = ranToRE(token());
				  return RANGE; }

	"["			{ fatal("unterminated range (missing ])"); }

	[()|=;/\\]		{ RETURN(*tok); }

	[*+?]			{ yylval.op = *tok;
				  RETURN(CLOSE); }

	"{0,}"		{ yylval.op = '*';
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

	"{" [0-9]* ","		{ fatal("illegal closure form, use '{n}', '{n,}', '{n,m}' where n and m are numbers"); }

	config		{ cur = cursor;
				  tok+= 5; /* skip "re2c:" */
				  iscfg = 1;
				  yylval.str = new Str(token());
				  return CONFIG;
				}

	name		{ cur = cursor;
				  yylval.symbol = Symbol::find(token());
				  return ID; }

	"."			{ cur = cursor;
				  yylval.regexp = mkDot();
				  return RANGE;
				}

	space+			{ goto scan; }

	eol			{ if(cursor == eof) RETURN(0);
				  pos = cursor; cline++;
				  goto scan;
	    			}

	any			{ std::ostringstream msg;
				  msg << "unexpected character: ";
				  prtChOrHex(msg, *tok);
				  fatal(msg.str().c_str());
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
	"\n"		{ if(cursor == eof) fatal("missing '}'");
				  pos = cursor; cline++;
				  goto code;
				}
	zero		{ if(cursor == eof) {
					if (depth) fatal("missing '}'");
					RETURN(0);
				  }
				  goto code;
				}
	dstring | sstring | any	{ goto code; }
*/

comment:
/*!re2c
	"*/"		{ if(--depth == 0)
					goto scan;
				    else
					goto comment; }
	"/*"		{ ++depth;
				  fatal("ambiguous /* found");
				  goto comment; }
	"\n"		{ if(cursor == eof) RETURN(0);
				  tok = pos = cursor; cline++;
				  goto comment;
				}
	any			{ if(cursor == eof) RETURN(0);
				  goto comment; }
*/

config:
/*!re2c
	space+		{ goto config; }
	"=" space*	{ iscfg = 2;
				  cur = cursor;
				  RETURN('='); 
				}
	any			{ fatal("missing '='"); }
*/

value:
/*!re2c
	number		{ cur = cursor;
				  yylval.number = atoi(token().to_string().c_str());
				  iscfg = 0;
				  return NUMBER;
				}
	value		{ cur = cursor;
				  yylval.str = new Str(token());
				  iscfg = 0;
				  return VALUE;
				}
*/
}

void Scanner::fatal(uint ofs, const char *msg) const
{
	out.flush();
#ifdef _MSC_VER
	std::cerr << filename << "(" << tline << "): error : "
		<< "column " << (tchar + ofs + 1) << ": "
		<< msg << std::endl;
#else
	std::cerr << "re2c: error: "
		<< "line " << tline << ", column " << (tchar + ofs + 1) << ": "
		<< msg << std::endl;
#endif
   	exit(1);
}

Scanner::~Scanner()
{
	if (bot)
	{
		delete [] bot;
	}
}

} // end namespace re2c


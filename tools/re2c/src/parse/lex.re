#include "src/util/c99_stdint.h"
#include <stddef.h>
#include <string.h>
#include <algorithm>
#include <limits>
#include <string>

#include "src/codegen/output.h"
#include "src/conf/opt.h"
#include "src/conf/warn.h"
#include "src/globals.h"
#include "src/ir/regexp/encoding/enc.h"
#include "src/ir/regexp/regexp.h"
#include "src/ir/regexp/regexp_null.h"
#include "src/parse/code.h"
#include "src/parse/extop.h"
#include "src/parse/input.h"
#include "src/parse/scanner.h"
#include "src/parse/parser.h" // needed by "y.tab.h"
#include "src/parse/unescape.h"
#include "src/util/range.h"
#include "src/util/s_to_n32_unsafe.h"
#include "y.tab.h"

extern YYSTYPE yylval;

#define	YYCTYPE		unsigned char
#define	YYCURSOR	cur
#define	YYLIMIT		lim
#define	YYMARKER	ptr
#define	YYCTXMARKER ctx
#define	YYFILL(n)	{ fill (n); }

namespace re2c
{

// source code is in ASCII: pointers have type 'char *'
// but re2c makes an implicit assumption that YYCTYPE is unsigned
// when it generates comparisons
/*!re2c
	re2c:yych:conversion = 1;
*/

/*!re2c
zero    = "\000";
dstring = "\"" ((. \ [\\"] ) | "\\" .)* "\"";
sstring = "'"  ((. \ [\\'] ) | "\\" .)* "'" ;
letter  = [a-zA-Z];
digit   = [0-9];
lineno  = [1-9] digit*;
name    = (letter|digit|"_")+;
space   = [ \t];
ws      = (space | [\r\n]);
eol     = ("\r\n" | "\n");
lineinf = lineno (space+ dstring)? eol;

	esc = "\\";
	hex_digit = [0-9a-fA-F];
	esc_hex = esc ("x" hex_digit{2} | [uX] hex_digit{4} | "U" hex_digit{8});
	esc_oct = esc [0-3] [0-7]{2}; // max 1-byte octal value is '\377'
	esc_simple = esc [abfnrtv\\];
*/

Scanner::ParseMode Scanner::echo()
{
	bool ignore_eoc = false;
	int  ignore_cnt = 0;

	if (eof && cur == eof) // Catch EOF
	{
		return Stop;
	}

	tok = cur;
echo:
/*!re2c
   beginRE     =  "%{" | "/*!re2c";
   beginRE     {
					if (opts->rFlag)
					{
						fatal("found standard 're2c' block while using -r flag");
					}
					if (opts->target == opt_t::CODE)
					{
						const size_t lexeme_len = cur[-1] == '{'
							? sizeof ("%{") - 1
							: sizeof ("/*!re2c") - 1;
						out.wraw(tok, tok_len () - lexeme_len);
					}
					tok = cur;
					return Parse;
				}
	"/*!rules:re2c"	{
					if (opts->rFlag)
					{
						opts.reset_mapCodeName ();
					}
					else
					{
						fatal("found 'rules:re2c' block without -r flag");
					}
					tok = cur;
					return Rules;
				}
	"/*!use:re2c"	{
					if (!opts->rFlag)
					{
						fatal("found 'use:re2c' block without -r flag");
					}
					reuse();
					if (opts->target == opt_t::CODE)
					{
						const size_t lexeme_len = sizeof ("/*!use:re2c") - 1;
						out.wraw(tok, tok_len () - lexeme_len);
					}
					tok = cur;
					return Reuse;
				}
	"/*!max:re2c" {
					if (opts->target != opt_t::DOT)
					{
						out.wdelay_yymaxfill ();
					}
					tok = pos = cur;
					ignore_eoc = true;
					goto echo;
				}
	"/*!getstate:re2c" {
					tok = pos = cur;
					out.wdelay_state_goto (opts->topIndent);
					ignore_eoc = true;
					goto echo;
				}
	"/*!ignore:re2c" {
					tok = pos = cur;
					ignore_eoc = true;
					goto echo;
				}
	"/*!types:re2c" {
					tok = pos = cur;
					ignore_eoc = true;
					if (opts->target != opt_t::DOT)
					{
						out.wdelay_line_info ().ws("\n")
							.wdelay_types ().ws("\n")
							.wline_info (cline, get_fname ().c_str ());
					}
					goto echo;
				}
	"*" "/"	"\r"? "\n"	{
					cline++;
					if (ignore_eoc)
					{
						if (ignore_cnt)
						{
							out.wline_info (cline, get_fname ().c_str ());
						}
						ignore_eoc = false;
						ignore_cnt = 0;
					}
					else if (opts->target == opt_t::CODE)
					{
						out.wraw(tok, tok_len ());
					}
					tok = pos = cur;
					goto echo;
				}
	"*" "/"		{
					if (ignore_eoc)
					{
						if (ignore_cnt)
						{
							out.ws("\n").wline_info (cline, get_fname ().c_str ());
						}
						ignore_eoc = false;
						ignore_cnt = 0;
					}
					else if (opts->target == opt_t::CODE)
					{
						out.wraw(tok, tok_len ());
					}
					tok = pos = cur;
					goto echo;
				}
	"\n" space* "#" space* "line" space+ / lineinf {
					set_sourceline ();
					goto echo;
				}
	"\n"		{
					if (ignore_eoc)
					{
						ignore_cnt++;
					}
					else if (opts->target == opt_t::CODE)
					{
						out.wraw(tok, tok_len ());
					}
					tok = pos = cur;
					cline++;
					goto echo;
				}
	zero		{
					if (!ignore_eoc && opts->target == opt_t::CODE)
					{
						out.wraw(tok, tok_len () - 1);
						// -1 so we don't write out the \0
					}
					if(cur == eof)
					{
						return Stop;
					}
				}
	*			{
					goto echo;
				}
*/
}

int Scanner::scan()
{
	uint32_t depth;

scan:
	tchar = cur - pos;
	tline = cline;
	tok = cur;
	switch (lexer_state)
	{
		case LEX_NORMAL:    goto start;
		case LEX_FLEX_NAME: goto flex_name;
	}

start:
/*!re2c
	"{"			{
					depth = 1;
					goto code;
				}

	":" / "=>"	{
					return *tok;
				}

	":="		{
					tok += 2; /* skip ":=" */
					depth = 0;
					goto code;
				}

        "//"            {
				goto nextLine;
			}
	"/*"		{
					depth = 1;
					goto comment;
				}

   endRE    =  "%}" | "*/";
   endRE    {
					tok = cur;
					return 0;
				}

	"'"  { yylval.regexp = lex_str('\'', opts->bCaseInsensitive || !opts->bCaseInverted); return TOKEN_REGEXP; }
	"\"" { yylval.regexp = lex_str('"',  opts->bCaseInsensitive ||  opts->bCaseInverted); return TOKEN_REGEXP; }
	"["  { yylval.regexp = lex_cls(false); return TOKEN_REGEXP; }
	"[^" { yylval.regexp = lex_cls(true);  return TOKEN_REGEXP; }

	"<>" / (space* ("{" | "=>" | ":=")) {
					return TOKEN_NOCOND;
				}
	"<!"		{
					return TOKEN_SETUP;
				}
	[<>,()|=;/\\]	{
					return *tok;
				}

	"*"			{
					yylval.op = *tok;
					return TOKEN_STAR;
				}
	[+?]		{
					yylval.op = *tok;
					return TOKEN_CLOSE;
				}

	"{" [0-9]+ "}"	{
					if (!s_to_u32_unsafe (tok + 1, cur - 1, yylval.extop.min))
					{
						fatal ("repetition count overflow");
					}
					yylval.extop.max = yylval.extop.min;
					return TOKEN_CLOSESIZE;
				}

	"{" [0-9]+ "," [0-9]+ "}"	{
					const char * p = strchr (tok, ',');
					if (!s_to_u32_unsafe (tok + 1, p, yylval.extop.min))
					{
						fatal ("repetition lower bound overflow");
					}
					if (!s_to_u32_unsafe (p + 1, cur - 1, yylval.extop.max))
					{
						fatal ("repetition upper bound overflow");
					}
					return TOKEN_CLOSESIZE;
				}

	"{" [0-9]+ ",}"		{
					if (!s_to_u32_unsafe (tok + 1, cur - 2, yylval.extop.min))
					{
						fatal ("repetition lower bound overflow");
					}
					yylval.extop.max = std::numeric_limits<uint32_t>::max();
					return TOKEN_CLOSESIZE;
				}

	"{" [0-9]* ","		{
					fatal("illegal closure form, use '{n}', '{n,}', '{n,m}' where n and m are numbers");
				}

	"{" name "}"	{
					if (!opts->FFlag) {
						fatal("curly braces for names only allowed with -F switch");
					}
					yylval.str = new std::string (tok + 1, tok_len () - 2); // -2 to omit braces
					return TOKEN_ID;
				}

	"re2c:" { lex_conf (); return TOKEN_CONF; }

	name / (space+ [^=>,])	{
					yylval.str = new std::string (tok, tok_len ());
					if (opts->FFlag)
					{
						lexer_state = LEX_FLEX_NAME;
						return TOKEN_FID;
					}
					else
					{
						return TOKEN_ID;
					}
				}

	name / (space* [=>,])	{
					yylval.str = new std::string (tok, tok_len ());
					return TOKEN_ID;
				}

	name / [^]	{
					if (!opts->FFlag) {
						yylval.str = new std::string (tok, tok_len());
						return TOKEN_ID;
					} else {
						RegExp *r = NULL;
						const bool casing = opts->bCaseInsensitive || opts->bCaseInverted;
						for (char *s = tok; s < cur; ++s) {
							const uint32_t c = static_cast<uint8_t>(*s);
							r = doCat(r, casing ? ichr(c) : schr(c));
						}
						yylval.regexp = r ? r : new NullOp;
						return TOKEN_REGEXP;
					}
				}

	"."			{
					yylval.regexp = mkDot();
					return TOKEN_REGEXP;
				}

	space+		{
					goto scan;
				}

	eol space* "#" space* "line" space+ / lineinf {
					set_sourceline ();
					goto scan;
				}

	eol			{
					if (cur == eof) return 0;
					pos = cur;
					cline++;
					goto scan;
				}

	*			{
					fatalf("unexpected character: '%c'", *tok);
					goto scan;
				}
*/

flex_name:
/*!re2c
	eol
	{
		YYCURSOR = tok;
		lexer_state = LEX_NORMAL;
		return TOKEN_FID_END;
	}
	*
	{
		YYCURSOR = tok;
		goto start;
	}
*/

code:
/*!re2c
	"}"			{
					if (depth == 0)
					{
						fatal("Curly braces are not allowed after ':='");
					}
					else if (--depth == 0)
					{
						yylval.code = new Code (tok, tok_len (), get_fname (), tline);
						return TOKEN_CODE;
					}
					goto code;
				}
	"{"			{
					if (depth == 0)
					{
						fatal("Curly braces are not allowed after ':='");
					}
					else
					{
						++depth;
					}
					goto code;
				}
	"\n" space* "#" space* "line" space+ / lineinf {
					set_sourceline ();
					goto code;
				}
	"\n" /  ws	{
					if (depth == 0)
					{
						goto code;
					}
					else if (cur == eof)
					{
						fatal("missing '}'");
					}
					pos = cur;
					cline++;
					goto code;
				}
	"\n"		{
					if (depth == 0)
					{
						tok += strspn(tok, " \t\r\n");
						while (cur > tok && strchr(" \t\r\n", cur[-1]))
						{
							--cur;
						}
						yylval.code = new Code (tok, tok_len (), get_fname (), tline);
						return TOKEN_CODE;
					}
					else if (cur == eof)
					{
						fatal("missing '}'");
					}
					pos = cur;
					cline++;
					goto code;
				}
	zero		{
					if (cur == eof)
					{
						if (depth)
						{
							fatal("missing '}'");
						}
						return 0;
					}
					goto code;
				}
	dstring | sstring	{
					goto code;
				}
	*			{
					goto code;
				}
*/

comment:
/*!re2c
	"*/"		{
					if (--depth == 0)
					{
						goto scan;
					}
					else
					{
						goto comment;
					}
				}
	"/*"		{
					++depth;
					fatal("ambiguous /* found");
					goto comment;
				}
	"\n" space* "#" space* "line" space+ / lineinf {
					set_sourceline ();
					goto comment;
				}
	"\n"		{
					if (cur == eof)
					{
						return 0;
					}
					tok = pos = cur;
					cline++;
					goto comment;
				}
	*			{
					if (cur == eof)
					{
						return 0;
					}
					goto comment;
				}
*/

nextLine:
/*!re2c                                  /* resync emacs */
   "\n"     { if(cur == eof) {
                  return 0;
               }
               tok = pos = cur;
               cline++;
               goto scan;
            }
   *        {  if(cur == eof) {
                  return 0;
               }
               goto nextLine;
            }
*/
}

static void escape (std::string & dest, const std::string & src)
{
	dest = src;
	size_t l = dest.length();
	for (size_t p = 0; p < l; ++p)
	{
		if (dest[p] == '\\')
		{
			dest.insert(++p, "\\");
			++l;
		}
	}
}

RegExp *Scanner::lex_cls(bool neg)
{
	Range *r = NULL, *s;
	uint32_t u, l;
fst:
	/*!re2c
		"]" { goto end; }
		""  { l = lex_cls_chr(); goto snd; }
	*/
snd:
	/*!re2c
		""          { u = l; goto add; }
		"-" / [^\]] {
			u = lex_cls_chr();
			if (l > u) {
				warn.swapped_range(get_line(), l, u);
				std::swap(l, u);
			}
			goto add;
		}
	*/
add:
	if (!(s = opts->encoding.encodeRange(l, u))) {
		fatalf ("Bad code point range: '0x%X - 0x%X'", l, u);
	}
	r = Range::add(r, s);
	goto fst;
end:
	if (neg) {
		r = Range::sub(opts->encoding.fullRange(), r);
	}
	return cls(r);
}

uint32_t Scanner::lex_cls_chr()
{
	tok = cur;
	/*!re2c
		*          { fatal ((tok - pos) - tchar, "syntax error"); }
		esc [xXuU] { fatal ((tok - pos) - tchar, "syntax error in hexadecimal escape sequence"); }
		esc [0-7]  { fatal ((tok - pos) - tchar, "syntax error in octal escape sequence"); }
		esc        { fatal ((tok - pos) - tchar, "syntax error in escape sequence"); }

		. \ esc    { return static_cast<uint8_t>(tok[0]); }
		esc_hex    { return unesc_hex(tok, cur); }
		esc_oct    { return unesc_oct(tok, cur); }
		esc "a"    { return static_cast<uint8_t>('\a'); }
		esc "b"    { return static_cast<uint8_t>('\b'); }
		esc "f"    { return static_cast<uint8_t>('\f'); }
		esc "n"    { return static_cast<uint8_t>('\n'); }
		esc "r"    { return static_cast<uint8_t>('\r'); }
		esc "t"    { return static_cast<uint8_t>('\t'); }
		esc "v"    { return static_cast<uint8_t>('\v'); }
		esc "\\"   { return static_cast<uint8_t>('\\'); }
		esc "-"    { return static_cast<uint8_t>('-'); }
		esc "]"    { return static_cast<uint8_t>(']'); }
		esc .      {
			warn.useless_escape(tline, tok - pos, tok[1]);
			return static_cast<uint8_t>(tok[1]);
		}
	*/
}

uint32_t Scanner::lex_str_chr(char quote, bool &end)
{
	end = false;
	tok = cur;
	/*!re2c
		*          { fatal ((tok - pos) - tchar, "syntax error"); }
		esc [xXuU] { fatal ((tok - pos) - tchar, "syntax error in hexadecimal escape sequence"); }
		esc [0-7]  { fatal ((tok - pos) - tchar, "syntax error in octal escape sequence"); }
		esc        { fatal ((tok - pos) - tchar, "syntax error in escape sequence"); }

		. \ esc    {
			end = tok[0] == quote;
			return static_cast<uint8_t>(tok[0]);
		}
		esc_hex    { return unesc_hex(tok, cur); }
		esc_oct    { return unesc_oct(tok, cur); }
		esc "a"    { return static_cast<uint8_t>('\a'); }
		esc "b"    { return static_cast<uint8_t>('\b'); }
		esc "f"    { return static_cast<uint8_t>('\f'); }
		esc "n"    { return static_cast<uint8_t>('\n'); }
		esc "r"    { return static_cast<uint8_t>('\r'); }
		esc "t"    { return static_cast<uint8_t>('\t'); }
		esc "v"    { return static_cast<uint8_t>('\v'); }
		esc "\\"   { return static_cast<uint8_t>('\\'); }
		esc .      {
			if (tok[1] != quote) {
				warn.useless_escape(tline, tok - pos, tok[1]);
			}
			return static_cast<uint8_t>(tok[1]);
		}
	*/
}

RegExp *Scanner::lex_str(char quote, bool casing)
{
	RegExp *r = NULL;
	for (bool end;;) {
		const uint32_t c = lex_str_chr(quote, end);
		if (end) {
			return r ? r : new NullOp;
		}
		r = doCat(r, casing ? ichr(c) : schr(c));
	}
}

void Scanner::set_sourceline ()
{
sourceline:
	tok = cur;
/*!re2c	
	lineno		{
					if (!s_to_u32_unsafe (tok, cur, cline))
					{
						fatal ("line number overflow");
					}
					goto sourceline; 
				}
	dstring		{
					escape (in.file_name, std::string (tok + 1, tok_len () - 2)); // -2 to omit quotes
			  		goto sourceline; 
				}
	"\n"			{
  					if (cur == eof)
  					{
						--cur; 
					}
			  		else
			  		{
			  			pos = cur; 
			  		}
			  		tok = cur;
			  		return; 
				}
	*			{
  					goto sourceline;
  				}
*/
}

} // end namespace re2c

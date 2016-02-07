#include "src/util/c99_stdint.h"
#include <string>

#include "src/codegen/output.h"
#include "src/conf/opt.h"
#include "src/globals.h"
#include "src/ir/regexp/encoding/enc.h"
#include "src/parse/scanner.h"
#include "src/util/s_to_n32_unsafe.h"

namespace re2c
{

// global re2c config (affects the whole file)
/*!re2c
	re2c:define:YYCTYPE     = "unsigned char";
	re2c:define:YYCURSOR    = cur;
	re2c:define:YYLIMIT     = lim;
	re2c:define:YYMARKER    = ptr;
	re2c:define:YYCTXMARKER = ctx;
	re2c:define:YYFILL      = fill;

	// source code is in ASCII: pointers have type 'char *'
	// but re2c makes an implicit assumption that YYCTYPE is unsigned
	// when it generates comparisons
	re2c:yych:conversion = 1;

	space = [ \t];

	conf_assign = space* "=" space*;

	naked_char = . \ (space | [;]);
	naked      = "" | (naked_char \ ['"]) naked_char*;

	number = "0" | ("-"? [1-9] [0-9]*);
*/

void Scanner::lex_conf ()
{
	tok = cur;
/*!re2c
	* { fatal ((tok - pos) - tchar, "unrecognized configuration"); }

	"flags:" [ewxu8]
	{
		Enc::type_t enc = Enc::ASCII;
		switch (cur[-1])
		{
			case 'e': enc = Enc::EBCDIC; break;
			case 'w': enc = Enc::UCS2;   break;
			case 'x': enc = Enc::UTF16;  break;
			case 'u': enc = Enc::UTF32;  break;
			case '8': enc = Enc::UTF8;   break;
		}
		const int32_t n = lex_conf_number ();
		if (n == 0)
		{
			opts.unset_encoding (enc);
		}
		else if (!opts.set_encoding (enc))
		{
			fatalf ("Cannot set %s encoding: please reset %s encoding first"
				, Enc::name (enc)
				, Enc::name (opts->encoding.type ()));
		}
		return;
	}

	"define:YYCONDTYPE"           { opts.set_yycondtype       (lex_conf_string ()); return; }
	"define:YYGETCONDITION"       { opts.set_cond_get         (lex_conf_string ()); return; }
	"define:YYGETCONDITION:naked" { opts.set_cond_get_naked   (lex_conf_number () != 0); return; }
	"define:YYSETCONDITION"       { opts.set_cond_set         (lex_conf_string ()); return; }
	"define:YYSETCONDITION@cond"  { opts.set_cond_set_arg     (lex_conf_string ()); return; }
	"define:YYSETCONDITION:naked" { opts.set_cond_set_naked   (lex_conf_number () != 0); return; }
	"condprefix"                  { opts.set_condPrefix       (lex_conf_string ()); return; }
	"condenumprefix"              { opts.set_condEnumPrefix   (lex_conf_string ()); return; }
	"cond:divider"                { opts.set_condDivider      (lex_conf_string ()); return; }
	"cond:divider@cond"           { opts.set_condDividerParam (lex_conf_string ()); return; }
	"cond:goto"                   { opts.set_condGoto         (lex_conf_string ()); return; }
	"cond:goto@cond"              { opts.set_condGotoParam    (lex_conf_string ()); return; }
	"variable:yyctable"           { opts.set_yyctable         (lex_conf_string ()); return; }

	"define:YYGETSTATE"       { opts.set_state_get       (lex_conf_string ()); return; }
	"define:YYGETSTATE:naked" { opts.set_state_get_naked (lex_conf_number () != 0); return; }
	"define:YYSETSTATE"       { opts.set_state_set       (lex_conf_string ()); return; }
	"define:YYSETSTATE:naked" { opts.set_state_set_naked (lex_conf_number () != 0); return; }
	"define:YYSETSTATE@state" { opts.set_state_set_arg   (lex_conf_string ()); return; }
	"label:yyFillLabel"       { opts.set_yyfilllabel     (lex_conf_string ()); return; }
	"label:yyNext"            { opts.set_yynext          (lex_conf_string ()); return; }
	"state:abort"             { opts.set_bUseStateAbort  (lex_conf_number () != 0); return; }
	"state:nextlabel"         { opts.set_bUseStateNext   (lex_conf_number () != 0); return; }
	"variable:yyaccept"       { opts.set_yyaccept        (lex_conf_string ()); return; }

	"variable:yybm"     { opts.set_yybm         (lex_conf_string ()); return; }
	"yybm:hex"          { opts.set_yybmHexTable (lex_conf_number () != 0); return; }
	"cgoto:threshold"
	{
		const int32_t n = lex_conf_number ();
		if (n < 0)
		{
			fatal ("configuration 'cgoto:threshold' must be nonnegative");
		}
		opts.set_cGotoThreshold (static_cast<uint32_t> (n));
		return;
	}
	"variable:yytarget" { opts.set_yytarget (lex_conf_string ()); return; }

	"define:YYCURSOR"    { opts.set_yycursor    (lex_conf_string ()); return; }
	"define:YYMARKER"    { opts.set_yymarker    (lex_conf_string ()); return; }
	"define:YYCTXMARKER" { opts.set_yyctxmarker (lex_conf_string ()); return; }
	"define:YYLIMIT"     { opts.set_yylimit     (lex_conf_string ()); return; }

	"define:YYPEEK"       { opts.set_yypeek       (lex_conf_string ()); return; }
	"define:YYSKIP"       { opts.set_yyskip       (lex_conf_string ()); return; }
	"define:YYBACKUP"     { opts.set_yybackup     (lex_conf_string ()); return; }
	"define:YYBACKUPCTX"  { opts.set_yybackupctx  (lex_conf_string ()); return; }
	"define:YYRESTORE"    { opts.set_yyrestore    (lex_conf_string ()); return; }
	"define:YYRESTORECTX" { opts.set_yyrestorectx (lex_conf_string ()); return; }
	"define:YYLESSTHAN"   { opts.set_yylessthan   (lex_conf_string ()); return; }

	"indent:string" { opts.set_indString (lex_conf_string ()); return; }
	"indent:top"
	{
		const int32_t n = lex_conf_number ();
		if (n < 0)
		{
			fatal ("configuration 'indent:top' must be nonnegative");
		}
		opts.set_topIndent (static_cast<uint32_t> (n));
		return;
	}

	"define:YYDEBUG" { opts.set_yydebug (lex_conf_string ()); return; }

	"define:YYCTYPE"  { opts.set_yyctype        (lex_conf_string ()); return; }
	"variable:yych"   { opts.set_yych           (lex_conf_string ()); return; }
	"yych:conversion" { opts.set_yychConversion (lex_conf_number () != 0); return; }
	"yych:emit"       { opts.set_bEmitYYCh      (lex_conf_number () != 0); return; }

	"define:YYFILL"       { opts.set_fill         (lex_conf_string ()); return; }
	"yyfill:enable"       { opts.set_fill_use     (lex_conf_number () != 0); return; }
	"define:YYFILL@len"   { opts.set_fill_arg     (lex_conf_string ()); return; }
	"yyfill:parameter"    { opts.set_fill_arg_use (lex_conf_number () != 0); return; }
	"define:YYFILL:naked" { opts.set_fill_naked   (lex_conf_number () != 0); return; }
	"yyfill:check"        { opts.set_fill_check   (lex_conf_number () != 0); return; }

	"labelprefix" { opts.set_labelPrefix (lex_conf_string ()); return; }

	// try to lex number first, otherwize it would be lexed as a naked string
	"startlabel" / conf_assign number { out.set_force_start_label (lex_conf_number () != 0); return; }
	"startlabel"                      { out.set_user_start_label (lex_conf_string ()); return; }

	// deprecated
	"variable:yystable" { lex_conf_string (); return; }
*/
}

void Scanner::lex_conf_assign ()
{
/*!re2c
	*           { fatal ("missing '=' in configuration"); }
	conf_assign { return; }
*/
}

void Scanner::lex_conf_semicolon ()
{
/*!re2c
	*          { fatal ("missing ending ';' in configuration"); }
	space* ";" { return; }
*/
}

int32_t Scanner::lex_conf_number ()
{
	lex_conf_assign ();
	tok = cur;
/*!re2c
	number
	{
		int32_t n = 0;
		if (!s_to_i32_unsafe (tok, cur, n))
		{
			fatal ("configuration value overflow");
		}
		lex_conf_semicolon ();
		return n;
	}
*/
}

std::string Scanner::lex_conf_string ()
{
	lex_conf_assign ();
	std::string s;
	tok = cur;
/*!re2c
	['"] {
		const char quote = tok[0];
		for (bool end;;) {
			const uint32_t c = lex_str_chr(quote, end);
			if (end) {
				goto end;
			}
			if (c > 0xFF) {
				fatalf ("multibyte character in configuration string: 0x%X", c);
			} else {
				s += static_cast<char>(c);
			}
		}
	}
	naked {
		s = std::string(tok, tok_len());
		goto end;
	}
*/
end:
	lex_conf_semicolon ();
	return s;
}

} // end namespace re2c

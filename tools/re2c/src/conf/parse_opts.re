#include "src/codegen/input_api.h"
#include "src/conf/msg.h"
#include "src/conf/opt.h"
#include "src/conf/warn.h"
#include "src/globals.h"
#include "src/ir/regexp/empty_class_policy.h"
#include "src/ir/regexp/encoding/enc.h"

namespace re2c
{

static inline bool next (char * & arg, char ** & argv)
{
	arg = *++argv;
	return arg != NULL;
}

parse_opts_t parse_opts (char ** argv, Opt & opts)
{
#define YYCTYPE unsigned char
	char * YYCURSOR;
	char * YYMARKER;
	Warn::option_t option;

/*!re2c
	re2c:yyfill:enable = 0;
	re2c:yych:conversion = 1;

	end = "\x00";
	filename = [^\x00-] [^\x00]*;
*/

opt:
	if (!next (YYCURSOR, argv))
	{
		goto end;
	}
/*!re2c
	*
	{
		error ("bad option: %s", *argv);
		return EXIT_FAIL;
	}

	"--" end
	{
		// all remaining arguments are non-options
		// so they must be input files
		// re2c expects exactly one input file
		for (char * f; next (f, argv);)
		{
			if (!opts.source (f))
			{
				return EXIT_FAIL;
			}
		}
		goto end;
	}

	"-"      end { if (!opts.source ("<stdin>")) return EXIT_FAIL; goto opt; }
	filename end { if (!opts.source (*argv))     return EXIT_FAIL; goto opt; }

	"-"  { goto opt_short; }
	"--" { goto opt_long; }

	"-W"      end { warn.set_all ();       goto opt; }
	"-Werror" end { warn.set_all_error (); goto opt; }
	"-W"          { option = Warn::W;        goto opt_warn; }
	"-Wno-"       { option = Warn::WNO;      goto opt_warn; }
	"-Werror-"    { option = Warn::WERROR;   goto opt_warn; }
	"-Wno-error-" { option = Warn::WNOERROR; goto opt_warn; }
*/

opt_warn:
/*!re2c
	*
	{
		error ("bad warning: %s", *argv);
		return EXIT_FAIL;
	}
	"condition-order"        end { warn.set (Warn::CONDITION_ORDER,        option); goto opt; }
	"empty-character-class"  end { warn.set (Warn::EMPTY_CHARACTER_CLASS,  option); goto opt; }
	"match-empty-string"     end { warn.set (Warn::MATCH_EMPTY_STRING,     option); goto opt; }
	"swapped-range"          end { warn.set (Warn::SWAPPED_RANGE,          option); goto opt; }
	"undefined-control-flow" end { warn.set (Warn::UNDEFINED_CONTROL_FLOW, option); goto opt; }
	"unreachable-rules"      end { warn.set (Warn::UNREACHABLE_RULES,      option); goto opt; }
	"useless-escape"         end { warn.set (Warn::USELESS_ESCAPE,         option); goto opt; }
*/

opt_short:
/*!re2c
	*
	{
		error ("bad short option: %s", *argv);
		return EXIT_FAIL;
	}
	end { goto opt; }
	[?h] { usage ();   return EXIT_OK; }
	"v"  { version (); return EXIT_OK; }
	"V"  { vernum ();  return EXIT_OK; }
	"b" { opts.set_bFlag (true);             goto opt_short; }
	"c" { opts.set_cFlag (true);             goto opt_short; }
	"d" { opts.set_dFlag (true);             goto opt_short; }
	"D" { opts.set_target (opt_t::DOT);      goto opt_short; }
	"f" { opts.set_fFlag (true);             goto opt_short; }
	"F" { opts.set_FFlag (true);             goto opt_short; }
	"g" { opts.set_gFlag (true);             goto opt_short; }
	"i" { opts.set_iFlag (true);             goto opt_short; }
	"r" { opts.set_rFlag (true);             goto opt_short; }
	"s" { opts.set_sFlag (true);             goto opt_short; }
	"S" { opts.set_target (opt_t::SKELETON); goto opt_short; }
	"e" { if (!opts.set_encoding (Enc::EBCDIC)) { error_encoding (); return EXIT_FAIL; } goto opt_short; }
	"u" { if (!opts.set_encoding (Enc::UTF32))  { error_encoding (); return EXIT_FAIL; } goto opt_short; }
	"w" { if (!opts.set_encoding (Enc::UCS2))   { error_encoding (); return EXIT_FAIL; } goto opt_short; }
	"x" { if (!opts.set_encoding (Enc::UTF16))  { error_encoding (); return EXIT_FAIL; } goto opt_short; }
	"8" { if (!opts.set_encoding (Enc::UTF8))   { error_encoding (); return EXIT_FAIL; } goto opt_short; }
	"o" end { if (!next (YYCURSOR, argv)) { error_arg ("-o, --output"); return EXIT_FAIL; } goto opt_output; }
	"o"     { *argv = YYCURSOR;                                                             goto opt_output; }
	"t" end { if (!next (YYCURSOR, argv)) { error_arg ("-t, --type-header"); return EXIT_FAIL; } goto opt_header; }
	"t"     { *argv = YYCURSOR;                                                                  goto opt_header; }
	"1" { goto opt_short; } // deprecated
*/

opt_long:
/*!re2c
	*
	{
		error ("bad long option: %s", *argv);
		return EXIT_FAIL;
	}
	"help"               end { usage ();   return EXIT_OK; }
	"version"            end { version (); return EXIT_OK; }
	"vernum"             end { vernum ();  return EXIT_OK; }
	"bit-vectors"        end { opts.set_bFlag (true);             goto opt; }
	"start-conditions"   end { opts.set_cFlag (true);             goto opt; }
	"debug-output"       end { opts.set_dFlag (true);             goto opt; }
	"emit-dot"           end { opts.set_target (opt_t::DOT);      goto opt; }
	"storable-state"     end { opts.set_fFlag (true);             goto opt; }
	"flex-syntax"        end { opts.set_FFlag (true);             goto opt; }
	"computed-gotos"     end { opts.set_gFlag (true);             goto opt; }
	"no-debug-info"      end { opts.set_iFlag (true);             goto opt; }
	"reusable"           end { opts.set_rFlag (true);             goto opt; }
	"nested-ifs"         end { opts.set_sFlag (true);             goto opt; }
	"no-generation-date" end { opts.set_bNoGenerationDate (true); goto opt; }
	"no-version"         end { opts.set_version (false);          goto opt; }
	"case-insensitive"   end { opts.set_bCaseInsensitive (true);  goto opt; }
	"case-inverted"      end { opts.set_bCaseInverted (true);     goto opt; }
	"skeleton"           end { opts.set_target (opt_t::SKELETON); goto opt; }
	"ecb"                end { if (!opts.set_encoding (Enc::EBCDIC)) { error_encoding (); return EXIT_FAIL; } goto opt; }
	"unicode"            end { if (!opts.set_encoding (Enc::UTF32))  { error_encoding (); return EXIT_FAIL; } goto opt; }
	"wide-chars"         end { if (!opts.set_encoding (Enc::UCS2))   { error_encoding (); return EXIT_FAIL; } goto opt; }
	"utf-16"             end { if (!opts.set_encoding (Enc::UTF16))  { error_encoding (); return EXIT_FAIL; } goto opt; }
	"utf-8"              end { if (!opts.set_encoding (Enc::UTF8))   { error_encoding (); return EXIT_FAIL; } goto opt; }
	"output"             end { if (!next (YYCURSOR, argv)) { error_arg ("-o, --output"); return EXIT_FAIL; } goto opt_output; }
	"type-header"        end { if (!next (YYCURSOR, argv)) { error_arg ("-t, --type-header"); return EXIT_FAIL; } goto opt_header; }
	"encoding-policy"    end { goto opt_encoding_policy; }
	"input"              end { goto opt_input; }
	"empty-class"        end { goto opt_empty_class; }
	"dfa-minimization"   end { goto opt_dfa_minimization; }
	"single-pass"        end { goto opt; } // deprecated
*/

opt_output:
/*!re2c
	*
	{
		error ("bad argument to option -o, --output: %s", *argv);
		return EXIT_FAIL;
	}
	filename end { if (!opts.output (*argv)) return EXIT_FAIL; goto opt; }
*/

opt_header:
/*!re2c
	*
	{
		error ("bad argument to option -t, --type-header: %s", *argv);
		return EXIT_FAIL;
	}
	filename end { opts.set_header_file (*argv); goto opt; }
*/

opt_encoding_policy:
	if (!next (YYCURSOR, argv))
	{
		error_arg ("--encoding-policy");
		return EXIT_FAIL;
	}
/*!re2c
	*
	{
		error ("bad argument to option --encoding-policy (expected: ignore | substitute | fail): %s", *argv);
		return EXIT_FAIL;
	}
	"ignore"     end { opts.set_encoding_policy (Enc::POLICY_IGNORE);     goto opt; }
	"substitute" end { opts.set_encoding_policy (Enc::POLICY_SUBSTITUTE); goto opt; }
	"fail"       end { opts.set_encoding_policy (Enc::POLICY_FAIL);       goto opt; }
*/

opt_input:
	if (!next (YYCURSOR, argv))
	{
		error_arg ("--input");
		return EXIT_FAIL;
	}
/*!re2c
	*
	{
		error ("bad argument to option --input (expected: default | custom): %s", *argv);
		return EXIT_FAIL;
	}
	"default" end { opts.set_input_api (InputAPI::DEFAULT); goto opt; }
	"custom"  end { opts.set_input_api (InputAPI::CUSTOM);  goto opt; }
*/

opt_empty_class:
	if (!next (YYCURSOR, argv))
	{
		error_arg ("--empty-class");
		return EXIT_FAIL;
	}
/*!re2c
	*
	{
		error ("bad argument to option --empty-class (expected: match-empty | match-none | error): %s", *argv);
		return EXIT_FAIL;
	}
	"match-empty" end { opts.set_empty_class_policy (EMPTY_CLASS_MATCH_EMPTY); goto opt; }
	"match-none"  end { opts.set_empty_class_policy (EMPTY_CLASS_MATCH_NONE);  goto opt; }
	"error"       end { opts.set_empty_class_policy (EMPTY_CLASS_ERROR);       goto opt; }
*/

opt_dfa_minimization:
	if (!next (YYCURSOR, argv))
	{
		error_arg ("--minimization");
		return EXIT_FAIL;
	}
/*!re2c
	*
	{
		error ("bad argument to option --dfa-minimization (expected: table | moore): %s", *argv);
		return EXIT_FAIL;
	}
	"table" end { opts.set_dfa_minimization (DFA_MINIMIZATION_TABLE); goto opt; }
	"moore" end { opts.set_dfa_minimization (DFA_MINIMIZATION_MOORE); goto opt; }
*/

end:
	if (!opts.source_file)
	{
		error ("no source file");
		return EXIT_FAIL;
	}

	return OK;

#undef YYCTYPE
}

} // namespace re2c

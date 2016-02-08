#ifndef _RE2C_CONF_OPT_
#define _RE2C_CONF_OPT_

#include "src/util/c99_stdint.h"
#include <stddef.h>
#include <string>

#include "src/codegen/input_api.h"
#include "src/ir/dfa/dfa.h"
#include "src/ir/regexp/empty_class_policy.h"
#include "src/ir/regexp/encoding/enc.h"
#include "src/util/forbid_copy.h"

namespace re2c
{

#define RE2C_OPTS \
	/* target */ \
	OPT1 (opt_t::target_t, target, CODE) \
	/* fingerprint */ \
	OPT (bool, bNoGenerationDate, false) \
	OPT (bool, version, true) \
	/* regular expressions */ \
	OPT (Enc, encoding, Enc ()) \
	OPT (bool, bCaseInsensitive, false) \
	OPT (bool, bCaseInverted, false) \
	OPT (empty_class_policy_t, empty_class_policy, EMPTY_CLASS_MATCH_EMPTY) \
	/* conditions */ \
	OPT (bool, cFlag, false) \
	OPT (bool, tFlag, false) \
	OPT (const char *,  header_file, NULL) \
	OPT (std::string, yycondtype, "YYCONDTYPE") \
	OPT (std::string, cond_get, "YYGETCONDITION") \
	OPT (bool, cond_get_naked, false) \
	OPT (std::string, cond_set, "YYSETCONDITION" ) \
	OPT (std::string, cond_set_arg, "@@" ) \
	OPT (bool, cond_set_naked, false ) \
	OPT (std::string, yyctable, "yyctable") \
	OPT (std::string, condPrefix, "yyc_") \
	OPT (std::string, condEnumPrefix, "yyc") \
	OPT (std::string, condDivider, "/* *********************************** */") \
	OPT (std::string, condDividerParam, "@@") \
	OPT (std::string, condGoto, "goto @@;") \
	OPT (std::string, condGotoParam, "@@") \
	/* states */ \
	OPT (bool, fFlag, false) \
	OPT (std::string, state_get, "YYGETSTATE") \
	OPT (bool, state_get_naked, false) \
	OPT (std::string, state_set, "YYSETSTATE") \
	OPT (std::string, state_set_arg, "@@") \
	OPT (bool, state_set_naked, false) \
	OPT (std::string, yyfilllabel, "yyFillLabel") \
	OPT (std::string, yynext, "yyNext") \
	OPT (std::string, yyaccept, "yyaccept") \
	OPT (bool, bUseStateAbort, false) \
	OPT (bool, bUseStateNext, false) \
	/* reuse */ \
	OPT (bool, rFlag, false) \
	/* partial flex syntax support */ \
	OPT (bool, FFlag, false) \
	/* code generation */ \
	OPT (bool, sFlag, false) \
	OPT (bool, bFlag, false) \
	OPT (std::string, yybm, "yybm") \
	OPT (bool, yybmHexTable, false) \
	OPT (bool, gFlag, false) \
	OPT (std::string, yytarget, "yytarget") \
	OPT (uint32_t, cGotoThreshold, 9) \
	/* formatting */ \
	OPT (uint32_t, topIndent, 0) \
	OPT (std::string, indString, "\t") \
	/* input API */ \
	OPT (InputAPI, input_api, InputAPI ()) \
	OPT (std::string, yycursor, "YYCURSOR") \
	OPT (std::string, yymarker, "YYMARKER") \
	OPT (std::string, yyctxmarker, "YYCTXMARKER") \
	OPT (std::string, yylimit, "YYLIMIT") \
	OPT (std::string, yypeek, "YYPEEK") \
	OPT (std::string, yyskip, "YYSKIP") \
	OPT (std::string, yybackup, "YYBACKUP") \
	OPT (std::string, yybackupctx, "YYBACKUPCTX") \
	OPT (std::string, yyrestore, "YYRESTORE") \
	OPT (std::string, yyrestorectx, "YYRESTORECTX") \
	OPT (std::string, yylessthan, "YYLESSTHAN") \
	/* #line directives */ \
	OPT (bool, iFlag, false) \
	/* debug */ \
	OPT (bool, dFlag, false) \
	OPT (std::string, yydebug, "YYDEBUG") \
	/* yych */ \
	OPT (std::string, yyctype, "YYCTYPE") \
	OPT (std::string, yych, "yych") \
	OPT (bool, bEmitYYCh, true) \
	OPT (bool, yychConversion, false) \
	/* YYFILL */ \
	OPT (std::string, fill, "YYFILL") \
	OPT (bool, fill_use, true) \
	OPT (bool, fill_check, true) \
	OPT (std::string, fill_arg, "@@") \
	OPT (bool, fill_arg_use, true) \
	OPT (bool, fill_naked, false) \
	/* labels */ \
	OPT (std::string, labelPrefix, "yy") \
	/* internals */ \
	OPT (dfa_minimization_t, dfa_minimization, DFA_MINIMIZATION_MOORE)

struct opt_t
{
	enum target_t
	{
		CODE,
		DOT,
		SKELETON
	};

#define OPT1 OPT
#define OPT(type, name, value) type name;
	RE2C_OPTS
#undef OPT1
#undef OPT

	opt_t ();
	opt_t (const opt_t & opt);
	opt_t & operator = (const opt_t & opt);
	void fix ();
};

class useropt_t;
class realopt_t
{
	opt_t real;
	useropt_t & user;
public:
	realopt_t (useropt_t & opt);
	const opt_t * operator -> ();
	void sync ();
};

class useropt_t
{
	opt_t opt;
	bool diverge;
public:
	useropt_t ();
	opt_t * operator -> ();
	friend void realopt_t::sync ();
};

struct Opt
{
	static const opt_t baseopt;

	const char * source_file;
	const char * output_file;

private:
	useropt_t useropt;
	realopt_t realopt;

public:
	Opt ()
		: source_file (NULL)
		, output_file (NULL)
		, useropt ()
		, realopt (useropt)
	{}

	// read-only access, forces options syncronization
	const opt_t * operator -> ()
	{
		return realopt.operator -> ();
	}

	bool source (const char * s);
	bool output (const char * s);

	// Inplace configurations are applied immediately when parsed.
	// This is very bad: first, re2c behaviour is changed in the middle
	// of the block; second, config is resynced too often (every
	// attempt to read config that has been updated results in
	// automatic resync). It is much better to set all options at once.
	bool set_encoding (Enc::type_t t)                    { return useropt->encoding.set (t); }
	void unset_encoding (Enc::type_t t)                  { useropt->encoding.unset (t); }
	void set_encoding_policy (Enc::policy_t p)           { useropt->encoding.setPolicy (p); }
	void set_input_api (InputAPI::type_t t)              { useropt->input_api.set (t); }
#define OPT1 OPT
#define OPT(type, name, value) void set_##name (type arg) { useropt->name = arg; }
	RE2C_OPTS
#undef OPT1
#undef OPT

	// helpers
	std::string yychConversion ()
	{
		return realopt->yychConversion
			? "(" + realopt->yyctype + ")"
			: "";
	}

	// bad temporary hacks, should be fixed by proper scoping of config (parts).
	void reset_encoding (const Enc & enc);
	void reset_mapCodeName ();

	FORBID_COPY (Opt);
};

enum parse_opts_t
{
	OK,
	EXIT_OK,
	EXIT_FAIL
};

parse_opts_t parse_opts (char ** argv, Opt & opts);

} // namespace re2c

#endif // _RE2C_CONF_OPT_

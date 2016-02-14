#ifndef _RE2C_PARSE_SPEC_
#define _RE2C_PARSE_SPEC_

#include "src/ir/regexp/regexp_rule.h"
#include "src/parse/rules.h"

namespace re2c
{

struct Spec
{
	RegExp * re;
	rules_t rules;

	Spec ()
		: re (NULL)
		, rules ()
	{}
	Spec (const Spec & spec)
		: re (spec.re)
		, rules (spec.rules)
	{}
	Spec & operator = (const Spec & spec)
	{
		re = spec.re;
		rules = spec.rules;
		return *this;
	}
	bool add_def (RuleOp * r)
	{
		if (rules.find (rule_rank_t::def ()) != rules.end ())
		{
			return false;
		}
		else
		{
			add (r);
			return true;
		}
	}
	void add (RuleOp * r)
	{
		rules[r->rank].line = r->loc.line;
		re = mkAlt (re, r);
	}
	void clear ()
	{
		re = NULL;
		rules.clear ();
	}
};

} // namespace re2c

#endif // _RE2C_PARSE_SPEC_

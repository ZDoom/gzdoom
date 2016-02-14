#ifndef _RE2C_PARSE_RULES_
#define _RE2C_PARSE_RULES_

#include <map>
#include <set>

#include "src/ir/rule_rank.h"

namespace re2c
{

struct rule_info_t
{
	uint32_t line;
	std::set<rule_rank_t> shadow;
	bool reachable;

	rule_info_t ()
		: line (0)
		, shadow ()
		, reachable (false)
	{}
};

typedef std::map<rule_rank_t, rule_info_t> rules_t;

} // namespace re2c

#endif // _RE2C_PARSE_RULES_

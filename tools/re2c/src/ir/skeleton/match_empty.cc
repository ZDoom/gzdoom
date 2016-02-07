#include <map>
#include <set>

#include "src/conf/warn.h"
#include "src/globals.h"
#include "src/ir/rule_rank.h"
#include "src/ir/skeleton/path.h"
#include "src/ir/skeleton/skeleton.h"
#include "src/parse/rules.h"

namespace re2c
{

void Skeleton::warn_match_empty ()
{
	Node & head = nodes[0];

	head.calc_reachable ();
	const std::set<rule_t> & reach = head.reachable;

	// warn about rules that match empty string
	if (!head.rule.rank.is_none ())
	{
		bool reachable = head.end ();
		for (std::set<rule_t>::const_iterator i = reach.begin ();
			!reachable && i != reach.end (); ++i)
		{
			reachable |= i->rank.is_none ();
		}
		if (reachable)
		{
			warn.match_empty_string (rules[head.rule.rank].line);
		}
	}

	// warn about rules that match empty string with nonempty trailing context
	if (head.ctx)
	{
		for (std::set<rule_t>::const_iterator i = reach.begin (); i != reach.end (); ++i)
		{
			if (i->restorectx)
			{
				warn.match_empty_string (rules[i->rank].line);
			}
		}
	}
}

} // namespace re2c

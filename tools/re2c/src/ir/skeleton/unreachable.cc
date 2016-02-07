#include "src/util/c99_stdint.h"
#include <map>
#include <set>
#include <utility>

#include "src/conf/warn.h"
#include "src/globals.h"
#include "src/ir/rule_rank.h"
#include "src/ir/skeleton/path.h"
#include "src/ir/skeleton/skeleton.h"
#include "src/parse/rules.h"

namespace re2c
{

void Node::calc_reachable ()
{
	if (!reachable.empty ())
	{
		return;
	}
	else if (end ())
	{
		reachable.insert (rule);
	}
	else if (loop < 2)
	{
		local_inc _ (loop);
		for (arcs_t::iterator i = arcs.begin (); i != arcs.end (); ++i)
		{
			i->first->calc_reachable ();
			reachable.insert (i->first->reachable.begin (), i->first->reachable.end ());
		}
	}
}

void Skeleton::warn_unreachable_rules ()
{
	nodes->calc_reachable ();
	for (uint32_t i = 0; i < nodes_count; ++i)
	{
		const rule_rank_t r1 = nodes[i].rule.rank;
		const std::set<rule_t> & rs = nodes[i].reachable;
		for (std::set<rule_t>::const_iterator j = rs.begin (); j != rs.end (); ++j)
		{
			const rule_rank_t r2 = j->rank;
			if (r1 == r2 || r2.is_none ())
			{
				rules[r1].reachable = true;
			}
			else
			{
				rules[r1].shadow.insert (r2);
			}
		}
	}

	// warn about unreachable rules:
	//   - rules that are shadowed by other rules, e.g. rule '[a]' is shadowed by '[a] [^]'
	//   - infinite rules that consume infinitely many characters and fail on YYFILL, e.g. '[^]*'
	//   - rules that contain never-matching link, e.g. '[]' with option '--empty-class match-none'
	// default rule '*' should not be reported
	for (rules_t::const_iterator i = rules.begin (); i != rules.end (); ++i)
	{
		const rule_rank_t r = i->first;
		if (!r.is_none () && !r.is_def () && !rules[r].reachable)
		{
			warn.unreachable_rule (cond, i->second, rules);
		}
	}
}

} // namespace re2c

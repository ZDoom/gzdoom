#include <map>
#include <utility>
#include <vector>

#include "src/conf/warn.h"
#include "src/globals.h"
#include "src/ir/rule_rank.h"
#include "src/ir/skeleton/path.h"
#include "src/ir/skeleton/skeleton.h"
#include "src/ir/skeleton/way.h"
#include "src/util/u32lim.h"

namespace re2c
{

// We don't need all patterns that cause undefined behaviour.
// We only need some examples, the shorter the better.
// See also note [counting skeleton edges].
void Node::naked_ways (way_t & prefix, std::vector<way_t> & ways, nakeds_t &size)
{
	if (!rule.rank.is_none ())
	{
		return;
	}
	else if (end ())
	{
		ways.push_back (prefix);
		size = size + nakeds_t::from64(prefix.size ());
	}
	else if (loop < 2)
	{
		local_inc _ (loop);
		for (arcsets_t::iterator i = arcsets.begin ();
			i != arcsets.end () && !size.overflow (); ++i)
		{
			prefix.push_back (&i->second);
			i->first->naked_ways (prefix, ways, size);
			prefix.pop_back ();
		}
	}
}

void Skeleton::warn_undefined_control_flow ()
{
	way_t prefix;
	std::vector<way_t> ways;
	Node::nakeds_t size = Node::nakeds_t::from32(0u);

	nodes->naked_ways (prefix, ways, size);

	if (!ways.empty ())
	{
		warn.undefined_control_flow (line, cond, ways, size.overflow ());
	}
	else if (size.overflow ())
	{
		warn.fail (Warn::UNDEFINED_CONTROL_FLOW, line, "DFA is too large to check undefined control flow");
	}
}

} // namespace re2c

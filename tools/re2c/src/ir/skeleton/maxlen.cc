#include "src/util/c99_stdint.h"
#include <algorithm>
#include <limits>
#include <map>
#include <utility>

#include "src/ir/skeleton/skeleton.h"

namespace re2c
{

// 0 < DIST_MAX < DIST_ERROR <= std::numeric_limits<uint32_t>::max()
const uint32_t Node::DIST_ERROR = std::numeric_limits<uint32_t>::max();
const uint32_t Node::DIST_MAX = DIST_ERROR - 1;

// different from YYMAXFILL calculation
// in the way it handles loops and empty regexp
void Node::calc_dist ()
{
	if (dist != DIST_ERROR)
	{
		return;
	}
	else if (end ())
	{
		dist = 0;
	}
	else if (loop < 2)
	{
		local_inc _ (loop);
		for (arcs_t::iterator i = arcs.begin (); i != arcs.end (); ++i)
		{
			i->first->calc_dist ();
			if (i->first->dist != DIST_ERROR)
			{
				if (dist == DIST_ERROR)
				{
					dist = i->first->dist;
				}
				else
				{
					dist = std::max (dist, i->first->dist);
				}
			}
		}
		dist = std::min (dist + 1, DIST_MAX);
	}
}

} // namespace re2c

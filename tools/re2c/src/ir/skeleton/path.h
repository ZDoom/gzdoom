#ifndef _RE2C_IR_SKELETON_PATH_
#define _RE2C_IR_SKELETON_PATH_

#include <vector>

#include "src/ir/rule_rank.h"
#include "src/util/c99_stdint.h"

namespace re2c
{

struct rule_t
{
	rule_rank_t rank;
	bool restorectx;

	rule_t (rule_rank_t r, bool c)
		: rank (r)
		, restorectx (c)
	{}

	// needed by STL containers
	// same as 'std::pair' comparator
	bool operator < (const rule_t & r) const
	{
		return rank < r.rank
			|| (!(r.rank < rank) && restorectx < r.restorectx);
	}
};

class path_t
{
public:
	typedef std::vector<uint32_t> arc_t;

private:
	std::vector<const arc_t *> arcs;

	rule_t rule;
	size_t rule_pos;

	bool ctx;
	size_t ctx_pos;

public:
	explicit path_t (rule_t r, bool c)
		: arcs ()
		, rule (r)
		, rule_pos (0)
		, ctx (c)
		, ctx_pos (0)
	{}
	size_t len () const
	{
		return arcs.size ();
	}
	size_t len_matching () const
	{
		return rule.restorectx
			? ctx_pos
			: rule_pos;
	}
	rule_rank_t match () const
	{
		return rule.rank;
	}
	const arc_t * operator [] (size_t i) const
	{
		return arcs[i];
	}
	void extend (rule_t r, bool c, const arc_t * a)
	{
		arcs.push_back (a);
		if (!r.rank.is_none ())
		{
			rule = r;
			rule_pos = arcs.size ();
		}
		if (c)
		{
			ctx = true;
			ctx_pos = arcs.size ();
		}
	}
	void append (const path_t * p)
	{
		if (!p->rule.rank.is_none ())
		{
			rule = p->rule;
			rule_pos = arcs.size () + p->rule_pos;
		}
		if (p->ctx)
		{
			ctx = true;
			ctx_pos = arcs.size () + p->ctx_pos;
		}
		arcs.insert (arcs.end (), p->arcs.begin (), p->arcs.end ());
	}
};

} // namespace re2c

#endif // _RE2C_IR_SKELETON_PATH_

#include <stdlib.h>
#include <algorithm>
#include <utility>

#include "src/codegen/go.h"
#include "src/conf/msg.h"
#include "src/ir/dfa/dfa.h"
#include "src/ir/regexp/regexp.h"
#include "src/ir/regexp/regexp_rule.h"
#include "src/ir/skeleton/skeleton.h"

namespace re2c
{

Node::Node ()
	: arcs ()
	, arcsets ()
	, loop (0)
	, rule (rule_rank_t::none (), false)
	, ctx (false)
	, dist (DIST_ERROR)
	, reachable ()
	, suffix (NULL)
{}

void Node::init(bool c, RuleOp *r, const std::vector<std::pair<Node*, uint32_t> > &a)
{
	if (r)
	{
		rule.rank = r->rank;
		rule.restorectx = r->ctx->fixedLength () != 0;
	}

	ctx = c;

	uint32_t lb = 0;
	std::vector<std::pair<Node*, uint32_t> >::const_iterator
		i = a.begin(),
		e = a.end();
	for (; i != e; ++i)
	{
		Node *n = i->first;
		const uint32_t ub = i->second - 1;

		// pick at most 0x100 unique edges from this range
		// (for 1-byte code units this covers the whole range: [0 - 0xFF])
		//   - range bounds must be included
		//   - values should be evenly distributed
		//   - values should be deterministic
		const uint32_t step = 1 + (ub - lb) / 0x100;
		for (uint32_t c = lb; c < ub; c += step)
		{
			arcs[n].push_back (c);
		}
		arcs[n].push_back (ub);

		arcsets[n].push_back (std::make_pair (lb, ub));
		lb = ub + 1;
	}
}

Node::~Node ()
{
	delete suffix;
}

bool Node::end () const
{
	return arcs.size () == 0;
}

Skeleton::Skeleton
	( const dfa_t &dfa
	, const charset_t &cs
	, const rules_t &rs
	, const std::string &dfa_name
	, const std::string &dfa_cond
	, uint32_t dfa_line
	)
	: name (dfa_name)
	, cond (dfa_cond)
	, line (dfa_line)
	, nodes_count (dfa.states.size())
	, nodes (new Node [nodes_count + 1]) // +1 for default state
	, sizeof_key (4)
	, rules (rs)
{
	const size_t nc = cs.size() - 1;

	// initialize skeleton nodes
	Node *nil = &nodes[nodes_count];
	for (size_t i = 0; i < nodes_count; ++i)
	{
		dfa_state_t *s = dfa.states[i];
		std::vector<std::pair<Node*, uint32_t> > arcs;
		for (size_t c = 0; c < nc;)
		{
			const size_t j = s->arcs[c];
			for (;++c < nc && s->arcs[c] == j;);
			Node *to = j == dfa_t::NIL
				? nil
				: &nodes[j];
			arcs.push_back(std::make_pair(to, cs[c]));
		}
		// all arcs go to default node => this node is final, drop arcs
		if (arcs.size() == 1 && arcs[0].first == nil)
		{
			arcs.clear();
		}
		nodes[i].init(s->ctx, s->rule, arcs);
	}

	// calculate maximal path length, check overflow
	nodes->calc_dist ();
	const uint32_t maxlen = nodes->dist;
	if (maxlen == Node::DIST_MAX)
	{
		error ("DFA path %sis too long", incond (cond).c_str ());
		exit(1);
	}

	// calculate maximal rule rank (disregarding default and none rules)
	uint32_t maxrule = 0;
	for (uint32_t i = 0; i < nodes_count; ++i)
	{
		const rule_rank_t r = nodes[i].rule.rank;
		if (!r.is_none () && !r.is_def ())
		{
			maxrule = std::max (maxrule, r.uint32 ());
		}
	}
	// two upper values reserved for default and none rules)
	maxrule += 2;

	// initialize size of key
	const uint32_t max = std::max (maxlen, maxrule);
	if (max <= std::numeric_limits<uint8_t>::max())
	{
		sizeof_key = 1;
	}
	else if (max <= std::numeric_limits<uint16_t>::max())
	{
		sizeof_key = 2;
	}
}

Skeleton::~Skeleton ()
{
	delete [] nodes;
}

uint32_t Skeleton::rule2key (rule_rank_t r) const
{
	switch (sizeof_key)
	{
		default: // shouldn't happen
		case 4: return rule2key<uint32_t> (r);
		case 2: return rule2key<uint16_t> (r);
		case 1: return rule2key<uint8_t>  (r);
	}
}

} // namespace re2c

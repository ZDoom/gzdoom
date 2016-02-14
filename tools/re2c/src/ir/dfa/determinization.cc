#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <vector>

#include "src/ir/dfa/dfa.h"
#include "src/ir/nfa/nfa.h"
#include "src/ir/regexp/regexp.h"
#include "src/ir/regexp/regexp_rule.h"
#include "src/ir/rule_rank.h"
#include "src/parse/rules.h"
#include "src/util/ord_hash_set.h"
#include "src/util/range.h"

namespace re2c
{

const size_t dfa_t::NIL = std::numeric_limits<size_t>::max();

/*
 * note [marking DFA states]
 *
 * DFA state is a set of NFA states.
 * However, DFA state includes not all NFA states that are in
 * epsilon-closure (NFA states that have only epsilon-transitions
 * and are not context of final states are omitted).
 * The included states are called 'kernel' states.
 *
 * We mark visited NFA states during closure construction.
 * These marks serve two purposes:
 *   - avoid loops in NFA
 *   - avoid duplication of NFA states in kernel
 *
 * Note that after closure construction:
 *   - all non-kernel states must be unmarked (these states are
 *     not stored in kernel and it is impossible to unmark them
 *     afterwards)
 *   - all kernel states must be marked (because we may later
 *     extend this kernel with epsilon-closure of another NFA
 *     state). Kernel states are unmarked later (before finding
 *     or adding DFA state).
 */
static nfa_state_t **closure(nfa_state_t **cP, nfa_state_t *n)
{
	if (!n->mark)
	{
		n->mark = true;
		switch (n->type)
		{
			case nfa_state_t::ALT:
				cP = closure(cP, n->value.alt.out2);
				cP = closure(cP, n->value.alt.out1);
				n->mark = false;
				break;
			case nfa_state_t::CTX:
				*(cP++) = n;
				cP = closure(cP, n->value.ctx.out);
				break;
			default:
				*(cP++) = n;
				break;
		}
	}

	return cP;
}

static size_t find_state
	( nfa_state_t **kernel
	, nfa_state_t **end
	, ord_hash_set_t &kernels
	)
{
	// zero-sized kernel corresponds to default state
	if (kernel == end)
	{
		return dfa_t::NIL;
	}

	// see note [marking DFA states]
	for (nfa_state_t **p = kernel; p != end; ++p)
	{
		(*p)->mark = false;
	}

	// sort kernel states: we need this to get stable hash
	// and to compare states with simple 'memcmp'
	std::sort(kernel, end);
	const size_t size = static_cast<size_t>(end - kernel) * sizeof(nfa_state_t*);
	return kernels.insert(kernel, size);
}

dfa_t::dfa_t(const nfa_t &nfa, const charset_t &charset, rules_t &rules)
	: states()
	, nchars(charset.size() - 1) // (n + 1) bounds for n ranges
{
	std::map<size_t, std::set<RuleOp*> > s2rules;
	ord_hash_set_t kernels;
	nfa_state_t **const buffer = new nfa_state_t*[nfa.size];
	std::vector<std::vector<nfa_state_t*> > arcs(nchars);

	find_state(buffer, closure(buffer, nfa.root), kernels);
	for (size_t i = 0; i < kernels.size(); ++i)
	{
		dfa_state_t *s = new dfa_state_t;
		states.push_back(s);

		nfa_state_t **kernel;
		const size_t kernel_size = kernels.deref<nfa_state_t*>(i, kernel);
		for (size_t j = 0; j < kernel_size; ++j)
		{
			nfa_state_t *n = kernel[j];
			switch (n->type)
			{
				case nfa_state_t::RAN:
				{
					nfa_state_t *m = n->value.ran.out;
					size_t c = 0;
					for (Range *r = n->value.ran.ran; r; r = r->next ())
					{
						for (; charset[c] != r->lower(); ++c);
						for (; charset[c] != r->upper(); ++c)
						{
							arcs[c].push_back(m);
						}
					}
					break;
				}
				case nfa_state_t::CTX:
					s->ctx = true;
					break;
				case nfa_state_t::FIN:
					s2rules[i].insert(n->value.fin.rule);
					break;
				default:
					break;
			}
		}

		s->arcs = new size_t[nchars];
		for(size_t c = 0; c < nchars; ++c)
		{
			nfa_state_t **end = buffer;
			for (std::vector<nfa_state_t*>::const_iterator j = arcs[c].begin(); j != arcs[c].end(); ++j)
			{
				end = closure(end, *j);
			}
			s->arcs[c] = find_state(buffer, end, kernels);
		}

		for(size_t c = 0; c < nchars; ++c)
		{
			arcs[c].clear();
		}
	}
	delete[] buffer;

	const size_t count = states.size();
	for (size_t i = 0; i < count; ++i)
	{
		dfa_state_t *s = states[i];
		std::set<RuleOp*> &rs = s2rules[i];
		// for each final state: choose the rule with the smallest rank
		for (std::set<RuleOp*>::const_iterator j = rs.begin(); j != rs.end(); ++j)
		{
			RuleOp *rule = *j;
			if (!s->rule || rule->rank < s->rule->rank)
			{
				s->rule = rule;
			}
		}
		// other rules are shadowed by the chosen rule
		for (std::set<RuleOp*>::const_iterator j = rs.begin(); j != rs.end(); ++j)
		{
			RuleOp *rule = *j;
			if (s->rule != rule)
			{
				rules[rule->rank].shadow.insert(s->rule->rank);
			}
		}
	}
}

dfa_t::~dfa_t()
{
	std::vector<dfa_state_t*>::iterator
		i = states.begin(),
		e = states.end();
	for (; i != e; ++i)
	{
		delete *i;
	}
}

} // namespace re2c


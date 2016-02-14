#include <limits>
#include <stack>
#include <vector>

#include "src/ir/dfa/dfa.h"

namespace re2c
{

static const size_t INFINITI = std::numeric_limits<size_t>::max();
static const size_t UNDEFINED = INFINITI - 1;

static bool loopback(size_t node, size_t narcs, const size_t *arcs)
{
	for (size_t i = 0; i < narcs; ++i)
	{
		if (arcs[i] == node)
		{
			return true;
		}
	}
	return false;
}

/*
 * node [finding strongly connected components of DFA]
 *
 * A slight modification of Tarjan's algorithm.
 *
 * The algorithm walks graph in deep-first order. It maintains a stack
 * of nodes that have already been visited but haven't been assigned to
 * SCC yet. For each node the algorithm calculates 'lowlink': index of
 * the highest ancestor node reachable in one step from a descendant of
 * the node. Lowlink is used to determine when a set of nodes should be
 * popped off the stack into a new SCC.
 *
 * We use lowlink to hold different kinds of information:
 *   - values in range [0 .. stack size] mean that this node is on stack
 *     (link to a node with the smallest index reachable from this one)
 *   - UNDEFINED means that this node has not been visited yet
 *   - INFINITI means that this node has already been popped off stack
 *
 * We use stack size (rather than topological sort index) as unique index
 * of a node on stack. This is safe because indices of nodes on stack are
 * still unique and less than indices of nodes that have been popped off
 * stack (INFINITI).
 *
 */
static void scc(
	const dfa_t &dfa,
	std::stack<size_t> &stack,
	std::vector<size_t> &lowlink,
	std::vector<bool> &trivial,
	size_t i)
{
	const size_t link = stack.size();
	lowlink[i] = link;
	stack.push(i);

	const size_t *arcs = dfa.states[i]->arcs;
	for (size_t c = 0; c < dfa.nchars; ++c)
	{
		const size_t j = arcs[c];
		if (j != dfa_t::NIL)
		{
			if (lowlink[j] == UNDEFINED)
			{
				scc(dfa, stack, lowlink, trivial, j);
			}
			if (lowlink[j] < lowlink[i])
			{
				lowlink[i] = lowlink[j];
			}
		}
	}

	if (lowlink[i] == link)
	{
		// SCC is non-trivial (has loops) iff it either:
		//   - consists of multiple nodes (they all must be interconnected)
		//   - consists of single node which loops back to itself
		trivial[i] = i == stack.top()
			&& !loopback(i, dfa.nchars, arcs);

		size_t j;
		do
		{
			j = stack.top();
			stack.pop();
			lowlink[j] = INFINITI;
		}
		while (j != i);
	}
}

static void calc_fill(
	const dfa_t &dfa,
	const std::vector<bool> &trivial,
	std::vector<size_t> &fill,
	size_t i)
{
	if (fill[i] == UNDEFINED)
	{
		fill[i] = 0;
		const size_t *arcs = dfa.states[i]->arcs;
		for (size_t c = 0; c < dfa.nchars; ++c)
		{
			const size_t j = arcs[c];
			if (j != dfa_t::NIL)
			{
				calc_fill(dfa, trivial, fill, j);
				size_t max = 1;
				if (trivial[j])
				{
					max += fill[j];
				}
				if (max > fill[i])
				{
					fill[i] = max;
				}
			}
		}
	}
}

void fillpoints(const dfa_t &dfa, std::vector<size_t> &fill)
{
	const size_t size = dfa.states.size();

	// find DFA states that belong to non-trivial SCC
	std::stack<size_t> stack;
	std::vector<size_t> lowlink(size, UNDEFINED);
	std::vector<bool> trivial(size, false);
	scc(dfa, stack, lowlink, trivial, 0);

	// for each DFA state, calculate YYFILL argument:
	// maximal path length to the next YYFILL state
	fill.resize(size, UNDEFINED);
	calc_fill(dfa, trivial, fill, 0);

	// The following states must trigger YYFILL:
	//   - inital state
	//   - all states in non-trivial SCCs
	// for other states, reset YYFILL argument to zero
	for (size_t i = 1; i < size; ++i)
	{
		if (trivial[i])
		{
			fill[i] = 0;
		}
	}
}

} // namespace re2c

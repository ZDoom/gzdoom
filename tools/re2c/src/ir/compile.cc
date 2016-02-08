#include <algorithm>
#include <ostream>
#include <set>

#include "src/codegen/output.h"
#include "src/ir/compile.h"
#include "src/ir/adfa/adfa.h"
#include "src/ir/dfa/dfa.h"
#include "src/ir/nfa/nfa.h"
#include "src/ir/regexp/regexp.h"
#include "src/ir/skeleton/skeleton.h"
#include "src/parse/spec.h"

namespace re2c {

static std::string make_name(const std::string &cond, uint32_t line)
{
	std::ostringstream os;
	os << "line" << line;
	std::string name = os.str();
	if (!cond.empty ())
	{
		name += "_";
		name += cond;
	}
	return name;
}

smart_ptr<DFA> compile (Spec & spec, Output & output, const std::string & cond, uint32_t cunits)
{
	const uint32_t line = output.source.get_block_line();
	const std::string name = make_name(cond, line);

	// The original set of code units (charset) might be very large.
	// A common trick it is to split charset into disjoint character ranges
	// and choose a representative of each range (we choose lower bound).
	// The set of all representatives is the new (compacted) charset.
	// Don't forget to include zero and upper bound, even if they
	// do not explicitely apper in ranges.
	std::set<uint32_t> bounds;
	spec.re->split(bounds);
	bounds.insert(0);
	bounds.insert(cunits);
	charset_t cs;
	for (std::set<uint32_t>::const_iterator i = bounds.begin(); i != bounds.end(); ++i)
	{
		cs.push_back(*i);
	}

	nfa_t nfa(spec.re);

	dfa_t dfa(nfa, cs, spec.rules);

	// skeleton must be constructed after DFA construction
	// but prior to any other DFA transformations
	Skeleton *skeleton = new Skeleton(dfa, cs, spec.rules, name, cond, line);

	minimization(dfa);

	// find YYFILL states and calculate argument to YYFILL
	std::vector<size_t> fill;
	fillpoints(dfa, fill);

	// ADFA stands for 'DFA with actions'
	DFA *adfa = new DFA(dfa, fill, skeleton, cs, name, cond, line);

	/*
	 * note [reordering DFA states]
	 *
	 * re2c-generated code depends on the order of states in DFA: simply
	 * flipping two states may change the output significantly.
	 * The order of states is affected by many factors, e.g.:
	 *   - flipping left and right subtrees of alternative when constructing
	 *     AST (also applies to iteration and counted repetition)
	 *   - changing the order in which graph nodes are visited (applies to
	 *     any intermediate representation: bytecode, NFA, DFA, etc.)
	 *
	 * To make the resulting code independent of such changes, we hereby
	 * reorder DFA states. The ordering scheme is very simple:
	 *
	 * Starting with DFA root, walk DFA nodes in breadth-first order.
	 * Child nodes are ordered accoding to the (alphabetically) first symbol
	 * leading to each node. Each node must be visited exactly once.
	 * Default state (NULL) is always the last state.
	 */
	adfa->reorder();

	// skeleton is constructed, do further DFA transformations
	adfa->prepare();

	// finally gather overall DFA statistics
	adfa->calc_stats();

	// accumulate global statistics from this particular DFA
	output.max_fill = std::max (output.max_fill, adfa->max_fill);
	if (adfa->need_accept)
	{
		output.source.set_used_yyaccept ();
	}

	return make_smart_ptr(adfa);
}

} // namespace re2c

#ifndef _RE2C_IR_SKELETON_SKELETON_
#define _RE2C_IR_SKELETON_SKELETON_

#include "src/util/c99_stdint.h"
#include <stddef.h>
#include <stdio.h>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <utility>

#include "src/ir/regexp/regexp.h"
#include "src/ir/rule_rank.h"
#include "src/ir/skeleton/path.h"
#include "src/ir/skeleton/way.h"
#include "src/parse/rules.h"
#include "src/util/local_increment.h"
#include "src/util/forbid_copy.h"
#include "src/util/u32lim.h"

namespace re2c
{

struct dfa_t;
struct OutputFile;
class RuleOp;

struct Node
{
	/*
	 * note [counting skeleton edges]
	 *
	 * To avoid any possible overflows all size calculations are wrapped in
	 * a special truncated unsigned 32-bit integer type that checks overflow
	 * on each binary operation or conversion from another type.
	 *
	 * Two things contribute to size calculation: path length and the number
	 * of outgoing arcs in each node. Some considerations on why these values
	 * will not overflow before they are converted to truncated type:
	 *
	 *   - Maximal number of outgoing arcs in each node cannot exceed 32 bits:
	 *     it is bounded by the number of code units in current encoding, and
	 *     re2c doesn't support any encoding with more than 2^32 code units.
	 *     Conversion is safe.
	 *
	 *   - Maximal path length cannot exceed 32 bits: we estimate it right
	 *     after skeleton construction and check for overflow. If path length
	 *     does overflow, an error is reported and re2c aborts.
	 */

	// Type for calculating the size of path cover.
	// Paths are dumped to file as soon as generated and don't eat
	// heap space. The total size of path cover (measured in edges)
	// is O(N^2) where N is the number of edges in skeleton.
	typedef u32lim_t<1024 * 1024 * 1024> covers_t; // ~1Gb

	// Type for counting arcs in paths that cause undefined behaviour.
	// These paths are stored on heap, so the limit should be low.
	// Most real-world cases have only a few short paths.
	// We don't need all paths anyway, just some examples.
	typedef u32lim_t<1024> nakeds_t; // ~1Kb

	typedef std::map<Node *, path_t::arc_t> arcs_t;
	typedef std::map<Node *, way_arc_t> arcsets_t;
	typedef local_increment_t<uint8_t> local_inc;

	// outgoing arcs
	arcs_t arcs;
	arcsets_t arcsets;

	// how many times this node has been visited
	// (controls looping in graph traversals)
	uint8_t loop;

	// rule for corresponding DFA state (if any)
	rule_t rule;

	// start of trailing context
	bool ctx;

	// maximal distance to end node (assuming one iteration per loop)
	static const uint32_t DIST_ERROR;
	static const uint32_t DIST_MAX;
	uint32_t dist;

	// rules reachable from this node (including absent rule)
	std::set<rule_t> reachable;

	// path to end node (for constructing path cover)
	path_t * suffix;

	Node ();
	void init(bool b, RuleOp *r, const std::vector<std::pair<Node*, uint32_t> > &arcs);
	~Node ();
	bool end () const;
	void calc_dist ();
	void calc_reachable ();
	template <typename cunit_t, typename key_t>
		void cover (path_t & prefix, FILE * input, FILE * keys, covers_t &size);
	void naked_ways (way_t & prefix, std::vector<way_t> & ways, nakeds_t &size);

	FORBID_COPY (Node);
};

struct Skeleton
{
	const std::string name;
	const std::string cond;
	const uint32_t line;

	const size_t nodes_count;
	Node * nodes;
	size_t sizeof_key;
	rules_t rules;

	Skeleton
		( const dfa_t &dfa
		, const charset_t &cs
		, const rules_t & rs
		, const std::string &dfa_name
		, const std::string &dfa_cond
		, uint32_t dfa_line
		);
	~Skeleton ();
	void warn_undefined_control_flow ();
	void warn_unreachable_rules ();
	void warn_match_empty ();
	void emit_data (const char * fname);
	static void emit_prolog (OutputFile & o);
	void emit_start
		( OutputFile & o
		, size_t maxfill
		, bool backup
		, bool backupctx
		, bool accept
		) const;
	void emit_end
		( OutputFile & o
		, bool backup
		, bool backupctx
		) const;
	static void emit_epilog (OutputFile & o, const std::set<std::string> & names);
	void emit_action (OutputFile & o, uint32_t ind, rule_rank_t rank) const;

	template <typename key_t> static key_t rule2key (rule_rank_t r);
	uint32_t rule2key (rule_rank_t r) const;

private:
	template <typename cunit_t, typename key_t>
		void generate_paths_cunit_key (FILE * input, FILE * keys);
	template <typename cunit_t>
		void generate_paths_cunit (FILE * input, FILE * keys);
	void generate_paths (FILE * input, FILE * keys);

	FORBID_COPY (Skeleton);
};

template<typename key_t> key_t Skeleton::rule2key (rule_rank_t r)
{
	if (r.is_none()) {
		return std::numeric_limits<key_t>::max();
	} else if (r.is_def()) {
		key_t k = std::numeric_limits<key_t>::max();
		return --k;
	} else {
		return static_cast<key_t>(r.uint32());
	}
}

} // namespace re2c

#endif // _RE2C_IR_SKELETON_SKELETON_

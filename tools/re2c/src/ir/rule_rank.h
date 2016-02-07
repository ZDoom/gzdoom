#ifndef _RE2C_IR_RULE_RANK_
#define _RE2C_IR_RULE_RANK_

#include "src/util/c99_stdint.h"
#include <iosfwd>

namespace re2c
{

template <typename num_t> class counter_t;

// rule rank public API:
//     - get rule rank corresponding to nonexistent/default rule
//     - check if rank corresponds to nonexistent/default rule
//     - compare ranks
//     - output rank to std::ostream
//
// rule rank private API (for rule rank counter):
//     - get first rank
//     - get next rank
class rule_rank_t
{
	static const uint32_t NONE;
	static const uint32_t DEF;
	uint32_t value;
	rule_rank_t ();
	void inc ();

public:
	static rule_rank_t none ();
	static rule_rank_t def ();
	bool is_none () const;
	bool is_def () const;
	bool operator < (const rule_rank_t & r) const;
	bool operator == (const rule_rank_t & r) const;
	friend std::ostream & operator << (std::ostream & o, rule_rank_t r);
	uint32_t uint32 () const;

	friend class counter_t<rule_rank_t>;
};

} // namespace re2c

#endif // _RE2C_IR_RULE_RANK_

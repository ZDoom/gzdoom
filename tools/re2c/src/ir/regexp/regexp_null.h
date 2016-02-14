#ifndef _RE2C_IR_REGEXP_REGEXP_NULL_
#define _RE2C_IR_REGEXP_REGEXP_NULL_

#include "src/ir/regexp/regexp.h"

namespace re2c
{

class NullOp: public RegExp
{
public:
	void split (std::set<uint32_t> &);
	uint32_t calc_size() const;
	uint32_t fixedLength ();
	nfa_state_t *compile(nfa_t &nfa, nfa_state_t *n);
	void display (std::ostream & o) const;
};

} // end namespace re2c

#endif // _RE2C_IR_REGEXP_REGEXP_NULL_

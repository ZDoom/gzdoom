#ifndef _RE2C_IR_REGEXP_REGEXP_CAT_
#define _RE2C_IR_REGEXP_REGEXP_CAT_

#include "src/ir/regexp/regexp.h"

namespace re2c
{

class CatOp: public RegExp
{
	RegExp * exp1;
	RegExp * exp2;

public:
	inline CatOp (RegExp * e1, RegExp * e2)
		: exp1 (e1)
		, exp2 (e2)
	{}
	void split (std::set<uint32_t> &);
	uint32_t calc_size() const;
	uint32_t fixedLength ();
	nfa_state_t *compile(nfa_t &nfa, nfa_state_t *n);
	void display (std::ostream & o) const;

	FORBID_COPY (CatOp);
};

} // end namespace re2c

#endif // _RE2C_IR_REGEXP_REGEXP_CAT_

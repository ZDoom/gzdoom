#ifndef _RE2C_IR_REGEXP_REGEXP_
#define _RE2C_IR_REGEXP_REGEXP_

#include "src/util/c99_stdint.h"
#include <iosfwd>
#include <set>
#include <vector>

#include "src/util/free_list.h"
#include "src/util/forbid_copy.h"

namespace re2c
{

struct nfa_state_t;
struct nfa_t;

typedef std::vector<uint32_t> charset_t;

class RegExp
{
public:
	static free_list <RegExp *> vFreeList;

	inline RegExp ()
	{
		vFreeList.insert (this);
	}
	inline virtual ~RegExp ()
	{
		vFreeList.erase (this);
	}
	virtual void split (std::set<uint32_t> &) = 0;
	virtual uint32_t calc_size() const = 0;
	virtual uint32_t fixedLength ();
	virtual nfa_state_t *compile(nfa_t &nfa, nfa_state_t *n) = 0;
	virtual void display (std::ostream &) const = 0;
	friend std::ostream & operator << (std::ostream & o, const RegExp & re);

	FORBID_COPY (RegExp);
};

RegExp * doAlt (RegExp * e1, RegExp * e2);
RegExp * mkAlt (RegExp * e1, RegExp * e2);
RegExp * doCat (RegExp * e1, RegExp * e2);
RegExp * repeat (RegExp * e, uint32_t n);
RegExp * repeat_from_to (RegExp * e, uint32_t n, uint32_t m);
RegExp * repeat_from (RegExp * e, uint32_t n);

} // end namespace re2c

#endif // _RE2C_IR_REGEXP_REGEXP_

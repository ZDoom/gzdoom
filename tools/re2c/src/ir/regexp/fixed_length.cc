#include "src/util/c99_stdint.h"

#include "src/ir/regexp/regexp.h"
#include "src/ir/regexp/regexp_alt.h"
#include "src/ir/regexp/regexp_cat.h"
#include "src/ir/regexp/regexp_match.h"
#include "src/ir/regexp/regexp_null.h"

namespace re2c
{

uint32_t RegExp::fixedLength ()
{
	return ~0u;
}

uint32_t AltOp::fixedLength ()
{
	uint32_t l1 = exp1->fixedLength ();
	uint32_t l2 = exp1->fixedLength ();

	if (l1 != l2 || l1 == ~0u)
	{
		return ~0u;
	}

	return l1;
}

uint32_t CatOp::fixedLength ()
{
	const uint32_t l1 = exp1->fixedLength ();
	if (l1 != ~0u)
	{
		const uint32_t l2 = exp2->fixedLength ();
		if (l2 != ~0u)
		{
			return l1 + l2;
		}
	}
	return ~0u;
}

uint32_t MatchOp::fixedLength ()
{
	return 1;
}

uint32_t NullOp::fixedLength ()
{
	return 0;
}

} // end namespace re2c


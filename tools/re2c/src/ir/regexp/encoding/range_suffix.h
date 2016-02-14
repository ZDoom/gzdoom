#ifndef _RE2C_IR_REGEXP_ENCODING_RANGE_SUFFIX_
#define _RE2C_IR_REGEXP_ENCODING_RANGE_SUFFIX_

#include "src/util/c99_stdint.h"
#include <stddef.h> // NULL

#include "src/util/forbid_copy.h"
#include "src/util/free_list.h"

namespace re2c {

class RegExp;

struct RangeSuffix
{
	static free_list<RangeSuffix *> freeList;

	uint32_t l;
	uint32_t h;
	RangeSuffix * next;
	RangeSuffix * child;

	RangeSuffix (uint32_t lo, uint32_t hi)
		: l     (lo)
		, h     (hi)
		, next  (NULL)
		, child (NULL)
	{
		freeList.insert(this);
	}

	FORBID_COPY (RangeSuffix);
};

RegExp * to_regexp (RangeSuffix * p);

} // namespace re2c

#endif // _RE2C_IR_REGEXP_ENCODING_RANGE_SUFFIX_

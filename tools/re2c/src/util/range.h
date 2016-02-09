#ifndef _RE2C_UTIL_RANGE_
#define _RE2C_UTIL_RANGE_

#include "src/util/c99_stdint.h"
#include <assert.h>
#include <stddef.h> // NULL

#include "src/test/range/test.h"
#include "src/util/forbid_copy.h"
#include "src/util/free_list.h"

namespace re2c
{

class Range
{
public:
	static free_list<Range*> vFreeList;

private:
	Range * nx;
	// [lb,ub)
	uint32_t lb;
	uint32_t ub;

public:
	static Range * sym (uint32_t c)
	{
		return new Range (NULL, c, c + 1);
	}
	static Range * ran (uint32_t l, uint32_t u)
	{
		return new Range (NULL, l, u);
	}
	~Range ()
	{
		vFreeList.erase (this);
	}
	Range * next () const { return nx; }
	uint32_t lower () const { return lb; }
	uint32_t upper () const { return ub; }
	static Range * add (const Range * r1, const Range * r2);
	static Range * sub (const Range * r1, const Range * r2);

private:
	Range (Range * n, uint32_t l, uint32_t u)
		: nx (n)
		, lb (l)
		, ub (u)
	{
		assert (lb < ub);
		vFreeList.insert (this);
	}
	static void append_overlapping (Range * & head, Range * & tail, const Range * r);
	static void append (Range ** & ptail, uint32_t l, uint32_t u);

	// test addition and subtraction
	//template <uint8_t> friend Range * re2c_test::range (uint32_t n);

	FORBID_COPY (Range);
};

} // namespace re2c

#endif // _RE2C_UTIL_RANGE_

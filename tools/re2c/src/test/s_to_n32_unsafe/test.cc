#include <limits>
#include <stdio.h>

#include "src/util/s_to_n32_unsafe.h"

namespace re2c_test {

static const uint32_t DIGITS = 256;

// writes string backwards and returns pointer to the start
// no terminating null as we don't need it
static char * u64_to_s_fastest_ever (uint64_t u, char * s)
{
	while (u > 0)
	{
		const uint64_t d = u % 10 + '0';
		*--s = static_cast<char> (d);
		u /= 10;
	}
	return s;
}

static int32_t test_u (uint64_t i)
{
	char s [DIGITS];
	char * const s_end = s + DIGITS;
	char * const s_start = u64_to_s_fastest_ever (i, s_end);
	uint32_t u = i == 0; // not equal to i
	if (s_to_u32_unsafe (s_start, s_end, u) && u != i)
	{
		fprintf (stderr, "unsigned: expected: %lu, got: %u\n", i, u);
		return 1;
	}
	return 0;
}

static int32_t test_i (int64_t i)
{
	char s [DIGITS];
	char * const s_end = s + DIGITS;
	const uint64_t i_abs = i < 0
		? static_cast<uint64_t> (-i)
		: static_cast<uint64_t> (i);
	char * s_start = u64_to_s_fastest_ever (i_abs, s_end);
	if (i < 0)
	{
		*--s_start = '-';
	}
	int32_t j = i == 0; // not equal to i
	if (s_to_i32_unsafe (s_start, s_end, j) && j != i)
	{
		fprintf (stderr, "signed: expected: %ld, got: %d\n", i, j);
		return 1;
	}
	return 0;
}

static int32_t test ()
{
	int32_t ok = 0;

	static const uint64_t UDELTA = 0xFFFF;
	// zero neighbourhood
	for (uint64_t i = 0; i <= UDELTA; ++i)
	{
		ok |= test_u (i);
	}
	// u32_max neighbourhood
	static const uint64_t u32_max = std::numeric_limits<uint32_t>::max();
	for (uint64_t i = u32_max - UDELTA; i <= u32_max + UDELTA; ++i)
	{
		ok |= test_u (i);
	}

	static const int64_t IDELTA = 0xFFFF;
	// i32_min neighbourhood
	static const int64_t i32_min = std::numeric_limits<int32_t>::min();
	for (int64_t i = i32_min - IDELTA; i <= i32_min + IDELTA; ++i)
	{
		ok |= test_i (i);
	}
	// zero neighbourhood
	for (int64_t i = -IDELTA; i <= IDELTA; ++i)
	{
		ok |= test_i (i);
	}
	// i32_max neighbourhood
	static const int64_t i32_max = std::numeric_limits<int32_t>::max();
	for (int64_t i = i32_max - IDELTA; i <= i32_max + IDELTA; ++i)
	{
		ok |= test_i (i);
	}

	return ok;
}

} // namespace re2c_test

int main ()
{
	return re2c_test::test ();
}

#include <stdio.h>

#include "src/test/range/test.h"
#include "src/test/range/test-impl.h"

namespace re2c_test {

static bool equal (const re2c::Range * r1, const re2c::Range * r2)
{
	for (; r1 && r2; r1 = r1->next (), r2 = r2->next ())
	{
		if (r1->lower () != r2->lower ()
			|| r1->upper () != r2->upper ())
		{
			return false;
		}
	}
	return !r1 && !r2;
}

static void show (const re2c::Range * r)
{
	if (!r)
	{
		fprintf (stderr, "[]");
	}
	for (; r; r = r->next ())
	{
		const uint32_t l = r->lower ();
		const uint32_t u = r->upper () - 1;
		if (l < u)
		{
			fprintf (stderr, "[%X-%X]", l, u);
		}
		else
		{
			fprintf (stderr, "[%X]", l);
		}
	}
}

static int32_t diff
	( const re2c::Range * r1
	, const re2c::Range * r2
	, const re2c::Range * op1
	, const re2c::Range * op2
	, const char * op)
{
	if (equal (op1, op2))
	{
		return 0;
	}
	else
	{
		fprintf (stderr, "%s error: ", op);
		show (r1);
		fprintf (stderr, " %s ", op);
		show (r2);
		fprintf (stderr, " ====> ");
		show (op2);
		fprintf (stderr, " =/= ");
		show (op1);
		fprintf (stderr, "\n");
		return 1;
	}
}

static int32_t test ()
{
	int32_t ok = 0;

	static const uint32_t BITS = 8;
	static const uint32_t N = 1u << BITS;
	for (uint32_t i = 0; i <= N; ++i)
	{
		for (uint32_t j = 0; j <= N; ++j)
		{
			re2c::Range * r1 = range<BITS> (i);
			re2c::Range * r2 = range<BITS> (j);
			ok |= diff (r1, r2, add<BITS> (i, j), re2c::Range::add (r1, r2), "U");
			ok |= diff (r1, r2, sub<BITS> (i, j), re2c::Range::sub (r1, r2), "D");
			re2c::Range::vFreeList.clear ();
		}
	}

	return ok;
}

} // namespace re2c_test

int main ()
{
	return re2c_test::test ();
}

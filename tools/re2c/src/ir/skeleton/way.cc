#include <stddef.h>
#include <algorithm>

#include "src/ir/skeleton/way.h"

namespace re2c
{

static bool cmp_way_arcs (const way_arc_t * a1, const way_arc_t * a2);
static void fprint_way_arc (FILE * f, const way_arc_t & arc);

bool cmp_way_arcs (const way_arc_t * a1, const way_arc_t * a2)
{
	return std::lexicographical_compare(a1->begin(), a1->end(), a2->begin(), a2->end());
}

// define strict weak ordering on patterns:
// 1st criterion is length (short patterns go first)
// 2nd criterion is lexicographical order (applies to patterns of equal length)
bool cmp_ways (const way_t & w1, const way_t & w2)
{
	const size_t s1 = w1.size ();
	const size_t s2 = w2.size ();
	return (s1 == s2 && std::lexicographical_compare(w1.begin(), w1.end(), w2.begin(), w2.end(), cmp_way_arcs))
		|| s1 < s2;
}

void fprint_way (FILE * f, const way_t & w)
{
	fprintf (f, "'");
	const size_t len = w.size ();
	for (size_t i = 0 ; i < len; ++i)
	{
		if (i > 0)
		{
			fprintf (f, " ");
		}
		if (w[i] == NULL)
		{
			fprintf (stderr, "(nil)");
		}
		else
		{
			fprint_way_arc (stderr, *w[i]);
		}
	}
	fprintf (f, "'");
}

void fprint_way_arc (FILE * f, const way_arc_t & arc)
{
	const size_t ranges = arc.size ();
	if (ranges == 1 && arc[0].first == arc[0].second)
	{
		fprintf (f, "\\x%X", arc[0].first);
	}
	else
	{
		fprintf (f, "[");
		for (size_t i = 0; i < ranges; ++i)
		{
			const uint32_t l = arc[i].first;
			const uint32_t u = arc[i].second;
			fprintf (f, "\\x%X", l);
			if (l != u)
			{
				fprintf (f, "-\\x%X", u);
			}
		}
		fprintf (f, "]");
	}
}

} // namespace re2c

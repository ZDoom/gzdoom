#include <algorithm> // min
#include <string.h> // memset

#include "src/codegen/bitmap.h"
#include "src/codegen/go.h"
#include "src/codegen/output.h"
#include "src/conf/opt.h"
#include "src/globals.h"

namespace re2c
{

BitMap *BitMap::first = NULL;

BitMap::BitMap(const Go *g, const State *x)
	: go(g)
	, on(x)
	, next(first)
	, i(0)
	, m(0)
{
	first = this;
}

BitMap::~BitMap()
{
	delete next;
}

const BitMap *BitMap::find(const Go *g, const State *x)
{
	for (const BitMap *b = first; b; b = b->next)
	{
		if (matches(b->go->span, b->go->nSpans, b->on, g->span, g->nSpans, x))
		{
			return b;
		}
	}

	return new BitMap(g, x);
}

const BitMap *BitMap::find(const State *x)
{
	for (const BitMap *b = first; b; b = b->next)
	{
		if (b->on == x)
		{
			return b;
		}
	}

	return NULL;
}

static void doGen(const Go *g, const State *s, uint32_t *bm, uint32_t f, uint32_t m)
{
	Span *b = g->span, *e = &b[g->nSpans];
	uint32_t lb = 0;

	for (; b < e; ++b)
	{
		if (b->to == s)
		{
			for (; lb < b->ub && lb < 256; ++lb)
			{
				bm[lb-f] |= m;
			}
		}

		lb = b->ub;
	}
}

void BitMap::gen(OutputFile & o, uint32_t ind, uint32_t lb, uint32_t ub)
{
	if (first && bUsedYYBitmap)
	{
		o.wind(ind).ws("static const unsigned char ").wstring(opts->yybm).ws("[] = {");

		uint32_t c = 1, n = ub - lb;
		const BitMap *cb = first;

		while((cb = cb->next) != NULL) {
			++c;
		}
		BitMap *b = first;

		uint32_t *bm = new uint32_t[n];
		
		for (uint32_t i = 0, t = 1; b; i += n, t += 8)
		{
			memset(bm, 0, n * sizeof(uint32_t));

			for (uint32_t m = 0x80; b && m; m >>= 1)
			{
				b->i = i;
				b->m = m;
				doGen(b->go, b->on, bm, lb, m);
				b = const_cast<BitMap*>(b->next);
			}

			if (c > 8)
			{
				o.ws("\n").wind(ind+1).ws("/* table ").wu32(t).ws(" .. ").wu32(std::min(c, t+7)).ws(": ").wu32(i).ws(" */");
			}

			for (uint32_t j = 0; j < n; ++j)
			{
				if (j % 8 == 0)
				{
					o.ws("\n").wind(ind+1);
				}

				if (opts->yybmHexTable)
				{
					o.wu32_hex(bm[j]);
				}
				else
				{
					o.wu32_width(bm[j], 3);
				}
				o.ws(", ");
			}
		}

		o.ws("\n").wind(ind).ws("};\n");
		
		delete[] bm;
	}
}

// All spans in b1 that lead to s1 are pairwise equal to that in b2 leading to s2
bool matches(const Span * b1, uint32_t n1, const State * s1, const Span * b2, uint32_t n2, const State * s2)
{
	const Span * e1 = &b1[n1];
	uint32_t lb1 = 0;
	const Span * e2 = &b2[n2];
	uint32_t lb2 = 0;

	for (;;)
	{
		for (; b1 < e1 && b1->to != s1; ++b1)
		{
			lb1 = b1->ub;
		}
		for (; b2 < e2 && b2->to != s2; ++b2)
		{
			lb2 = b2->ub;
		}
		if (b1 == e1)
		{
			return b2 == e2;
		}
		if (b2 == e2)
		{
			return false;
		}
		if (lb1 != lb2 || b1->ub != b2->ub)
		{
			return false;
		}
		++b1;
		++b2;
	}
}

} // end namespace re2c

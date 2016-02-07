#include <stddef.h>
#include "src/util/c99_stdint.h"
#include <string>
#include <utility>
#include <vector>

#include "src/codegen/bitmap.h"
#include "src/codegen/go.h"
#include "src/conf/opt.h"
#include "src/globals.h"
#include "src/ir/adfa/adfa.h"
#include "src/util/allocate.h"

namespace re2c
{

static uint32_t unmap (Span * new_span, const Span * old_span, uint32_t old_nspans, const State * x);

Cases::Cases (const Span * span, uint32_t span_size)
	: def (span_size == 0 ? NULL : span[span_size - 1].to)
	, cases (new Case[span_size])
	, cases_size (0)
{
	for (uint32_t i = 0, lb = 0; i < span_size; ++ i)
	{
		add (lb, span[i].ub, span[i].to);
		lb = span[i].ub;
	}
}

void Cases::add (uint32_t lb, uint32_t ub, State * to)
{
	for (uint32_t i = 0; i < cases_size; ++i)
	{
		if (cases[i].to == to)
		{
			cases[i].ranges.push_back (std::make_pair (lb, ub));
			return;
		}
	}
	cases[cases_size].ranges.push_back (std::make_pair (lb, ub));
	cases[cases_size].to = to;
	++cases_size;
}

Cond::Cond (const std::string & cmp, uint32_t val)
	: compare (cmp)
	, value (val)
{}

Binary::Binary (const Span * s, uint32_t n, const State * next)
	: cond (NULL)
	, thn (NULL)
	, els (NULL)
{
	const uint32_t l = n / 2;
	const uint32_t h = n - l;
	cond = new Cond ("<=", s[l - 1].ub - 1);
	thn = new If (l > 4 ? If::BINARY : If::LINEAR, &s[0], l, next);
	els = new If (h > 4 ? If::BINARY : If::LINEAR, &s[l], h, next);
}

Linear::Linear (const Span * s, uint32_t n, const State * next)
	: branches ()
{
	for (;;)
	{
		const State *bg = s[0].to;
		while (n >= 3 && s[2].to == bg && (s[1].ub - s[0].ub) == 1)
		{
			if (s[1].to == next && n == 3)
			{
				branches.push_back (std::make_pair (new Cond ("!=", s[0].ub), bg));
				return ;
			}
			else
			{
				branches.push_back (std::make_pair (new Cond ("==", s[0].ub), s[1].to));
			}
			n -= 2;
			s += 2;
		}
		if (n == 1)
		{
			if (next == NULL || s[0].to != next)
			{
				branches.push_back (std::make_pair (static_cast<const Cond *> (NULL), s[0].to));
			}
			return;
		}
		else if (n == 2 && bg == next)
		{
			branches.push_back (std::make_pair (new Cond (">=", s[0].ub), s[1].to));
			return;
		}
		else
		{
			branches.push_back (std::make_pair (new Cond ("<=", s[0].ub - 1), bg));
			n -= 1;
			s += 1;
		}
	}
}

If::If (type_t t, const Span * sp, uint32_t nsp, const State * next)
	: type (t)
	, info ()
{
	switch (type)
	{
		case BINARY:
			info.binary = new Binary (sp, nsp, next);
			break;
		case LINEAR:
			info.linear = new Linear (sp, nsp, next);
			break;
	}
}

SwitchIf::SwitchIf (const Span * sp, uint32_t nsp, const State * next)
	: type (IF)
	, info ()
{
	if ((!opts->sFlag && nsp > 2) || (nsp > 8 && (sp[nsp - 2].ub - sp[0].ub <= 3 * (nsp - 2))))
	{
		type = SWITCH;
		info.cases = new Cases (sp, nsp);
	}
	else if (nsp > 5)
	{
		info.ifs = new If (If::BINARY, sp, nsp, next);
	}
	else
	{
		info.ifs = new If (If::LINEAR, sp, nsp, next);
	}
}

GoBitmap::GoBitmap (const Span * span, uint32_t nSpans, const Span * hspan, uint32_t hSpans, const BitMap * bm, const State * bm_state, const State * next)
	: bitmap (bm)
	, bitmap_state (bm_state)
	, hgo (NULL)
	, lgo (NULL)
{
	Span * bspan = allocate<Span> (nSpans);
	uint32_t bSpans = unmap (bspan, span, nSpans, bm_state);
	lgo = bSpans == 0
		? NULL
		:  new SwitchIf (bspan, bSpans, next);
	// if there are any low spans, then next state for high spans
	// must be NULL to trigger explicit goto generation in linear 'if'
	hgo = hSpans == 0
		? NULL
		: new SwitchIf (hspan, hSpans, lgo ? NULL : next);
	operator delete (bspan);
}

const uint32_t CpgotoTable::TABLE_SIZE = 0x100;

CpgotoTable::CpgotoTable (const Span * span, uint32_t nSpans)
	: table (new const State * [TABLE_SIZE])
{
	uint32_t c = 0;
	for (uint32_t i = 0; i < nSpans; ++i)
	{
		for(; c < span[i].ub && c < TABLE_SIZE; ++c)
		{
			table[c] = span[i].to;
		}
	}
}

Cpgoto::Cpgoto (const Span * span, uint32_t nSpans, const Span * hspan, uint32_t hSpans, const State * next)
	: hgo (hSpans == 0 ? NULL : new SwitchIf (hspan, hSpans, next))
	, table (new CpgotoTable (span, nSpans))
{}

Dot::Dot (const Span * sp, uint32_t nsp, const State * s)
	: from (s)
	, cases (new Cases (sp, nsp))
{}

Go::Go ()
	: nSpans (0)
	, span (NULL)
	, type (EMPTY)
	, info ()
{}

void Go::init (const State * from)
{
	if (nSpans == 0)
	{
		return;
	}

	// initialize high (wide) spans
	uint32_t hSpans = 0;
	const Span * hspan = NULL;
	for (uint32_t i = 0; i < nSpans; ++i)
	{
		if (span[i].ub > 0x100)
		{
			hspan = &span[i];
			hSpans = nSpans - i;
			break;
		}
	}

	// initialize bitmaps
	uint32_t nBitmaps = 0;
	const BitMap * bitmap = NULL;
	const State * bitmap_state = NULL;
	for (uint32_t i = 0; i < nSpans; ++i)
	{
		if (span[i].to->isBase)
		{
			const BitMap *b = BitMap::find (span[i].to);
			if (b && matches(b->go->span, b->go->nSpans, b->on, span, nSpans, span[i].to))
			{
				if (bitmap == NULL)
				{
					bitmap = b;
					bitmap_state = span[i].to;
				}
				nBitmaps++;
			}
		}
	}

	const uint32_t dSpans = nSpans - hSpans - nBitmaps;
	if (opts->target == opt_t::DOT)
	{
		type = DOT;
		info.dot = new Dot (span, nSpans, from);
	}
	else if (opts->gFlag && (dSpans >= opts->cGotoThreshold))
	{
		type = CPGOTO;
		info.cpgoto = new Cpgoto (span, nSpans, hspan, hSpans, from->next);
	}
	else if (opts->bFlag && (nBitmaps > 0))
	{
		type = BITMAP;
		info.bitmap = new GoBitmap (span, nSpans, hspan, hSpans, bitmap, bitmap_state, from->next);
		bUsedYYBitmap = true;
	}
	else
	{
		type = SWITCH_IF;
		info.switchif = new SwitchIf (span, nSpans, from->next);
	}
}

/*
 * Find all spans, that map to the given state. For each of them,
 * find upper adjacent span, that maps to another state (if such
 * span exists, otherwize try lower one).
 * If input contains single span that maps to the given state,
 * then output contains 0 spans.
 */
uint32_t unmap (Span * new_span, const Span * old_span, uint32_t old_nspans, const State * x)
{
	uint32_t new_nspans = 0;
	for (uint32_t i = 0; i < old_nspans; ++i)
	{
		if (old_span[i].to != x)
		{
			if (new_nspans > 0 && new_span[new_nspans - 1].to == old_span[i].to)
				new_span[new_nspans - 1].ub = old_span[i].ub;
			else
			{
				new_span[new_nspans].to = old_span[i].to;
				new_span[new_nspans].ub = old_span[i].ub;
				++new_nspans;
			}
		}
	}
	if (new_nspans > 0)
		new_span[new_nspans - 1].ub = old_span[old_nspans - 1].ub;
	return new_nspans;
}

} // namespace re2c

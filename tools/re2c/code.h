/* $Id: code.h 525 2006-05-25 13:32:49Z helly $ */
#ifndef _code_h
#define _code_h

#include "re.h"
#include "dfa.h"

namespace re2c
{

class BitMap
{
public:
	static BitMap	*first;

	const Go        *go;
	const State     *on;
	const BitMap    *next;
	uint            i;
	uint            m;

public:
	static const BitMap *find(const Go*, const State*);
	static const BitMap *find(const State*);
	static void gen(std::ostream&, uint ind, uint, uint);
	static void stats();
	BitMap(const Go*, const State*);
	~BitMap();

#if PEDANTIC
	BitMap(const BitMap& oth)
		: go(oth.go)
		, on(oth.on)
		, next(oth.next)
		, i(oth.i)
		, m(oth.m)
	{
	}
	BitMap& operator = (const BitMap& oth)
	{
		new(this) BitMap(oth);
		return *this;
	}
#endif
};

#ifdef _MSC_VER
# pragma warning(disable: 4355) /* 'this' : used in base member initializer list */
#endif

} // end namespace re2c

#endif

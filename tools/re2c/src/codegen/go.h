#ifndef _RE2C_CODEGEN_GO_
#define _RE2C_CODEGEN_GO_

#include <iostream>
#include <set>
#include <vector>

#include "src/codegen/output.h"
#include "src/util/c99_stdint.h"
#include "src/util/forbid_copy.h"

namespace re2c
{

class BitMap;
struct State;
struct If;

struct Span
{
	uint32_t ub;
	State * to;

	FORBID_COPY (Span);
};

struct Case
{
	std::vector<std::pair<uint32_t, uint32_t> > ranges;
	const State * to;
	void emit (OutputFile & o, uint32_t ind);

	inline Case ()
		: ranges ()
		, to (NULL)
	{}

	FORBID_COPY (Case);
};

struct Cases
{
	const State * def;
	Case * cases;
	uint32_t cases_size;
	void add (uint32_t lb, uint32_t ub, State * to);
	Cases (const Span * s, uint32_t n);
	~Cases ();
	void emit (OutputFile & o, uint32_t ind, bool & readCh);
	void used_labels (std::set<label_t> & used);

	FORBID_COPY (Cases);
};

struct Cond
{
	std::string compare;
	uint32_t value;
	Cond (const std::string & cmp, uint32_t val);
};

struct Binary
{
	Cond * cond;
	If * thn;
	If * els;
	Binary (const Span * s, uint32_t n, const State * next);
	~Binary ();
	void emit (OutputFile & o, uint32_t ind, bool & readCh);
	void used_labels (std::set<label_t> & used);

	FORBID_COPY (Binary);
};

struct Linear
{
	std::vector<std::pair<const Cond *, const State *> > branches;
	Linear (const Span * s, uint32_t n, const State * next);
	~Linear ();
	void emit (OutputFile & o, uint32_t ind, bool & readCh);
	void used_labels (std::set<label_t> & used);
};

struct If
{
	enum type_t
	{
		BINARY,
		LINEAR
	} type;
	union
	{
		Binary * binary;
		Linear * linear;
	} info;
	If (type_t t, const Span * sp, uint32_t nsp, const State * next);
	~If ();
	void emit (OutputFile & o, uint32_t ind, bool & readCh);
	void used_labels (std::set<label_t> & used);
};

struct SwitchIf
{
	enum
	{
		SWITCH,
		IF
	} type;
	union
	{
		Cases * cases;
		If * ifs;
	} info;
	SwitchIf (const Span * sp, uint32_t nsp, const State * next);
	~SwitchIf ();
	void emit (OutputFile & o, uint32_t ind, bool & readCh);
	void used_labels (std::set<label_t> & used);
};

struct GoBitmap
{
	const BitMap * bitmap;
	const State * bitmap_state;
	SwitchIf * hgo;
	SwitchIf * lgo;
	GoBitmap (const Span * span, uint32_t nSpans, const Span * hspan, uint32_t hSpans, const BitMap * bm, const State * bm_state, const State * next);
	~GoBitmap ();
	void emit (OutputFile & o, uint32_t ind, bool & readCh);
	void used_labels (std::set<label_t> & used);

	FORBID_COPY (GoBitmap);
};

struct CpgotoTable
{
	static const uint32_t TABLE_SIZE;
	const State ** table;
	CpgotoTable (const Span * span, uint32_t nSpans);
	~CpgotoTable ();
	void emit (OutputFile & o, uint32_t ind);
	void used_labels (std::set<label_t> & used);

private:
	label_t max_label () const;

	FORBID_COPY (CpgotoTable);
};

struct Cpgoto
{
	SwitchIf * hgo;
	CpgotoTable * table;
	Cpgoto (const Span * span, uint32_t nSpans, const Span * hspan, uint32_t hSpans, const State * next);
	~Cpgoto ();
	void emit (OutputFile & o, uint32_t ind, bool & readCh);
	void used_labels (std::set<label_t> & used);

	FORBID_COPY (Cpgoto);
};

struct Dot
{
	const State * from;
	Cases * cases;
	Dot (const Span * sp, uint32_t nsp, const State * from);
	~Dot ();
	void emit (OutputFile & o);

	FORBID_COPY (Dot);
};

struct Go
{
	uint32_t nSpans; // number of spans
	Span * span;
	enum
	{
		EMPTY,
		SWITCH_IF,
		BITMAP,
		CPGOTO,
		DOT
	} type;
	union
	{
		SwitchIf * switchif;
		GoBitmap * bitmap;
		Cpgoto * cpgoto;
		Dot * dot;
	} info;

	Go ();
	~Go ();
	void init (const State * from);
	void emit (OutputFile & o, uint32_t ind, bool & readCh);
	void used_labels (std::set<label_t> & used);

	Go (const Go & g)
		: nSpans (g.nSpans)
		, span (g.span)
		, type (g.type)
		, info (g.info)
	{}
	Go & operator = (const Go & g)
	{
		nSpans = g.nSpans;
		span = g.span;
		type = g.type;
		info = g.info;
		return * this;
	}
};

} // namespace re2c

#endif // _RE2C_CODEGEN_GO_

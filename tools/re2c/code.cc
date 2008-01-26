/* $Id: code.cc 717 2007-04-29 22:29:59Z helly $ */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <iomanip>
#include <iostream>
#include "substr.h"
#include "globals.h"
#include "dfa.h"
#include "parser.h"
#include "code.h"

namespace re2c
{

// there must be at least one span in list;  all spans must cover
// same range

std::string indent(uint ind)
{
	std::string str;

	while (ind-- > 0)
	{
		str += indString;
	}
	return str;
}

static std::string space(uint this_label)
{
	int nl = next_label > 999999 ? 6 : next_label > 99999 ? 5 : next_label > 9999 ? 4 : next_label > 999 ? 3 : next_label > 99 ? 2 : next_label > 9 ? 1 : 0;
	int tl = this_label > 999999 ? 6 : this_label > 99999 ? 5 : this_label > 9999 ? 4 : this_label > 999 ? 3 : this_label > 99 ? 2 : this_label > 9 ? 1 : 0;

	return std::string(std::max(1, nl - tl + 1), ' ');
}

void Go::compact()
{
	// arrange so that adjacent spans have different targets
	uint i = 0;

	for (uint j = 1; j < nSpans; ++j)
	{
		if (span[j].to != span[i].to)
		{
			++i;
			span[i].to = span[j].to;
		}

		span[i].ub = span[j].ub;
	}

	nSpans = i + 1;
}

void Go::unmap(Go *base, const State *x)
{
	Span *s = span, *b = base->span, *e = &b[base->nSpans];
	uint lb = 0;
	s->ub = 0;
	s->to = NULL;

	for (; b != e; ++b)
	{
		if (b->to == x)
		{
			if ((s->ub - lb) > 1)
			{
				s->ub = b->ub;
			}
		}
		else
		{
			if (b->to != s->to)
			{
				if (s->ub)
				{
					lb = s->ub;
					++s;
				}

				s->to = b->to;
			}

			s->ub = b->ub;
		}
	}

	s->ub = e[ -1].ub;
	++s;
	nSpans = s - span;
}

void doGen(const Go *g, const State *s, uint *bm, uint f, uint m)
{
	Span *b = g->span, *e = &b[g->nSpans];
	uint lb = 0;

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

void prt(std::ostream& o, const Go *g, const State *s)
{
	Span *b = g->span, *e = &b[g->nSpans];
	uint lb = 0;

	for (; b < e; ++b)
	{
		if (b->to == s)
		{
			printSpan(o, lb, b->ub);
		}

		lb = b->ub;
	}
}

bool matches(const Go *g1, const State *s1, const Go *g2, const State *s2)
{
	Span *b1 = g1->span, *e1 = &b1[g1->nSpans];
	uint lb1 = 0;
	Span *b2 = g2->span, *e2 = &b2[g2->nSpans];
	uint lb2 = 0;

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
		if (matches(b->go, b->on, g, x))
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

void BitMap::gen(std::ostream &o, uint ind, uint lb, uint ub)
{
	if (first && bLastPass)
	{
		o << indent(ind) << "static const unsigned char " << mapCodeName["yybm"] << "[] = {";

		uint c = 1, n = ub - lb;
		const BitMap *cb = first;

		while((cb = cb->next) != NULL) {
			++c;
		}
		BitMap *b = first;

		uint *bm = new uint[n];
		
		for (uint i = 0, t = 1; b; i += n, t += 8)
		{
			memset(bm, 0, n * sizeof(uint));

			for (uint m = 0x80; b && m; m >>= 1)
			{
				b->i = i;
				b->m = m;
				doGen(b->go, b->on, bm, lb, m);
				b = const_cast<BitMap*>(b->next);
			}

			if (c > 8)
			{
				o << "\n" << indent(ind+1) << "/* table " << t << " .. " << std::min(c, t+7) << ": " << i << " */";
			}

			for (uint j = 0; j < n; ++j)
			{
				if (j % 8 == 0)
				{
					o << "\n" << indent(ind+1);
				}

				if (yybmHexTable)
				{
					prtHex(o, bm[j], false);
				}
				else
				{
					o << std::setw(3) << (uint)bm[j];
				}
				o  << ", ";
			}
		}

		o << "\n" << indent(ind) << "};\n";
		/* stats(); */
		
		delete[] bm;
	}
}

void BitMap::stats()
{
	uint n = 0;

	for (const BitMap *b = first; b; b = b->next)
	{
		prt(std::cerr, b->go, b->on);
		std::cerr << std::endl;
		++n;
	}

	std::cerr << n << " bitmaps\n";
	first = NULL;
}

void genGoTo(std::ostream &o, uint ind, const State *from, const State *to, bool & readCh)
{
	if (readCh && from->label + 1 != to->label)
	{
		o << indent(ind) << mapCodeName["yych"] << " = " << yychConversion << "*" << mapCodeName["YYCURSOR"] << ";\n";
		readCh = false;
	}

	o << indent(ind) << "goto " << labelPrefix << to->label << ";\n";
	vUsedLabels.insert(to->label);
}

void genIf(std::ostream &o, uint ind, const char *cmp, uint v, bool &readCh)
{
	o << indent(ind) << "if(";
	if (readCh)
	{
		o << "(" << mapCodeName["yych"] << " = " << yychConversion << "*" << mapCodeName["YYCURSOR"] << ")";
		readCh = false;
	}
	else
	{
		o << mapCodeName["yych"];
	}

	o << " " << cmp << " ";
	prtChOrHex(o, v);
	o << ") ";
}

static void need(std::ostream &o, uint ind, uint n, bool & readCh, bool bSetMarker)
{
	uint fillIndex = next_fill_index;

	if (fFlag)
	{
		next_fill_index++;
		o << indent(ind) << mapCodeName["YYSETSTATE"] << "(" << fillIndex << ");\n";
	}

	if (bUseYYFill)
	{
		if (n == 1)
		{
			o << indent(ind) << "if(" << mapCodeName["YYLIMIT"] << " == " << mapCodeName["YYCURSOR"] << ") " << mapCodeName["YYFILL"];
		}
		else
		{
			o << indent(ind) << "if((" << mapCodeName["YYLIMIT"] << " - " << mapCodeName["YYCURSOR"] << ") < " << n << ") " << mapCodeName["YYFILL"];
		}
		if (bUseYYFillParam)
		{
			o << "(" << n << ")";
		}
		o << ";\n";
	}

	if (fFlag)
	{
		o << mapCodeName["yyFillLabel"] << fillIndex << ":\n";
	}

	if (bSetMarker)
	{
		o << indent(ind) << mapCodeName["yych"] << " = " << yychConversion << "*(" << mapCodeName["YYMARKER"] << " = " << mapCodeName["YYCURSOR"] << ");\n";
	}
	else
	{
		o << indent(ind) << mapCodeName["yych"] << " = " << yychConversion << "*" << mapCodeName["YYCURSOR"] << ";\n";
	}
	readCh = false;
}

void Match::emit(std::ostream &o, uint ind, bool &readCh) const
{
	if (state->link)
	{
		o << indent(ind) << "++" << mapCodeName["YYCURSOR"] << ";\n";
	}
	else if (!readAhead())
	{
		/* do not read next char if match */
		o << indent(ind) << "++" << mapCodeName["YYCURSOR"] << ";\n";
		readCh = true;
	}
	else
	{
		o << indent(ind) << mapCodeName["yych"] << " = " << yychConversion << "*++" << mapCodeName["YYCURSOR"] << ";\n";
		readCh = false;
	}

	if (state->link)
	{
		need(o, ind, state->depth, readCh, false);
	}
}

void Enter::emit(std::ostream &o, uint ind, bool &readCh) const
{
	if (state->link)
	{
		o << indent(ind) << "++" << mapCodeName["YYCURSOR"] << ";\n";
		if (vUsedLabels.count(label))
		{
			o << labelPrefix << label << ":\n";
		}
		need(o, ind, state->depth, readCh, false);
	}
	else
	{
		/* we shouldn't need 'rule-following' protection here */
		o << indent(ind) << mapCodeName["yych"] << " = " << yychConversion << "*++" << mapCodeName["YYCURSOR"] << ";\n";
		if (vUsedLabels.count(label))
		{
			o << labelPrefix << label << ":\n";
		}
		readCh = false;
	}
}

void Initial::emit(std::ostream &o, uint ind, bool &readCh) const
{
	if (!startLabelName.empty())
	{
		o << startLabelName << ":\n";
	}

	if (vUsedLabels.count(1))
	{
		if (state->link)
		{
			o << indent(ind) << "++" << mapCodeName["YYCURSOR"] << ";\n";
		}
		else
		{
			o << indent(ind) << mapCodeName["yych"] << " = " << yychConversion << "*++" << mapCodeName["YYCURSOR"] << ";\n";
		}
	}

	if (vUsedLabels.count(label))
	{
		o << labelPrefix << label << ":\n";
	}
	else if (!label)
	{
		o << "\n";
	}

	if (dFlag)
	{
		o << indent(ind) << mapCodeName["YYDEBUG"] << "(" << label << ", *" << mapCodeName["YYCURSOR"] << ");\n";
	}

	if (state->link)
	{
		need(o, ind, state->depth, readCh, setMarker && bUsedYYMarker);
	}
	else
	{
		if (setMarker && bUsedYYMarker)
		{
			o << indent(ind) << mapCodeName["YYMARKER"] << " = " << mapCodeName["YYCURSOR"] << ";\n";
		}
		readCh = false;
	}
}

void Save::emit(std::ostream &o, uint ind, bool &readCh) const
{
	if (bUsedYYAccept)
	{
		o << indent(ind) << mapCodeName["yyaccept"] << " = " << selector << ";\n";
	}

	if (state->link)
	{
		if (bUsedYYMarker)
		{
			o << indent(ind) << mapCodeName["YYMARKER"] << " = ++" << mapCodeName["YYCURSOR"] << ";\n";
		}
		need(o, ind, state->depth, readCh, false);
	}
	else
	{
		if (bUsedYYMarker)
		{
			o << indent(ind) << mapCodeName["yych"] << " = " << yychConversion << "*(" << mapCodeName["YYMARKER"] << " = ++" << mapCodeName["YYCURSOR"] << ");\n";
		}
		else
		{
			o << indent(ind) << mapCodeName["yych"] << " = " << yychConversion << "*++" << mapCodeName["YYCURSOR"] << ";\n";
		}
		readCh = false;
	}
}

Move::Move(State *s) : Action(s)
{
	;
}

void Move::emit(std::ostream &, uint, bool &) const
{
	;
}

Accept::Accept(State *x, uint n, uint *s, State **r)
		: Action(x), nRules(n), saves(s), rules(r)
{
	;
}

void Accept::genRuleMap()
{
	for (uint i = 0; i < nRules; ++i)
	{
		if (saves[i] != ~0u)
		{
			mapRules[saves[i]] = rules[i];
		}
	}
}

void Accept::emitBinary(std::ostream &o, uint ind, uint l, uint r, bool &readCh) const
{
	if (l < r)
	{
		uint m = (l + r) >> 1;

		o << indent(ind) << "if(" << mapCodeName["yyaccept"] << " <= " << m << ") {\n";
		emitBinary(o, ++ind, l, m, readCh);
		o << indent(--ind) << "} else {\n";
		emitBinary(o, ++ind, m + 1, r, readCh);
		o << indent(--ind) << "}\n";
	}
	else
	{
		genGoTo(o, ind, state, mapRules.find(l)->second, readCh);
	}
}

void Accept::emit(std::ostream &o, uint ind, bool &readCh) const
{
	if (mapRules.size() > 0)
	{
		bUsedYYMarker = true;
		o << indent(ind) << mapCodeName["YYCURSOR"] << " = " << mapCodeName["YYMARKER"] << ";\n";

		if (readCh) // shouldn't be necessary, but might become at some point
		{
			o << indent(ind) << mapCodeName["yych"] << " = " << yychConversion << "*" << mapCodeName["YYCURSOR"] << ";\n";
			readCh = false;
		}

		if (mapRules.size() > 1)
		{
			bUsedYYAccept = true;

			if (gFlag && mapRules.size() >= cGotoThreshold)
			{
				o << indent(ind++) << "{\n";
				o << indent(ind++) << "static void *" << mapCodeName["yytarget"] << "[" << mapRules.size() << "] = {\n";
				for (RuleMap::const_iterator it = mapRules.begin(); it != mapRules.end(); ++it)
				{
					o << indent(ind) << "&&" << labelPrefix << it->second->label << ",\n";
					vUsedLabels.insert(it->second->label);
				}
				o << indent(--ind) << "};\n";
				o << indent(ind) << "goto *" << mapCodeName["yytarget"] << "[" << mapCodeName["yyaccept"] << "];\n";
				o << indent(--ind) << "}\n";
			}
			else if (sFlag)
			{
				emitBinary(o, ind, 0, mapRules.size() - 1, readCh);
			}
			else
			{
				o << indent(ind) << "switch(" << mapCodeName["yyaccept"] << ") {\n";
		
				for (RuleMap::const_iterator it = mapRules.begin(); it != mapRules.end(); ++it)
				{
					o << indent(ind) << "case " << it->first << ": \t";
					genGoTo(o, 0, state, it->second, readCh);
				}
			
				o << indent(ind) << "}\n";
			}
		}
		else
		{
			// no need to write if statement here since there is only case 0.
			genGoTo(o, ind, state, mapRules.find(0)->second, readCh);
		}
	}
}

Rule::Rule(State *s, RuleOp *r) : Action(s), rule(r)
{
	;
}

void Rule::emit(std::ostream &o, uint ind, bool &) const
{
	uint back = rule->ctx->fixedLength();

	if (back != 0u)
	{
		o << indent(ind) << mapCodeName["YYCURSOR"] << " = " << mapCodeName["YYCTXMARKER"] << ";\n";
	}

	RuleLine rl(*rule);

	o << file_info(sourceFileInfo, &rl);
	o << indent(ind);
	o << rule->code->text;
	o << "\n";
	o << outputFileInfo;
}

void doLinear(std::ostream &o, uint ind, Span *s, uint n, const State *from, const State *next, bool &readCh, uint mask)
{
	for (;;)
	{
		State *bg = s[0].to;

		while (n >= 3 && s[2].to == bg && (s[1].ub - s[0].ub) == 1)
		{
			if (s[1].to == next && n == 3)
			{
				if (!mask || (s[0].ub > 0x00FF))
				{
					genIf(o, ind, "!=", s[0].ub, readCh);
					genGoTo(o, 0, from, bg, readCh);
				}
				if (next->label != from->label + 1)
				{
					genGoTo(o, ind, from, next, readCh);
				}
				return ;
			}
			else
			{
				if (!mask || (s[0].ub > 0x00FF))
				{
					genIf(o, ind, "==", s[0].ub, readCh);
					genGoTo(o, 0, from, s[1].to, readCh);
				}
			}

			n -= 2;
			s += 2;
		}

		if (n == 1)
		{
			//	    	if(bg != next){
			if (s[0].to->label != from->label + 1)
			{
				genGoTo(o, ind, from, s[0].to, readCh);
			}
			//	    	}
			return ;
		}
		else if (n == 2 && bg == next)
		{
			if (!mask || (s[0].ub > 0x00FF))
			{
				genIf(o, ind, ">=", s[0].ub, readCh);
				genGoTo(o, 0, from, s[1].to, readCh);
			}
			if (next->label != from->label + 1)
			{
				genGoTo(o, ind, from, next, readCh);
			}
			return ;
		}
		else
		{
			if (!mask || ((s[0].ub - 1) > 0x00FF))
			{
				genIf(o, ind, "<=", s[0].ub - 1, readCh);
				genGoTo(o, 0, from, bg, readCh);
			}
			n -= 1;
			s += 1;
		}
	}

	if (next->label != from->label + 1)
	{
		genGoTo(o, ind, from, next, readCh);
	}
}

void Go::genLinear(std::ostream &o, uint ind, const State *from, const State *next, bool &readCh, uint mask) const
{
	doLinear(o, ind, span, nSpans, from, next, readCh, mask);
}

bool genCases(std::ostream &o, uint ind, uint lb, Span *s, bool &newLine, uint mask)
{
	bool used = false;

	if (!newLine)
	{
		o << "\n";
	}
	newLine = true;
	if (lb < s->ub)
	{
		for (;;)
		{
			if (!mask || lb > 0x00FF)
			{
				o << indent(ind) << "case ";
				prtChOrHex(o, lb);
				o << ":";
				newLine = false;
				used = true;
			}

			if (++lb == s->ub)
			{
				break;
			}

			o << "\n";
			newLine = true;
		}
	}
	return used;
}

void Go::genSwitch(std::ostream &o, uint ind, const State *from, const State *next, bool &readCh, uint mask) const
{
	bool newLine = true;

	if ((mask ? wSpans : nSpans) <= 2)
	{
		genLinear(o, ind, from, next, readCh, mask);
	}
	else
	{
		State *def = span[nSpans - 1].to;
		Span **sP = new Span * [nSpans - 1], **r, **s, **t;

		t = &sP[0];

		for (uint i = 0; i < nSpans; ++i)
		{
			if (span[i].to != def)
			{
				*(t++) = &span[i];
			}
		}

		if (dFlag)
		{
			o << indent(ind) << mapCodeName["YYDEBUG"] << "(-1, " << mapCodeName["yych"] << ");\n";
		}

		if (readCh)
		{
			o << indent(ind) << "switch((" << mapCodeName["yych"] << " = " << yychConversion << "*" << mapCodeName["YYCURSOR"] << ")) {\n";
			readCh = false;
		}
		else
		{
			o << indent(ind) << "switch(" << mapCodeName["yych"] << ") {\n";
		}

		while (t != &sP[0])
		{
			bool used = false;

			r = s = &sP[0];

			if (*s == &span[0])
			{
				used |= genCases(o, ind, 0, *s, newLine, mask);
			}
			else
			{
				used |= genCases(o, ind, (*s)[ -1].ub, *s, newLine, mask);
			}

			State *to = (*s)->to;

			while (++s < t)
			{
				if ((*s)->to == to)
				{
					used |= genCases(o, ind, (*s)[ -1].ub, *s, newLine, mask);
				}
				else
				{
					*(r++) = *s;
				}
			}

			if (used)
			{
				genGoTo(o, newLine ? ind+1 : 1, from, to, readCh);
				newLine = true;
			}
			t = r;
		}

		o << indent(ind) << "default:";
		genGoTo(o, 1, from, def, readCh);
		o << indent(ind) << "}\n";

		delete [] sP;
	}
}

void doBinary(std::ostream &o, uint ind, Span *s, uint n, const State *from, const State *next, bool &readCh, uint mask)
{
	if (n <= 4)
	{
		doLinear(o, ind, s, n, from, next, readCh, mask);
	}
	else
	{
		uint h = n / 2;

		genIf(o, ind, "<=", s[h - 1].ub - 1, readCh);
		o << "{\n";
		doBinary(o, ind+1, &s[0], h, from, next, readCh, mask);
		o << indent(ind) << "} else {\n";
		doBinary(o, ind+1, &s[h], n - h, from, next, readCh, mask);
		o << indent(ind) << "}\n";
	}
}

void Go::genBinary(std::ostream &o, uint ind, const State *from, const State *next, bool &readCh, uint mask) const
{
	if (mask)
	{
		Span * sc = new Span[wSpans];
		
		for (uint i = 0, j = 0; i < nSpans; i++)
		{
			if (span[i].ub > 0xFF)
			{
				sc[j++] = span[i];
			}
		}

		doBinary(o, ind, sc, wSpans, from, next, readCh, mask);

		delete[] sc;
	}
	else
	{
		doBinary(o, ind, span, nSpans, from, next, readCh, mask);
	}
}

void Go::genBase(std::ostream &o, uint ind, const State *from, const State *next, bool &readCh, uint mask) const
{
	if ((mask ? wSpans : nSpans) == 0)
	{
		return ;
	}

	if (!sFlag)
	{
		genSwitch(o, ind, from, next, readCh, mask);
		return ;
	}

	if ((mask ? wSpans : nSpans) > 8)
	{
		Span *bot = &span[0], *top = &span[nSpans - 1];
		uint util;

		if (bot[0].to == top[0].to)
		{
			util = (top[ -1].ub - bot[0].ub) / (nSpans - 2);
		}
		else
		{
			if (bot[0].ub > (top[0].ub - top[ -1].ub))
			{
				util = (top[0].ub - bot[0].ub) / (nSpans - 1);
			}
			else
			{
				util = top[ -1].ub / (nSpans - 1);
			}
		}

		if (util <= 2)
		{
			genSwitch(o, ind, from, next, readCh, mask);
			return ;
		}
	}

	if ((mask ? wSpans : nSpans) > 5)
	{
		genBinary(o, ind, from, next, readCh, mask);
	}
	else
	{
		genLinear(o, ind, from, next, readCh, mask);
	}
}

void Go::genCpGoto(std::ostream &o, uint ind, const State *from, const State *next, bool &readCh) const
{
	std::string sYych;
	
	if (readCh)
	{
		sYych = "(" + mapCodeName["yych"] + " = " + yychConversion + "*" + mapCodeName["YYCURSOR"] + ")";
	}
	else
	{
		sYych = mapCodeName["yych"];
	}

	readCh = false;
	if (wFlag)
	{
		o << indent(ind) << "if(" << sYych <<" & ~0xFF) {\n";
		genBase(o, ind+1, from, next, readCh, 1);
		o << indent(ind++) << "} else {\n";
		sYych = mapCodeName["yych"];
	}
	else
	{
		o << indent(ind++) << "{\n";
	}
	o << indent(ind++) << "static void *" << mapCodeName["yytarget"] << "[256] = {\n";
	o << indent(ind);

	uint ch = 0;
	for (uint i = 0; i < lSpans; ++i)
	{
		vUsedLabels.insert(span[i].to->label);
		for(; ch < span[i].ub; ++ch)
		{
			o << "&&" << labelPrefix << span[i].to->label;
			if (ch == 255)
			{
				o << "\n";
				i = lSpans;
				break;
			}
			else if (ch % 8 == 7)
			{
				o << ",\n" << indent(ind);
			}
			else
			{
				o << "," << space(span[i].to->label);
			}
		}
	}
	o << indent(--ind) << "};\n";
	o << indent(ind) << "goto *" << mapCodeName["yytarget"] << "[" << sYych << "];\n";
	o << indent(--ind) << "}\n";
}

void Go::genGoto(std::ostream &o, uint ind, const State *from, const State *next, bool &readCh)
{
	if ((gFlag || wFlag) && wSpans == ~0u)
	{
		uint nBitmaps = 0;
		std::set<uint> vTargets;
		wSpans = 0;
		lSpans = 1;
		dSpans = 0;
		for (uint i = 0; i < nSpans; ++i)
		{
			if (span[i].ub > 0xFF)
			{
				wSpans++;
			}
			if (span[i].ub < 0x100 || !wFlag)
			{
				lSpans++;

				State *to = span[i].to;
	
				if (to && to->isBase)
				{
					const BitMap *b = BitMap::find(to);
	
					if (b && matches(b->go, b->on, this, to))
					{
						nBitmaps++;
					}
					else
					{
						dSpans++;
						vTargets.insert(to->label);
					}
				}
				else
				{
					dSpans++;
					vTargets.insert(to->label);
				}
			}
		}
		lTargets = vTargets.size() >> nBitmaps;
	}

	if (gFlag && (lTargets >= cGotoThreshold || dSpans >= cGotoThreshold))
	{
		genCpGoto(o, ind, from, next, readCh);
		return;
	}
	else if (bFlag)
	{
		for (uint i = 0; i < nSpans; ++i)
		{
			State *to = span[i].to;

			if (to && to->isBase)
			{
				const BitMap *b = BitMap::find(to);
				std::string sYych;

				if (b && matches(b->go, b->on, this, to))
				{
					Go go;
					go.span = new Span[nSpans];
					go.unmap(this, to);
					if (readCh)
					{
						sYych = "(" + mapCodeName["yych"] + " = " + yychConversion + "*" + mapCodeName["YYCURSOR"] + ")";
					}
					else
					{
						sYych = mapCodeName["yych"];
					}
					readCh = false;
					if (wFlag)
					{
						o << indent(ind) << "if(" << sYych << " & ~0xFF) {\n";
						sYych = mapCodeName["yych"];
						genBase(o, ind+1, from, next, readCh, 1);
						o << indent(ind) << "} else ";
					}
					else
					{
						o << indent(ind);
					}
					o << "if(" << mapCodeName["yybm"] << "[" << b->i << "+" << sYych << "] & ";
					if (yybmHexTable)
					{
						prtHex(o, b->m, false);
					}
					else
					{
						o << (uint) b->m;
					}
					o << ") {\n";
					genGoTo(o, ind+1, from, to, readCh);
					o << indent(ind) << "}\n";
					go.genBase(o, ind, from, next, readCh, 0);
					delete [] go.span;
					return ;
				}
			}
		}
	}

	genBase(o, ind, from, next, readCh, 0);
}

void State::emit(std::ostream &o, uint ind, bool &readCh) const
{
	if (vUsedLabels.count(label))
	{
		o << labelPrefix << label << ":\n";
	}
	if (dFlag && !action->isInitial())
	{
		o << indent(ind) << mapCodeName["YYDEBUG"] << "(" << label << ", *" << mapCodeName["YYCURSOR"] << ");\n";
	}
	if (isPreCtxt)
	{
		o << indent(ind) << mapCodeName["YYCTXMARKER"] << " = " << mapCodeName["YYCURSOR"] << " + 1;\n";
	}
	action->emit(o, ind, readCh);
}

uint merge(Span *x0, State *fg, State *bg)
{
	Span *x = x0, *f = fg->go.span, *b = bg->go.span;
	uint nf = fg->go.nSpans, nb = bg->go.nSpans;
	State *prev = NULL, *to;
	// NB: we assume both spans are for same range

	for (;;)
	{
		if (f->ub == b->ub)
		{
			to = f->to == b->to ? bg : f->to;

			if (to == prev)
			{
				--x;
			}
			else
			{
				x->to = prev = to;
			}

			x->ub = f->ub;
			++x;
			++f;
			--nf;
			++b;
			--nb;

			if (nf == 0 && nb == 0)
			{
				return x - x0;
			}
		}

		while (f->ub < b->ub)
		{
			to = f->to == b->to ? bg : f->to;

			if (to == prev)
			{
				--x;
			}
			else
			{
				x->to = prev = to;
			}

			x->ub = f->ub;
			++x;
			++f;
			--nf;
		}

		while (b->ub < f->ub)
		{
			to = b->to == f->to ? bg : f->to;

			if (to == prev)
			{
				--x;
			}
			else
			{
				x->to = prev = to;
			}

			x->ub = b->ub;
			++x;
			++b;
			--nb;
		}
	}
}

const uint cInfinity = ~0;

class SCC
{

public:
	State	**top, **stk;

public:
	SCC(uint);
	~SCC();
	void traverse(State*);

#ifdef PEDANTIC
private:
	SCC(const SCC& oth)
		: top(oth.top)
		, stk(oth.stk)
	{
	}
	SCC& operator = (const SCC& oth)
	{
		new(this) SCC(oth);
		return *this;
	}
#endif
};

SCC::SCC(uint size)
	: top(new State * [size])
	, stk(top)
{
}

SCC::~SCC()
{
	delete [] stk;
}

void SCC::traverse(State *x)
{
	*top = x;
	uint k = ++top - stk;
	x->depth = k;

	for (uint i = 0; i < x->go.nSpans; ++i)
	{
		State *y = x->go.span[i].to;

		if (y)
		{
			if (y->depth == 0)
			{
				traverse(y);
			}

			if (y->depth < x->depth)
			{
				x->depth = y->depth;
			}
		}
	}

	if (x->depth == k)
	{
		do
		{
			(*--top)->depth = cInfinity;
			(*top)->link = x;
		}
		while (*top != x);
	}
}

static bool state_is_in_non_trivial_SCC(const State* s)
{
	
	// does not link to self
	if (s->link != s)
	{
		return true;
	}
	
	// or exists i: (s->go.spans[i].to->link == s)
	//
	// Note: (s->go.spans[i].to == s) is allowed, corresponds to s
	// looping back to itself.
	//
	for (uint i = 0; i < s->go.nSpans; ++i)
	{
		const State* t = s->go.span[i].to;
	
		if (t && t->link == s)
		{
			return true;
		}
	}
	// otherwise no
	return false;
}

uint maxDist(State *s)
{
	if (s->depth != cInfinity)
	{
		// Already calculated, just return result.
    	return s->depth;
	}
	uint mm = 0;

	for (uint i = 0; i < s->go.nSpans; ++i)
	{
		State *t = s->go.span[i].to;

		if (t)
		{
			uint m = 1;

			if (!t->link) // marked as non-key state
			{
				if (t->depth == cInfinity)
				{
					t->depth = maxDist(t);
				}
				m += t->depth;
			}

			if (m > mm)
			{
				mm = m;
			}
		}
	}

	s->depth = mm;
	return mm;
}

void calcDepth(State *head)
{
	State* s;

	// mark non-key states by s->link = NULL ;
	for (s = head; s; s = s->next)
	{
		if (s != head && !state_is_in_non_trivial_SCC(s))
		{
			s->link = NULL;
		}
		//else: key state, leave alone
	}
	
	for (s = head; s; s = s->next)
	{
		s->depth = cInfinity;
	}

	// calculate max number of transitions before guarantied to reach
	// a key state.
	for (s = head; s; s = s->next)
	{
		maxDist(s);
	}
}

void DFA::findSCCs()
{
	SCC scc(nStates);
	State *s;

	for (s = head; s; s = s->next)
	{
		s->depth = 0;
		s->link = NULL;
	}

	for (s = head; s; s = s->next)
	{
		if (!s->depth)
		{
			scc.traverse(s);
		}
	}

	calcDepth(head);
}

void DFA::split(State *s)
{
	State *move = new State;
	(void) new Move(move);
	addState(&s->next, move);
	move->link = s->link;
	move->rule = s->rule;
	move->go = s->go;
	s->rule = NULL;
	s->go.nSpans = 1;
	s->go.span = new Span[1];
	s->go.span[0].ub = ubChar;
	s->go.span[0].to = move;
}

void DFA::findBaseState()
{
	Span *span = new Span[ubChar - lbChar];

	for (State *s = head; s; s = s->next)
	{
		if (!s->link)
		{
			for (uint i = 0; i < s->go.nSpans; ++i)
			{
				State *to = s->go.span[i].to;

				if (to && to->isBase)
				{
					to = to->go.span[0].to;
					uint nSpans = merge(span, s, to);

					if (nSpans < s->go.nSpans)
					{
						delete [] s->go.span;
						s->go.nSpans = nSpans;
						s->go.span = new Span[nSpans];
						memcpy(s->go.span, span, nSpans*sizeof(Span));
					}

					break;
				}
			}
		}
	}

	delete [] span;
}

void DFA::emit(std::ostream &o, uint ind)
{
	State *s;
	uint i, bitmap_brace = 0;

	findSCCs();
	head->link = head;

	uint nRules = 0;

	for (s = head; s; s = s->next)
	{
		s->depth = maxDist(s);
		if (maxFill < s->depth)
		{
			maxFill = s->depth;
		}
		if (s->rule && s->rule->accept >= nRules)
		{
			nRules = s->rule->accept + 1;
		}
	}

	uint nSaves = 0;
	uint *saves = new uint[nRules];
	memset(saves, ~0, (nRules)*sizeof(*saves));

	// mark backtracking points
	bool bSaveOnHead = false;

	for (s = head; s; s = s->next)
	{
		if (s->rule)
		{
			for (i = 0; i < s->go.nSpans; ++i)
			{
				if (s->go.span[i].to && !s->go.span[i].to->rule)
				{
					delete s->action;
					s->action = NULL;

					if (saves[s->rule->accept] == ~0u)
					{
						saves[s->rule->accept] = nSaves++;
					}

					bSaveOnHead |= s == head;
					(void) new Save(s, saves[s->rule->accept]); // sets s->action
				}
			}
		}
	}

	// insert actions
	State **rules = new State * [nRules];

	memset(rules, 0, (nRules)*sizeof(*rules));

	State *accept = NULL;
	Accept *accfixup = NULL;

	for (s = head; s; s = s->next)
	{
		State * ow;

		if (!s->rule)
		{
			ow = accept;
		}
		else
		{
			if (!rules[s->rule->accept])
			{
				State *n = new State;
				(void) new Rule(n, s->rule);
				rules[s->rule->accept] = n;
				addState(&s->next, n);
			}

			ow = rules[s->rule->accept];
		}

		for (i = 0; i < s->go.nSpans; ++i)
		{
			if (!s->go.span[i].to)
			{
				if (!ow)
				{
					ow = accept = new State;
					accfixup = new Accept(accept, nRules, saves, rules);
					addState(&s->next, accept);
				}

				s->go.span[i].to = ow;
			}
		}
	}
	
	if (accfixup)
	{
		accfixup->genRuleMap();
	}

	// split ``base'' states into two parts
	for (s = head; s; s = s->next)
	{
		s->isBase = false;

		if (s->link)
		{
			for (i = 0; i < s->go.nSpans; ++i)
			{
				if (s->go.span[i].to == s)
				{
					s->isBase = true;
					split(s);

					if (bFlag)
					{
						BitMap::find(&s->next->go, s);
					}

					s = s->next;
					break;
				}
			}
		}
	}

	// find ``base'' state, if possible
	findBaseState();

	delete head->action;
	head->action = NULL;

	if (bFlag)
	{
		o << indent(ind++) << "{\n";
		bitmap_brace = 1;
		BitMap::gen(o, ind, lbChar, ubChar <= 256 ? ubChar : 256);
	}

	bUsedYYAccept = false;
	
	uint start_label = next_label;

	(void) new Initial(head, next_label++, bSaveOnHead);

	if (bUseStartLabel)
	{
		if (startLabelName.empty())
		{
			vUsedLabels.insert(start_label);
		}
	}

	for (s = head; s; s = s->next)
	{
		s->label = next_label++;
	}

	// Save 'next_fill_index' and compute information about code generation
	// while writing to null device.
	uint save_fill_index = next_fill_index;
	null_stream  null_dev;

	for (s = head; s; s = s->next)
	{
		bool readCh = false;
		s->emit(null_dev, ind, readCh);
		s->go.genGoto(null_dev, ind, s, s->next, readCh);
	}
	if (last_fill_index < next_fill_index)
	{
		last_fill_index = next_fill_index;
	}
	next_fill_index = save_fill_index;

	// Generate prolog
	o << "\n" << outputFileInfo;
	o << indent(ind++) << "{\n";

	if (!fFlag)
	{
		o << indent(ind) << mapCodeName["YYCTYPE"] << " " << mapCodeName["yych"] << ";\n";
		if (bUsedYYAccept)
		{
			o << indent(ind) << "unsigned int "<< mapCodeName["yyaccept"] << " = 0;\n";
		}
	}
	else
	{
		o << "\n";
	}

	genGetState(o, ind, start_label);

	if (vUsedLabels.count(1))
	{
		vUsedLabels.insert(0);
		o << indent(ind) << "goto " << labelPrefix << "0;\n";
	}

	// Generate code
	for (s = head; s; s = s->next)
	{
		bool readCh = false;
		s->emit(o, ind, readCh);
		s->go.genGoto(o, ind, s, s->next, readCh);
	}

	// Generate epilog
	o << indent(--ind) << "}\n";
	if (bitmap_brace)
	{
		o << indent(--ind) << "}\n";
	}

	// Cleanup
	if (BitMap::first)
	{
		delete BitMap::first;
		BitMap::first = NULL;
	}

	delete [] saves;
	delete [] rules;

	bUseStartLabel = false;
}

void genGetState(std::ostream &o, uint& ind, uint start_label)
{
	if (fFlag && !bWroteGetState)
	{
		vUsedLabels.insert(start_label);
		o << indent(ind) << "switch(" << mapCodeName["YYGETSTATE"] << "()) {\n";
		if (bUseStateAbort)
		{
			o << indent(ind) << "default: abort();\n";
			o << indent(ind) << "case -1: goto " << labelPrefix << start_label << ";\n";
		}
		else
		{
			o << indent(ind) << "default: goto " << labelPrefix << start_label << ";\n";
		}

		for (size_t i=0; i<last_fill_index; ++i)
		{
			o << indent(ind) << "case " << i << ": goto " << mapCodeName["yyFillLabel"] << i << ";\n";
		}

		o << indent(ind) << "}\n";
		if (bUseStateNext)
		{
			o << mapCodeName["yyNext"] << ":\n";
		}
		bWroteGetState = true;
	}
}

std::ostream& operator << (std::ostream& o, const file_info& li)
{
	if (li.ln && !iFlag)
	{
		o << "#line " << li.ln->get_line() << " \"" << li.fname << "\"\n";
	}
	return o;
}

uint Scanner::get_line() const
{
	return cline;
}

void Scanner::config(const Str& cfg, int num)
{
	if (cfg.to_string() == "indent:top")
	{
		if (num < 0)
		{
			fatal("configuration 'indent:top' must be a positive integer");
		}
		topIndent = num;
	}
	else if (cfg.to_string() == "yybm:hex")
	{
		yybmHexTable = num != 0;
	}
	else if (cfg.to_string() == "startlabel")
	{
		bUseStartLabel = num != 0;
		startLabelName = "";
	}
	else if (cfg.to_string() == "state:abort")
	{
		bUseStateAbort = num != 0;
	}
	else if (cfg.to_string() == "state:nextlabel")
	{
		bUseStateNext = num != 0;
	}
	else if (cfg.to_string() == "yyfill:enable")
	{
		bUseYYFill = num != 0;
	}
	else if (cfg.to_string() == "yyfill:parameter")
	{
		bUseYYFillParam = num != 0;
	}
	else if (cfg.to_string() == "cgoto:threshold")
	{
		cGotoThreshold = num;
	}
	else if (cfg.to_string() == "yych:conversion")
	{
		if (num)
		{
			yychConversion  = "(";
			yychConversion += mapCodeName["YYCTYPE"];
			yychConversion += ")";
		}
		else
		{
			yychConversion  = "";
		}
	}
	else
	{
		fatal("unrecognized configuration name or illegal integer value");
	}
}

static std::set<std::string> mapVariableKeys;
static std::set<std::string> mapDefineKeys;
static std::set<std::string> mapLabelKeys;

void Scanner::config(const Str& cfg, const Str& val)
{
	if (mapDefineKeys.empty())
	{
		mapVariableKeys.insert("variable:yyaccept");
		mapVariableKeys.insert("variable:yybm");
		mapVariableKeys.insert("variable:yych");
		mapVariableKeys.insert("variable:yytarget");
		mapDefineKeys.insert("define:YYCTXMARKER");
		mapDefineKeys.insert("define:YYCTYPE");
		mapDefineKeys.insert("define:YYCURSOR");
		mapDefineKeys.insert("define:YYDEBUG");
		mapDefineKeys.insert("define:YYFILL");
		mapDefineKeys.insert("define:YYGETSTATE");
		mapDefineKeys.insert("define:YYLIMIT");
		mapDefineKeys.insert("define:YYMARKER");
		mapDefineKeys.insert("define:YYSETSTATE");
		mapLabelKeys.insert("label:yyFillLabel");
		mapLabelKeys.insert("label:yyNext");
	}

	std::string strVal;

	if (val.len >= 2 && val.str[0] == val.str[val.len-1] 
	&& (val.str[0] == '"' || val.str[0] == '\''))
	{
		SubStr tmp(val.str + 1, val.len - 2);
		unescape(tmp, strVal);
	}
	else
	{
		strVal = val.to_string();
	}

	if (cfg.to_string() == "indent:string")
	{
		indString = strVal;
	}
	else if (cfg.to_string() == "startlabel")
	{
		startLabelName = strVal;
		bUseStartLabel = !startLabelName.empty();
	}
	else if (cfg.to_string() == "labelprefix")
	{
		labelPrefix = strVal;
	}
	else if (mapVariableKeys.find(cfg.to_string()) != mapVariableKeys.end())
    {
    	if (bFirstPass && !mapCodeName.insert(
    			std::make_pair(cfg.to_string().substr(sizeof("variable:") - 1), strVal)
    			).second)
    	{
			fatal("variable already being used and cannot be changed");
    	}
    }
	else if (mapDefineKeys.find(cfg.to_string()) != mapDefineKeys.end())
    {
    	if (bFirstPass && !mapCodeName.insert(
    			std::make_pair(cfg.to_string().substr(sizeof("define:") - 1), strVal)
    			).second)
    	{
 			fatal("define already being used and cannot be changed");
 		}
    }
	else if (mapLabelKeys.find(cfg.to_string()) != mapLabelKeys.end())
    {
    	if (bFirstPass && !mapCodeName.insert(
    			std::make_pair(cfg.to_string().substr(sizeof("label:") - 1), strVal)
    			).second)
    	{
			fatal("label already being used and cannot be changed");
    	}
    }
	else
	{
		fatal("unrecognized configuration name or illegal string value");
	}
}

} // end namespace re2c

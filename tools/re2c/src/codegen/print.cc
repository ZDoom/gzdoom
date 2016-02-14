#include <iostream>

#include "src/codegen/print.h"
#include "src/conf/opt.h"
#include "src/globals.h"
#include "src/ir/regexp/encoding/enc.h"

namespace re2c
{

bool is_print (uint32_t c)
{
	return c >= 0x20 && c < 0x7F;
}

bool is_space (uint32_t c)
{
	switch (c)
	{
		case '\t':
		case '\f':
		case '\v':
		case '\n':
		case '\r':
		case ' ':
			return true;
		default:
			return false;
	}
}

char hexCh(uint32_t c)
{
	static const char * sHex = "0123456789ABCDEF";
	return sHex[c & 0x0F];
}

void prtChOrHex(std::ostream& o, uint32_t c)
{
	if (opts->encoding.type () != Enc::EBCDIC
		&& (is_print (c) || is_space (c)))
	{
		o << '\'';
		prtCh(o, c);
		o << '\'';
	}
	else
	{
		prtHex(o, c);
	}
}

void prtHex(std::ostream& o, uint32_t c)
{
	o << "0x";
	const uint32_t cunit_size = opts->encoding.szCodeUnit ();
	if (cunit_size >= 4)
	{
		o << hexCh (c >> 28u)
			<< hexCh (c >> 24u)
			<< hexCh (c >> 20u)
			<< hexCh (c >> 16u);
	}
	if (cunit_size >= 2)
	{
		o << hexCh (c >> 12u)
			<< hexCh (c >> 8u);
	}
	o << hexCh (c >> 4u)
		<< hexCh (c);
}

void prtCh(std::ostream& o, uint32_t c)
{
	const bool dot = opts->target == opt_t::DOT;

	switch (c)
	{
		case '\'':
		o << (dot ? "'" : "\\'");
		break;

		case '"':
		o << (dot ? "\\\"" : "\"");
		break;

		case '\n':
		o << (dot ? "\\\\n" : "\\n");
		break;

		case '\t':
		o << (dot ? "\\\\t" : "\\t");
		break;

		case '\v':
		o << (dot ? "\\\\v" : "\\v");
		break;

		case '\b':
		o << (dot ? "\\\\b" : "\\b");
		break;

		case '\r':
		o << (dot ? "\\\\r" : "\\r");
		break;

		case '\f':
		o << (dot ? "\\\\f" : "\\f");
		break;

		case '\a':
		o << (dot ? "\\\\a" :"\\a");
		break;

		case '\\':
		o << "\\\\"; // both .dot and C/C++ code expect "\\"
		break;

		default:
		o << static_cast<char> (c);
		break;
	}
}

void prtChOrHexForSpan(std::ostream& o, uint32_t c)
{
	if (opts->encoding.type () != Enc::EBCDIC
		&& is_print (c)
		&& (c != ']'))
	{
		prtCh(o, c);
	}
	else
	{
		prtHex(o, c);
	}
}

void printSpan(std::ostream& o, uint32_t lb, uint32_t ub)
{
	o << "[";
	if ((ub - lb) == 1)
	{
		prtChOrHexForSpan(o, lb);
	}
	else
	{
		prtChOrHexForSpan(o, lb);
		o << "-";
		prtChOrHexForSpan(o, ub - 1);
	}
	o << "]";
}

} // end namespace re2c


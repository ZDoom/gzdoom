#include <ostream>

#include "src/codegen/label.h"

namespace re2c {

const uint32_t label_t::FIRST = 0;

label_t::label_t ()
	: value (FIRST)
{}

void label_t::inc ()
{
	++value;
}

label_t label_t::first ()
{
	return label_t ();
}

bool label_t::operator < (const label_t & l) const
{
	return value < l.value;
}

uint32_t label_t::width () const
{
	uint32_t v = value;
	uint32_t n = 0;
	while (v /= 10) ++n;
	return n;
}

std::ostream & operator << (std::ostream & o, label_t l)
{
	o << l.value;
	return o;
}

} // namespace re2c

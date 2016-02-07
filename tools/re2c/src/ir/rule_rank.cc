#include <assert.h>
#include <limits>
#include <ostream>

#include "src/ir/rule_rank.h"

namespace re2c
{

const uint32_t rule_rank_t::NONE = std::numeric_limits<uint32_t>::max();
const uint32_t rule_rank_t::DEF = rule_rank_t::NONE - 1;

rule_rank_t::rule_rank_t ()
	: value (0)
{}

void rule_rank_t::inc ()
{
	assert (value < DEF - 1);
	++value;
}

rule_rank_t rule_rank_t::none ()
{
	rule_rank_t r;
	r.value = NONE;
	return r;
}

rule_rank_t rule_rank_t::def ()
{
	rule_rank_t r;
	r.value = DEF;
	return r;
}

bool rule_rank_t::is_none () const
{
	return value == NONE;
}

bool rule_rank_t::is_def () const
{
	return value == DEF;
}

bool rule_rank_t::operator < (const rule_rank_t & r) const
{
	return value < r.value;
}

bool rule_rank_t::operator == (const rule_rank_t & r) const
{
	return value == r.value;
}

std::ostream & operator << (std::ostream & o, rule_rank_t r)
{
	o << r.value;
	return o;
}

uint32_t rule_rank_t::uint32 () const
{
	return value;
}

} // namespace re2c

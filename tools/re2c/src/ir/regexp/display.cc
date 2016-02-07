#include <iostream>

#include "src/ir/regexp/regexp.h"
#include "src/ir/regexp/regexp_alt.h"
#include "src/ir/regexp/regexp_cat.h"
#include "src/ir/regexp/regexp_close.h"
#include "src/ir/regexp/regexp_match.h"
#include "src/ir/regexp/regexp_null.h"
#include "src/ir/regexp/regexp_rule.h"

namespace re2c
{

std::ostream & operator << (std::ostream & o, const RegExp & re)
{
	re.display (o);
	return o;
}

void AltOp::display (std::ostream & o) const
{
	o << exp1 << "|" << exp2;
}

void CatOp::display (std::ostream & o) const
{
	o << exp1 << exp2;
}

void CloseOp::display (std::ostream & o) const
{
	o << exp << "+";
}

void MatchOp::display (std::ostream & o) const
{
	o << match;
}

void NullOp::display (std::ostream & o) const
{
	o << "_";
}

void RuleOp::display (std::ostream & o) const
{
	o << exp << "/" << ctx << ";";
}

} // end namespace re2c


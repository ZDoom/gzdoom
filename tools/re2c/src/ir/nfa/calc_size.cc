#include "src/util/c99_stdint.h"

#include "src/ir/regexp/regexp.h"
#include "src/ir/regexp/regexp_alt.h"
#include "src/ir/regexp/regexp_cat.h"
#include "src/ir/regexp/regexp_close.h"
#include "src/ir/regexp/regexp_match.h"
#include "src/ir/regexp/regexp_null.h"
#include "src/ir/regexp/regexp_rule.h"

namespace re2c
{

uint32_t AltOp::calc_size() const
{
	return exp1->calc_size()
		+ exp2->calc_size()
		+ 1;
}

uint32_t CatOp::calc_size() const
{
	return exp1->calc_size()
		+ exp2->calc_size();
}

uint32_t CloseOp::calc_size() const
{
	return exp->calc_size() + 1;
}

uint32_t MatchOp::calc_size() const
{
	return 1;
}

uint32_t NullOp::calc_size() const
{
	return 0;
}

uint32_t RuleOp::calc_size() const
{
	const uint32_t n = ctx->calc_size();
	return exp->calc_size()
		+ (n > 0 ? n + 1 : 0)
		+ 1;
}

} // end namespace re2c

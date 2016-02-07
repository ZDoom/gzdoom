#ifndef _RE2C_IR_DFA_DFA_
#define _RE2C_IR_DFA_DFA_

#include "src/util/c99_stdint.h"
#include <vector>

#include "src/ir/regexp/regexp.h"
#include "src/parse/rules.h"
#include "src/util/forbid_copy.h"

namespace re2c
{

struct nfa_t;
class RuleOp;

struct dfa_state_t
{
	size_t *arcs;
	RuleOp *rule;
	bool ctx;

	dfa_state_t()
		: arcs(NULL)
		, rule(NULL)
		, ctx(false)
	{}
	~dfa_state_t()
	{
		delete[] arcs;
	}

	FORBID_COPY(dfa_state_t);
};

struct dfa_t
{
	static const size_t NIL;

	std::vector<dfa_state_t*> states;
	const size_t nchars;

	dfa_t(const nfa_t &nfa, const charset_t &charset, rules_t &rules);
	~dfa_t();
};

enum dfa_minimization_t
{
	DFA_MINIMIZATION_TABLE,
	DFA_MINIMIZATION_MOORE
};

void minimization(dfa_t &dfa);
void fillpoints(const dfa_t &dfa, std::vector<size_t> &fill);

} // namespace re2c

#endif // _RE2C_IR_DFA_DFA_

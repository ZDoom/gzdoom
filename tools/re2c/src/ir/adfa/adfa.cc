#include <assert.h>
#include <queue>
#include <set>
#include <vector>
#include <utility>

#include "src/codegen/go.h"
#include "src/ir/adfa/adfa.h"
#include "src/ir/dfa/dfa.h"
#include "src/ir/skeleton/skeleton.h"
#include "src/util/allocate.h"

namespace re2c
{

DFA::DFA
	( const dfa_t &dfa
	, const std::vector<size_t> &fill
	, Skeleton *skel
	, const charset_t &charset
	, const std::string &n
	, const std::string &c
	, uint32_t l
	)
	: accepts ()
	, skeleton (skel)
	, name (n)
	, cond (c)
	, line (l)
	, lbChar(0)
	, ubChar(charset.back())
	, nStates(0)
	, head(NULL)

	// statistics
	, max_fill (0)
	, need_backup (false)
	, need_backupctx (false)
	, need_accept (false)
{
	const size_t nstates = dfa.states.size();
	const size_t nchars = dfa.nchars;

	State **i2s = new State*[nstates];
	for (size_t i = 0; i < nstates; ++i)
	{
		i2s[i] = new State;
	}

	State **p = &head;
	for (size_t i = 0; i < nstates; ++i)
	{
		dfa_state_t *t = dfa.states[i];
		State *s = i2s[i];

		++nStates;
		*p = s;
		p = &s->next;

		s->isPreCtxt = t->ctx;
		s->rule = t->rule;
		s->fill = fill[i];
		s->go.span = allocate<Span>(nchars);
		uint32_t j = 0;
		for (uint32_t c = 0; c < nchars; ++j)
		{
			const size_t to = t->arcs[c];
			for (;++c < nchars && t->arcs[c] == to;);
			s->go.span[j].to = to == dfa_t::NIL ? NULL : i2s[to];
			s->go.span[j].ub = charset[c];
		}
		s->go.nSpans = j;
	}
	*p = NULL;

	delete[] i2s;
}

DFA::~DFA()
{
	State *s;

	while ((s = head))
	{
		head = s->next;
		delete s;
	}

	delete skeleton;
}

void DFA::reorder()
{
	std::vector<State*> ord;
	ord.reserve(nStates);

	std::queue<State*> todo;
	todo.push(head);

	std::set<State*> done;
	done.insert(head);

	for(;!todo.empty();)
	{
		State *s = todo.front();
		todo.pop();
		ord.push_back(s);
		for(uint32_t i = 0; i < s->go.nSpans; ++i)
		{
			State *q = s->go.span[i].to;
			if(q && done.insert(q).second)
			{
				todo.push(q);
			}
		}
	}

	assert(nStates == ord.size());

	ord.push_back(NULL);
	for(uint32_t i = 0; i < nStates; ++i)
	{
		ord[i]->next = ord[i + 1];
	}
}

void DFA::addState(State *s, State *next)
{
	++nStates;
	s->next = next->next;
	next->next = s;
}

} // namespace re2c


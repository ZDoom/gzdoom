#ifndef _RE2C_IR_ADFA_ADFA_
#define _RE2C_IR_ADFA_ADFA_

#include <stddef.h>
#include "src/util/c99_stdint.h"
#include <set>
#include <string>

#include "src/codegen/go.h"
#include "src/codegen/label.h"
#include "src/ir/adfa/action.h"
#include "src/ir/regexp/regexp.h"
#include "src/util/forbid_copy.h"

namespace re2c
{

struct Skeleton;
struct Output;
struct OutputFile;
struct dfa_t;

struct State
{
	label_t label;
	RuleOp * rule;
	State * next;
	size_t fill;

	bool isPreCtxt;
	bool isBase;
	Go go;
	Action action;

	State ()
		: label (label_t::first ())
		, rule (NULL)
		, next (0)
		, fill (0)
		, isPreCtxt (false)
		, isBase (false)
		, go ()
		, action ()
	{}
	~State ()
	{
		operator delete (go.span);
	}

	FORBID_COPY (State);
};

class DFA
{
	accept_t accepts;
	Skeleton * skeleton;

public:
	const std::string name;
	const std::string cond;
	const uint32_t line;

	uint32_t lbChar;
	uint32_t ubChar;
	uint32_t nStates;
	State * head;

	// statistics
	size_t max_fill;
	bool need_backup;
	bool need_backupctx;
	bool need_accept;

public:
	DFA	( const dfa_t &dfa
		, const std::vector<size_t> &fill
		, Skeleton *skel
		, const charset_t &charset
		, const std::string &n
		, const std::string &c
		, uint32_t l
		);
	~DFA ();
	void reorder();
	void prepare();
	void calc_stats();
	void emit (Output &, uint32_t &, bool, bool &);

private:
	void addState(State*, State *);
	void split (State *);
	void findBaseState ();
	void count_used_labels (std::set<label_t> & used, label_t prolog, label_t start, bool force_start) const;
	void emit_body (OutputFile &, uint32_t &, const std::set<label_t> & used_labels, label_t initial) const;

	FORBID_COPY (DFA);
};

} // namespace re2c

#endif // _RE2C_IR_ADFA_ADFA_

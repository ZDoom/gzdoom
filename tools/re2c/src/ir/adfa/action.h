#ifndef _RE2C_IR_ADFA_ACTION_
#define _RE2C_IR_ADFA_ACTION_

#include <vector>

#include "src/codegen/label.h"
#include "src/util/c99_stdint.h"
#include "src/util/uniq_vector.h"

namespace re2c
{

struct OutputFile;
class RuleOp;
struct State;

struct Initial
{
	label_t label;
	bool setMarker;

	inline Initial (label_t l, bool b)
		: label (l)
		, setMarker (b)
	{}
};

typedef uniq_vector_t<const State *> accept_t;

class Action
{
public:
	enum type_t
	{
		MATCH,
		INITIAL,
		SAVE,
		MOVE,
		ACCEPT,
		RULE
	} type;
	union
	{
		Initial * initial;
		uint32_t save;
		const accept_t * accepts;
		const RuleOp * rule;
	} info;

public:
	inline Action ()
		: type (MATCH)
		, info ()
	{}
	~Action ()
	{
		clear ();
	}
	void set_initial (label_t label, bool used_marker)
	{
		clear ();
		type = INITIAL;
		info.initial = new Initial (label, used_marker);
	}
	void set_save (uint32_t save)
	{
		clear ();
		type = SAVE;
		info.save = save;
	}
	void set_move ()
	{
		clear ();
		type = MOVE;
	}
	void set_accept (const accept_t * accepts)
	{
		clear ();
		type = ACCEPT;
		info.accepts = accepts;
	}
	void set_rule (const RuleOp * const rule)
	{
		clear ();
		type = RULE;
		info.rule = rule;
	}

private:
	void clear ()
	{
		switch (type)
		{
			case INITIAL:
				delete info.initial;
				break;
			case MATCH:
			case SAVE:
			case MOVE:
			case ACCEPT:
			case RULE:
				break;
		}
	}
};

} // namespace re2c

#endif // _RE2C_IR_ADFA_ACTION_

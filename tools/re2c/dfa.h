/* $Id: dfa.h 569 2006-06-05 22:14:00Z helly $ */
#ifndef _dfa_h
#define _dfa_h

#include <iosfwd>
#include <map>
#include "re.h"

namespace re2c
{

extern void prtCh(std::ostream&, uint, bool useTalx = true);
extern void prtHex(std::ostream&, uint, bool useTalx = true);
extern void prtChOrHex(std::ostream&, uint, bool useTalx = true);
extern void printSpan(std::ostream&, uint, uint);

class DFA;

class State;

class Action
{

public:
	State	*state;

public:
	Action(State*);
	virtual ~Action();

	virtual void emit(std::ostream&, uint, bool&) const = 0;
	virtual bool isRule() const;
	virtual bool isMatch() const;
	virtual bool isInitial() const;
	virtual bool readAhead() const;

#ifdef PEDANTIC
protected:
	Action(const Action& oth)
		: state(oth.state)
	{
	}
	Action& operator = (const Action& oth)
	{
		state = oth.state;
		return *this;
	}
#endif
};

class Match: public Action
{
public:
	Match(State*);
	void emit(std::ostream&, uint, bool&) const;
	bool isMatch() const;
};

class Enter: public Action
{
public:
	uint	label;

public:
	Enter(State*, uint);
	void emit(std::ostream&, uint, bool&) const;
};

class Initial: public Enter
{
public:
	bool setMarker;

public:
	Initial(State*, uint, bool);
	void emit(std::ostream&, uint, bool&) const;
	bool isInitial() const;
};

class Save: public Match
{

public:
	uint	selector;

public:
	Save(State*, uint);
	void emit(std::ostream&, uint, bool&) const;
	bool isMatch() const;
};

class Move: public Action
{

public:
	Move(State*);
	void emit(std::ostream&, uint, bool&) const;
};

class Accept: public Action
{

public:
	typedef std::map<uint, State*> RuleMap;

	uint	nRules;
	uint	*saves;
	State	**rules;
	RuleMap mapRules;

public:
	Accept(State*, uint, uint*, State**);
	void emit(std::ostream&, uint, bool&) const;
	void emitBinary(std::ostream &o, uint ind, uint l, uint r, bool &readCh) const;
	void genRuleMap();

#ifdef PEDANTIC
private:
	Accept(const Accept& oth)
		: Action(oth)
		, nRules(oth.nRules)
		, saves(oth.saves)
		, rules(oth.rules)
	{
	}
	Accept& operator=(const Accept& oth)
	{
		new(this) Accept(oth);
		return *this;
	}
#endif
};

class Rule: public Action
{

public:
	RuleOp	*rule;

public:
	Rule(State*, RuleOp*);
	void emit(std::ostream&, uint, bool&) const;
	bool isRule() const;

#ifdef PEDANTIC
private:
	Rule (const Rule& oth)
		: Action(oth)
		, rule(oth.rule)
	{
	}
	Rule& operator=(const Rule& oth)
	{
		new(this) Rule(oth);
		return *this;
	}
#endif
};

class Span
{

public:
	uint	ub;
	State	*to;

public:
	uint show(std::ostream&, uint) const;
};

class Go
{
public:
	Go()
		: nSpans(0)
		, wSpans(~0u)
		, lSpans(~0u)
		, dSpans(~0u)
		, lTargets(~0u)
		, span(NULL)
	{
	}

public:
	uint	nSpans; // number of spans
	uint    wSpans; // number of spans in wide mode
	uint    lSpans; // number of low (non wide) spans
	uint    dSpans; // number of decision spans (decide between g and b mode)
	uint    lTargets;
	Span	*span;

public:
	void genGoto(  std::ostream&, uint ind, const State *from, const State *next, bool &readCh);
	void genBase(  std::ostream&, uint ind, const State *from, const State *next, bool &readCh, uint mask) const;
	void genLinear(std::ostream&, uint ind, const State *from, const State *next, bool &readCh, uint mask) const;
	void genBinary(std::ostream&, uint ind, const State *from, const State *next, bool &readCh, uint mask) const;
	void genSwitch(std::ostream&, uint ind, const State *from, const State *next, bool &readCh, uint mask) const;
	void genCpGoto(std::ostream&, uint ind, const State *from, const State *next, bool &readCh) const;
	void compact();
	void unmap(Go*, const State*);
};

class State
{

public:
	uint	label;
	RuleOp	*rule;
	State	*next;
	State	*link;
	uint	depth;		// for finding SCCs
	uint	kCount;
	Ins 	**kernel;

	bool    isPreCtxt;
	bool    isBase;
	Go      go;
	Action  *action;

public:
	State();
	~State();
	void emit(std::ostream&, uint, bool&) const;
	friend std::ostream& operator<<(std::ostream&, const State&);
	friend std::ostream& operator<<(std::ostream&, const State*);

#ifdef PEDANTIC
private:
	State(const State& oth)
		: label(oth.label)
		, rule(oth.rule)
		, next(oth.next)
		, link(oth.link)
		, depth(oth.depth)
		, kCount(oth.kCount)
		, kernel(oth.kernel)
		, isBase(oth.isBase)
		, go(oth.go)
		, action(oth.action)
	{
	}
	State& operator = (const State& oth)
	{
		new(this) State(oth);
		return *this;
	}
#endif
};

class DFA
{

public:
	uint	lbChar;
	uint	ubChar;
	uint	nStates;
	State	*head, **tail;
	State	*toDo;

public:
	DFA(Ins*, uint, uint, uint, Char*);
	~DFA();
	void addState(State**, State*);
	State *findState(Ins**, uint);
	void split(State*);

	void findSCCs();
	void findBaseState();
	void emit(std::ostream&, uint);

	friend std::ostream& operator<<(std::ostream&, const DFA&);
	friend std::ostream& operator<<(std::ostream&, const DFA*);

#ifdef PEDANTIC
	DFA(const DFA& oth)
		: lbChar(oth.lbChar)
		, ubChar(oth.ubChar)
		, nStates(oth.nStates)
		, head(oth.head)
		, tail(oth.tail)
		, toDo(oth.toDo)
	{
	}
	DFA& operator = (const DFA& oth)
	{
		new(this) DFA(oth);
		return *this;
	}
#endif
};

inline Action::Action(State *s) : state(s)
{
	delete s->action;
	s->action = this;
}

inline Action::~Action()
{
}

inline bool Action::isRule() const
{
	return false;
}

inline bool Action::isMatch() const
{
	return false;
}

inline bool Action::isInitial() const
{
	return false;
}

inline bool Action::readAhead() const
{
	return !isMatch() || (state && state->next && state->next->action && !state->next->action->isRule());
}

inline Match::Match(State *s) : Action(s)
{ }

inline bool Match::isMatch() const
{
	return true;
}

inline Enter::Enter(State *s, uint l) : Action(s), label(l)
{ }

inline Initial::Initial(State *s, uint l, bool b) : Enter(s, l), setMarker(b)
{ }

inline bool Initial::isInitial() const
{
	return true;
}

inline Save::Save(State *s, uint i) : Match(s), selector(i)
{ }

inline bool Save::isMatch() const
{
	return false;
}

inline bool Rule::isRule() const
{
	return true;
}

inline std::ostream& operator<<(std::ostream &o, const State *s)
{
	return o << *s;
}

inline std::ostream& operator<<(std::ostream &o, const DFA *dfa)
{
	return o << *dfa;
}

} // end namespace re2c

#endif

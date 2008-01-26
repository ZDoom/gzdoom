/* $Id: re.h 775 2007-07-10 19:33:17Z helly $ */
#ifndef _re_h
#define _re_h

#include <iostream>
#include <set>
#include "token.h"
#include "ins.h"
#include "globals.h"

namespace re2c
{

template<class _Ty>
class free_list: protected std::set<_Ty>
{
public:
	typedef typename std::set<_Ty>::iterator   iterator;
	typedef typename std::set<_Ty>::size_type  size_type;
	typedef typename std::set<_Ty>::key_type   key_type;
	
	free_list(): in_clear(false)
	{
	}
	
	using std::set<_Ty>::insert;

	size_type erase(const key_type& key)
	{
		if (!in_clear)
		{
			return std::set<_Ty>::erase(key);
		}
		return 0;
	}
	
	void clear()
	{
		in_clear = true;

		for(iterator it = this->begin(); it != this->end(); ++it)
		{
			delete *it;
		}
		std::set<_Ty>::clear();
		
		in_clear = false;
	}

	~free_list()
	{
		clear();
	}

protected:
	bool in_clear;
};

typedef struct extop
{
	char op;
	int	minsize;
	int	maxsize;
}

ExtOp;

struct CharPtn
{
	uint	card;
	CharPtn	*fix;
	CharPtn	*nxt;
};

typedef CharPtn *CharPtr;

struct CharSet
{
	CharSet();
	~CharSet();

	CharPtn	*fix;
	CharPtn	*freeHead, **freeTail;
	CharPtr	*rep;
	CharPtn	*ptn;
};

class Range
{

public:
	Range	*next;
	uint	lb, ub;		// [lb,ub)

	static free_list<Range*> vFreeList;

public:
	Range(uint l, uint u) : next(NULL), lb(l), ub(u)
	{
		vFreeList.insert(this);
	}

	Range(Range &r) : next(NULL), lb(r.lb), ub(r.ub)
	{
		vFreeList.insert(this);
	}

	~Range()
	{
		vFreeList.erase(this);
	}

	friend std::ostream& operator<<(std::ostream&, const Range&);
	friend std::ostream& operator<<(std::ostream&, const Range*);
};

inline std::ostream& operator<<(std::ostream &o, const Range *r)
{
	return r ? o << *r : o;
}

class RegExp
{

public:
	uint	size;

	static free_list<RegExp*> vFreeList;

public:
	RegExp() : size(0)
	{
		vFreeList.insert(this);
	}

	virtual ~RegExp()
	{
		vFreeList.erase(this);
	}

	virtual const char *typeOf() = 0;
	RegExp *isA(const char *t)
	{
		return typeOf() == t ? this : NULL;
	}

	virtual void split(CharSet&) = 0;
	virtual void calcSize(Char*) = 0;
	virtual uint fixedLength();
	virtual void compile(Char*, Ins*) = 0;
	virtual void display(std::ostream&) const = 0;
	friend std::ostream& operator<<(std::ostream&, const RegExp&);
	friend std::ostream& operator<<(std::ostream&, const RegExp*);
};

inline std::ostream& operator<<(std::ostream &o, const RegExp &re)
{
	re.display(o);
	return o;
}

inline std::ostream& operator<<(std::ostream &o, const RegExp *re)
{
	return o << *re;
}

class NullOp: public RegExp
{

public:
	static const char *type;

public:
	const char *typeOf()
	{
		return type;
	}

	void split(CharSet&);
	void calcSize(Char*);
	uint fixedLength();
	void compile(Char*, Ins*);
	void display(std::ostream &o) const
	{
		o << "_";
	}
};

class MatchOp: public RegExp
{

public:
	static const char *type;
	Range	*match;

public:
	MatchOp(Range *m) : match(m)
	{
	}

	const char *typeOf()
	{
		return type;
	}

	void split(CharSet&);
	void calcSize(Char*);
	uint fixedLength();
	void compile(Char*, Ins*);
	void display(std::ostream&) const;

#ifdef PEDANTIC
private:
	MatchOp(const MatchOp& oth)
		: RegExp(oth)
		, match(oth.match)
	{
	}
	
	MatchOp& operator = (const MatchOp& oth)
	{
		new(this) MatchOp(oth);
		return *this;
	}
#endif
};

class RuleOp: public RegExp
{
public:
	static const char *type;

private:
	RegExp   *exp;

public:
	RegExp   *ctx;
	Ins      *ins;
	uint     accept;
	Token    *code;
	uint     line;

public:
	RuleOp(RegExp*, RegExp*, Token*, uint);

	~RuleOp()
	{
		delete code;
	}

	const char *typeOf()
	{
		return type;
	}

	void split(CharSet&);
	void calcSize(Char*);
	void compile(Char*, Ins*);
	void display(std::ostream &o) const
	{
		o << exp << "/" << ctx << ";";
	}

#ifdef PEDANTIC
private:
	RuleOp(const RuleOp& oth)
		: RegExp(oth)
		, exp(oth.exp)
		, ctx(oth.ctx)
		, ins(oth.ins)
		, accept(oth.accept)
		, code(oth.code)
		, line(oth.line)
	{
	}
	RuleOp& operator = (const RuleOp& oth)
	{
		new(this) RuleOp(oth);
		return *this;
	}
#endif
};

class RuleLine: public line_number
{
public:

	RuleLine(const RuleOp& _op)
		: op(_op)
	{
	}

	uint get_line() const
	{
		return op.code->line;
	}

	const RuleOp& op;
};

RegExp *mkAlt(RegExp*, RegExp*);

class AltOp: public RegExp
{

private:
	RegExp	*exp1, *exp2;

public:
	static const char *type;

public:
	AltOp(RegExp *e1, RegExp *e2)
		: exp1(e1)
		, exp2(e2)
	{
	}

	const char *typeOf()
	{
		return type;
	}

	void split(CharSet&);
	void calcSize(Char*);
	uint fixedLength();
	void compile(Char*, Ins*);
	void display(std::ostream &o) const
	{
		o << exp1 << "|" << exp2;
	}

	friend RegExp *mkAlt(RegExp*, RegExp*);

#ifdef PEDANTIC
private:
	AltOp(const AltOp& oth)
		: RegExp(oth)
		, exp1(oth.exp1)
		, exp2(oth.exp2)
	{
	}
	AltOp& operator = (const AltOp& oth)
	{
		new(this) AltOp(oth);
		return *this;
	}
#endif
};

class CatOp: public RegExp
{

private:
	RegExp	*exp1, *exp2;

public:
	static const char *type;

public:
	CatOp(RegExp *e1, RegExp *e2)
		: exp1(e1)
		, exp2(e2)
	{
	}

	const char *typeOf()
	{
		return type;
	}

	void split(CharSet&);
	void calcSize(Char*);
	uint fixedLength();
	void compile(Char*, Ins*);
	void display(std::ostream &o) const
	{
		o << exp1 << exp2;
	}

#ifdef PEDANTIC
private:
	CatOp(const CatOp& oth)
		: RegExp(oth)
		, exp1(oth.exp1)
		, exp2(oth.exp2)
	{
	}
	CatOp& operator = (const CatOp& oth)
	{
		new(this) CatOp(oth);
		return *this;
	}
#endif
};

class CloseOp: public RegExp
{

private:
	RegExp	*exp;

public:
	static const char *type;

public:
	CloseOp(RegExp *e)
		: exp(e)
	{
	}

	const char *typeOf()
	{
		return type;
	}

	void split(CharSet&);
	void calcSize(Char*);
	void compile(Char*, Ins*);
	void display(std::ostream &o) const
	{
		o << exp << "+";
	}

#ifdef PEDANTIC
private:
	CloseOp(const CloseOp& oth)
		: RegExp(oth)
		, exp(oth.exp)
	{
	}
	CloseOp& operator = (const CloseOp& oth)
	{
		new(this) CloseOp(oth);
		return *this;
	}
#endif
};

class CloseVOp: public RegExp
{

private:
	RegExp	*exp;
	int	min;
	int	max;

public:
	static const char *type;

public:
	CloseVOp(RegExp *e, int lb, int ub)
		: exp(e)
		, min(lb)
		, max(ub)
	{
	}

	const char *typeOf()
	{
		return type;
	}

	void split(CharSet&);
	void calcSize(Char*);
	void compile(Char*, Ins*);
	void display(std::ostream &o) const
	{
		o << exp << "+";
	}
#ifdef PEDANTIC
private:
	CloseVOp(const CloseVOp& oth)
		: RegExp(oth)
		, exp(oth.exp)
		, min(oth.min)
		, max(oth.max)
	{
	}
	CloseVOp& operator = (const CloseVOp& oth)
	{
		new(this) CloseVOp(oth);
		return *this;
	}
#endif
};

extern void genCode(std::ostream&, RegExp*);
extern void genCode(std::ostream&, uint, RegExp*);
extern void genGetState(std::ostream&, uint&, uint);
extern RegExp *mkDiff(RegExp*, RegExp*);
extern RegExp *mkAlt(RegExp*, RegExp*);

} // end namespace re2c

#endif

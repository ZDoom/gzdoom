/* $Id: re.h,v 1.5 2004/05/13 02:58:18 nuffer Exp $ */
#ifndef _re_h
#define _re_h

#include <iostream>
#include "token.h"
#include "ins.h"

typedef struct extop {
      char      op;
      int	minsize;
      int	maxsize;
} ExtOp;

struct CharPtn {
    uint	card;
    CharPtn	*fix;
    CharPtn	*nxt;
};

struct CharSet {
    CharPtn	*fix;
    CharPtn	*freeHead, **freeTail;
    CharPtn	*rep[nChars];
    CharPtn	ptn[nChars];
};

class Range {
public:
    Range	*next;
    uint	lb, ub;		// [lb,ub)
public:
    Range(uint l, uint u) : next(NULL), lb(l), ub(u)
	{ }
    Range(Range &r) : next(NULL), lb(r.lb), ub(r.ub)
	{ }
    friend std::ostream& operator<<(std::ostream&, const Range&);
    friend std::ostream& operator<<(std::ostream&, const Range*);
};

inline std::ostream& operator<<(std::ostream &o, const Range *r){
	return r? o << *r : o;
}

class RegExp {
public:
    uint	size;
public:
    virtual char *typeOf() = 0;
    RegExp *isA(char *t)
	{ return typeOf() == t? this : NULL; }
    virtual void split(CharSet&) = 0;
    virtual void calcSize(Char*) = 0;
    virtual uint fixedLength();
    virtual void compile(Char*, Ins*) = 0;
    virtual void display(std::ostream&) const = 0;
    friend std::ostream& operator<<(std::ostream&, const RegExp&);
    friend std::ostream& operator<<(std::ostream&, const RegExp*);
};

inline std::ostream& operator<<(std::ostream &o, const RegExp &re){
    re.display(o);
    return o;
}

inline std::ostream& operator<<(std::ostream &o, const RegExp *re){
    return o << *re;
}

class NullOp: public RegExp {
public:
    static char *type;
public:
    char *typeOf()
	{ return type; }
    void split(CharSet&);
    void calcSize(Char*);
    uint fixedLength();
    void compile(Char*, Ins*);
    void display(std::ostream &o) const {
	o << "_";
    }
};

class MatchOp: public RegExp {
public:
    static char *type;
    Range	*match;
public:
    MatchOp(Range *m) : match(m)
	{ }
    char *typeOf()
	{ return type; }
    void split(CharSet&);
    void calcSize(Char*);
    uint fixedLength();
    void compile(Char*, Ins*);
    void display(std::ostream&) const;
};

class RuleOp: public RegExp {
private:
    RegExp	*exp;
public:
    RegExp	*ctx;
    static char *type;
    Ins		*ins;
    uint	accept;
    Token	*code;
    uint	line;
public:
    RuleOp(RegExp*, RegExp*, Token*, uint);
    char *typeOf()
	{ return type; }
    void split(CharSet&);
    void calcSize(Char*);
    void compile(Char*, Ins*);
    void display(std::ostream &o) const {
	o << exp << "/" << ctx << ";";
    }
};

class AltOp: public RegExp {
private:
    RegExp	*exp1, *exp2;
public:
    static char *type;
public:
    AltOp(RegExp *e1, RegExp *e2)
	{ exp1 = e1;  exp2 = e2; }
    char *typeOf()
	{ return type; }
    void split(CharSet&);
    void calcSize(Char*);
    uint fixedLength();
    void compile(Char*, Ins*);
    void display(std::ostream &o) const {
	o << exp1 << "|" << exp2;
    }
    friend RegExp *mkAlt(RegExp*, RegExp*);
};

class CatOp: public RegExp {
private:
    RegExp	*exp1, *exp2;
public:
    static char *type;
public:
    CatOp(RegExp *e1, RegExp *e2)
	{ exp1 = e1;  exp2 = e2; }
    char *typeOf()
	{ return type; }
    void split(CharSet&);
    void calcSize(Char*);
    uint fixedLength();
    void compile(Char*, Ins*);
    void display(std::ostream &o) const {
	o << exp1 << exp2;
    }
};

class CloseOp: public RegExp {
private:
    RegExp	*exp;
public:
    static char *type;
public:
    CloseOp(RegExp *e)
	{ exp = e; }
    char *typeOf()
	{ return type; }
    void split(CharSet&);
    void calcSize(Char*);
    void compile(Char*, Ins*);
    void display(std::ostream &o) const {
	o << exp << "+";
    }
};

class CloseVOp: public RegExp {
private:
    RegExp	*exp;
    int		min;
    int		max;
public:
    static char *type;
public:
    CloseVOp(RegExp *e, int lb, int ub)
	{ exp = e; min = lb; max = ub; }
    char *typeOf()
	{ return type; }
    void split(CharSet&);
    void calcSize(Char*);
    void compile(Char*, Ins*);
    void display(std::ostream &o) const {
	o << exp << "+";
    }
};

extern void genCode(std::ostream&, RegExp*);
extern RegExp *mkDiff(RegExp*, RegExp*);
extern RegExp *strToRE(SubStr);
extern RegExp *ranToRE(SubStr);
extern RegExp *strToCaseInsensitiveRE(SubStr s);
	
#endif

/* $Id: actions.cc,v 1.5 2004/05/13 02:58:17 nuffer Exp $ */
#include <time.h>
#include <string.h>
#include <iostream>
#include <iomanip>

#include "globals.h"
#include "parser.h"
#include "dfa.h"

Symbol *Symbol::first = NULL;

Symbol::Symbol(const SubStr &str) : next(first), name(str), re(NULL) {
    first = this;
}

Symbol *Symbol::find(const SubStr &str){
    for(Symbol *sym = first; sym; sym = sym->next)
	if(sym->name == str) return sym;
    return new Symbol(str);
}

void showIns(std::ostream &o, const Ins &i, const Ins &base){
    o.width(3);
    o << &i - &base << ": ";
    switch(i.i.tag){
    case CHAR: {
	o << "match ";
	for(const Ins *j = &(&i)[1]; j < (Ins*) i.i.link; ++j)
	    prtCh(o, (const uchar)j->c.value);
	break;
    } case GOTO:
	o << "goto " << ((Ins*) i.i.link - &base);
	break;
    case FORK:
	o << "fork " << ((Ins*) i.i.link - &base);
	break;
    case CTXT:
	o << "term " << ((RuleOp*) i.i.link)->accept;
	break;
    case TERM:
	o << "term " << ((RuleOp*) i.i.link)->accept;
	break;
    }
    o << "\n";
}

uint RegExp::fixedLength(){
    return ~0;
}

char *NullOp::type = "NullOp";

void NullOp::calcSize(Char*){
    size = 0;
}

uint NullOp::fixedLength(){
    return 0;
}

void NullOp::compile(Char*, Ins*){
    ;
}

void NullOp::split(CharSet&){
    ;
}

std::ostream& operator<<(std::ostream &o, const Range &r){
    if((r.ub - r.lb) == 1){
	prtCh(o, r.lb);
    } else {
	prtCh(o, r.lb); o << "-"; prtCh(o, r.ub-1);
    }
    return o << r.next;
}

Range *doUnion(Range *r1, Range *r2){
    Range *r, **rP = &r;
    for(;;){
	Range *s;
	if(r1->lb <= r2->lb){
	    s = new Range(*r1);
	} else {
	    s = new Range(*r2);
	}
	*rP = s;
	rP = &s->next;
	for(;;){
	    if(r1->lb <= r2->lb){
		if(r1->lb > s->ub)
		    break;
		if(r1->ub > s->ub)
		    s->ub = r1->ub;
		if(!(r1 = r1->next)){
		    uint ub = 0;
		    for(; r2 && r2->lb <= s->ub; r2 = r2->next)
			ub = r2->ub;
		    if(ub > s->ub)
			s->ub = ub;
		    *rP = r2;
		    return r;
		}
	    } else {
		if(r2->lb > s->ub)
		    break;
		if(r2->ub > s->ub)
		    s->ub = r2->ub;
		if(!(r2 = r2->next)){
		    uint ub = 0;
		    for(; r1 && r1->lb <= s->ub; r1 = r1->next)
			ub = r1->ub;
		    if(ub > s->ub)
			s->ub = ub;
		    *rP = r1;
		    return r;
		}
	    }
	}
    }
    *rP = NULL;
    return r;
}

Range *doDiff(Range *r1, Range *r2){
    Range *r, *s, **rP = &r;
    for(; r1; r1 = r1->next){
	uint lb = r1->lb;
	for(; r2 && r2->ub <= r1->lb; r2 = r2->next);
	for(; r2 && r2->lb <  r1->ub; r2 = r2->next){
	    if(lb < r2->lb){
		*rP = s = new Range(lb, r2->lb);
		rP = &s->next;
	    }
	    if((lb = r2->ub) >= r1->ub)
		goto noMore;
	}
	*rP = s = new Range(lb, r1->ub);
	rP = &s->next;
    noMore:;
    }
    *rP = NULL;
    return r;
}

MatchOp *merge(MatchOp *m1, MatchOp *m2){
    if(!m1)
	return m2;
    if(!m2)
	return m1;
    return new MatchOp(doUnion(m1->match, m2->match));
}

char *MatchOp::type = "MatchOp";

void MatchOp::display(std::ostream &o) const{
    o << match;
}

void MatchOp::calcSize(Char *rep){
    size = 1;
    for(Range *r = match; r; r = r->next)
	for(uint c = r->lb; c < r->ub; ++c)
	    if(rep[c] == c)
		++size;
}

uint MatchOp::fixedLength(){
    return 1;
}

void MatchOp::compile(Char *rep, Ins *i){
    i->i.tag = CHAR;
    i->i.link = &i[size];
    Ins *j = &i[1];
    uint bump = size;
    for(Range *r = match; r; r = r->next){
	for(uint c = r->lb; c < r->ub; ++c){
	    if(rep[c] == c){
		j->c.value = c;
		j->c.bump = --bump;
		j++;
	    }
	}
    }
}

void MatchOp::split(CharSet &s){
    for(Range *r = match; r; r = r->next){
	for(uint c = r->lb; c < r->ub; ++c){
	    CharPtn *x = s.rep[c], *a = x->nxt;
	    if(!a){
		if(x->card == 1)
		    continue;
		x->nxt = a = s.freeHead;
		if(!(s.freeHead = s.freeHead->nxt))
		    s.freeTail = &s.freeHead;
		a->nxt = NULL;
		x->fix = s.fix;
		s.fix = x;
	    }
	    if(--(x->card) == 0){
		*s.freeTail = x;
		*(s.freeTail = &x->nxt) = NULL;
	    }
	    s.rep[c] = a;
	    ++(a->card);
	}
    }
    for(; s.fix; s.fix = s.fix->fix)
	if(s.fix->card)
	    s.fix->nxt = NULL;
}

RegExp *mkDiff(RegExp *e1, RegExp *e2){
    MatchOp *m1, *m2;
    if(!(m1 = (MatchOp*) e1->isA(MatchOp::type)))
	return NULL;
    if(!(m2 = (MatchOp*) e2->isA(MatchOp::type)))
	return NULL;
    Range *r = doDiff(m1->match, m2->match);
    return r? (RegExp*) new MatchOp(r) : (RegExp*) new NullOp;
}

RegExp *doAlt(RegExp *e1, RegExp *e2){
    if(!e1)
	return e2;
    if(!e2)
	return e1;
    return new AltOp(e1, e2);
}

RegExp *mkAlt(RegExp *e1, RegExp *e2){
    AltOp *a;
    MatchOp *m1, *m2;
    if((a = (AltOp*) e1->isA(AltOp::type))){
	if((m1 = (MatchOp*) a->exp1->isA(MatchOp::type)))
	    e1 = a->exp2;
    } else if((m1 = (MatchOp*) e1->isA(MatchOp::type))){
	    e1 = NULL;
    }
    if((a = (AltOp*) e2->isA(AltOp::type))){
	if((m2 = (MatchOp*) a->exp1->isA(MatchOp::type)))
	    e2 = a->exp2;
    } else if((m2 = (MatchOp*) e2->isA(MatchOp::type))){
	    e2 = NULL;
    }
    return doAlt(merge(m1, m2), doAlt(e1, e2));
}

char *AltOp::type = "AltOp";

void AltOp::calcSize(Char *rep){
    exp1->calcSize(rep);
    exp2->calcSize(rep);
    size = exp1->size + exp2->size + 2;
}

uint AltOp::fixedLength(){
    uint l1 = exp1->fixedLength();
    uint l2 = exp1->fixedLength();
    if(l1 != l2 || l1 == ~0u)
	return ~0;
    return l1;
}

void AltOp::compile(Char *rep, Ins *i){
    i->i.tag = FORK;
    Ins *j = &i[exp1->size + 1];
    i->i.link = &j[1];
    exp1->compile(rep, &i[1]);
    j->i.tag = GOTO;
    j->i.link = &j[exp2->size + 1];
    exp2->compile(rep, &j[1]);
}

void AltOp::split(CharSet &s){
    exp1->split(s);
    exp2->split(s);
}

char *CatOp::type = "CatOp";

void CatOp::calcSize(Char *rep){
    exp1->calcSize(rep);
    exp2->calcSize(rep);
    size = exp1->size + exp2->size;
}

uint CatOp::fixedLength(){
    uint l1, l2;
    if((l1 = exp1->fixedLength()) != ~0u )
        if((l2 = exp2->fixedLength()) != ~0u)
	    return l1+l2;
    return ~0;
}

void CatOp::compile(Char *rep, Ins *i){
    exp1->compile(rep, &i[0]);
    exp2->compile(rep, &i[exp1->size]);
}

void CatOp::split(CharSet &s){
    exp1->split(s);
    exp2->split(s);
}

char *CloseOp::type = "CloseOp";

void CloseOp::calcSize(Char *rep){
    exp->calcSize(rep);
    size = exp->size + 1;
}

void CloseOp::compile(Char *rep, Ins *i){
    exp->compile(rep, &i[0]);
    i += exp->size;
    i->i.tag = FORK;
    i->i.link = i - exp->size;
}

void CloseOp::split(CharSet &s){
    exp->split(s);
}

char *CloseVOp::type = "CloseVOp";

void CloseVOp::calcSize(Char *rep){
    exp->calcSize(rep);
    if(max >= 0)
      size = (exp->size * min) + ((1 + exp->size) * (max - min));
    else
      size = (exp->size * min) + 1;
}

void CloseVOp::compile(Char *rep, Ins *i){
    Ins *jumppoint;
    int st = 0;
    jumppoint = i + ((1 + exp->size) * (max - min));
    for(st = min; st < max; st++) {
      i->i.tag = FORK;
      i->i.link = jumppoint;
      i+=1;
      exp->compile(rep, &i[0]);
      i += exp->size;
    }
    for(st = 0; st < min; st++) {
      exp->compile(rep, &i[0]);
      i += exp->size;
      if(max < 0 && st == 0) {
        i->i.tag = FORK;
        i->i.link = i - exp->size;
        i++;
      }
    }
}

void CloseVOp::split(CharSet &s){
    exp->split(s);
}

RegExp *expr(Scanner &);

uchar unescape(SubStr &s){
    s.len--;
    uchar c;
    if((c = *s.str++) != '\\' || s.len == 0)
	return xlat[c];
    s.len--;
    switch(c = *s.str++){
    case 'n':
	return xlat['\n'];
    case 't':
	return xlat['\t'];
    case 'v':
	return xlat['\v'];
    case 'b':
	return xlat['\b'];
    case 'r':
	return xlat['\r'];
    case 'f':
	return xlat['\f'];
    case 'a':
	return xlat['\a'];
    case '0': case '1': case '2': case '3':
    case '4': case '5': case '6': case '7': {
	uchar v = c - '0';
	for(; s.len != 0 && '0' <= (c = *s.str) && c <= '7'; s.len--, s.str++)
	    v = v*8 + (c - '0');
	return v;
    } default:
	return xlat[c];
    }
}

Range *getRange(SubStr &s){
    uchar lb = unescape(s), ub;
    if(s.len < 2 || *s.str != '-'){
	ub = lb;
    } else {
	s.len--; s.str++;
	ub = unescape(s);
	if(ub < lb){
	    uchar tmp;
	    tmp = lb; lb = ub; ub = tmp;
	}
    }
    return new Range(lb, ub+1);
}

RegExp *matchChar(uint c){
    return new MatchOp(new Range(c, c+1));
}

RegExp *strToRE(SubStr s){
    s.len -= 2; s.str += 1;
    if(s.len == 0)
	return new NullOp;
    RegExp *re = matchChar(unescape(s));
    while(s.len > 0)
	re = new CatOp(re, matchChar(unescape(s)));
    return re;
}

RegExp *strToCaseInsensitiveRE(SubStr s){
    s.len -= 2; s.str += 1;
    if(s.len == 0)
	return new NullOp;
	uchar c = unescape(s);
	RegExp *re, *reL, *reU;
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
	    reL = matchChar(tolower(c));
    	reU = matchChar(toupper(c));
	    re = mkAlt(reL, reU);
	} else {
	    re = matchChar(c);
	}
    while(s.len > 0) {
		uchar c = unescape(s);
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
		    reL = matchChar(tolower(c));
    		reU = matchChar(toupper(c));
			re = new CatOp(re, mkAlt(reL, reU));
    	} else {
			re = new CatOp(re, matchChar(c));
		}
	}
    return re;
}

RegExp *ranToRE(SubStr s){
    s.len -= 2; s.str += 1;
    if(s.len == 0)
	return new NullOp;
    Range *r = getRange(s);
    while(s.len > 0)
	r = doUnion(r, getRange(s));
    return new MatchOp(r);
}

char *RuleOp::type = "RuleOp";

RuleOp::RuleOp(RegExp *e, RegExp *c, Token *t, uint a)
	: exp(e), ctx(c), ins(NULL), accept(a), code(t) {
    ;
}

void RuleOp::calcSize(Char *rep){
    exp->calcSize(rep);
    ctx->calcSize(rep);
    size = exp->size + ctx->size + 1;
}

void RuleOp::compile(Char *rep, Ins *i){
    ins = i;
    exp->compile(rep, &i[0]);
    i += exp->size;
    ctx->compile(rep, &i[0]);
    i += ctx->size;
    i->i.tag = TERM;
    i->i.link = this;
}

void RuleOp::split(CharSet &s){
    exp->split(s);
    ctx->split(s);
}

extern void printSpan(std::ostream&, uint, uint);

void optimize(Ins *i){
    while(!isMarked(i)){
	mark(i);
	if(i->i.tag == CHAR){
	    i = (Ins*) i->i.link;
	} else if(i->i.tag == GOTO || i->i.tag == FORK){
	    Ins *target = (Ins*) i->i.link;
	    optimize(target);
	    if(target->i.tag == GOTO)
		i->i.link = target->i.link == target? i : target;
	    if(i->i.tag == FORK){
		Ins *follow = (Ins*) &i[1];
		optimize(follow);
		if(follow->i.tag == GOTO && follow->i.link == follow){
		    i->i.tag = GOTO;
		} else if(i->i.link == i){
		    i->i.tag = GOTO;
		    i->i.link = follow;
		}
	    }
	    return;
	} else {
	    ++i;
	}
    }
}

void genCode(std::ostream& o, RegExp *re){
    CharSet cs;
    uint j;
    memset(&cs, 0, sizeof(cs));
    for(j = 0; j < nChars; ++j){
	cs.rep[j] = &cs.ptn[0];
	cs.ptn[j].nxt = &cs.ptn[j+1];
    }
    cs.freeHead = &cs.ptn[1];
    *(cs.freeTail = &cs.ptn[nChars-1].nxt) = NULL;
    cs.ptn[0].card = nChars;
    cs.ptn[0].nxt = NULL;
    re->split(cs);
/*
    for(uint k = 0; k < nChars;){
	for(j = k; ++k < nChars && cs.rep[k] == cs.rep[j];);
	printSpan(cerr, j, k);
	cerr << "\t" << cs.rep[j] - &cs.ptn[0] << endl;
    }
*/
    Char rep[nChars];
    for(j = 0; j < nChars; ++j){
	if(!cs.rep[j]->nxt)
	    cs.rep[j]->nxt = &cs.ptn[j];
	rep[j] = (Char) (cs.rep[j]->nxt - &cs.ptn[0]);
    }

    re->calcSize(rep);
    Ins *ins = new Ins[re->size+1];
    memset(ins, 0, (re->size+1)*sizeof(Ins));
    re->compile(rep, ins);
    Ins *eoi = &ins[re->size];
    eoi->i.tag = GOTO;
    eoi->i.link = eoi;

    optimize(ins);
    for(j = 0; j < re->size;){
	unmark(&ins[j]);
	if(ins[j].i.tag == CHAR){
	    j = (Ins*) ins[j].i.link - ins;
	} else {
	    j++;
	}
    }

    DFA *dfa = new DFA(ins, re->size, 0, 256, rep);
    dfa->emit(o);
    delete dfa;
    delete [] ins;
}

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "globals.h"
#include "substr.h"
#include "dfa.h"

inline char octCh(uint c){
    return '0' + c%8;
}

void prtCh(std::ostream &o, uchar c){
    uchar oc = talx[c];
    switch(oc){
    case '\'': o << "\\'"; break;
    case '\n': o << "\\n"; break;
    case '\t': o << "\\t"; break;
    case '\v': o << "\\v"; break;
    case '\b': o << "\\b"; break;
    case '\r': o << "\\r"; break;
    case '\f': o << "\\f"; break;
    case '\a': o << "\\a"; break;
    case '\\': o << "\\\\"; break;
    default:
	if(isprint(oc))
	    o << (char) oc;
	else
	    o << '\\' << octCh(c/64) << octCh(c/8) << octCh(c);
    }
}

void printSpan(std::ostream &o, uint lb, uint ub){
    if(lb > ub)
	o << "*";
    o << "[";
    if((ub - lb) == 1){
	prtCh(o, lb);
    } else {
	prtCh(o, lb);
	o << "-";
	prtCh(o, ub-1);
    }
    o << "]";
}

uint Span::show(std::ostream &o, uint lb){
    if(to){
	printSpan(o, lb, ub);
	o << " " << to->label << "; ";
    }
    return ub;
}

std::ostream& operator<<(std::ostream &o, const State &s){
    o << "state " << s.label;
    if(s.rule)
	o << " accepts " << s.rule->accept;
    o << "\n";
    ++oline;
    uint lb = 0;
    for(uint i = 0; i < s.go.nSpans; ++i)
	lb = s.go.span[i].show(o, lb);
    return o;
}

std::ostream& operator<<(std::ostream &o, const DFA &dfa){
    for(State *s = dfa.head; s; s = s->next)
    {
	o << s << "\n\n";
	++oline;
    }
    return o;
}

State::State() : rule(NULL), link(NULL), kCount(0), kernel(NULL), action(NULL) {
    go.nSpans = 0;
    go.span = NULL;
}

State::~State(){
    delete [] kernel;
    delete [] go.span;
}

static Ins **closure(Ins **cP, Ins *i){
    while(!isMarked(i)){
	mark(i);
	*(cP++) = i;
	if(i->i.tag == FORK){
	    cP = closure(cP, i + 1);
	    i = (Ins*) i->i.link;
	} else if(i->i.tag == GOTO){
	    i = (Ins*) i->i.link;
	} else
	    break;
    }
    return cP;
}

struct GoTo {
    Char	ch;
    void	*to;
};

DFA::DFA(Ins *ins, uint ni, uint lb, uint ub, Char *rep)
    : lbChar(lb), ubChar(ub) {
    Ins **work = new Ins*[ni+1];
    uint nc = ub - lb;
    GoTo *goTo = new GoTo[nc];
    Span *span = new Span[nc];
    memset((char*) goTo, 0, nc*sizeof(GoTo));
    tail = &head;
    head = NULL;
    nStates = 0;
    toDo = NULL;
    findState(work, closure(work, &ins[0]) - work);
    while(toDo){
	State *s = toDo;
	toDo = s->link;

	Ins **cP, **iP, *i;
	uint nGoTos = 0;
	uint j;

	s->rule = NULL;
	for(iP = s->kernel; (i = *iP); ++iP){
	    if(i->i.tag == CHAR){
		for(Ins *j = i + 1; j < (Ins*) i->i.link; ++j){
		    if(!(j->c.link = goTo[j->c.value - lb].to))
			goTo[nGoTos++].ch = j->c.value;
		    goTo[j->c.value - lb].to = j;
		}
	    } else if(i->i.tag == TERM){
		if(!s->rule || ((RuleOp*) i->i.link)->accept < s->rule->accept)
		    s->rule = (RuleOp*) i->i.link;
	    }
	}

	for(j = 0; j < nGoTos; ++j){
	    GoTo *go = &goTo[goTo[j].ch - lb];
	    i = (Ins*) go->to;
	    for(cP = work; i; i = (Ins*) i->c.link)
		cP = closure(cP, i + i->c.bump);
	    go->to = findState(work, cP - work);
	}

	s->go.nSpans = 0;
	for(j = 0; j < nc;){
	    State *to = (State*) goTo[rep[j]].to;
	    while(++j < nc && goTo[rep[j]].to == to);
	    span[s->go.nSpans].ub = lb + j;
	    span[s->go.nSpans].to = to;
	    s->go.nSpans++;
	}

	for(j = nGoTos; j-- > 0;)
	    goTo[goTo[j].ch - lb].to = NULL;

	s->go.span = new Span[s->go.nSpans];
	memcpy((char*) s->go.span, (char*) span, s->go.nSpans*sizeof(Span));

	(void) new Match(s);

    }
    delete [] work;
    delete [] goTo;
    delete [] span;
}

DFA::~DFA(){
    State *s;
    while((s = head)){
	head = s->next;
	delete s;
    }
}

void DFA::addState(State **a, State *s){
    s->label = nStates++;
    s->next = *a;
    *a = s;
    if(a == tail)
	tail = &s->next;
}

State *DFA::findState(Ins **kernel, uint kCount){
    Ins **cP, **iP, *i;
    State *s;

    kernel[kCount] = NULL;

    cP = kernel;
    for(iP = kernel; (i = *iP); ++iP){
	 if(i->i.tag == CHAR || i->i.tag == TERM){
	     *cP++ = i;
	} else {
	     unmark(i);
	}
    }
    kCount = cP - kernel;
    kernel[kCount] = NULL;

    for(s = head; s; s = s->next){
	 if(s->kCount == kCount){
	     for(iP = s->kernel; (i = *iP); ++iP)
		 if(!isMarked(i))
		     goto nextState;
	     goto unmarkAll;
	 }
	 nextState:;
    }

    s = new State;
    addState(tail, s);
    s->kCount = kCount;
    s->kernel = new Ins*[kCount+1];
    memcpy(s->kernel, kernel, (kCount+1)*sizeof(Ins*));
    s->link = toDo;
    toDo = s;

unmarkAll:
    for(iP = kernel; (i = *iP); ++iP)
	 unmark(i);

    return s;
}

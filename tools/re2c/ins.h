/* $Id: ins.h,v 1.3 2004/05/13 02:58:17 nuffer Exp $ */
#ifndef _ins_h
#define _ins_h

#include "basics.h"

const uint nChars = 256;
typedef uchar Char;

const uint CHAR = 0;
const uint GOTO = 1;
const uint FORK = 2;
const uint TERM = 3;
const uint CTXT = 4;

union Ins {
    struct {
	byte	tag;
	byte	marked;
	void	*link;
    }			i;
    struct {
	ushort	value;
	ushort	bump;
	void	*link;
    }			c;
};

inline bool isMarked(Ins *i){
    return i->i.marked != 0;
}

inline void mark(Ins *i){
    i->i.marked = true;
}

inline void unmark(Ins *i){
    i->i.marked = false;
}

#endif

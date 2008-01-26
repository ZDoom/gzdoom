/* $Id: ins.h 535 2006-05-25 13:36:14Z helly $ */
#ifndef _ins_h
#define _ins_h

#include "basics.h"

namespace re2c
{

typedef unsigned short Char;

const uint CHAR = 0;
const uint GOTO = 1;
const uint FORK = 2;
const uint TERM = 3;
const uint CTXT = 4;

union Ins {

	struct
	{
		byte	tag;
		byte	marked;
		void	*link;
	}

	i;

	struct
	{
		ushort	value;
		ushort	bump;
		void	*link;
	}

	c;
};

inline bool isMarked(Ins *i)
{
	return i->i.marked != 0;
}

inline void mark(Ins *i)
{
	i->i.marked = true;
}

inline void unmark(Ins *i)
{
	i->i.marked = false;
}

} // end namespace re2c

#endif

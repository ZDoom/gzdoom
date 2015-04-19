#ifndef P_TAGS_H
#define P_TAGS_H 1

#include "r_defs.h"
#include "r_state.h"

class FSectorTagIterator
{
protected:
	int searchtag;
	int start;

public:
	FSectorTagIterator(int tag)
	{
		searchtag = tag;
		start = sectors[(unsigned)tag % (unsigned)numsectors].firsttag;
	}

	// Special constructor for actions that treat tag 0 as  'back of activation line'
	FSectorTagIterator(int tag, line_t *line)
	{
		if (tag == 0)
		{
			searchtag = INT_MIN;
			start = (line == NULL || line->backsector == NULL)? -1 : (int)(line->backsector - sectors);
		}
		else
		{
			searchtag = tag;
			start = sectors[(unsigned)tag % (unsigned)numsectors].firsttag;
		}
	}

	int Next();
	int NextCompat(bool compat, int secnum);
};

class FLineIdIterator
{
protected:
	int searchtag;
	int start;

public:
	FLineIdIterator(int id)
	{
		searchtag = id;
		start = lines[(unsigned) id % (unsigned) numlines].firstid;
	}

	int Next();
};


inline int P_FindFirstSectorFromTag(int tag)
{
	FSectorTagIterator it(tag);
	return it.Next();
}

inline int P_FindFirstLineFromID(int tag)
{
	FLineIdIterator it(tag);
	return it.Next();
}

#endif

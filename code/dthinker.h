// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//	MapObj data. Map Objects or mobjs are actors, entities,
//	thinker, take-your-pick... anything that moves, acts, or
//	suffers state changes of more or less violent nature.
//
//-----------------------------------------------------------------------------

#ifndef __DTHINKER_H__
#define __DTHINKER_H__

#include <stdlib.h>
#include "dobject.h"
#include "lists.h"

class AActor;
class player_s;
struct pspdef_s;

typedef void (*actionf_v)();
typedef void (*actionf_p1)( AActor* );
typedef void (*actionf_p2)( player_s*, pspdef_s* );

typedef union
{
	void *acvoid;
	actionf_p1	acp1;
	actionf_v	acv;
	actionf_p2	acp2;
} actionf_t;

class FThinkerIterator;

enum { MAX_STATNUM = 127 };

// Doubly linked list of thinkers
class DThinker : public DObject, protected Node
{
	DECLARE_CLASS (DThinker, DObject)

public:
	DThinker (int statnum = MAX_STATNUM) throw();
	void Destroy ();
	virtual ~DThinker ();
	virtual void RunThink ();
	virtual void PostBeginPlay ();	// Called just before the first tick

	void *operator new (size_t size);
	void operator delete (void *block);

	void ChangeStatNum (int statnum);

	static void RunThinkers ();
	static void RunThinkers (int statnum);
	static void DestroyAllThinkers ();
	static void DestroyMostThinkers ();
	static void SerializeAll (FArchive &arc, bool keepPlayers);

private:
	static void DestroyThinkersInList (Node *first);
	static void DestroyMostThinkersInList (List &list, int stat);
	static int TickThinkers (List *list, List *dest);	// Returns: # of thinkers ticked


	static List Thinkers[MAX_STATNUM+1];		// Current thinkers
	static List FreshThinkers[MAX_STATNUM+1];	// Newly created thinkers
	static bool bSerialOverride;

	friend class FThinkerIterator;
	friend class DObject;
};

class FThinkerIterator
{
private:
	TypeInfo *m_ParentType;
	Node *m_CurrThinker;
	BYTE m_Stat;
	bool m_SearchStats;
	bool m_SearchingFresh;

public:
	FThinkerIterator (TypeInfo *type, int statnum=MAX_STATNUM+1);
	DThinker *Next ();
	void Reinit ();
};

template <class T> class TThinkerIterator : public FThinkerIterator
{
public:
	TThinkerIterator (int statnum=MAX_STATNUM+1) : FThinkerIterator (RUNTIME_CLASS(T), statnum)
	{
	}
	T *Next ()
	{
		return static_cast<T *>(FThinkerIterator::Next ());
	}
};

#endif //__DTHINKER_H__
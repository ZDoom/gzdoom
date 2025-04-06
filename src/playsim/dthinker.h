/*
** dthinker.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifndef __DTHINKER_H__
#define __DTHINKER_H__

#include <stdlib.h>
#include "dobject.h"
#include "statnums.h"

class AActor;
class player_t;
struct pspdef_s;
struct FState;
class DThinker;
class FSerializer;
struct FLevelLocals;

class FThinkerIterator;

enum { MAX_STATNUM = 127 };

// Doubly linked ring list of thinkers
struct FThinkerList
{
	// No destructor. If this list goes away it's the GC's task to clean the orphaned thinkers. Otherwise this may clash with engine shutdown.
	void AddTail(DThinker *thinker);
	DThinker *GetHead() const;
	DThinker *GetTail() const;
	bool IsEmpty() const;
	void DestroyThinkers();
	bool DoDestroyThinkers();
	int TickThinkers(FThinkerList *dest);	// Returns: # of thinkers ticked
	int ProfileThinkers(FThinkerList *dest);
	void SaveList(FSerializer &arc);

private:
	DThinker *Sentinel = nullptr;

	friend struct FThinkerCollection;
};

struct FThinkerCollection
{
	void DestroyThinkersInList(int statnum)
	{
		Thinkers[statnum].DestroyThinkers();
		FreshThinkers[statnum].DestroyThinkers();
	}

	void RunThinkers(FLevelLocals *Level);	// The level is needed to tick the lights
	void RunClientsideThinkers(FLevelLocals* Level);
	void DestroyAllThinkers(bool fullgc = true);
	void SerializeThinkers(FSerializer &arc, bool keepPlayers);
	void MarkRoots();
	DThinker *FirstThinker(int statnum);
	void Link(DThinker *thinker, int statnum);

private:
	FThinkerList Thinkers[MAX_STATNUM + 2];
	FThinkerList FreshThinkers[MAX_STATNUM + 1];

	friend class FThinkerIterator;
};

class DThinker : public DObject
{
	DECLARE_CLASS (DThinker, DObject)
public:
	static const int DEFAULT_STAT = STAT_DEFAULT;
	void OnDestroy () override;
	virtual ~DThinker ();
	virtual void Tick ();
	void CallTick();
	virtual void PostBeginPlay ();	// Called just before the first tick
	virtual void CallPostBeginPlay(); // different in actor.
	virtual void PostSerialize();
	void CallPostSerialize();
	void Serialize(FSerializer &arc) override;
	size_t PropagateMark();
	
	void ChangeStatNum (int statnum);

private:
	void Remove();

	friend struct FThinkerList;
	friend struct FThinkerCollection;
	friend class FThinkerIterator;
	friend class DObject;
	friend class FDoomSerializer;

	DThinker *NextThinker = nullptr, *PrevThinker = nullptr;

public:
	FLevelLocals *Level;

	friend struct FLevelLocals;	// Needs access to FreshThinkers until the thinker storage gets refactored.
};

class FThinkerIterator
{
protected:
	const PClass *m_ParentType;
private:
	FLevelLocals *Level;
	FThinkerCollection* m_ThinkerPool;
	DThinker *m_CurrThinker;
	uint8_t m_Stat;
	bool m_SearchStats;
	bool m_SearchingFresh;

public:
	FThinkerIterator (FLevelLocals *Level, const PClass *type, int statnum=MAX_STATNUM+1, bool clientside = false);
	FThinkerIterator (FLevelLocals *Level, const PClass *type, int statnum, DThinker *prev, bool clientside = false);
	DThinker *Next (bool exact = false);
	void Reinit ();
};

template <class T> class TThinkerIterator : public FThinkerIterator
{
public:
	TThinkerIterator (FLevelLocals *Level, int statnum=MAX_STATNUM+1, bool clientside = false) : FThinkerIterator (Level, RUNTIME_CLASS(T), statnum, clientside)
	{
	}
	TThinkerIterator (FLevelLocals *Level, int statnum, DThinker *prev, bool clientside = false) : FThinkerIterator (Level, RUNTIME_CLASS(T), statnum, prev, clientside)
	{
	}
	TThinkerIterator (FLevelLocals *Level, const PClass *subclass, int statnum=MAX_STATNUM+1, bool clientside = false) : FThinkerIterator(Level, subclass, statnum, clientside)
	{
	}
	TThinkerIterator (FLevelLocals *Level, FName subclass, int statnum=MAX_STATNUM+1, bool clientside = false) : FThinkerIterator(Level, PClass::FindClass(subclass), statnum, clientside)
	{
	}
	TThinkerIterator (FLevelLocals *Level, FName subclass, int statnum, DThinker *prev, bool clientside = false) : FThinkerIterator(Level, PClass::FindClass(subclass), statnum, prev, clientside)
	{
	}
	T *Next (bool exact = false)
	{
		return static_cast<T *>(FThinkerIterator::Next (exact));
	}
};


#endif //__DTHINKER_H__

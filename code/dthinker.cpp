#include "dthinker.h"
#include "z_zone.h"
#include "stats.h"
#include "p_local.h"

static cycle_t ThinkCycles, BotSupportCycles;

// Cap is both the head and tail of the thinker list
DThinker DThinker::Cap;

IMPLEMENT_SERIAL (DThinker, DObject)

void DThinker::Serialize (FArchive &arc)
{
	Super::Serialize (arc);

	// We do not serialize the m_Next and m_Prev members here, because
	// the DThinker constructor sets those for us, and because the thinker
	// list contains an entry that should never be saved (Cap). We rely on
	// SerializeAll to be smart enough to serialize all the thinkers for us.

	if (arc.IsStoring ())
		arc << m_RemoveMe;
	else
		arc >> m_RemoveMe;
}

void DThinker::SerializeAll (FArchive &arc, bool hubLoad)
{
	DThinker *thinker;

	if (arc.IsStoring ())
	{
		// Before saving any thinkers, first clear out the ones that want
		// to be destroyed, because they no longer retain their type
		// information and will try to save themselves as DObjects.

		thinker = Cap.m_Next;
		while (thinker != &Cap)
		{
			DThinker *next = thinker->m_Next;
			if (thinker->m_RemoveMe)
				thinker->Remove ();
			thinker = next;
		}

		thinker = Cap.m_Next;
		while (thinker != &Cap)
		{
			arc << (BYTE)1;
			arc << thinker;
			thinker = thinker->m_Next;
		}
		arc << (BYTE)0;
	}
	else
	{
		if (hubLoad)
			DestroyMostThinkers ();
		else
			DestroyAllThinkers ();

		BYTE more;
		arc >> more;
		while (more)
		{
			DThinker *thinker;
			arc >> thinker;
			arc >> more;
		}

		// killough 3/26/98: Spawn icon landings:
		P_SpawnBrainTargets ();
	}
}

DThinker::DThinker ()
{
	m_RemoveMe = false;
	if (this != &Cap)
	{
		// Add a new thinker at the end of the list.
		Cap.m_Prev->m_Next = this;
		m_Next = &Cap;
		m_Prev = Cap.m_Prev;
		Cap.m_Prev = this;
	}
	else
		InitThinkers ();
}

DThinker::~DThinker ()
{
}

bool DThinker::IsValid () const
{
	return !m_RemoveMe;
}

// Destroy every thinker
void DThinker::DestroyAllThinkers ()
{
	DThinker *currentthinker = Cap.m_Next;
	while (currentthinker != &Cap)
	{
		DThinker *next = currentthinker->m_Next;
		currentthinker->~DThinker ();
		currentthinker->m_Next->m_Prev = currentthinker->m_Prev;
		currentthinker->m_Prev->m_Next = currentthinker->m_Next;
		Z_Free (currentthinker);
		currentthinker = next;
	}
}

// Destroy all thinkers except for player-controlled actors
void DThinker::DestroyMostThinkers ()
{
	DThinker *thinker = Cap.m_Next;
	while (thinker != &Cap)
	{
		DThinker *next = thinker->m_Next;
		if (!thinker->IsKindOf (RUNTIME_CLASS (AActor)) ||
			static_cast<AActor *>(thinker)->player == NULL ||
			static_cast<AActor *>(thinker)->player->mo
			 != static_cast<AActor *>(thinker))
		{
			thinker->~DThinker ();
			thinker->m_Next->m_Prev = thinker->m_Prev;
			thinker->m_Prev->m_Next = thinker->m_Next;
			Z_Free (thinker);
		}
		thinker = next;
	}
}

void DThinker::Remove ()
{
	m_Next->m_Prev = m_Prev;
	m_Prev->m_Next = m_Next;
	Z_Free (this);
}

void DThinker::InitThinkers ()
{
	Cap.m_Prev = Cap.m_Next = &Cap;
}

void DThinker::RunThinkers ()
{
	DThinker *currentthinker;

	ThinkCycles = BotSupportCycles = 0;

	clock (ThinkCycles);
	currentthinker = Cap.m_Next;
	while (currentthinker != &Cap)
	{
		if (currentthinker->m_RemoveMe)
		{
			// time to remove it
			DThinker *nextthinker = currentthinker->m_Next;
			currentthinker->Remove ();
			currentthinker = nextthinker;
		}
		else
		{
			currentthinker->RunThink ();
			currentthinker = currentthinker->m_Next;
		}
	}
	unclock (ThinkCycles);
}

void *DThinker::operator new (size_t size)
{
	return Z_Malloc (size, PU_LEVSPEC, 0);
}

// Deallocation is lazy -- it will not actually be freed
// until its thinking turn comes up.
void DThinker::operator delete (void *mem)
{
	static_cast<DThinker *>(mem)->m_RemoveMe = true;
}

BEGIN_STAT (think)
{
	sprintf (out, "Think time = %04.1f ms",
		SecondsPerCycle * (double)ThinkCycles * 1000);
}
END_STAT (think)
#include "dthinker.h"
#include "z_zone.h"
#include "stats.h"
#include "p_local.h"
#include "statnums.h"

static cycle_t ThinkCycles;
extern cycle_t BotSupportCycles;

IMPLEMENT_CLASS (DThinker)

static Node *NextToThink;

List DThinker::Thinkers[MAX_STATNUM+1];
bool DThinker::bSerialOverride = false;

void DThinker::SerializeAll (FArchive &arc, bool hubLoad)
{
	Node *node;
	DThinker *thinker;
	BYTE stat;
	int statcount;
	int i;

	// Save lists of thinkers, but not by storing the first one and letting
	// the archiver catch the rest. (Which leads to buttloads of recursion
	// and makes the file larger.) Instead, we explicitly save each thinker
	// in sequence. When restoring an archive, we also have to maintain
	// the thinker lists here instead of relying an the archiver to do it
	// for us.

	if (arc.IsStoring ())
	{
		for (statcount = i = 0; i <= MAX_STATNUM; i++)
		{
			if (!Thinkers[i].IsEmpty ())
			{
				statcount++;
			}
		}
		arc << statcount;
		for (i = 0; i <= MAX_STATNUM; i++)
		{
			node = Thinkers[i].Head;
			if (node->Succ != NULL)
			{
				stat = i;
				arc << stat;
				do
				{
					thinker = static_cast<DThinker *> (node);
					arc << thinker;
					node = node->Succ;
				} while (node->Succ != NULL);
				thinker = NULL;
				arc << thinker;		// Save a final NULL for this list
			}
		}
	}
	else
	{
		if (hubLoad)
			DestroyMostThinkers ();
		else
			DestroyAllThinkers ();

		// Prevent the constructor from inserting thinkers into a list.
		bSerialOverride = true;
		arc << statcount;
		while (statcount > 0)
		{
			arc << stat << thinker;
			while (thinker != NULL)
			{
				Thinkers[stat].AddTail (thinker);
				arc << thinker;
			}
			statcount--;
		}
		bSerialOverride = false;
	}
}

DThinker::DThinker (int statnum)
{
	if (bSerialOverride)
	{ // The serializer will insert us into the right list
		Succ = NULL;
		return;
	}

	if ((unsigned)statnum > MAX_STATNUM)
	{
		statnum = MAX_STATNUM;
	}
	Thinkers[statnum].AddTail (this);
}

DThinker::~DThinker ()
{
	if (Succ != NULL)
	{
		Remove ();
	}
}

void DThinker::Destroy ()
{
	if (this == NextToThink)
		NextToThink = Succ;

	if (Succ != NULL)
	{
		Remove ();
		Succ = NULL;
	}
	Super::Destroy ();
}

void DThinker::ChangeStatNum (int statnum)
{
	if ((unsigned)statnum > MAX_STATNUM)
	{
		statnum = MAX_STATNUM;
	}
	Remove ();
	Thinkers[statnum].AddTail (this);
}

// Destroy every thinker
void DThinker::DestroyAllThinkers ()
{
	int i;

	for (i = 0; i <= MAX_STATNUM; i++)
	{
		Node *node = Thinkers[i].Head;
		while (node->Succ != NULL)
		{
			Node *next = node->Succ;
			DObject::BeginFrame ();
			static_cast<DThinker *> (node)->Destroy ();
			DObject::EndFrame ();
			node = next;
		}
		// Should already be empty from the Destroys
		//Thinkers[i].MakeEmpty ();
	}
}

// Destroy all thinkers except for player-controlled actors
// Players are simply removed from the list of thinkers and
// will be added back after serialization is complete.
void DThinker::DestroyMostThinkers ()
{
	int i;

	for (i = 0; i <= MAX_STATNUM; i++)
	{
		if (i != STAT_PLAYER)
		{
			Node *node = Thinkers[i].Head;
			while (node->Succ != NULL)
			{
				Node *next = node->Succ;
				DObject::BeginFrame ();
				static_cast<DThinker *> (node)->Destroy ();
				DObject::EndFrame ();
				node = next;
			}
		}
		else
		{
			Node *node = Thinkers[i].Head;
			while (node->Succ != NULL)
			{
				Node *next = node->Succ;
				node->Remove ();
				node = next;
			}
		}
	}
}

void DThinker::RunThinkers ()
{
	int i;

	ThinkCycles = BotSupportCycles = 0;

	clock (ThinkCycles);
	for (i = STAT_FIRST_THINKING; i <= MAX_STATNUM; i++)
	{
		Node *node = Thinkers[i].Head;
		while (node->Succ != NULL)
		{
			NextToThink = node->Succ;
			DThinker *thinker = static_cast<DThinker *> (node);
			if (thinker->ObjectFlags & OF_JustSpawned)
			{	// OF_JustSpawned is valid with actors only
				thinker->ObjectFlags &= ~OF_JustSpawned;
				static_cast<AActor *> (thinker)->PostBeginPlay ();
				if (thinker->ObjectFlags & OF_MassDestruction)
				{ // object destroyed itself
					return;
				}
			}
	//		Printf (PRINT_HIGH, "%d: %s\n", gametic, RUNTIME_TYPE(thinker)->Name);
			thinker->RunThink ();
			node = NextToThink;
		}
	}
	unclock (ThinkCycles);
}

void DThinker::RunThink ()
{
}

void *DThinker::operator new (size_t size)
{
	return Z_Malloc (size, PU_LEVSPEC, 0);
}

// Deallocation is lazy -- it will not actually be freed
// until its thinking turn comes up.
void DThinker::operator delete (void *mem)
{
	Z_Free (mem);
}

FThinkerIterator::FThinkerIterator (TypeInfo *type, int statnum)
{
	if ((unsigned)statnum > MAX_STATNUM)
	{
		m_Stat = STAT_FIRST_THINKING;
		m_SearchStats = true;
	}
	else
	{
		m_Stat = statnum;
		m_SearchStats = false;
	}
	m_ParentType = type;
	m_CurrThinker = DThinker::Thinkers[m_Stat].Head;
}

void FThinkerIterator::Reinit ()
{
	m_CurrThinker = DThinker::Thinkers[m_Stat].Head;
}

DThinker *FThinkerIterator::Next ()
{
	do
	{
		while (m_CurrThinker->Succ)
		{
			DThinker *thinker = static_cast<DThinker *> (m_CurrThinker);
			if (thinker->IsKindOf (m_ParentType))
			{
				m_CurrThinker = m_CurrThinker->Succ;
				return thinker;
			}
			m_CurrThinker = m_CurrThinker->Succ;
		}
		if (m_SearchStats)
		{
			m_Stat++;
			if (m_Stat > MAX_STATNUM)
			{
				m_Stat = STAT_FIRST_THINKING;
			}
		}
		m_CurrThinker = DThinker::Thinkers[m_Stat].Head;
	} while (m_SearchStats && m_Stat != STAT_FIRST_THINKING);
	return NULL;
}

ADD_STAT (think, out)
{
	sprintf (out, "Think time = %04.1f ms",
		SecondsPerCycle * (double)ThinkCycles * 1000);
}

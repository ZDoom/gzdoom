//-----------------------------------------------------------------------------
//
// Copyright 2018 Benjamin Berkels
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//

#include "d_player.h"
#include "netsync.h"
#include "c_dispatch.h"
#include "actor.h"
#include "doomstat.h"
#include "i_net.h"

extern bool netserver, netclient;

IDList<AActor> g_NetIDList;

void CountActors();

void NetSyncData::AssignNetID(AActor *pActor)
{
	if (netserver)
	{
		NetID = g_NetIDList.getNewID();
		g_NetIDList.useID(NetID, pActor);
	}
	else
		NetID = -1;
}

void NetSyncData::FreeNetID()
{
	g_NetIDList.freeID(NetID);
}

template <typename T>
void IDList<T>::clear()
{
	for (unsigned int ulIdx = 0; ulIdx < MAX_NETID; ulIdx++)
		freeID(ulIdx);

	_firstFreeID = 1;
}

template <typename T>
void IDList<T>::rebuild()
{
	clear();

	T *pActor;

	TThinkerIterator<T> it;

	while ((pActor = it.Next()))
	{
		if ((pActor->syncdata.NetID > 0) && (pActor->syncdata.NetID < MAX_NETID))
			useID(pActor->syncdata.NetID, pActor);
	}
}

template <typename T>
void IDList<T>::useID(const int lNetID, T *pActor)
{
	if (isIndexValid(lNetID))
	{
		if ((_entries[lNetID].bFree == false) && (_entries[lNetID].pActor != pActor))
			Printf("IDList<T>::useID is using an already used ID.\n");

		_entries[lNetID].bFree = false;
		_entries[lNetID].pActor = pActor;
	}
}

template <typename T>
unsigned int IDList<T>::getNewID()
{
	// Actor's network ID is the first availible net ID.
	unsigned int ulID = _firstFreeID;

	do
	{
		_firstFreeID++;
		if (_firstFreeID >= MAX_NETID)
			_firstFreeID = 1;

		if (_firstFreeID == ulID)
		{
			// [BB] In case there is no free netID, the server has to abort the current game.
			if (netserver)
			{
				// [BB] We can only spawn (MAX_NETID-2) actors with netID, because ID zero is reserved and
				// we already check that a new ID for the next actor is available when assign a net ID.
				Printf("ACTOR_GetNewNetID: Network ID limit reached (>=%d actors)\n", MAX_NETID - 1);
				CountActors();
				I_Error("Network ID limit reached (>=%d actors)!\n", MAX_NETID - 1);
			}

			return (0);
		}
	} while (_entries[_firstFreeID].bFree == false);

	return (ulID);
}

template class IDList<AActor>;

//*****************************************************************************

void CountActors()
{
	TMap<FName, int> actorCountMap;
	AActor * mo;
	int numActors = 0;
	int numActorsWithNetID = 0;

	TThinkerIterator<AActor> it;

	while ((mo = it.Next()))
	{
		numActors++;
		if (mo->syncdata.NetID > 0)
			numActorsWithNetID++;
		const FName curName = mo->GetClass()->TypeName.GetChars();
		if (actorCountMap.CheckKey(curName) == NULL)
			actorCountMap.Insert(curName, 1);
		else
			actorCountMap[curName] ++;
	}

	const TMap<FName, int>::Pair *pair;

	Printf("%d actors in total found, %d have a NetID. Detailed listing:\n", numActors, numActorsWithNetID);

	TMap<FName, int>::ConstIterator mapit(actorCountMap);
	while (mapit.NextPair(pair))
	{
		Printf("%s %d\n", pair->Key.GetChars(), pair->Value);
	}
}

CCMD(countactors)
{
	CountActors();
}

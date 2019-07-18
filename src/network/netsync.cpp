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
#include "cmdlib.h"
#include "g_levellocals.h"

extern bool netserver;

void CountActors();

class NetSyncWriter
{
public:
	NetSyncWriter(NetCommand &cmd, int netid) : cmd(cmd), NetID(netid) { }

	void Write(void *compval, void *actorval, size_t size)
	{
		if (memcmp(compval, actorval, size) != 0)
		{
			if (firstwrite)
			{
				firstwrite = false;
				cmd.AddShort(NetID);
			}
			cmd.AddByte(fieldindex);
			cmd.AddBuffer(actorval, (int)size);
			memcpy(compval, actorval, size);
		}
		fieldindex++;
	}

	void WriteEnd()
	{
		if (!firstwrite)
		{
			cmd.AddByte(255);
		}
	}

private:
	NetCommand &cmd;
	int NetID;
	bool firstwrite = true;
	int fieldindex = 0;
};

NetSyncClass::NetSyncClass()
{
	mSyncVars.Push({ myoffsetof(AActor, Vel.X), sizeof(double) });
	mSyncVars.Push({ myoffsetof(AActor, Vel.Y), sizeof(double) });
	mSyncVars.Push({ myoffsetof(AActor, Vel.Z), sizeof(double) });
	mSyncVars.Push({ myoffsetof(AActor, SpriteAngle.Degrees), sizeof(double) });
	mSyncVars.Push({ myoffsetof(AActor, SpriteRotation.Degrees), sizeof(double) });
	mSyncVars.Push({ myoffsetof(AActor, Angles.Yaw.Degrees), sizeof(double) });
	mSyncVars.Push({ myoffsetof(AActor, Angles.Pitch.Degrees), sizeof(double) });
	mSyncVars.Push({ myoffsetof(AActor, Angles.Roll.Degrees), sizeof(double) });
	mSyncVars.Push({ myoffsetof(AActor, Scale.X), sizeof(double) });
	mSyncVars.Push({ myoffsetof(AActor, Scale.Y), sizeof(double) });
	mSyncVars.Push({ myoffsetof(AActor, Alpha), sizeof(double) });
	mSyncVars.Push({ myoffsetof(AActor, sprite), sizeof(uint32_t) });
	mSyncVars.Push({ myoffsetof(AActor, frame), sizeof(uint8_t) });
	mSyncVars.Push({ myoffsetof(AActor, effects), sizeof(uint8_t) });
	mSyncVars.Push({ myoffsetof(AActor, RenderStyle.AsDWORD), sizeof(uint32_t) });
	mSyncVars.Push({ myoffsetof(AActor, Translation), sizeof(uint32_t) });
	mSyncVars.Push({ myoffsetof(AActor, RenderRequired), sizeof(uint32_t) });
	mSyncVars.Push({ myoffsetof(AActor, RenderHidden), sizeof(uint32_t) });
	mSyncVars.Push({ myoffsetof(AActor, renderflags.Value), sizeof(uint32_t) });
	mSyncVars.Push({ myoffsetof(AActor, Floorclip), sizeof(double) });
	mSyncVars.Push({ myoffsetof(AActor, VisibleStartAngle.Degrees), sizeof(double) });
	mSyncVars.Push({ myoffsetof(AActor, VisibleStartPitch.Degrees), sizeof(double) });
	mSyncVars.Push({ myoffsetof(AActor, VisibleEndAngle.Degrees), sizeof(double) });
	mSyncVars.Push({ myoffsetof(AActor, VisibleEndPitch.Degrees), sizeof(double) });
	mSyncVars.Push({ myoffsetof(AActor, Speed), sizeof(double) });
	mSyncVars.Push({ myoffsetof(AActor, FloatSpeed), sizeof(double) });
	mSyncVars.Push({ myoffsetof(AActor, CameraHeight), sizeof(double) });
	mSyncVars.Push({ myoffsetof(AActor, CameraFOV), sizeof(double) });
	mSyncVars.Push({ myoffsetof(AActor, StealthAlpha), sizeof(double) });

	// To do: create NetSyncVariable entries for all the script variables we want sent to the client
}

void NetSyncClass::InitSyncData(AActor *actor)
{
	auto &syncdata = actor->syncdata;

	size_t scriptvarsize = 0;
	for (unsigned int i = 0; i < mSyncVars.Size(); i++)
		scriptvarsize += mSyncVars[i].size;
	syncdata.CompareData.Resize((unsigned int)scriptvarsize);

	syncdata.Pos = actor->Pos();

	size_t pos = 0;
	for (unsigned int i = 0; i < mSyncVars.Size(); i++)
	{
		memcpy(syncdata.CompareData.Data() + pos, ((uint8_t*)actor) + mSyncVars[i].offset, scriptvarsize);
		pos += mSyncVars[i].size;
	}
}

void NetSyncClass::WriteSyncUpdate(NetCommand &cmd, AActor *actor)
{
	NetSyncWriter writer(cmd, actor->syncdata.NetID);

	DVector3 pos = actor->Pos();
	writer.Write(&actor->syncdata.Pos, &pos, sizeof(DVector3));

	size_t compareoffset = 0;
	for (unsigned int i = 0; i < mSyncVars.Size(); i++)
	{
		writer.Write(actor->syncdata.CompareData.Data() + compareoffset, ((uint8_t*)actor) + mSyncVars[i].offset, mSyncVars[i].size);
		compareoffset += mSyncVars[i].size;
	}

	writer.WriteEnd();
}

void NetSyncClass::ReadSyncUpdate(ByteInputStream &stream, AActor *actor)
{
	while (true)
	{
		int fieldindex = stream.ReadByte();
		if (fieldindex == 255)
			break;

		if (fieldindex == 0)
		{
			DVector3 pos;
			stream.ReadBuffer(&pos, sizeof(DVector3));
			actor->SetOrigin(pos, true);
		}
		else if (fieldindex <= (int)mSyncVars.Size())
		{
			stream.ReadBuffer(((uint8_t*)actor) + mSyncVars[fieldindex - 1].offset, mSyncVars[fieldindex - 1].size);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

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

	TThinkerIterator<T> it = primaryLevel->GetThinkerIterator<T>();

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

	TThinkerIterator<AActor> it = primaryLevel->GetThinkerIterator<AActor>();

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

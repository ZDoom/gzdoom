
#pragma once

#include "vectors.h"
#include "r_data/renderstyle.h"

class AActor;
class NetCommand;
class ByteInputStream;

class NetSyncVariable
{
public:
	size_t offset;
	size_t size;
};

class NetSyncClass
{
public:
	NetSyncClass();

	void InitSyncData(AActor *actor);
	void WriteSyncUpdate(NetCommand &cmd, AActor *actor);
	void ReadSyncUpdate(ByteInputStream &stream, AActor *actor);

private:
	TArray<NetSyncVariable> mSyncVars;

	NetSyncClass(const NetSyncClass &) = delete;
	NetSyncClass &operator=(const NetSyncClass &) = delete;
};

class NetSyncData
{
public:
	int NetID;
	DVector3 Pos;
	TArray<uint8_t> CompareData;
	NetSyncClass *SyncClass; // Maybe this should be stored in the actor's PClass
};

//==========================================================================
//
// IDList
//
// Manages IDs to reference a certain type of objects over the network.
// Since it still mimics the old Actor ID mechanism, 0 is never assigned as
// ID.
//
// @author Benjamin Berkels
//
//==========================================================================

template <typename T>
class IDList
{
public:
	const static int MAX_NETID = 32768;

private:
	// List of all possible network ID's for an actor. Slot is true if it available for use.
	typedef struct
	{
		// Is this node occupied, or free to be used by a new actor?
		bool bFree;

		// If this node is occupied, this is the actor occupying it.
		T *pActor;

	} IDNODE_t;

	IDNODE_t _entries[MAX_NETID];
	unsigned int _firstFreeID;

	inline bool isIndexValid(const int lNetID) const
	{
		return (lNetID >= 0) && (lNetID < MAX_NETID);
	}
public:
	void clear();

	// [BB] Rebuild the global list of used / free NetIDs from scratch.
	void rebuild();

	IDList()
	{
		clear();
	}

	void useID(const int lNetID, T *pActor);

	void freeID(const int lNetID)
	{
		if (isIndexValid(lNetID))
		{
			_entries[lNetID].bFree = true;
			_entries[lNetID].pActor = nullptr;
		}
	}

	unsigned int getNewID();

	T* findPointerByID(const int lNetID) const
	{
		if (isIndexValid(lNetID) == false)
			return nullptr;

		if ((_entries[lNetID].bFree == false) && (_entries[lNetID].pActor))
			return _entries[lNetID].pActor;

		return nullptr;
	}
};

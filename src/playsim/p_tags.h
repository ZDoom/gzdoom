#ifndef P_TAGS_H
#define P_TAGS_H 1

#include "r_defs.h"
#include "r_state.h"

struct FTagItem
{
	int target;		// either sector or line
	int tag;
	int nexttag;	// for hashing
};

class FSectorTagIterator;
class FLineIdIterator;
struct FLevelLocals;

class FTagManager
{
	enum
	{
		TAG_HASH_SIZE = 256
	};

	// Only the iterators and the map loader, including its helpers may access this. Everything else should go through FLevelLocals's interface.
	friend class FSectorTagIterator;
	friend class FLineIdIterator;
	friend class MapLoader;
	friend struct FLevelLocals;
	friend class UDMFParser;

	FLevelLocals *Level;
	TArray<FTagItem> allTags;
	TArray<FTagItem> allIDs;
	TArray<int> startForSector;
	TArray<int> startForLine;
	int TagHashFirst[TAG_HASH_SIZE];
	int IDHashFirst[TAG_HASH_SIZE];

	bool SectorHasTags(int sect) const
	{
		return sect >= 0 && sect < (int)startForSector.Size() && startForSector[sect] >= 0;
	}

	bool LineHasIDs(int sect) const
	{
		return sect >= 0 && sect < (int)startForLine.Size() && startForLine[sect] >= 0;
	}


    FTagManager(FLevelLocals *l) : Level(l) {}
	void Clear()
	{
		allTags.Clear();
		allIDs.Clear();
		startForSector.Clear();
		startForLine.Clear();
		memset(TagHashFirst, -1, sizeof(TagHashFirst));
		memset(IDHashFirst, -1, sizeof(IDHashFirst));
	}

	bool SectorHasTags(const sector_t *sector) const;
	int GetFirstSectorTag(const sector_t *sect) const;
	int GetFirstSectorTag(int sect) const;
	bool SectorHasTag(int sector, int tag) const;
	bool SectorHasTag(const sector_t *sector, int tag) const;

	int GetFirstLineID(const line_t *line) const;
	bool LineHasID(int line, int id) const;
	bool LineHasID(const line_t *line, int id) const;

	void HashTags();
public:	// The ones below are called by functions that cannot be declared as friend.
	void AddSectorTag(int sector, int tag);
	void AddLineID(int line, int tag);
	void RemoveSectorTags(int sect);
	void RemoveLineIDs(int line);

	void DumpTags();

	int CountSectorTags(const sector_t *sector);
	int GetSectorTag(const sector_t *sector, int index);

	int CountLineIDs(const line_t *line);
	int GetLineID(const line_t *line, int index);
};

class FSectorTagIterator
{
	friend struct FLevelLocals;
protected:
	int searchtag;
	int start;
	FTagManager &tagManager;

	FSectorTagIterator(FTagManager &tm) : tagManager(tm)
	{
		// For DSectorTagIterator
	}

	void Init(int tag)
	{
		searchtag = tag;
		start = tag == 0 ? 0 : tagManager.TagHashFirst[((unsigned int)tag) % FTagManager::TAG_HASH_SIZE];
	}

	void Init(int tag, line_t *line)
	{
		if (tag == 0)
		{
			searchtag = INT_MIN;
			start = (line == NULL || line->backsector == NULL) ? -1 : line->backsector->Index();
		}
		else
		{
			searchtag = tag;
			start = tagManager.TagHashFirst[((unsigned int)tag) % FTagManager::TAG_HASH_SIZE];
		}
	}

	FSectorTagIterator(FTagManager &tm, int tag) : tagManager(tm)
	{
		Init(tag);
	}

	// Special constructor for actions that treat tag 0 as  'back of activation line'
	FSectorTagIterator(FTagManager &tm, int tag, line_t *line) : tagManager(tm)
	{
		Init(tag, line);
	}
public:
	int Next();
	int NextCompat(bool compat, int secnum);
};

class FLineIdIterator
{
	friend struct FLevelLocals;
protected:
	int searchtag;
	int start;
	FTagManager &tagManager;

	FLineIdIterator(FTagManager &tm, int id) : tagManager(tm)
	{
		searchtag = id;
		start = tagManager.IDHashFirst[((unsigned int)id) % FTagManager::TAG_HASH_SIZE];
	}

public:
	int Next();
};


#endif

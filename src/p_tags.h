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

class FTagManager
{
	enum
	{
		TAG_HASH_SIZE = 256
	};

	friend class FSectorTagIterator;
	friend class FLineIdIterator;

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

public:
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
	bool SectorHasTag(int sector, int tag) const;
	bool SectorHasTag(const sector_t *sector, int tag) const;

	bool LineHasID(int line, int id) const;
	bool LineHasID(const line_t *line, int id) const;

	void HashTags();
	void AddSectorTag(int sector, int tag);
	void AddLineID(int line, int tag);
	void RemoveSectorTags(int sect);

	void DumpTags();
};

extern FTagManager tagManager;

class FSectorTagIterator
{
protected:
	int searchtag;
	int start;

public:
	FSectorTagIterator(int tag)
	{
		searchtag = tag;
		start = tag == 0 ? 0 : tagManager.TagHashFirst[((unsigned int)tag) % FTagManager::TAG_HASH_SIZE];
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
			start = tagManager.TagHashFirst[((unsigned int)tag) % FTagManager::TAG_HASH_SIZE];
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
		start = tagManager.IDHashFirst[((unsigned int)id) % FTagManager::TAG_HASH_SIZE];
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

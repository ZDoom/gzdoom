#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include "zstring.h"

string::PoolGroup string::Pond;

#ifndef NOPOOLS
struct string::Pool
{
	// The pool's performance (and thus the string class's performance) is
	// controlled via these two constants. A small pool size will result
	// in more frequent garbage collection, while a large pool size will
	// result in longer garbage collection. A large pool can also end up
	// wasting memory. Something that's not too small and not too large
	// is ideal. Similarly, making the granularity too big will also result
	// in more frequent garbage collection. But if you do a lot of
	// concatenation with the += operator, then a large granularity is good
	// because it gives the string more room to grow without needing to
	// be reallocated.
	//
	// Note that the granularity must be a power of 2. The pool size need
	// not be, although it's best to make it a multiple of the granularity.
	enum { POOL_SIZE = 64*1024 };
	enum { BLOCK_GRANULARITY = 16 };

	Pool (size_t minSize);
	~Pool ();
	char *Alloc (string *owner, size_t len);
	char *Realloc (char *chars, size_t newlen);
	void Free (char *chars);
	void MergeFreeBlocks (StringHeader *block);
	void CollectGarbage (bool noGenerations);
	bool BigEnough (size_t len) const;
	size_t RoundLen (size_t len) const;

	Pool *Next;
	size_t FreeSpace;
	char *PoolData;
	char *MaxAlloc;
	StringHeader *NextAlloc;
	StringHeader *GarbageStart;
	int GenerationNum;
};

// The PoolGroup does not get a constructor, because there is no way to
// guarantee it will be constructed before any strings that need it.
// Instead, we rely on the loader to initialize Pools to NULL for us.

string::PoolGroup::~PoolGroup ()
{
	int count = 0;
	Pool *pool = Pools, *next;
	while (pool != NULL)
	{
		count++;
		next = pool->Next;
		delete pool;
		pool = next;
	}
	Pools = NULL;
}

char *string::PoolGroup::Alloc (string *owner, size_t len)
{
	char *mem;
	Pool *pool, *best, **prev, **bestprev;

	// If no pools, create one
	if (Pools == NULL)
	{
		Pools = new string::Pool (len);
	}

	// Try to allocate space from an existing pool
	for (pool = Pools; pool != NULL; pool = pool->Next)
	{
		mem = pool->Alloc (owner, len);
		if (mem != NULL)
		{
			return mem;
		}
	}

	// Compact the pool with the most free space and try again
	best = Pools;
	bestprev = &Pools;
	pool = best->Next;
	prev = &best->Next;
	while (pool != NULL)
	{
		if (pool->FreeSpace > best->FreeSpace)
		{
			bestprev = prev;
			best = pool;
		}
		prev = &pool->Next;
		pool = pool->Next;
	}
	if (best->BigEnough (len))
	{
		best->CollectGarbage (false);
		mem = best->Alloc (owner, len);
		if (mem == NULL)
		{
			best->CollectGarbage (true);
			mem = best->Alloc (owner, len);
		}
		// Move the pool to the front of the list
		*bestprev = best->Next;
		best->Next = Pools;
		Pools = best;
	}
	else
	{
		// No pools were large enough to hold the string, so create a new one
		pool = new string::Pool (len);
		pool->Next = Pools;
		Pools = pool;
		mem = pool->Alloc (owner, len);
	}
	return mem;
}

char *string::PoolGroup::Realloc (string *owner, char *chars, size_t newlen)
{
	if (chars == NULL)
	{
		chars = Alloc (owner, newlen);
		if (chars != NULL)
		{
			chars[0] = '\0';
		}
		return chars;
	}
	Pool *pool = FindPool (chars);
	char *newchars = pool->Realloc (chars, newlen);
	if (newchars == NULL)
	{
		newchars = Alloc (owner, newlen);
		if (newchars != NULL)
		{
			StrCopy (newchars, chars, GetHeader (chars)->Len);
			pool->Free (chars);
		}
	}
	return newchars;
}

void string::PoolGroup::Free (char *chars)
{
	Pool *pool = FindPool (chars);
	pool->Free (chars);
}

string::Pool *string::PoolGroup::FindPool (char *chars) const
{
	Pool *pool = Pools;
	while (pool != NULL)
	{
		if (pool->PoolData <= chars && pool->MaxAlloc > chars)
		{
			break;
		}
		pool = pool->Next;
	}
	return pool;
}

string::StringHeader *string::PoolGroup::GetHeader (char *chars)
{
	return (StringHeader *)(chars - sizeof(StringHeader));
}

string::Pool::Pool (size_t minSize)
{
	if (minSize < POOL_SIZE)
	{
		minSize = POOL_SIZE;
	}
	minSize = RoundLen (minSize-1);
	PoolData = new char[minSize];
	FreeSpace = minSize;
	MaxAlloc = PoolData + minSize;
	Next = NULL;
	NextAlloc = (StringHeader *)PoolData;
	NextAlloc->Owner = NULL;
	NextAlloc->Len = minSize;
	GarbageStart = NextAlloc;
	GenerationNum = 0;
}

string::Pool::~Pool ()
{
	if (PoolData != NULL)
	{
		// Watch out! During program exit, the pool may be deleted before
		// all the strings stored in it. So we need to walk through the pool
		// and make any owned strings un-owned.
		StringHeader *str;

		for (str = (StringHeader *)PoolData; str < NextAlloc; )
		{
			if (str->Owner != NULL)
			{
				str->Owner->Chars = NULL;
				str->Owner = NULL;
				str = (StringHeader *)((char *)str + RoundLen(str->Len));
			}
			else
			{
				str = (StringHeader *)((char *)str + str->Len);
			}
		}

		delete[] PoolData;
		PoolData = NULL;
	}
}

char *string::Pool::Alloc (string *owner, size_t len)
{
	if (NextAlloc == (StringHeader *)MaxAlloc)
	{
		return NULL;
	}
	size_t needlen = RoundLen (len);
	if (NextAlloc->Len >= needlen)
	{
		char *chars = (char *)NextAlloc + sizeof(StringHeader);
		chars[0] = '\0';
		NextAlloc->Owner = owner;
		NextAlloc->Len = len;
		NextAlloc = (StringHeader *)((char *)NextAlloc + needlen);
		if (NextAlloc != (StringHeader *)MaxAlloc)
		{
			NextAlloc->Owner = NULL;
			NextAlloc->Len = MaxAlloc - (char *)NextAlloc;
		}
		FreeSpace -= needlen;
		return chars;
	}
	return NULL;
}

char *string::Pool::Realloc (char *chars, size_t newlen)
{
	size_t needlen = RoundLen (newlen);
	StringHeader *oldhead = (StringHeader *)(chars - sizeof(StringHeader));
	size_t oldtruelen = RoundLen (oldhead->Len);

	if (oldtruelen > needlen)
	{ // Shrinking, so make a new free block after this one.
		StringHeader *nextblock = (StringHeader *)((char *)oldhead + needlen);
		nextblock->Owner = NULL;
		nextblock->Len = oldtruelen - needlen;
		MergeFreeBlocks (nextblock);
		oldhead->Len = newlen;
		return chars;
	}

	if (oldtruelen == needlen)
	{ // There is already enough space allocated for the needed growth
		oldhead->Len = newlen;
		return chars;
	}

	// If there is free space after this string, try to grow into it.
	StringHeader *nexthead = (StringHeader *)((char *)oldhead + oldtruelen);
	if (nexthead < (StringHeader *)MaxAlloc && nexthead->Owner == NULL)
	{
		// Make sure there's only one free block past this string
		MergeFreeBlocks (nexthead);
		// Is there enough room to grow?
		if (oldtruelen + nexthead->Len >= needlen)
		{
			oldhead->Len = newlen;
			size_t newfreelen = oldtruelen + nexthead->Len - needlen;
			if (newfreelen > 0)
			{
				StringHeader *nextnewhead = (StringHeader *)((char *)oldhead + needlen);
				nextnewhead->Owner = NULL;
				nextnewhead->Len = newfreelen;
				// If this is the last string in the pool, then the NextAlloc marker also needs to move
				if (nexthead == NextAlloc)
				{
					NextAlloc = nextnewhead;
				}
			}
			FreeSpace -= needlen - oldtruelen;
			return chars;
		}
	}

	// There was insufficient room for growth, so try to allocate space at the end of the pool
	char *newchars = Alloc (oldhead->Owner, newlen);
	if (newchars != NULL)
	{
		string::StrCopy (newchars, chars, oldhead->Len);
		Free (chars);
		return newchars;
	}

	// There was not enough space
	return NULL;
}

void string::Pool::Free (char *chars)
{
	StringHeader *head = (StringHeader *)(chars - sizeof(StringHeader));
	size_t truelen = RoundLen (head->Len);

	FreeSpace += truelen;

	head->Owner = NULL;
	head->Len = truelen;
	MergeFreeBlocks (head);
}

void string::Pool::MergeFreeBlocks (StringHeader *head)
{
	StringHeader *block;

	for (block = head;
		block->Owner == NULL && block != NextAlloc;
		block = (StringHeader *)((char *)block + block->Len))
	{
	}

	// If this chain of blocks meets up with the free space, then they can join up with it.
	if (block == NextAlloc)
	{
		if (GarbageStart == NextAlloc)
		{
			GarbageStart = head;
		}
		NextAlloc = head;
		head->Len = MaxAlloc - (char *)head;
	}
	else
	{
		head->Len = (char *)block - (char *)head;
	}
}

bool string::Pool::BigEnough (size_t len) const
{
	return FreeSpace >= RoundLen (len);
}

size_t string::Pool::RoundLen (size_t len) const
{
	return (len + 1 + sizeof(StringHeader) + BLOCK_GRANULARITY - 1) & ~(BLOCK_GRANULARITY - 1);
}

void string::Pool::CollectGarbage (bool noGenerations)
{
	// This is a generational garbage collector. The space occupied by strings from
	// the first two generations will not be collected unless noGenerations is set true.

	if (noGenerations)
	{
		GarbageStart = (StringHeader *)PoolData;
		GenerationNum = 0;
	}

	StringHeader *moveto, *movefrom;
	moveto = movefrom = GarbageStart;

	while (movefrom < NextAlloc)
	{
		if (movefrom->Owner != NULL)
		{
			size_t truelen = RoundLen (movefrom->Len);
			if (moveto != movefrom)
			{
				memmove (moveto, movefrom, truelen);
				moveto->Owner->Chars = (char *)moveto + sizeof(StringHeader);
			}
			moveto = (StringHeader *)((char *)moveto + truelen);
			movefrom = (StringHeader *)((char *)movefrom + truelen);
		}
		else
		{
			movefrom = (StringHeader *)((char *)movefrom + movefrom->Len);
		}
	}

	NextAlloc = moveto;
	if (NextAlloc != (StringHeader *)MaxAlloc)
	{
		NextAlloc->Len = MaxAlloc - (char *)moveto;
		NextAlloc->Owner = NULL;
		if (NextAlloc->Len != FreeSpace)
			FreeSpace = FreeSpace;
	}
	else if (FreeSpace != 0)
		FreeSpace = FreeSpace;

	if (++GenerationNum <= 3)
	{
		GarbageStart = moveto;
	}
}
#else
char *string::PoolGroup::Alloc (string *owner, size_t len)
{
	char *mem = (char *)malloc (len + 1 + sizeof(StringHeader));
	StringHeader *head = (StringHeader *)mem;
	mem += sizeof(StringHeader);
	head->Len = len;
	return mem;
}

char *string::PoolGroup::Realloc (string *owner, char *chars, size_t newlen)
{
	if (chars == NULL)
	{
		chars = Alloc (owner, newlen);
		chars[0] = '\0';
		return chars;
	}
	StringHeader *head = (StringHeader *)(chars - sizeof(StringHeader));
	head = (StringHeader *)realloc (head, newlen+1+sizeof(StringHeader));
	head->Len = newlen;
	return (char *)head + sizeof(StringHeader);
}

void string::PoolGroup::Free (char *chars)
{
	free (chars - sizeof(StringHeader));
}

#endif

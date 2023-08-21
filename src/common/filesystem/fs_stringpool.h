#pragma once

// Storage for all the static strings the file system must hold.
class StringPool
{
public:
	StringPool(size_t blocksize = 10*1024) : TopBlock(nullptr), FreeBlocks(nullptr), BlockSize(blocksize) {}
	~StringPool();
	const char* Strdup(const char*);

protected:
	struct Block;

	Block *AddBlock(size_t size);
	void *iAlloc(size_t size);

	Block *TopBlock;
	Block *FreeBlocks;
	size_t BlockSize;
public:
	bool shared;
};


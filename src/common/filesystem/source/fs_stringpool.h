#pragma once

namespace FileSys {
// Storage for all the static strings the file system must hold.
class StringPool
{
	// do not allow externally defining this.
	friend class FileSystem;
	friend class FResourceFile;
private:
	StringPool(bool _shared, size_t blocksize = 10*1024) : TopBlock(nullptr), BlockSize(blocksize), shared(_shared) {}
public:
	~StringPool();
	const char* Strdup(const char*);
	void* Alloc(size_t size);

protected:
	struct Block;

	Block *AddBlock(size_t size);

	Block *TopBlock;
	size_t BlockSize;
public:
	bool shared;
};

}

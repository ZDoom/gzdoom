#ifndef __FILE_ZIP_H
#define __FILE_ZIP_H

#include "resourcefile.h"

// This holds a compresed Zip entry with all needed info to decompress it.
struct FCompressedBuffer
{
	unsigned mSize;
	unsigned mCompressedSize;
	int mMethod;
	int mZipFlags;
	char *mBuffer;
	
	bool Decompress(char *destbuffer);
	void Clean()
	{
		mSize = mCompressedSize = 0;
		if (mBuffer != nullptr)
		{
			delete[] mBuffer;
			mBuffer = nullptr;
		}
	}
};

enum
{
	LUMPFZIP_NEEDFILESTART = 128
};

//==========================================================================
//
// Zip Lump
//
//==========================================================================

struct FZipLump : public FResourceLump
{
	WORD	GPFlags;
	BYTE	Method;
	int		CompressedSize;
	int		Position;

	virtual FileReader *GetReader();
	virtual int FillCache();

private:
	void SetLumpAddress();
	virtual int GetFileOffset();
};


//==========================================================================
//
// Zip file
//
//==========================================================================

class FZipFile : public FResourceFile
{
	FZipLump *Lumps;

public:
	FZipFile(const char * filename, FileReader *file);
	virtual ~FZipFile();
	bool Open(bool quiet);
	virtual FResourceLump *GetLump(int no) { return ((unsigned)no < NumLumps)? &Lumps[no] : NULL; }
	FCompressedBuffer GetRawLump(int lumpnum);
};


#endif
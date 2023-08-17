#ifndef __FILE_ZIP_H
#define __FILE_ZIP_H

#include "resourcefile.h"

//==========================================================================
//
// Zip Lump
//
//==========================================================================

struct FZipLump : public FResourceLump
{
	uint16_t	GPFlags;
	uint8_t	Method;
	bool	NeedFileStart;
	int		CompressedSize;
	int64_t		Position;
	unsigned CRC32;

	virtual FileReader *GetReader();
	virtual int FillCache();

private:
	void SetLumpAddress();
	virtual int GetFileOffset();
	FCompressedBuffer GetRawData();
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
	FZipFile(const char * filename, FileReader &file);
	virtual ~FZipFile();
	bool Open(LumpFilterInfo* filter, FileSystemMessageFunc Printf);
	virtual FResourceLump *GetLump(int no) { return ((unsigned)no < NumLumps)? &Lumps[no] : NULL; }
};


#endif
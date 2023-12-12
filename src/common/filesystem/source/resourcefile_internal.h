#pragma once

#include "resourcefile.h"

namespace FileSys {

// Base class for uncompressed resource files (WAD, GRP, PAK and single lumps)
class FUncompressedFile : public FResourceFile
{
protected:

	FUncompressedFile(const char *filename, StringPool* sp);
	FUncompressedFile(const char *filename, FileReader &r, StringPool* sp);
	//FileData Read(int entry) override; todo later. It's only needed for optimization once everything works
	FileReader GetEntryReader(uint32_t entry, bool) override;
};


// should only be used internally.
struct FExternalLump : public FResourceLump
{
	const char* FileName;

	FExternalLump(const char *_filename, int filesize, StringPool* sp);
	virtual int FillCache() override;

};

}
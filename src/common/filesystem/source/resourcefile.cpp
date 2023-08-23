/*
** resourcefile.cpp
**
** Base classes for resource file management
**
**---------------------------------------------------------------------------
** Copyright 2009 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
**
*/

#include <zlib.h>
#include "resourcefile_internal.h"
#include "md5.hpp"
#include "fs_stringpool.h"
#include "files_internal.h"

namespace FileSys {
	
std::string ExtractBaseName(const char* path, bool include_extension)
{
	const char* src, * dot;

	src = path + strlen(path) - 1;

	if (src >= path)
	{
		// back up until a / or the start
		while (src != path && src[-1] != '/' && src[-1] != '\\') // check both on all systems for consistent behavior with archives.
			src--;

		if (!include_extension && (dot = strrchr(src, '.')))
		{
			return std::string(src, dot - src);
		}
		else
		{
			return std::string(src);
		}
	}
	return std::string();
}

void strReplace(std::string& str, const char *from, const char* to) 
{
	if (*from == 0)
		return;
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) 
	{
		str.replace(start_pos, strlen(from), to);
		start_pos += strlen(to);
	}
}

//==========================================================================
//
// File reader that reads from a lump's cache
//
//==========================================================================

class FLumpReader : public MemoryReader
{
	FResourceLump *source;

public:
	FLumpReader(FResourceLump *src)
		: MemoryReader(NULL, src->LumpSize), source(src)
	{
		src->Lock();
		bufptr = src->Cache;
	}

	~FLumpReader()
	{
		source->Unlock();
	}
};


//==========================================================================
//
// Base class for resource lumps
//
//==========================================================================

FResourceLump::~FResourceLump()
{
	if (Cache != NULL && RefCount >= 0)
	{
		delete [] Cache;
		Cache = NULL;
	}
	Owner = NULL;
}


//==========================================================================
//
// Sets up the lump name information for anything not coming from a WAD file.
//
//==========================================================================

void FResourceLump::LumpNameSetup(const char *iname, StringPool* allocator)
{
	// this causes interference with real Dehacked lumps.
	if (!stricmp(iname, "dehacked.exe"))
	{
		iname = "";
	}
	else if (allocator)
	{
		iname = allocator->Strdup(iname);
	}

	FullName = iname;
}

//==========================================================================
//
// Checks for embedded resource files
//
//==========================================================================

static bool IsWadInFolder(const FResourceFile* const archive, const char* const resPath)
{
	// Checks a special case when <somefile.wad> was put in
	// <myproject> directory inside <myproject.zip>

	if (NULL == archive)
	{
		return false;
	}

    const auto dirName = ExtractBaseName(archive->FileName);
	const auto fileName = ExtractBaseName(resPath, true);
	const std::string filePath = dirName + '/' + fileName;

	return 0 == stricmp(filePath.c_str(), resPath);
}

void FResourceLump::CheckEmbedded(LumpFilterInfo* lfi)
{
	// Checks for embedded archives
	const char *c = strstr(FullName, ".wad");
	if (c && strlen(c) == 4 && (!strchr(FullName, '/') || IsWadInFolder(Owner, FullName)))
	{
		Flags |= LUMPF_EMBEDDED;
	}
	else if (lfi) for (auto& fstr : lfi->embeddings)
	{
		if (!stricmp(FullName, fstr.c_str()))
		{
			Flags |= LUMPF_EMBEDDED;
		}
	}
}


//==========================================================================
//
// this is just for completeness. For non-Zips only an uncompressed lump can
// be returned.
//
//==========================================================================

FCompressedBuffer FResourceLump::GetRawData()
{
	FCompressedBuffer cbuf = { (unsigned)LumpSize, (unsigned)LumpSize, METHOD_STORED, 0, 0, new char[LumpSize] };
	memcpy(cbuf.mBuffer, Lock(), LumpSize);
	Unlock();
	cbuf.mCRC32 = crc32(0, (uint8_t*)cbuf.mBuffer, LumpSize);
	return cbuf;
}


//==========================================================================
//
// Returns the owner's FileReader if it can be used to access this lump
//
//==========================================================================

FileReader *FResourceLump::GetReader()
{
	return NULL;
}

//==========================================================================
//
// Returns a file reader to the lump's cache
//
//==========================================================================

FileReader FResourceLump::NewReader()
{
	return FileReader(new FLumpReader(this));
}

//==========================================================================
//
// Caches a lump's content and increases the reference counter
//
//==========================================================================

void *FResourceLump::Lock()
{
	if (Cache != NULL)
	{
		if (RefCount > 0) RefCount++;
	}
	else if (LumpSize > 0)
	{
		try
		{
			FillCache();
		}
		catch (const FileSystemException& err)
		{
			// enrich the message with info about this lump.
			throw FileSystemException("%s, file '%s': %s", getName(), Owner->FileName, err.what());
		}
	}
	return Cache;
}

//==========================================================================
//
// Decrements reference counter and frees lump if counter reaches 0
//
//==========================================================================

int FResourceLump::Unlock()
{
	if (LumpSize > 0 && RefCount > 0)
	{
		if (--RefCount == 0)
		{
			delete [] Cache;
			Cache = NULL;
		}
	}
	return RefCount;
}

//==========================================================================
//
// Opens a resource file
//
//==========================================================================

typedef FResourceFile * (*CheckFunc)(const char *filename, FileReader &file, LumpFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp);

FResourceFile *CheckWad(const char *filename, FileReader &file, LumpFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp);
FResourceFile *CheckGRP(const char *filename, FileReader &file, LumpFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp);
FResourceFile *CheckRFF(const char *filename, FileReader &file, LumpFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp);
FResourceFile *CheckPak(const char *filename, FileReader &file, LumpFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp);
FResourceFile *CheckZip(const char *filename, FileReader &file, LumpFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp);
FResourceFile *Check7Z(const char *filename,  FileReader &file, LumpFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp);
FResourceFile* CheckSSI(const char* filename, FileReader& file, LumpFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp);
FResourceFile* CheckWHRes(const char* filename, FileReader& file, LumpFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp);
FResourceFile *CheckLump(const char *filename,FileReader &file, LumpFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp);
FResourceFile *CheckDir(const char *filename, bool nosub, LumpFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp);

static CheckFunc funcs[] = { CheckWad, CheckZip, Check7Z, CheckPak, CheckGRP, CheckRFF, CheckSSI, CheckWHRes, CheckLump };

static int nulPrintf(FSMessageLevel msg, const char* fmt, ...)
{
	return 0;
}

FResourceFile *FResourceFile::DoOpenResourceFile(const char *filename, FileReader &file, bool containeronly, LumpFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp)
{
	if (Printf == nullptr) Printf = nulPrintf;
	for(auto func : funcs)
	{
		if (containeronly && func == CheckLump) break;
		FResourceFile *resfile = func(filename, file, filter, Printf, sp);
		if (resfile != NULL) return resfile;
	}
	return NULL;
}

FResourceFile *FResourceFile::OpenResourceFile(const char *filename, FileReader &file, bool containeronly, LumpFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp)
{
	return DoOpenResourceFile(filename, file, containeronly, filter, Printf, sp);
}


FResourceFile *FResourceFile::OpenResourceFile(const char *filename, bool containeronly, LumpFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp)
{
	FileReader file;
	if (!file.OpenFile(filename)) return nullptr;
	return DoOpenResourceFile(filename, file, containeronly, filter, Printf, sp);
}

FResourceFile *FResourceFile::OpenDirectory(const char *filename, LumpFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp)
{
	if (Printf == nullptr) Printf = nulPrintf;
	return CheckDir(filename, false, filter, Printf, sp);
}

//==========================================================================
//
// Resource file base class
//
//==========================================================================

FResourceFile::FResourceFile(const char *filename, StringPool* sp)
{
	stringpool = sp ? sp : new StringPool;
	FileName = stringpool->Strdup(filename);
}

FResourceFile::FResourceFile(const char *filename, FileReader &r, StringPool* sp)
	: FResourceFile(filename,sp)
{
	Reader = std::move(r);
}

FResourceFile::~FResourceFile()
{
	if (!stringpool->shared) delete stringpool;
}

int lumpcmp(const void * a, const void * b)
{
	FResourceLump * rec1 = (FResourceLump *)a;
	FResourceLump * rec2 = (FResourceLump *)b;
	return stricmp(rec1->getName(), rec2->getName());
}

//==========================================================================
//
// FResourceFile :: GenerateHash
//
// Generates a hash identifier for use in file identification.
// Potential uses are mod-wide compatibility settings or localization add-ons.
// This only hashes the lump directory but not the actual content
//
//==========================================================================

void FResourceFile::GenerateHash()
{
	// hash the lump directory after sorting
	using namespace FileSys::md5;

	auto n = snprintf(Hash, 48, "%08X-%04X-", (unsigned)Reader.GetLength(), NumLumps);

	md5_state_t state;
	md5_init(&state);

	uint8_t digest[16];
	for(uint32_t i = 0; i < NumLumps; i++)
	{
		auto lump = GetLump(i);
		md5_append(&state, (const uint8_t*)lump->FullName, (unsigned)strlen(lump->FullName) + 1);
		md5_append(&state, (const uint8_t*)&lump->LumpSize, 4);
	}
	md5_finish(&state, digest);
	for (auto c : digest)
	{
		n += snprintf(Hash + n, 3, "%02X", c);
	}
}

//==========================================================================
//
// FResourceFile :: PostProcessArchive
//
// Sorts files by name.
// For files named "filter/<game>/*": Using the same filter rules as config
// autoloading, move them to the end and rename them without the "filter/"
// prefix. Filtered files that don't match are deleted.
//
//==========================================================================

void FResourceFile::PostProcessArchive(void *lumps, size_t lumpsize, LumpFilterInfo *filter)
{
	// Entries in archives are sorted alphabetically
	qsort(lumps, NumLumps, lumpsize, lumpcmp);
	if (!filter) return;

	// Filter out lumps using the same names as the Autoload.* sections
	// in the ini file use. We reduce the maximum lump concidered after
	// each one so that we don't risk refiltering already filtered lumps.
	uint32_t max = NumLumps;
	max -= FilterLumpsByGameType(filter, lumps, lumpsize, max);

	ptrdiff_t len;
	ptrdiff_t lastpos = -1;
	std::string file;
	std::string& LumpFilter = filter->dotFilter;
	while ((len = LumpFilter.find_first_of('.', lastpos+1)) != LumpFilter.npos)
	{
		max -= FilterLumps(std::string(LumpFilter, 0, len), lumps, lumpsize, max);
		lastpos = len;
	}
	max -= FilterLumps(LumpFilter, lumps, lumpsize, max);

	JunkLeftoverFilters(lumps, lumpsize, max);
}

//==========================================================================
//
// FResourceFile :: FilterLumps
//
// Finds any lumps between [0,<max>) that match the pattern
// "filter/<filtername>/*" and moves them to the end of the lump list.
// Returns the number of lumps moved.
//
//==========================================================================

int FResourceFile::FilterLumps(const std::string& filtername, void *lumps, size_t lumpsize, uint32_t max)
{
	uint32_t start, end;

	if (filtername.empty())
	{
		return 0;
	}
	std::string filter = "filter/" + filtername + '/';

	bool found = FindPrefixRange(filter.c_str(), lumps, lumpsize, max, start, end);

	// Workaround for old Doom filter names (todo: move out of here.)
	if (!found && filtername.find("doom.id.doom") == 0)
	{
		strReplace(filter, "doom.id.doom", "doom.doom");
		found = FindPrefixRange(filter.c_str(), lumps, lumpsize, max, start, end);
	}

	if (found)
	{
		void *from = (uint8_t *)lumps + start * lumpsize;

		// Remove filter prefix from every name
		void *lump_p = from;
		for (uint32_t i = start; i < end; ++i, lump_p = (uint8_t *)lump_p + lumpsize)
		{
			FResourceLump *lump = (FResourceLump *)lump_p;
			assert(strnicmp(lump->FullName, filter.c_str(), filter.length()) == 0);
			lump->LumpNameSetup(lump->FullName + filter.length(), nullptr);
		}

		// Move filtered lumps to the end of the lump list.
		size_t count = (end - start) * lumpsize;
		void *to = (uint8_t *)lumps + NumLumps * lumpsize - count;
		assert (to >= from);

		if (from != to)
		{
			// Copy filtered lumps to a temporary buffer.
			uint8_t *filteredlumps = new uint8_t[count];
			memcpy(filteredlumps, from, count);

			// Shift lumps left to make room for the filtered ones at the end.
			memmove(from, (uint8_t *)from + count, (NumLumps - end) * lumpsize);

			// Copy temporary buffer to newly freed space.
			memcpy(to, filteredlumps, count);

			delete[] filteredlumps;
		}
	}
	return end - start;
}

//==========================================================================
//
// FResourceFile :: FilterLumpsByGameType
//
// Matches any lumps that match "filter/game-<gametype>/*". Includes
// inclusive gametypes like Raven.
//
//==========================================================================

int FResourceFile::FilterLumpsByGameType(LumpFilterInfo *filter, void *lumps, size_t lumpsize, uint32_t max)
{
	if (filter == nullptr)
	{
		return 0;
	}
	int count = 0;
	for (auto &fstring : filter->gameTypeFilter)
	{
		count += FilterLumps(fstring, lumps, lumpsize, max);
	}
	return count;
}

//==========================================================================
//
// FResourceFile :: JunkLeftoverFilters
//
// Deletes any lumps beginning with "filter/" that were not matched.
//
//==========================================================================

void FResourceFile::JunkLeftoverFilters(void *lumps, size_t lumpsize, uint32_t max)
{
	uint32_t start, end;
	if (FindPrefixRange("filter/", lumps, lumpsize, max, start, end))
	{
		// Since the resource lumps may contain non-POD data besides the
		// full name, we "delete" them by erasing their names so they
		// can't be found.
		void *stop = (uint8_t *)lumps + end * lumpsize;
		for (void *p = (uint8_t *)lumps + start * lumpsize; p < stop; p = (uint8_t *)p + lumpsize)
		{
			FResourceLump *lump = (FResourceLump *)p;
			lump->clearName();
		}
	}
}

//==========================================================================
//
// FResourceFile :: FindPrefixRange
//
// Finds a range of lumps that start with the prefix string. <start> is left
// indicating the first matching one. <end> is left at one plus the last
// matching one.
//
//==========================================================================

bool FResourceFile::FindPrefixRange(const char* filter, void *lumps, size_t lumpsize, uint32_t maxlump, uint32_t &start, uint32_t &end)
{
	uint32_t min, max, mid, inside;
	FResourceLump *lump;
	int cmp = 0;

	end = start = 0;

	// Pretend that our range starts at 1 instead of 0 so that we can avoid
	// unsigned overflow if the range starts at the first lump.
	lumps = (uint8_t *)lumps - lumpsize;

	// Binary search to find any match at all.
	mid = min = 1, max = maxlump;
	while (min <= max)
	{
		mid = min + (max - min) / 2;
		lump = (FResourceLump *)((uint8_t *)lumps + mid * lumpsize);
		cmp = strnicmp(lump->FullName, filter, (int)strlen(filter));
		if (cmp == 0)
			break;
		else if (cmp < 0)
			min = mid + 1;
		else		
			max = mid - 1;
	}
	if (max < min)
	{ // matched nothing
		return false;
	}

	// Binary search to find first match.
	inside = mid;
	min = 1, max = mid;
	while (min <= max)
	{
		mid = min + (max - min) / 2;
		lump = (FResourceLump *)((uint8_t *)lumps + mid * lumpsize);
		cmp = strnicmp(lump->FullName, filter, (int)strlen(filter));
		// Go left on matches and right on misses.
		if (cmp == 0)
			max = mid - 1;
		else
			min = mid + 1;
	}
	start = mid + (cmp != 0) - 1;

	// Binary search to find last match.
	min = inside, max = maxlump;
	while (min <= max)
	{
		mid = min + (max - min) / 2;
		lump = (FResourceLump *)((uint8_t *)lumps + mid * lumpsize);
		cmp = strnicmp(lump->FullName, filter, (int)strlen(filter));
		// Go right on matches and left on misses.
		if (cmp == 0)
			min = mid + 1;
		else
			max = mid - 1;
	}
	end = mid - (cmp != 0);
	return true;
}

//==========================================================================
//
// Finds a lump by a given name. Used for savegames
//
//==========================================================================

FResourceLump *FResourceFile::FindLump(const char *name)
{
	for (unsigned i = 0; i < NumLumps; i++)
	{
		FResourceLump *lump = GetLump(i);
		if (!stricmp(name, lump->FullName))
		{
			return lump;
		}
	}
	return nullptr;
}

//==========================================================================
//
// Caches a lump's content and increases the reference counter
//
//==========================================================================

FileReader *FUncompressedLump::GetReader()
{
	Owner->Reader.Seek(Position, FileReader::SeekSet);
	return &Owner->Reader;
}

//==========================================================================
//
// Caches a lump's content and increases the reference counter
//
//==========================================================================

int FUncompressedLump::FillCache()
{
	const char * buffer = Owner->Reader.GetBuffer();

	if (buffer != NULL)
	{
		// This is an in-memory file so the cache can point directly to the file's data.
		Cache = const_cast<char*>(buffer) + Position;
		RefCount = -1;
		return -1;
	}

	Owner->Reader.Seek(Position, FileReader::SeekSet);
	Cache = new char[LumpSize];

	auto read = Owner->Reader.Read(Cache, LumpSize);
	if (read != LumpSize)
	{
		throw FileSystemException("only read %d of %d bytes", (int)read, (int)LumpSize);
	}

	RefCount = 1;
	return 1;
}

//==========================================================================
//
// Base class for uncompressed resource files
//
//==========================================================================

FUncompressedFile::FUncompressedFile(const char *filename, StringPool* sp)
: FResourceFile(filename, sp)
{}

FUncompressedFile::FUncompressedFile(const char *filename, FileReader &r, StringPool* sp)
	: FResourceFile(filename, r, sp)
{}


//==========================================================================
//
// external lump
//
//==========================================================================

FExternalLump::FExternalLump(const char *_filename, int filesize, StringPool* stringpool)
{
	FileName = stringpool->Strdup(_filename);

	if (filesize == -1)
	{
		FileReader f;

		if (f.OpenFile(_filename))
		{
			LumpSize = (int)f.GetLength();
		}
		else
		{
			LumpSize = 0;
		}
	}
	else
	{
		LumpSize = filesize;
	}
}


//==========================================================================
//
// Caches a lump's content and increases the reference counter
// For external lumps this reopens the file each time it is accessed
//
//==========================================================================

int FExternalLump::FillCache()
{
	Cache = new char[LumpSize];
	FileReader f;

	if (f.OpenFile(FileName))
	{
		auto read = f.Read(Cache, LumpSize);
		if (read != LumpSize)
		{
			throw FileSystemException("only read %d of %d bytes", (int)read, (int)LumpSize);
		}
	}
	else
	{
		throw FileSystemException("unable to open file");
	}
	RefCount = 1;
	return 1;
}


}

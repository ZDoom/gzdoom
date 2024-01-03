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

#include <miniz.h>
#include "resourcefile.h"
#include "md5.hpp"
#include "fs_stringpool.h"
#include "files_internal.h"
#include "unicode.h"
#include "fs_findfile.h"
#include "fs_decompress.h"
#include "wildcards.hpp"

namespace FileSys {
	
// this is for restricting shared file readers to the main thread.
thread_local bool mainThread;
void SetMainThread()
{
	// only set the global flag on the first thread calling this.
	static bool done = false;
	if (!done)
	{
		mainThread = done = true;
	}
}

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
// Checks for embedded resource files
//
//==========================================================================

bool FResourceFile::IsFileInFolder(const char* const resPath)
{
	// Checks a special case when <somefile.wad> was put in
	// <myproject> directory inside <myproject.zip>

    const auto dirName = ExtractBaseName(FileName);
	const auto fileName = ExtractBaseName(resPath, true);
	const std::string filePath = dirName + '/' + fileName;

	return 0 == stricmp(filePath.c_str(), resPath);
}

void FResourceFile::CheckEmbedded(uint32_t entry, LumpFilterInfo* lfi)
{
	// Checks for embedded archives
	auto FullName = Entries[entry].FileName;
	const char *c = strstr(FullName, ".wad"); // fixme: Use lfi for this.
	if (c && strlen(c) == 4 && (!strchr(FullName, '/') || IsFileInFolder(FullName)))
	{
		Entries[entry].Flags |= RESFF_EMBEDDED;
	}
	else if (lfi) for (auto& fstr : lfi->embeddings)
	{
		if (!stricmp(FullName, fstr.c_str()))
		{
			Entries[entry].Flags |= RESFF_EMBEDDED;
		}
	}
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
FResourceFile* CheckHog(const char* filename, FileReader& file, LumpFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp);
FResourceFile* CheckMvl(const char* filename, FileReader& file, LumpFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp);
FResourceFile* CheckWHRes(const char* filename, FileReader& file, LumpFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp);
FResourceFile *CheckLump(const char *filename,FileReader &file, LumpFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp);
FResourceFile *CheckDir(const char *filename, bool nosub, LumpFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp);

static CheckFunc funcs[] = { CheckWad, CheckZip, Check7Z, CheckPak, CheckGRP, CheckRFF, CheckSSI, CheckHog, CheckMvl, CheckWHRes, CheckLump };

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
	stringpool = sp ? sp : new StringPool(false);
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

//==========================================================================
//
// this is just for completeness. For non-Zips only an uncompressed lump can
// be returned.
//
//==========================================================================

FCompressedBuffer FResourceFile::GetRawData(uint32_t entry)
{
	size_t LumpSize = entry << NumLumps ? Entries[entry].Length : 0;
	FCompressedBuffer cbuf = { LumpSize, LumpSize, METHOD_STORED, 0, 0, LumpSize == 0? nullptr : new char[LumpSize] };
	if (LumpSize > 0)
	{
		auto fr = GetEntryReader(entry, READER_SHARED, 0);
		size_t read = fr.Read(cbuf.mBuffer, LumpSize);
		if (read < LumpSize)
		{
			delete cbuf.mBuffer;
			cbuf.mBuffer = nullptr;
			LumpSize = cbuf.mCompressedSize = cbuf.mSize = 0;
		}
	}
	if (LumpSize > 0)
		cbuf.mCRC32 = crc32(0, (uint8_t*)cbuf.mBuffer, LumpSize);
	return cbuf;
}


//==========================================================================
//
// normalize the visible file name in the system
// to lowercase canonical precomposed Unicode.
//
//==========================================================================

const char* FResourceFile::NormalizeFileName(const char* fn, int fallbackcp)
{
	if (!fn || !*fn) return "";
	auto norm = tolower_normalize(fn);
	if (!norm)
	{
		if (fallbackcp == 437)
		{
			std::vector<char> buffer;
			ibm437_to_utf8(fn, buffer);
			norm = tolower_normalize(buffer.data());
		}
		// maybe handle other codepages
		else
		{
			// if the filename is not valid UTF-8, nuke all bytes larger than 0x80 so that we still got something semi-usable
			std::string ffn = fn;
			for (auto& c : ffn)
			{
				if (c & 0x80) c = '@';
			}
			norm = tolower_normalize(&ffn.front());
		}
	}
	FixPathSeparator(norm);
	auto pooled = stringpool->Strdup(norm);
	free(norm);
	return pooled;

}

//==========================================================================
//
// allocate the Entries array
// this also uses the string pool to reduce maintenance
//
//==========================================================================

FResourceEntry* FResourceFile::AllocateEntries(int count)
{
	NumLumps = count;
	Entries = (FResourceEntry*)stringpool->Alloc(count * sizeof(FResourceEntry));
	memset(Entries, 0, count * sizeof(FResourceEntry));
	return Entries;
}


//---------------------------------------------------
int entrycmp(const void* a, const void* b)
{
	FResourceEntry* rec1 = (FResourceEntry*)a;
	FResourceEntry* rec2 = (FResourceEntry*)b;
	// we are comparing lowercase UTF-8 here 
	return strcmp(rec1->FileName, rec2->FileName);
}

//==========================================================================
//
// FResourceFile :: GenerateHash
//
// Generates a hash identifier for use in file identification.
// Potential uses are mod-wide compatibility settings or localization add-ons.
// This only hashes the directory but not the actual content
//
//==========================================================================

void FResourceFile::GenerateHash()
{
	// hash the directory after sorting
	using namespace FileSys::md5;

	auto n = snprintf(Hash, 48, "%08X-%04X-", (unsigned)Reader.GetLength(), NumLumps);

	md5_state_t state;
	md5_init(&state);

	uint8_t digest[16];
	for(uint32_t i = 0; i < NumLumps; i++)
	{
		auto name = getName(i);
		auto size = Length(i);
		if (name == nullptr) 
			continue;
		md5_append(&state, (const uint8_t*)name, (unsigned)strlen(name) + 1);
		md5_append(&state, (const uint8_t*)&size, sizeof(size));
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

void FResourceFile::PostProcessArchive(LumpFilterInfo *filter)
{
	// only do this for archive types which contain full file names. All others are assumed to be pre-sorted.
	if (NumLumps == 0 || !(Entries[0].Flags & RESFF_FULLPATH)) return;

	// First eliminate all unwanted files
	if (filter)
	{
		for (uint32_t i = 0; i < NumLumps; i++)
		{
			std::string name = Entries[i].FileName;
			for (auto& pattern : filter->blockednames)
			{
				if (wildcards::match(name, pattern))
				{
					Entries[i].FileName = "";
					continue;
				}
			}
		}
	}

	// Entries in archives are sorted alphabetically.
	qsort(Entries, NumLumps, sizeof(Entries[0]), entrycmp);
	if (!filter) return;
	FindCommonFolder(filter);

	// Filter out lumps using the same names as the Autoload.* sections
	// in the ini file. We reduce the maximum lump concidered after
	// each one so that we don't risk refiltering already filtered lumps.
	uint32_t max = NumLumps;

	for (auto& LumpFilter : filter->gameTypeFilter)
	{
		ptrdiff_t len;
		ptrdiff_t lastpos = -1;
		std::string file;
		while (size_t(len = LumpFilter.find_first_of('.', lastpos + 1)) != LumpFilter.npos)
		{
			max -= FilterLumps(std::string(LumpFilter, 0, len), max);
			lastpos = len;
		}
		max -= FilterLumps(LumpFilter, max);
	}

	JunkLeftoverFilters(max);

	for (uint32_t i = 0; i < NumLumps; i++)
	{
		CheckEmbedded(i, filter);
	}

}

//==========================================================================
//
// FResourceFile :: FindCommonFolder
//
// Checks if all content is in a common folder that can be stripped out.
//
//==========================================================================

void FResourceFile::FindCommonFolder(LumpFilterInfo* filter)
{
	std::string name0, name1;
	bool foundspeciallump = false;
	bool foundprefix = false;

	// try to find a path prefix.
	for (uint32_t i = 0; i < NumLumps; i++)
	{
		if (*Entries[i].FileName == 0) continue;
		std::string name = Entries[i].FileName;

		// first eliminate files we do not want to have.
		// Some, like MacOS resource forks and executables are eliminated unconditionally, but the calling code can alsp pass a list of invalid content.
		if (name.find("filter/") == 0)
			return; // 'filter' is a reserved name of the file system. If this appears in the root we got no common folder, and 'filter' cannot be it.

		if (!foundprefix)
		{
			// check for special names, if one of these gets found this must be treated as a normal zip.
			bool isspecial = name.find("/") == std::string::npos ||
				(filter && std::find(filter->reservedFolders.begin(), filter->reservedFolders.end(), name) != filter->reservedFolders.end());
			if (isspecial) break;
			name0 = std::string(name, 0, name.rfind("/") + 1);
			name1 = std::string(name, 0, name.find("/") + 1);
			foundprefix = true;
		}

		if (name.find(name0) != 0)
		{
			if (!name1.empty())
			{
				name0 = name1;
				if (name.find(name0) != 0)
				{
					name0 = "";
				}
			}
			if (name0.empty())
				break;
		}
		if (!foundspeciallump && filter)
		{
			// at least one of the more common definition lumps must be present.
			for (auto& p : filter->requiredPrefixes)
			{
				if (name.find(name0 + p) == 0 || name.rfind(p) == size_t(name.length() - p.length()))
				{
					foundspeciallump = true;
					break;
				}
			}
		}
	}
	// If it ran through the list without finding anything it should not attempt any path remapping.
	if (!foundspeciallump || name0.empty()) return;

	size_t pathlen = name0.length();
	for (uint32_t i = 0; i < NumLumps; i++)
	{
		if (Entries[i].FileName[0] == 0) continue;
		Entries[i].FileName += pathlen;

	}
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

int FResourceFile::FilterLumps(const std::string& filtername, uint32_t max)
{
	uint32_t start, end;

	if (filtername.empty())
	{
		return 0;
	}
	std::string filter = "filter/" + filtername + '/';

	bool found = FindPrefixRange(filter.c_str(), max, start, end);

	if (found)
	{

		// Remove filter prefix from every name
		for (uint32_t i = start; i < end; ++i)
		{
			assert(strnicmp(Entries[i].FileName, filter.c_str(), filter.length()) == 0);
			Entries[i].FileName += filter.length();
		}

		// Move filtered lumps to the end of the lump list.
		size_t count = (end - start);
		auto from = Entries + start;
		auto to = Entries + NumLumps - count;
		assert (to >= from);

		if (from != to)
		{
			// Copy filtered lumps to a temporary buffer.
			auto filteredlumps = new FResourceEntry[count];
			memcpy(filteredlumps, from, count * sizeof(*Entries));

			// Shift lumps left to make room for the filtered ones at the end.
			memmove(from, from + count, (NumLumps - end) * sizeof(*Entries));

			// Copy temporary buffer to newly freed space.
			memcpy(to, filteredlumps, count * sizeof(*Entries));

			delete[] filteredlumps;
		}
	}
	return end - start;
}

//==========================================================================
//
// FResourceFile :: JunkLeftoverFilters
//
// Deletes any lumps beginning with "filter/" that were not matched.
//
//==========================================================================

void FResourceFile::JunkLeftoverFilters(uint32_t max)
{
	uint32_t start, end;
	if (FindPrefixRange("filter/", max, start, end))
	{
		// Since the resource lumps may contain non-POD data besides the
		// full name, we "delete" them by erasing their names so they
		// can't be found.
		for (uint32_t i = start; i < end; i++)
		{
			Entries[i].FileName = "";
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

bool FResourceFile::FindPrefixRange(const char* filter, uint32_t maxlump, uint32_t &start, uint32_t &end)
{
	uint32_t min, max, mid, inside;
	int cmp = 0;

	end = start = 0;

	// Pretend that our range starts at 1 instead of 0 so that we can avoid
	// unsigned overflow if the range starts at the first lump.
	auto lumps = &Entries[-1];

	// Binary search to find any match at all.
	mid = min = 1, max = maxlump;
	while (min <= max)
	{
		mid = min + (max - min) / 2;
		auto lump = &lumps[mid];
		cmp = strnicmp(lump->FileName, filter, strlen(filter));
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
		auto lump = &lumps[mid];
		cmp = strnicmp(lump->FileName, filter, strlen(filter));
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
		auto lump = &lumps[mid];
		cmp = strnicmp(lump->FileName, filter, strlen(filter));
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

int FResourceFile::FindEntry(const char *name)
{
	auto norm_fn = tolower_normalize(name);
	for (unsigned i = 0; i < NumLumps; i++)
	{
		if (!strcmp(norm_fn, getName(i)))
		{
			free(norm_fn);
			return i;
		}
	}
	free(norm_fn);
	return -1;
}


//==========================================================================
//
// Caches a lump's content and increases the reference counter
//
//==========================================================================

FileReader FResourceFile::GetEntryReader(uint32_t entry, int readertype, int readerflags)
{
	FileReader fr;
	if (entry < NumLumps)
	{
		if (Entries[entry].Flags & RESFF_NEEDFILESTART)
		{
			SetEntryAddress(entry);
		}
		if (!(Entries[entry].Flags & RESFF_COMPRESSED))
		{
			auto buf = Reader.GetBuffer();
			// if this is backed by a memory buffer, create a new reader directly referencing it.
			if (buf != nullptr)
			{
				fr.OpenMemory(buf + Entries[entry].Position, Entries[entry].Length);
			}
			else
			{
				if (readertype == READER_SHARED && !mainThread)
					readertype = READER_NEW;
				if (readertype == READER_SHARED)
				{
					fr.OpenFilePart(Reader, Entries[entry].Position, Entries[entry].Length);
				}
				else if (readertype == READER_NEW)
				{
					fr.OpenFile(FileName, Entries[entry].Position, Entries[entry].Length);
				}
				else if (readertype == READER_CACHED)
				{
					Reader.Seek(Entries[entry].Position, FileReader::SeekSet);
					auto data = Reader.Read(Entries[entry].Length);
					fr.OpenMemoryArray(data);
				}
			}
		}
		else
		{
			FileReader fri;
			if (readertype == READER_NEW || !mainThread) fri.OpenFile(FileName, Entries[entry].Position, Entries[entry].CompressedSize);
			else fri.OpenFilePart(Reader, Entries[entry].Position, Entries[entry].CompressedSize);
			int flags = DCF_TRANSFEROWNER | DCF_EXCEPTIONS;
			if (readertype == READER_CACHED) flags |= DCF_CACHED;
			else if (readerflags & READERFLAG_SEEKABLE) flags |= DCF_SEEKABLE;
			OpenDecompressor(fr, fri, Entries[entry].Length, Entries[entry].Method, flags);
		}
	}
	return fr;
}

FileData FResourceFile::Read(uint32_t entry)
{
	if (!(Entries[entry].Flags & RESFF_COMPRESSED) && Reader.isOpen())
	{
		auto buf = Reader.GetBuffer();
		// if this is backed by a memory buffer, we can just return a reference to the backing store.
		if (buf != nullptr)
		{
			return FileData(buf + Entries[entry].Position, Entries[entry].Length, false);
		}
	}

	auto fr = GetEntryReader(entry, READER_SHARED, 0);
	return fr.Read(entry < NumLumps ? Entries[entry].Length : 0);
}



}

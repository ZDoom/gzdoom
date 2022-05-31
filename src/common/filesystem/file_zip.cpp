/*
** file_zip.cpp
**
**---------------------------------------------------------------------------
** Copyright 1998-2009 Randy Heit
** Copyright 2005-2009 Christoph Oelckers
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

#include <time.h>
#include "file_zip.h"
#include "cmdlib.h"

#include "printf.h"
#include "w_zip.h"

#include "ancientzip.h"

#define BUFREADCOMMENT (0x400)

//==========================================================================
//
// Decompression subroutine
//
//==========================================================================

static bool UncompressZipLump(char *Cache, FileReader &Reader, int Method, int LumpSize, int CompressedSize, int GPFlags)
{
	try
	{
		switch (Method)
		{
		case METHOD_STORED:
		{
			Reader.Read(Cache, LumpSize);
			break;
		}

		case METHOD_DEFLATE:
		case METHOD_BZIP2:
		case METHOD_LZMA:
		{
			FileReader frz;
			if (frz.OpenDecompressor(Reader, LumpSize, Method, false, [](const char* err) { I_Error("%s", err); }))
			{
				frz.Read(Cache, LumpSize);
			}
			break;
		}

		// Fixme: These should also use a stream
		case METHOD_IMPLODE:
		{
			FZipExploder exploder;
			exploder.Explode((unsigned char *)Cache, LumpSize, Reader, CompressedSize, GPFlags);
			break;
		}

		case METHOD_SHRINK:
		{
			ShrinkLoop((unsigned char *)Cache, LumpSize, Reader, CompressedSize);
			break;
		}

		default:
			assert(0);
			return false;
		}
	}
	catch (CRecoverableError &err)
	{
		Printf("%s\n", err.GetMessage());
		return false;
	}
	return true;
}

bool FCompressedBuffer::Decompress(char *destbuffer)
{
	FileReader mr;
	mr.OpenMemory(mBuffer, mCompressedSize);
	return UncompressZipLump(destbuffer, mr, mMethod, mSize, mCompressedSize, mZipFlags);
}

//-----------------------------------------------------------------------
//
// Finds the central directory end record in the end of the file.
// Taken from Quake3 source but the file in question is not GPL'ed. ;)
//
//-----------------------------------------------------------------------

static uint32_t Zip_FindCentralDir(FileReader &fin, bool* zip64)
{
	unsigned char buf[BUFREADCOMMENT + 4];
	uint32_t FileSize;
	uint32_t uBackRead;
	uint32_t uMaxBack; // maximum size of global comment
	uint32_t uPosFound=0;

	FileSize = (uint32_t)fin.GetLength();
	uMaxBack = min<uint32_t>(0xffff, FileSize);

	uBackRead = 4;
	while (uBackRead < uMaxBack)
	{
		uint32_t uReadSize, uReadPos;
		int i;
		if (uBackRead + BUFREADCOMMENT > uMaxBack) 
			uBackRead = uMaxBack;
		else
			uBackRead += BUFREADCOMMENT;
		uReadPos = FileSize - uBackRead;

		uReadSize = min<uint32_t>((BUFREADCOMMENT + 4), (FileSize - uReadPos));

		if (fin.Seek(uReadPos, FileReader::SeekSet) != 0) break;

		if (fin.Read(buf, (int32_t)uReadSize) != (int32_t)uReadSize) break;

		for (i = (int)uReadSize - 3; (i--) > 0;)
		{
			if (buf[i] == 'P' && buf[i+1] == 'K' && buf[i+2] == 5 && buf[i+3] == 6 && !*zip64 && uPosFound == 0)
			{
				*zip64 = false;
				uPosFound = uReadPos + i;
			}
			if (buf[i] == 'P' && buf[i+1] == 'K' && buf[i+2] == 6 && buf[i+3] == 6)
			{
				*zip64 = true;
				uPosFound = uReadPos + i;
				return uPosFound;
			}
		}
	}
	return uPosFound;
}

//==========================================================================
//
// Zip file
//
//==========================================================================

FZipFile::FZipFile(const char * filename, FileReader &file)
: FResourceFile(filename, file)
{
	Lumps = NULL;
}

bool FZipFile::Open(bool quiet, LumpFilterInfo* filter)
{
	bool zip64 = false;
	uint32_t centraldir = Zip_FindCentralDir(Reader, &zip64);
	int skipped = 0;

	Lumps = NULL;

	if (centraldir == 0)
	{
		if (!quiet) Printf(TEXTCOLOR_RED "\n%s: ZIP file corrupt!\n", FileName.GetChars());
		return false;
	}

	uint64_t dirsize, DirectoryOffset;
	if (!zip64)
	{
		FZipEndOfCentralDirectory info;
		// Read the central directory info.
		Reader.Seek(centraldir, FileReader::SeekSet);
		Reader.Read(&info, sizeof(FZipEndOfCentralDirectory));

		// No multi-disk zips!
		if (info.NumEntries != info.NumEntriesOnAllDisks ||
			info.FirstDisk != 0 || info.DiskNumber != 0)
		{
			if (!quiet) Printf(TEXTCOLOR_RED "\n%s: Multipart Zip files are not supported.\n", FileName.GetChars());
			return false;
		}
		
		NumLumps = LittleShort(info.NumEntries);
		dirsize = LittleLong(info.DirectorySize);
		DirectoryOffset = LittleLong(info.DirectoryOffset);
	}
	else
	{
		FZipEndOfCentralDirectory64 info;
		// Read the central directory info.
		Reader.Seek(centraldir, FileReader::SeekSet);
		Reader.Read(&info, sizeof(FZipEndOfCentralDirectory64));

		// No multi-disk zips!
		if (info.NumEntries != info.NumEntriesOnAllDisks ||
			info.FirstDisk != 0 || info.DiskNumber != 0)
		{
			if (!quiet) Printf(TEXTCOLOR_RED "\n%s: Multipart Zip files are not supported.\n", FileName.GetChars());
			return false;
		}
		
		NumLumps = (uint32_t)info.NumEntries;
		dirsize = info.DirectorySize;
		DirectoryOffset = info.DirectoryOffset;
	}
	Lumps = new FZipLump[NumLumps];

	// Load the entire central directory. Too bad that this contains variable length entries...
	void *directory = malloc(dirsize);
	Reader.Seek(DirectoryOffset, FileReader::SeekSet);
	Reader.Read(directory, dirsize);

	char *dirptr = (char*)directory;
	FZipLump *lump_p = Lumps;

	FString name0, name1;
	bool foundspeciallump = false;
	bool foundprefix = false;

	// Check if all files have the same prefix so that this can be stripped out.
	// This will only be done if there is either a MAPINFO, ZMAPINFO or GAMEINFO lump in the subdirectory, denoting a ZDoom mod.
	if (NumLumps > 1) for (uint32_t i = 0; i < NumLumps; i++)
	{
		FZipCentralDirectoryInfo *zip_fh = (FZipCentralDirectoryInfo *)dirptr;

		int len = LittleShort(zip_fh->NameLength);
		FString name(dirptr + sizeof(FZipCentralDirectoryInfo), len);

		dirptr += sizeof(FZipCentralDirectoryInfo) +
			LittleShort(zip_fh->NameLength) +
			LittleShort(zip_fh->ExtraLength) +
			LittleShort(zip_fh->CommentLength);

		if (dirptr > ((char*)directory) + dirsize)	// This directory entry goes beyond the end of the file.
		{
			free(directory);
			if (!quiet) Printf(TEXTCOLOR_RED "\n%s: Central directory corrupted.", FileName.GetChars());
			return false;
		}

		name.ToLower();
		if (name.IndexOf("filter/") == 0)
			continue; // 'filter' is a reserved name of the file system.
		if (name.IndexOf("__macosx") == 0) 
			continue; // skip Apple garbage. At this stage only the root folder matters.
		if (!foundprefix)
		{
			// check for special names, if one of these gets found this must be treated as a normal zip.
			bool isspecial = name.IndexOf("/") < 0 || (filter && filter->reservedFolders.Find(name) < filter->reservedFolders.Size());
			if (isspecial) break;
			name0 = name.Left(name.LastIndexOf("/")+1);
			name1 = name.Left(name.IndexOf("/") + 1);
			foundprefix = true;
		}

		if (name.IndexOf(name0) != 0)
		{
			if (name1.IsNotEmpty())
			{
				name0 = name1;
				if (name.IndexOf(name0) != 0)
				{
					name0 = "";
				}
			}
			if (name0.IsEmpty()) 
				break;
		}
		if (!foundspeciallump && filter)
		{
			// at least one of the more common definition lumps must be present.
			for (auto &p : filter->requiredPrefixes)
			{ 
				if (name.IndexOf(name0 + p) == 0 || name.LastIndexOf(p) == ptrdiff_t(name.Len() - strlen(p)))
				{
					foundspeciallump = true;
					break;
				}
			}
		}
	}
	// If it ran through the list without finding anything it should not attempt any path remapping.
	if (!foundspeciallump) name0 = "";

	dirptr = (char*)directory;
	lump_p = Lumps;
	for (uint32_t i = 0; i < NumLumps; i++)
	{
		FZipCentralDirectoryInfo *zip_fh = (FZipCentralDirectoryInfo *)dirptr;

		int len = LittleShort(zip_fh->NameLength);
		FString name(dirptr + sizeof(FZipCentralDirectoryInfo), len);
		dirptr += sizeof(FZipCentralDirectoryInfo) + 
				  LittleShort(zip_fh->NameLength) + 
				  LittleShort(zip_fh->ExtraLength) + 
				  LittleShort(zip_fh->CommentLength);

		if (dirptr > ((char*)directory) + dirsize)	// This directory entry goes beyond the end of the file.
		{
			free(directory);
			if (!quiet) Printf(TEXTCOLOR_RED "\n%s: Central directory corrupted.", FileName.GetChars());
			return false;
		}

		if (name.IndexOf("__macosx") == 0 || name.IndexOf("__MACOSX") == 0)
		{
			skipped++;
			continue; // Weed out Apple's resource fork garbage right here because it interferes with safe operation.
		}
		if (name0.IsNotEmpty()) name = name.Mid(name0.Len());

		// skip Directories
		if (name.IsEmpty() || (name.Back() == '/' && LittleLong(zip_fh->UncompressedSize32) == 0))
		{
			skipped++;
			continue;
		}

		// Ignore unknown compression formats
		zip_fh->Method = LittleShort(zip_fh->Method);
		if (zip_fh->Method != METHOD_STORED &&
			zip_fh->Method != METHOD_DEFLATE &&
			zip_fh->Method != METHOD_LZMA &&
			zip_fh->Method != METHOD_BZIP2 &&
			zip_fh->Method != METHOD_IMPLODE &&
			zip_fh->Method != METHOD_SHRINK)
		{
			if (!quiet) Printf(TEXTCOLOR_YELLOW "\n%s: '%s' uses an unsupported compression algorithm (#%d).\n", FileName.GetChars(), name.GetChars(), zip_fh->Method);
			skipped++;
			continue;
		}
		// Also ignore encrypted entries
		zip_fh->Flags = LittleShort(zip_fh->Flags);
		if (zip_fh->Flags & ZF_ENCRYPTED)
		{
			if (!quiet) Printf(TEXTCOLOR_YELLOW "\n%s: '%s' is encrypted. Encryption is not supported.\n", FileName.GetChars(), name.GetChars());
			skipped++;
			continue;
		}

		FixPathSeperator(name);
		name.ToLower();
		
		uint32_t UncompressedSize =LittleLong(zip_fh->UncompressedSize32);
		uint32_t CompressedSize = LittleLong(zip_fh->CompressedSize32);
		uint64_t LocalHeaderOffset = LittleLong(zip_fh->LocalHeaderOffset32);
		if (zip_fh->ExtraLength > 0)
		{
			uint8_t* rawext = (uint8_t*)zip_fh + sizeof(*zip_fh) + zip_fh->NameLength;
			uint32_t ExtraLength = LittleLong(zip_fh->ExtraLength);
			
			while (ExtraLength > 0)
			{
				auto zip_64 = (FZipCentralDirectoryInfo64BitExt*)rawext;
				uint32_t BlockLength = LittleLong(zip_64->Length);
				rawext += BlockLength + 4;
				ExtraLength -= BlockLength + 4;
				if (LittleLong(zip_64->Type) == 1 && BlockLength >= 0x18)
				{
					if (zip_64->CompressedSize > 0x7fffffff || zip_64->UncompressedSize > 0x7fffffff)
					{
						// The file system is limited to 32 bit file sizes;
						if (!quiet) Printf(TEXTCOLOR_YELLOW "\n%s: '%s' is too large.\n", FileName.GetChars(), name.GetChars());
						skipped++;
						continue;
					}
					UncompressedSize = (uint32_t)zip_64->UncompressedSize;
					CompressedSize = (uint32_t)zip_64->CompressedSize;
					LocalHeaderOffset = zip_64->LocalHeaderOffset;
				}
			}
		}

		lump_p->LumpNameSetup(name);
		lump_p->LumpSize = UncompressedSize;
		lump_p->Owner = this;
		// The start of the Reader will be determined the first time it is accessed.
		lump_p->Flags = LUMPF_FULLPATH;
		lump_p->NeedFileStart = true;
		lump_p->Method = uint8_t(zip_fh->Method);
		if (lump_p->Method != METHOD_STORED) lump_p->Flags |= LUMPF_COMPRESSED;
		lump_p->GPFlags = zip_fh->Flags;
		lump_p->CRC32 = zip_fh->CRC32;
		lump_p->CompressedSize = CompressedSize;
		lump_p->Position = LocalHeaderOffset;
		lump_p->CheckEmbedded(filter);

		lump_p++;
	}
	// Resize the lump record array to its actual size
	NumLumps -= skipped;
	free(directory);

	GenerateHash();
	PostProcessArchive(&Lumps[0], sizeof(FZipLump), filter);
	return true;
}

//==========================================================================
//
// Zip file
//
//==========================================================================

FZipFile::~FZipFile()
{
	if (Lumps != NULL) delete [] Lumps;
}

//==========================================================================
//
//
//
//==========================================================================

FCompressedBuffer FZipLump::GetRawData()
{
	FCompressedBuffer cbuf = { (unsigned)LumpSize, (unsigned)CompressedSize, Method, GPFlags, CRC32, new char[CompressedSize] };
	if (NeedFileStart) SetLumpAddress();
	Owner->Reader.Seek(Position, FileReader::SeekSet);
	Owner->Reader.Read(cbuf.mBuffer, CompressedSize);
	return cbuf;
}

//==========================================================================
//
// SetLumpAddress
//
//==========================================================================

void FZipLump::SetLumpAddress()
{
	// This file is inside a zip and has not been opened before.
	// Position points to the start of the local file header, which we must
	// read and skip so that we can get to the actual file data.
	FZipLocalFileHeader localHeader;
	int skiplen;

	Owner->Reader.Seek(Position, FileReader::SeekSet);
	Owner->Reader.Read(&localHeader, sizeof(localHeader));
	skiplen = LittleShort(localHeader.NameLength) + LittleShort(localHeader.ExtraLength);
	Position += sizeof(localHeader) + skiplen;
	NeedFileStart = false;
}

//==========================================================================
//
// Get reader (only returns non-NULL if not encrypted)
//
//==========================================================================

FileReader *FZipLump::GetReader()
{
	// Don't return the reader if this lump is encrypted
	// In that case always force caching of the lump
	if (Method == METHOD_STORED)
	{
		if (NeedFileStart) SetLumpAddress();
		Owner->Reader.Seek(Position, FileReader::SeekSet);
		return &Owner->Reader;
	}
	else return NULL;	
}

//==========================================================================
//
// Fills the lump cache and performs decompression
//
//==========================================================================

int FZipLump::FillCache()
{
	if (NeedFileStart) SetLumpAddress();
	const char *buffer;

	if (Method == METHOD_STORED && (buffer = Owner->Reader.GetBuffer()) != NULL)
	{
		// This is an in-memory file so the cache can point directly to the file's data.
		Cache = const_cast<char*>(buffer) + Position;
		RefCount = -1;
		return -1;
	}

	Owner->Reader.Seek(Position, FileReader::SeekSet);
	Cache = new char[LumpSize];
	UncompressZipLump(Cache, Owner->Reader, Method, LumpSize, CompressedSize, GPFlags);
	RefCount = 1;
	return 1;
}

//==========================================================================
//
//
//
//==========================================================================

int FZipLump::GetFileOffset()
{
	if (Method != METHOD_STORED) return -1;
	if (NeedFileStart) SetLumpAddress();
	return (int)Position;
}

//==========================================================================
//
// File open
//
//==========================================================================

FResourceFile *CheckZip(const char *filename, FileReader &file, bool quiet, LumpFilterInfo* filter)
{
	char head[4];

	if (file.GetLength() >= (long)sizeof(FZipLocalFileHeader))
	{
		file.Seek(0, FileReader::SeekSet);
		file.Read(&head, 4);
		file.Seek(0, FileReader::SeekSet);
		if (!memcmp(head, "PK\x3\x4", 4))
		{
			auto rf = new FZipFile(filename, file);
			if (rf->Open(quiet, filter)) return rf;

			file = std::move(rf->Reader); // to avoid destruction of reader
			delete rf;
		}
	}
	return NULL;
}



//==========================================================================
//
// time_to_dos
//
// Converts time from struct tm to the DOS format used by zip files.
//
//==========================================================================

static std::pair<uint16_t, uint16_t> time_to_dos(struct tm *time)
{
	std::pair<uint16_t, uint16_t> val;
	if (time == NULL || time->tm_year < 80)
	{
		val.first = val.second = 0;
	}
	else
	{
		val.first = (time->tm_year - 80) * 512 + (time->tm_mon + 1) * 32 + time->tm_mday;
		val.second= time->tm_hour * 2048 + time->tm_min * 32 + time->tm_sec / 2;
	}
	return val;
}

//==========================================================================
//
// append_to_zip
//
// Write a given file to the zipFile.
// 
// zipfile: zip object to be written to
// 
// returns: position = success, -1 = error
//
//==========================================================================

int AppendToZip(FileWriter *zip_file, const char *filename, FCompressedBuffer &content, std::pair<uint16_t, uint16_t> &dostime)
{
	FZipLocalFileHeader local;
	int position;

	local.Magic = ZIP_LOCALFILE;
	local.VersionToExtract[0] = 20;
	local.VersionToExtract[1] = 0;
	local.Flags = content.mMethod == METHOD_DEFLATE ? LittleShort((uint16_t)2) : LittleShort((uint16_t)content.mZipFlags);
	local.Method = LittleShort((uint16_t)content.mMethod);
	local.ModDate = LittleShort(dostime.first);
	local.ModTime = LittleShort(dostime.second);
	local.CRC32 = content.mCRC32;
	local.UncompressedSize = LittleLong(content.mSize);
	local.CompressedSize = LittleLong(content.mCompressedSize);
	local.NameLength = LittleShort((unsigned short)strlen(filename));
	local.ExtraLength = 0;

	// Fill in local directory header.

	position = (int)zip_file->Tell();

	// Write out the header, file name, and file data.
	if (zip_file->Write(&local, sizeof(local)) != sizeof(local) ||
		zip_file->Write(filename, strlen(filename)) != strlen(filename) ||
		zip_file->Write(content.mBuffer, content.mCompressedSize) != content.mCompressedSize)
	{
		return -1;
	}
	return position;
}


//==========================================================================
//
// write_central_dir
//
// Writes the central directory entry for a file.
//
//==========================================================================

int AppendCentralDirectory(FileWriter *zip_file, const char *filename, FCompressedBuffer &content, std::pair<uint16_t, uint16_t> &dostime, int position)
{
	FZipCentralDirectoryInfo dir;

	dir.Magic = ZIP_CENTRALFILE;
	dir.VersionMadeBy[0] = 20;
	dir.VersionMadeBy[1] = 0;
	dir.VersionToExtract[0] = 20;
	dir.VersionToExtract[1] = 0;
	dir.Flags = content.mMethod == METHOD_DEFLATE ? LittleShort((uint16_t)2) : LittleShort((uint16_t)content.mZipFlags);
	dir.Method = LittleShort((uint16_t)content.mMethod);
	dir.ModTime = LittleShort(dostime.first);
	dir.ModDate = LittleShort(dostime.second);
	dir.CRC32 = content.mCRC32;
	dir.CompressedSize32 = LittleLong(content.mCompressedSize);
	dir.UncompressedSize32 = LittleLong(content.mSize);
	dir.NameLength = LittleShort((unsigned short)strlen(filename));
	dir.ExtraLength = 0;
	dir.CommentLength = 0;
	dir.StartingDiskNumber = 0;
	dir.InternalAttributes = 0;
	dir.ExternalAttributes = 0;
	dir.LocalHeaderOffset32 = LittleLong(position);

	if (zip_file->Write(&dir, sizeof(dir)) != sizeof(dir) ||
		zip_file->Write(filename,  strlen(filename)) != strlen(filename))
	{
		return -1;
	}
	return 0;
}

bool WriteZip(const char *filename, TArray<FString> &filenames, TArray<FCompressedBuffer> &content)
{
	// try to determine local time
	struct tm *ltime;
	time_t ttime;
	ttime = time(nullptr);
	ltime = localtime(&ttime);
	auto dostime = time_to_dos(ltime);

	TArray<int> positions;

	if (filenames.Size() != content.Size()) return false;

	auto f = FileWriter::Open(filename);
	if (f != nullptr)
	{
		for (unsigned i = 0; i < filenames.Size(); i++)
		{
			int pos = AppendToZip(f, filenames[i], content[i], dostime);
			if (pos == -1)
			{
				delete f;
				remove(filename);
				return false;
			}
			positions.Push(pos);
		}

		int dirofs = (int)f->Tell();
		for (unsigned i = 0; i < filenames.Size(); i++)
		{
			if (AppendCentralDirectory(f, filenames[i], content[i], dostime, positions[i]) < 0)
			{
				delete f;
				remove(filename);
				return false;
			}
		}

		// Write the directory terminator.
		FZipEndOfCentralDirectory dirend;
		dirend.Magic = ZIP_ENDOFDIR;
		dirend.DiskNumber = 0;
		dirend.FirstDisk = 0;
		dirend.NumEntriesOnAllDisks = dirend.NumEntries = LittleShort((uint16_t)filenames.Size());
		dirend.DirectoryOffset = LittleLong(dirofs);
		dirend.DirectorySize = LittleLong((uint32_t)(f->Tell() - dirofs));
		dirend.ZipCommentLength = 0;
		if (f->Write(&dirend, sizeof(dirend)) != sizeof(dirend))
		{
			delete f;
			remove(filename);
			return false;
		}
		delete f;
		return true;
	}
	return false;
}

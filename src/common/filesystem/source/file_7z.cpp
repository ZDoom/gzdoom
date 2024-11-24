/*
** file_7z.cpp
**
**---------------------------------------------------------------------------
** Copyright 2009 Randy Heit
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

// Note that 7z made the unwise decision to include windows.h :(
#include "7z.h"
#include "7zCrc.h"
#include "resourcefile.h"
#include "fs_findfile.h"
#include "unicode.h"
#include "critsec.h"
#include <mutex>


namespace FileSys {
	
//-----------------------------------------------------------------------
//
// Interface classes to 7z library
//
//-----------------------------------------------------------------------

extern ISzAlloc g_Alloc;

struct CZDFileInStream
{
	ISeekInStream s;
	FileReader &File;

	CZDFileInStream(FileReader &_file) 
		: File(_file)
	{
		s.Read = Read;
		s.Seek = Seek;
	}

	static SRes Read(const ISeekInStream *pp, void *buf, size_t *size)
	{
		CZDFileInStream *p = (CZDFileInStream *)pp;
		auto numread = p->File.Read(buf, (ptrdiff_t)*size);
		if (numread < 0)
		{
			*size = 0;
			return SZ_ERROR_READ;
		}
		*size = numread;
		return SZ_OK;
	}

	static SRes Seek(const ISeekInStream *pp, Int64 *pos, ESzSeek origin)
	{
		CZDFileInStream *p = (CZDFileInStream *)pp;
		FileReader::ESeek move_method;
		int res;
		if (origin == SZ_SEEK_SET)
		{
			move_method = FileReader::SeekSet;
		}
		else if (origin == SZ_SEEK_CUR)
		{
			move_method = FileReader::SeekCur;
		}
		else if (origin == SZ_SEEK_END)
		{
			move_method = FileReader::SeekEnd;
		}
		else
		{
			return 1;
		}
		res = (int)p->File.Seek((ptrdiff_t)*pos, move_method);
		*pos = p->File.Tell();
		return res;
	}
};

struct C7zArchive
{
	CSzArEx DB;
	CZDFileInStream ArchiveStream;
	CLookToRead2 LookStream;
	Byte StreamBuffer[1<<14];
	UInt32 BlockIndex;
	Byte *OutBuffer;
	size_t OutBufferSize;

	C7zArchive(FileReader &file) : ArchiveStream(file)
	{
		if (g_CrcTable[1] == 0)
		{
			CrcGenerateTable();
		}
		file.Seek(0, FileReader::SeekSet);
		LookToRead2_CreateVTable(&LookStream, false);
		LookStream.realStream = &ArchiveStream.s;
		LookToRead2_INIT(&LookStream);
		LookStream.bufSize = sizeof(StreamBuffer);
		LookStream.buf = StreamBuffer;
		SzArEx_Init(&DB);
		BlockIndex = 0xFFFFFFFF;
		OutBuffer = NULL;
		OutBufferSize = 0;
	}

	~C7zArchive()
	{
		if (OutBuffer != NULL)
		{
			IAlloc_Free(&g_Alloc, OutBuffer);
		}
		SzArEx_Free(&DB, &g_Alloc);
	}

	SRes Open()
	{
		return SzArEx_Open(&DB, &LookStream.vt, &g_Alloc, &g_Alloc);
	}

	SRes Extract(UInt32 file_index, char *buffer)
	{
		size_t offset, out_size_processed;
		SRes res = SzArEx_Extract(&DB, &LookStream.vt, file_index,
			&BlockIndex, &OutBuffer, &OutBufferSize,
			&offset, &out_size_processed,
			&g_Alloc, &g_Alloc);
		if (res == SZ_OK)
		{
			memcpy(buffer, OutBuffer + offset, out_size_processed);
		}
		return res;
	}
};

//==========================================================================
//
// 7-zip file
//
//==========================================================================

class F7ZFile : public FResourceFile
{
	friend struct F7ZLump;

	C7zArchive *Archive;
	FCriticalSection critsec;

public:
	F7ZFile(const char * filename, FileReader &filer, StringPool* sp);
	bool Open(FileSystemFilterInfo* filter, FileSystemMessageFunc Printf);
	virtual ~F7ZFile();
	FileData Read(uint32_t entry) override;
	FileReader GetEntryReader(uint32_t entry, int, int) override;
};



//==========================================================================
//
// 7Z file
//
//==========================================================================

F7ZFile::F7ZFile(const char * filename, FileReader &filer, StringPool* sp)
	: FResourceFile(filename, filer, sp, 0) 
{
	Archive = nullptr;
}


//==========================================================================
//
// Open it
//
//==========================================================================

bool F7ZFile::Open(FileSystemFilterInfo *filter, FileSystemMessageFunc Printf)
{
	Archive = new C7zArchive(Reader);
	int skipped = 0;
	SRes res;

	res = Archive->Open();
	if (res != SZ_OK)
	{
		delete Archive;
		Archive = NULL;
		if (res == SZ_ERROR_UNSUPPORTED)
		{
			Printf(FSMessageLevel::Error, "%s: Decoder does not support this archive\n", FileName);
		}
		else if (res == SZ_ERROR_MEM)
		{
			Printf(FSMessageLevel::Error, "Cannot allocate memory\n");
		}
		else if (res == SZ_ERROR_CRC)
		{
			Printf(FSMessageLevel::Error, "CRC error\n");
		}
		else
		{
			Printf(FSMessageLevel::Error, "error #%d\n", res);
		}
		return false;
	}

	CSzArEx* const archPtr = &Archive->DB;

	AllocateEntries(archPtr->NumFiles);
	NumLumps = archPtr->NumFiles;

	std::u16string nameUTF16;
	std::vector<char> nameASCII;

	uint32_t j = 0;
	for (uint32_t i = 0; i < NumLumps; ++i)
	{
		// skip Directories
		if (SzArEx_IsDir(archPtr, i))
		{
			continue;
		}

		const size_t nameLength = SzArEx_GetFileNameUtf16(archPtr, i, NULL);

		if (0 == nameLength)
		{
			continue;
		}

		nameUTF16.resize((unsigned)nameLength);
		nameASCII.resize((unsigned)nameLength);

		SzArEx_GetFileNameUtf16(archPtr, i, (UInt16*)nameUTF16.data());
		utf16_to_utf8((uint16_t*)nameUTF16.data(), nameASCII);

		Entries[j].FileName = NormalizeFileName(nameASCII.data());
		Entries[j].Length = SzArEx_GetFileSize(archPtr, i);
		Entries[j].Flags = RESFF_FULLPATH|RESFF_COMPRESSED;
		Entries[j].ResourceID = -1;
		Entries[j].Method = METHOD_INVALID;
		Entries[j].Position = i;
		j++;
	}
	// Resize the lump record array to its actual size
	NumLumps = j;

	if (NumLumps > 0)
	{
		// Quick check for unsupported compression method

		FileData temp(nullptr, Entries[0].Length);

		if (SZ_OK != Archive->Extract((UInt32)Entries[0].Position, (char*)temp.writable()))
		{
			Printf(FSMessageLevel::Error, "%s: unsupported 7z/LZMA file!\n", FileName);
			return false;
		}
	}

	GenerateHash();
	PostProcessArchive(filter);
	return true;
}

//==========================================================================
//
// 
//
//==========================================================================

F7ZFile::~F7ZFile()
{
	if (Archive != nullptr)
	{
		delete Archive;
	}
}

//==========================================================================
//
// Reads data for one entry into a buffer
//
//==========================================================================

FileData F7ZFile::Read(uint32_t entry)
{
	FileData buffer;
	if (entry < NumLumps && Entries[entry].Length > 0)
	{
		auto p = buffer.allocate(Entries[entry].Length);
		// There is no realistic way to keep multiple references to a 7z file open without massive overhead so to make this thread-safe a mutex is the only option.
		std::lock_guard<FCriticalSection> lock(critsec);
		SRes code = Archive->Extract((UInt32)Entries[entry].Position, (char*)p);
		if (code != SZ_OK) buffer.clear();
	}
	return buffer;
}

//==========================================================================
//
// This can only return a FileReader to a memory buffer.
//
//==========================================================================

FileReader F7ZFile::GetEntryReader(uint32_t entry, int, int)
{
	FileReader fr;
	if (entry < 0 || entry >= NumLumps) return fr;
	auto buffer = Read(entry);
	if (buffer.size() > 0)
		fr.OpenMemoryArray(buffer);
	return fr;
}


//==========================================================================
//
// File open
//
//==========================================================================

FResourceFile *Check7Z(const char *filename, FileReader &file, FileSystemFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp)
{
	char head[k7zSignatureSize];

	if (file.GetLength() >= k7zSignatureSize)
	{
		file.Seek(0, FileReader::SeekSet);
		file.Read(&head, k7zSignatureSize);
		file.Seek(0, FileReader::SeekSet);
		if (!memcmp(head, k7zSignature, k7zSignatureSize))
		{
			auto rf = new F7ZFile(filename, file, sp);
			if (rf->Open(filter, Printf)) return rf;

			file = rf->Destroy();
		}
	}
	return NULL;
}


}

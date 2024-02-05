/*
** writezip.cpp
**
**---------------------------------------------------------------------------
** Copyright 1998-2009 Randy Heit
** Copyright 2005-2023 Christoph Oelckers
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

#include <stdint.h>
#include <algorithm>
#include "tarray.h"
#include "files.h"
#include "m_swap.h"
#include "w_zip.h"
#include "fs_decompress.h"

using FileSys::FCompressedBuffer;



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
		val.first = time->tm_hour * 2048 + time->tm_min * 32 + time->tm_sec / 2;
		val.second = (time->tm_year - 80) * 512 + (time->tm_mon + 1) * 32 + time->tm_mday;
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

static int AppendToZip(FileWriter *zip_file, const FCompressedBuffer &content, std::pair<uint16_t, uint16_t> &dostime)
{
	FZipLocalFileHeader local;
	int position;

	int flags = 0;
	int method = content.mMethod;
	if (method >= FileSys::METHOD_IMPLODE_MIN && method <= FileSys::METHOD_IMPLODE_MAX)
	{
		flags = method - FileSys::METHOD_IMPLODE_MIN;
		method = FileSys::METHOD_IMPLODE;
	}
	else if (method == FileSys::METHOD_DEFLATE)
	{
		flags = 2;
	}
	else if (method >= 1337)
		return -1;

	local.Magic = ZIP_LOCALFILE;
	local.VersionToExtract[0] = 20;
	local.VersionToExtract[1] = 0;
	local.Flags = LittleShort((uint16_t)flags);
	local.Method = LittleShort((uint16_t)method);
	local.ModDate = LittleShort(dostime.first);
	local.ModTime = LittleShort(dostime.second);
	local.CRC32 = content.mCRC32;
	local.UncompressedSize = LittleLong((unsigned)content.mSize);
	local.CompressedSize = LittleLong((unsigned)content.mCompressedSize);
	local.NameLength = LittleShort((unsigned short)strlen(content.filename));
	local.ExtraLength = 0;

	// Fill in local directory header.

	position = (int)zip_file->Tell();

	// Write out the header, file name, and file data.
	if (zip_file->Write(&local, sizeof(local)) != sizeof(local) ||
		zip_file->Write(content.filename, strlen(content.filename)) != strlen(content.filename) ||
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

int AppendCentralDirectory(FileWriter *zip_file, const FCompressedBuffer &content, std::pair<uint16_t, uint16_t> &dostime, int position)
{
	FZipCentralDirectoryInfo dir;

	int flags = 0;
	int method = content.mMethod;
	if (method >= FileSys::METHOD_IMPLODE_MIN && method <= FileSys::METHOD_IMPLODE_MAX)
	{
		flags = method - FileSys::METHOD_IMPLODE_MIN;
		method = FileSys::METHOD_IMPLODE;
	}
	else if (method == FileSys::METHOD_DEFLATE)
	{
		flags = 2;
	}
	else if (method >= 1337)
		return -1;

	dir.Magic = ZIP_CENTRALFILE;
	dir.VersionMadeBy[0] = 20;
	dir.VersionMadeBy[1] = 0;
	dir.VersionToExtract[0] = 20;
	dir.VersionToExtract[1] = 0;
	dir.Flags = LittleShort((uint16_t)flags);
	dir.Method = LittleShort((uint16_t)method);
	dir.ModTime = LittleShort(dostime.first);
	dir.ModDate = LittleShort(dostime.second);
	dir.CRC32 = content.mCRC32;
	dir.CompressedSize32 = LittleLong((unsigned)content.mCompressedSize);
	dir.UncompressedSize32 = LittleLong((unsigned)content.mSize);
	dir.NameLength = LittleShort((unsigned short)strlen(content.filename));
	dir.ExtraLength = 0;
	dir.CommentLength = 0;
	dir.StartingDiskNumber = 0;
	dir.InternalAttributes = 0;
	dir.ExternalAttributes = 0;
	dir.LocalHeaderOffset32 = LittleLong((unsigned)position);

	if (zip_file->Write(&dir, sizeof(dir)) != sizeof(dir) ||
		zip_file->Write(content.filename,  strlen(content.filename)) != strlen(content.filename))
	{
		return -1;
	}
	return 0;
}

bool WriteZip(const char* filename, const FCompressedBuffer* content, size_t contentcount)
{
	// try to determine local time
	struct tm *ltime;
	time_t ttime;
	ttime = time(nullptr);
	ltime = localtime(&ttime);
	auto dostime = time_to_dos(ltime);

	TArray<int> positions;

	auto f = FileWriter::Open(filename);
	if (f != nullptr)
	{
		for (size_t i = 0; i < contentcount; i++)
		{
			int pos = AppendToZip(f, content[i], dostime);
			if (pos == -1)
			{
				delete f;
				remove(filename);
				return false;
			}
			positions.Push(pos);
		}

		int dirofs = (int)f->Tell();
		for (size_t i = 0; i < contentcount; i++)
		{
			if (AppendCentralDirectory(f, content[i], dostime, positions[i]) < 0)
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
		dirend.NumEntriesOnAllDisks = dirend.NumEntries = LittleShort((uint16_t)contentcount);
		dirend.DirectoryOffset = LittleLong((unsigned)dirofs);
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

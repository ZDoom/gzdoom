//	$Id: file.cpp,v 1.6 1999/12/28 11:14:05 cisc Exp $

#include <stdio.h>
#include "fmgen_types.h"
#include "fmgen_headers.h"
#include "fmgen_file.h"

// ---------------------------------------------------------------------------
//	構築/消滅
// ---------------------------------------------------------------------------

FileIO::FileIO()
{
	flags = 0;
}

FileIO::FileIO(const char* filename, uint flg)
{
	flags = 0;
	Open(filename, flg);
}

FileIO::~FileIO()
{
	Close();
}

// ---------------------------------------------------------------------------
//	ファイルを開く
// ---------------------------------------------------------------------------

bool FileIO::Open(const char* filename, uint flg)
{
	char mode[5] = "rwb";
	Close();

	strncpy(path, filename, MAX_PATH - 1);

	if(flg & readonly)
		strcpy(mode, "rb");
	else {
		if(flg & create)
			strcpy(mode, "rwb+");
		else
			strcpy(mode, "rwb");
	}

	pfile = fopen(filename, mode);

	flags = (flg & readonly) | (pfile == NULL ? 0 : open);

	if (pfile == NULL)
		error = file_not_found;

	SetLogicalOrigin(0);

	return !(pfile == NULL);
}

// ---------------------------------------------------------------------------
//	ファイルがない場合は作成
// ---------------------------------------------------------------------------

bool FileIO::CreateNew(const char* filename)
{
	uint flg = create;

	return Open(filename, flg);
}

// ---------------------------------------------------------------------------
//	ファイルを作り直す
// ---------------------------------------------------------------------------

bool FileIO::Reopen(uint flg)
{
	if (!(flags & open)) return false;
	if ((flags & readonly) && (flg & create)) return false;

	if (flags & readonly) flg |= readonly;

	return Open(path, flg);
}

// ---------------------------------------------------------------------------
//	ファイルを閉じる
// ---------------------------------------------------------------------------

void FileIO::Close()
{
	if (GetFlags() & open)
	{
		fclose(pfile);
		flags = 0;
	}
}

// ---------------------------------------------------------------------------
//	ファイル殻の読み出し
// ---------------------------------------------------------------------------

int32 FileIO::Read(void* dest, int32 size)
{
	if (!(GetFlags() & open))
		return -1;

	size_t readsize;
	if (!(readsize = fread(dest, 1, static_cast<size_t>(size), pfile)))
		return -1;
	return size;
}

// ---------------------------------------------------------------------------
//	ファイルへの書き出し
// ---------------------------------------------------------------------------

int32 FileIO::Write(const void* dest, int32 size)
{
	if (!(GetFlags() & open) || (GetFlags() & readonly))
		return -1;

	size_t writtensize;
	if (!(writtensize = fwrite(dest, 1, static_cast<size_t>(size), pfile)))
		return -1;
	return static_cast<int32>(writtensize);
}

// ---------------------------------------------------------------------------
//	ファイルをシーク
// ---------------------------------------------------------------------------

bool FileIO::Seek(int32 pos, SeekMethod method)
{
	if (!(GetFlags() & open))
		return false;

	int origin;
	switch (method)
	{
	case begin:
		origin = SEEK_SET;
		break;
	case current:
		origin = SEEK_CUR;
		break;
	case end:
		origin = SEEK_END;
		break;
	default:
		return false;
	}

	return (fseek(pfile, pos, origin) != 0);
}

// ---------------------------------------------------------------------------
//	ファイルの位置を得る
// ---------------------------------------------------------------------------

int32 FileIO::Tellp()
{
	if (!(GetFlags() & open))
		return 0;

	return static_cast<int32>(ftell(pfile));
}

// ---------------------------------------------------------------------------
//	現在の位置をファイルの終端とする
// ---------------------------------------------------------------------------

bool FileIO::SetEndOfFile()
{
	if (!(GetFlags() & open))
		return false;
	return Seek(0, end);
}

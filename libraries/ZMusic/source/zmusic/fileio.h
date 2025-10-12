/*
    The common sound font reader interface. Used by GUS, Timidity++ and WildMidi 
    backends for reading sound font configurations.
	
    The FileInterface is also used by streaming sound formats.
	
    Copyright (C) 2019 Christoph Oelckers

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/ 


#pragma once
#include <stdio.h>
#include <string.h>
#include <cstdint>
#include <vector>
#include <string>

#if defined _WIN32 && !defined _WINDOWS_	// only define this if windows.h is not included.
	// I'd rather not include Windows.h for just this. This header is not supposed to pollute everything it touches.
extern "C" __declspec(dllimport) int __stdcall MultiByteToWideChar(unsigned CodePage, unsigned long  dwFlags, const char* lpMultiByteStr, int cbMultiByte, const wchar_t* lpWideCharStr, int cchWideChar);
enum
{
	CP_UTF8 = 65001
};
#endif

namespace MusicIO
{

//==========================================================================
//
// This class defines a common file wrapper interface which allows these
// libraries to work with any kind of file access API, e.g. stdio (provided below),
// Win32, POSIX, iostream or custom implementations (like GZDoom's FileReader.)
//
//==========================================================================

struct FileInterface
{
	std::string filename;
	long length = -1;

	// It's really too bad that the using code requires long instead of size_t.
	// Fortunately 2GB files are unlikely to come by here.
protected:
	// 
	virtual ~FileInterface() {}
public:
	virtual char* gets(char* buff, int n) = 0;
	virtual long read(void* buff, int32_t size) = 0;
	virtual long seek(long offset, int whence) = 0;
	virtual long tell() = 0;
	virtual void close()
	{
		delete this;
	}

	long filelength()
	{
		if (length == -1)
		{
			long pos = tell();
			seek(0, SEEK_END);
			length = tell();
			seek(pos, SEEK_SET);
		}
		return length;
	}
};

//==========================================================================
//
// Inplementation of the FileInterface for stdio's FILE*.
//
//==========================================================================

struct StdioFileReader : public FileInterface
{
	FILE* f = nullptr;

	~StdioFileReader()
	{
		if (f) fclose(f);
	}
	char* gets(char* buff, int n) override
	{
		if (!f) return nullptr;
		return fgets(buff, n, f);
	}
	long read(void* buff, int32_t size) override
	{
		if (!f) return 0;
		return (long)fread(buff, 1, size, f);
	}
	long seek(long offset, int whence) override
	{
		if (!f) return 0;
		return fseek(f, offset, whence);
	}
	long tell() override
	{
		if (!f) return 0;
		return ftell(f);
	}
};


//==========================================================================
//
// Inplementation of the FileInterface for a block of memory
//
//==========================================================================

struct MemoryReader : public FileInterface
{
	const uint8_t *mData;
	long mLength;
	long mPos;
	
	MemoryReader(const uint8_t *data, long length)
	 : mData(data), mLength(length), mPos(0)
	{
	}
	
	char* gets(char* strbuf, int len) override
	{
		if (len > mLength - mPos) len = mLength - mPos;
		if (len <= 0) return NULL;

		char *p = strbuf;
		while (len > 1)
		{
			if (mData[mPos] == 0)
			{
				mPos++;
				break;
			}
			if (mData[mPos] != '\r')
			{
				*p++ = mData[mPos];
				len--;
				if (mData[mPos] == '\n')
				{
					mPos++;
					break;
				}
			}
			mPos++;
		}
		if (p == strbuf) return nullptr;
		*p++ = 0;
		return strbuf;
	}
	long read(void* buff, int32_t size) override
	{
		long len = long(size);
		if (len > mLength - mPos) len = mLength - mPos;
		if (len < 0) len = 0;
		memcpy(buff, mData + mPos, len);
		mPos += len;
		return len;
	}
	long seek(long offset, int whence) override
	{
		switch (whence)
		{
		case SEEK_CUR:
			offset += mPos;
			break;

		case SEEK_END:
			offset += mLength;
			break;

		}
		if (offset < 0 || offset > mLength) return -1;
		mPos = offset;
		return 0;
	}
	long tell() override
	{
		return mPos;
	}
protected:
	MemoryReader() {}
};

//==========================================================================
//
// Inplementation of the FileInterface for an std::vector owned by the reader
//
//==========================================================================

struct VectorReader : public MemoryReader
{
	std::vector<uint8_t> mVector;

	template <class getFunc>
	VectorReader(getFunc getter)	// read contents to a buffer and return a reader to it
	{
		getter(mVector);
		mData = mVector.data();
		mLength = (long)mVector.size();
		mPos = 0;
	}
	VectorReader(const uint8_t* data, size_t size)
	{
		mVector.resize(size);
		memcpy(mVector.data(), data, size);
		mData = mVector.data();
		mLength = (long)size;
		mPos = 0;
	}
};


//==========================================================================
//
// The following two functions are needed to allow using UTF-8 in the file interface.
// fopen on Windows is only safe for ASCII.
//
//==========================================================================

#ifdef _WIN32
inline std::wstring wideString(const char *filename)
{
	std::wstring filePathW;
	auto len = strlen(filename);
	filePathW.resize(len);
	int newSize = MultiByteToWideChar(CP_UTF8, 0, filename, (int)len, const_cast<wchar_t*>(filePathW.c_str()), (int)len);
	filePathW.resize(newSize);
	return filePathW;
}
#endif

inline FILE* utf8_fopen(const char* filename, const char *mode)
{
#ifndef _WIN32
	return fopen(filename, mode);
#else
	auto fn = wideString(filename);
	auto mo = wideString(mode);
	return _wfopen(fn.c_str(), mo.c_str());
#endif

}

inline bool fileExists(const char *fn)
{
	FILE *f = utf8_fopen(fn, "rb");
	if (!f) return false;
	fclose(f);
	return true;
}

//==========================================================================
//
// This class providea a framework for reading sound fonts.
// This is needed when the sound font data is not read from
// the file system. e.g. zipped GUS patch sets.
//
//==========================================================================

class SoundFontReaderInterface
{
protected:
	virtual ~SoundFontReaderInterface() {}
public:
	virtual struct FileInterface* open_file(const char* fn) = 0;
	virtual void add_search_path(const char* path) = 0;
	virtual void close() { delete this; }
};


//==========================================================================
//
// A basic sound font reader for reading data from the file system.
//
//==========================================================================

class FileSystemSoundFontReader : public SoundFontReaderInterface
{
protected:
	std::vector<std::string> mPaths;
	std::string mBaseFile;
	bool mAllowAbsolutePaths;

	bool IsAbsPath(const char *name)
	{
		if (name[0] == '/' || name[0] == '\\') return true;
		#ifdef _WIN32
			/* [A-Za-z]: (for Windows) */
			if (isalpha(name[0]) && name[1] == ':') return true;
		#endif /* _WIN32 */
		return 0;
	}

public:
	FileSystemSoundFontReader(const char *configfilename, bool allowabs = false)
	{
		// Note that this does not add the directory the base file is in to the search path!
		// The caller of this has to do it themselves!
		mBaseFile = configfilename;
		mAllowAbsolutePaths = allowabs;
	}

	struct FileInterface* open_file(const char* fn) override
	{
		FILE *f = nullptr;
		std::string fullname;
		if (!fn) 
		{
			f = utf8_fopen(mBaseFile.c_str(), "rb");
			fullname = mBaseFile;
		}
		else
		{
			if (!IsAbsPath(fn))
			{
				for(int i = (int)mPaths.size()-1; i>=0; i--)
				{
					fullname = mPaths[i] + fn;
					f = utf8_fopen(fullname.c_str(), "rb");
					if (f) break;
				}
			}
			if (!f) f = fopen(fn, "rb");
		}
		if (!f) return nullptr;
		auto tf = new StdioFileReader;
		tf->f = f;
		tf->filename = fullname;
		return tf;
	}
	
	void add_search_path(const char* path) override
	{
		std::string p  = path;
		if (p.back() != '/' && p.back() != '\\') p += '/';	// always let it end with a slash.
		mPaths.push_back(p);
	}
};

//==========================================================================
//
// This reader exists to trick Timidity config readers into accepting
// a loose SF2 file by providing a fake config pointing to the given file.
//
//==========================================================================

class SF2Reader : public FileSystemSoundFontReader
{
	std::string mMainConfigForSF2;

public:
    SF2Reader(const char *filename)
		: FileSystemSoundFontReader(filename)
	{
		mMainConfigForSF2 = "soundfont \"" + mBaseFile + "\"\n";
	}
	
	struct FileInterface* open_file(const char* fn) override
	{
		if (fn == nullptr)
		{
			return new MemoryReader((uint8_t*)mMainConfigForSF2.c_str(), (long)mMainConfigForSF2.length());
		}
		else return FileSystemSoundFontReader::open_file(fn);
	}
};

MusicIO::SoundFontReaderInterface* ClientOpenSoundFont(const char* name, int type);

} 


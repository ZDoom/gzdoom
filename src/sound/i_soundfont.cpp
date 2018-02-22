/*
** i_soundfont.cpp
** The sound font manager for the MIDI synths
**
**---------------------------------------------------------------------------
** Copyright 2018 Christoph Oelckers
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
*/

#include <ctype.h>
#include <assert.h>
#include <stdio.h>
#include "i_soundfont.h"
#include "cmdlib.h"
#include "w_wad.h"
#include "i_system.h"
#include "gameconfigfile.h"
#include "resourcefiles/resourcefile.h"


//==========================================================================
//
//
//
//==========================================================================

FSoundFontReader::FSoundFontReader(FWadCollection *coll, const FSoundFontInfo *sfi)
{
	collection = coll;
	if (sfi->type == SF_SF2)
	{
		mMainConfigForSF2.Format("soundfont %s", sfi->mFilename.GetChars());
	}
	if (coll != nullptr)
	{
		auto num = coll->GetNumWads();
		for(int i = 0; i < num; i++)
		{
			auto wadname = ExtractFileBase(coll->GetWadFullName(i), false);
			if (sfi->mName.CompareNoCase(wadname) == 0)
			{
				// For the given sound font we may only read from this file and no other.
				// This is to avoid conflicts with duplicate patches between patch sets.
				// For SF2 fonts it's just an added precaution in cause some zipped patch set
				// contains extraneous data that gets in the way.
				mFindInfile = i;
				break;
			}
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

FileReader *FSoundFontReader::OpenMainConfigFile()
{
	if (mMainConfigForSF2.IsNotEmpty())
	{
		return new MemoryReader(mMainConfigForSF2.GetChars(), mMainConfigForSF2.Len());
	}
	else
	{
		auto lump = collection->CheckNumForFullName("timidity.cfg", mFindInfile);
		return lump < 0? nullptr : collection->ReopenLumpNum(lump);
	}
}

//==========================================================================
//
//
//
//==========================================================================

FileReader *FSoundFontReader::OpenFile(const char *name)
{
	auto lump = collection->CheckNumForFullName(name, mFindInfile);
	if (lump >= 0)
	{
		// For SF2 files return the backing file reader to avoid reopening the file.
		// Note that there is no possibility of a non-SF2 resource file having only one lump due to the check in the init code.
		if (collection->GetLumpCount(mFindInfile) == 1)
		{
			auto fr = collection->GetFileReader(mFindInfile);
			fr->Seek(0, SEEK_SET);
			return fr;
		}
		return collection->ReopenLumpNum(lump);
	}
	return nullptr;
}

//==========================================================================
//
// returns both a file reader and the full name of the looked up file
//
//==========================================================================

std::pair<FileReader *, FString> FSoundFontReader::LookupFile(const char *name)
{
	if (IsAbsPath(name))
	{
		auto fr = OpenFile(name);
		if (fr != nullptr) return std::make_pair(fr, name);
	}
	else
	{
		for(int i = mPaths.Size()-1; i>=0; i--)
		{
			FString fullname = mPaths[i] + name;
			auto fr = OpenFile(fullname);
			if (fr != nullptr) return std::make_pair(fr, fullname);
		}
	}
	return std::make_pair(nullptr, FString());
}

//==========================================================================
//
// This adds a directory to the path list
//
//==========================================================================

void FSoundFontReader::AddPath(const char *strp)
{
	if (*strp == 0) return;
	if (!mAllowAbsolutePaths && IsAbsPath(strp)) return;	// of no use so we may just discard it right away
	int i = 0;
	FString str = strp;
	FixPathSeperator(str);
	if (str.Back() != '/') str += '/';	// always let it end with a slash.
	for (auto &s : mPaths)
	{
		if (pathcmp(s.GetChars(), str) == 0)
		{
			// move string to the back.
			mPaths.Delete(i);
			mPaths.Push(str);
			return;
		}
		i++;
	}
	mPaths.Push(str);
}

int FSoundFontReader::pathcmp(const char *p1, const char *p2)
{
	return mCaseSensitivePaths? strcmp(p1, p2) : stricmp(p1, p2);
}

//==========================================================================
//
// Note that the file type has already been checked
//
//==========================================================================

FSF2Reader::FSF2Reader(const char *fn)
{
	mMainConfigForSF2.Format("soundfont %s", fn);
	mFilename = fn;
}

FileReader *FSF2Reader::OpenFile(const char *name)
{
	if (mFilename.CompareNoCase(name) == 0)
	{
		auto fr = new FileReader;
		if (fr->Open(name)) return fr;
		delete fr;
	}
	return nullptr;
}

//==========================================================================
//
//
//
//==========================================================================

FZipPatReader::FZipPatReader(const char *filename)
{
	resf = FResourceFile::OpenResourceFile(filename, nullptr);
}

FZipPatReader::~FZipPatReader()
{
	if (resf != nullptr) delete resf;
}

FileReader *FZipPatReader::OpenFile(const char *name)
{
	if (resf != nullptr)
	{
		auto lump = resf->FindLump(name);
		if (lump != nullptr)
		{
			return lump->NewReader();
		}
	}
	return nullptr;
}

//==========================================================================
//
//
//
//==========================================================================

FPatchSetReader::FPatchSetReader(const char *filename)
{
#ifndef _WIN32
	mCaseSensitivePaths = true;
	const char *paths[] = {
		"/usr/local/lib/timidity",
		"/etc/timidity",
		"/etc"
	};
#else
	const char *paths[] = {
		"C:/TIMIDITY",
		"/TIMIDITY",
		progdir
	};
#endif
	mAllowAbsolutePaths = true;
	FileReader *fr = new FileReader;
	if (fr->Open(filename))
	{
		mFullPathToConfig = filename;
	}
	else if (!IsAbsPath(filename))
	{
		for(auto c : paths)
		{
			FStringf fullname("%s/%s", c, filename);
			if (fr->Open(fullname))
			{
				mFullPathToConfig = fullname;
			}
		}
	}
	if (mFullPathToConfig.Len() > 0)
	{
		FixPathSeperator(mFullPathToConfig);
		mBasePath = ExtractFilePath(mFullPathToConfig);
		if (mBasePath.Len() > 0 && mBasePath.Back() != '/') mBasePath += '/';
	}
}

FileReader *FPatchSetReader::OpenMainConfigFile()
{
	auto fr = new FileReader;
	if (fr->Open(mBasePath)) return fr;
	delete fr;
	return nullptr;
}

FileReader *FPatchSetReader::OpenFile(const char *name)
{
	FString path;
	if (IsAbsPath(name)) path = name;
	else path = mBasePath + name;
	auto fr = new FileReader;
	if (fr->Open(path)) return fr;
	delete fr;
	return nullptr;
}

//==========================================================================
//
// collects everything out of the soundfonts directory.
// This may either be .sf2 files or zipped GUS patch sets with a
// 'timidity.cfg' in the root directory.
// Other compression types are not supported, in particular not 7z because
// due to the solid nature of its archives would be too slow.
//
//==========================================================================

void FSoundFontManager::ProcessOneFile(const FString &fn, TArray<FString> &sffiles)
{
	auto fb = ExtractFileBase(fn, false);
	for (auto &sfi : soundfonts)
	{
		// We already got a soundfont with this name. Do not add again.
		if (!sfi.mName.CompareNoCase(fb)) return;
	}
	
	FileReader fr;
	if (fr.Open(fn))
	{
		// Try to identify. We only accept .sf2 and .zip by content. All other archives are intentionally ignored.
		char head[16] = { 0};
		fr.Read(head, 16);
		if (!memcmp(head, "RIFF", 4) && !memcmp(head+8, "sfbkLIST", 8))
		{
			sffiles.Push(fn);
			FSoundFontInfo sft = { fb, fn, SF_SF2 };
			soundfonts.Push(sft);
		}
		else if (!memcmp(head, "PK", 2))
		{
			auto zip = FResourceFile::OpenResourceFile(fn, &fr);
			if (zip != nullptr && zip->LumpCount() > 1)	// Anything with just one lump cannot possibly be a packed GUS patch set so skip it right away and simplify the lookup code
			{
				auto zipl = zip->FindLump("timidity.cfg");
				if (zipl != nullptr)
				{
					// It seems like this is what we are looking for
					sffiles.Push(fn);
					FSoundFontInfo sft = { fb, fn, SF_GUS };
					soundfonts.Push(sft);
				}
				delete zip;
			}
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FSoundFontManager::CollectSoundfonts()
{
	findstate_t c_file;
	void *file;
	TArray<FString> sffiles;
	
	
	if (GameConfig != NULL && GameConfig->SetSection ("FileSearch.Directories"))
	{
		const char *key;
		const char *value;
		
		while (GameConfig->NextInSection (key, value))
		{
			if (stricmp (key, "Path") == 0)
			{
				FString dir;
				
				dir = NicePath(value);
				if (dir.IsNotEmpty())
				{
					if (dir.Back() != '/') dir += '/';
					FString path = dir + '*';
					if ((file = I_FindFirst(path, &c_file)) != ((void *)(-1)))
					{
						do
						{
							if (!(I_FindAttr(&c_file) & FA_DIREC))
							{
								FStringf name("%s%s", path.GetChars(), I_FindName(&c_file));
								ProcessOneFile(name, sffiles);
							}
						} while (I_FindNext(file, &c_file) == 0);
						I_FindClose(file);
					}
				}
			}
		}
	}
	soundfontcollection.InitMultipleFiles(sffiles);
}

//==========================================================================
//
//
//
//==========================================================================

const FSoundFontInfo *FSoundFontManager::FindSoundFont(const char *name, int allowed) const
{
	for(auto &sfi : soundfonts)
	{
		if (allowed & sfi.type && !sfi.mFilename.CompareNoCase(name))
		{
			return &sfi;
		}
	}
	return nullptr;
}

//==========================================================================
//
//
//
//==========================================================================

FSoundFontReader *FSoundFontManager::OpenSoundFont(const char *name, int allowed)
{
	auto sfi = FindSoundFont(name, allowed);
	if (sfi != nullptr)
	{
		return new FSoundFontReader(&soundfontcollection, sfi);
	}
	// The sound font collection did not yield any good results.
	// Next check if the file is a .sf file
	if (allowed & SF_SF2)
	{
		FileReader fr;
		if (fr.Open(name))
		{
			char head[16] = { 0};
			fr.Read(head, 16);
			if (!memcmp(head, "RIFF", 4) && !memcmp(head+8, "sfbkLIST", 8))
			{
				FSoundFontInfo sft = { name, name, SF_SF2 };
				soundfonts.Push(sft);
			}

		}
	}
	return nullptr;

}



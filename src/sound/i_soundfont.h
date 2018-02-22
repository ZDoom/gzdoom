#pragma once

#include "doomtype.h"
#include "w_wad.h"
#include "files.h"

enum
{
    SF_SF2 = 1,
    SF_GUS = 2
};

struct FSoundFontInfo
{
    FString mName;        // This is what the sounfont is identified with. It's the extension-less base file name
    FString mFilename;    // Full path to the backing file - this is needed by FluidSynth to load the sound font.
    int type;
};

//==========================================================================
//
//
//
//==========================================================================

class FSoundFontReader
{
protected:
    FWadCollection *collection;
    FString mMainConfigForSF2;
    int mFindInfile;
    // This is only doable for loose config files that get set as sound fonts. All other cases read from a contained environment where this does not apply.
    bool mAllowAbsolutePaths = false;
    // This has only meaning if being run on a platform with a case sensitive file system and loose files.
    // When reading from an archive it will always be case insensitive, just like the lump manager (since it repurposes the same implementation.)
    bool mCaseSensitivePaths = false;
    TArray<FString> mPaths;
    
    
    FSoundFontReader()
    {
        collection = nullptr;
        mFindInfile = -1;
    }
    
    int pathcmp(const char *p1, const char *p2);
    
    
public:
    
    FSoundFontReader(FWadCollection *coll, const FSoundFontInfo *sfi);
    virtual FileReader *OpenMainConfigFile();    // this is special because it needs to be synthesized for .sf files and set some restrictions for patch sets
    virtual FileReader *OpenFile(const char *name);
    std::pair<FileReader *, FString> LookupFile(const char *name);
    void AddPath(const char *str);
};

//==========================================================================
//
//
//
//==========================================================================

class FSF2Reader : FSoundFontReader
{
    FString mFilename;
public:
    FSF2Reader(const char *filename);
    virtual FileReader *OpenFile(const char *name) override;
};

//==========================================================================
//
//
//
//==========================================================================

class FZipPatReader : FSoundFontReader
{
    FResourceFile *resf;
public:
    FZipPatReader(const char *filename);
    ~FZipPatReader();
    virtual FileReader *OpenFile(const char *name) override;
};

//==========================================================================
//
// This one gets ugly...
//
//==========================================================================

class FPatchSetReader : FSoundFontReader
{
    FString mBasePath;
    FString mFullPathToConfig;
    
public:
    FPatchSetReader(const char *filename);
    ~FPatchSetReader();
    virtual FileReader *OpenMainConfigFile() override;
    virtual FileReader *OpenFile(const char *name) override;
};

//==========================================================================
//
//
//
//==========================================================================

class FSoundFontManager
{
    TArray<FSoundFontInfo> soundfonts;
    FWadCollection soundfontcollection;
    
    
    void ProcessOneFile(const FString & fn, TArray<FString> &sffiles);
    
public:
    void CollectSoundfonts();
    const FSoundFontInfo *FindSoundFont(const char *name, int allowedtypes) const;
    FSoundFontReader *OpenSoundFont(const char *name, int allowedtypes);
    const auto &GetList() const { return soundfonts; } // This is for the menu
    
};



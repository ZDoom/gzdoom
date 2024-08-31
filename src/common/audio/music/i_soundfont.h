#pragma once

#include "zstring.h"
#include "tarray.h"
#include "filesystem.h"
#include "files.h"
#include "filereadermusicinterface.h"

struct FSoundFontInfo
{
    FString mName;        // This is what the sounfont is identified with. It's the extension-less base file name
	FString mNameExt;     // Same with extension. Used for comparing with input names so they can be done with or without extension.
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
    // This is only doable for loose config files that get set as sound fonts. All other cases read from a contained environment where this does not apply.
    bool mAllowAbsolutePaths = false;
    // This has only meaning if being run on a platform with a case sensitive file system and loose files.
    // When reading from an archive it will always be case insensitive, just like the lump manager.
    bool mCaseSensitivePaths = false;
    TArray<FString> mPaths;


    int pathcmp(const char *p1, const char *p2);


public:

    virtual ~FSoundFontReader() {}
    virtual FileReader OpenMainConfigFile() = 0;    // this is special because it needs to be synthesized for .sf files and set some restrictions for patch sets
	virtual FString MainConfigFileName()
	{
		return basePath() + "timidity.cfg";
	}

    virtual FileReader OpenFile(const char *name) = 0;
    std::pair<FileReader , FString> LookupFile(const char *name);
    void AddPath(const char *str);
	virtual FString basePath() const
	{
		return "";	// archived patch sets do not use paths
	}

	virtual FileReader Open(const char* name, std::string &filename);
    virtual void close()
    {
        delete this;
    }

	ZMusicCustomReader* open_interface(const char* name);

};

//==========================================================================
//
//
//
//==========================================================================

class FSF2Reader : public FSoundFontReader
{
	FString mMainConfigForSF2;
	FString mFilename;
public:
    FSF2Reader(const char *filename);
	virtual FileReader OpenMainConfigFile() override;
    virtual FileReader OpenFile(const char *name) override;
};

//==========================================================================
//
//
//
//==========================================================================

class FZipPatReader : public FSoundFontReader
{
    FResourceFile *resf;
public:
    FZipPatReader(const char *filename);
    ~FZipPatReader();
	virtual FileReader OpenMainConfigFile() override;
	virtual FileReader OpenFile(const char *name) override;
	bool isOk() { return resf != nullptr; }
};

//==========================================================================
//
//
//
//==========================================================================

class FLumpPatchSetReader : public FSoundFontReader
{
	int mLumpIndex;;
	FString mBasePath;

public:
    FLumpPatchSetReader(const char *filename);
    virtual FileReader OpenMainConfigFile() override;
    virtual FileReader OpenFile(const char *name) override;
	virtual FString basePath() const override
	{
		return mBasePath;
	}

};

//==========================================================================
//
//
//
//==========================================================================

class FPatchSetReader : public FSoundFontReader
{
	FString mBasePath;
	FString mFullPathToConfig;

public:
	FPatchSetReader(FileReader &reader);
	FPatchSetReader(const char *filename);
	virtual FileReader OpenMainConfigFile() override;
	virtual FileReader OpenFile(const char *name) override;
	virtual FString basePath() const override
	{
		return mBasePath;
	}
};

//==========================================================================
//
//
//
//==========================================================================

class FSoundFontManager
{
    TArray<FSoundFontInfo> soundfonts;

    void ProcessOneFile(const char* fn);

public:
    void CollectSoundfonts();
    const FSoundFontInfo *FindSoundFont(const char *name, int allowedtypes) const;
    FSoundFontReader *OpenSoundFont(const char *name, int allowedtypes);
    const auto &GetList() const { return soundfonts; } // This is for the menu

};


extern FSoundFontManager sfmanager;

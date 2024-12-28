#pragma once

#include "zstring.h"
#include "tarray.h"

class FGameTexture;
class FSerializer;

// The savegame manager contains too much code that is game specific. Parts are shareable but need more work first.
struct FSaveGameNode
{
	FString SaveTitle;
	FString Filename;
	FString CreationTime;
	bool bOldVersion = false;
	bool bMissingWads = false;
	bool bNoDelete = false;
};

struct FSavegameManagerBase
{
protected:
	TArray<FSaveGameNode*> SaveGames;
	FSaveGameNode NewSaveNode;
	int LastSaved = -1;
	int LastAccessed = -1;
	FGameTexture *SavePic = nullptr;

public:
	int WindowSize = 0;
	FString SaveCommentString;
	FSaveGameNode *quickSaveSlot = nullptr;
	virtual ~FSavegameManagerBase();

protected:
	int InsertSaveNode(FSaveGameNode *node);
	virtual void PerformSaveGame(const char *fn, const char *sgdesc) = 0;
	virtual void PerformLoadGame(const char *fn, bool) = 0;
	virtual FString ExtractSaveComment(FSerializer &arc) = 0;
	virtual FString BuildSaveName(const char* prefix, int slot) = 0;
public:
	void NotifyNewSave(const FString &file, const FString &title, bool okForQuicksave, bool forceQuicksave);
	void ClearSaveGames();

	virtual void ReadSaveStrings() = 0;
	void UnloadSaveData();

	int RemoveSaveSlot(int index);
	void LoadSavegame(int Selected);
	void DoSave(int Selected, const char *savegamestring);
	unsigned ExtractSaveData(int index);
	void ClearSaveStuff();
	bool DrawSavePic(int x, int y, int w, int h);
	void DrawSaveComment(FFont *font, int cr, int x, int y, int scalefactor);
	void SetFileInfo(int Selected);
	unsigned SavegameCount();
	FSaveGameNode *GetSavegame(int i);
	void InsertNewSaveNode();
	bool RemoveNewSaveNode();

};

extern FString SavegameFolder;	// specifies a subdirectory for the current IWAD.
FString G_GetSavegamesFolder();
FString G_BuildSaveName(const char* prefix);

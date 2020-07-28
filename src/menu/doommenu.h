#pragma once
#include "menu.h"

void M_StartControlPanel (bool makeSound, bool scaleoverride = false);


struct FNewGameStartup
{
	const char *PlayerClass;
	int Episode;
	int Skill;
};

extern FNewGameStartup NewGameStartupInfo;
void M_StartupEpisodeMenu(FNewGameStartup *gs);
void M_StartupSkillMenu(FNewGameStartup *gs);
void M_CreateGameMenus();

// The savegame manager contains too much code that is game specific. Parts are shareable but need more work first.
struct FSaveGameNode
{
	FString SaveTitle;
	FString Filename;
	bool bOldVersion = false;
	bool bMissingWads = false;
	bool bNoDelete = false;
};

struct FSavegameManager
{
private:
	TArray<FSaveGameNode*> SaveGames;
	FSaveGameNode NewSaveNode;
	int LastSaved = -1;
	int LastAccessed = -1;
	FGameTexture *SavePic = nullptr;

public:
	int WindowSize = 0;
	FString SaveCommentString;
	FSaveGameNode *quickSaveSlot = nullptr;
	~FSavegameManager();

private:
	int InsertSaveNode(FSaveGameNode *node);
public:
	void NotifyNewSave(const FString &file, const FString &title, bool okForQuicksave, bool forceQuicksave);
	void ClearSaveGames();

	void ReadSaveStrings();
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

extern FSavegameManager savegameManager;


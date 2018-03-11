#ifndef P_CONVERSATION_H
#define P_CONVERSATION_H 1

#include <tarray.h>

#include "s_sound.h"
#include "textures/textures.h"

struct FStrifeDialogueReply;
class FTexture;
struct FBrokenLines;

struct FStrifeDialogueItemCheck
{
	PClassActor *Item;
	int Amount;
};

// FStrifeDialogueNode holds text an NPC says to the player
struct FStrifeDialogueNode
{
	~FStrifeDialogueNode ();
	PClassActor *DropType = nullptr;
	TArray<FStrifeDialogueItemCheck> ItemCheck;
	int ThisNodeNum = 0;	// location of this node in StrifeDialogues
	int ItemCheckNode = 0;	// index into StrifeDialogues

	PClassActor *SpeakerType = nullptr;
	FString SpeakerName;
	FSoundID SpeakerVoice;
	FString Backdrop;
	FString Dialogue;
	FString Goodbye; // must init to null for binary scripts to work as intended

	FStrifeDialogueReply *Children = nullptr;
	FName MenuClassName;
	FString UserData;
};

// FStrifeDialogueReply holds responses the player can give to the NPC
struct FStrifeDialogueReply
{
	FStrifeDialogueReply *Next = nullptr;
	PClassActor *GiveType = nullptr;
	int ActionSpecial = 0;
	int Args[5] = {};
	int PrintAmount = 0;
	TArray<FStrifeDialogueItemCheck> ItemCheck;
	TArray<FStrifeDialogueItemCheck> ItemCheckRequire;
	TArray<FStrifeDialogueItemCheck> ItemCheckExclude;
	FString Reply;
	FString QuickYes;
	FString QuickNo;
	FString LogString;
	int NextNode = 0;	// index into StrifeDialogues
	int LogNumber = 0;
	bool NeedsGold = false;
};

extern TArray<FStrifeDialogueNode *> StrifeDialogues;

struct MapData;

void SetStrifeType(int convid, PClassActor *Class);
void SetConversation(int convid, PClassActor *Class, int dlgindex);
PClassActor *GetStrifeType (int typenum);
int GetConversation(int conv_id);
int GetConversation(FName classname);

bool LoadScriptFile (const char *name, bool include, int type = 0);

void P_LoadStrifeConversations (MapData *map, const char *mapname);
void P_FreeStrifeConversations ();

void P_StartConversation (AActor *npc, AActor *pc, bool facetalker, bool saveangle);
void P_ResumeConversation ();

void P_ConversationCommand (int netcode, int player, uint8_t **stream);

class FileReader;
bool P_ParseUSDF(int lumpnum, FileReader &lump, int lumplen);


#endif

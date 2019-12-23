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
	FString ThisNodeName = nullptr;
	FString ItemCheckNodeName = nullptr;

	PClassActor *SpeakerType = nullptr;
	FString SpeakerName;
	FSoundID SpeakerVoice = 0;
	FString Backdrop;
	FString Dialogue;
	FString Goodbye; // must init to null for binary scripts to work as intended

	FStrifeDialogueReply *Children = nullptr;
	FName MenuClassName = NAME_None;
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
	FString NextNodeName = nullptr;
	bool NeedsGold = false;
	bool CloseDialog = true;
};

struct MapData;

PClassActor *GetStrifeType (int typenum);

void P_FreeStrifeConversations ();

void P_StartConversation (AActor *npc, AActor *pc, bool facetalker, bool saveangle);
void P_ResumeConversation ();

void P_ConversationCommand (int netcode, int player, uint8_t **stream);

class FileReader;


#endif

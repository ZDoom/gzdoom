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
	PClassInventory *Item;
	int Amount;
};

// FStrifeDialogueNode holds text an NPC says to the player
struct FStrifeDialogueNode
{
	~FStrifeDialogueNode ();
	PClassActor *DropType;
	TArray<FStrifeDialogueItemCheck> ItemCheck;
	int ThisNodeNum;	// location of this node in StrifeDialogues
	int ItemCheckNode;	// index into StrifeDialogues

	PClassActor *SpeakerType;
	char *SpeakerName;
	FSoundID SpeakerVoice;
	FTextureID Backdrop;
	char *Dialogue;

	FStrifeDialogueReply *Children;
};

// FStrifeDialogueReply holds responses the player can give to the NPC
struct FStrifeDialogueReply
{
	~FStrifeDialogueReply ();

	FStrifeDialogueReply *Next;
	PClassActor *GiveType;
	int ActionSpecial;
	int Args[5];
	TArray<FStrifeDialogueItemCheck> ItemCheck;
	char *Reply;
	char *QuickYes;
	int NextNode;	// index into StrifeDialogues
	int LogNumber;
	char *LogString;
	char *QuickNo;
	bool NeedsGold;
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

void P_ConversationCommand (int netcode, int player, BYTE **stream);

class FileReader;
bool P_ParseUSDF(int lumpnum, FileReader *lump, int lumplen);


#endif

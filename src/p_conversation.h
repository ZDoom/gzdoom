#ifndef P_CONVERSATION_H
#define P_CONVERSATION_H 1

// TODO: Generalize the conversation system to something NWN-like that
// users can edit as simple text files. Particularly useful would be
// the ability to call ACS functions to implement AppearsWhen properties
// and ACS scripts to implement ActionTaken properties.
// TODO: Make this work in multiplayer and in demos. Multiplayer probably
// isn't possible for Strife conversations, but demo playback should be.

struct FStrifeDialogueReply;
class FTexture;
struct brokenlines_t;

// FStrifeDialogueNode holds text an NPC says to the player
struct FStrifeDialogueNode
{
	~FStrifeDialogueNode ();
	const TypeInfo *DropType;
	const TypeInfo *ItemCheck[3];
	int ItemCheckNode;	// index into StrifeDialogues

	const TypeInfo *SpeakerType;
	char *SpeakerName;
	int SpeakerVoice;
	int Backdrop;
	char *Dialogue;

	FStrifeDialogueReply *Children;
};

// FStrifeDialogueReply holds responses the player can give to the NPC
struct FStrifeDialogueReply
{
	~FStrifeDialogueReply ();

	FStrifeDialogueReply *Next;
	const TypeInfo *GiveType;
	const TypeInfo *ItemCheck[3];
	int ItemCheckAmount[3];
	char *Reply;
	char *QuickYes;
	int NextNode;	// index into StrifeDialogues
	int LogNumber;
	char *QuickNo;

	brokenlines_t *ReplyLines;
};

extern TArray<FStrifeDialogueNode *> StrifeDialogues;

// There were 344 types in Strife, and Strife conversations refer
// to their index in the mobjinfo table. This table indexes all
// the Strife actor types in the order Strife had them and is
// initialized as part of the actor's setup in infodefaults.cpp.
extern const TypeInfo *StrifeTypes[344];

void P_LoadStrifeConversations (const char *mapname);
void P_FreeStrifeConversations ();

void P_StartConversation (AActor *npc, AActor *pc);
void P_ResumeConversation ();

#endif

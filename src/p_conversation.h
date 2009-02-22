#ifndef P_CONVERSATION_H
#define P_CONVERSATION_H 1

// TODO: Generalize the conversation system to something NWN-like that
// users can edit as simple text files. Particularly useful would be
// the ability to call ACS functions to implement AppearsWhen properties
// and ACS scripts to implement ActionTaken properties.
// TODO: Make this work in demos.

struct FStrifeDialogueReply;
class FTexture;
struct FBrokenLines;

// FStrifeDialogueNode holds text an NPC says to the player
struct FStrifeDialogueNode
{
	~FStrifeDialogueNode ();
	const PClass *DropType;
	const PClass *ItemCheck[3];
	int ItemCheckNode;	// index into StrifeDialogues

	const PClass *SpeakerType;
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
	const PClass *GiveType;
	const PClass *ItemCheck[3];
	int ItemCheckAmount[3];
	char *Reply;
	char *QuickYes;
	int NextNode;	// index into StrifeDialogues
	int LogNumber;
	char *QuickNo;
	bool NeedsGold;

	FBrokenLines *ReplyLines;
};

// [CW] These are used to make conversations work.
enum
{
	CONV_NPCANGLE,
	CONV_ANIMATE,
	CONV_GIVEINVENTORY,
	CONV_TAKEINVENTORY,
	CONV_SETNULL,
	CONV_CLOSE,
};

extern TArray<FStrifeDialogueNode *> StrifeDialogues;

// There were 344 types in Strife, and Strife conversations refer
// to their index in the mobjinfo table. This table indexes all
// the Strife actor types in the order Strife had them and is
// initialized as part of the actor's setup in infodefaults.cpp.
extern const PClass *StrifeTypes[1001];

struct MapData;

void P_LoadStrifeConversations (MapData *map, const char *mapname);
void P_FreeStrifeConversations ();

void P_StartConversation (AActor *npc, AActor *pc, bool facetalker, bool saveangle);
void P_ResumeConversation ();

void P_ConversationCommand (int player, BYTE **stream);

#endif

/*
** p_converstation.cpp
** Implements Strife style conversation dialogs
**
**---------------------------------------------------------------------------
** Copyright 2004-2008 Randy Heit
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

#include <assert.h>

#include "actor.h"
#include "p_conversation.h"
#include "w_wad.h"
#include "cmdlib.h"
#include "v_text.h"
#include "gi.h"
#include "a_keys.h"
#include "p_enemy.h"
#include "gstrings.h"
#include "sound/i_music.h"
#include "p_setup.h"
#include "d_net.h"
#include "d_event.h"
#include "doomstat.h"
#include "c_console.h"
#include "sbar.h"
#include "p_lnspec.h"
#include "p_local.h"
#include "menu/menu.h"
#include "g_levellocals.h"
#include "vm.h"
#include "actorinlines.h"

// The conversations as they exist inside a SCRIPTxx lump.
struct Response
{
	int32_t GiveType;
	int32_t Item[3];
	int32_t Count[3];
	char Reply[32];
	char Yes[80];
	int32_t Link;
	uint32_t Log;
	char No[80];
};

struct Speech
{
	uint32_t SpeakerType;
	int32_t DropType;
	int32_t ItemCheck[3];
	int32_t Link;
	char Name[16];
	char Sound[8];
	char Backdrop[8];
	char Dialogue[320];
	Response Responses[5];
};

// The Teaser version of the game uses an older version of the structure
struct TeaserSpeech
{
	uint32_t SpeakerType;
	int32_t DropType;
	uint32_t VoiceNumber;
	char Name[16];
	char Dialogue[320];
	Response Responses[5];
};

static FRandom pr_randomspeech("RandomSpeech");

TArray<FStrifeDialogueNode *> StrifeDialogues;

typedef TMap<int, int> FDialogueIDMap;				// maps dialogue IDs to dialogue array index (for ACS)
typedef TMap<FName, int> FDialogueMap;				// maps actor class names to dialogue array index

FClassMap StrifeTypes;
static FDialogueIDMap DialogueRoots;
static FDialogueMap ClassRoots;
static int ConversationMenuY;

static int ConversationPauseTic;
static int StaticLastReply;

static bool LoadScriptFile(int lumpnum, FileReader &lump, int numnodes, bool include, int type);
static FStrifeDialogueNode *ReadRetailNode (FileReader &lump, uint32_t &prevSpeakerType);
static FStrifeDialogueNode *ReadTeaserNode (FileReader &lump, uint32_t &prevSpeakerType);
static void ParseReplies (FStrifeDialogueReply **replyptr, Response *responses);
static bool DrawConversationMenu ();
static void PickConversationReply (int replyindex);
static void TerminalResponse (const char *str);

static FStrifeDialogueNode *PrevNode;

//============================================================================
//
// GetStrifeType
//
// Given an item type number, returns the corresponding PClass.
//
//============================================================================

void SetStrifeType(int convid, PClassActor *Class)
{
	StrifeTypes[convid] = Class;
}

void ClearStrifeTypes()
{
	StrifeTypes.Clear();
}

void SetConversation(int convid, PClassActor *Class, int dlgindex)
{
	if (convid != -1)
	{
		DialogueRoots[convid] = dlgindex;
	}
	if (Class != NULL)
	{
		ClassRoots[Class->TypeName] = dlgindex;
	}
}

PClassActor *GetStrifeType (int typenum)
{
	PClassActor **ptype = StrifeTypes.CheckKey(typenum);
	if (ptype == NULL) return NULL;
	else return *ptype;
}

int GetConversation(int conv_id)
{
	int *pindex = DialogueRoots.CheckKey(conv_id);
	if (pindex == NULL) return -1;
	else return *pindex;
}

int GetConversation(FName classname)
{
	int *pindex = ClassRoots.CheckKey(classname);
	if (pindex == NULL) return -1;
	else return *pindex;
}

//============================================================================
//
// P_LoadStrifeConversations
//
// Loads the SCRIPT00 and SCRIPTxx files for a corresponding map.
//
//============================================================================

void P_LoadStrifeConversations (MapData *map, const char *mapname)
{
	P_FreeStrifeConversations ();
	if (map->Size(ML_CONVERSATION) > 0)
	{
		LoadScriptFile (map->lumpnum, map->Reader(ML_CONVERSATION), map->Size(ML_CONVERSATION), false, 0);
	}
	else
	{
		if (strnicmp (mapname, "MAP", 3) == 0)
		{
			char scriptname_b[9] = { 'S','C','R','I','P','T',mapname[3],mapname[4],0 };
			char scriptname_t[9] = { 'D','I','A','L','O','G',mapname[3],mapname[4],0 };

			if (   LoadScriptFile(scriptname_t, false, 2)
				|| LoadScriptFile(scriptname_b, false, 1))
			{
				return;
			}
		}

		if (gameinfo.Dialogue.IsNotEmpty())
		{
			if (LoadScriptFile(gameinfo.Dialogue, false, 0))
			{
				return;
			}
		}

		LoadScriptFile("SCRIPT00", false, 1);
	}
}

//============================================================================
//
// LoadScriptFile
//
// Loads a SCRIPTxx file and converts it into a more useful internal format.
//
//============================================================================

bool LoadScriptFile (const char *name, bool include, int type)
{
	int lumpnum = Wads.CheckNumForName (name);
	const bool found = lumpnum >= 0
		|| (lumpnum = Wads.CheckNumForFullName (name)) >= 0;

	if (!found)
	{
		if (type == 0)
		{
			Printf(TEXTCOLOR_RED "Could not find dialog file %s\n", name);
		}

		return false;
	}
	FileReader lump = Wads.ReopenLumpReader (lumpnum);

	bool res = LoadScriptFile(lumpnum, lump, Wads.LumpLength(lumpnum), include, type);
	return res;
}

static bool LoadScriptFile(int lumpnum, FileReader &lump, int numnodes, bool include, int type)
{
	int i;
	uint32_t prevSpeakerType;
	FStrifeDialogueNode *node;
	char buffer[4];

	lump.Read(buffer, 4);
	lump.Seek(-4, FileReader::SeekCur);

	// The binary format is so primitive that this check is enough to detect it.
	bool isbinary = (buffer[0] == 0 || buffer[1] == 0 || buffer[2] == 0 || buffer[3] == 0);

	if ((type == 1 && !isbinary) || (type == 2 && isbinary))
	{
		DPrintf(DMSG_ERROR, "Incorrect data format for conversation script in %s.\n", Wads.GetLumpFullName(lumpnum));
		return false;
	}

	if (!isbinary)
	{
		P_ParseUSDF(lumpnum, lump, numnodes);
	}
	else 
	{
		if (!include)
		{
			LoadScriptFile("SCRIPT00", true, 1);
		}
		if (!(gameinfo.flags & GI_SHAREWARE))
		{
			// Strife scripts are always a multiple of 1516 bytes because each entry
			// is exactly 1516 bytes long.
			if (numnodes % 1516 != 0)
			{
				DPrintf(DMSG_ERROR, "Incorrect data format for conversation script in %s.\n", Wads.GetLumpFullName(lumpnum));
				return false;
			}
			numnodes /= 1516;
		}
		else
		{
			// And the teaser version has 1488-byte entries.
			if (numnodes % 1488 != 0)
			{
				DPrintf(DMSG_ERROR, "Incorrect data format for conversation script in %s.\n", Wads.GetLumpFullName(lumpnum));
				return false;
			}
			numnodes /= 1488;
		}

		prevSpeakerType = 0;

		for (i = 0; i < numnodes; ++i)
		{
			if (!(gameinfo.flags & GI_SHAREWARE))
			{
				node = ReadRetailNode (lump, prevSpeakerType);
			}
			else
			{
				node = ReadTeaserNode (lump, prevSpeakerType);
			}
			node->ThisNodeNum = StrifeDialogues.Push(node);
		}
	}
	return true;
}

//============================================================================
//
// ReadRetailNode
//
// Converts a single dialogue node from the Retail version of Strife.
//
//============================================================================

static FStrifeDialogueNode *ReadRetailNode (FileReader &lump, uint32_t &prevSpeakerType)
{
	FStrifeDialogueNode *node;
	Speech speech;
	char fullsound[16];
	PClassActor *type;
	int j;

	node = new FStrifeDialogueNode;

	lump.Read (&speech, sizeof(speech));

	// Byte swap all the ints in the original data
	speech.SpeakerType = LittleLong(speech.SpeakerType);
	speech.DropType = LittleLong(speech.DropType);
	speech.Link = LittleLong(speech.Link);

	// Assign the first instance of a conversation as the default for its
	// actor, so newly spawned actors will use this conversation by default.
	type = GetStrifeType (speech.SpeakerType);
	node->SpeakerType = type;

	if ((signed)(speech.SpeakerType) >= 0 && prevSpeakerType != speech.SpeakerType)
	{
		if (type != NULL)
		{
			ClassRoots[type->TypeName] = StrifeDialogues.Size();
		}
		DialogueRoots[speech.SpeakerType] = StrifeDialogues.Size();
		prevSpeakerType = speech.SpeakerType;
	}

	// Convert the rest of the data to our own internal format.
	node->Dialogue = speech.Dialogue;

	// The speaker's portrait, if any.
	speech.Dialogue[0] = 0; 	//speech.Backdrop[8] = 0;
	node->Backdrop = speech.Backdrop;

	// The speaker's voice for this node, if any.
	speech.Backdrop[0] = 0; 	//speech.Sound[8] = 0;
	mysnprintf (fullsound, countof(fullsound), "svox/%s", speech.Sound);
	node->SpeakerVoice = fullsound;

	// The speaker's name, if any.
	speech.Sound[0] = 0; 		//speech.Name[16] = 0;
	node->SpeakerName = speech.Name;

	// The item the speaker should drop when killed.
	node->DropType = GetStrifeType(speech.DropType);

	// Items you need to have to make the speaker use a different node.
	node->ItemCheck.Resize(3);
	for (j = 0; j < 3; ++j)
	{
		auto inv = GetStrifeType(speech.ItemCheck[j]);
		if (!inv->IsDescendantOf(NAME_Inventory)) inv = nullptr;
		node->ItemCheck[j].Item = inv;
		node->ItemCheck[j].Amount = -1;
	}
	node->ItemCheckNode = speech.Link;
	node->Children = NULL;

	ParseReplies (&node->Children, &speech.Responses[0]);

	return node;
}

//============================================================================
//
// ReadTeaserNode
//
// Converts a single dialogue node from the Teaser version of Strife.
//
//============================================================================

static FStrifeDialogueNode *ReadTeaserNode (FileReader &lump, uint32_t &prevSpeakerType)
{
	FStrifeDialogueNode *node;
	TeaserSpeech speech;
	char fullsound[16];
	PClassActor *type;
	int j;

	node = new FStrifeDialogueNode;

	lump.Read (&speech, sizeof(speech));

	// Byte swap all the ints in the original data
	speech.SpeakerType = LittleLong(speech.SpeakerType);
	speech.DropType = LittleLong(speech.DropType);

	// Assign the first instance of a conversation as the default for its
	// actor, so newly spawned actors will use this conversation by default.
	type = GetStrifeType(speech.SpeakerType);
	node->SpeakerType = type;

	if ((signed)speech.SpeakerType >= 0 && prevSpeakerType != speech.SpeakerType)
	{
		if (type != NULL)
		{
			ClassRoots[type->TypeName] = StrifeDialogues.Size();
		}
		DialogueRoots[speech.SpeakerType] = StrifeDialogues.Size();
		prevSpeakerType = speech.SpeakerType;
	}

	// Convert the rest of the data to our own internal format.
	node->Dialogue = speech.Dialogue;

	// The Teaser version doesn't have portraits.
	node->Backdrop = "";

	// The speaker's voice for this node, if any.
	if (speech.VoiceNumber != 0)
	{
		mysnprintf (fullsound, countof(fullsound), "svox/voc%u", speech.VoiceNumber);
		node->SpeakerVoice = fullsound;
	}
	else
	{
		node->SpeakerVoice = 0;
	}

	// The speaker's name, if any.
	speech.Dialogue[0] = 0; 	//speech.Name[16] = 0;
	node->SpeakerName = speech.Name;

	// The item the speaker should drop when killed.
	node->DropType = GetStrifeType (speech.DropType);

	// Items you need to have to make the speaker use a different node.
	node->ItemCheck.Resize(3);
	for (j = 0; j < 3; ++j)
	{
		node->ItemCheck[j].Item = NULL;
		node->ItemCheck[j].Amount = -1;
	}
	node->ItemCheckNode = 0;
	node->Children = NULL;

	ParseReplies (&node->Children, &speech.Responses[0]);

	return node;
}

//============================================================================
//
// ParseReplies
//
// Convert PC responses. Rather than being stored inside the main node, they
// hang off it as a singly-linked list, so no space is wasted on replies that
// don't even matter.
//
//============================================================================

static void ParseReplies (FStrifeDialogueReply **replyptr, Response *responses)
{
	FStrifeDialogueReply *reply;
	int j, k;

	// Byte swap first.
	for (j = 0; j < 5; ++j)
	{
		responses[j].GiveType = LittleLong(responses[j].GiveType);
		responses[j].Link = LittleLong(responses[j].Link);
		responses[j].Log = LittleLong(responses[j].Log);
		for (k = 0; k < 3; ++k)
		{
			responses[j].Item[k] = LittleLong(responses[j].Item[k]);
			responses[j].Count[k] = LittleLong(responses[j].Count[k]);
		}
	}

	for (j = 0; j < 5; ++j)
	{
		Response *rsp = &responses[j];

		// If the reply has no text and goes nowhere, then it doesn't
		// need to be remembered.
		if (rsp->Reply[0] == 0 && rsp->Link == 0)
		{
			continue;
		}
		reply = new FStrifeDialogueReply;

		// The next node to use when this reply is chosen.
		reply->NextNode = rsp->Link;

		// The message to record in the log for this reply.
		reply->LogNumber = rsp->Log;
		reply->LogString = "";

		// The item to receive when this reply is used.
		reply->GiveType = GetStrifeType (rsp->GiveType);
		reply->ActionSpecial = 0;

		// Do you need anything special for this reply to succeed?
		reply->ItemCheck.Resize(3);
		for (k = 0; k < 3; ++k)
		{
			auto inv = GetStrifeType(rsp->Item[k]);
			if (!inv->IsDescendantOf(NAME_Inventory)) inv = nullptr;
			reply->ItemCheck[k].Item = inv;
			reply->ItemCheck[k].Amount = rsp->Count[k];
		}
		reply->PrintAmount = reply->ItemCheck[0].Amount;
		reply->ItemCheckRequire.Clear();
		reply->ItemCheckExclude.Clear();

		// If the first item check has a positive amount required, then
		// add that to the reply string. Otherwise, use the reply as-is.
		reply->Reply = rsp->Reply;
		reply->NeedsGold = (rsp->Count[0] > 0);

		// QuickYes messages are shown when you meet the item checks.
		// QuickNo messages are shown when you don't.
		if (rsp->Yes[0] == '_' && rsp->Yes[1] == 0)
		{
			reply->QuickYes = "";
		}
		else
		{
			reply->QuickYes = rsp->Yes;
		}
		if (reply->ItemCheck[0].Item != 0)
		{
			reply->QuickNo = rsp->No;
		}
		else
		{
			reply->QuickNo = "";
		}
		reply->Next = *replyptr;
		*replyptr = reply;
		replyptr = &reply->Next;
	}
}

//============================================================================
//
// FStrifeDialogueNode :: ~FStrifeDialogueNode
//
//============================================================================

FStrifeDialogueNode::~FStrifeDialogueNode ()
{
	FStrifeDialogueReply *tokill = Children;
	while (tokill != NULL)
	{
		FStrifeDialogueReply *next = tokill->Next;
		delete tokill;
		tokill = next;
	}
}

//============================================================================
//
// FindNode
//
// Returns the index that matches the given conversation node.
//
//============================================================================

static int FindNode (const FStrifeDialogueNode *node)
{
	int rootnode = 0;

	while (StrifeDialogues[rootnode] != node)
	{
		rootnode++;
	}
	return rootnode;
}

//============================================================================
//
// CheckStrifeItem
//
// Checks if you have an item. A NULL itemtype is always considered to be
// present.
//
//============================================================================

static bool CheckStrifeItem (player_t *player, PClassActor *itemtype, int amount=-1)
{
	if (itemtype == NULL || amount == 0)
		return true;

	auto item = player->ConversationPC->FindInventory (itemtype);
	if (item == NULL)
		return false;

	return amount < 0 || item->IntVar(NAME_Amount) >= amount;
}

//============================================================================
//
// TakeStrifeItem
//
// Takes away some of an item, unless that item is special and should not
// be removed.
//
//============================================================================

static void TakeStrifeItem (player_t *player, PClassActor *itemtype, int amount)
{
	if (itemtype == NULL || amount == 0)
		return;

	// Don't take quest items.
	if (itemtype->IsDescendantOf (PClass::FindClass(NAME_QuestItem)))
		return;

	// Don't take keys.
	if (itemtype->IsDescendantOf (PClass::FindActor(NAME_Key)))
		return;

	// Don't take the sigil.
	if (itemtype->TypeName == NAME_Sigil)
		return;

	IFVM(Actor, TakeInventory)
	{
		VMValue params[] = { player->mo, itemtype, amount, false, false };
		VMCall(func, params, 5, nullptr, 0);
	}
}

CUSTOM_CVAR(Float, dlg_musicvolume, 1.0f, CVAR_ARCHIVE)
{
	if (self < 0.f) self = 0.f;
	else if (self > 1.f) self = 1.f;
}

//============================================================================
//
// ShouldSkipReply
//
// Determines whether this reply should be skipped or not.
//
//============================================================================

static bool ShouldSkipReply(FStrifeDialogueReply *reply, player_t *player)
{
	if (reply->Reply.IsEmpty())
		return true;

	int i;
	for (i = 0; i < (int)reply->ItemCheckRequire.Size(); ++i)
	{
		if (!CheckStrifeItem(player, reply->ItemCheckRequire[i].Item, reply->ItemCheckRequire[i].Amount))
		{
			return true;
		}
	}

	for (i = 0; i < (int)reply->ItemCheckExclude.Size(); ++i)
	{
		if (CheckStrifeItem(player, reply->ItemCheckExclude[i].Item, reply->ItemCheckExclude[i].Amount))
		{
			return true;
		}
	}
	return false;
}

DEFINE_ACTION_FUNCTION(FStrifeDialogueReply, ShouldSkipReply)
{
	PARAM_SELF_STRUCT_PROLOGUE(FStrifeDialogueReply);
	PARAM_POINTER(player, player_t);
	ACTION_RETURN_BOOL(ShouldSkipReply(self, player));
}

DEFINE_ACTION_FUNCTION(DConversationMenu, SendConversationReply)
{
	PARAM_PROLOGUE;
	PARAM_INT(node);
	PARAM_INT(reply);
	switch (node)
	{
	case -1:
		Net_WriteByte(DEM_CONVNULL);
		break;

	case -2:
		Net_WriteByte(DEM_CONVCLOSE);
		break;

	default:
		Net_WriteByte(DEM_CONVREPLY);
		Net_WriteWord(node);
		Net_WriteByte(reply);
		break;
	}
	StaticLastReply = reply;
	return 0;
}


//============================================================================
//
// P_FreeStrifeConversations
//
//============================================================================

void P_FreeStrifeConversations ()
{
	FStrifeDialogueNode *node;

	while (StrifeDialogues.Pop (node))
	{
		delete node;
	}

	DialogueRoots.Clear();
	ClassRoots.Clear();

	PrevNode = NULL;
	if (CurrentMenu != NULL && CurrentMenu->IsKindOf("ConversationMenu"))
	{
		CurrentMenu->Close();
	}
}

//============================================================================
//
// P_StartConversation
//
// Begins a conversation between a PC and NPC.
//
//============================================================================

void P_StartConversation (AActor *npc, AActor *pc, bool facetalker, bool saveangle)
{
	AActor *oldtarget;
	int i;

	// Make sure this is actually a player.
	if (pc->player == NULL) return;

	// [CW] If an NPC is talking to a PC already, then don't let
	// anyone else talk to the NPC.
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || pc->player == &players[i])
			continue;

		if (npc == players[i].ConversationNPC)
			return;
	}

	pc->Vel.Zero();
	pc->player->Vel.Zero();
	static_cast<APlayerPawn*>(pc)->PlayIdle ();

	pc->player->ConversationPC = pc;
	pc->player->ConversationNPC = npc;
	npc->flags5 |= MF5_INCONVERSATION;

	FStrifeDialogueNode *CurNode = npc->Conversation;

	if (pc->player == &players[consoleplayer])
	{
		S_Sound (CHAN_VOICE | CHAN_UI, gameinfo.chatSound, 1, ATTN_NONE);
	}

	npc->reactiontime = 2;
	pc->player->ConversationFaceTalker = facetalker;
	if (saveangle)
	{
		pc->player->ConversationNPCAngle = npc->Angles.Yaw;
	}
	oldtarget = npc->target;
	npc->target = pc;
	if (facetalker)
	{
		if (!(npc->flags8 & MF8_DONTFACETALKER))
			A_FaceTarget (npc);
		pc->Angles.Yaw = pc->AngleTo(npc);
	}
	if ((npc->flags & MF_FRIENDLY) || (npc->flags4 & MF4_NOHATEPLAYERS))
	{
		npc->target = oldtarget;
	}

	// Check if we should jump to another node
	while (CurNode->ItemCheck.Size() > 0 && CurNode->ItemCheck[0].Item != NULL)
	{
		bool jump = true;
		for (i = 0; i < (int)CurNode->ItemCheck.Size(); ++i)
		{
			if(!CheckStrifeItem (pc->player, CurNode->ItemCheck[i].Item, CurNode->ItemCheck[i].Amount))
			{
				jump = false;
				break;
			}
		}
		if (jump && CurNode->ItemCheckNode > 0)
		{
			int root = pc->player->ConversationNPC->ConversationRoot;
			CurNode = StrifeDialogues[root + CurNode->ItemCheckNode - 1];
		}
		else
		{
			break;
		}
	}

	// The rest is only done when the conversation is actually displayed.
	if (pc->player == &players[consoleplayer])
	{
		if (CurNode->SpeakerVoice != 0)
		{
			I_SetMusicVolume (dlg_musicvolume);
			S_Sound (npc, CHAN_VOICE|CHAN_NOPAUSE, CurNode->SpeakerVoice, 1, ATTN_NORM);
		}

		// Create the menu. This may be a user-defined class so check if it is good to use.
		FName cls = CurNode->MenuClassName;
		if (cls == NAME_None) cls = gameinfo.DefaultConversationMenuClass;
		if (cls == NAME_None) cls = "ConversationMenu";
		auto mcls = PClass::FindClass(cls);
		if (mcls == nullptr || !mcls->IsDescendantOf("ConversationMenu")) mcls = PClass::FindClass("ConversationMenu");
		assert(mcls);

		auto cmenu = mcls->CreateNew();
		IFVIRTUALPTRNAME(cmenu, "ConversationMenu", Init)
		{
			VMValue params[] = { cmenu, CurNode, pc->player, StaticLastReply };
			VMReturn ret(&ConversationMenuY);
			VMCall(func, params, countof(params), &ret, 1);
		}

		if (CurNode != PrevNode)
		{ // Only reset the selection if showing a different menu.
			StaticLastReply = 0;
			PrevNode = CurNode;
		}

		// And open the menu
		M_StartControlPanel (false);
		M_ActivateMenu((DMenu*)cmenu);
		menuactive = MENU_OnNoPause;
	}
}

//============================================================================
//
// P_ResumeConversation
//
// Resumes a conversation that was interrupted by a slideshow.
//
//============================================================================

void P_ResumeConversation ()
{
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;

		player_t *p = &players[i];

		if (p->ConversationPC != NULL && p->ConversationNPC != NULL)
		{
			P_StartConversation (p->ConversationNPC, p->ConversationPC, p->ConversationFaceTalker, false);
		}
	}
}

//============================================================================
//
// HandleReply
//
// Run by the netcode on all machines.
//
//============================================================================

static void HandleReply(player_t *player, bool isconsole, int nodenum, int replynum)
{
	const char *replyText = NULL;
	FStrifeDialogueReply *reply;
	FStrifeDialogueNode *node;
	AActor *npc;
	bool takestuff;
	int i;

	if (player->ConversationNPC == NULL || (unsigned)nodenum >= StrifeDialogues.Size())
	{
		return;
	}

	// Find the reply.
	node = StrifeDialogues[nodenum];
	for (i = 0, reply = node->Children; reply != NULL && i != replynum; ++i, reply = reply->Next)
	{ }
	npc = player->ConversationNPC;
	if (reply == NULL)
	{
		// The default reply was selected
		if (!(npc->flags8 & MF8_DONTFACETALKER))
			npc->Angles.Yaw = player->ConversationNPCAngle;
		npc->flags5 &= ~MF5_INCONVERSATION;
		return;
	}

	// Check if you have the requisite items for this choice
	for (i = 0; i < (int)reply->ItemCheck.Size(); ++i)
	{
		if (!CheckStrifeItem(player, reply->ItemCheck[i].Item, reply->ItemCheck[i].Amount))
		{
			// No, you don't. Say so and let the NPC animate negatively.
			if (reply->QuickNo.IsNotEmpty() && isconsole)
			{
				TerminalResponse(reply->QuickNo);
			}
			npc->ConversationAnimation(2);
			if (!(npc->flags8 & MF8_DONTFACETALKER))
				npc->Angles.Yaw = player->ConversationNPCAngle;
			npc->flags5 &= ~MF5_INCONVERSATION;
			return;
		}
	}

	// Yay, you do! Let the NPC animate affirmatively.
	npc->ConversationAnimation(1);

	// If this reply gives you something, then try to receive it.
	takestuff = true;
	if (reply->GiveType != NULL)
	{
		if (reply->GiveType->IsDescendantOf(NAME_Inventory))
		{
			if (reply->GiveType->IsDescendantOf(NAME_Weapon))
			{
				if (player->mo->FindInventory(reply->GiveType) != NULL)
				{
					takestuff = false;
				}
			}
	
			if (takestuff)
			{
				auto item = Spawn(reply->GiveType);
				// Items given here should not count as items!
				item->ClearCounters();
				if (item->GetClass()->TypeName == NAME_FlameThrower)
				{
					// The flame thrower gives less ammo when given in a dialog
					item->IntVar(NAME_AmmoGive1) = 40;
				}
				item->flags |= MF_DROPPED;
				if (!CallTryPickup(item, player->mo))
				{
					item->Destroy();
					takestuff = false;
				}
			}
		
			if (reply->GiveType->IsDescendantOf("SlideshowStarter"))
				gameaction = ga_slideshow;
		}
		else
		{
			// Trying to give a non-inventory item.
			takestuff = false;
			if (isconsole)
			{
				Printf("Attempting to give non-inventory item %s\n", reply->GiveType->TypeName.GetChars());
			}
		}
	}

	if (reply->ActionSpecial != 0)
	{
		takestuff |= !!P_ExecuteSpecial(reply->ActionSpecial, NULL, player->mo, false,
			reply->Args[0], reply->Args[1], reply->Args[2], reply->Args[3], reply->Args[4]);
	}

	// Take away required items if the give was successful or none was needed.
	if (takestuff)
	{
		for (i = 0; i < (int)reply->ItemCheck.Size(); ++i)
		{
			TakeStrifeItem (player, reply->ItemCheck[i].Item, reply->ItemCheck[i].Amount);
		}
		replyText = reply->QuickYes;
	}
	else
	{
		replyText = "$txt_haveenough";
	}

	// Update the quest log, if needed.
	if (reply->LogString.IsNotEmpty())
	{
		const char *log = reply->LogString;
		if (log[0] == '$')
		{
			log = GStrings(log + 1);
		}

		player->SetLogText(log);
	}
	else if (reply->LogNumber != 0) 
	{
		player->SetLogNumber(reply->LogNumber);
	}

	if (nullptr != replyText && '\0' != replyText[0] && isconsole)
	{
		TerminalResponse(replyText);
	}

	// Does this reply alter the speaker's conversation node? If NextNode is positive,
	// the next time they talk, they will show the new node. If it is negative, then they
	// will show the new node right away without terminating the dialogue.
	if (reply->NextNode != 0)
	{
		int rootnode = npc->ConversationRoot;
		const bool isNegative = reply->NextNode < 0;
		const unsigned next = (unsigned)(rootnode + (isNegative ? -1 : 1) * reply->NextNode - 1);

		if (next < StrifeDialogues.Size())
		{
			npc->Conversation = StrifeDialogues[next];

			if (isNegative)
			{
				if (gameaction != ga_slideshow)
				{
					P_StartConversation (npc, player->mo, player->ConversationFaceTalker, false);
					return;
				}
				else
				{
					S_StopSound (npc, CHAN_VOICE);
				}
			}
		}
		else
		{
			Printf ("Next node %u is invalid, no such dialog page\n", next);
		}
	}

	if (!(npc->flags8 & MF8_DONTFACETALKER))
		npc->Angles.Yaw = player->ConversationNPCAngle;

	// [CW] Set these to NULL because we're not using to them
	// anymore. However, this can interfere with slideshows
	// so we don't set them to NULL in that case.
	if (gameaction != ga_slideshow)
	{
		npc->flags5 &= ~MF5_INCONVERSATION;
		player->ConversationFaceTalker = false;
		player->ConversationNPC = NULL;
		player->ConversationPC = NULL;
		player->ConversationNPCAngle = 0.;
	}

	if (isconsole)
	{
		I_SetMusicVolume (level.MusicVolume);
	}
}

//============================================================================
//
// P_ConversationCommand
//
// Complete a conversation command.
//
//============================================================================

void P_ConversationCommand (int netcode, int pnum, uint8_t **stream)
{
	player_t *player = &players[pnum];

	// The conversation menus are normally closed by the menu code, but that
	// doesn't happen during demo playback, so we need to do it here.
	if (demoplayback && CurrentMenu != NULL && CurrentMenu->IsKindOf("ConversationMenu"))
	{
		CurrentMenu->Close();
	}
	if (netcode == DEM_CONVREPLY)
	{
		int nodenum = ReadWord(stream);
		int replynum = ReadByte(stream);
		HandleReply(player, pnum == consoleplayer, nodenum, replynum);
	}
	else
	{
		assert(netcode == DEM_CONVNULL || netcode == DEM_CONVCLOSE);
		if (player->ConversationNPC != NULL)
		{
			if (!(player->ConversationNPC->flags8 & MF8_DONTFACETALKER))
				player->ConversationNPC->Angles.Yaw = player->ConversationNPCAngle;
			player->ConversationNPC->flags5 &= ~MF5_INCONVERSATION;
		}
		if (netcode == DEM_CONVNULL)
		{
			player->ConversationFaceTalker = false;
			player->ConversationNPC = NULL;
			player->ConversationPC = NULL;
			player->ConversationNPCAngle = 0.;
		}
	}
}

//============================================================================
//
// TerminalResponse
//
// Similar to C_MidPrint, but lower and colored and sized to match the
// rest of the dialogue text.
//
//============================================================================

static void TerminalResponse (const char *str)
{
	if (str != NULL)
	{
		// handle string table replacement
		if (str[0] == '$')
		{
			str = GStrings(str + 1);
		}

		if (StatusBar != NULL)
		{
			AddToConsole(-1, str);
			AddToConsole(-1, "\n");
			// The message is positioned a bit above the menu choices, because
			// merchants can tell you something like this but continue to show
			// their dialogue screen. I think most other conversations use this
			// only as a response for terminating the dialogue.
			StatusBar->AttachMessage(Create<DHUDMessageFadeOut>(SmallFont, str,
				float(CleanWidth/2) + 0.4f, float(ConversationMenuY - 110 + CleanHeight/2), CleanWidth, -CleanHeight,
				CR_UNTRANSLATED, 3.f, 1.f), MAKE_ID('T','A','L','K'));
		}
		else
		{
			Printf("%s\n", str);
		}
	}
}

DEFINE_FIELD(FStrifeDialogueNode, DropType);
DEFINE_FIELD(FStrifeDialogueNode, ThisNodeNum);
DEFINE_FIELD(FStrifeDialogueNode, ItemCheckNode);
DEFINE_FIELD(FStrifeDialogueNode, SpeakerType);
DEFINE_FIELD(FStrifeDialogueNode, SpeakerName);
DEFINE_FIELD(FStrifeDialogueNode, SpeakerVoice);
DEFINE_FIELD(FStrifeDialogueNode, Backdrop);
DEFINE_FIELD(FStrifeDialogueNode, Dialogue);
DEFINE_FIELD(FStrifeDialogueNode, Goodbye);
DEFINE_FIELD(FStrifeDialogueNode, Children);
DEFINE_FIELD(FStrifeDialogueNode, MenuClassName);
DEFINE_FIELD(FStrifeDialogueNode, UserData);

DEFINE_FIELD(FStrifeDialogueReply, Next);
DEFINE_FIELD(FStrifeDialogueReply, GiveType);
DEFINE_FIELD(FStrifeDialogueReply, ActionSpecial);
DEFINE_FIELD(FStrifeDialogueReply, Args);
DEFINE_FIELD(FStrifeDialogueReply, PrintAmount);
DEFINE_FIELD(FStrifeDialogueReply, Reply);
DEFINE_FIELD(FStrifeDialogueReply, QuickYes);
DEFINE_FIELD(FStrifeDialogueReply, QuickNo);
DEFINE_FIELD(FStrifeDialogueReply, LogString);
DEFINE_FIELD(FStrifeDialogueReply, NextNode);
DEFINE_FIELD(FStrifeDialogueReply, LogNumber);
DEFINE_FIELD(FStrifeDialogueReply, NeedsGold);

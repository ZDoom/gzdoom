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
#include "s_sound.h"
#include "v_text.h"
#include "v_video.h"
#include "m_random.h"
#include "gi.h"
#include "templates.h"
#include "a_strifeglobal.h"
#include "a_keys.h"
#include "p_enemy.h"
#include "gstrings.h"
#include "sound/i_music.h"
#include "p_setup.h"
#include "d_net.h"
#include "g_level.h"
#include "d_event.h"
#include "d_gui.h"
#include "doomstat.h"
#include "c_console.h"
#include "sbar.h"
#include "farchive.h"
#include "p_lnspec.h"
#include "r_utility.h"
#include "p_local.h"
#include "menu/menu.h"

// The conversations as they exist inside a SCRIPTxx lump.
struct Response
{
	SDWORD GiveType;
	SDWORD Item[3];
	SDWORD Count[3];
	char Reply[32];
	char Yes[80];
	SDWORD Link;
	DWORD Log;
	char No[80];
};

struct Speech
{
	DWORD SpeakerType;
	SDWORD DropType;
	SDWORD ItemCheck[3];
	SDWORD Link;
	char Name[16];
	char Sound[8];
	char Backdrop[8];
	char Dialogue[320];
	Response Responses[5];
};

// The Teaser version of the game uses an older version of the structure
struct TeaserSpeech
{
	DWORD SpeakerType;
	SDWORD DropType;
	DWORD VoiceNumber;
	char Name[16];
	char Dialogue[320];
	Response Responses[5];
};

static FRandom pr_randomspeech("RandomSpeech");

void GiveSpawner (player_t *player, const PClass *type);

TArray<FStrifeDialogueNode *> StrifeDialogues;

typedef TMap<int, int> FDialogueIDMap;				// maps dialogue IDs to dialogue array index (for ACS)
typedef TMap<FName, int> FDialogueMap;				// maps actor class names to dialogue array index

FClassMap StrifeTypes;
static FDialogueIDMap DialogueRoots;
static FDialogueMap ClassRoots;
static int ConversationMenuY;

static int ConversationPauseTic;
static bool ShowGold;

static bool LoadScriptFile(int lumpnum, FileReader *lump, int numnodes, bool include, int type);
static FStrifeDialogueNode *ReadRetailNode (FileReader *lump, DWORD &prevSpeakerType);
static FStrifeDialogueNode *ReadTeaserNode (FileReader *lump, DWORD &prevSpeakerType);
static void ParseReplies (FStrifeDialogueReply **replyptr, Response *responses);
static bool DrawConversationMenu ();
static void PickConversationReply (int replyindex);
static void TerminalResponse (const char *str);

static FStrifeDialogueNode *PrevNode;

#define NUM_RANDOM_LINES 10
#define NUM_RANDOM_GOODBYES 3

//============================================================================
//
// GetStrifeType
//
// Given an item type number, returns the corresponding PClass.
//
//============================================================================

void SetStrifeType(int convid, const PClass *Class)
{
	StrifeTypes[convid] = Class;
}

void ClearStrifeTypes()
{
	StrifeTypes.Clear();
}

void SetConversation(int convid, const PClass *Class, int dlgindex)
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

const PClass *GetStrifeType (int typenum)
{
	const PClass **ptype = StrifeTypes.CheckKey(typenum);
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
		map->Seek(ML_CONVERSATION);
		LoadScriptFile (map->lumpnum, map->file, map->Size(ML_CONVERSATION), false, 0);
	}
	else
	{
		if (strnicmp (mapname, "MAP", 3) != 0)
		{
			return;
		}
		char scriptname_b[9] = { 'S','C','R','I','P','T',mapname[3],mapname[4],0 };
		char scriptname_t[9] = { 'D','I','A','L','O','G',mapname[3],mapname[4],0 };

		if (!LoadScriptFile(scriptname_t, false, 2))
		{
			if (!LoadScriptFile (scriptname_b, false, 1))
			{
				LoadScriptFile ("SCRIPT00", false, 1);
			}
		}
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
	FileReader *lump;

	if (lumpnum < 0)
	{
		return false;
	}
	lump = Wads.ReopenLumpNum (lumpnum);

	bool res = LoadScriptFile(lumpnum, lump, Wads.LumpLength(lumpnum), include, type);
	delete lump;
	return res;
}

static bool LoadScriptFile(int lumpnum, FileReader *lump, int numnodes, bool include, int type)
{
	int i;
	DWORD prevSpeakerType;
	FStrifeDialogueNode *node;
	char buffer[4];

	lump->Read(buffer, 4);
	lump->Seek(-4, SEEK_CUR);

	// The binary format is so primitive that this check is enough to detect it.
	bool isbinary = (buffer[0] == 0 || buffer[1] == 0 || buffer[2] == 0 || buffer[3] == 0);

	if ((type == 1 && !isbinary) || (type == 2 && isbinary))
	{
		DPrintf("Incorrect data format for %s.", Wads.GetLumpFullName(lumpnum));
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
				DPrintf("Incorrect data format for %s.", Wads.GetLumpFullName(lumpnum));
				return false;
			}
			numnodes /= 1516;
		}
		else
		{
			// And the teaser version has 1488-byte entries.
			if (numnodes % 1488 != 0)
			{
				DPrintf("Incorrect data format for %s.", Wads.GetLumpFullName(lumpnum));
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

static FStrifeDialogueNode *ReadRetailNode (FileReader *lump, DWORD &prevSpeakerType)
{
	FStrifeDialogueNode *node;
	Speech speech;
	char fullsound[16];
	const PClass *type;
	int j;

	node = new FStrifeDialogueNode;

	lump->Read (&speech, sizeof(speech));

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
	node->Dialogue = ncopystring (speech.Dialogue);

	// The speaker's portrait, if any.
	speech.Dialogue[0] = 0; 	//speech.Backdrop[8] = 0;
	node->Backdrop = TexMan.CheckForTexture (speech.Backdrop, FTexture::TEX_MiscPatch);

	// The speaker's voice for this node, if any.
	speech.Backdrop[0] = 0; 	//speech.Sound[8] = 0;
	mysnprintf (fullsound, countof(fullsound), "svox/%s", speech.Sound);
	node->SpeakerVoice = fullsound;

	// The speaker's name, if any.
	speech.Sound[0] = 0; 		//speech.Name[16] = 0;
	node->SpeakerName = ncopystring (speech.Name);

	// The item the speaker should drop when killed.
	node->DropType = GetStrifeType (speech.DropType);

	// Items you need to have to make the speaker use a different node.
	node->ItemCheck.Resize(3);
	for (j = 0; j < 3; ++j)
	{
		node->ItemCheck[j].Item = GetStrifeType (speech.ItemCheck[j]);
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

static FStrifeDialogueNode *ReadTeaserNode (FileReader *lump, DWORD &prevSpeakerType)
{
	FStrifeDialogueNode *node;
	TeaserSpeech speech;
	char fullsound[16];
	const PClass *type;
	int j;

	node = new FStrifeDialogueNode;

	lump->Read (&speech, sizeof(speech));

	// Byte swap all the ints in the original data
	speech.SpeakerType = LittleLong(speech.SpeakerType);
	speech.DropType = LittleLong(speech.DropType);

	// Assign the first instance of a conversation as the default for its
	// actor, so newly spawned actors will use this conversation by default.
	type = GetStrifeType (speech.SpeakerType);
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
	node->Dialogue = ncopystring (speech.Dialogue);

	// The Teaser version doesn't have portraits.
	node->Backdrop.SetInvalid();

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
	node->SpeakerName = ncopystring (speech.Name);

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
		reply->LogString = NULL;

		// The item to receive when this reply is used.
		reply->GiveType = GetStrifeType (rsp->GiveType);
		reply->ActionSpecial = 0;

		// Do you need anything special for this reply to succeed?
		reply->ItemCheck.Resize(3);
		for (k = 0; k < 3; ++k)
		{
			reply->ItemCheck[k].Item = GetStrifeType (rsp->Item[k]);
			reply->ItemCheck[k].Amount = rsp->Count[k];
		}

		// If the first item check has a positive amount required, then
		// add that to the reply string. Otherwise, use the reply as-is.
		reply->Reply = copystring (rsp->Reply);
		reply->NeedsGold = (rsp->Count[0] > 0);

		// QuickYes messages are shown when you meet the item checks.
		// QuickNo messages are shown when you don't.
		if (rsp->Yes[0] == '_' && rsp->Yes[1] == 0)
		{
			reply->QuickYes = NULL;
		}
		else
		{
			reply->QuickYes = ncopystring (rsp->Yes);
		}
		if (reply->ItemCheck[0].Item != 0)
		{
			reply->QuickNo = ncopystring (rsp->No);
		}
		else
		{
			reply->QuickNo = NULL;
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
	if (SpeakerName != NULL) delete[] SpeakerName;
	if (Dialogue != NULL) delete[] Dialogue;
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
// FStrifeDialogueReply :: ~FStrifeDialogueReply
//
//============================================================================

FStrifeDialogueReply::~FStrifeDialogueReply ()
{
	if (Reply != NULL) delete[] Reply;
	if (QuickYes != NULL) delete[] QuickYes;
	if (QuickNo != NULL) delete[] QuickNo;
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

static bool CheckStrifeItem (player_t *player, const PClass *itemtype, int amount=-1)
{
	AInventory *item;

	if (itemtype == NULL || amount == 0)
		return true;

	item = player->ConversationPC->FindInventory (itemtype);
	if (item == NULL)
		return false;

	return amount < 0 || item->Amount >= amount;
}

//============================================================================
//
// TakeStrifeItem
//
// Takes away some of an item, unless that item is special and should not
// be removed.
//
//============================================================================

static void TakeStrifeItem (player_t *player, const PClass *itemtype, int amount)
{
	if (itemtype == NULL || amount == 0)
		return;

	// Don't take quest items.
	if (itemtype->IsDescendantOf (PClass::FindClass(NAME_QuestItem)))
		return;

	// Don't take keys.
	if (itemtype->IsDescendantOf (RUNTIME_CLASS(AKey)))
		return;

	// Don't take the sigil.
	if (itemtype == RUNTIME_CLASS(ASigil))
		return;

	player->mo->TakeInventory(itemtype, amount);
}

CUSTOM_CVAR(Float, dlg_musicvolume, 1.0f, CVAR_ARCHIVE)
{
	if (self < 0.f) self = 0.f;
	else if (self > 1.f) self = 1.f;
}

//============================================================================
//
// The conversation menu
//
//============================================================================

class DConversationMenu : public DMenu
{
	DECLARE_CLASS(DConversationMenu, DMenu)

	FString mSpeaker;
	FBrokenLines *mDialogueLines;
	TArray<FString> mResponseLines;
	TArray<unsigned int> mResponses;
	bool mShowGold;
	FStrifeDialogueNode *mCurNode;
	int mYpos;

public:
	static int mSelection;

	//=============================================================================
	//
	//
	//
	//=============================================================================

	DConversationMenu(FStrifeDialogueNode *CurNode) 
	{
		mCurNode = CurNode;
		mDialogueLines = NULL;
		mShowGold = false;

		// Format the speaker's message.
		const char * toSay = CurNode->Dialogue;
		if (strncmp (toSay, "RANDOM_", 7) == 0)
		{
			FString dlgtext;

			dlgtext.Format("TXT_%s_%02d", toSay, 1+(pr_randomspeech() % NUM_RANDOM_LINES));
			toSay = GStrings[dlgtext];
			if (toSay == NULL)
			{
				toSay = GStrings["TXT_GOAWAY"];	// Ok, it's lame - but it doesn't look like an error to the player. ;)
			}
		}
		else
		{
			// handle string table replacement
			if (toSay[0] == '$')
			{
				toSay = GStrings(toSay + 1);
			}
		}
		if (toSay == NULL)
		{
			toSay = ".";
		}
		mDialogueLines = V_BreakLines (SmallFont, screen->GetWidth()/CleanXfac - 24*2, toSay);

		FStrifeDialogueReply *reply;
		int i,j;
		for (reply = CurNode->Children, i = 1; reply != NULL; reply = reply->Next)
		{
			if (reply->Reply == NULL)
			{
				continue;
			}
			mShowGold |= reply->NeedsGold;

			const char *ReplyText = reply->Reply;
			if (ReplyText[0] == '$')
			{
				ReplyText = GStrings(ReplyText + 1);
			}
			FString ReplyString = ReplyText;
			if (reply->NeedsGold) ReplyString.AppendFormat(" for %u", reply->ItemCheck[0].Amount);

			FBrokenLines *ReplyLines = V_BreakLines (SmallFont, 320-50-10, ReplyString);

			mResponses.Push(mResponseLines.Size());
			for (j = 0; ReplyLines[j].Width >= 0; ++j)
			{
				mResponseLines.Push(ReplyLines[j].Text);
			}
			++i;
			V_FreeBrokenLines (ReplyLines);
		}
		char goodbye[25];
		mysnprintf(goodbye, countof(goodbye), "TXT_RANDOMGOODBYE_%d", 1+(pr_randomspeech() % NUM_RANDOM_GOODBYES));
		const char *goodbyestr = GStrings[goodbye];
		if (goodbyestr == NULL) goodbyestr = "Bye.";
		mResponses.Push(mResponseLines.Size());
		mResponseLines.Push(FString(goodbyestr));

		// Determine where the top of the reply list should be positioned.
		i = OptionSettings.mLinespacing;
		mYpos = MIN<int> (140, 192 - mResponseLines.Size() * i);
		for (i = 0; mDialogueLines[i].Width >= 0; ++i)
		{ }
		i = 44 + i * 10;
		if (mYpos - 100 < i - screen->GetHeight() / CleanYfac / 2)
		{
			mYpos = i - screen->GetHeight() / CleanYfac / 2 + 100;
		}
		ConversationMenuY = mYpos;
		//ConversationMenu.indent = 50;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	void Destroy()
	{
		V_FreeBrokenLines(mDialogueLines);
		mDialogueLines = NULL;
		I_SetMusicVolume (1.f);
	}

	bool DimAllowed()
	{
		return false;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	bool MenuEvent(int mkey, bool fromcontroller)
	{
		if (demoplayback)
		{ // During demo playback, don't let the user do anything besides close this menu.
			if (mkey == MKEY_Back)
			{
				Close();
				return true;
			}
			return false;
		}
		if (mkey == MKEY_Up)
		{
			if (--mSelection < 0) mSelection = mResponses.Size() - 1;
			return true;
		}
		else if (mkey == MKEY_Down)
		{
			if (++mSelection >= (int)mResponses.Size()) mSelection = 0;
			return true;
		}
		else if (mkey == MKEY_Back)
		{
			Net_WriteByte (DEM_CONVNULL);
			Close();
			return true;
		}
		else if (mkey == MKEY_Enter)
		{
			if ((unsigned)mSelection >= mResponses.Size())
			{
				Net_WriteByte(DEM_CONVCLOSE);
			}
			else
			{
				// Send dialogue and reply numbers across the wire.
				assert((unsigned)mCurNode->ThisNodeNum < StrifeDialogues.Size());
				assert(StrifeDialogues[mCurNode->ThisNodeNum] == mCurNode);
				Net_WriteByte(DEM_CONVREPLY);
				Net_WriteWord(mCurNode->ThisNodeNum);
				Net_WriteByte(mSelection);
			}
			Close();
			return true;
		}
		return false;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	bool MouseEvent(int type, int x, int y)
	{
		int sel = -1;
		int fh = OptionSettings.mLinespacing;

		// convert x/y from screen to virtual coordinates, according to CleanX/Yfac use in DrawTexture
		x = ((x - (screen->GetWidth() / 2)) / CleanXfac) + 160;
		y = ((y - (screen->GetHeight() / 2)) / CleanYfac) + 100;

		if (x >= 24 && x <= 320-24 && y >= mYpos && y < mYpos + fh * (int)mResponseLines.Size())
		{
			sel = (y - mYpos) / fh;
			for(unsigned i=0;i<mResponses.Size(); i++)
			{
				if ((int)mResponses[i] > sel)
				{
					sel = i-1;
					break;
				}
			}
		}
		if (sel != -1 && sel != mSelection)
		{
			//S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", snd_menuvolume, ATTN_NONE);
		}
		mSelection = sel;
		if (type == MOUSE_Release)
		{
			return MenuEvent(MKEY_Enter, true);
		}
		return true;
	}


	//=============================================================================
	//
	//
	//
	//=============================================================================

	bool Responder(event_t *ev)
	{
		if (demoplayback)
		{ // No interaction during demo playback
			return false;
		}
		if (ev->type == EV_GUI_Event && ev->subtype == EV_GUI_Char && ev->data1 >= '0' && ev->data1 <= '9')
		{ // Activate an item of type numberedmore (dialogue only)
			mSelection = ev->data1 == '0' ? 9 : ev->data1 - '1';
			return MenuEvent(MKEY_Enter, false);
		}
		return Super::Responder(ev);
	}

	//============================================================================
	//
	// DrawConversationMenu
	//
	//============================================================================

	void Drawer()
	{
		const char *speakerName;
		int x, y, linesize;
		int width, fontheight;

		player_t *cp = &players[consoleplayer];

		assert (mDialogueLines != NULL);
		assert (mCurNode != NULL);

		FStrifeDialogueNode *CurNode = mCurNode;

		if (CurNode == NULL)
		{
			Close ();
			return;
		}

		// [CW] Freeze the game depending on MAPINFO options.
		if (ConversationPauseTic < gametic && !multiplayer && !(level.flags2 & LEVEL2_CONV_SINGLE_UNFREEZE))
		{
			menuactive = MENU_On;
		}

		if (CurNode->Backdrop.isValid())
		{
			screen->DrawTexture (TexMan(CurNode->Backdrop), 0, 0, DTA_320x200, true, TAG_DONE);
		}
		x = 16 * screen->GetWidth() / 320;
		y = 16 * screen->GetHeight() / 200;
		linesize = 10 * CleanYfac;

		// Who is talking to you?
		if (CurNode->SpeakerName != NULL)
		{
			speakerName = CurNode->SpeakerName;
			if (speakerName[0] == '$') speakerName = GStrings(speakerName+1);
		}
		else
		{
			speakerName = cp->ConversationNPC->GetTag("Person");
		}

		// Dim the screen behind the dialogue (but only if there is no backdrop).
		if (!CurNode->Backdrop.isValid())
		{
			int i;
			for (i = 0; mDialogueLines[i].Width >= 0; ++i)
			{ }
			screen->Dim (0, 0.45f, 14 * screen->GetWidth() / 320, 13 * screen->GetHeight() / 200,
				308 * screen->GetWidth() / 320 - 14 * screen->GetWidth () / 320,
				speakerName == NULL ? linesize * i + 6 * CleanYfac
				: linesize * i + 6 * CleanYfac + linesize * 3/2);
		}

		// Dim the screen behind the PC's choices.

		screen->Dim (0, 0.45f, (24-160) * CleanXfac + screen->GetWidth()/2,
			(mYpos - 2 - 100) * CleanYfac + screen->GetHeight()/2,
			272 * CleanXfac,
			MIN<int>(mResponseLines.Size() * OptionSettings.mLinespacing + 4, 200 - mYpos) * CleanYfac);

		if (speakerName != NULL)
		{
			screen->DrawText (SmallFont, CR_WHITE, x, y, speakerName,
				DTA_CleanNoMove, true, TAG_DONE);
			y += linesize * 3 / 2;
		}
		x = 24 * screen->GetWidth() / 320;
		for (int i = 0; mDialogueLines[i].Width >= 0; ++i)
		{
			screen->DrawText (SmallFont, CR_UNTRANSLATED, x, y, mDialogueLines[i].Text,
				DTA_CleanNoMove, true, TAG_DONE);
			y += linesize;
		}

		if (ShowGold)
		{
			AInventory *coin = cp->ConversationPC->FindInventory (RUNTIME_CLASS(ACoin));
			char goldstr[32];

			mysnprintf (goldstr, countof(goldstr), "%d", coin != NULL ? coin->Amount : 0);
			screen->DrawText (SmallFont, CR_GRAY, 21, 191, goldstr, DTA_320x200, true,
				DTA_FillColor, 0, DTA_Alpha, HR_SHADOW, TAG_DONE);
			screen->DrawTexture (TexMan(((AInventory *)GetDefaultByType (RUNTIME_CLASS(ACoin)))->Icon),
				3, 190, DTA_320x200, true,
				DTA_FillColor, 0, DTA_Alpha, HR_SHADOW, TAG_DONE);
			screen->DrawText (SmallFont, CR_GRAY, 20, 190, goldstr, DTA_320x200, true, TAG_DONE);
			screen->DrawTexture (TexMan(((AInventory *)GetDefaultByType (RUNTIME_CLASS(ACoin)))->Icon),
				2, 189, DTA_320x200, true, TAG_DONE);
		}

		y = mYpos;
		fontheight = OptionSettings.mLinespacing;

		int response = 0;
		for (unsigned i = 0; i < mResponseLines.Size(); i++, y += fontheight)
		{
			width = SmallFont->StringWidth(mResponseLines[i]);
			x = 64;

			screen->DrawText (SmallFont, CR_GREEN, x, y, mResponseLines[i], DTA_Clean, true, TAG_DONE);

			if (i == mResponses[response])
			{
				char tbuf[16];

				response++;
				mysnprintf (tbuf, countof(tbuf), "%d.", response);
				x = 50 - SmallFont->StringWidth (tbuf);
				screen->DrawText (SmallFont, CR_GREY, x, y, tbuf, DTA_Clean, true, TAG_DONE);

				if (response == mSelection+1)
				{
					int color = ((DMenu::MenuTime%8) < 4) || DMenu::CurrentMenu != this ? CR_RED:CR_GREY;

					x = (50 + 3 - 160) * CleanXfac + screen->GetWidth() / 2;
					int yy = (y + fontheight/2 - 5 - 100) * CleanYfac + screen->GetHeight() / 2;
					screen->DrawText (ConFont, color, x, yy, "\xd",
						DTA_CellX, 8 * CleanXfac,
						DTA_CellY, 8 * CleanYfac,
						TAG_DONE);
				}
			}
		}
	}

};

IMPLEMENT_ABSTRACT_CLASS(DConversationMenu)
int DConversationMenu::mSelection;	// needs to be preserved if the same dialogue is restarted


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
	if (DMenu::CurrentMenu != NULL && DMenu::CurrentMenu->IsKindOf(RUNTIME_CLASS(DConversationMenu)))
	{
		DMenu::CurrentMenu->Close();
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

	pc->velx = pc->vely = 0;	// Stop moving
	pc->player->velx = pc->player->vely = 0;
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
		pc->player->ConversationNPCAngle = npc->angle;
	}
	oldtarget = npc->target;
	npc->target = pc;
	if (facetalker)
	{
		A_FaceTarget (npc);
		pc->angle = pc->AngleTo(npc);
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

		DConversationMenu *cmenu = new DConversationMenu(CurNode);


		if (CurNode != PrevNode)
		{ // Only reset the selection if showing a different menu.
			DConversationMenu::mSelection = 0;
			PrevNode = CurNode;
		}

		// And open the menu
		M_StartControlPanel (false);
		M_ActivateMenu(cmenu);
		ConversationPauseTic = gametic + 20;
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
		npc->angle = player->ConversationNPCAngle;
		npc->flags5 &= ~MF5_INCONVERSATION;
		return;
	}

	// Check if you have the requisite items for this choice
	for (i = 0; i < (int)reply->ItemCheck.Size(); ++i)
	{
		if (!CheckStrifeItem(player, reply->ItemCheck[i].Item, reply->ItemCheck[i].Amount))
		{
			// No, you don't. Say so and let the NPC animate negatively.
			if (reply->QuickNo && isconsole)
			{
				TerminalResponse(reply->QuickNo);
			}
			npc->ConversationAnimation(2);
			npc->angle = player->ConversationNPCAngle;
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
		if (reply->GiveType->IsDescendantOf(RUNTIME_CLASS(AInventory)))
		{
			if (reply->GiveType->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
			{
				if (player->mo->FindInventory(reply->GiveType) != NULL)
				{
					takestuff = false;
				}
			}
	
			if (takestuff)
			{
				AInventory *item = static_cast<AInventory *>(Spawn(reply->GiveType, 0, 0, 0, NO_REPLACE));
				// Items given here should not count as items!
				item->ClearCounters();
				if (item->GetClass()->TypeName == NAME_FlameThrower)
				{
					// The flame thrower gives less ammo when given in a dialog
					static_cast<AWeapon*>(item)->AmmoGive1 = 40;
				}
				item->flags |= MF_DROPPED;
				if (!item->CallTryPickup(player->mo))
				{
					item->Destroy();
					takestuff = false;
				}
			}
		
			if (reply->GiveType->IsDescendantOf(RUNTIME_CLASS(ASlideshowStarter)))
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
	if (reply->LogString != NULL)
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

	if (replyText != NULL && isconsole)
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

	npc->angle = player->ConversationNPCAngle;

	// [CW] Set these to NULL because we're not using to them
	// anymore. However, this can interfere with slideshows
	// so we don't set them to NULL in that case.
	if (gameaction != ga_slideshow)
	{
		npc->flags5 &= ~MF5_INCONVERSATION;
		player->ConversationFaceTalker = false;
		player->ConversationNPC = NULL;
		player->ConversationPC = NULL;
		player->ConversationNPCAngle = 0;
	}

	if (isconsole)
	{
		I_SetMusicVolume (1.f);
	}
}

//============================================================================
//
// P_ConversationCommand
//
// Complete a conversation command.
//
//============================================================================

void P_ConversationCommand (int netcode, int pnum, BYTE **stream)
{
	player_t *player = &players[pnum];

	// The conversation menus are normally closed by the menu code, but that
	// doesn't happen during demo playback, so we need to do it here.
	if (demoplayback && DMenu::CurrentMenu != NULL &&
		DMenu::CurrentMenu->IsKindOf(RUNTIME_CLASS(DConversationMenu)))
	{
		DMenu::CurrentMenu->Close();
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
			player->ConversationNPC->angle = player->ConversationNPCAngle;
			player->ConversationNPC->flags5 &= ~MF5_INCONVERSATION;
		}
		if (netcode == DEM_CONVNULL)
		{
			player->ConversationFaceTalker = false;
			player->ConversationNPC = NULL;
			player->ConversationPC = NULL;
			player->ConversationNPCAngle = 0;
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
			StatusBar->AttachMessage(new DHUDMessageFadeOut(SmallFont, str,
				float(CleanWidth/2) + 0.4f, float(ConversationMenuY - 110 + CleanHeight/2), CleanWidth, -CleanHeight,
				CR_UNTRANSLATED, 3, 1), MAKE_ID('T','A','L','K'));
		}
		else
		{
			Printf("%s\n", str);
		}
	}
}


template<> FArchive &operator<< (FArchive &arc, FStrifeDialogueNode *&node)
{
	DWORD convnum;
	if (arc.IsStoring())
	{
		arc.WriteCount (node == NULL? ~0u : node->ThisNodeNum);
	}
	else 
	{
		convnum = arc.ReadCount();
		if (convnum >= StrifeDialogues.Size())
		{
			node = NULL;
		}
		else
		{
			node = StrifeDialogues[convnum];
		}
	}
	return arc;
}

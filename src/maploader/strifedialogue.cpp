/*
** strifedialogue.cpp
** loads Strife style conversation dialogs
**
**---------------------------------------------------------------------------
** Copyright 2004-2008 Randy Heit
** Copyright 2006-2019 Christoph Oelckers
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
#include "p_setup.h"
#include "d_net.h"
#include "d_event.h"
#include "doomstat.h"
#include "c_console.h"
#include "g_levellocals.h"
#include "maploader.h"

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

//============================================================================
//
// P_LoadStrifeConversations
//
// Loads the SCRIPT00 and SCRIPTxx files for a corresponding map.
//
//============================================================================

void MapLoader::LoadStrifeConversations (MapData *map, const char *mapname)
{
	if (map->Size(ML_CONVERSATION) > 0)
	{
		LoadScriptFile (nullptr, map->lumpnum, map->Reader(ML_CONVERSATION), map->Size(ML_CONVERSATION), false, 0);
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

bool MapLoader::LoadScriptFile (const char *name, bool include, int type)
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

	auto fn = Wads.GetLumpFile(lumpnum);
	auto wadname = Wads.GetWadName(fn);
	if (stricmp(wadname, "STRIFE0.WAD") && stricmp(wadname, "STRIFE1.WAD") && stricmp(wadname, "SVE.WAD")) name = nullptr;	// Only localize IWAD content.

	bool res = LoadScriptFile(name, lumpnum, lump, Wads.LumpLength(lumpnum), include, type);
	return res;
}

bool MapLoader::LoadScriptFile(const char *name, int lumpnum, FileReader &lump, int numnodes, bool include, int type)
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
		ParseUSDF(lumpnum, lump, numnodes);
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
				node = ReadRetailNode (name, lump, prevSpeakerType);
			}
			else
			{
				node = ReadTeaserNode (name, lump, prevSpeakerType);
			}
			node->ThisNodeNum = Level->StrifeDialogues.Push(node);
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

static FString TokenFromString(const char *speech)
{
	FString token = speech;
	token.ToUpper();
	token.ReplaceChars(".,-+!?'", ' ');
	token.Substitute(" ", "");
	token.Truncate(5);
	return token;
}

FStrifeDialogueNode *MapLoader::ReadRetailNode (const char *name, FileReader &lump, uint32_t &prevSpeakerType)
{
	FStrifeDialogueNode *node;
	Speech speech;
	char fullsound[16];
	PClassActor *type;
	int j;

	node = new FStrifeDialogueNode;

	auto pos = lump.Tell();
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
			Level->ClassRoots[type->TypeName] = Level->StrifeDialogues.Size();
		}
		Level->DialogueRoots[speech.SpeakerType] = Level->StrifeDialogues.Size();
		prevSpeakerType = speech.SpeakerType;
	}

	// Convert the rest of the data to our own internal format.

	if (name && strncmp(speech.Dialogue, "RANDOM_", 7))
	{
		FStringf label("$TXT_DLG_%s_d%d_%s", name, int(pos), TokenFromString(speech.Dialogue).GetChars());
		node->Dialogue = GStrings.exists(label.GetChars()+1)? label : FString(speech.Dialogue);
	}
	else
	{
		node->Dialogue = speech.Dialogue;
	}

	// The speaker's portrait, if any.
	speech.Dialogue[0] = 0; 	//speech.Backdrop[8] = 0;
	node->Backdrop = speech.Backdrop;

	// The speaker's voice for this node, if any.
	speech.Backdrop[0] = 0; 	//speech.Sound[8] = 0;
	mysnprintf (fullsound, countof(fullsound), "svox/%s", speech.Sound);
	node->SpeakerVoice = fullsound;

	// The speaker's name, if any.
	speech.Sound[0] = 0; 		//speech.Name[16] = 0;
	if (name && speech.Name[0])
	{
		FString label = speech.Name;
		label.ReplaceChars(' ', '_');
		label.ReplaceChars('\'', '_');
		node->SpeakerName.Format("$TXT_SPEAKER_%s", label.GetChars());
		if (!GStrings.exists(node->SpeakerName.GetChars() + 1)) node->SpeakerName = speech.Name;

	}
	else
	{
		node->SpeakerName = speech.Name;
	}

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

	ParseReplies (name, int(pos), &node->Children, &speech.Responses[0]);

	return node;
}

//============================================================================
//
// ReadTeaserNode
//
// Converts a single dialogue node from the Teaser version of Strife.
//
//============================================================================

FStrifeDialogueNode *MapLoader::ReadTeaserNode (const char *name, FileReader &lump, uint32_t &prevSpeakerType)
{
	FStrifeDialogueNode *node;
	TeaserSpeech speech;
	char fullsound[16];
	PClassActor *type;
	int j;

	node = new FStrifeDialogueNode;

	auto pos = lump.Tell() * 1516 / 1488;
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
			Level->ClassRoots[type->TypeName] = Level->StrifeDialogues.Size();
		}
		Level->DialogueRoots[speech.SpeakerType] = Level->StrifeDialogues.Size();
		prevSpeakerType = speech.SpeakerType;
	}

	// Convert the rest of the data to our own internal format.
	if (name && strncmp(speech.Dialogue, "RANDOM_", 7))
	{
		FStringf label("$TXT_DLG_%s_d%d_%s", name, pos, TokenFromString(speech.Dialogue).GetChars());
		node->Dialogue = GStrings.exists(label.GetChars() + 1)? label : FString(speech.Dialogue);
	}
	else
	{
		node->Dialogue = speech.Dialogue;
	}

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
	if ((name && speech.Name[0]))
	{
		FString label = speech.Name;
		label.ReplaceChars(' ', '_');
		label.ReplaceChars('\'', '_');
		node->SpeakerName.Format("$TXT_SPEAKER_%s", label.GetChars());
		if (!GStrings.exists(node->SpeakerName.GetChars() + 1)) node->SpeakerName = speech.Name;
	}
	else
	{
		node->SpeakerName = speech.Name;
	}

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

	ParseReplies (name, int(pos), &node->Children, &speech.Responses[0]);

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

void MapLoader::ParseReplies (const char *name, int pos, FStrifeDialogueReply **replyptr, Response *responses)
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
		if (reply->NextNode < 0)
		{
			reply->NextNode *= -1;
			reply->CloseDialog = false;
		}

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

		if (name)
		{
			FStringf label("$TXT_RPLY%d_%s_d%d_%s", j, name, pos, TokenFromString(rsp->Reply).GetChars());
			reply->Reply = GStrings.exists(label.GetChars() + 1)? label : FString(rsp->Reply);

			reply->Reply = label;
		}
		else
		{
			reply->Reply = rsp->Reply;
		}


		// If the first item check has a positive amount required, then
		// add that to the reply string. Otherwise, use the reply as-is.
		reply->NeedsGold = (rsp->Count[0] > 0);

		// QuickYes messages are shown when you meet the item checks.
		// QuickNo messages are shown when you don't.
		// Note that empty nodes contain a '_' in retail Strife, a '.' in the teasers and an empty string in SVE.
		if (((rsp->Yes[0] == '_' || rsp->Yes[0] == '.') && rsp->Yes[1] == 0) || rsp->Yes[0] == 0)
		{
			reply->QuickYes = "";
		}
		else
		{
			if (name)
			{
				FStringf label("$TXT_RYES%d_%s_d%d_%s", j, name, pos, TokenFromString(rsp->Yes).GetChars());
				reply->QuickYes = GStrings.exists(label.GetChars() + 1)? label : FString(rsp->Yes);
			}
			else
			{
				reply->QuickYes = rsp->Yes;
			}
		}
		if (reply->ItemCheck[0].Item != 0)
		{
			FStringf label("$TXT_RNO%d_%s_d%d_%s", j, name, pos, TokenFromString(rsp->No).GetChars());
			reply->QuickNo = GStrings.exists(label.GetChars() + 1)? label : FString(rsp->No);
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


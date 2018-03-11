//
// p_usdf.cpp
//
// USDF dialogue parser
//
//---------------------------------------------------------------------------
// Copyright (c) 2010
//		Braden "Blzut3" Obrzut <admin@maniacsvault.net>
//		Christoph Oelckers
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//    * Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//    * Neither the name of the <organization> nor the
//      names of its contributors may be used to endorse or promote products
//      derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "p_setup.h"
#include "p_lnspec.h"
#include "templates.h"
#include "i_system.h"
#include "p_conversation.h"
#include "p_udmf.h"
#include "doomerrors.h"
#include "cmdlib.h"
#include "actor.h"
#include "a_pickups.h"
#include "w_wad.h"

#define Zd 1
#define St 2

class USDFParser : public UDMFParserBase
{
	//===========================================================================
	//
	// Checks an actor type (different representation depending on namespace)
	//
	//===========================================================================

	PClassActor *CheckActorType(const char *key)
	{
		PClassActor *type = nullptr;
		if (namespace_bits == St)
		{
			type = GetStrifeType(CheckInt(key));
		}
		else if (namespace_bits == Zd)
		{
			PClassActor *cls = PClass::FindActor(CheckString(key));
			if (cls == nullptr)
			{
				sc.ScriptMessage("Unknown actor class '%s'", key);
				return nullptr;
			}
			type = cls;
		}
		return type;
	}

	PClassActor *CheckInventoryActorType(const char *key)
	{
		PClassActor* const type = CheckActorType(key);
		return nullptr != type && type->IsDescendantOf(RUNTIME_CLASS(AInventory)) ? type : nullptr;
	}

	//===========================================================================
	//
	// Parse a cost/require/exclude block
	//
	//===========================================================================

	bool ParseCostRequireExclude(FStrifeDialogueReply *response, FName type)
	{
		FStrifeDialogueItemCheck check;
		check.Item = NULL;
		check.Amount = -1;

		while (!sc.CheckToken('}'))
		{
			FName key = ParseKey();
			switch(key)
			{
			case NAME_Item:
				check.Item = CheckInventoryActorType(key);
				break;

			case NAME_Amount:
				check.Amount = CheckInt(key);
				break;
			}
		}

		switch (type)
		{
		case NAME_Cost:		response->ItemCheck.Push(check);	break;
		case NAME_Require:	response->ItemCheckRequire.Push(check); break;
		case NAME_Exclude:	response->ItemCheckExclude.Push(check); break;
		}
		return true;
	}

	//===========================================================================
	//
	// Parse a choice block
	//
	//===========================================================================

	bool ParseChoice(FStrifeDialogueReply **&replyptr)
	{
		FStrifeDialogueReply *reply = new FStrifeDialogueReply;

		reply->Next = *replyptr;
		*replyptr = reply;
		replyptr = &reply->Next;

		FString ReplyString;
		FString QuickYes;
		FString QuickNo;
		FString LogString;
		bool closeDialog = false;


		reply->NeedsGold = false;
		while (!sc.CheckToken('}'))
		{
			bool block = false;
			FName key = ParseKey(true, &block);
			if (!block)
			{
				switch(key)
				{
				case NAME_Text:
					ReplyString = CheckString(key);
					break;

				case NAME_Displaycost:
					reply->NeedsGold = CheckBool(key);
					break;

				case NAME_Yesmessage:
					QuickYes = CheckString(key);
					//if (!QuickYes.Compare("_")) QuickYes = "";
					break;

				case NAME_Nomessage:
					QuickNo = CheckString(key);
					break;

				case NAME_Log:
					if (namespace_bits == St)
					{
						const char *s = CheckString(key);
						if(strlen(s) < 4 || strnicmp(s, "LOG", 3) != 0)
						{
							sc.ScriptMessage("Log must be in the format of LOG# to compile, ignoring.");
						}
						else
						{
							reply->LogNumber = atoi(s + 3);
						}
					}
					else
					{
						LogString = CheckString(key);
					}
					break;

				case NAME_Giveitem:
					reply->GiveType = CheckActorType(key);
					break;

				case NAME_Nextpage:
					reply->NextNode = CheckInt(key);
					break;

				case NAME_Closedialog:
					closeDialog = CheckBool(key);
					break;

				case NAME_Special:
					reply->ActionSpecial = CheckInt(key);
					if (reply->ActionSpecial < 0)
						reply->ActionSpecial = 0;
					break;

				case NAME_SpecialName:
					if (namespace_bits == Zd)
						reply->ActionSpecial = P_FindLineSpecial(CheckString(key));
					break;

				case NAME_Arg0:
				case NAME_Arg1:
				case NAME_Arg2:
				case NAME_Arg3:
				case NAME_Arg4:
					reply->Args[int(key)-int(NAME_Arg0)] = CheckInt(key);
					break;


				}
			}
			else
			{
				switch(key)
				{
				case NAME_Cost:
				case NAME_Require:
				case NAME_Exclude:
					// Require and Exclude are exclusive to namespace ZDoom. [FishyClockwork]
					if (key == NAME_Cost || namespace_bits == Zd)
					{
						ParseCostRequireExclude(reply, key);
						break;
					}
					// Intentional fall-through

				default:
					sc.UnGet();
					Skip();
				}
			}
		}
		// Todo: Finalize
		if (reply->ItemCheck.Size() > 0)
		{
			reply->PrintAmount = reply->ItemCheck[0].Amount;
			if (reply->PrintAmount <= 0) reply->NeedsGold = false;
		}

		reply->Reply = ReplyString;
		reply->QuickYes = QuickYes;
		if (reply->ItemCheck.Size() > 0 && reply->ItemCheck[0].Item != NULL)
		{
			reply->QuickNo = QuickNo;
		}
		else
		{
			reply->QuickNo = "";
		}
		reply->LogString = LogString;
		if(!closeDialog) reply->NextNode *= -1;
		return true;
	}

	//===========================================================================
	//
	// Parse an ifitem block
	//
	//===========================================================================

	bool ParseIfItem(FStrifeDialogueNode *node)
	{
		FStrifeDialogueItemCheck check;
		check.Item = NULL;
		check.Amount = -1;

		while (!sc.CheckToken('}'))
		{
			FName key = ParseKey();
			switch(key)
			{
			case NAME_Item:
				check.Item = CheckInventoryActorType(key);
				break;

			case NAME_Count:
				// Not yet implemented in the engine. Todo later
				check.Amount = CheckInt(key);
				break;
			}
		}

		node->ItemCheck.Push(check);
		return true;
	}

	//===========================================================================
	//
	// Parse a page block
	//
	//===========================================================================

	bool ParsePage()
	{
		FStrifeDialogueNode *node = new FStrifeDialogueNode;
		FStrifeDialogueReply **replyptr = &node->Children;

		node->ThisNodeNum = StrifeDialogues.Push(node);
		node->ItemCheckNode = -1;

		FString SpeakerName;
		FString Dialogue;
		FString Goodbye;

		while (!sc.CheckToken('}'))
		{
			bool block = false;
			FName key = ParseKey(true, &block);
			if (!block)
			{
				switch(key)
				{
				case NAME_Name:
					SpeakerName = CheckString(key);
					break;

				case NAME_Panel:
					node->Backdrop = CheckString(key);
					break;

				case NAME_Userstring:
					if (namespace_bits == Zd)
					{
						node->UserData = CheckString(key);
					}
					break;

				case NAME_Voice:
					{
						const char * name = CheckString(key);
						if (name[0] != 0)
						{
							FString soundname = "svox/";
							soundname += name;
							node->SpeakerVoice = FSoundID(S_FindSound(soundname));
							if (node->SpeakerVoice == 0 && namespace_bits == Zd)
							{
								node->SpeakerVoice = FSoundID(S_FindSound(name));
							}
						}
					}
					break;

				case NAME_Dialog:
					Dialogue = CheckString(key);
					break;

				case NAME_Drop:
					node->DropType = CheckActorType(key);
					break;

				case NAME_Link:
					node->ItemCheckNode = CheckInt(key);
					break;

				case NAME_Goodbye:
					// Custom goodbyes are exclusive to namespace ZDoom. [FishyClockwork]
					if (namespace_bits == Zd)
					{
						Goodbye = CheckString(key);
					}
					break;
				}
			}
			else
			{
				switch(key)
				{
				case NAME_Ifitem:
					if (!ParseIfItem(node)) return false;
					break;

				case NAME_Choice:
					if (!ParseChoice(replyptr)) return false;
					break;

				default:
					sc.UnGet();
					Skip();
				}
			}
		}
		node->SpeakerName = SpeakerName;
		node->Dialogue = Dialogue;
		node->Goodbye = Goodbye;
		return true;
	}


	//===========================================================================
	//
	// Parse a conversation block
	//
	//===========================================================================

	bool ParseConversation()
	{
		PClassActor *type = NULL;
		int dlgid = -1;
		FName clsid;
		unsigned int startpos = StrifeDialogues.Size();

		while (!sc.CheckToken('}'))
		{
			bool block = false;
			FName key = ParseKey(true, &block);
			if (!block)
			{
				switch(key)
				{
				case NAME_Actor:
					type = CheckActorType(key);
					if (namespace_bits == St)
					{
						dlgid = CheckInt(key);
					}
					break;

				case NAME_Id:
					if (namespace_bits == Zd)
					{
						dlgid = CheckInt(key);
					}
					break;

				case NAME_Class:
					if (namespace_bits == Zd)
					{
						clsid = CheckString(key);
					}
					break;
				}
			}
			else
			{
				switch(key)
				{
				case NAME_Page:
					if (!ParsePage()) return false;
					break;

				default:
					sc.UnGet();
					Skip();
				}
			}
		}
		if (type == NULL && dlgid == 0)
		{
			sc.ScriptMessage("No valid actor type defined in conversation.");
			return false;
		}
		SetConversation(dlgid, type, startpos);
		for(;startpos < StrifeDialogues.Size(); startpos++)
		{
			StrifeDialogues[startpos]->SpeakerType = type;
			StrifeDialogues[startpos]->MenuClassName = clsid;
		}
		return true;
	}

	//===========================================================================
	//
	// Parse an USDF lump
	//
	//===========================================================================

public:
	bool Parse(int lumpnum, FileReader &lump, int lumplen)
	{
		char *buffer = new char[lumplen];
		lump.Read(buffer, lumplen);
		sc.OpenMem(Wads.GetLumpFullName(lumpnum), buffer, lumplen);
		delete [] buffer;
		sc.SetCMode(true);
		// Namespace must be the first field because everything else depends on it.
		if (sc.CheckString("namespace"))
		{
			sc.MustGetToken('=');
			sc.MustGetToken(TK_StringConst);
			namespc = sc.String;
			switch(namespc)
			{
			case NAME_ZDoom:
				namespace_bits = Zd;
				break;
			case NAME_Strife:
				namespace_bits = St;
				break;
			default:
				sc.ScriptMessage("Unknown namespace %s. Ignoring dialogue lump.\n", sc.String);
				return false;
			}
			sc.MustGetToken(';');
		}
		else
		{
			sc.ScriptMessage("Map does not define a namespace.\n");
			return false;
		}

		while (sc.GetString())
		{
			if (sc.Compare("conversation"))
			{
				sc.MustGetToken('{');
				if (!ParseConversation()) return false;
			}
			else if (sc.Compare("include"))
			{
				sc.MustGetToken('=');
				sc.MustGetToken(TK_StringConst);
				LoadScriptFile(sc.String, true);
				sc.MustGetToken(';');
			}
			else
			{
				Skip();
			}
		}
		return true;
	}
};



bool P_ParseUSDF(int lumpnum, FileReader &lump, int lumplen)
{
	USDFParser parse;

	try
	{
		if (!parse.Parse(lumpnum, lump, lumplen))
		{
			// clean up the incomplete dialogue structures here
			return false;
		}
		return true;
	}
	catch(CRecoverableError &err)
	{
		Printf("%s", err.GetMessage());
		return false;
	}
}

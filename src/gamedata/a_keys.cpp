/*
** a_keys.cpp
** Implements all keys and associated data
**
**---------------------------------------------------------------------------
** Copyright 2005-2016 Cheistoph Oelckers
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
#include "a_keys.h"
#include "tarray.h"
#include "gi.h"
#include "gstrings.h"
#include "d_player.h"
#include "sbar.h"
#include "filesystem.h"
#include "v_font.h"
#include "vm.h"
#include "g_levellocals.h"

//===========================================================================
//
// Data for the LOCKDEFS
//
//===========================================================================

//===========================================================================
//
//
//===========================================================================

struct OneKey
{
	PClassActor *key;
	int count;

	bool check(AActor *owner)
	{
		// P_GetMapColorForKey() checks the key directly
		if (owner->IsA(key) || owner->GetSpecies() == key->TypeName) return true;

		if (owner->IsKindOf(NAME_DehackedPickup))
		{
			auto cls = owner->GetClass()->ActorInfo()->Replacee;
			if (cls == key || owner->Species == key->TypeName)
			{
				return true;
			}
		}


		// Other calls check an actor that may have a key in its inventory.
		for (AActor *item = owner->Inventory; item != NULL; item = item->Inventory)
		{
			if (item->IsA(key))
			{
				return true;
			}
			else if (item->GetSpecies() == key->TypeName)
			{
				return true;
			}
		}
		return false;
	}
};

//===========================================================================
//
//
//===========================================================================

struct Keygroup
{
	TArray<OneKey> anykeylist;

	bool check(AActor *owner)
	{
		for(unsigned int i=0;i<anykeylist.Size();i++)
		{
			if (anykeylist[i].check(owner)) return true;
		}
		return false;
	}
};

//===========================================================================
//
//
//===========================================================================

struct Lock
{
	TArray<Keygroup> keylist;
	TArray<FSoundID> locksound;
	FString Message;
	FString RemoteMsg;
	int	rgb = 0;

	bool check(AActor * owner)
	{
		// An empty key list means that any key will do
		if (!keylist.Size())
		{
			auto kt = PClass::FindActor(NAME_Key);
			for (AActor *item = owner->Inventory; item != NULL; item = item->Inventory)
			{
				if (item->IsKindOf (kt))
				{
					return true;
				}
			}
			return false;
		}
		else for(unsigned int i=0;i<keylist.Size();i++)
		{
			if (!keylist[i].check(owner)) return false;
		}
		return true;
	}
};

static TMap<int, Lock> Locks;
static bool keysdone = false;		// have the locks been initialized?
static TArray<PClassActor *> KeyTypes;	// List of all keys sorted by lock. This is for use by the status bar to draw ordered key lists.

//===========================================================================
//
//
//===========================================================================

static const char * keywords_lock[]={
	"ANY",
	"MESSAGE",
	"REMOTEMESSAGE",
	"MAPCOLOR",
	"LOCKEDSOUND",
	NULL
};

//===========================================================================
//
//
//===========================================================================

static void AddOneKey(Keygroup *keygroup, PClassActor *mi, FScanner &sc, bool ignorekey, int &currentnumber)
{
	if (mi)
	{
		// Any inventory item can be used to unlock a door
		if (mi->IsDescendantOf(NAME_Inventory))
		{
			OneKey k = {mi,1};
			keygroup->anykeylist.Push (k);

			//... but only keys get key numbers!
			if (mi->IsDescendantOf(NAME_Key))
			{
				if (!ignorekey &&
					GetDefaultByType(mi)->special1 == 0)
				{
					GetDefaultByType(mi)->special1 = ++currentnumber;
				}
			}
		}
		else
		{
			sc.ScriptError("'%s' is not an inventory item", sc.String);
		}
	}
	else
	{
		sc.ScriptError("Unknown item '%s'", sc.String);
	}
}


//===========================================================================
//
//
//===========================================================================

static void ParseKeygroup(Keygroup *keygroup, FScanner &sc, bool ignorekey, int &currentnumber)
{
	PClassActor *mi;

	sc.MustGetStringName("{");
	while (!sc.CheckString("}"))
	{
		sc.MustGetString();
		mi = PClass::FindActor(sc.String);
		AddOneKey(keygroup, mi, sc, ignorekey, currentnumber);
	}
	keygroup->anykeylist.ShrinkToFit();
}

//===========================================================================
//
//
//===========================================================================

static void PrintMessage (const char *str)
{
	if (str != NULL)
	{
		if (str[0]=='$') 
		{
			str = GStrings.GetString(str+1);
		}
		C_MidPrint (nullptr, str);
	}
}

//===========================================================================
//
//
//===========================================================================

static void ParseLock(FScanner &sc, int &currentnumber)
{
	int i,r,g,b;
	int keynum;
	Lock sink;
	PClassActor *mi;

	sc.MustGetNumber();
	keynum = sc.Number;

	sc.MustGetString();
	if (!sc.Compare("{"))
	{
		if (!CheckGame(sc.String, false)) keynum = -1;
		sc.MustGetStringName("{");
	}

	if (keynum == 0 || keynum < -1)
	{
		sc.ScriptError("Lock index %d out of range", keynum);
	}
	bool ignorekey = keynum == -1;	// tell the parsing functions to ignore what they read for other games' keys.

	auto lock = keynum == -1? &sink : &Locks.InsertNew(keynum);

	lock->locksound.Push(S_FindSound("*keytry"));
	lock->locksound.Push(S_FindSound("misc/keytry"));

	while (!sc.CheckString("}"))
	{
		sc.MustGetString();
		switch(i = sc.MatchString(keywords_lock))
		{
		case 0:	// Any
		{
			lock->keylist.Reserve(1);
			ParseKeygroup(&lock->keylist.Last(), sc, ignorekey, currentnumber);
			if (lock->keylist.Last().anykeylist.Size() == 0)
			{
				lock->keylist.Pop();
			}
			break;
		}

		case 1:	// message
			sc.MustGetString();
			lock->Message = sc.String;
			break;

		case 2: // remotemsg
			sc.MustGetString();
			lock->RemoteMsg = sc.String;
			break;

		case 3:	// mapcolor
			sc.MustGetNumber();
			r = sc.Number;
			sc.MustGetNumber();
			g = sc.Number;
			sc.MustGetNumber();
			b = sc.Number;
			lock->rgb = MAKERGB(r,g,b);
			break;

		case 4:	// locksound
			lock->locksound.Clear();
			for (;;)
			{
				sc.MustGetString();
				lock->locksound.Push(S_FindSound(sc.String));
				if (!sc.GetString())
				{
					break;
				}
				if (!sc.Compare(","))
				{
					sc.UnGet();
					break;
				}
			}
			break;

		default:
			mi = PClass::FindActor(sc.String);
			if (mi) 
			{
				lock->keylist.Reserve(1);
				AddOneKey(&lock->keylist.Last(), mi, sc, ignorekey, currentnumber);
				if (lock->keylist.Last().anykeylist.Size() == 0)
				{
					lock->keylist.Pop();
				}
			}
			break;
		}
	}
	// copy the messages if the other one does not exist
	if (lock->RemoteMsg.IsEmpty() && lock->Message.IsNotEmpty())
	{
		lock->RemoteMsg = lock->Message;
	}
	if (lock->Message.IsEmpty() && lock->RemoteMsg.IsNotEmpty())
	{
		lock->Message = lock->RemoteMsg;
	}
	lock->keylist.ShrinkToFit();
}

//===========================================================================
//
// Clears all key numbers so the parser can assign its own ones
// This ensures that only valid keys are considered by the key cheats
//
//===========================================================================

static void ClearLocks()
{
	unsigned int i;
	auto kt = PClass::FindActor(NAME_Key);
	for(i = 0; i < PClassActor::AllActorClasses.Size(); i++)
	{
		if (PClassActor::AllActorClasses[i]->IsDescendantOf(kt))
		{
			auto key = GetDefaultByType(PClassActor::AllActorClasses[i]);
			if (key != NULL)
			{
				key->special1 = 0;
			}
		}
	}
	Locks.Clear();
	keysdone = false;
}

//---------------------------------------------------------------------------
//
// create a sorted list of the defined keys so 
// this doesn't have to be done each frame
//
// For use by the HUD and statusbar code to get a consistent order.
//
//---------------------------------------------------------------------------

static int ktcmp(const void * a, const void * b)
{
	auto key1 = GetDefaultByType(*(PClassActor **)a);
	auto key2 = GetDefaultByType(*(PClassActor **)b);
	return key1->special1 - key2->special1;
}

static void CreateSortedKeyList()
{
	TArray<PClassActor *> UnassignedKeyTypes;
	KeyTypes.Clear();
	for (unsigned int i = 0; i < PClassActor::AllActorClasses.Size(); i++)
	{
		PClassActor *ti = PClassActor::AllActorClasses[i];
		auto kt = PClass::FindActor(NAME_Key);

		if (ti->IsDescendantOf(kt))
		{
			auto key = GetDefaultByType(ti);

			if (key->special1 > 0)
			{
				KeyTypes.Push(ti);
			}
			else
			{
				UnassignedKeyTypes.Push(ti);
			}
		}
	}
	if (KeyTypes.Size())
	{
		qsort(&KeyTypes[0], KeyTypes.Size(), sizeof(KeyTypes[0]), ktcmp);
	}
	KeyTypes.Append(UnassignedKeyTypes);
}

//===========================================================================
//
// P_InitKeyMessages
//
//===========================================================================

void P_InitKeyMessages()
{
	int lastlump, lump, currentnumber = 0;

	lastlump = 0;

	ClearLocks();
	while ((lump = fileSystem.FindLump ("LOCKDEFS", &lastlump)) != -1)
	{
		FScanner sc(lump);
		while (sc.GetString ())
		{
			if (sc.Compare("LOCK")) 
			{
				ParseLock(sc, currentnumber);
			}
			else if (sc.Compare("CLEARLOCKS"))
			{
				// clear all existing lock definitions and key numbers
				ClearLocks();
				currentnumber = 0;
			}
			else
			{
				sc.ScriptError("Unknown command %s in LockDef", sc.String);
			}
		}
		sc.Close();
	}
	CreateSortedKeyList();
	keysdone = true;
}

//===========================================================================
//
// P_CheckKeys
//
// Returns true if the actor has the required key. If not, a message is
// shown if the actor is also the consoleplayer's camarea, and false is
// returned.
//
//===========================================================================

int P_CheckKeys (AActor *owner, int keynum, bool remote, bool quiet)
{
	const char *failtext = NULL;
	FSoundID *failsound;
	int numfailsounds;

	if (owner == NULL) return false;
	if (keynum<=0) return true;
	// Just a safety precaution. The messages should have been initialized upon game start.
	if (!keysdone) P_InitKeyMessages();

	FSoundID failage[2] = { S_FindSound("*keytry"), S_FindSound("misc/keytry") };

	auto lock = Locks.CheckKey(keynum);
	if (!lock) 
	{
		if (quiet) return false;
		if (keynum == 103 && (gameinfo.flags & GI_SHAREWARE))
			failtext = "$TXT_RETAIL_ONLY";
		else
			failtext = "$TXT_DOES_NOT_WORK";

		failsound = failage;
		numfailsounds = countof(failage);
	}
	else
	{
		if (lock->check(owner)) return true;
		if (quiet) return false;
		failtext = remote? lock->RemoteMsg.GetChars() : lock->Message.GetChars();
		failsound = &lock->locksound[0];
		numfailsounds = lock->locksound.Size();
	}

	// If we get here, that means the actor isn't holding an appropriate key.

	// show a message if we're viewing as the current actor, or if the message was triggered by a voodoo doll of the current player
	bool doprintmsg = owner->CheckLocalView() || 
		(!(owner->Level->i_compatflags2 & COMPATF2_NOVDOLLLOCKMSG) && owner->player && owner->player->mo->CheckLocalView());
	if ( doprintmsg )
	{
		PrintMessage(failtext);

		// Play the first defined key sound.
		for (int i = 0; i < numfailsounds; ++i)
		{
			if (failsound[i] != NO_SOUND)
			{
				auto snd = S_FindSkinnedSound(owner, failsound[i]);
				if (snd != NO_SOUND)
				{
					S_Sound (owner, CHAN_VOICE, 0, snd, 1, ATTN_NORM);
					break;
				}
			}
		}
	}

	return false;
}

// [MK] for ZScript, simply returns if a lock is defined or not
int P_IsLockDefined(int keynum)
{
	return !!Locks.CheckKey(keynum);
}

//==========================================================================
//
// These functions can be used to get color information for
// automap display of keys and locked doors
//
//==========================================================================

int P_GetMapColorForLock (int locknum)
{
	if (locknum > 0)
	{
		auto lock = Locks.CheckKey(locknum);

		if (lock) return lock->rgb;
	}
	return -1;
}

//==========================================================================
//
//
//
//==========================================================================

int P_GetMapColorForKey (AActor * key)
{
	decltype(Locks)::Iterator it(Locks);
	decltype(Locks)::Pair *pair;

	int foundlock = INT_MAX;
	int rgb = 0;
	while (it.NextPair(pair))
	{
		if (pair->Key < foundlock && pair->Value.check(key))
		{
			rgb = pair->Value.rgb;
			foundlock = pair->Key;
		}
	}
	return rgb;
}


int P_GetKeyTypeCount()
{
	return KeyTypes.Size();
}

PClassActor *P_GetKeyType(int num)
{
	if ((unsigned)num >= KeyTypes.Size()) return nullptr;
	return KeyTypes[num];
}

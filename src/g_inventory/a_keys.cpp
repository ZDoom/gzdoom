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
#include "c_console.h"
#include "w_wad.h"
#include "v_font.h"
#include "vm.h"

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
	TArray<Keygroup *> keylist;
	TArray<FSoundID> locksound;
	FString Message;
	FString RemoteMsg;
	int	rgb;

	Lock()
	{
		rgb=0;
	}

	~Lock()
	{
		for(unsigned int i=0;i<keylist.Size();i++) delete keylist[i];
		keylist.Clear();
	}

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
			if (!keylist[i]->check(owner)) return false;
		}
		return true;
	}
};

//===========================================================================
//
//
//===========================================================================

static Lock *locks[256];		// all valid locks
static bool keysdone=false;		// have the locks been initialized?
static int currentnumber;		// number to be assigned to next key
static bool ignorekey;			// set to true when the current lock is not being used
static TArray<PClassActor *> KeyTypes;	// List of all keys sorted by lock.

static void ClearLocks();

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

static void AddOneKey(Keygroup *keygroup, PClassActor *mi, FScanner &sc)
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

static Keygroup *ParseKeygroup(FScanner &sc)
{
	Keygroup *keygroup;
	PClassActor *mi;

	sc.MustGetStringName("{");
	keygroup = new Keygroup;
	while (!sc.CheckString("}"))
	{
		sc.MustGetString();
		mi = PClass::FindActor(sc.String);
		AddOneKey(keygroup, mi, sc);
	}
	if (keygroup->anykeylist.Size() == 0)
	{
		delete keygroup;
		return NULL;
	}
	keygroup->anykeylist.ShrinkToFit();
	return keygroup;
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
			str = GStrings(str+1);
		}
		C_MidPrint (SmallFont, str);
	}
}

//===========================================================================
//
//
//===========================================================================

static void ParseLock(FScanner &sc)
{
	int i,r,g,b;
	int keynum;
	Lock sink;
	Lock *lock = &sink;
	Keygroup *keygroup;
	PClassActor *mi;

	sc.MustGetNumber();
	keynum = sc.Number;

	sc.MustGetString();
	if (!sc.Compare("{"))
	{
		if (!CheckGame(sc.String, false)) keynum = -1;
		sc.MustGetStringName("{");
	}

	ignorekey = true;
	if (keynum > 0 && keynum <= 255) 
	{
		lock = new Lock;
		if (locks[keynum])
		{
			delete locks[keynum];
		}
		locks[keynum] = lock;
		locks[keynum]->locksound.Push("*keytry");
		locks[keynum]->locksound.Push("misc/keytry");
		ignorekey=false;
	}
	else if (keynum != -1)
	{
		sc.ScriptError("Lock index %d out of range", keynum);
	}

	while (!sc.CheckString("}"))
	{
		sc.MustGetString();
		switch(i = sc.MatchString(keywords_lock))
		{
		case 0:	// Any
			keygroup = ParseKeygroup(sc);
			if (keygroup)
			{
				lock->keylist.Push(keygroup);
			}
			break;

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
				lock->locksound.Push(sc.String);
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
				keygroup = new Keygroup;
				AddOneKey(keygroup, mi, sc);
				if (keygroup) 
				{
					keygroup->anykeylist.ShrinkToFit();
					lock->keylist.Push(keygroup);
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
	for(i = 0; i < 256; i++)
	{
		if (locks[i] != NULL) 
		{
			delete locks[i];
			locks[i] = NULL;
		}
	}
	currentnumber = 0;
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
	int lastlump, lump;

	lastlump = 0;

	ClearLocks();
	while ((lump = Wads.FindLump ("LOCKDEFS", &lastlump)) != -1)
	{
		FScanner sc(lump);
		while (sc.GetString ())
		{
			if (sc.Compare("LOCK")) 
			{
				ParseLock(sc);
			}
			else if (sc.Compare("CLEARLOCKS"))
			{
				// clear all existing lock definitions and key numbers
				ClearLocks();
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
// P_DeinitKeyMessages
//
//===========================================================================

void P_DeinitKeyMessages()
{
	ClearLocks();
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
	if (keynum<=0 || keynum>255) return true;
	// Just a safety precaution. The messages should have been initialized upon game start.
	if (!keysdone) P_InitKeyMessages();

	FSoundID failage[2] = { "*keytry", "misc/keytry" };

	if (!locks[keynum]) 
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
		if (locks[keynum]->check(owner)) return true;
		if (quiet) return false;
		failtext = remote? locks[keynum]->RemoteMsg : locks[keynum]->Message;
		failsound = &locks[keynum]->locksound[0];
		numfailsounds = locks[keynum]->locksound.Size();
	}

	// If we get here, that means the actor isn't holding an appropriate key.

	if (owner == players[consoleplayer].camera)
	{
		PrintMessage(failtext);

		// Play the first defined key sound.
		for (int i = 0; i < numfailsounds; ++i)
		{
			if (failsound[i] != 0)
			{
				int snd = S_FindSkinnedSound(owner, failsound[i]);
				if (snd != 0)
				{
					S_Sound (owner, CHAN_VOICE, snd, 1, ATTN_NORM);
					break;
				}
			}
		}
	}

	return false;
}

//==========================================================================
//
// These functions can be used to get color information for
// automap display of keys and locked doors
//
//==========================================================================

int P_GetMapColorForLock (int lock)
{
	if (lock > 0 && lock < 256)
	{
		if (locks[lock]) return locks[lock]->rgb;
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
	int i;

	for (i = 0; i < 256; i++)
	{
		if (locks[i] && locks[i]->check(key)) return locks[i]->rgb;
	}
	return 0;
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

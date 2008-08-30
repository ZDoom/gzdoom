#include "a_keys.h"
#include "tarray.h"
#include "gi.h"
#include "gstrings.h"
#include "d_player.h"
#include "c_console.h"
#include "s_sound.h"
#include "sc_man.h"
#include "v_palette.h"
#include "w_wad.h"



struct OneKey
{
	const PClass * key;
	int count;

	bool check(AActor * owner)
	{
		return !!owner->FindInventory(key);
	}
};

struct Keygroup
{
	TArray<OneKey> anykeylist;

	bool check(AActor * owner)
	{
		for(unsigned int i=0;i<anykeylist.Size();i++)
		{
			if (anykeylist[i].check(owner)) return true;
		}
		return false;
	}
};

struct Lock
{
	TArray<Keygroup *> keylist;
	char * message;
	char * remotemsg;
	FSoundID locksound;
	int	rgb;

	Lock()
	{
		message=remotemsg=NULL;
		rgb=0;
	}

	~Lock()
	{
		for(unsigned int i=0;i<keylist.Size();i++) delete keylist[i];
		keylist.Clear();
		if (message) delete [] message;
		if (remotemsg) delete [] remotemsg;
	}

	bool check(AActor * owner)
	{
		// An empty key list means that any key will do
		if (!keylist.Size())
		{
			for (AInventory * item = owner->Inventory; item != NULL; item = item->Inventory)
			{
				if (item->IsKindOf (RUNTIME_CLASS(AKey)))
				{
					return true;
				}
			}
		}
		else for(unsigned int i=0;i<keylist.Size();i++)
		{
			if (!keylist[i]->check(owner)) return false;
		}
		return true;
	}
};


static Lock * locks[256];		// all valid locks
static bool keysdone=false;		// have the locks been initialized?
static int currentnumber;		// number to be assigned to next key
static bool ignorekey;			// set to true when the current lock is not being used

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

static void AddOneKey(Keygroup *keygroup, const PClass *mi, FScanner &sc)
{
	if (mi)
	{
		// Any inventory item can be used to unlock a door
		if (mi->IsDescendantOf(RUNTIME_CLASS(AInventory)))
		{
			OneKey k = {mi,1};
			keygroup->anykeylist.Push (k);

			//... but only keys get key numbers!
			if (mi->IsDescendantOf(RUNTIME_CLASS(AKey)))
			{
				if (!ignorekey &&
					static_cast<AKey*>(GetDefaultByType(mi))->KeyNumber == 0)
				{
					static_cast<AKey*>(GetDefaultByType(mi))->KeyNumber=++currentnumber;
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

static Keygroup * ParseKeygroup(FScanner &sc)
{
	Keygroup * keygroup;
	const PClass * mi;

	sc.MustGetStringName("{");
	keygroup = new Keygroup;
	while (!sc.CheckString("}"))
	{
		sc.MustGetString();
		mi = PClass::FindClass(sc.String);
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
			str=GStrings(str+1);
		}
		C_MidPrint (str);
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
	Lock * lock=&sink;
	Keygroup * keygroup;
	const PClass * mi;

	sc.MustGetNumber();
	keynum = sc.Number;

	sc.MustGetString();
	if (sc.Compare("DOOM"))
	{
		if (gameinfo.gametype != GAME_Doom) keynum=-1;
	}
	else if (sc.Compare("HERETIC"))
	{
		if (gameinfo.gametype != GAME_Heretic) keynum=-1;
	}
	else if (sc.Compare("HEXEN"))
	{
		if (gameinfo.gametype != GAME_Hexen) keynum=-1;
	}
	else if (sc.Compare("STRIFE"))
	{
		if (gameinfo.gametype != GAME_Strife) keynum=-1;
	}
	else if (sc.Compare("CHEX"))
	{
		if (gameinfo.gametype != GAME_Chex) keynum=-1;
	}
	else sc.UnGet();

	ignorekey = true;
	if (keynum > 0 && keynum < 255) 
	{
		lock = new Lock;
		if (locks[keynum])
		{
			delete locks[keynum];
		}
		locks[keynum] = lock;
		locks[keynum]->locksound = "misc/keytry";
		ignorekey=false;
	}
	else if (keynum != -1)
	{
		sc.ScriptError("Lock index %d out of range", keynum);
	}

	sc.MustGetStringName("{");
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
			lock->message = copystring(sc.String);
			break;

		case 2: // remotemsg
			sc.MustGetString();
			lock->remotemsg = copystring(sc.String);
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
			sc.MustGetString();
			lock->locksound = sc.String;
			break;

		default:
			mi = PClass::FindClass(sc.String);
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
	if (!lock->remotemsg && lock->message)
	{
		lock->remotemsg = copystring(lock->message);
	}
	if (!lock->message && lock->remotemsg)
	{
		lock->message = copystring(lock->remotemsg);
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
	for(i=0;i<PClass::m_Types.Size();i++)
	{
		if (PClass::m_Types[i]->IsDescendantOf(RUNTIME_CLASS(AKey)))
		{
			AKey *key = static_cast<AKey*>(GetDefaultByType(PClass::m_Types[i]));
			if (key != NULL)
			{
				key->KeyNumber = 0;
			}
		}
	}
	for(i=0;i<256;i++)
	{
		if (locks[i]!=NULL) 
		{
			delete locks[i];
			locks[i]=NULL;
		}
	}
	currentnumber=0;
	keysdone=false;
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

bool P_CheckKeys (AActor *owner, int keynum, bool remote)
{
	const char *failtext = NULL;
	FSoundID failsound;

	if (keynum<=0 || keynum>255) return true;
	// Just a safety precaution. The messages should have been initialized upon game start.
	if (!keysdone) P_InitKeyMessages();

	if (!locks[keynum]) 
	{
		if (keynum==103 && gameinfo.gametype == GAME_Strife)
			failtext = "THIS AREA IS ONLY AVAILABLE IN THE RETAIL VERSION OF STRIFE";
		else
			failtext = "That doesn't seem to work";

		failsound = "misc/keytry";
	}
	else
	{
		if (locks[keynum]->check(owner)) return true;
		failtext = remote? locks[keynum]->remotemsg : locks[keynum]->message;
		failsound = locks[keynum]->locksound;
	}

	// If we get here, that means the actor isn't holding an appropriate key.

	if (owner == players[consoleplayer].camera)
	{
		PrintMessage(failtext);
		S_Sound (owner, CHAN_VOICE, failsound, 1, ATTN_NORM);
	}

	return false;
}

//==========================================================================
//
// AKey implementation
//
//==========================================================================

IMPLEMENT_CLASS (AKey)

bool AKey::HandlePickup (AInventory *item)
{
	// In single player, you can pick up an infinite number of keys
	// even though you can only hold one of each.
	if (multiplayer)
	{
		return Super::HandlePickup (item);
	}
	if (GetClass() == item->GetClass())
	{
		item->ItemFlags |= IF_PICKUPGOOD;
		return true;
	}
	if (Inventory != NULL)
	{
		return Inventory->HandlePickup (item);
	}
	return false;
}

bool AKey::ShouldStay ()
{
	return !!multiplayer;
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

int P_GetMapColorForKey (AInventory * key)
{
	int i;

	for (i = 0; i < 256; i++)
	{
		if (locks[i] && locks[i]->check(key)) return locks[i]->rgb;
	}
	return 0;
}

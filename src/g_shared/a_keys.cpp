#include "a_keys.h"
#include "tarray.h"
#include "gi.h"
#include "gstrings.h"
#include "d_player.h"
#include "c_console.h"
#include "s_sound.h"

// Special "keys"
enum
{
	AnyKey = 100,
	AllKeys = 101,
	ThatDoesntWork = 102,
	OnlyInRetail = 103,
	All3Keys = AllKeys + 128
};

static TArray<const TypeInfo *> KeyTypes;

//
// Keys
//

// [RH] P_CheckKeys
//
//	Returns true if the player has the desired key,
//	false otherwise.

//===========================================================================
//
// FindValidKeys
//
// Records all the keys valid for the current game type. This is called the
// first time P_CheckKeys needs to print a messsage for a player who needs a
// key.
//
//===========================================================================

static void FindValidKeys ()
{
	const TypeInfo *basetype;
	int i;

	switch (gameinfo.gametype)
	{
	default:
	case GAME_Doom:		basetype = RUNTIME_CLASS(ADoomKey);		break;
	case GAME_Heretic:	basetype = RUNTIME_CLASS(AHereticKey);	break;
	case GAME_Hexen:	basetype = RUNTIME_CLASS(AHexenKey);	break;
	case GAME_Strife:	basetype = RUNTIME_CLASS(AStrifeKey);	break;
	}

	for (i = 0; i < TypeInfo::m_NumTypes; ++i)
	{
		if (TypeInfo::m_Types[i]->IsDescendantOf (basetype))
		{
			KeyTypes.Push (TypeInfo::m_Types[i]);
		}
	}
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
	const char *failtext;
	const char *failsound;
	DWORD keymask;
	AInventory *item;

	failtext = NULL;
	failsound = NULL;

	switch (keynum)
	{
	case 0:
		// Key 0 is like a psuedo-key. Everything pretends to have it, so it
		// can act as a no-key-needed flag.
		return true;

	case OnlyInRetail:
		failtext = "THIS AREA IS ONLY AVAILABLE IN THE RETAIL VERSION OF STRIFE";
		break;

	case ThatDoesntWork:
		failtext = "That doesn't seem to work";
		break;

	case AllKeys:
	case All3Keys:
		if (gameinfo.gametype == GAME_Strife)
		{
			// Strife has 26 (actually 27) different keys, so I am not
			// going to implement the AllKeys check for Strife.
			failtext = "That doesn't seem to work";
			break;
		}
		// Intentional fall-through

	default:
		keymask = 0;

		// See if the owner is holding a matching key.
		for (item = owner->Inventory; item != NULL; item = item->Inventory)
		{
			if (item->IsKindOf (RUNTIME_CLASS(AKey)))
			{
				AKey *key = static_cast<AKey *> (item);

				if (keynum == AnyKey)
				{
					return true;
				}
				else if ((keynum & 0x7f) == AllKeys)
				{
					keymask |= 1 << (key->KeyNumber - 1);
				}
				else if (key->KeyNumber == keynum || key->AltKeyNumber == keynum)
				{
					return true;
				}
			}
		}

		if ((keynum & 0x7f) == AllKeys)
		{
			// The check for holding "all keys" is only made against the
			// standard keys and not any custom user-added keys.
			switch (gameinfo.gametype)
			{
			case GAME_Doom:
				if (keynum & 0x80)
				{ // Cards and skulls are treated the same
					keymask |= keymask >> 3;
					keymask |= keymask << 3;
				}
				if ((keymask & (1|2|4|8|16|32)) == (1|2|4|8|16|32))
				{
					return true;
				}
				failtext = (keynum & 0x80) ? GStrings(PD_ALL3) : GStrings(PD_ALL6);
				break;

			case GAME_Heretic:
				if ((keymask & (1|2|4)) == (1|2|4))
				{
					return true;
				}
				failtext = GStrings(PD_ALL3);
				break;

			case GAME_Hexen:
				if ((keymask & ((1<<11)-1)) == ((1<<11)-1))
				{
					return true;
				}
				failtext = "You need all the keys";
				break;
				
			default:
				// The other enumerations won't reach this switch, but make GCC happy.
				failtext = "That doesn't seem to work";
				break;
			}
		}
		break;
	}

	// If we get here, that means the actor isn't holding an appropriate key.
	// If a failtext isn't already set, locate a key that would have satisfied
	// the lock and grab text from it.

	if (owner->CheckLocalView (consoleplayer))
	{
		if (KeyTypes.Size() == 0)
		{
			FindValidKeys ();
		}

		for (size_t i = 0; i < KeyTypes.Size(); ++i)
		{
			AKey *keydef = (AKey *)GetDefaultByType (KeyTypes[i]);
			if (keydef->KeyNumber == keynum || keydef->AltKeyNumber == keynum)
			{
				AKey *tempkey = static_cast<AKey*>(Spawn (KeyTypes[i], 0,0,0));
				failtext = tempkey->NeedKeyMessage (remote, keynum);
				failsound = tempkey->NeedKeySound ();
				tempkey->Destroy ();
				break;
			}
		}
		if (failtext == NULL)
		{
			failtext = "You don't have the key";
		}
		if (failsound == NULL)
		{
			failsound = "misc/keytry";
		}
		C_MidPrint (failtext);
		S_Sound (owner, CHAN_VOICE, failsound, 1, ATTN_NORM);
	}

	return false;
}

IMPLEMENT_STATELESS_ACTOR (AKey, Any, -1, 0)
 PROP_Inventory_FlagsSet (IF_INTERHUBSTRIP)
 PROP_Inventory_PickupSound ("misc/k_pkup")
END_DEFAULTS

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

const char *AKey::NeedKeyMessage (bool remote, int keynum)
{
	return "You don't have the key";
}

const char *AKey::NeedKeySound ()
{
	return "misc/keytry";
}

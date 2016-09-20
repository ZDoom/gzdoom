
#include "a_doomglobal.h"
#include "a_hexenglobal.h"
#include "a_strifeglobal.h"
#include "ravenshared.h"
#include "a_weaponpiece.h"
#include "d_dehacked.h"

// For NULL states, which aren't owned by any actor, the owner
// is recorded as AActor with the following state. AActor should
// never actually have this many states of its own, so this
// is (relatively) safe.

#define NULL_STATE_INDEX	127

// These are special tokens found in the data stream of an archive.
// Whenever a new object is encountered, it gets created using new and
// is then asked to serialize itself before processing of the previous
// object continues. This can result in some very deep recursion if
// you aren't careful about how you organize your data.

#define NEW_OBJ				((BYTE)1)	// Data for a new object follows
#define NEW_CLS_OBJ			((BYTE)2)	// Data for a new class and object follows
#define OLD_OBJ				((BYTE)3)	// Reference to an old object follows
#define NULL_OBJ			((BYTE)4)	// Load as NULL
#define M1_OBJ				((BYTE)44)	// Load as (DObject*)-1

#define NEW_PLYR_OBJ		((BYTE)5)	// Data for a new player follows
#define NEW_PLYR_CLS_OBJ	((BYTE)6)	// Data for a new class and player follows

#define NEW_NAME			((BYTE)27)	// A new name follows
#define OLD_NAME			((BYTE)28)	// Reference to an old name follows
#define NIL_NAME			((BYTE)33)	// Load as NULL

#define NEW_SPRITE			((BYTE)11)	// A new sprite name follows
#define OLD_SPRITE			((BYTE)12)	// Reference to an old sprite name follows



void P_SerializeACSScriptNumber(FArchive &arc, int &scriptnum, bool was2byte)
{
	arc << scriptnum;
	// If the script number is negative, then it's really a name.
	// So read/store the name after it.
	if (scriptnum < 0)
	{
		if (arc.IsStoring())
		{
			arc.WriteName(FName(ENamedName(-scriptnum)).GetChars());
		}
		else
		{
			const char *nam = arc.ReadName();
			scriptnum = -FName(nam);
		}
	}
}


#if 0
// still needed as reference.
FArchive &FArchive::ReadObject(DObject* &obj, PClass *wanttype)
{
	BYTE objHead;
	const PClass *type;
	BYTE playerNum;
	DWORD index;
	DObject *newobj;

	operator<< (objHead);

	switch (objHead)
	{
	case NULL_OBJ:
		obj = NULL;
		break;

	case M1_OBJ:
		obj = (DObject *)~0;
		break;

	case OLD_OBJ:
		index = ReadCount();
		if (index >= ArchiveToObject.Size())
		{
			I_Error("Object reference too high (%u; max is %u)\n", index, ArchiveToObject.Size());
		}
		obj = ArchiveToObject[index];
		break;

	case NEW_PLYR_CLS_OBJ:
		operator<< (playerNum);
		if (m_HubTravel)
		{
			// If travelling inside a hub, use the existing player actor
			type = ReadClass(wanttype);
			//			Printf ("New player class: %s (%u)\n", type->Name, m_File->Tell());
			obj = players[playerNum].mo;

			// But also create a new one so that we can get past the one
			// stored in the archive.
			AActor *tempobj = static_cast<AActor *>(type->CreateNew());
			MapObject(obj != NULL ? obj : tempobj);
			tempobj->SerializeUserVars(*this);
			tempobj->Serialize(*this);
			tempobj->CheckIfSerialized();
			// If this player is not present anymore, keep the new body
			// around just so that the load will succeed.
			if (obj != NULL)
			{
				// When the temporary player's inventory items were loaded,
				// they became owned by the real player. Undo that now.
				for (AInventory *item = tempobj->Inventory; item != NULL; item = item->Inventory)
				{
					item->Owner = tempobj;
				}
				tempobj->Destroy();
			}
			else
			{
				obj = tempobj;
				players[playerNum].mo = static_cast<APlayerPawn *>(obj);
			}
			break;
		}
		/* fallthrough when not travelling to a previous level */
	case NEW_CLS_OBJ:
		type = ReadClass(wanttype);
		//		Printf ("New class: %s (%u)\n", type->Name, m_File->Tell());
		newobj = obj = type->CreateNew();
		MapObject(obj);
		newobj->SerializeUserVars(*this);
		newobj->Serialize(*this);
		newobj->CheckIfSerialized();
		break;

	case NEW_PLYR_OBJ:
		operator<< (playerNum);
		if (m_HubTravel)
		{
			type = ReadStoredClass(wanttype);
			//			Printf ("Use player class: %s (%u)\n", type->Name, m_File->Tell());
			obj = players[playerNum].mo;

			AActor *tempobj = static_cast<AActor *>(type->CreateNew());
			MapObject(obj != NULL ? obj : tempobj);
			tempobj->SerializeUserVars(*this);
			tempobj->Serialize(*this);
			tempobj->CheckIfSerialized();
			if (obj != NULL)
			{
				for (AInventory *item = tempobj->Inventory;
					item != NULL; item = item->Inventory)
				{
					item->Owner = tempobj;
				}
				tempobj->Destroy();
			}
			else
			{
				obj = tempobj;
				players[playerNum].mo = static_cast<APlayerPawn *>(obj);
			}
			break;
		}
		/* fallthrough when not travelling to a previous level */
	case NEW_OBJ:
		type = ReadStoredClass(wanttype);
		//		Printf ("Use class: %s (%u)\n", type->Name, m_File->Tell());
		obj = type->CreateNew();
		MapObject(obj);
		obj->SerializeUserVars(*this);
		obj->Serialize(*this);
		obj->CheckIfSerialized();
		break;

	default:
		I_Error("Unknown object code (%d) in archive\n", objHead);
	}
	return *this;
}
#endif


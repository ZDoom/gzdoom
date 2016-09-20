
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


FArchive &FArchive::WriteObject (DObject *obj)
{
#if 0
	player_t *player;
	BYTE id[2];

	if (obj == NULL)
	{
		id[0] = NULL_OBJ;
		Write (id, 1);
	}
	else if (obj == (DObject*)~0)
	{
		id[0] = M1_OBJ;
		Write (id, 1);
	}
	else if (obj->ObjectFlags & OF_EuthanizeMe)
	{
		// Objects that want to die are not saved to the archive, but
		// we leave the pointers to them alone.
		id[0] = NULL_OBJ;
		Write (id, 1);
	}
	else
	{
		PClass *type = obj->GetClass();
		DWORD *classarcid;

		if (type == RUNTIME_CLASS(DObject))
		{
			//I_Error ("Tried to save an instance of DObject.\n"
			//		 "This should not happen.\n");
			id[0] = NULL_OBJ;
			Write (id, 1);
		}
		else if (NULL == (classarcid = ClassToArchive.CheckKey(type)))
		{
			// No instances of this class have been written out yet.
			// Write out the class, then write out the object. If this
			// is an actor controlled by a player, make note of that
			// so that it can be overridden when moving around in a hub.
			if (obj->IsKindOf (RUNTIME_CLASS (AActor)) &&
				(player = static_cast<AActor *>(obj)->player) &&
				player->mo == obj)
			{
				id[0] = NEW_PLYR_CLS_OBJ;
				id[1] = (BYTE)(player - players);
				Write (id, 2);
			}
			else
			{
				id[0] = NEW_CLS_OBJ;
				Write (id, 1);
			}
			WriteClass (type);
//			Printf ("Make class %s (%u)\n", type->Name, m_File->Tell());
			MapObject (obj);
			obj->SerializeUserVars (*this);
			obj->Serialize (*this);
			obj->CheckIfSerialized ();
		}
		else
		{
			// An instance of this class has already been saved. If
			// this object has already been written, save a reference
			// to the saved object. Otherwise, save a reference to the
			// class, then save the object. Again, if this is a player-
			// controlled actor, remember that.
			DWORD *objarcid = ObjectToArchive.CheckKey(obj);

			if (objarcid == NULL)
			{

				if (obj->IsKindOf (RUNTIME_CLASS (AActor)) &&
					(player = static_cast<AActor *>(obj)->player) &&
					player->mo == obj)
				{
					id[0] = NEW_PLYR_OBJ;
					id[1] = (BYTE)(player - players);
					Write (id, 2);
				}
				else
				{
					id[0] = NEW_OBJ;
					Write (id, 1);
				}
				WriteCount (*classarcid);
//				Printf ("Reuse class %s (%u)\n", type->Name, m_File->Tell());
				MapObject (obj);
				obj->SerializeUserVars (*this);
				obj->Serialize (*this);
				obj->CheckIfSerialized ();
			}
			else
			{
				id[0] = OLD_OBJ;
				Write (id, 1);
				WriteCount (*objarcid);
			}
		}
	}
#endif
	return *this;
}


FArchive &operator<< (FArchive &arc, sector_t *&sec)
{
	return arc.SerializePointer (sectors, (BYTE **)&sec, sizeof(*sectors));
}

FArchive &operator<< (FArchive &arc, const sector_t *&sec)
{
	return arc.SerializePointer (sectors, (BYTE **)&sec, sizeof(*sectors));
}

FArchive &operator<< (FArchive &arc, line_t *&line)
{
	return arc.SerializePointer (lines, (BYTE **)&line, sizeof(*lines));
}

FArchive &operator<< (FArchive &arc, vertex_t *&vert)
{
	return arc.SerializePointer (vertexes, (BYTE **)&vert, sizeof(*vertexes));
}

FArchive &operator<< (FArchive &arc, side_t *&side)
{
	return arc.SerializePointer (sides, (BYTE **)&side, sizeof(*sides));
}

FArchive &operator<<(FArchive &arc, DAngle &ang)
{
	arc << ang.Degrees;
	return arc;
}

FArchive &operator<<(FArchive &arc, DVector3 &vec)
{
	arc << vec.X << vec.Y << vec.Z;
	return arc;
}

FArchive &operator<<(FArchive &arc, DVector2 &vec)
{
	arc << vec.X << vec.Y;
	return arc;
}

//==========================================================================
//
//
//==========================================================================

FArchive &operator<< (FArchive &arc, FState *&state)
{
	PClassActor *info;

	if (arc.IsStoring ())
	{
		if (state == NULL)
		{
			arc.UserWriteClass (RUNTIME_CLASS(AActor));
			arc.WriteCount (NULL_STATE_INDEX);
			return arc;
		}

		info = FState::StaticFindStateOwner (state);

		if (info != NULL)
		{
			arc.UserWriteClass (info);
			arc.WriteCount ((DWORD)(state - info->OwnedStates));
		}
		else
		{
			/* this was never working as intended.
			I_Error ("Cannot find owner for state %p:\n"
					 "%s %c%c %3d [%p] -> %p", state,
					 sprites[state->sprite].name,
					 state->GetFrame() + 'A',
					 state->GetFullbright() ? '*' : ' ',
					 state->GetTics(),
					 state->GetAction(),
					 state->GetNextState());
			*/
		}
	}
	else
	{
		PClassActor *info;
		DWORD ofs;

		arc.UserReadClass<PClassActor>(info);
		ofs = arc.ReadCount ();
		if (ofs == NULL_STATE_INDEX && info == RUNTIME_CLASS(AActor))
		{
			state = NULL;
		}
		else
		{
			state = info->OwnedStates + ofs;
		}
	}
	return arc;
}

FArchive &operator<< (FArchive &arc, secplane_t &plane)
{
	arc << plane.normal << plane.D;
	if (plane.normal.Z != 0)
	{	// plane.c should always be non-0. Otherwise, the plane
		// would be perfectly vertical. (But then, don't let this crash on a broken savegame...)
		plane.negiC = -1 / plane.normal.Z;
	}
	return arc;
}

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

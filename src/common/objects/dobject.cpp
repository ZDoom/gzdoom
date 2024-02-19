/*
** dobject.cpp
** Implements the base class DObject, which most other classes derive from
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#include <stdlib.h>

#include "dobject.h"
#include "cmdlib.h"
#include "c_dispatch.h"
#include "serializer.h"
#include "vm.h"
#include "types.h"
#include "i_time.h"
#include "printf.h"
#include "maps.h"

//==========================================================================
//
//
//
//==========================================================================

ClassReg DObject::RegistrationInfo =
{
	nullptr,								// MyClass
	"DObject",								// Name
	nullptr,								// ParentType
	nullptr,								
	nullptr,								// Pointers
	&DObject::InPlaceConstructor,			// ConstructNative
	nullptr,
	sizeof(DObject),						// SizeOf
};
_DECLARE_TI(DObject)

// This bit is needed in the playsim - but give it a less crappy name.
DEFINE_FIELD_BIT(DObject,ObjectFlags, bDestroyed, OF_EuthanizeMe)


//==========================================================================
//
//
//
//==========================================================================

CCMD (dumpclasses)
{
	// This is by no means speed-optimized. But it's an informational console
	// command that will be executed infrequently, so I don't mind.
	struct DumpInfo
	{
		const PClass *Type;
		DumpInfo *Next;
		DumpInfo *Children;

		static DumpInfo *FindType (DumpInfo *root, const PClass *type)
		{
			if (root == NULL)
			{
				return root;
			}
			if (root->Type == type)
			{
				return root;
			}
			if (root->Next != NULL)
			{
				return FindType (root->Next, type);
			}
			if (root->Children != NULL)
			{
				return FindType (root->Children, type);
			}
			return NULL;
		}

		static DumpInfo *AddType (DumpInfo **root, const PClass *type)
		{
			DumpInfo *info, *parentInfo;

			if (*root == NULL)
			{
				info = new DumpInfo;
				info->Type = type;
				info->Next = NULL;
				info->Children = *root;
				*root = info;
				return info;
			}
			if (type->ParentClass == (*root)->Type)
			{
				parentInfo = *root;
			}
			else if (type == (*root)->Type)
			{
				return *root;
			}
			else
			{
				parentInfo = FindType (*root, type->ParentClass);
				if (parentInfo == NULL)
				{
					parentInfo = AddType (root, type->ParentClass);
				}
			}
			// Has this type already been added?
			for (info = parentInfo->Children; info != NULL; info = info->Next)
			{
				if (info->Type == type)
				{
					return info;
				}
			}
			info = new DumpInfo;
			info->Type = type;
			info->Next = parentInfo->Children;
			info->Children = NULL;
			parentInfo->Children = info;
			return info;
		}

		static void PrintTree (DumpInfo *root, int level)
		{
			Printf ("%*c%s\n", level, ' ', root->Type->TypeName.GetChars());
			if (root->Children != NULL)
			{
				PrintTree (root->Children, level + 2);
			}
			if (root->Next != NULL)
			{
				PrintTree (root->Next, level);
			}
		}

		static void FreeTree (DumpInfo *root)
		{
			if (root->Children != NULL)
			{
				FreeTree (root->Children);
			}
			if (root->Next != NULL)
			{
				FreeTree (root->Next);
			}
			delete root;
		}
	};

	unsigned int i;
	int shown, omitted;
	DumpInfo *tree = NULL;
	const PClass *root = NULL;

	if (argv.argc() > 1)
	{
		root = PClass::FindClass (argv[1]);
		if (root == NULL)
		{
			Printf ("Class '%s' not found\n", argv[1]);
			return;
		}
	}

	shown = omitted = 0;
	DumpInfo::AddType (&tree, root != NULL ? root : RUNTIME_CLASS(DObject));
	for (i = 0; i < PClass::AllClasses.Size(); i++)
	{
		PClass *cls = PClass::AllClasses[i];
		if (root == NULL || cls == root || cls->IsDescendantOf(root))
		{
			DumpInfo::AddType (&tree, cls);
//			Printf (" %s\n", PClass::m_Types[i]->Name + 1);
			shown++;
		}
		else
		{
			omitted++;
		}
	}
	DumpInfo::PrintTree (tree, 2);
	DumpInfo::FreeTree (tree);
	Printf ("%d classes shown, %d omitted\n", shown, omitted);
}

//==========================================================================
//
//
//
//==========================================================================

#include "d_net.h"

void DObject::InPlaceConstructor (void *mem)
{
	new ((EInPlace *)mem) DObject;
}

DObject::DObject ()
: Class(0), ObjectFlags(0)
{
	ObjectFlags = GC::CurrentWhite & OF_WhiteBits;
	ObjNext = GC::Root;
	GCNext = nullptr;
	GC::Root = this;
}

DObject::DObject (PClass *inClass)
: Class(inClass), ObjectFlags(0)
{
	ObjectFlags = GC::CurrentWhite & OF_WhiteBits;
	ObjNext = GC::Root;
	GCNext = nullptr;
	GC::Root = this;
}

//==========================================================================
//
//
//
//==========================================================================

DObject::~DObject ()
{
	if (!PClass::bShutdown)
	{
		PClass *type = GetClass();
		if (!(ObjectFlags & OF_Cleanup) && !PClass::bShutdown)
		{
			if (!(ObjectFlags & (OF_YesReallyDelete|OF_Released)))
			{
				Printf("Warning: '%s' is freed outside the GC process.\n",
					type != NULL ? type->TypeName.GetChars() : "==some object==");
			}

			if (!(ObjectFlags & OF_Released))
			{
				// Find all pointers that reference this object and NULL them.
				Release();
			}
		}

		if (nullptr != type)
		{
			type->DestroySpecials(this);
		}
	}
}

void DObject::Release()
{
	DObject **probe;

	// Unlink this object from the GC list.
	for (probe = &GC::Root; *probe != NULL; probe = &((*probe)->ObjNext))
	{
		if (*probe == this)
		{
			*probe = ObjNext;
			if (&ObjNext == GC::SweepPos)
			{
				GC::SweepPos = probe;
			}
			break;
		}
	}

	// If it's gray, also unlink it from the gray list.
	if (this->IsGray())
	{
		for (probe = &GC::Gray; *probe != NULL; probe = &((*probe)->GCNext))
		{
			if (*probe == this)
			{
				*probe = GCNext;
				break;
			}
		}
	}
	ObjNext = nullptr;
	GCNext = nullptr;
	ObjectFlags |= OF_Released;
}

//==========================================================================
//
//
//
//==========================================================================

void DObject::Destroy ()
{
	NetworkEntityManager::RemoveNetworkEntity(this);

	// We cannot call the VM during shutdown because all the needed data has been or is in the process of being deleted.
	if (PClass::bVMOperational)
	{
		IFVIRTUAL(DObject, OnDestroy)
		{
			VMValue params[1] = { (DObject*)this };
			VMCall(func, params, 1, nullptr, 0);
		}
	}
	OnDestroy();
	ObjectFlags = (ObjectFlags & ~OF_Fixed) | OF_EuthanizeMe;
}

DEFINE_ACTION_FUNCTION(DObject, Destroy)
{
	PARAM_SELF_PROLOGUE(DObject);
	self->Destroy();
	return 0;	
}

//==========================================================================
//
//
//
//==========================================================================

template<typename M>
static void PropagateMarkMap(M *map)
{
	TMapIterator<typename M::KeyType,DObject*> it(*map);
	typename M::Pair * p;
	while(it.NextPair(p))
	{
		GC::Mark(&p->Value);
	}
}

size_t DObject::PropagateMark()
{
	const PClass *info = GetClass();
	if (!PClass::bShutdown)
	{
		const size_t *offsets = info->FlatPointers;
		if (offsets == NULL)
		{
			const_cast<PClass *>(info)->BuildFlatPointers();
			offsets = info->FlatPointers;
		}
		while (*offsets != ~(size_t)0)
		{
			GC::Mark((DObject **)((uint8_t *)this + *offsets));
			offsets++;
		}

		offsets = info->ArrayPointers;
		if (offsets == NULL)
		{
			const_cast<PClass *>(info)->BuildArrayPointers();
			offsets = info->ArrayPointers;
		}
		while (*offsets != ~(size_t)0)
		{
			auto aray = (TArray<DObject*>*)((uint8_t *)this + *offsets);
			for (auto &p : *aray)
			{
				GC::Mark(&p);
			}
			offsets++;
		}

		{
			const std::pair<size_t,PType *> *maps = info->MapPointers;
			if (maps == NULL)
			{
				const_cast<PClass *>(info)->BuildMapPointers();
				maps = info->MapPointers;
			}
			while (maps->first != ~(size_t)0)
			{
				if(maps->second->RegType == REGT_STRING)
				{ // FString,DObject*
					PropagateMarkMap((ZSMap<FString,DObject*>*)((uint8_t *)this + maps->first));
				}
				else
				{ // uint32_t,DObject*
					PropagateMarkMap((ZSMap<uint32_t,DObject*>*)((uint8_t *)this + maps->first));
				}
				maps++;
			}

		}
		return info->Size;
	}
	return 0;
}

//==========================================================================
//
//
//
//==========================================================================

size_t DObject::PointerSubstitution (DObject *old, DObject *notOld)
{
	const PClass *info = GetClass();
	const size_t *offsets = info->FlatPointers;
	size_t changed = 0;
	if (offsets == NULL)
	{
		const_cast<PClass *>(info)->BuildFlatPointers();
		offsets = info->FlatPointers;
	}
	while (*offsets != ~(size_t)0)
	{
		if (*(DObject **)((uint8_t *)this + *offsets) == old)
		{
			*(DObject **)((uint8_t *)this + *offsets) = notOld;
			changed++;
		}
		offsets++;
	}

	offsets = info->ArrayPointers;
	if (offsets == NULL)
	{
		const_cast<PClass *>(info)->BuildArrayPointers();
		offsets = info->ArrayPointers;
	}
	while (*offsets != ~(size_t)0)
	{
		auto aray = (TArray<DObject*>*)((uint8_t *)this + *offsets);
		for (auto &p : *aray)
		{
			if (p == old)
			{
				p = notOld;
				changed++;
			}
		}
		offsets++;
	}


	return changed;
}

//==========================================================================
//
//
//
//==========================================================================

void DObject::SerializeUserVars(FSerializer &arc)
{
	if (arc.isWriting())
	{
		// Write all fields that aren't serialized by native code.
		GetClass()->WriteAllFields(arc, this);
	}
	else
	{
		GetClass()->ReadAllFields(arc, this);
	}
}


//==========================================================================
//
//
//
//==========================================================================

void DObject::Serialize(FSerializer &arc)
{
	const auto SerializeFlag = [&](const char *const name, const EObjectFlags flag)
	{
		int value = ObjectFlags & flag;
		int defaultvalue = 0;
		arc(name, value, defaultvalue);
		if (arc.isReading())
		{
			ObjectFlags |= value;
		}
	};

	SerializeFlag("justspawned", OF_JustSpawned);
	SerializeFlag("spawned", OF_Spawned);
	SerializeFlag("networked", OF_Networked);
		
	ObjectFlags |= OF_SerialSuccess;

	if (arc.isReading() && (ObjectFlags & OF_Networked))
	{
		ClearNetworkID();
		EnableNetworking(true);
	}
}

void DObject::CheckIfSerialized () const
{
	if (!(ObjectFlags & OF_SerialSuccess))
	{
		I_Error (
			"BUG: %s::Serialize\n"
			"(or one of its superclasses) needs to call\n"
			"Super::Serialize\n",
			StaticType()->TypeName.GetChars());
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DObject::SetNetworkID(const uint32_t id)
{
	if (!IsNetworked())
	{
		ObjectFlags |= OF_Networked;
		_networkID = id;
	}
}

void DObject::ClearNetworkID()
{
	ObjectFlags &= ~OF_Networked;
	_networkID = NetworkEntityManager::WorldNetID;
}

void DObject::EnableNetworking(const bool enable)
{
	if (enable)
		NetworkEntityManager::AddNetworkEntity(this);
	else
		NetworkEntityManager::RemoveNetworkEntity(this);
}

static unsigned int GetNetworkID(DObject* const self)
{
	return self->GetNetworkID();
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, GetNetworkID, GetNetworkID)
{
	PARAM_SELF_PROLOGUE(DObject);

	ACTION_RETURN_INT(self->GetNetworkID());
}

static void EnableNetworking(DObject* const self, const bool enable)
{
	self->EnableNetworking(enable);
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, EnableNetworking, EnableNetworking)
{
	PARAM_SELF_PROLOGUE(DObject);
	PARAM_BOOL(enable);

	self->EnableNetworking(enable);
	return 0;
}

static DObject* GetNetworkEntity(const unsigned int id)
{
	return NetworkEntityManager::GetNetworkEntity(id);
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, GetNetworkEntity, GetNetworkEntity)
{
	PARAM_PROLOGUE;
	PARAM_UINT(id);

	ACTION_RETURN_OBJECT(NetworkEntityManager::GetNetworkEntity(id));
}

DEFINE_ACTION_FUNCTION(DObject, MSTime)
{
	ACTION_RETURN_INT((uint32_t)I_msTime());
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, MSTimef, I_msTimeF)
{
	ACTION_RETURN_FLOAT(I_msTimeF());
}

void *DObject::ScriptVar(FName field, PType *type)
{
	auto cls = GetClass();
	auto sym = dyn_cast<PField>(cls->FindSymbol(field, true));
	if (sym && (sym->Type == type || type == nullptr))
	{
		if (!(sym->Flags & VARF_Meta))
		{
			return (((char*)this) + sym->Offset);
		}
		else
		{
			return (cls->Meta + sym->Offset);
		}
	}
	// This is only for internal use so I_Error is fine.
	I_Error("Variable %s not found in %s\n", field.GetChars(), cls->TypeName.GetChars());
}

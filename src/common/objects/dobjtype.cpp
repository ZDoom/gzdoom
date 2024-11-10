/*
** dobjtype.cpp
** Implements the type information class
**
**---------------------------------------------------------------------------
** Copyright 1998-2016 Randy Heit
** Copyright 2005-2016 Christoph Oelckers
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

// HEADER FILES ------------------------------------------------------------

#include <limits>

#include "dobject.h"
#include "serializer.h"
#include "autosegs.h"
#include "v_text.h"
#include "c_cvars.h"
#include "vm.h"
#include "symbols.h"
#include "types.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------
bool AreCompatiblePointerTypes(PType* dest, PType* source, bool forcompare = false);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------
EXTERN_CVAR(Bool, strictdecorate);

// PUBLIC DATA DEFINITIONS -------------------------------------------------
FMemArena ClassDataAllocator(32768);	// use this for all static class data that can be released in bulk when the type system is shut down.

TArray<PClass *> PClass::AllClasses;
TMap<FName, PClass*> PClass::ClassMap;
TArray<VMFunction**> PClass::FunctionPtrList;
bool PClass::bShutdown;
bool PClass::bVMOperational;

// Originally this was just a bogus pointer, but with the VM performing a read barrier on every object pointer write
// that does not work anymore. WP_NOCHANGE needs to point to a vaild object to work as intended.
// This Object does not need to be garbage collected, though, but it needs to provide the proper structure so that the
// GC can process it.
DObject *WP_NOCHANGE;
DEFINE_GLOBAL(WP_NOCHANGE);


// PRIVATE DATA DEFINITIONS ------------------------------------------------

// A harmless non-nullptr FlatPointer for classes without pointers.
static const std::pair<size_t,PType *> TheEnd = {~(size_t)0 , nullptr};

//==========================================================================
//
// PClass :: WriteValue
//
// Similar to PStruct's version, except it also needs to traverse parent
// classes.
//
//==========================================================================

static void RecurseWriteFields(const PClass *type, FSerializer &ar, const void *addr)
{
	if (type != nullptr)
	{
		RecurseWriteFields(type->ParentClass, ar, addr);
		// Don't write this part if it has no non-transient variables
		for (unsigned i = 0; i < type->Fields.Size(); ++i)
		{
			if (!(type->Fields[i]->Flags & (VARF_Transient|VARF_Meta)))
			{
				// Tag this section with the class it came from in case
				// a more-derived class has variables that shadow a less-
				// derived class. Whether or not that is a language feature
				// that will actually be allowed remains to be seen.
				FString key;
				key.Format("class:%s", type->TypeName.GetChars());
				if (ar.BeginObject(key.GetChars()))
				{
					type->VMType->Symbols.WriteFields(ar, addr);
					ar.EndObject();
				}
				break;
			}
		}
	}
}

// Same as WriteValue, but does not create a new object in the serializer
// This is so that user variables do not contain unnecessary subblocks.
void PClass::WriteAllFields(FSerializer &ar, const void *addr) const
{
	RecurseWriteFields(this, ar, addr);
}

//==========================================================================
//
// PClass :: ReadAllFields
//
//==========================================================================

bool PClass::ReadAllFields(FSerializer &ar, void *addr) const
{
	bool readsomething = false;
	bool foundsomething = false;
	const char *key;
	key = ar.GetKey();
	if (strcmp(key, "classtype"))
	{
		// this does not represent a DObject
		Printf(TEXTCOLOR_RED "trying to read user variables but got a non-object (first key is '%s')\n", key);
		ar.mErrors++;
		return false;
	}
	while ((key = ar.GetKey()))
	{
		if (strncmp(key, "class:", 6))
		{
			// We have read all user variable blocks.
			break;
		}
		foundsomething = true;
		PClass *type = PClass::FindClass(key + 6);
		if (type != nullptr)
		{
			// Only read it if the type is related to this one.
			if (IsDescendantOf(type))
			{
				if (ar.BeginObject(nullptr))
				{
					readsomething |= type->VMType->Symbols.ReadFields(ar, addr, type->TypeName.GetChars());
					ar.EndObject();
				}
			}
			else
			{
				DPrintf(DMSG_ERROR, "Unknown superclass %s of class %s\n",
					type->TypeName.GetChars(), TypeName.GetChars());
			}
		}
		else
		{
			DPrintf(DMSG_ERROR, "Unknown superclass %s of class %s\n",
				key+6, TypeName.GetChars());
		}
	}
	return readsomething || !foundsomething;
}

//==========================================================================
//
// cregcmp
//
// Sorter to keep built-in types in a deterministic order. (Needed?)
//
//==========================================================================

static NO_SANITIZE_M int cregcmp (const void *a, const void *b) NO_SANITIZE
{
	const PClass *class1 = *(const PClass **)a;
	const PClass *class2 = *(const PClass **)b;
	return strcmp(class1->TypeName.GetChars(), class2->TypeName.GetChars());
}

//==========================================================================
//
// PClass :: StaticInit												STATIC
//
// Creates class metadata for all built-in types.
//
//==========================================================================

void PClass::StaticInit ()
{
	Namespaces.GlobalNamespace = Namespaces.NewNamespace(0);

	AutoSegs::TypeInfos.ForEach([](ClassReg* typeInfo)
	{
		typeInfo->RegisterClass();
	});

	// Keep built-in classes in consistant order. I did this before, though
	// I'm not sure if this is really necessary to maintain any sort of sync.
	qsort(&AllClasses[0], AllClasses.Size(), sizeof(AllClasses[0]), cregcmp);

	// WP_NOCHANGE must point to a valid object, although it does not need to be a weapon.
	// A simple DObject is enough to give the GC the ability to deal with it, if subjected to it.
	WP_NOCHANGE = Create<DObject>();
	WP_NOCHANGE->Release();
}

//==========================================================================
//
// PClass :: StaticShutdown											STATIC
//
// Frees all static class data.
//
//==========================================================================

void ClearServices();

void PClass::StaticShutdown ()
{
	if (WP_NOCHANGE != nullptr)
	{
		delete WP_NOCHANGE;
	}

	ClearServices();
	// delete all variables containing pointers to script functions.
	for (auto p : FunctionPtrList)
	{
		*p = nullptr;
	}
	FunctionPtrList.Clear();
	VMFunction::DeleteAll();
	// From this point onward no scripts may be called anymore because the data needed by the VM is getting deleted now.
	// This flags DObject::Destroy not to call any scripted OnDestroy methods anymore.
	bVMOperational = false;

	// Make a full garbage collection here so that all destroyed but uncollected higher level objects 
	// that still exist are properly taken down before the low level data is deleted.
	GC::FullGC();
	GC::FullGC();


	Namespaces.ReleaseSymbols();

	// This must be done in two steps because the native classes are not ordered by inheritance,
	// so all meta data must be gone before deleting the actual class objects.
	for (auto cls : AllClasses)	if (cls->Meta != nullptr) cls->DestroyMeta(cls->Meta);
	for (auto cls : AllClasses)	delete cls;
	// Unless something went wrong, anything left here should be class and type objects only, which do not own any scripts.
	bShutdown = true;
	TypeTable.Clear();
	ClassDataAllocator.FreeAllBlocks();
	AllClasses.Clear();
	ClassMap.Clear();

	AutoSegs::TypeInfos.ForEach([](ClassReg* typeInfo)
	{
		typeInfo->MyClass = nullptr;
	});
}

//==========================================================================
//
// PClass Constructor
//
//==========================================================================

PClass::PClass()
{
	PClass::AllClasses.Push(this);
}

//==========================================================================
//
// PClass Destructor
//
//==========================================================================

PClass::~PClass()
{
	if (Defaults != nullptr)
	{
		M_Free(Defaults);
		Defaults = nullptr;
	}
	if (Meta != nullptr)
	{
		M_Free(Meta);
		Meta = nullptr;
	}
}

//==========================================================================
//
// ClassReg :: RegisterClass
//
// Create metadata describing the built-in class this struct is intended
// for.
//
//==========================================================================

PClass *ClassReg::RegisterClass()
{
	// Skip classes that have already been registered
	if (MyClass != nullptr)
	{
		return MyClass;
	}

	// Add type to list
	PClass *cls = new PClass;

	SetupClass(cls);
	cls->InsertIntoHash(true);
	if (ParentType != nullptr)
	{
		cls->ParentClass = ParentType->RegisterClass();
	}
	return cls;
}

//==========================================================================
//
// ClassReg :: SetupClass
//
// Copies the class-defining parameters from a ClassReg to the Class object
// created for it.
//
//==========================================================================

void ClassReg::SetupClass(PClass *cls)
{
	assert(MyClass == nullptr);
	MyClass = cls;
	cls->TypeName = FName(Name+1);
	cls->Size = SizeOf;
	cls->Pointers = Pointers;
	cls->ConstructNative = ConstructNative;
}

//==========================================================================
//
// PClass :: InsertIntoHash
//
// Add class to the type table.
//
//==========================================================================

void PClass::InsertIntoHash (bool native)
{
	auto k = ClassMap.CheckKey(TypeName);
	if (k != nullptr)
	{ // This type has already been inserted
		I_Error("Tried to register class '%s' more than once.\n", TypeName.GetChars());
	}
	else
	{
		ClassMap[TypeName] = this;
	}
}

//==========================================================================
//
// PClass :: FindParentClass
//
// Finds a parent class that matches the given name, including itself.
//
//==========================================================================

const PClass *PClass::FindParentClass(FName name) const
{
	for (const PClass *type = this; type != nullptr; type = type->ParentClass)
	{
		if (type->TypeName == name)
		{
			return type;
		}
	}
	return nullptr;
}

//==========================================================================
//
// PClass :: FindClass
//
// Find a type, passed the name as a name.
//
//==========================================================================

PClass *PClass::FindClass (FName zaname)
{
	if (zaname == NAME_None)
	{
		return nullptr;
	}
	auto k = ClassMap.CheckKey(zaname);
	return k ? *k : nullptr;
}

//==========================================================================
//
// PClass :: CreateNew
//
// Create a new object that this class represents
//
//==========================================================================

DObject *PClass::CreateNew()
{
	uint8_t *mem = (uint8_t *)M_Malloc (Size);
	assert (mem != nullptr);

	// Set this object's defaults before constructing it.
	if (Defaults != nullptr)
		memcpy (mem, Defaults, Size);
	else
		memset (mem, 0, Size);

	if (ConstructNative == nullptr || bAbstract)
	{
		M_Free(mem);
		I_Error("Attempt to instantiate abstract class %s.", TypeName.GetChars());
	}
	ConstructNative (mem);

	if (Defaults != nullptr)
		((DObject *)mem)->ObjectFlags |= ((DObject *)Defaults)->ObjectFlags & (OF_Transient | OF_ClientSide);

	((DObject *)mem)->SetClass (const_cast<PClass *>(this));
	InitializeSpecials(mem, Defaults, &PClass::SpecialInits);
	return (DObject *)mem;
}

//==========================================================================
//
// PClass :: InitializeSpecials
//
// Initialize special fields (e.g. strings) of a newly-created instance.
//
//==========================================================================

void PClass::InitializeSpecials(void *addr, void *defaults, TArray<FTypeAndOffset> PClass::*Inits)
{
	// Once we reach a native class, we can stop going up the family tree,
	// since native classes handle initialization natively.
	if ((!bRuntimeClass && Inits == &PClass::SpecialInits) || ParentClass == nullptr)
	{
		return;
	}
	ParentClass->InitializeSpecials(addr, defaults, Inits);
	for (auto tao : (this->*Inits))
	{
		tao.first->InitializeValue((char*)addr + tao.second, defaults == nullptr? nullptr : ((char*)defaults) + tao.second);
	}
}

//==========================================================================
//
// PClass :: DestroySpecials
//
// Destroy special fields (e.g. strings) of an instance that is about to be
// deleted.
//
//==========================================================================

void PClass::DestroySpecials(void *addr)
{
	if (!bRuntimeClass)
	{
		return;
	}
	assert(ParentClass != nullptr);
	ParentClass->DestroySpecials(addr);
	for (auto tao : SpecialInits)
	{
		tao.first->DestroyValue((uint8_t *)addr + tao.second);
	}
}

//==========================================================================
//
// PClass :: DestroyMeta
//
// Same for meta data
//
//==========================================================================

void PClass::DestroyMeta(void *addr)
{
	if (ParentClass != nullptr) ParentClass->DestroyMeta(addr);
	for (auto tao : MetaInits)
	{
		tao.first->DestroyValue((uint8_t *)addr + tao.second);
	}
}

//==========================================================================
//
// PClass :: Derive
//
// Copies inheritable values into the derived class and other miscellaneous setup.
//
//==========================================================================

void PClass::Derive(PClass *newclass, FName name)
{
	newclass->bRuntimeClass = true;
	newclass->ParentClass = this;
	newclass->ConstructNative = ConstructNative;
	newclass->TypeName = name;
	newclass->MetaSize = MetaSize;

}

//==========================================================================
//
// PClass :: CreateDerivedClass
//
// Create a new class based on an existing class
//
//==========================================================================

PClass *PClass::CreateDerivedClass(FName name, unsigned int size, bool *newlycreated, int fileno)
{
	assert(size >= Size);
	PClass *type;
	bool notnew;

	const PClass *existclass = FindClass(name);

	if (newlycreated) *newlycreated = false;
	if (existclass != nullptr)
	{
		// This is a placeholder so fill it in
		if (existclass->Size == TentativeClass)
		{
			type = const_cast<PClass*>(existclass);
			if (!IsDescendantOf(type->ParentClass))
			{
				I_Error("%s must inherit from %s but doesn't.", name.GetChars(), type->ParentClass->TypeName.GetChars());
			}
			DPrintf(DMSG_SPAMMY, "Defining placeholder class %s\n", name.GetChars());
			notnew = true;
		}
		else
		{
			// a different class with the same name already exists. Let the calling code deal with this.
			return nullptr;
		}
	}
	else
	{
		type = new PClass;
		notnew = false;
	}

	type->TypeName = name;
	type->bRuntimeClass = true;
	Derive(type, name);
	type->Size = size;
	if (size != TentativeClass)
	{
		NewClassType(type, fileno);
		if (newlycreated) *newlycreated = true;
		type->Virtuals = Virtuals;
	}
	else
		type->bOptional = false;

	if (!notnew)
	{
		type->InsertIntoHash(false);
	}
	return type;
}

//==========================================================================
//
// PClass :: AddField
//
//==========================================================================

PField *PClass::AddField(FName name, PType *type, uint32_t flags, int fileno)
{
	PField *field;
	if (!(flags & VARF_Meta))
	{
		unsigned oldsize = Size;
		field = VMType->Symbols.AddField(name, type, flags, Size, nullptr, fileno);

		// Only initialize the defaults if they have already been created.
		// For ZScript this is not the case, it will first define all fields before
		// setting up any defaults for any class.
		if (field != nullptr && !(flags & VARF_Native) && Defaults != nullptr)
		{
			Defaults = (uint8_t *)M_Realloc(Defaults, Size);
			memset(Defaults + oldsize, 0, Size - oldsize);
		}
	}
	else
	{
		// Same as above, but a different data storage.
		unsigned oldsize = MetaSize;
		field = VMType->Symbols.AddField(name, type, flags, MetaSize, nullptr, fileno);

		if (field != nullptr && !(flags & VARF_Native) && Meta != nullptr)
		{
			Meta = (uint8_t *)M_Realloc(Meta, MetaSize);
			memset(Meta + oldsize, 0, MetaSize - oldsize);
		}
	}
	if (field != nullptr) Fields.Push(field);
	return field;
}

//==========================================================================
//
// PClass :: FindClassTentative
//
// Like FindClass but creates a placeholder if no class is found.
// This will be filled in when the actual class is constructed.
//
//==========================================================================

PClass *PClass::FindClassTentative(FName name)
{
	if (name == NAME_None)
	{
		return nullptr;
	}

	PClass *found = FindClass(name);
	if (found != nullptr) return found;

	PClass *type = new PClass;
	DPrintf(DMSG_SPAMMY, "Creating placeholder class %s : %s\n", name.GetChars(), TypeName.GetChars());

	Derive(type, name);
	type->Size = TentativeClass;

	type->InsertIntoHash(false);
	return type;
}

//==========================================================================
//
// PClass :: FindVirtualIndex
//
// Compares a prototype with the existing list of virtual functions
// and returns an index if something matching is found.
//
//==========================================================================

bool ShouldAllowGameSpecificVirtual(FName name, unsigned index, PType* arg, PType* varg);

int PClass::FindVirtualIndex(FName name, PFunction::Variant *variant, PFunction *parentfunc, bool exactReturnType, bool ignorePointerReadOnly)
{
	auto proto = variant->Proto;
	auto &flags = variant->ArgFlags;
	for (unsigned i = 0; i < Virtuals.Size(); i++)
	{
		if (Virtuals[i]->Name == name)
		{
			auto vproto = Virtuals[i]->Proto;
			auto &vflags = Virtuals[i]->ArgFlags;

			int n = flags.size();

			bool flagsOk = true;

			for(int i = 0; i < n; i++)
			{
				int argA = i >= vflags.size() ? 0 : vflags[i];
				int argB = i >= flags.size() ? 0 : flags[i];

				bool AisRef = argA & (VARF_Out | VARF_Ref);
				bool BisRef = argB & (VARF_Out | VARF_Ref);

				if(AisRef != BisRef)
				{
					flagsOk = false;
					break;
				}
			}

			if (!flagsOk || vproto->ReturnTypes.Size() != proto->ReturnTypes.Size() ||
				vproto->ArgumentTypes.Size() < proto->ArgumentTypes.Size())
			{
				continue;	// number of parameters does not match, so it's incompatible
			}
			bool fail = false;
			// The first argument is self and will mismatch so just skip it.
			for (unsigned a = 1; a < proto->ArgumentTypes.Size(); a++)
			{
				if (proto->ArgumentTypes[a] != vproto->ArgumentTypes[a])
				{
					if(ignorePointerReadOnly && proto->ArgumentTypes[a]->isPointer() && vproto->ArgumentTypes[a]->isPointer())
					{
						PPointer *ppa = proto->ArgumentTypes[a]->toPointer();
						PPointer *ppb = vproto->ArgumentTypes[a]->toPointer();

						if(ppa->PointedType != ppb->PointedType)
						{
							fail = true;
							break;
						}
					}
					else if(!ShouldAllowGameSpecificVirtual(name, a, proto->ArgumentTypes[a], vproto->ArgumentTypes[a]))
					{
						fail = true;
						break;
					}
				}
			}
			if (fail) continue;

			for (unsigned a = 0; a < proto->ReturnTypes.Size(); a++)
			{
				PType* expected = vproto->ReturnTypes[a];
				PType* actual = proto->ReturnTypes[a];

				if (expected != actual && (exactReturnType || !AreCompatiblePointerTypes(expected, actual)))
				{
					fail = true;
					break;
				}
			}
			if (!fail)
			{
				if (vproto->ArgumentTypes.Size() > proto->ArgumentTypes.Size() && parentfunc)
				{
					// Check if the difference between both functions is only some optional arguments.
					for (unsigned a = proto->ArgumentTypes.Size(); a < vproto->ArgumentTypes.Size(); a++)
					{
						if (!(parentfunc->Variants[0].ArgFlags[a] & VARF_Optional)) return -1;
					}

					// Extend the prototype
					TArray<PType*> argumentTypes = proto->ArgumentTypes;

					for (unsigned a = proto->ArgumentTypes.Size(); a < vproto->ArgumentTypes.Size(); a++)
					{
						argumentTypes.Push(vproto->ArgumentTypes[a]);
						variant->ArgFlags.Push(parentfunc->Variants[0].ArgFlags[a]);
						variant->ArgNames.Push(NAME_None);
					}

					variant->Proto = NewPrototype(proto->ReturnTypes, argumentTypes);
				}
				return i;
			}
		}
	}
	return -1;
}

PSymbol *PClass::FindSymbol(FName symname, bool searchparents) const
{
	if (VMType == nullptr) return nullptr;
	return VMType->Symbols.FindSymbol(symname, searchparents);
}

//==========================================================================
//
// PClass :: BuildFlatPointers
//
// Create the FlatPointers array, if it doesn't exist already.
// It comprises all the Pointers from superclasses plus this class's own
// Pointers. If this class does not define any new Pointers, then
// FlatPointers will be set to the same array as the super class.
//
//==========================================================================

void PClass::BuildFlatPointers() const
{
	using pairType = std::pair<size_t, PObjectPointer *>;

	if (FlatPointers != nullptr)
	{ // Already built: Do nothing.
		return;
	}
	else
	{
		TArray<pairType> NativePointers;
		if (Pointers != nullptr)
		{
			for (size_t i = 0; Pointers[i] != ~(size_t)0; i++)
			{
				NativePointers.Push({Pointers[i], nullptr}); // native pointers have a null type
			}
		}

		if (ParentClass == nullptr)
		{ // No parent (i.e. DObject): FlatPointers is the same as Pointers.
			if (NativePointers.Size() == 0)
			{ // No pointers: Make FlatPointers a harmless non-nullptr.
				FlatPointers = (pairType*)(&TheEnd);
				FlatPointersSize = 0;
			}
			else
			{
				pairType *flat = (pairType*)ClassDataAllocator.Alloc(sizeof(pairType) * NativePointers.Size());
				memcpy(flat, NativePointers.Data(), sizeof(pairType) * NativePointers.Size());

				FlatPointers = flat;
				FlatPointersSize = NativePointers.Size();
			}
		}
		else
		{
			ParentClass->BuildFlatPointers();

			TArray<pairType> ScriptPointers;

			// Collect all pointers in scripted fields. These are not part of the Pointers list.
			for (auto field : Fields)
			{
				if (!(field->Flags & VARF_Native))
				{
					field->Type->SetPointer(Defaults, unsigned(field->Offset), &ScriptPointers);
				}
			}

			if (NativePointers.Size() == 0 && ScriptPointers.Size() == 0)
			{ // No new pointers: Just use the same FlatPointers as the parent.
				FlatPointers = ParentClass->FlatPointers;
				FlatPointersSize = ParentClass->FlatPointersSize;
			}
			else
			{	// New pointers: Create a new FlatPointers array and add them.
				// Concatenate them into a new array
				pairType *flat = (pairType*)ClassDataAllocator.Alloc(sizeof(pairType) * (ParentClass->FlatPointersSize + NativePointers.Size() + ScriptPointers.Size()));

				if (ParentClass->FlatPointersSize > 0)
				{
					memcpy (flat, ParentClass->FlatPointers, sizeof(pairType) * ParentClass->FlatPointersSize);
				}
				if (NativePointers.Size() > 0)
				{
					memcpy(flat + ParentClass->FlatPointersSize, NativePointers.Data(), sizeof(pairType) * NativePointers.Size());
				}
				if (ScriptPointers.Size() > 0)
				{
					memcpy(flat + ParentClass->FlatPointersSize + NativePointers.Size(), &ScriptPointers[0], sizeof(pairType) * ScriptPointers.Size());
				}
				FlatPointers = flat;
				FlatPointersSize = ParentClass->FlatPointersSize + NativePointers.Size() + ScriptPointers.Size();
			}
		}
	}
}

//==========================================================================
//
// PClass :: BuildArrayPointers
//
// same as above, but creates a list to dynamic object arrays
//
//==========================================================================

void PClass::BuildArrayPointers() const
{
	using pairType = std::pair<size_t, PDynArray *>;

	if (ArrayPointers != nullptr)
	{ // Already built: Do nothing.
		return;
	}
	else if (ParentClass == nullptr)
	{ // No parent (i.e. DObject): Make ArrayPointers a harmless non-nullptr.
		ArrayPointers = (pairType*)(&TheEnd);
		ArrayPointersSize = 0;
	}
	else
	{
		ParentClass->BuildArrayPointers();

		TArray<pairType> ScriptPointers;

		// Collect all arrays to pointers in scripted fields.
		for (auto field : Fields)
		{
			if (!(field->Flags & VARF_Native))
			{
				field->Type->SetPointerArray(Defaults, unsigned(field->Offset), &ScriptPointers);
			}
		}

		if (ScriptPointers.Size() == 0)
		{ // No new pointers: Just use the same ArrayPointers as the parent.
			ArrayPointers = ParentClass->ArrayPointers;
			ArrayPointersSize = ParentClass->ArrayPointersSize;
		}
		else
		{	// New pointers: Create a new ArrayPointers array and add them.
			// Concatenate them into a new array
			pairType *flat = (pairType*)ClassDataAllocator.Alloc(sizeof(pairType) * (ParentClass->ArrayPointersSize + ScriptPointers.Size()));
			if (ParentClass->ArrayPointersSize > 0)
			{
				memcpy(flat, ParentClass->ArrayPointers, sizeof(pairType) * ParentClass->ArrayPointersSize);
			}

			if (ScriptPointers.Size() > 0)
			{
				memcpy(flat + ParentClass->ArrayPointersSize, ScriptPointers.Data(), sizeof(pairType) * ScriptPointers.Size());
			}

			ArrayPointers = flat;
			ArrayPointersSize = ParentClass->ArrayPointersSize + ScriptPointers.Size();
		}
	}
}

//==========================================================================
//
// PClass :: BuildMapPointers
//
// same as above, but creates a list to dynamic object arrays
//
//==========================================================================

void PClass::BuildMapPointers() const
{
	using pairType = std::pair<size_t, PMap *>;

	if (MapPointers != nullptr)
	{ // Already built: Do nothing.
		return;
	}
	else if (ParentClass == nullptr)
	{ // No parent (i.e. DObject): Make MapPointers a harmless non-nullptr.
		MapPointers = (pairType*)(&TheEnd);
		MapPointersSize = 0;
	}
	else
	{
		ParentClass->BuildMapPointers();

		TArray<pairType> ScriptPointers;

		// Collect all arrays to pointers in scripted fields.
		for (auto field : Fields)
		{
			if (!(field->Flags & VARF_Native))
			{
				field->Type->SetPointerMap(Defaults, unsigned(field->Offset), &ScriptPointers);
			}
		}

		if (ScriptPointers.Size() == 0)
		{ // No new pointers: Just use the same ArrayPointers as the parent.
			MapPointers = ParentClass->MapPointers;
			MapPointersSize = ParentClass->MapPointersSize;
		}
		else
		{	// New pointers: Create a new FlatPointers array and add them.
			// Concatenate them into a new array
			pairType *flat = (pairType*)ClassDataAllocator.Alloc(sizeof(pairType) * (ParentClass->MapPointersSize + ScriptPointers.Size()));
			if (ParentClass->MapPointersSize > 0)
			{
				memcpy(flat, ParentClass->MapPointers, sizeof(pairType) * ParentClass->MapPointersSize); 
			}

			if (ScriptPointers.Size() > 0)
			{
				memcpy(flat + ParentClass->MapPointersSize, ScriptPointers.Data(), sizeof(pairType) * ScriptPointers.Size());
			}
			MapPointers = flat;
			MapPointersSize = ParentClass->MapPointersSize + ScriptPointers.Size();
		}
	}
}

//==========================================================================
//
// PClass :: NativeClass
//
// Finds the native type underlying this class.
//
//==========================================================================

const PClass *PClass::NativeClass() const
{
	const PClass *cls = this;

	while (cls && cls->bRuntimeClass)
		cls = cls->ParentClass;

	return cls;
}

VMFunction *PClass::FindFunction(FName clsname, FName funcname)
{
	auto cls = PClass::FindClass(clsname);
	if (!cls) return nullptr;
	auto func = dyn_cast<PFunction>(cls->FindSymbol(funcname, true));
	if (!func) return nullptr;
	return func->Variants[0].Implementation;
}

void PClass::FindFunction(VMFunction **pptr, FName clsname, FName funcname)
{
	auto cls = PClass::FindClass(clsname);
	if (!cls) return;
	auto func = dyn_cast<PFunction>(cls->FindSymbol(funcname, true));
	if (!func) return;
	*pptr = func->Variants[0].Implementation;
	FunctionPtrList.Push(pptr);
}

unsigned GetVirtualIndex(PClass *cls, const char *funcname)
{
	// Look up the virtual function index in the defining class because this may have gotten overloaded in subclasses with something different than a virtual override.
	auto sym = dyn_cast<PFunction>(cls->FindSymbol(funcname, false));
	assert(sym != nullptr);
	auto VIndex = sym->Variants[0].Implementation->VirtualIndex;
	return VIndex;
}


void PClass::InitializeDefaults()
{
	if (VMType != nullptr)	// purely internal classes have no symbol table
	{
		if (bRuntimeClass)
		{
			// Copy parent values from the parent defaults.
			assert(ParentClass != nullptr);
			if (Defaults != nullptr) ParentClass->InitializeSpecials(Defaults, ParentClass->Defaults, &PClass::SpecialInits);
			for (const PField* field : Fields)
			{
				if (!(field->Flags & VARF_Native) && !(field->Flags & VARF_Meta))
				{
					field->Type->SetDefaultValue(Defaults, unsigned(field->Offset), &SpecialInits);
				}
			}
		}
		if (Meta != nullptr) ParentClass->InitializeSpecials(Meta, ParentClass->Meta, &PClass::MetaInits);
		for (const PField* field : Fields)
		{
			if (!(field->Flags & VARF_Native) && (field->Flags & VARF_Meta))
			{
				field->Type->SetDefaultValue(Meta, unsigned(field->Offset), &MetaInits);
			}
		}
	}
}

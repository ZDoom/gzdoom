/*
** dobject.h
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

#ifndef __DOBJECT_H__
#define __DOBJECT_H__

#include <stdlib.h>
#include "tarray.h"
#include "doomtype.h"
#include "m_alloc.h"
#ifndef _MSC_VER
#include "autosegs.h"
#endif

struct PClass;

class FArchive;

class   DObject;
class           DArgs;
class           DBoundingBox;
class           DCanvas;
class           DConsoleCommand;
class                   DConsoleAlias;
class           DSeqNode;
class                   DSeqActorNode;
class                   DSeqPolyNode;
class                   DSeqSectorNode;
class           DThinker;
class                   AActor;
class                   DPolyAction;
class                           DMovePoly;
class                                   DPolyDoor;
class                           DRotatePoly;
class                   DPusher;
class                   DScroller;
class                   DSectorEffect;
class                           DLighting;
class                                   DFireFlicker;
class                                   DFlicker;
class                                   DGlow;
class                                   DGlow2;
class                                   DLightFlash;
class                                   DPhased;
class                                   DStrobe;
class                           DMover;
class                                   DElevator;
class                                   DMovingCeiling;
class                                           DCeiling;
class                                           DDoor;
class                                   DMovingFloor;
class                                           DFloor;
class                                           DFloorWaggle;
class                                           DPlat;
class                                   DPillar;

struct FActorInfo;

enum EMetaType
{
	META_Int,		// An int
	META_Fixed,		// A fixed point number
	META_String,	// A string
};

class FMetaData
{
private:
	FMetaData (EMetaType type, DWORD id) : Type(type), ID(id) {}

	FMetaData *Next;
	EMetaType Type;
	DWORD ID;
	union
	{
		int Int;
		char *String;
		fixed_t Fixed;
	} Value;

	friend class FMetaTable;
};

class FMetaTable
{
public:
	FMetaTable() : Meta(NULL) {}
	FMetaTable(const FMetaTable &other);
	~FMetaTable();
	FMetaTable &operator = (const FMetaTable &other);

	void SetMetaInt (DWORD id, int parm);
	void SetMetaFixed (DWORD id, fixed_t parm);
	void SetMetaString (DWORD id, const char *parm);	// The string is copied

	int GetMetaInt (DWORD id, int def=0) const;
	fixed_t GetMetaFixed (DWORD id, fixed_t def=0) const;
	const char *GetMetaString (DWORD id) const;

	FMetaData *FindMeta (EMetaType type, DWORD id) const;

private:
	FMetaData *Meta;
	FMetaData *FindMetaDef (EMetaType type, DWORD id);
	void FreeMeta ();
	void CopyMeta (const FMetaTable *other);
};

#define RUNTIME_TYPE(object)	(object->GetClass())	// Passed an object, returns the type of that object
#define RUNTIME_CLASS(cls)		(&cls::_StaticType)		// Passed a class name, returns a PClass representing that class
#define NATIVE_TYPE(object)		(object->StaticType())	// Passed an object, returns the type of the C++ class representing the object

struct ClassReg
{
	PClass *MyClass;
	const char *Name;
	PClass *ParentType;
	unsigned int SizeOf;
	const size_t *Pointers;
	void (*ConstructNative)(void *);

	void RegisterClass();
};

enum EInPlace { EC_InPlace };

#define DECLARE_ABSTRACT_CLASS(cls,parent) \
public: \
	static PClass _StaticType; \
	virtual PClass *StaticType() const { return &_StaticType; } \
	static ClassReg RegistrationInfo, *RegistrationInfoPtr; \
private: \
	typedef parent Super; \
	typedef cls ThisClass;

#define DECLARE_CLASS(cls,parent) \
	DECLARE_ABSTRACT_CLASS(cls,parent) \
		private: static void InPlaceConstructor (void *mem);

#define HAS_OBJECT_POINTERS \
	static const size_t PointerOffsets[];

// Taking the address of a field in an object at address 1 instead of
// address 0 keeps GCC from complaining about possible misuse of offsetof.
#define DECLARE_POINTER(field)	(size_t)&((ThisClass*)1)->field - 1,
#define END_POINTERS			~(size_t)0 };

#if defined(_MSC_VER)
#	pragma data_seg(".creg$u")
#	pragma data_seg()
#	define _DECLARE_TI(cls) __declspec(allocate(".creg$u")) ClassReg *cls::RegistrationInfoPtr = &cls::RegistrationInfo;
#else
#	define _DECLARE_TI(cls) ClassReg *cls::RegistrationInfoPtr __attribute__((section(CREG_SECTION))) = &cls::RegistrationInfo;
#endif

#define _IMP_PCLASS(cls,ptrs,create) \
	PClass cls::_StaticType; \
	ClassReg cls::RegistrationInfo = {\
		RUNTIME_CLASS(cls), \
		#cls, \
		RUNTIME_CLASS(cls::Super), \
		sizeof(cls), \
		ptrs, \
		create }; \
	_DECLARE_TI(cls)

#define _IMP_CREATE_OBJ(cls) \
	void cls::InPlaceConstructor(void *mem) { new((EInPlace *)mem) cls; }

#define IMPLEMENT_POINTY_CLASS(cls) \
	_IMP_CREATE_OBJ(cls) \
	_IMP_PCLASS(cls,cls::PointerOffsets,cls::InPlaceConstructor) \
	const size_t cls::PointerOffsets[] = {

#define IMPLEMENT_CLASS(cls) \
	_IMP_CREATE_OBJ(cls) \
	_IMP_PCLASS(cls,NULL,cls::InPlaceConstructor) 

#define IMPLEMENT_ABSTRACT_CLASS(cls) \
	_IMP_PCLASS(cls,NULL,NULL)

enum EObjectFlags
{
	OF_MassDestruction	= 0x00000001,   // Object is queued for deletion
	OF_Cleanup			= 0x00000002,   // Object is being deconstructed as a result of a queued deletion
	OF_JustSpawned		= 0x00000004,   // Thinker was spawned this tic
	OF_SerialSuccess	= 0x10000000    // For debugging Serialize() calls
};

class DObject
{
public:
	static PClass _StaticType;
	virtual PClass *StaticType() const { return &_StaticType; }
	static ClassReg RegistrationInfo, *RegistrationInfoPtr;
	static void InPlaceConstructor (void *mem);
private:
	typedef DObject ThisClass;

	// Per-instance variables. There are three.
public:
	DWORD ObjectFlags;			// Flags for this object
private:
	PClass *Class;				// This object's type
	unsigned int Index;			// This object's index in the global object table

public:
	DObject ();
	DObject (PClass *inClass);
	virtual ~DObject ();

	inline bool IsKindOf (const PClass *base) const;
	inline bool IsA (const PClass *type) const;

	virtual void Serialize (FArchive &arc);

	// For catching Serialize functions in derived classes
	// that don't call their base class.
	void CheckIfSerialized () const;

	virtual void Destroy ();

	static void BeginFrame ();
	static void EndFrame ();

	// If you need to replace one object with another and want to
	// change any pointers from the old object to the new object,
	// use this method.
	static void PointerSubstitution (DObject *old, DObject *notOld);

	static void StaticShutdown ();

	PClass *GetClass() const
	{
		if (Class == NULL)
		{
			// Save a little time the next time somebody wants this object's type
			// by recording it now.
			const_cast<DObject *>(this)->Class = StaticType();
		}
		return Class;
	}

	void SetClass (PClass *inClass)
	{
		Class = inClass;
	}

	void *operator new(size_t len)
	{
		return M_Malloc(len);
	}

	void operator delete (void *mem)
	{
		M_Free(mem);
	}

protected:
	// This form of placement new and delete is for use *only* by PClass's
	// CreateNew() method. Do not use them for some other purpose.
	void *operator new(size_t, EInPlace *mem)
	{
		return (void *)mem;
	}

	void operator delete (void *mem, EInPlace *)
	{
		free (mem);
	}

private:
	static TArray<DObject *> Objects;
	static TArray<unsigned int> FreeIndices;
	static TArray<DObject *> ToDestroy;

	static void DestroyScan (DObject *obj);
	static void DestroyScan ();

	void RemoveFromArray ();

	static bool Inactive;
};

#include "dobjtype.h"

inline bool DObject::IsKindOf (const PClass *base) const
{
	return base->IsAncestorOf (GetClass ());
}

inline bool DObject::IsA (const PClass *type) const
{
	return (type == GetClass());
}

#endif //__DOBJECT_H__

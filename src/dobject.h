/*
** dobject.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2001 Randy Heit
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

class FArchive;

class	DObject;
class		DArgs;
class		DBoundingBox;
class		DCanvas;
class		DConsoleCommand;
class			DConsoleAlias;
class		DSeqNode;
class			DSeqActorNode;
class			DSeqPolyNode;
class			DSeqSectorNode;
class		DThinker;
class			AActor;
class			DPolyAction;
class				DMovePoly;
class					DPolyDoor;
class				DRotatePoly;
class			DPusher;
class			DScroller;
class			DSectorEffect;
class				DLighting;
class					DFireFlicker;
class					DFlicker;
class					DGlow;
class					DGlow2;
class					DLightFlash;
class					DPhased;
class					DStrobe;
class				DMover;
class					DElevator;
class					DMovingCeiling;
class						DCeiling;
class						DDoor;
class					DMovingFloor;
class						DFloor;
class						DFloorWaggle;
class						DPlat;
class					DPillar;

struct FActorInfo;

struct TypeInfo
{
#if !defined(_MSC_VER) && !defined(__GNUC__)
	TypeInfo () : Pointers (NULL) { RegisterType(); }
	TypeInfo (const size_t *p, const char *inName, const TypeInfo *inParentType, unsigned int inSize)
		: Name (inName),
		  ParentType (inParentType),
		  SizeOf (inSize),
		  Pointers (p),
		  TypeIndex (0),
		  ActorInfo (NULL),
		  HashNext (0)
	{ RegisterType(); }
	TypeInfo (const size_t *p, const char *inName, const TypeInfo *inParentType, unsigned int inSize, DObject *(*inNew)())
		: Name (inName),
		  ParentType (inParentType),
		  SizeOf (inSize),
		  Pointers (p),
		  CreateNew (inNew),
		  TypeIndex(0),
		  ActorInfo (NULL),
		  HashNext (0)
	{ RegisterType(); }
#else
	static void StaticInit ();
#endif

	const char *Name;
	TypeInfo *ParentType;
	unsigned int SizeOf;
	const size_t *Pointers;
	DObject *(*CreateNew)();
	FActorInfo *ActorInfo;
	unsigned int HashNext;
	unsigned short TypeIndex;

	void RegisterType ();

	// Returns true if this type is an ancestor of (or same as) the passed type.
	bool IsAncestorOf (const TypeInfo *ti) const
	{
		while (ti)
		{
			if (this == ti)
				return true;
			ti = ti->ParentType;
		}
		return false;
	}
	inline bool IsDescendantOf (const TypeInfo *ti) const
	{
		return ti->IsAncestorOf (this);
	}

	static const TypeInfo *FindType (const char *name);
	static const TypeInfo *IFindType (const char *name);
	
	static unsigned short m_NumTypes, m_MaxTypes;
	static TypeInfo **m_Types;

	enum { HASH_SIZE = 256 };
	static unsigned int TypeHash[HASH_SIZE];
};

#define RUNTIME_TYPE(object)	(object->StaticType())
#define RUNTIME_CLASS(cls)		(&cls::_StaticType)

#define DECLARE_ABSTRACT_CLASS(cls,parent) \
public: \
	static TypeInfo _StaticType; \
	virtual TypeInfo *StaticType() const { return RUNTIME_CLASS(cls); } \
private: \
	typedef parent Super; \
	typedef cls ThisClass;

#define DECLARE_CLASS(cls,parent) \
	DECLARE_ABSTRACT_CLASS(cls,parent) \
	protected: static DObject *CreateObject (); private:

#define HAS_OBJECT_POINTERS \
	static const size_t _Pointers_[];

#define DECLARE_POINTER(field)	((size_t)&((ThisClass *)0)->field),
#define END_POINTERS			~0 };

#if !defined(_MSC_VER) && !defined(__GNUC__)
#	define _IMP_TYPEINFO(cls,ptr,create) \
		TypeInfo cls::_StaticType (ptr, #cls, RUNTIME_CLASS(cls::Super), sizeof(cls), create);
#else

#	if defined(_MSC_VER)
#		pragma data_seg(".creg$u")
#		pragma data_seg()
#		define _DECLARE_TI(cls) __declspec(allocate(".creg$u")) TypeInfo *_##cls##AddType = &cls::_StaticType;
#	else
#		define _DECLARE_TI(cls) TypeInfo *_##cls##AddType __attribute__((section("creg"))) = &cls::_StaticType;
#	endif

#	define _IMP_TYPEINFO(cls,ptrs,create) \
		TypeInfo cls::_StaticType = { \
			#cls, \
			RUNTIME_CLASS(cls::Super), \
			sizeof(cls), \
			ptrs, \
			create, }; \
		_DECLARE_TI(cls)

#endif

#define _IMP_CREATE_OBJ(cls) \
	DObject *cls::CreateObject() { return new cls; }

#define IMPLEMENT_POINTY_CLASS(cls) \
	_IMP_CREATE_OBJ(cls) \
	_IMP_TYPEINFO(cls,cls::_Pointers_,cls::CreateObject) \
	const size_t cls::_Pointers_[] = {

#define IMPLEMENT_CLASS(cls) \
	_IMP_CREATE_OBJ(cls) \
	_IMP_TYPEINFO(cls,NULL,cls::CreateObject)

#define IMPLEMENT_ABSTRACT_CLASS(cls) \
	_IMP_TYPEINFO(cls,NULL,NULL)

enum EObjectFlags
{
	OF_MassDestruction	= 0x00000001,	// Object is queued for deletion
	OF_Cleanup			= 0x00000002,	// Object is being deconstructed as a result of a queued deletion
	OF_JustSpawned		= 0x00000004,	// Thinker was spawned this tic
	OF_SerialSuccess	= 0x10000000	// For debugging Serialize() calls
};

class DObject
{
public: \
	static TypeInfo _StaticType; \
	virtual TypeInfo *StaticType() const { return &_StaticType; } \
private: \
	typedef DObject ThisClass;

public:
	DObject ();
	virtual ~DObject ();

	inline bool IsKindOf (const TypeInfo *base) const
	{
		return base->IsAncestorOf (StaticType ());
	}

	inline bool IsA (const TypeInfo *type) const
	{
		return (type == StaticType());
	}

	virtual void Serialize (FArchive &arc);

	// For catching Serialize functions in derived classes
	// that don't call their base class.
	void CheckIfSerialized () const;

	virtual void Destroy ();

	static void BeginFrame ();
	static void EndFrame ();

	DWORD ObjectFlags;

	static void STACK_ARGS StaticShutdown ();

private:
	static TArray<DObject *> Objects;
	static TArray<size_t> FreeIndices;
	static TArray<DObject *> ToDestroy;

	static void DestroyScan (DObject *obj);
	static void DestroyScan ();

	void RemoveFromArray ();

	static bool Inactive;
	size_t Index;
};

template<class T>
inline
FArchive &operator<< (FArchive &arc, T* &object)
{
	return arc.SerializeObject ((DObject*&)object, RUNTIME_CLASS(T));
}

#include "farchive.h"

#endif //__DOBJECT_H__

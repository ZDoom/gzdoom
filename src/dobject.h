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

struct TypeInfo
{
	TypeInfo () : Pointers (NULL) {}
	TypeInfo (const size_t *p, const char *inName, const TypeInfo *inParentType, unsigned int inSize)
		: Name (inName),
		  ParentType (inParentType),
		  SizeOf (inSize),
		  Pointers (p),
		  TypeIndex(0)
	{}
	TypeInfo (const size_t *p, const char *inName, const TypeInfo *inParentType, unsigned int inSize, DObject *(*inNew)())
		: Name (inName),
		  ParentType (inParentType),
		  SizeOf (inSize),
		  Pointers (p),
		  CreateNew (inNew),
		  TypeIndex(0)
	{}

	const char *Name;
	const TypeInfo *ParentType;
	unsigned int SizeOf;
	const size_t *const Pointers;
	DObject *(*CreateNew)();
	unsigned short TypeIndex;

	void RegisterType ();

	// Returns true if this type is an ansector of (or same as) the passed type.
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
	
	static unsigned short m_NumTypes, m_MaxTypes;
	static TypeInfo **m_Types;
};

struct ClassInit
{
	ClassInit (TypeInfo *newClass);
};

#define RUNTIME_TYPE(object)	(object->StaticType())
#define RUNTIME_CLASS(cls)		(&cls::_StaticType)

#define _DECLARE_CLASS(cls,parent) \
	virtual TypeInfo *StaticType() const { return RUNTIME_CLASS(cls); } \
private: \
	typedef parent Super; \
	typedef cls ThisClass; \
protected:

#define DECLARE_CLASS(cls,parent) \
public: \
	static const size_t cls##Pointers[]; \
	static TypeInfo _StaticType; \
	_DECLARE_CLASS(cls,parent)

#define _DECLARE_SERIAL(cls,parent) \
	static DObject *CreateObject (); \
public: \
	bool CanSerialize() { return true; } \
	void Serialize (FArchive &); \
	inline friend FArchive &operator>> (FArchive &arc, cls* &object) \
	{ \
		return arc.ReadObject ((DObject* &)object, RUNTIME_CLASS(cls)); \
	}

#define BEGIN_POINTERS(cls)		const size_t cls::cls##Pointers[] = {
#define DECLARE_POINTER(field)	((size_t)&((ThisClass *)0)->field),
#define END_POINTERS			~0 };
#define NO_POINTERS(cls)		const size_t cls::cls##Pointers[] = { ~0 };

#define DECLARE_SERIAL(cls,parent) \
	DECLARE_CLASS(cls,parent) \
	_DECLARE_SERIAL(cls,parent)

#define _IMPLEMENT_CLASS(cls,parent,new) \
	TypeInfo cls::_StaticType (cls::cls##Pointers, #cls, RUNTIME_CLASS(parent), sizeof(cls), new);

#define IMPLEMENT_POINTY_CLASS(cls,parent) \
	_IMPLEMENT_CLASS(cls,parent,NULL) \
	BEGIN_POINTERS(cls)

#define IMPLEMENT_CLASS(cls,parent) \
	_IMPLEMENT_CLASS(cls,parent,NULL) \
	NO_POINTERS(cls)

#define _IMPLEMENT_SERIAL(cls,parent) \
	_IMPLEMENT_CLASS(cls,parent,cls::CreateObject) \
	DObject *cls::CreateObject() { return new cls; } \
	static ClassInit _Init##cls(RUNTIME_CLASS(cls));

#define IMPLEMENT_POINTY_SERIAL(cls,parent) \
	_IMPLEMENT_SERIAL(cls,parent) \
	BEGIN_POINTERS(cls)

#define IMPLEMENT_SERIAL(cls,parent) \
	_IMPLEMENT_SERIAL(cls,parent) \
	NO_POINTERS(cls)

enum EObjectFlags
{
	OF_MassDestruction	= 0x00000001,	// Object is queued for deletion
	OF_Cleanup			= 0x00000002	// Object is being deconstructed as a result of a queued deletion
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

	virtual void Serialize (FArchive &arc) {}

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

#include "farchive.h"

#endif //__DOBJECT_H__

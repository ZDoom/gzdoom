#ifndef __DOBJECT_H__
#define __DOBJECT_H__

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
	const char *Name;
	const TypeInfo *ParentType;
	unsigned int SizeOf;
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

#define DECLARE_CLASS(cls,parent) \
public: \
	static TypeInfo _StaticType; \
	virtual TypeInfo *StaticType() const { return RUNTIME_CLASS(cls); } \
private: \
	typedef parent Super; \
	typedef cls ThisClass; \
protected: \

#define DECLARE_SERIAL(cls,parent) \
	DECLARE_CLASS(cls,parent) \
	static DObject *CreateObject (); \
public: \
	bool CanSerialize() { return true; } \
	void Serialize (FArchive &); \
	inline friend FArchive &operator>> (FArchive &arc, cls* &object) \
	{ \
		return arc.ReadObject ((DObject* &)object, RUNTIME_CLASS(cls)); \
	}

#define _IMPLEMENT_CLASS(cls,parent,new) \
	TypeInfo cls::_StaticType = { #cls, RUNTIME_CLASS(parent), sizeof(cls), new, 0 }; \

#define IMPLEMENT_CLASS(cls,parent) \
	_IMPLEMENT_CLASS(cls,parent,NULL)

#define IMPLEMENT_SERIAL(cls,parent) \
	_IMPLEMENT_CLASS(cls,parent,cls::CreateObject) \
	DObject *cls::CreateObject() { return new cls; } \
	static ClassInit _Init##cls(RUNTIME_CLASS(cls));


class DObject
{
public: \
	static TypeInfo _StaticType; \
	virtual TypeInfo *StaticType() const { return &_StaticType; } \
private: \
	typedef DObject ThisClass;

public:
	DObject () {}
	virtual ~DObject () {}

	inline bool IsKindOf (const TypeInfo *base) const
	{
		return base->IsAncestorOf (StaticType ());
	}

	inline bool IsA (const TypeInfo *type) const
	{
		return (type == StaticType());
	}

	virtual void Serialize (FArchive &arc) {}
};

#include "farchive.h"

#endif //__DOBJECT_H__
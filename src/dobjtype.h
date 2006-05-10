#ifndef DOBJTYPE_H
#define DOBJTYPE_H

#ifndef __DOBJECT_H__
#error You must #include "dobject.h" to get dobjtype.h
#endif

struct PClass
{
	static void StaticInit ();
	static void StaticFreeData (PClass *type);

	// Per-class information -------------------------------------
	FName				 TypeName;		// this class's name
	unsigned int		 Size;			// this class's size
	PClass				*ParentClass;	// the class this class derives from
	const size_t		*Pointers;		// object pointers defined by this class *only*
	const size_t		*FlatPointers;	// object pointers defined by this class and all its superclasses; not initialized by default
	FActorInfo			*ActorInfo;
	PClass				*HashNext;
	FMetaTable			 Meta;
	BYTE				*Defaults;
	bool				 bRuntimeClass;	// class was defined at run-time, not compile-time
	unsigned short		 ClassIndex;

	void (*ConstructNative)(void *);

	// The rest are all functions and static data ----------------
	void InsertIntoHash ();
	DObject *CreateNew () const;
	PClass *CreateDerivedClass (FName name, unsigned int size);
	void BuildFlatPointers ();

	// Returns true if this type is an ancestor of (or same as) the passed type.
	bool IsAncestorOf (const PClass *ti) const
	{
		while (ti)
		{
			if (this == ti)
				return true;
			ti = ti->ParentClass;
		}
		return false;
	}
	inline bool IsDescendantOf (const PClass *ti) const
	{
		return ti->IsAncestorOf (this);
	}

	static const PClass *FindClass (const char *name);
	static const PClass *FindClass (FName name);

	static TArray<PClass *> m_Types;
	static TArray<PClass *> m_RuntimeActors;

	enum { HASH_SIZE = 256 };
	static PClass *TypeHash[HASH_SIZE];
};

#endif

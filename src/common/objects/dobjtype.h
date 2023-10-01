#ifndef DOBJTYPE_H
#define DOBJTYPE_H

#ifndef __DOBJECT_H__
#error You must #include "dobject.h" to get dobjtype.h
#endif

#include <limits.h>

typedef std::pair<const class PType *, unsigned> FTypeAndOffset;

#if 0
// This is intentionally not in vm.h so that this file remains free of DObject pollution.
class VMException : public DObject
{
	DECLARE_CLASS(VMException, DObject);
};
#endif

// An action function -------------------------------------------------------

class VMFrameStack;
struct VMValue;
struct VMReturn;
class VMFunction;
class PClassType;
struct FNamespaceManager;
class PSymbol;
class PField;

enum
{
	TentativeClass = UINT_MAX,
};


class PClass
{
protected:
	void Derive(PClass *newclass, FName name);
	void SetSuper();
public:
	void InitializeSpecials(void* addr, void* defaults, TArray<FTypeAndOffset> PClass::* Inits);
	void WriteAllFields(FSerializer &ar, const void *addr) const;
	bool ReadAllFields(FSerializer &ar, void *addr) const;
	int FindVirtualIndex(FName name, PFunction::Variant *variant, PFunction *parentfunc, bool exactReturnType);
	PSymbol *FindSymbol(FName symname, bool searchparents) const;
	PField *AddField(FName name, PType *type, uint32_t flags, int fileno = 0);
	void InitializeDefaults();

	static void StaticInit();
	static void StaticShutdown();

	// Per-class information -------------------------------------
	PClass				*ParentClass = nullptr;	// the class this class derives from
	const size_t		*Pointers = nullptr;		// object pointers defined by this class *only*
	const size_t		*FlatPointers = nullptr;	// object pointers defined by this class and all its superclasses; not initialized by default
	const size_t		*ArrayPointers = nullptr;	// dynamic arrays containing object pointers.
	const std::pair<size_t,PType *>	*MapPointers = nullptr;	// maps containing object pointers.
	uint8_t				*Defaults = nullptr;
	uint8_t				*Meta = nullptr;			// Per-class static script data
	unsigned			 Size = sizeof(DObject);
	unsigned			 MetaSize = 0;
	FName				 TypeName = NAME_None;
	FName				 SourceLumpName = NAME_None;
	bool				 bRuntimeClass = false;	// class was defined at run-time, not compile-time
	bool				 bDecorateClass = false;	// may be subject to some idiosyncracies due to DECORATE backwards compatibility
	bool				 bAbstract = false;
	bool				 bSealed = false;
	bool				 bFinal = false;
	bool				 bOptional = false;
	TArray<VMFunction*>	 Virtuals;	// virtual function table
	TArray<FTypeAndOffset> MetaInits;
	TArray<FTypeAndOffset> SpecialInits;
	TArray<PField *> Fields;
	TArray<FName> SealedRestriction;
	PClassType			*VMType = nullptr;

	void (*ConstructNative)(void *);

	// The rest are all functions and static data ----------------
	PClass();
	~PClass();
	void InsertIntoHash(bool native);
	DObject *CreateNew();
	PClass *CreateDerivedClass(FName name, unsigned int size, bool *newlycreated = nullptr, int fileno = 0);

	void InitializeActorInfo();
	void BuildFlatPointers();
	void BuildArrayPointers();
	void BuildMapPointers();
	void DestroySpecials(void *addr);
	void DestroyMeta(void *addr);
	const PClass *NativeClass() const;

	// Returns true if this type is an ancestor of (or same as) the passed type.
	bool IsAncestorOf(const PClass *ti) const
	{
		while (ti)
		{
			if (this == ti)
				return true;
			ti = ti->ParentClass;
		}
		return false;
	}

	inline bool IsDescendantOf(const PClass *ti) const
	{
		return ti->IsAncestorOf(this);
	}

	inline bool IsDescendantOf(FName ti) const
	{
		auto me = this;
		while (me)
		{
			if (me->TypeName == ti)
				return true;
			me = me->ParentClass;
		}
		return false;
	}

	// Find a type, given its name.
	const PClass *FindParentClass(FName name) const;
	PClass *FindParentClass(FName name) { return const_cast<PClass *>(const_cast<const PClass *>(this)->FindParentClass(name)); }

	static PClass *FindClass(const char *name)			{ return FindClass(FName(name, true)); }
	static PClass *FindClass(const FString &name)		{ return FindClass(FName(name, true)); }
	static PClass *FindClass(ENamedName name)			{ return FindClass(FName(name)); }
	static PClass *FindClass(FName name);
	static PClassActor *FindActor(const char *name)		{ return FindActor(FName(name, true)); }
	static PClassActor *FindActor(const FString &name)	{ return FindActor(FName(name, true)); }
	static PClassActor *FindActor(ENamedName name)		{ return FindActor(FName(name)); }
	static PClassActor *FindActor(FName name);
	static VMFunction *FindFunction(FName cls, FName func);
	static void FindFunction(VMFunction **pptr, FName cls, FName func);
	PClass *FindClassTentative(FName name);

	static TMap<FName, PClass*> ClassMap;
	static TArray<PClass *> AllClasses;
	static TArray<VMFunction**> FunctionPtrList;

	static bool bShutdown;
	static bool bVMOperational;
};

#endif

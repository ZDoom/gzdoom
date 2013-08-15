#ifndef DOBJTYPE_H
#define DOBJTYPE_H

#ifndef __DOBJECT_H__
#error You must #include "dobject.h" to get dobjtype.h
#endif

#include "thingdef/thingdef_type.h"
#include "vm.h"

// Variable/parameter/field flags -------------------------------------------

// Making all these different storage types use a common set of flags seems
// like the simplest thing to do.

#define VARF_Optional		(1<<0)	// func param is optional
#define VARF_Method			(1<<1)	// func has an implied self parameter
#define VARF_Action			(1<<2)	// func has implied owner and state parameters

// Symbol information -------------------------------------------------------

class PSymbol : public DObject
{
	DECLARE_ABSTRACT_CLASS(PSymbol, DObject);
public:
	virtual ~PSymbol();

	FName SymbolName;

protected:
	PSymbol(FName name) { SymbolName = name; }
};

// A constant value ---------------------------------------------------------

class PSymbolConst : public PSymbol
{
	DECLARE_CLASS(PSymbolConst, PSymbol);
public:
	int ValueType;
	union
	{
		int Value;
		double Float;
	};

	PSymbolConst(FName name) : PSymbol(name) {}
	PSymbolConst() : PSymbol(NAME_None) {}
};

// A variable ---------------------------------------------------------

class PSymbolVariable : public PSymbol
{
	DECLARE_CLASS(PSymbolVariable, PSymbol);
public:
	FExpressionType ValueType;
	//int size;
	intptr_t offset;
	bool bUserVar;

	PSymbolVariable(FName name) : PSymbol(name) {}
	PSymbolVariable() : PSymbol(NAME_None) {}
};

// An action function -------------------------------------------------------

struct FState;
struct StateCallData;
class VMFrameStack;
struct VMValue;
struct VMReturn;
typedef int (*actionf_p)(VMFrameStack *stack, VMValue *param, int numparam, VMReturn *ret, int numret);/*(VM_ARGS)*/
class VMFunction;

// A VM function ------------------------------------------------------------

class PSymbolVMFunction : public PSymbol
{
	DECLARE_CLASS(PSymbolVMFunction, PSymbol);
	HAS_OBJECT_POINTERS;
public:
	VMFunction *Function;

	PSymbolVMFunction(FName name) : PSymbol(name) {}
	PSymbolVMFunction() : PSymbol(NAME_None) {}
};

// A symbol table -----------------------------------------------------------

struct PSymbolTable
{
	PSymbolTable();
	~PSymbolTable();

	size_t MarkSymbols();

	// Sets the table to use for searches if this one doesn't contain the
	// requested symbol.
	void SetParentTable (PSymbolTable *parent);

	// Finds a symbol in the table, optionally searching parent tables
	// as well.
	PSymbol *FindSymbol (FName symname, bool searchparents) const;

	// Places the symbol in the table and returns a pointer to it or NULL if
	// a symbol with the same name is already in the table. This symbol is
	// not copied and will be freed when the symbol table is destroyed.
	PSymbol *AddSymbol (PSymbol *sym);

	// Frees all symbols from this table.
	void ReleaseSymbols();

private:
	typedef TMap<FName, PSymbol *> MapType;

	PSymbolTable *ParentSymbolTable;
	MapType Symbols;

	friend class DObject;
};

// Basic information shared by all types ------------------------------------

// Only one copy of a type is ever instantiated at one time.
// - Enums, classes, and structs are defined by their names and outer classes.
// - Pointers are uniquely defined by the type they point at.
// - ClassPointers are also defined by their class restriction.
// - Arrays are defined by their element type and count.
// - DynArrays are defined by their element type.
// - Maps are defined by their key and value types.
// - Prototypes are defined by the argument and return types.
// - Functions are defined by their names and outer objects.
// In table form:
//                  Outer  Name  Type  Type2  Count
//   Enum             *      *
//   Class            *      *
//   Struct           *      *
//   Function         *      *
//   Pointer                       *
//   ClassPointer                  +      *
//   Array                         *            *
//   DynArray                      *
//   Map                           *      *
//   Prototype                     *+     *+

class PClassType;
class PType : public DObject
{
	//DECLARE_ABSTRACT_CLASS_WITH_META(PType, DObject, PClassType);
	// We need to unravel the _WITH_META macro, since PClassType isn't defined yet,
	// and we can't define it until we've defined PClass. But we can't define that
	// without defining PType.
	DECLARE_ABSTRACT_CLASS(PType, DObject)
	HAS_OBJECT_POINTERS;
protected:
	enum { MetaClassNum = CLASSREG_PClassType };
public:
	typedef PClassType MetaClass;
	MetaClass *GetClass() const;

	unsigned int	Size;			// this type's size
	unsigned int	Align;			// this type's preferred alignment
	PType			*HashNext;		// next type in this type table

	PType();
	PType(unsigned int size, unsigned int align);
	virtual ~PType();

	// Returns true if this type matches the two identifiers. Referring to the
	// above table, any type is identified by at most two characteristics. Each
	// type that implements this function will cast these to the appropriate type.
	// It is up to the caller to make sure they are the correct types. There is
	// only one prototype for this function in order to simplify type table
	// management.
	virtual bool IsMatch(intptr_t id1, intptr_t id2) const;

	// Get the type IDs used by IsMatch
	virtual void GetTypeIDs(intptr_t &id1, intptr_t &id2) const;

	static void StaticInit();
};

// Some categorization typing -----------------------------------------------

class PBasicType : public PType
{
	DECLARE_ABSTRACT_CLASS(PBasicType, PType);
public:
	PBasicType();
	PBasicType(unsigned int size, unsigned int align);
};

class PCompoundType : public PType
{
	DECLARE_ABSTRACT_CLASS(PCompoundType, PType);
};

class PNamedType : public PCompoundType
{
	DECLARE_ABSTRACT_CLASS(PNamedType, PCompoundType);
	HAS_OBJECT_POINTERS;
public:
	DObject			*Outer;			// object this type is contained within
	FName			TypeName;		// this type's name

	PNamedType() : Outer(NULL) {}
	PNamedType(FName name, DObject *outer) : Outer(outer), TypeName(name) {}

	virtual bool IsMatch(intptr_t id1, intptr_t id2) const;
	virtual void GetTypeIDs(intptr_t &id1, intptr_t &id2) const;
};

// Basic types --------------------------------------------------------------

class PInt : public PBasicType
{
	DECLARE_CLASS(PInt, PBasicType);
public:
	PInt(unsigned int size, bool unsign);

	bool Unsigned;
protected:
	PInt();
};

class PFloat : public PBasicType
{
	DECLARE_CLASS(PFloat, PBasicType);
public:
	PFloat(unsigned int size);
protected:
	PFloat();
};

class PString : public PBasicType
{
	DECLARE_CLASS(PString, PBasicType);
public:
	PString();
};

// Variations of integer types ----------------------------------------------

class PName : public PInt
{
	DECLARE_CLASS(PName, PInt);
public:
	PName();
};

class PSound : public PInt
{
	DECLARE_CLASS(PSound, PInt);
public:
	PSound();
};

class PColor : public PInt
{
	DECLARE_CLASS(PColor, PInt);
public:
	PColor();
};

// Pointers -----------------------------------------------------------------

class PStatePointer : public PInt
{
	DECLARE_CLASS(PStatePointer, PInt);
public:
	PStatePointer();
};

class PPointer : public PInt
{
	DECLARE_CLASS(PPointer, PInt);
	HAS_OBJECT_POINTERS;
public:
	PPointer(PType *pointsat);

	PType *PointedType;

	virtual bool IsMatch(intptr_t id1, intptr_t id2) const;
	virtual void GetTypeIDs(intptr_t &id1, intptr_t &id2) const;
protected:
	PPointer();
};

class PClass;
class PClassPointer : public PPointer
{
	DECLARE_CLASS(PClassPointer, PPointer);
	HAS_OBJECT_POINTERS;
public:
	PClassPointer(PClass *restrict);

	PClass *ClassRestriction;

	typedef PClass *Type2;

	virtual bool IsMatch(intptr_t id1, intptr_t id2) const;
	virtual void GetTypeIDs(intptr_t &id1, intptr_t &id2) const;
protected:
	PClassPointer();
};

// Struct/class fields ------------------------------------------------------

// A PField describes a symbol that takes up physical space in the struct.
class PField : public PSymbol
{
	DECLARE_CLASS(PField, PSymbol);
	HAS_OBJECT_POINTERS
public:
	PField(FName name, PType *type) : PSymbol(name), FieldOffset(0), FieldType(type) {}

	unsigned int FieldOffset;
	PType *FieldType;
protected:
	PField();
};

// Compound types -----------------------------------------------------------

class PEnum : public PNamedType
{
	DECLARE_CLASS(PEnum, PNamedType);
	HAS_OBJECT_POINTERS;
public:
	PEnum(FName name, DObject *outer);

	PType *ValueType;
	TMap<FName, int> Values;
protected:
	PEnum();
};

class PArray : public PCompoundType
{
	DECLARE_CLASS(PArray, PCompoundType);
	HAS_OBJECT_POINTERS;
public:
	PArray(PType *etype, unsigned int ecount);

	PType *ElementType;
	unsigned int ElementCount;
	unsigned int ElementSize;

	virtual bool IsMatch(intptr_t id1, intptr_t id2) const;
	virtual void GetTypeIDs(intptr_t &id1, intptr_t &id2) const;
protected:
	PArray();
};

// A vector is an array with extra operations.
class PVector : public PArray
{
	DECLARE_CLASS(PVector, PArray);
	HAS_OBJECT_POINTERS;
public:
	PVector(unsigned int size);
protected:
	PVector();
};

class PDynArray : public PCompoundType
{
	DECLARE_CLASS(PDynArray, PCompoundType);
	HAS_OBJECT_POINTERS;
public:
	PDynArray(PType *etype);

	PType *ElementType;

	virtual bool IsMatch(intptr_t id1, intptr_t id2) const;
	virtual void GetTypeIDs(intptr_t &id1, intptr_t &id2) const;
protected:
	PDynArray();
};

class PMap : public PCompoundType
{
	DECLARE_CLASS(PMap, PCompoundType);
	HAS_OBJECT_POINTERS;
public:
	PMap(PType *keytype, PType *valtype);

	PType *KeyType;
	PType *ValueType;

	virtual bool IsMatch(intptr_t id1, intptr_t id2) const;
	virtual void GetTypeIDs(intptr_t &id1, intptr_t &id2) const;
protected:
	PMap();
};

class PStruct : public PNamedType
{
	DECLARE_CLASS(PStruct, PNamedType);
public:
	PStruct(FName name, DObject *outer);

	TArray<PField *> Fields;
	PSymbolTable	 Symbols;

	PField *AddField(FName name, PType *type);

	size_t PropagateMark();
protected:
	PStruct();
};

class PPrototype : public PCompoundType
{
	DECLARE_CLASS(PPrototype, PCompoundType);
public:
	PPrototype(const TArray<PType *> &rettypes, const TArray<PType *> &argtypes);

	TArray<PType *> ArgumentTypes;
	TArray<PType *> ReturnTypes;

	size_t PropagateMark();
	virtual bool IsMatch(intptr_t id1, intptr_t id2) const;
	virtual void GetTypeIDs(intptr_t &id1, intptr_t &id2) const;
protected:
	PPrototype();
};

// TBD: Should we really support overloading?
class PFunction : public PSymbol
{
	DECLARE_CLASS(PFunction, PSymbol);
public:
	struct Variant
	{
		PPrototype *Proto;
		VMFunction *Implementation;
		TArray<DWORD> ArgFlags;		// Should be the same length as Proto->ArgumentTypes
	};
	TArray<Variant> Variants;
	DWORD Flags;

	unsigned AddVariant(PPrototype *proto, TArray<DWORD> &argflags, VMFunction *impl);

	size_t PropagateMark();

	PFunction(FName name) : PSymbol(name), Flags(0) {}
	PFunction() : PSymbol(NAME_None), Flags(0) {}
};

// Meta-info for every class derived from DObject ---------------------------

class PClassClass;
class PClass : public PStruct
{
	DECLARE_CLASS(PClass, PStruct);
	HAS_OBJECT_POINTERS;
protected:
	virtual void Derive(PClass *newclass);
	// We unravel _WITH_META here just as we did for PType.
	enum { MetaClassNum = CLASSREG_PClassClass };
public:
	typedef PClassClass MetaClass;
	MetaClass *GetClass() const;

	static void StaticInit();
	static void StaticShutdown();
	static void StaticBootstrap();

	// Per-class information -------------------------------------
	PClass				*ParentClass;	// the class this class derives from
	const size_t		*Pointers;		// object pointers defined by this class *only*
	const size_t		*FlatPointers;	// object pointers defined by this class and all its superclasses; not initialized by default
	BYTE				*Defaults;
	bool				 bRuntimeClass;	// class was defined at run-time, not compile-time

	void (*ConstructNative)(void *);

	// The rest are all functions and static data ----------------
	PClass();
	~PClass();
	void InsertIntoHash();
	DObject *CreateNew() const;
	PClass *CreateDerivedClass(FName name, unsigned int size);
	unsigned int Extend(unsigned int extension);
	void InitializeActorInfo();
	void BuildFlatPointers();
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

	// Find a type, given its name.
	static PClass *FindClass(const char *name)			{ return FindClass(FName(name, true)); }
	static PClass *FindClass(const FString &name)		{ return FindClass(FName(name, true)); }
	static PClass *FindClass(ENamedName name)			{ return FindClass(FName(name)); }
	static PClass *FindClass(FName name);
	static PClassActor *FindActor(const char *name)		{ return FindActor(FName(name, true)); }
	static PClassActor *FindActor(const FString &name)	{ return FindActor(FName(name, true)); }
	static PClassActor *FindActor(ENamedName name)		{ return FindActor(FName(name)); }
	static PClassActor *FindActor(FName name);
	PClass *FindClassTentative(FName name);	// not static!

	static TArray<PClass *> AllClasses;

	static bool bShutdown;
};

class PClassType : public PClass
{
	DECLARE_CLASS(PClassType, PClass);
protected:
	virtual void Derive(PClass *newclass);
public:
	PClassType();

	PClass *TypeTableType;	// The type to use for hashing into the type table
};

inline PType::MetaClass *PType::GetClass() const
{
	return static_cast<MetaClass *>(DObject::GetClass());
}

class PClassClass : public PClassType
{
	DECLARE_CLASS(PClassClass, PClassType);
public:
	PClassClass();
};

inline PClass::MetaClass *PClass::GetClass() const
{
	return static_cast<MetaClass *>(DObject::GetClass());
}

// A class that hasn't had its parent class defined yet ---------------------

class PClassWaitingForParent : public PClass
{
	DECLARE_CLASS(PClassWaitingForParent, PClass);
public:
	PClassWaitingForParent(FName myname, FName parentname);

	FName ParentName;
};

// Type tables --------------------------------------------------------------

struct FTypeTable
{
	enum { HASH_SIZE = 1021 };

	PType *TypeHash[HASH_SIZE];

	PType *FindType(PClass *metatype, intptr_t parm1, intptr_t parm2, size_t *bucketnum);
	void ReplaceType(PType *newtype, PType *oldtype, size_t bucket);
	void AddType(PType *type, PClass *metatype, intptr_t parm1, intptr_t parm2, size_t bucket);
	void AddType(PType *type);
	void Mark();
	void Clear();

	static size_t Hash(const PClass *p1, intptr_t p2, intptr_t p3);
};


extern FTypeTable TypeTable;

// Returns a type from the TypeTable. Will create one if it isn't present.
PVector *NewVector(unsigned int size);
PMap *NewMap(PType *keytype, PType *valuetype);
PArray *NewArray(PType *type, unsigned int count);
PDynArray *NewDynArray(PType *type);
PPointer *NewPointer(PType *type);
PClassPointer *NewClassPointer(PClass *restrict);
PClassWaitingForParent *NewUnknownClass(FName myname, FName parentname);
PEnum *NewEnum(FName name, DObject *outer);
PStruct *NewStruct(FName name, DObject *outer);
PPrototype *NewPrototype(const TArray<PType *> &rettypes, const TArray<PType *> &argtypes);

// Built-in types -----------------------------------------------------------

extern PInt *TypeSInt8,  *TypeUInt8;
extern PInt *TypeSInt16, *TypeUInt16;
extern PInt *TypeSInt32, *TypeUInt32;
extern PFloat *TypeFloat32, *TypeFloat64;
extern PString *TypeString;
extern PName *TypeName;
extern PSound *TypeSound;
extern PColor *TypeColor;
extern PStatePointer *TypeState;

#endif

#ifndef DOBJTYPE_H
#define DOBJTYPE_H

#ifndef __DOBJECT_H__
#error You must #include "dobject.h" to get dobjtype.h
#endif

typedef std::pair<const class PType *, unsigned> FTypeAndOffset;

#include "vm.h"

// Variable/parameter/field flags -------------------------------------------

// Making all these different storage types use a common set of flags seems
// like the simplest thing to do.

enum
{
	VARF_Optional		= (1<<0),	// func param is optional
	VARF_Method			= (1<<1),	// func has an implied self parameter
	VARF_Action			= (1<<2),	// func has implied owner and state parameters
	VARF_Native			= (1<<3),	// func is native code, field is natively defined
	VARF_ReadOnly		= (1<<4),	// field is read only, do not write to it
	VARF_Private		= (1<<5),	// field is private to containing class
	VARF_Protected		= (1<<6),	// field is only accessible by containing class and children.
	VARF_Deprecated		= (1<<7),	// Deprecated fields should output warnings when used.
	VARF_Virtual		= (1<<8),	// function is virtual
	VARF_Final			= (1<<9),	// Function may not be overridden in subclasses
	VARF_In				= (1<<10),
	VARF_Out			= (1<<11),
	VARF_Implicit		= (1<<12),	// implicitly created parameters (i.e. do not compare types when checking function signatures)
	VARF_Static			= (1<<13),	// static class data (by necessity read only.)
	VARF_InternalAccess	= (1<<14),	// overrides VARF_ReadOnly for internal script code.
	VARF_Override		= (1<<15),	// overrides a virtual function from the parent class.
	VARF_Ref			= (1<<16),	// argument is passed by reference.
	VARF_Transient		= (1<<17)  // don't auto serialize field.
};

// Symbol information -------------------------------------------------------

class PTypeBase : public DObject
{
	DECLARE_ABSTRACT_CLASS(PTypeBase, DObject)

public:
	virtual FString QualifiedName() const
	{
		return "";
	}
};

class PSymbol : public PTypeBase
{
	DECLARE_ABSTRACT_CLASS(PSymbol, PTypeBase);
public:
	virtual ~PSymbol();

	virtual FString QualifiedName() const
	{
		return SymbolName.GetChars();
	}

	FName SymbolName;

protected:
	PSymbol(FName name) { SymbolName = name; }
};

// An action function -------------------------------------------------------

struct FState;
struct StateCallData;
class VMFrameStack;
struct VMValue;
struct VMReturn;
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

// A symbol for a type ------------------------------------------------------

class PSymbolType : public PSymbol
{
	DECLARE_CLASS(PSymbolType, PSymbol);
	HAS_OBJECT_POINTERS;
public:
	class PType *Type;

	PSymbolType(FName name, class PType *ty) : PSymbol(name), Type(ty) {}
	PSymbolType() : PSymbol(NAME_None) {}
};

// A symbol table -----------------------------------------------------------

struct PSymbolTable
{
	PSymbolTable();
	PSymbolTable(PSymbolTable *parent);
	~PSymbolTable();

	size_t MarkSymbols();

	// Sets the table to use for searches if this one doesn't contain the
	// requested symbol.
	void SetParentTable (PSymbolTable *parent);
	PSymbolTable *GetParentTable() const
	{
		return ParentSymbolTable;
	}

	// Finds a symbol in the table, optionally searching parent tables
	// as well.
	PSymbol *FindSymbol (FName symname, bool searchparents) const;

	// Like FindSymbol with searchparents set true, but also returns the
	// specific symbol table the symbol was found in.
	PSymbol *FindSymbolInTable(FName symname, PSymbolTable *&symtable);


	// Places the symbol in the table and returns a pointer to it or NULL if
	// a symbol with the same name is already in the table. This symbol is
	// not copied and will be freed when the symbol table is destroyed.
	PSymbol *AddSymbol (PSymbol *sym);

	// Similar to AddSymbol but always succeeds. Returns the symbol that used
	// to be in the table with this name, if any.
	PSymbol *ReplaceSymbol(PSymbol *sym);

	// Frees all symbols from this table.
	void ReleaseSymbols();

	typedef TMap<FName, PSymbol *> MapType;

	MapType::Iterator GetIterator()
	{
		return MapType::Iterator(Symbols);
	}

private:

	PSymbolTable *ParentSymbolTable;
	MapType Symbols;

	friend class DObject;
};

// A symbol for a compiler tree node ----------------------------------------

class PSymbolTreeNode : public PSymbol
{
	DECLARE_CLASS(PSymbolTreeNode, PSymbol);
public:
	struct ZCC_TreeNode *Node;

	PSymbolTreeNode(FName name, struct ZCC_TreeNode *node) : PSymbol(name), Node(node) {}
	PSymbolTreeNode() : PSymbol(NAME_None) {}
};

extern PSymbolTable		 GlobalSymbols;

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

struct ZCC_ExprConstant;
class PClassType;
class PType : public PTypeBase
{
	//DECLARE_ABSTRACT_CLASS_WITH_META(PType, DObject, PClassType);
	// We need to unravel the _WITH_META macro, since PClassType isn't defined yet,
	// and we can't define it until we've defined PClass. But we can't define that
	// without defining PType.
	DECLARE_ABSTRACT_CLASS(PType, PTypeBase)
	HAS_OBJECT_POINTERS;
protected:
	enum { MetaClassNum = CLASSREG_PClassType };

public:
	typedef PClassType MetaClass;
	MetaClass *GetClass() const;

	struct Conversion
	{
		Conversion(PType *target, void (*convert)(ZCC_ExprConstant *, class FSharedStringArena &))
			: TargetType(target), ConvertConstant(convert) {}

		PType *TargetType;
		void (*ConvertConstant)(ZCC_ExprConstant *val, class FSharedStringArena &strdump);
	};

	unsigned int	Size;			// this type's size
	unsigned int	Align;			// this type's preferred alignment
	PType			*HashNext;		// next type in this type table
	PSymbolTable	Symbols;
	bool			MemberOnly = false;		// type may only be used as a struct/class member but not as a local variable or function argument.
	FString			mDescriptiveName;
	BYTE loadOp, storeOp, moveOp, RegType, RegCount;

	PType(unsigned int size = 1, unsigned int align = 1);
	virtual ~PType();
	virtual bool isNumeric() { return false; }

	bool AddConversion(PType *target, void (*convertconst)(ZCC_ExprConstant *, class FSharedStringArena &));

	int FindConversion(PType *target, const Conversion **slots, int numslots);

	// Writes the value of a variable of this type at (addr) to an archive, preceded by
	// a tag indicating its type. The tag is there so that variable types can be changed
	// without completely breaking savegames, provided that the change isn't between
	// totally unrelated types.
	virtual void WriteValue(FSerializer &ar, const char *key,const void *addr) const;

	// Returns true if the stored value was compatible. False otherwise.
	// If the value was incompatible, then the memory at *addr is unchanged.
	virtual bool ReadValue(FSerializer &ar, const char *key,void *addr) const;

	// Sets the default value for this type at (base + offset)
	// If the default value is binary 0, then this function doesn't need
	// to do anything, because PClass::Extend() takes care of that.
	//
	// The stroffs array is so that types that need special initialization
	// and destruction (e.g. strings) can add their offsets to it for special
	// initialization when the object is created and destruction when the
	// object is destroyed.
	virtual void SetDefaultValue(void *base, unsigned offset, TArray<FTypeAndOffset> *special=NULL) const;
	virtual void SetPointer(void *base, unsigned offset, TArray<size_t> *ptrofs = NULL) const;

	// Initialize the value, if needed (e.g. strings)
	virtual void InitializeValue(void *addr, const void *def) const;

	// Destroy the value, if needed (e.g. strings)
	virtual void DestroyValue(void *addr) const;

	// Sets the value of a variable of this type at (addr)
	virtual void SetValue(void *addr, int val);
	virtual void SetValue(void *addr, double val);

	// Gets the value of a variable of this type at (addr)
	virtual int GetValueInt(void *addr) const;
	virtual double GetValueFloat(void *addr) const;

	// Gets the opcode to store from a register to memory
	int GetStoreOp() const
	{
		return storeOp;
	}

	// Gets the opcode to load from memory to a register
	int GetLoadOp() const
	{
		return loadOp;
	}

	// Gets the opcode to move from register to another register
	int GetMoveOp() const
	{
		return moveOp;
	}

	// Gets the register type for this type
	int GetRegType() const
	{
		return RegType;
	}

	int GetRegCount() const
	{
		return RegCount;
	}
	// Returns true if this type matches the two identifiers. Referring to the
	// above table, any type is identified by at most two characteristics. Each
	// type that implements this function will cast these to the appropriate type.
	// It is up to the caller to make sure they are the correct types. There is
	// only one prototype for this function in order to simplify type table
	// management.
	virtual bool IsMatch(intptr_t id1, intptr_t id2) const;

	// Get the type IDs used by IsMatch
	virtual void GetTypeIDs(intptr_t &id1, intptr_t &id2) const;

	const char *DescriptiveName() const;

	size_t PropagateMark();

	static void StaticInit();

private:
	// Stuff for type conversion searches
	class VisitQueue
	{
	public:
		VisitQueue() : In(0), Out(0) {}
		void Push(PType *type);
		PType *Pop();
		bool IsEmpty() { return In == Out; }

	private:
		// This is a fixed-sized ring buffer.
		PType *Queue[64];
		int In, Out;

		void Advance(int &ptr)
		{
			ptr = (ptr + 1) & (countof(Queue) - 1);
		}
	};

	class VisitedNodeSet
	{
	public:
		VisitedNodeSet() { memset(Buckets, 0, sizeof(Buckets)); }
		void Insert(PType *node);
		bool Check(const PType *node);

	private:
		PType *Buckets[32];

		size_t Hash(const PType *type) { return size_t(type) >> 4; }
	};

	TArray<Conversion> Conversions;
	PType *PredType;
	PType *VisitNext;
	short PredConv;
	short Distance;

	void MarkPred(PType *pred, int conv, int dist)
	{
		PredType = pred;
		PredConv = conv;
		Distance = dist;
	}
	void FillConversionPath(const Conversion **slots);
};

// Not-really-a-type types --------------------------------------------------

class PErrorType : public PType
{
	DECLARE_CLASS(PErrorType, PType);
public:
	PErrorType(int which = 1) : PType(0, which) {}
};

class PVoidType : public PType
{
	DECLARE_CLASS(PVoidType, PType);
public:
	PVoidType() : PType(0, 1) {}
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
	PTypeBase		*Outer;			// object this type is contained within
	FName			TypeName;		// this type's name

	PNamedType() : Outer(NULL) {
		mDescriptiveName = "NamedType";
	}
	PNamedType(FName name, PTypeBase *outer) : Outer(outer), TypeName(name) {
		mDescriptiveName = name.GetChars();
	}

	virtual bool IsMatch(intptr_t id1, intptr_t id2) const;
	virtual void GetTypeIDs(intptr_t &id1, intptr_t &id2) const;
	virtual FString QualifiedName() const;
};

// Basic types --------------------------------------------------------------

class PInt : public PBasicType
{
	DECLARE_CLASS(PInt, PBasicType);
public:
	PInt(unsigned int size, bool unsign, bool compatible = true);

	void WriteValue(FSerializer &ar, const char *key,const void *addr) const override;
	bool ReadValue(FSerializer &ar, const char *key,void *addr) const override;

	virtual void SetValue(void *addr, int val);
	virtual void SetValue(void *addr, double val);
	virtual int GetValueInt(void *addr) const;
	virtual double GetValueFloat(void *addr) const;
	virtual bool isNumeric() override { return IntCompatible; }

	bool Unsigned;
	bool IntCompatible;
protected:
	PInt();
	void SetOps();
};

class PBool : public PInt
{
	DECLARE_CLASS(PBool, PInt);
public:
	PBool();
};

class PFloat : public PBasicType
{
	DECLARE_CLASS(PFloat, PBasicType);
public:
	PFloat(unsigned int size);

	void WriteValue(FSerializer &ar, const char *key,const void *addr) const override;
	bool ReadValue(FSerializer &ar, const char *key,void *addr) const override;

	virtual void SetValue(void *addr, int val);
	virtual void SetValue(void *addr, double val);
	virtual int GetValueInt(void *addr) const;
	virtual double GetValueFloat(void *addr) const;
	virtual bool isNumeric() override { return true; }
protected:
	PFloat();
	void SetOps();
private:
	struct SymbolInitF
	{
		ENamedName Name;
		double Value;
	};
	struct SymbolInitI
	{
		ENamedName Name;
		int Value;
	};

	void SetSingleSymbols();
	void SetDoubleSymbols();
	void SetSymbols(const SymbolInitF *syminit, size_t count);
	void SetSymbols(const SymbolInitI *syminit, size_t count);
};

class PString : public PBasicType
{
	DECLARE_CLASS(PString, PBasicType);
public:
	PString();

	void WriteValue(FSerializer &ar, const char *key,const void *addr) const override;
	bool ReadValue(FSerializer &ar, const char *key,void *addr) const override;
	void SetDefaultValue(void *base, unsigned offset, TArray<FTypeAndOffset> *special=NULL) const override;
	void InitializeValue(void *addr, const void *def) const override;
	void DestroyValue(void *addr) const override;
};

// Variations of integer types ----------------------------------------------

class PName : public PInt
{
	DECLARE_CLASS(PName, PInt);
public:
	PName();

	void WriteValue(FSerializer &ar, const char *key,const void *addr) const override;
	bool ReadValue(FSerializer &ar, const char *key,void *addr) const override;
};

class PSound : public PInt
{
	DECLARE_CLASS(PSound, PInt);
public:
	PSound();

	void WriteValue(FSerializer &ar, const char *key,const void *addr) const override;
	bool ReadValue(FSerializer &ar, const char *key,void *addr) const override;
};

class PSpriteID : public PInt
{
	DECLARE_CLASS(PSpriteID, PInt);
public:
	PSpriteID();

	void WriteValue(FSerializer &ar, const char *key, const void *addr) const override;
	bool ReadValue(FSerializer &ar, const char *key, void *addr) const override;
};

class PTextureID : public PInt
{
	DECLARE_CLASS(PTextureID, PInt);
public:
	PTextureID();

	void WriteValue(FSerializer &ar, const char *key, const void *addr) const override;
	bool ReadValue(FSerializer &ar, const char *key, void *addr) const override;
};

class PColor : public PInt
{
	DECLARE_CLASS(PColor, PInt);
public:
	PColor();
};

class PStateLabel : public PInt
{
	DECLARE_CLASS(PStateLabel, PInt);
public:
	PStateLabel();
};

// Pointers -----------------------------------------------------------------

class PPointer : public PBasicType
{
	DECLARE_CLASS(PPointer, PBasicType);
	HAS_OBJECT_POINTERS;
public:
	PPointer();
	PPointer(PType *pointsat, bool isconst = false);

	PType *PointedType;
	bool IsConst;

	virtual bool IsMatch(intptr_t id1, intptr_t id2) const;
	virtual void GetTypeIDs(intptr_t &id1, intptr_t &id2) const;
	void SetPointer(void *base, unsigned offset, TArray<size_t> *special = NULL) const override;

	void WriteValue(FSerializer &ar, const char *key,const void *addr) const override;
	bool ReadValue(FSerializer &ar, const char *key,void *addr) const override;

protected:
	void SetOps();
};

class PStatePointer : public PPointer
{
	DECLARE_CLASS(PStatePointer, PPointer);
public:
	PStatePointer();

	void WriteValue(FSerializer &ar, const char *key, const void *addr) const override;
	bool ReadValue(FSerializer &ar, const char *key, void *addr) const override;
};


class PClassPointer : public PPointer
{
	DECLARE_CLASS(PClassPointer, PPointer);
	HAS_OBJECT_POINTERS;
public:
	PClassPointer(class PClass *restrict);

	class PClass *ClassRestriction;

	// this is only here to block PPointer's implementation
	void SetPointer(void *base, unsigned offset, TArray<size_t> *special = NULL) const override {}

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
	PField(FName name, PType *type, DWORD flags = 0, size_t offset = 0, int bitvalue = 0);

	size_t Offset;
	PType *Type;
	DWORD Flags;
	int BitValue;
protected:
	PField();
};

// Compound types -----------------------------------------------------------

class PEnum : public PNamedType
{
	DECLARE_CLASS(PEnum, PNamedType);
	HAS_OBJECT_POINTERS;
public:
	PEnum(FName name, PTypeBase *outer);

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

	void WriteValue(FSerializer &ar, const char *key,const void *addr) const override;
	bool ReadValue(FSerializer &ar, const char *key,void *addr) const override;

	void SetDefaultValue(void *base, unsigned offset, TArray<FTypeAndOffset> *special) const override;
	void SetPointer(void *base, unsigned offset, TArray<size_t> *special) const override;

protected:
	PArray();
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
	PStruct(FName name, PTypeBase *outer);

	TArray<PField *> Fields;
	bool HasNativeFields;

	virtual PField *AddField(FName name, PType *type, DWORD flags=0);
	virtual PField *AddNativeField(FName name, PType *type, size_t address, DWORD flags = 0, int bitvalue = 0);

	size_t PropagateMark();

	void WriteValue(FSerializer &ar, const char *key,const void *addr) const override;
	bool ReadValue(FSerializer &ar, const char *key,void *addr) const override;
	void SetDefaultValue(void *base, unsigned offset, TArray<FTypeAndOffset> *specials) const override;
	void SetPointer(void *base, unsigned offset, TArray<size_t> *specials) const override;

	static void WriteFields(FSerializer &ar, const void *addr, const TArray<PField *> &fields);
	bool ReadFields(FSerializer &ar, void *addr) const;
protected:
	PStruct();
};

// a native struct will always be abstract and cannot be instantiated. All variables are references.
// In addition, native structs can have methods (but no virtual ones.)
class PNativeStruct : public PStruct
{
	DECLARE_CLASS(PNativeStruct, PStruct);
public:
	PNativeStruct(FName name = NAME_None);
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
		TArray<FName> ArgNames;		// we need the names to access them later when the function gets compiled.
		uint32_t Flags;
		int UseFlags;
		PStruct *SelfClass;
	};
	TArray<Variant> Variants;
	PStruct *OwningClass = nullptr;

	unsigned AddVariant(PPrototype *proto, TArray<DWORD> &argflags, TArray<FName> &argnames, VMFunction *impl, int flags, int useflags);
	int GetImplicitArgs()
	{
		if (Variants[0].Flags & VARF_Action) return 3;
		else if (Variants[0].Flags & VARF_Method) return 1;
		return 0;
	}

	size_t PropagateMark();

	PFunction(PStruct *owner = nullptr, FName name = NAME_None) : PSymbol(name), OwningClass(owner) {}
};

// Meta-info for every class derived from DObject ---------------------------

enum
{
	TentativeClass = UINT_MAX,
};

class PClassClass;
class PClass : public PNativeStruct
{
	DECLARE_CLASS(PClass, PNativeStruct);
	HAS_OBJECT_POINTERS;
protected:
	// We unravel _WITH_META here just as we did for PType.
	enum { MetaClassNum = CLASSREG_PClassClass };
	TArray<FTypeAndOffset> SpecialInits;
	void Derive(PClass *newclass, FName name);
	void InitializeSpecials(void *addr) const;
	void SetSuper();
public:
	typedef PClassClass MetaClass;
	MetaClass *GetClass() const;

	void WriteValue(FSerializer &ar, const char *key,const void *addr) const override;
	void WriteAllFields(FSerializer &ar, const void *addr) const;
	bool ReadValue(FSerializer &ar, const char *key,void *addr) const override;
	bool ReadAllFields(FSerializer &ar, void *addr) const;
	void InitializeDefaults();
	int FindVirtualIndex(FName name, PPrototype *proto);
	virtual void DeriveData(PClass *newclass);

	static void StaticInit();
	static void StaticShutdown();
	static void StaticBootstrap();

	// Per-class information -------------------------------------
	PClass				*ParentClass;	// the class this class derives from
	const size_t		*Pointers;		// object pointers defined by this class *only*
	const size_t		*FlatPointers;	// object pointers defined by this class and all its superclasses; not initialized by default
	BYTE				*Defaults;
	bool				 bRuntimeClass;	// class was defined at run-time, not compile-time
	bool				 bExported;		// This type has been declared in a script
	bool				 bDecorateClass;	// may be subject to some idiosyncracies due to DECORATE backwards compatibility
	TArray<VMFunction*>	 Virtuals;	// virtual function table

	void (*ConstructNative)(void *);

	// The rest are all functions and static data ----------------
	PClass();
	~PClass();
	void InsertIntoHash();
	DObject *CreateNew() const;
	PClass *CreateDerivedClass(FName name, unsigned int size);
	PField *AddField(FName name, PType *type, DWORD flags=0) override;
	void InitializeActorInfo();
	void BuildFlatPointers();
	void DestroySpecials(void *addr) const;
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
	PClass *FindClassTentative(FName name);

	static TArray<PClass *> AllClasses;

	static bool bShutdown;
};

class PClassType : public PClass
{
	DECLARE_CLASS(PClassType, PClass);
protected:
public:
	PClassType();
	virtual void DeriveData(PClass *newclass);

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
PMap *NewMap(PType *keytype, PType *valuetype);
PArray *NewArray(PType *type, unsigned int count);
PDynArray *NewDynArray(PType *type);
PPointer *NewPointer(PType *type, bool isconst = false);
PClassPointer *NewClassPointer(PClass *restrict);
PEnum *NewEnum(FName name, PTypeBase *outer);
PStruct *NewStruct(FName name, PTypeBase *outer);
PNativeStruct *NewNativeStruct(FName name, PTypeBase *outer);
PPrototype *NewPrototype(const TArray<PType *> &rettypes, const TArray<PType *> &argtypes);

// Built-in types -----------------------------------------------------------

extern PErrorType *TypeError;
extern PErrorType *TypeAuto;
extern PVoidType *TypeVoid;
extern PInt *TypeSInt8,  *TypeUInt8;
extern PInt *TypeSInt16, *TypeUInt16;
extern PInt *TypeSInt32, *TypeUInt32;
extern PBool *TypeBool;
extern PFloat *TypeFloat32, *TypeFloat64;
extern PString *TypeString;
extern PName *TypeName;
extern PSound *TypeSound;
extern PColor *TypeColor;
extern PTextureID *TypeTextureID;
extern PSpriteID *TypeSpriteID;
extern PStruct *TypeVector2;
extern PStruct *TypeVector3;
extern PStruct *TypeColorStruct;
extern PStruct *TypeStringStruct;
extern PStatePointer *TypeState;
extern PStateLabel *TypeStateLabel;
extern PPointer *TypeNullPtr;

// A constant value ---------------------------------------------------------

class PSymbolConst : public PSymbol
{
	DECLARE_CLASS(PSymbolConst, PSymbol);
public:
	PType *ValueType;

	PSymbolConst(FName name, PType *type=NULL) : PSymbol(name), ValueType(type) {}
	PSymbolConst() : PSymbol(NAME_None), ValueType(NULL) {}
};

// A constant numeric value -------------------------------------------------

class PSymbolConstNumeric : public PSymbolConst
{
	DECLARE_CLASS(PSymbolConstNumeric, PSymbolConst);
public:
	union
	{
		int Value;
		double Float;
		void *Pad;
	};

	PSymbolConstNumeric(FName name, PType *type=NULL) : PSymbolConst(name, type) {}
	PSymbolConstNumeric(FName name, PType *type, int val) : PSymbolConst(name, type), Value(val) {}
	PSymbolConstNumeric(FName name, PType *type, unsigned int val) : PSymbolConst(name, type), Value((int)val) {}
	PSymbolConstNumeric(FName name, PType *type, double val) : PSymbolConst(name, type), Float(val) {}
	PSymbolConstNumeric() {}
};

// A constant string value --------------------------------------------------

class PSymbolConstString : public PSymbolConst
{
	DECLARE_CLASS(PSymbolConstString, PSymbolConst);
public:
	FString Str;

	PSymbolConstString(FName name, FString &str) : PSymbolConst(name, TypeString), Str(str) {}
	PSymbolConstString() {}
};

void ReleaseGlobalSymbols();

// Enumerations for serializing types in an archive -------------------------

enum ETypeVal : BYTE
{
	VAL_Int8,
	VAL_UInt8,
	VAL_Int16,
	VAL_UInt16,
	VAL_Int32,
	VAL_UInt32,
	VAL_Int64,
	VAL_UInt64,
	VAL_Zero,
	VAL_One,
	VAL_Float32,
	VAL_Float64,
	VAL_String,
	VAL_Name,
	VAL_Struct,
	VAL_Array,
	VAL_Object,
	VAL_State,
	VAL_Class,
};

#endif

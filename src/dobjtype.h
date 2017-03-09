#ifndef DOBJTYPE_H
#define DOBJTYPE_H

#ifndef __DOBJECT_H__
#error You must #include "dobject.h" to get dobjtype.h
#endif

typedef std::pair<const class PType *, unsigned> FTypeAndOffset;
class PStruct;

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
	VARF_Static			= (1<<13),
	VARF_InternalAccess	= (1<<14),	// overrides VARF_ReadOnly for internal script code.
	VARF_Override		= (1<<15),	// overrides a virtual function from the parent class.
	VARF_Ref			= (1<<16),	// argument is passed by reference.
	VARF_Transient		= (1<<17),  // don't auto serialize field.
	VARF_Meta			= (1<<18),	// static class data (by necessity read only.)
	VARF_VarArg			= (1<<19),  // [ZZ] vararg: don't typecheck values after ... in function signature
	VARF_UI				= (1<<20),  // [ZZ] ui: object is ui-scope only (can't modify playsim)
	VARF_Play			= (1<<21),  // [ZZ] play: object is playsim-scope only (can't access ui)
	VARF_VirtualScope	= (1<<22),  // [ZZ] virtualscope: object should use the scope of the particular class it's being used with (methods only)
	VARF_ClearScope		= (1<<23),  // [ZZ] clearscope: this method ignores the member access chain that leads to it and is always plain data.
};

// An action function -------------------------------------------------------

struct FState;
struct StateCallData;
class VMFrameStack;
struct VMValue;
struct VMReturn;
class VMFunction;
struct FNamespaceManager;

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
class PType : public PTypeBase
{
	DECLARE_ABSTRACT_CLASS(PType, PTypeBase)
protected:

public:
	PClass *TypeTableType;			// The type to use for hashing into the type table
	unsigned int	Size;			// this type's size
	unsigned int	Align;			// this type's preferred alignment
	PType			*HashNext;		// next type in this type table
	PSymbolTable	Symbols;
	bool			MemberOnly = false;		// type may only be used as a struct/class member but not as a local variable or function argument.
	FString			mDescriptiveName;
	VersionInfo		mVersion = { 0,0,0 };
	uint8_t loadOp, storeOp, moveOp, RegType, RegCount;

	PType(unsigned int size = 1, unsigned int align = 1);
	virtual ~PType();
	virtual bool isNumeric() { return false; }

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
	virtual void SetPointerArray(void *base, unsigned offset, TArray<size_t> *ptrofs = NULL) const;

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

	static void StaticInit();
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

public:
	typedef void(*WriteHandler)(FSerializer &ar, const char *key, const void *addr);
	typedef bool(*ReadHandler)(FSerializer &ar, const char *key, void *addr);

	PPointer();
	PPointer(PType *pointsat, bool isconst = false);

	PType *PointedType;
	bool IsConst;

	WriteHandler writer = nullptr;
	ReadHandler reader = nullptr;

	void InstallHandlers(WriteHandler w, ReadHandler r)
	{
		writer = w;
		reader = r;
	}

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
public:
	PClassPointer(class PClass *restrict = nullptr);

	class PClass *ClassRestriction;

	bool isCompatible(PType *type);

	void SetPointer(void *base, unsigned offset, TArray<size_t> *special = NULL) const override;
	virtual bool IsMatch(intptr_t id1, intptr_t id2) const;
	virtual void GetTypeIDs(intptr_t &id1, intptr_t &id2) const;
};

// Compound types -----------------------------------------------------------

class PEnum : public PInt
{
	DECLARE_CLASS(PEnum, PInt);
public:
	PEnum(FName name, PTypeBase *outer);

	PTypeBase *Outer;
	FName EnumName;
protected:
	PEnum();
};

class PArray : public PCompoundType
{
	DECLARE_CLASS(PArray, PCompoundType);
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

class PResizableArray : public PArray
{
	DECLARE_CLASS(PResizableArray, PArray);
public:
	PResizableArray(PType *etype);

	virtual bool IsMatch(intptr_t id1, intptr_t id2) const;
	virtual void GetTypeIDs(intptr_t &id1, intptr_t &id2) const;

protected:
	PResizableArray();
};

class PDynArray : public PCompoundType
{
	DECLARE_CLASS(PDynArray, PCompoundType);
public:
	PDynArray(PType *etype, PStruct *backing);

	PType *ElementType;
	PStruct *BackingType;

	virtual bool IsMatch(intptr_t id1, intptr_t id2) const;
	virtual void GetTypeIDs(intptr_t &id1, intptr_t &id2) const;

	void WriteValue(FSerializer &ar, const char *key, const void *addr) const override;
	bool ReadValue(FSerializer &ar, const char *key, void *addr) const override;
	void SetDefaultValue(void *base, unsigned offset, TArray<FTypeAndOffset> *specials) const override;
	void InitializeValue(void *addr, const void *def) const override;
	void DestroyValue(void *addr) const override;
	void SetPointerArray(void *base, unsigned offset, TArray<size_t> *ptrofs = NULL) const override;

protected:
	PDynArray();
};

class PMap : public PCompoundType
{
	DECLARE_CLASS(PMap, PCompoundType);
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
	// Some internal structs require explicit construction and destruction of fields the VM cannot handle directly so use thes two functions for it.
	VMFunction *mConstructor = nullptr;
	VMFunction *mDestructor = nullptr;

	virtual PField *AddField(FName name, PType *type, uint32_t flags=0);
	virtual PField *AddNativeField(FName name, PType *type, size_t address, uint32_t flags = 0, int bitvalue = 0);

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
	PNativeStruct(FName name = NAME_None, PTypeBase *outer = nullptr);
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


// Meta-info for every class derived from DObject ---------------------------

enum
{
	TentativeClass = UINT_MAX,
};

class PClass : public PNativeStruct
{
	DECLARE_CLASS(PClass, PNativeStruct);
	// We unravel _WITH_META here just as we did for PType.
protected:
	TArray<FTypeAndOffset> MetaInits;
	void Derive(PClass *newclass, FName name);
	void InitializeSpecials(void *addr, void *defaults, TArray<FTypeAndOffset> PClass::*Inits);
	void SetSuper();
	PField *AddMetaField(FName name, PType *type, uint32_t flags);
public:
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
	TArray<FTypeAndOffset> SpecialInits;
	PClass				*ParentClass;	// the class this class derives from
	const size_t		*Pointers;		// object pointers defined by this class *only*
	const size_t		*FlatPointers;	// object pointers defined by this class and all its superclasses; not initialized by default
	const size_t		*ArrayPointers;	// dynamic arrays containing object pointers.
	uint8_t				*Defaults;
	uint8_t				*Meta;			// Per-class static script data
	unsigned			 MetaSize;
	bool				 bRuntimeClass;	// class was defined at run-time, not compile-time
	bool				 bExported;		// This type has been declared in a script
	bool				 bDecorateClass;	// may be subject to some idiosyncracies due to DECORATE backwards compatibility
	TArray<VMFunction*>	 Virtuals;	// virtual function table

	void (*ConstructNative)(void *);

	// The rest are all functions and static data ----------------
	PClass();
	~PClass();
	void InsertIntoHash();
	DObject *CreateNew();
	PClass *CreateDerivedClass(FName name, unsigned int size);
	PField *AddField(FName name, PType *type, uint32_t flags=0) override;
	void InitializeActorInfo();
	void BuildFlatPointers();
	void BuildArrayPointers();
	void InitMeta(); 
	void DestroySpecials(void *addr);
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

	static TArray<PClass *> AllClasses;
	static TArray<VMFunction**> FunctionPtrList;

	static bool bShutdown;
	static bool bVMOperational;
};

// Type tables --------------------------------------------------------------

struct FTypeTable
{
	enum { HASH_SIZE = 1021 };

	PType *TypeHash[HASH_SIZE];

	PType *FindType(PClass *metatype, intptr_t parm1, intptr_t parm2, size_t *bucketnum);
	void AddType(PType *type, PClass *metatype, intptr_t parm1, intptr_t parm2, size_t bucket);
	void AddType(PType *type);
	void Clear();

	static size_t Hash(const PClass *p1, intptr_t p2, intptr_t p3);
};


extern FTypeTable TypeTable;

// Returns a type from the TypeTable. Will create one if it isn't present.
PMap *NewMap(PType *keytype, PType *valuetype);
PArray *NewArray(PType *type, unsigned int count);
PResizableArray *NewResizableArray(PType *type);
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
extern PPointer *TypeFont;
extern PStateLabel *TypeStateLabel;
extern PPointer *TypeNullPtr;
extern PPointer *TypeVoidPtr;

// Enumerations for serializing types in an archive -------------------------

inline bool &DObject::BoolVar(FName field)
{
	return *(bool*)ScriptVar(field, TypeBool);
}

inline int &DObject::IntVar(FName field)
{
	return *(int*)ScriptVar(field, TypeSInt32);
}

inline FSoundID &DObject::SoundVar(FName field)
{
	return *(FSoundID*)ScriptVar(field, TypeSound);
}

inline PalEntry &DObject::ColorVar(FName field)
{
	return *(PalEntry*)ScriptVar(field, TypeColor);
}

inline FName &DObject::NameVar(FName field)
{
	return *(FName*)ScriptVar(field, TypeName);
}

inline double &DObject::FloatVar(FName field)
{
	return *(double*)ScriptVar(field, TypeFloat64);
}

inline FString &DObject::StringVar(FName field)
{
	return *(FString*)ScriptVar(field, TypeString);
}

template<class T>
inline T *&DObject::PointerVar(FName field)
{
	return *(T**)ScriptVar(field, nullptr);	// pointer check is more tricky and for the handful of uses in the DECORATE parser not worth the hassle.
}

void RemoveUnusedSymbols();

#endif

#pragma once

#include "dobject.h"
#include "serializer.h"
#include "symbols.h"
#include "scopebarrier.h"

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
	VARF_Abstract		= (1<<24),  // [Player701] Function does not have a body and must be overridden in subclasses
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

class PContainerType;
class PPointer;
class PClassPointer;
class PArray;
class PStruct;
class PClassType;

struct ZCC_ExprConstant;
class PType : public PTypeBase
{
protected:

	enum ETypeFlags
	{
		TYPE_Scalar = 1,
		TYPE_Container = 2,
		TYPE_Int = 4,
		TYPE_IntNotInt = 8,				// catch-all for subtypes that are not being checked by type directly.
		TYPE_Float = 16,
		TYPE_Pointer = 32,
		TYPE_ObjectPointer = 64,
		TYPE_ClassPointer = 128,
		TYPE_Array = 256,

		TYPE_IntCompatible = TYPE_Int | TYPE_IntNotInt,	// must be the combination of all flags that are subtypes of int and can be cast to an int.
	};

public:
	FName TypeTableType;			// The type to use for hashing into the type table
	unsigned int	Size;			// this type's size
	unsigned int	Align;			// this type's preferred alignment
	unsigned int	Flags = 0;		// What is this type?
	PType			*HashNext;		// next type in this type table
	PSymbolTable	Symbols;
	bool			MemberOnly = false;		// type may only be used as a struct/class member but not as a local variable or function argument.
	FString			mDescriptiveName;
	VersionInfo		mVersion = { 0,0,0 };
	uint8_t loadOp, storeOp, moveOp, RegType, RegCount;
	EScopeFlags ScopeFlags = (EScopeFlags)0;
	bool            SizeKnown = true;

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
	virtual void SetDefaultValue(void *base, unsigned offset, TArray<FTypeAndOffset> *special=NULL);
	virtual void SetPointer(void *base, unsigned offset, TArray<size_t> *ptrofs = NULL);
	virtual void SetPointerArray(void *base, unsigned offset, TArray<size_t> *ptrofs = NULL);

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

	bool isScalar() const { return !!(Flags & TYPE_Scalar); }
	bool isContainer() const { return !!(Flags & TYPE_Container); }
	bool isInt() const { return (Flags & TYPE_IntCompatible) == TYPE_Int; }
	bool isIntCompatible() const { return !!(Flags & TYPE_IntCompatible); }
	bool isFloat() const { return !!(Flags & TYPE_Float); }
	bool isPointer() const { return !!(Flags & TYPE_Pointer); }
	bool isRealPointer() const { return (Flags & (TYPE_Pointer|TYPE_ClassPointer)) == TYPE_Pointer; }	// This excludes class pointers which use their PointedType differently
	bool isObjectPointer() const { return !!(Flags & TYPE_ObjectPointer); }
	bool isClassPointer() const { return !!(Flags & TYPE_ClassPointer); }
	bool isEnum() const { return TypeTableType == NAME_Enum; }
	bool isArray() const { return !!(Flags & TYPE_Array); }
	bool isStaticArray() const { return TypeTableType == NAME_StaticArray; }
	bool isDynArray() const { return TypeTableType == NAME_DynArray; }
	bool isStruct() const { return TypeTableType == NAME_Struct; }
	bool isClass() const { return TypeTableType == NAME_Object; }
	bool isPrototype() const { return TypeTableType == NAME_Prototype; }

	PContainerType *toContainer() { return isContainer() ? (PContainerType*)this : nullptr; }
	PPointer *toPointer() { return isPointer() ? (PPointer*)this : nullptr; }
	static PClassPointer *toClassPointer(PType *t) { return t && t->isClassPointer() ? (PClassPointer*)t : nullptr; }
	static PClassType *toClass(PType *t) { return t && t->isClass() ? (PClassType*)t : nullptr; }
};

// Not-really-a-type types --------------------------------------------------

class PErrorType : public PType
{
public:
	PErrorType(int which = 1) : PType(0, which) {}
};

class PVoidType : public PType
{
public:
	PVoidType() : PType(0, 1) {}
};

// Some categorization typing -----------------------------------------------

class PBasicType : public PType
{
protected:
	PBasicType(unsigned int size = 1, unsigned int align = 1);
};

class PCompoundType : public PType
{
protected:
	PCompoundType(unsigned int size = 1, unsigned int align = 1);
};

class PContainerType : public PCompoundType
{
public:
	PTypeBase		*Outer = nullptr;			// object this type is contained within
	FName			TypeName = NAME_None;		// this type's name

	PContainerType()
	{
		mDescriptiveName = "ContainerType";
		Flags |= TYPE_Container;
	}
	PContainerType(FName name, PTypeBase *outer) : Outer(outer), TypeName(name) 
	{
		mDescriptiveName = name.GetChars();
		Flags |= TYPE_Container;
	}

	virtual bool IsMatch(intptr_t id1, intptr_t id2) const;
	virtual void GetTypeIDs(intptr_t &id1, intptr_t &id2) const;
	virtual PField *AddField(FName name, PType *type, uint32_t flags = 0) = 0;
	virtual PField *AddNativeField(FName name, PType *type, size_t address, uint32_t flags = 0, int bitvalue = 0) = 0;
};

// Basic types --------------------------------------------------------------

class PInt : public PBasicType
{
public:
	PInt(unsigned int size, bool unsign, bool compatible = true);

	void WriteValue(FSerializer &ar, const char *key,const void *addr) const override;
	bool ReadValue(FSerializer &ar, const char *key,void *addr) const override;

	virtual void SetValue(void *addr, int val) override;
	virtual void SetValue(void *addr, double val) override;
	virtual int GetValueInt(void *addr) const override;
	virtual double GetValueFloat(void *addr) const override;
	virtual bool isNumeric() override { return IntCompatible; }

	bool Unsigned;
	bool IntCompatible;
protected:
	void SetOps();
};

class PBool : public PInt
{
public:
	PBool();
	virtual void SetValue(void *addr, int val);
	virtual void SetValue(void *addr, double val);
	virtual int GetValueInt(void *addr) const;
	virtual double GetValueFloat(void *addr) const;
};

class PFloat : public PBasicType
{
public:
	PFloat(unsigned int size = 8);

	void WriteValue(FSerializer &ar, const char *key,const void *addr) const override;
	bool ReadValue(FSerializer &ar, const char *key,void *addr) const override;

	virtual void SetValue(void *addr, int val) override;
	virtual void SetValue(void *addr, double val) override;
	virtual int GetValueInt(void *addr) const override;
	virtual double GetValueFloat(void *addr) const override;
	virtual bool isNumeric() override { return true; }
protected:
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
public:
	PString();

	void WriteValue(FSerializer &ar, const char *key,const void *addr) const override;
	bool ReadValue(FSerializer &ar, const char *key,void *addr) const override;
	void SetDefaultValue(void *base, unsigned offset, TArray<FTypeAndOffset> *special=NULL) override;
	void InitializeValue(void *addr, const void *def) const override;
	void DestroyValue(void *addr) const override;
};

// Variations of integer types ----------------------------------------------

class PName : public PInt
{
public:
	PName();

	void WriteValue(FSerializer &ar, const char *key,const void *addr) const override;
	bool ReadValue(FSerializer &ar, const char *key,void *addr) const override;
};

class PSound : public PInt
{
public:
	PSound();

	void WriteValue(FSerializer &ar, const char *key,const void *addr) const override;
	bool ReadValue(FSerializer &ar, const char *key,void *addr) const override;
};

class PSpriteID : public PInt
{
public:
	PSpriteID();

	void WriteValue(FSerializer &ar, const char *key, const void *addr) const override;
	bool ReadValue(FSerializer &ar, const char *key, void *addr) const override;
};

class PTextureID : public PInt
{
public:
	PTextureID();

	void WriteValue(FSerializer &ar, const char *key, const void *addr) const override;
	bool ReadValue(FSerializer &ar, const char *key, void *addr) const override;
};

class PColor : public PInt
{
public:
	PColor();
};

class PStateLabel : public PInt
{
public:
	PStateLabel();
};

// Pointers -----------------------------------------------------------------

class PPointer : public PBasicType
{

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

	bool IsMatch(intptr_t id1, intptr_t id2) const override;
	void GetTypeIDs(intptr_t &id1, intptr_t &id2) const override;

	void WriteValue(FSerializer &ar, const char *key,const void *addr) const override;
	bool ReadValue(FSerializer &ar, const char *key,void *addr) const override;

protected:
	void SetOps();
};

class PStatePointer : public PPointer
{
public:
	PStatePointer();

	void WriteValue(FSerializer &ar, const char *key, const void *addr) const override;
	bool ReadValue(FSerializer &ar, const char *key, void *addr) const override;
};


class PObjectPointer : public PPointer
{
public:
	PObjectPointer(PClass *pointedtype = nullptr, bool isconst = false);

	void WriteValue(FSerializer &ar, const char *key, const void *addr) const override;
	bool ReadValue(FSerializer &ar, const char *key, void *addr) const override;
	void SetPointer(void *base, unsigned offset, TArray<size_t> *special = NULL) override;
	PClass *PointedClass() const;
};


class PClassPointer : public PPointer
{
public:
	PClassPointer(class PClass *restrict = nullptr);

	class PClass *ClassRestriction;

	bool isCompatible(PType *type);
	void WriteValue(FSerializer &ar, const char *key, const void *addr) const override;
	bool ReadValue(FSerializer &ar, const char *key, void *addr) const override;

	void SetPointer(void *base, unsigned offset, TArray<size_t> *special = NULL) override;
	 bool IsMatch(intptr_t id1, intptr_t id2) const override;
	 void GetTypeIDs(intptr_t &id1, intptr_t &id2) const override;
};

// Compound types -----------------------------------------------------------

class PEnum : public PInt
{
public:
	PEnum(FName name, PTypeBase *outer);

	PTypeBase *Outer;
	FName EnumName;
};

class PArray : public PCompoundType
{
public:
	PArray(PType *etype, unsigned int ecount);

	PType *ElementType;
	unsigned int ElementCount;
	unsigned int ElementSize;

	bool IsMatch(intptr_t id1, intptr_t id2) const override;
	void GetTypeIDs(intptr_t &id1, intptr_t &id2) const override;

	void WriteValue(FSerializer &ar, const char *key,const void *addr) const override;
	bool ReadValue(FSerializer &ar, const char *key,void *addr) const override;

	void SetDefaultValue(void *base, unsigned offset, TArray<FTypeAndOffset> *special) override;
	void SetPointer(void *base, unsigned offset, TArray<size_t> *special) override;
	void SetPointerArray(void *base, unsigned offset, TArray<size_t> *ptrofs = NULL) override;
};

class PStaticArray : public PArray
{
public:
	PStaticArray(PType *etype);

	virtual bool IsMatch(intptr_t id1, intptr_t id2) const;
	virtual void GetTypeIDs(intptr_t &id1, intptr_t &id2) const;
};

class PDynArray : public PCompoundType
{
public:
	PDynArray(PType *etype, PStruct *backing);

	PType *ElementType;
	PStruct *BackingType;

	bool IsMatch(intptr_t id1, intptr_t id2) const override;
	void GetTypeIDs(intptr_t &id1, intptr_t &id2) const override;

	void WriteValue(FSerializer &ar, const char *key, const void *addr) const override;
	bool ReadValue(FSerializer &ar, const char *key, void *addr) const override;
	void SetDefaultValue(void *base, unsigned offset, TArray<FTypeAndOffset> *specials) override;
	void InitializeValue(void *addr, const void *def) const override;
	void DestroyValue(void *addr) const override;
	void SetPointerArray(void *base, unsigned offset, TArray<size_t> *ptrofs = NULL) override;
};

class PMap : public PCompoundType
{
public:
	PMap(PType *keytype, PType *valtype);

	PType *KeyType;
	PType *ValueType;

	virtual bool IsMatch(intptr_t id1, intptr_t id2) const;
	virtual void GetTypeIDs(intptr_t &id1, intptr_t &id2) const;
};

class PStruct : public PContainerType
{
public:
	PStruct(FName name, PTypeBase *outer, bool isnative = false);

	bool isNative;
	bool isOrdered = false;
	// Some internal structs require explicit construction and destruction of fields the VM cannot handle directly so use these two functions for it.
	VMFunction *mConstructor = nullptr;
	VMFunction *mDestructor = nullptr;

	 PField *AddField(FName name, PType *type, uint32_t flags=0) override;
	 PField *AddNativeField(FName name, PType *type, size_t address, uint32_t flags = 0, int bitvalue = 0) override;

	void WriteValue(FSerializer &ar, const char *key,const void *addr) const override;
	bool ReadValue(FSerializer &ar, const char *key,void *addr) const override;
	void SetDefaultValue(void *base, unsigned offset, TArray<FTypeAndOffset> *specials) override;
	void SetPointer(void *base, unsigned offset, TArray<size_t> *specials) override;
	void SetPointerArray(void *base, unsigned offset, TArray<size_t> *special) override;
};

class PPrototype : public PCompoundType
{
public:
	PPrototype(const TArray<PType *> &rettypes, const TArray<PType *> &argtypes);

	TArray<PType *> ArgumentTypes;
	TArray<PType *> ReturnTypes;

	virtual bool IsMatch(intptr_t id1, intptr_t id2) const;
	virtual void GetTypeIDs(intptr_t &id1, intptr_t &id2) const;
};


// Meta-info for every class derived from DObject ---------------------------

class PClassType : public PContainerType
{
public:
	PClass *Descriptor;
	PClassType *ParentType;

	PClassType(PClass *cls = nullptr);
	PField *AddField(FName name, PType *type, uint32_t flags = 0) override;
	PField *AddNativeField(FName name, PType *type, size_t address, uint32_t flags = 0, int bitvalue = 0) override;
};


inline PClass *PObjectPointer::PointedClass() const
{
	return static_cast<PClassType*>(PointedType)->Descriptor;
}

// Returns a type from the TypeTable. Will create one if it isn't present.
PMap *NewMap(PType *keytype, PType *valuetype);
PArray *NewArray(PType *type, unsigned int count);
PStaticArray *NewStaticArray(PType *type);
PDynArray *NewDynArray(PType *type);
PPointer *NewPointer(PType *type, bool isconst = false);
PPointer *NewPointer(PClass *type, bool isconst = false);
PClassPointer *NewClassPointer(PClass *restrict);
PEnum *NewEnum(FName name, PTypeBase *outer);
PStruct *NewStruct(FName name, PTypeBase *outer, bool native = false);
PPrototype *NewPrototype(const TArray<PType *> &rettypes, const TArray<PType *> &argtypes);
PClassType *NewClassType(PClass *cls);

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
extern PStruct* TypeVector2;
extern PStruct* TypeVector3;
extern PStruct* TypeVector4;
extern PStruct* TypeFVector2;
extern PStruct* TypeFVector3;
extern PStruct* TypeFVector4;
extern PStruct *TypeColorStruct;
extern PStruct *TypeStringStruct;
extern PStatePointer *TypeState;
extern PPointer *TypeFont;
extern PStateLabel *TypeStateLabel;
extern PPointer *TypeNullPtr;
extern PPointer *TypeVoidPtr;

inline FString &DObject::StringVar(FName field)
{
	return *(FString*)ScriptVar(field, TypeString);
}

// Type tables --------------------------------------------------------------

struct FTypeTable
{
	enum { HASH_SIZE = 1021 };

	PType *TypeHash[HASH_SIZE];

	PType *FindType(FName type_name, intptr_t parm1, intptr_t parm2, size_t *bucketnum);
	void AddType(PType *type, FName type_name, intptr_t parm1, intptr_t parm2, size_t bucket);
	void AddType(PType *type, FName type_name);
	void Clear();

	static size_t Hash(FName p1, intptr_t p2, intptr_t p3);
};


extern FTypeTable TypeTable;


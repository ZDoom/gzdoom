// Note: This must not be included by anything but dobject.h!
#pragma once

#include "sc_man.h"

class VMFunction;
class PType;
class PPrototype;
struct ZCC_TreeNode;
class PContainerType;

// This is needed in common code, despite being Doom specific.
enum EStateUseFlags
{
	SUF_ACTOR = 1,
	SUF_OVERLAY = 2,
	SUF_WEAPON = 4,
	SUF_ITEM = 8,
};


// Symbol information -------------------------------------------------------

class PTypeBase
{
public:
	// Allocate everything on the global memory arena because all subtypes of this 
	// will live until the end of the game.
	void *operator new(size_t size)
	{
		return ClassDataAllocator.Alloc(size);
	}

	void operator delete(void *)
	{}
};

class PSymbol : public DObject
{
	DECLARE_ABSTRACT_CLASS(PSymbol, DObject);
public:
	FName SymbolName;
	VersionInfo mVersion = { 0,0,0 };

protected:
	PSymbol(FName name) { SymbolName = name; }
};

// A VM function ------------------------------------------------------------

// A symbol for a type ------------------------------------------------------

class PSymbolType : public PSymbol
{
	DECLARE_CLASS(PSymbolType, PSymbol);
public:
	PType *Type = nullptr;

	PSymbolType(FName name, class PType *ty) : PSymbol(name), Type(ty) {}
	PSymbolType() : PSymbol(NAME_None) {}
};

// A symbol for a compiler tree node ----------------------------------------

class PSymbolTreeNode : public PSymbol
{
	DECLARE_CLASS(PSymbolTreeNode, PSymbol);
public:
	struct ZCC_TreeNode *Node = nullptr;

	PSymbolTreeNode(FName name, struct ZCC_TreeNode *node) : PSymbol(name), Node(node) {}
	PSymbolTreeNode() : PSymbol(NAME_None) {}
};

// Struct/class fields ------------------------------------------------------

// A PField describes a symbol that takes up physical space in the struct.
class PField : public PSymbol
{
	DECLARE_CLASS(PField, PSymbol);
	HAS_OBJECT_POINTERS
public:
	PField(FName name, PType *type, uint32_t flags = 0, size_t offset = 0, int bitvalue = 0);
	VersionInfo GetVersion();

	size_t Offset;
	PType *Type;
	uint32_t Flags;
	int BitValue;
	FString DeprecationMessage;
	int mDefFileNo = 0;
protected:
	PField();
};

// Properties ------------------------------------------------------

// For setting properties in class defaults.
class PProperty : public PSymbol
{
	DECLARE_CLASS(PProperty, PSymbol);
public:
	PProperty(FName name, TArray<PField *> &variables);

	TArray<PField *> Variables;

protected:
	PProperty();
};

class PPropFlag : public PSymbol
{
	DECLARE_CLASS(PPropFlag, PSymbol);
public:
	PPropFlag(FName name, PField *offset, int bitval, bool decorateonly);

	PField *Offset;
	int bitval;
	bool decorateOnly;

protected:
	PPropFlag();
};

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

	PSymbolConstNumeric(FName name, PType *type=nullptr) : PSymbolConst(name, type), Float(0) {}
	PSymbolConstNumeric(FName name, PType *type, int val) : PSymbolConst(name, type), Value(val) {}
	PSymbolConstNumeric(FName name, PType *type, unsigned int val) : PSymbolConst(name, type), Value((int)val) {}
	PSymbolConstNumeric(FName name, PType *type, double val) : PSymbolConst(name, type), Float(val) {}
	PSymbolConstNumeric() : Float(0) {}
};

// A constant string value --------------------------------------------------

class PSymbolConstString : public PSymbolConst
{
	DECLARE_CLASS(PSymbolConstString, PSymbolConst);
public:
	FString Str;

	PSymbolConstString(FName name, const FString &str);
	PSymbolConstString() {}
};


// A function for the VM --------------------------------------------------

// TBD: Should we really support overloading?
class PFunction : public PSymbol
{
	DECLARE_CLASS(PFunction, PSymbol);
public:
	struct Variant
	{
		PPrototype *Proto;
		VMFunction *Implementation;
		TArray<uint32_t> ArgFlags;		// Should be the same length as Proto->ArgumentTypes
		TArray<FName> ArgNames;		// we need the names to access them later when the function gets compiled.
		uint32_t Flags;
		int UseFlags;
		PContainerType *SelfClass;
		FString DeprecationMessage;
	};
	TArray<Variant> Variants;
	PContainerType *OwningClass = nullptr;

	unsigned AddVariant(PPrototype *proto, TArray<uint32_t> &argflags, TArray<FName> &argnames, VMFunction *impl, int flags, int useflags);
	int GetImplicitArgs();

	PFunction(PContainerType *owner = nullptr, FName name = NAME_None) : PSymbol(name), OwningClass(owner) {}
};

// A symbol table -----------------------------------------------------------

struct PSymbolTable
{
	PSymbolTable();
	PSymbolTable(PSymbolTable *parent);
	~PSymbolTable();

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
	PField *AddField(FName name, PType *type, uint32_t flags, unsigned &Size, unsigned *Align = nullptr, int fileno = 0);
	PField *AddNativeField(FName name, PType *type, size_t address, uint32_t flags, int bitvalue, int fileno = 0);
	bool ReadFields(FSerializer &ar, void *addr, const char *TypeName) const;
	void WriteFields(FSerializer &ar, const void *addr, const void *def = nullptr) const;

	// Similar to AddSymbol but always succeeds. Returns the symbol that used
	// to be in the table with this name, if any.
	void ReplaceSymbol(PSymbol *sym);

	void RemoveSymbol(PSymbol *sym);

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
	friend struct FNamespaceManager;
};

// Namespaces --------------------------------------------------

class PNamespace : public PTypeBase
{
public:
	PSymbolTable Symbols;
	PNamespace *Parent;
	int FileNum;	// This is for blocking DECORATE access to later files.

	PNamespace(int filenum, PNamespace *parent);
};

struct FNamespaceManager
{
	PNamespace *GlobalNamespace;
	TArray<PNamespace *> AllNamespaces;

	FNamespaceManager();
	PNamespace *NewNamespace(int filenum);
	void ReleaseSymbols();
	int RemoveSymbols();
};

extern FNamespaceManager Namespaces;
void RemoveUnusedSymbols();

struct AFuncDesc;
struct FieldDesc;
AFuncDesc *FindFunction(PContainerType *cls, const char * string);
FieldDesc *FindField(PContainerType *cls, const char * string);

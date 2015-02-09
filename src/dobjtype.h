#ifndef DOBJTYPE_H
#define DOBJTYPE_H

#ifndef __DOBJECT_H__
#error You must #include "dobject.h" to get dobjtype.h
#endif

#include "thingdef/thingdef_type.h"

// Symbol information -------------------------------------------------------

enum ESymbolType
{
	SYM_Const,
	SYM_Variable,
	SYM_ActionFunction
};

struct PSymbol
{
	virtual ~PSymbol();

	ESymbolType SymbolType;
	FName SymbolName;

protected:
	PSymbol(FName name, ESymbolType type) { SymbolType = type; SymbolName = name; }
};

// A constant value ---------------------------------------------------------

struct PSymbolConst : public PSymbol
{
	int ValueType;
	union
	{
		int Value;
		double Float;
	};

	PSymbolConst(FName name) : PSymbol(name, SYM_Const) {}
};

// A variable ---------------------------------------------------------

struct PSymbolVariable : public PSymbol
{
	FExpressionType ValueType;
	//int size;
	intptr_t offset;
	bool bUserVar;

	PSymbolVariable(FName name) : PSymbol(name, SYM_Variable) {}
};

// An action function -------------------------------------------------------
//
// The Arguments string is a string of characters as understood by
// the DECORATE parser:
//
// If the letter is uppercase, it is required. Lowercase letters are optional.
//   i = integer
//   f = fixed point
//   s = sound name
//   m = actor name
//   t = string
//   l = jump label
//   c = color
//   x = expression
//   y = expression
// If the final character is a +, the previous parameter is repeated indefinitely,
// and an "imaginary" first parameter is inserted containing the total number of
// parameters passed.
struct FState;
struct StateCallData;
typedef void (*actionf_p)(AActor *self, AActor *stateowner, FState *state, int parameters, StateCallData *statecall);

struct PSymbolActionFunction : public PSymbol
{
	FString Arguments;
	actionf_p Function;
	int defaultparameterindex;

	PSymbolActionFunction(FName name) : PSymbol(name, SYM_ActionFunction) {}
};

// A symbol table -----------------------------------------------------------

class PSymbolTable
{
public:
	PSymbolTable() : ParentSymbolTable(NULL)
	{
	}

	~PSymbolTable();

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
	PSymbolTable *ParentSymbolTable;
	TArray<PSymbol *> Symbols;

	friend class DObject;
};

// Meta-info for every class derived from DObject ---------------------------

struct PClass
{
	static void StaticInit ();
	static void StaticShutdown ();
	static void StaticFreeData (PClass *type);
	static void ClearRuntimeData();

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
	PSymbolTable		 Symbols;

	void (*ConstructNative)(void *);

	// The rest are all functions and static data ----------------
	void InsertIntoHash ();
	DObject *CreateNew () const;
	PClass *CreateDerivedClass (FName name, unsigned int size);
	unsigned int Extend(unsigned int extension);
	void InitializeActorInfo ();
	void BuildFlatPointers ();
	void FreeStateList();
	const PClass *NativeClass() const;

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

	// Find a type, given its name.
	static const PClass *FindClass (const char *name) { return FindClass (FName (name, true)); }
	static const PClass *FindClass (const FString &name) { return FindClass (FName (name, true)); }
	static const PClass *FindClass (ENamedName name) { return FindClass (FName (name)); }
	static const PClass *FindClass (FName name);
	const PClass *FindClassTentative (FName name);	// not static!
	PClass *GetReplacement() const;

	static TArray<PClass *> m_Types;
	static TArray<PClass *> m_RuntimeActors;

	enum { HASH_SIZE = 256 };
	static PClass *TypeHash[HASH_SIZE];

	static bool bShutdown;
};

#endif

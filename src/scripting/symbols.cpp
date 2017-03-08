/*
** symbols.cpp
** Implements the symbol types and symbol table
**
**---------------------------------------------------------------------------
** Copyright 1998-2016 Randy Heit
** Copyright 2006-2017 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include <float.h>
#include "dobject.h"
#include "i_system.h"

// PUBLIC DATA DEFINITIONS -------------------------------------------------

FNamespaceManager Namespaces;

// Symbol tables ------------------------------------------------------------

IMPLEMENT_CLASS(PTypeBase, true, false);
IMPLEMENT_CLASS(PSymbol, true, false);
IMPLEMENT_CLASS(PSymbolConst, false, false);
IMPLEMENT_CLASS(PSymbolConstNumeric, false, false);
IMPLEMENT_CLASS(PSymbolConstString, false, false);
IMPLEMENT_CLASS(PSymbolTreeNode, false, false)
IMPLEMENT_CLASS(PSymbolType, false, false)
IMPLEMENT_CLASS(PSymbolVMFunction, false, false)
IMPLEMENT_CLASS(PFunction, false, false)
IMPLEMENT_CLASS(PNamespace, false, true)

IMPLEMENT_POINTERS_START(PNamespace)
IMPLEMENT_POINTER(Parent)
IMPLEMENT_POINTERS_END


//==========================================================================
//
//
//
//==========================================================================

PSymbolConstString::PSymbolConstString(FName name, const FString &str)
	: PSymbolConst(name, TypeString), Str(str) 
{
}

//==========================================================================
//
// PFunction :: AddVariant
//
// Adds a new variant for this function. Does not check if a matching
// variant already exists.
//
//==========================================================================

unsigned PFunction::AddVariant(PPrototype *proto, TArray<uint32_t> &argflags, TArray<FName> &argnames, VMFunction *impl, int flags, int useflags)
{
	Variant variant;

	// I do not think we really want to deal with overloading here...
	assert(Variants.Size() == 0);

	variant.Flags = flags;
	variant.UseFlags = useflags;
	variant.Proto = proto;
	variant.ArgFlags = std::move(argflags);
	variant.ArgNames = std::move(argnames);
	variant.Implementation = impl;
	if (impl != nullptr) impl->Proto = proto;

	// SelfClass can differ from OwningClass, but this is variant-dependent.
	// Unlike the owner there can be cases where different variants can have different SelfClasses.
	// (Of course only if this ever gets enabled...)
	if (flags & VARF_Method)
	{
		assert(proto->ArgumentTypes.Size() > 0);
		auto selftypeptr = dyn_cast<PPointer>(proto->ArgumentTypes[0]);
		assert(selftypeptr != nullptr);
		variant.SelfClass = dyn_cast<PStruct>(selftypeptr->PointedType);
		assert(variant.SelfClass != nullptr);
	}
	else
	{
		variant.SelfClass = nullptr;
	}

	return Variants.Push(variant);
}

//==========================================================================
//
//
//
//==========================================================================

int PFunction::GetImplicitArgs()
{
	if (Variants[0].Flags & VARF_Action) return 3;
	else if (Variants[0].Flags & VARF_Method) return 1;
	return 0;
}

//==========================================================================
//
//
//
//==========================================================================

PSymbolTable::PSymbolTable()
: ParentSymbolTable(nullptr)
{
}

PSymbolTable::PSymbolTable(PSymbolTable *parent)
: ParentSymbolTable(parent)
{
}

PSymbolTable::~PSymbolTable ()
{
	ReleaseSymbols();
}

//==========================================================================
//
// this must explicitly delete all content because the symbols have
// been released from the GC.
//
//==========================================================================

void PSymbolTable::ReleaseSymbols()
{
	auto it = GetIterator();
	MapType::Pair *pair;
	while (it.NextPair(pair))
	{
		delete pair->Value;
	}
	Symbols.Clear();
}

//==========================================================================
//
//
//
//==========================================================================

void PSymbolTable::SetParentTable (PSymbolTable *parent)
{
	ParentSymbolTable = parent;
}

//==========================================================================
//
//
//
//==========================================================================

PSymbol *PSymbolTable::FindSymbol (FName symname, bool searchparents) const
{
	PSymbol * const *value = Symbols.CheckKey(symname);
	if (value == nullptr && searchparents && ParentSymbolTable != nullptr)
	{
		return ParentSymbolTable->FindSymbol(symname, searchparents);
	}
	return value != nullptr ? *value : nullptr;
}

//==========================================================================
//
//
//
//==========================================================================

PSymbol *PSymbolTable::FindSymbolInTable(FName symname, PSymbolTable *&symtable)
{
	PSymbol * const *value = Symbols.CheckKey(symname);
	if (value == nullptr)
	{
		if (ParentSymbolTable != nullptr)
		{
			return ParentSymbolTable->FindSymbolInTable(symname, symtable);
		}
		symtable = nullptr;
		return nullptr;
	}
	symtable = this;
	return *value;
}

//==========================================================================
//
//
//
//==========================================================================

PSymbol *PSymbolTable::AddSymbol (PSymbol *sym)
{
	// Symbols that already exist are not inserted.
	if (Symbols.CheckKey(sym->SymbolName) != nullptr)
	{
		return nullptr;
	}
	Symbols.Insert(sym->SymbolName, sym);
	sym->Release();	// no more GC, please!
	return sym;
}

//==========================================================================
//
//
//
//==========================================================================

void PSymbolTable::RemoveSymbol(PSymbol *sym)
{
	auto mysym = Symbols.CheckKey(sym->SymbolName);
	if (mysym == nullptr || *mysym != sym) return;
	Symbols.Remove(sym->SymbolName);
	delete sym;
}

//==========================================================================
//
//
//
//==========================================================================

void PSymbolTable::ReplaceSymbol(PSymbol *newsym)
{
	// If a symbol with a matching name exists, take its place and return it.
	PSymbol **symslot = Symbols.CheckKey(newsym->SymbolName);
	if (symslot != nullptr)
	{
		PSymbol *oldsym = *symslot;
		delete oldsym;
		*symslot = newsym;
	}
	// Else, just insert normally and return nullptr since there was no
	// symbol to replace.
	newsym->Release();	// no more GC, please!
	Symbols.Insert(newsym->SymbolName, newsym);
}

//==========================================================================
//
//
//
//==========================================================================

//==========================================================================
//
//
//
//==========================================================================

PNamespace::PNamespace(int filenum, PNamespace *parent)
{
	Parent = parent;
	if (parent) Symbols.SetParentTable(&parent->Symbols);
	FileNum = filenum;
}

//==========================================================================
//
//
//
//==========================================================================

FNamespaceManager::FNamespaceManager()
{
	GlobalNamespace = nullptr;
}

//==========================================================================
//
//
//
//==========================================================================

PNamespace *FNamespaceManager::NewNamespace(int filenum)
{
	PNamespace *parent = nullptr;
	// The parent will be the last namespace with this or a lower filenum.
	// This ensures that DECORATE won't see the symbols of later files.
	for (int i = AllNamespaces.Size() - 1; i >= 0; i--)
	{
		if (AllNamespaces[i]->FileNum <= filenum)
		{
			parent = AllNamespaces[i];
			break;
		}
	}
	auto newns = new PNamespace(filenum, parent);
	AllNamespaces.Push(newns);
	return newns;
}

//==========================================================================
//
//
//
//==========================================================================

size_t FNamespaceManager::MarkSymbols()
{
	for (auto ns : AllNamespaces)
	{
		GC::Mark(ns);
	}
	return AllNamespaces.Size();
}

//==========================================================================
//
//
//
//==========================================================================

void FNamespaceManager::ReleaseSymbols()
{
	RemoveSymbols();
	GlobalNamespace = nullptr;
	AllNamespaces.Clear();
}

//==========================================================================
//
// removes all symbols from the symbol tables.
// After running the compiler these are not needed anymore.
// Only the namespaces themselves are kept because the type table references them.
//
//==========================================================================

int FNamespaceManager::RemoveSymbols()
{
	int count = 0;
	for (auto ns : AllNamespaces)
	{
		count += ns->Symbols.Symbols.CountUsed();
		ns->Symbols.ReleaseSymbols();
	}
	return count;
}

//==========================================================================
//
// Clean out all compiler-only data from the symbol tables
//
//==========================================================================

void RemoveUnusedSymbols()
{
	// Global symbols are not needed anymore after running the compiler.
	int count = Namespaces.RemoveSymbols();

	// We do not need any non-field and non-function symbols in structs and classes anymore.
	// struct/class fields and functions are still needed so that the game can access the script data,
	// but all the rest serves no purpose anymore and can be entirely removed.
	for (size_t i = 0; i < countof(TypeTable.TypeHash); ++i)
	{
		for (PType *ty = TypeTable.TypeHash[i]; ty != nullptr; ty = ty->HashNext)
		{
			if (ty->IsKindOf(RUNTIME_CLASS(PStruct)))
			{
				auto it = ty->Symbols.GetIterator();
				PSymbolTable::MapType::Pair *pair;
				while (it.NextPair(pair))
				{
					if (!pair->Value->IsKindOf(RUNTIME_CLASS(PField)) && !pair->Value->IsKindOf(RUNTIME_CLASS(PFunction)))
					{
						ty->Symbols.RemoveSymbol(pair->Value);
						count++;
					}
				}
			}
		}
	}
	DPrintf(DMSG_SPAMMY, "%d symbols removed after compilation\n", count);
}

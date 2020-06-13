/*
** thingdef_data.cpp
**
** DECORATE data tables
**
**---------------------------------------------------------------------------
** Copyright 2002-2020 Christoph Oelckers
** Copyright 2004-2008 Randy Heit
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

#include "gstrings.h"
#include "v_font.h"
#include "menu.h"
#include "types.h"
#include "dictionary.h"
#include "vm.h"
#include "symbols.h"

static TArray<AFuncDesc> AFTable;
static TArray<FieldDesc> FieldTable;


//==========================================================================
//
//
//
//==========================================================================

template <typename Desc>
static int CompareClassNames(const char* const aname, const Desc& b)
{
	// ++ to get past the prefix letter of the native class name, which gets omitted by the FName for the class.
	const char* bname = b.ClassName;
	if ('\0' != *bname) ++bname;
	return stricmp(aname, bname);
}

template <typename Desc>
static int CompareClassNames(const Desc& a, const Desc& b)
{
	// ++ to get past the prefix letter of the native class name, which gets omitted by the FName for the class.
	const char* aname = a.ClassName;
	if ('\0' != *aname) ++aname;
	return CompareClassNames(aname, b);
}

//==========================================================================
//
// Find a function by name using a binary search
//
//==========================================================================

AFuncDesc *FindFunction(PContainerType *cls, const char * string)
{
	int min = 0, max = AFTable.Size() - 1;

	while (min <= max)
	{
		int mid = (min + max) / 2;
		int lexval = CompareClassNames(cls->TypeName.GetChars(), AFTable[mid]);
		if (lexval == 0) lexval = stricmp(string, AFTable[mid].FuncName);
		if (lexval == 0)
		{
			return &AFTable[mid];
		}
		else if (lexval > 0)
		{
			min = mid + 1;
		}
		else
		{
			max = mid - 1;
		}
	}
	return nullptr;
}

//==========================================================================
//
// Find a function by name using a binary search
//
//==========================================================================

FieldDesc *FindField(PContainerType *cls, const char * string)
{
	int min = 0, max = FieldTable.Size() - 1;
	const char * cname = cls ? cls->TypeName.GetChars() : "";

	while (min <= max)
	{
		int mid = (min + max) / 2;
		int lexval = CompareClassNames(cname, FieldTable[mid]);
		if (lexval == 0) lexval = stricmp(string, FieldTable[mid].FieldName);
		if (lexval == 0)
		{
			return &FieldTable[mid];
		}
		else if (lexval > 0)
		{
			min = mid + 1;
		}
		else
		{
			max = mid - 1;
		}
	}
	return nullptr;
}


//==========================================================================
//
// Find an action function in AActor's table
//
//==========================================================================

VMFunction *FindVMFunction(PClass *cls, const char *name)
{
	auto f = dyn_cast<PFunction>(cls->FindSymbol(name, true));
	return f == nullptr ? nullptr : f->Variants[0].Implementation;
}


//==========================================================================
//
// Sorting helpers
//
//==========================================================================

static int funccmp(const void * a, const void * b)
{
	int res = CompareClassNames(*(AFuncDesc*)a, *(AFuncDesc*)b);
	if (res == 0) res = stricmp(((AFuncDesc*)a)->FuncName, ((AFuncDesc*)b)->FuncName);
	return res;
}

static int fieldcmp(const void * a, const void * b)
{
	int res = CompareClassNames(*(FieldDesc*)a, *(FieldDesc*)b);
	if (res == 0) res = stricmp(((FieldDesc*)a)->FieldName, ((FieldDesc*)b)->FieldName);
	return res;
}

//==========================================================================
//
// Initialization
//
//==========================================================================

void InitImports()
{
	auto fontstruct = NewStruct("FFont", nullptr, true);
	fontstruct->Size = sizeof(FFont);
	fontstruct->Align = alignof(FFont);
	NewPointer(fontstruct, false)->InstallHandlers(
		[](FSerializer &ar, const char *key, const void *addr)
		{
			ar(key, *(FFont **)addr);
		},
			[](FSerializer &ar, const char *key, void *addr)
		{
			Serialize<FFont>(ar, key, *(FFont **)addr, nullptr);
			return true;
		}
	);

	// Create a sorted list of native action functions
	AFTable.Clear();
	if (AFTable.Size() == 0)
	{
		FAutoSegIterator probe(ARegHead, ARegTail);

		while (*++probe != NULL)
		{
			AFuncDesc *afunc = (AFuncDesc *)*probe;
			assert(afunc->VMPointer != NULL);
			*(afunc->VMPointer) = new VMNativeFunction(afunc->Function, afunc->FuncName);
			(*(afunc->VMPointer))->PrintableName.Format("%s.%s [Native]", afunc->ClassName+1, afunc->FuncName);
			(*(afunc->VMPointer))->DirectNativeCall = afunc->DirectNative;
			AFTable.Push(*afunc);
		}
		AFTable.ShrinkToFit();
		qsort(&AFTable[0], AFTable.Size(), sizeof(AFTable[0]), funccmp);
	}

	FieldTable.Clear();
	if (FieldTable.Size() == 0)
	{
		FAutoSegIterator probe(FRegHead, FRegTail);

		while (*++probe != NULL)
		{
			FieldDesc *afield = (FieldDesc *)*probe;
			FieldTable.Push(*afield);
		}
		FieldTable.ShrinkToFit();
		qsort(&FieldTable[0], FieldTable.Size(), sizeof(FieldTable[0]), fieldcmp);
	}
}


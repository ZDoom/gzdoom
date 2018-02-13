/*
** scopebarrier.cpp
**
**---------------------------------------------------------------------------
** Copyright 2017 ZZYZX
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

#include "dobject.h"
#include "scopebarrier.h"
#include "types.h"
#include "vmintern.h"


// Note: the same object can't be both UI and Play. This is checked explicitly in the field construction and will cause esoteric errors here if found.
int FScopeBarrier::SideFromFlags(int flags)
{
	if (flags & VARF_UI)
		return Side_UI;
	if (flags & VARF_Play)
		return Side_Play;
	if (flags & VARF_VirtualScope)
		return Side_Virtual;
	if (flags & VARF_ClearScope)
		return Side_Clear;
	return Side_PlainData;
}

// same as above, but from object flags
int FScopeBarrier::SideFromObjectFlags(EScopeFlags flags)
{
	if (flags & Scope_UI)
		return Side_UI;
	if (flags & Scope_Play)
		return Side_Play;
	return Side_PlainData;
}

//
int FScopeBarrier::FlagsFromSide(int side)
{
	switch (side)
	{
	case Side_Play:
		return VARF_Play;
	case Side_UI:
		return VARF_UI;
	case Side_Virtual:
		return VARF_VirtualScope;
	case Side_Clear:
		return VARF_ClearScope;
	default:
		return 0;
	}
}

EScopeFlags FScopeBarrier::ObjectFlagsFromSide(int side)
{
	switch (side)
	{
	case Side_Play:
		return Scope_Play;
	case Side_UI:
		return Scope_UI;
	default:
		return Scope_All;
	}
}

// used for errors
const char* FScopeBarrier::StringFromSide(int side)
{
	switch (side)
	{
	case Side_PlainData:
		return "data";
	case Side_UI:
		return "ui";
	case Side_Play:
		return "play";
	case Side_Virtual:
		return "virtualscope"; // should not happen!
	case Side_Clear:
		return "clearscope"; // should not happen!
	default:
		return "unknown";
	}
}

// this modifies VARF_ flags and sets the side properly.
int FScopeBarrier::ChangeSideInFlags(int flags, int side)
{
	flags &= ~(VARF_UI | VARF_Play | VARF_VirtualScope | VARF_ClearScope);
	flags |= FlagsFromSide(side);
	return flags;
}

// this modifies OF_ flags and sets the side properly.
EScopeFlags FScopeBarrier::ChangeSideInObjectFlags(EScopeFlags flags, int side)
{
	int f = int(flags);
	f &= ~(Scope_UI | Scope_Play);
	f |= ObjectFlagsFromSide(side);
	return (EScopeFlags)f;
}

FScopeBarrier::FScopeBarrier()
{
	sidefrom = -1;
	sidelast = -1;
	callable = true;
	readable = true;
	writable = true;
}

FScopeBarrier::FScopeBarrier(int flags1, int flags2, const char* name)
{
	sidefrom = -1;
	sidelast = -1;
	callable = true;
	readable = true;
	writable = true;

	AddFlags(flags1, flags2, name);
}

// AddFlags modifies ALLOWED actions by flags1->flags2.
// This is used for comparing a.b.c.d access - if non-allowed field is seen anywhere in the chain, anything after it is non-allowed.
// This struct is used so that the logic is in a single place.
void FScopeBarrier::AddFlags(int flags1, int flags2, const char* name)
{
	// note: if it's already non-readable, don't even try advancing
	if (!readable)
		return;

	// we aren't interested in any other flags
	//  - update: including VARF_VirtualScope. inside the function itself, we treat it as if it's PlainData.
	flags1 &= VARF_UI | VARF_Play;
	flags2 &= VARF_UI | VARF_Play | VARF_ReadOnly;

	if (sidefrom < 0) sidefrom = SideFromFlags(flags1);
	if (sidelast < 0) sidelast = sidefrom;

	// flags1 = what's trying to access
	// flags2 = what's being accessed

	int sideto = SideFromFlags(flags2);

	// plain data inherits whatever scope modifiers that context or field container has.
	// i.e. play String bla; is play, and all non-specified methods/fields inside it are play as well.
	if (sideto != Side_PlainData)
		sidelast = sideto;
	else sideto = sidelast;

	if ((sideto == Side_UI) && (sidefrom != Side_UI)) // only ui -> ui is readable
	{
		readable = false;
		if (name) readerror.Format("Can't read %s field %s from %s context", StringFromSide(sideto), name, StringFromSide(sidefrom));
	}

	if (!readable)
	{
		writable = false;
		callable = false;
		if (name)
		{
			writeerror.Format("Can't write %s field %s from %s context (not readable)", StringFromSide(sideto), name, StringFromSide(sidefrom));
			callerror.Format("Can't call %s function %s from %s context (not readable)", StringFromSide(sideto), name, StringFromSide(sidefrom));
		}
		return;
	}

	if (writable && (sidefrom != sideto)) // only matching types are writable (plain data implicitly takes context type by default, unless overridden)
	{
		writable = false;
		if (name) writeerror.Format("Can't write %s field %s from %s context", StringFromSide(sideto), name, StringFromSide(sidefrom));
	}

	if (callable && (sidefrom != sideto) && !(flags2 & VARF_ReadOnly)) // readonly on methods is used for plain data stuff that can be called from ui/play context.
	{
		callable = false;
		if (name) callerror.Format("Can't call %s function %s from %s context", StringFromSide(sideto), name, StringFromSide(sidefrom));
	}
}

// these are for vmexec.h
void FScopeBarrier::ValidateNew(PClass* cls, int outerside)
{
	int innerside = FScopeBarrier::SideFromObjectFlags(cls->VMType->ScopeFlags);
	if ((outerside != innerside) && (innerside != FScopeBarrier::Side_PlainData)) // "cannot construct ui class ... from data context"
		ThrowAbortException(X_OTHER, "Cannot construct %s class %s from %s context", FScopeBarrier::StringFromSide(innerside), cls->TypeName.GetChars(), FScopeBarrier::StringFromSide(outerside));
}

void FScopeBarrier::ValidateCall(PClass* selftype, VMFunction *calledfunc, int outerside)
{
	int innerside = FScopeBarrier::SideFromObjectFlags(selftype->VMType->ScopeFlags);
	if ((outerside != innerside) && (innerside != FScopeBarrier::Side_PlainData))
		ThrowAbortException(X_OTHER, "Cannot call %s function %s from %s context", FScopeBarrier::StringFromSide(innerside), calledfunc->PrintableName.GetChars(), FScopeBarrier::StringFromSide(outerside));
}
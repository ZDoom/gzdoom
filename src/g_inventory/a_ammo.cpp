/*
** a_ammo.cpp
** Implements ammo and backpack items.
**
**---------------------------------------------------------------------------
** Copyright 2000-2016 Randy Heit
** Copyright 2006-2016 Cheistoph Oelckers
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

#include "c_dispatch.h"
#include "d_player.h"
#include "serializer.h"

IMPLEMENT_CLASS(AAmmo, false, false)

DEFINE_FIELD(AAmmo, BackpackAmount)
DEFINE_FIELD(AAmmo, BackpackMaxAmount)

//===========================================================================
//
// AAmmo :: Serialize
//
//===========================================================================

void AAmmo::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	auto def = (AAmmo*)GetDefault();
	arc("backpackamount", BackpackAmount, def->BackpackAmount)
		("backpackmaxamount", BackpackMaxAmount, def->BackpackMaxAmount);
}

//===========================================================================
//
// AAmmo :: GetParentAmmo
//
// Returns the least-derived ammo type that this ammo is a descendant of.
// That is, if this ammo is an immediate subclass of Ammo, then this ammo's
// type is returned. If this ammo's superclass is not Ammo, then this
// function travels up the inheritance chain until it finds a type that is
// an immediate subclass of Ammo and returns that.
//
// The intent of this is that all unique ammo types will be immediate
// subclasses of Ammo. To make different pickups with different ammo amounts,
// you subclass the type of ammo you want a different amount for and edit
// that.
//
//===========================================================================

PClassActor *AAmmo::GetParentAmmo () const
{
	PClass *type = GetClass();

	while (type->ParentClass != RUNTIME_CLASS(AAmmo) && type->ParentClass != NULL)
	{
		type = type->ParentClass;
	}
	return static_cast<PClassActor *>(type);
}

DEFINE_ACTION_FUNCTION(AAmmo, GetParentAmmo)
{
	PARAM_SELF_PROLOGUE(AAmmo);
	ACTION_RETURN_OBJECT(self->GetParentAmmo());
}
